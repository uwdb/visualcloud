#ifndef VISUALCLOUD_GPUCONTEXT_H
#define VISUALCLOUD_GPUCONTEXT_H

#include <stdexcept>
#include <dynlink_nvcuvid.h>

class GPUContext {
public:
    GPUContext(const unsigned int deviceId) {
        CUresult result;

        if(!ensureInitialized())
            throw std::runtime_error("throw\n"); //TODO
        else if((result = cuCtxGetCurrent(&context)) != CUDA_SUCCESS)
            throw std::runtime_error("throw\n"); //TODO
        else if(context != nullptr)
            ;
        else if((result = cuDeviceGet(&device, deviceId)) != CUDA_SUCCESS)
            throw std::invalid_argument("Getting CUDA device " + std::to_string(deviceId) +
                                                " generated error " + std::to_string(result));
        else if((result = cuCtxCreate(&context, CU_CTX_SCHED_AUTO, device)) != CUDA_SUCCESS)
            throw std::runtime_error("throw\n"); //TODO
    }
    ~GPUContext() {
        CUresult result;

        if((result = cuCtxDestroy(context)) != CUDA_SUCCESS)
            throw std::runtime_error("log error");
    }

    CUcontext get() { return context; }

    void AttachToThread() const {
        CUresult result;

        if((result = cuCtxSetCurrent(context)) != CUDA_SUCCESS)
            throw std::runtime_error(std::to_string(result)); //TODO
    }

private:
    static bool isInitialized;
    static bool ensureInitialized() {
            CUresult result;

            if(isInitialized)
                return true;
            else if((result = cuInit(0, __CUDA_API_VERSION, nullptr)) != CUDA_SUCCESS)
                throw std::runtime_error("throw\n"); //TODO
            else if(cuvidInit(0) != CUDA_SUCCESS)
                throw std::runtime_error("throw\n"); //TODO
            else
                return (isInitialized = true);
        }

    CUdevice device;
    CUcontext context = nullptr;
};

#endif //VISUALCLOUD_GPUCONTEXT_H