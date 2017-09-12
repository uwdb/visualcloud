#ifndef VISUALCLOUD_ENCODEBUFFER_H
#define VISUALCLOUD_ENCODEBUFFER_H

#include "Configuration.h"
#include "nvEncodeAPI.h"
#include "GPUContext.h"
#include "VideoLock.h"
#include <mutex>
#include <stdint.h>

typedef struct _EncodeInputBuffer
{
    unsigned int      dwWidth;
    unsigned int      dwHeight;
#if defined (NV_WINDOWS)
    IDirect3DSurface9 *pNV12Surface;
#endif
    CUdeviceptr       pNV12devPtr;
    uint32_t          uNV12Stride;
    CUdeviceptr       pNV12TempdevPtr;
    uint32_t          uNV12TempStride;
    void*             nvRegisteredResource;
    NV_ENC_INPUT_PTR  hInputSurface;
    NV_ENC_BUFFER_FORMAT bufferFmt;
} EncodeInputBuffer;

typedef struct _EncodeOutputBuffer
{
    unsigned int          dwBitstreamBufferSize;
    NV_ENC_OUTPUT_PTR     hBitstreamBuffer;
    HANDLE                hOutputEvent;
    bool                  bWaitOnEvent;
    bool                  bEOSFlag;
} EncodeOutputBuffer;

typedef struct _EncodeBuffer
{
    EncodeOutputBuffer      stOutputBfr;
    EncodeInputBuffer       stInputBfr;
    EncodeAPI&              api;
    const Configuration&     configuration; // TODO change this to const (possibly others)
    const size_t            size;

    _EncodeBuffer(const _EncodeBuffer& other)
            : _EncodeBuffer(other.api, other.configuration, other.size)
    { }

    _EncodeBuffer(const _EncodeBuffer&& other) = delete;
/*            : stOutputBfr(other.stOutputBfr), stInputBfr(other.stInputBfr),
              api(other.api), configuration(other.configuration), size(other.size),
              owner(true) {
        //TODO I removed this, did I mean to?
        //other.owner = false;
    }*/

    // TODO is this size reasonable?
    _EncodeBuffer(EncodeAPI &api, const Configuration& configuration, size_t size=2*1024*1024)
            : stOutputBfr{0}, stInputBfr{0}, api(api), configuration(configuration), size(size) {
    //owner(true) {
        NVENCSTATUS status;

        //if((status = api.CreateEncoder(&configuration)) != NV_ENC_SUCCESS)
        //    throw status; //TODO
        if(configuration.height % 2 != 0)
            throw std::runtime_error("Buffer height must be even"); //TODO
        else if(configuration.width % 2 != 0)
            throw std::runtime_error("Buffer width must be even"); //TODO
        //TODO if a decoder is required, accept a VideoDecoder as parameter rathern than api
        else if(!api.encoderCreated())
            throw std::runtime_error("encoder not created"); //TODO
        else if(cuMemAllocPitch(&stInputBfr.pNV12devPtr,
                           (size_t*)&stInputBfr.uNV12Stride,
                           configuration.width,
                           configuration.height * 3 / 2,
                           16) != CUDA_SUCCESS)
            throw std::runtime_error("NV_ENC_ERR_OUT_OF_MEMORY"); //tODO
        else if((status = api.NvEncRegisterResource(
                NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR, (void *)stInputBfr.pNV12devPtr,
                configuration.width, configuration.height,
                stInputBfr.uNV12Stride, &stInputBfr.nvRegisteredResource)) != NV_ENC_SUCCESS)
            throw std::runtime_error(std::to_string(status)); //TODO
//        else if((status = api.NvEncMapInputResource(stInputBfr.nvRegisteredResource,
//                                                    &stInputBfr.hInputSurface)) != NV_ENC_SUCCESS)
//            throw "998"; //TODO
        else if((status = api.NvEncCreateBitstreamBuffer(
                size, &stOutputBfr.hBitstreamBuffer)) != NV_ENC_SUCCESS)
            throw std::runtime_error("997"); //TODO

        stInputBfr.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
        stInputBfr.dwWidth = configuration.width;
        stInputBfr.dwHeight = configuration.height;
        stOutputBfr.dwBitstreamBufferSize = size;
        stOutputBfr.hOutputEvent = nullptr;
    }

    ~_EncodeBuffer() {
        NVENCSTATUS status;

//        if(!owner)
  //          ;
        if((status = api.NvEncDestroyBitstreamBuffer(stOutputBfr.hBitstreamBuffer)) != NV_ENC_SUCCESS)
            printf("log\n"); //TODO log
//        else if((status = api.NvEncUnmapInputResource(stInputBfr.hInputSurface)) != NV_ENC_SUCCESS)
//            printf("log\n"); //TODO log
        else if((api.NvEncUnregisterResource(stInputBfr.nvRegisteredResource)) != NV_ENC_SUCCESS)
            printf("log\n"); //TODO log
        else if(cuMemFree(stInputBfr.pNV12devPtr) != CUDA_SUCCESS)
            printf("log\n"); //TODO log
    }

    void copy(VideoLock &lock, const CUDA_MEMCPY2D &parameters) {
        CUresult result;
        std::scoped_lock{lock};

        if ((result = cuMemcpy2D(&parameters)) != CUDA_SUCCESS) {
            throw std::runtime_error(std::to_string(result)); //TODO
        }
    }

    void copy(VideoLock &lock, const std::vector<CUDA_MEMCPY2D> &parameters) {
        std::scoped_lock{lock};
        std::for_each(parameters.begin(), parameters.end(), [](const CUDA_MEMCPY2D &parameters) {
            CUresult result;
            if ((result = cuMemcpy2D(&parameters)) != CUDA_SUCCESS) {
                throw std::runtime_error(std::to_string(result)); //TODO
            }
        });
    }

    void lock() {
        NVENCSTATUS status;
        if((status = api.NvEncMapInputResource(stInputBfr.nvRegisteredResource,
                                               &stInputBfr.hInputSurface)) != NV_ENC_SUCCESS)
            throw std::runtime_error(std::to_string(status)); //TODO
    }

    void unlock() {
        NVENCSTATUS status;
        if((status = api.NvEncUnmapInputResource(stInputBfr.hInputSurface)) != NV_ENC_SUCCESS)
            throw std::runtime_error(std::to_string(status)); //TODO
    }

//private:
  //  bool owner;
} EncodeBuffer;

typedef struct _MotionEstimationBuffer
{
    EncodeOutputBuffer      stOutputBfr;
    EncodeInputBuffer       stInputBfr[2];
    unsigned int            inputFrameIndex;
    unsigned int            referenceFrameIndex;
} MotionEstimationBuffer;

#endif //VISUALCLOUD_ENCODEBUFFER_H
