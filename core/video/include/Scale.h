#ifndef LIGHTDB_SCALE_H
#define LIGHTDB_SCALE_H

#include "VideoLock.h"
#include <cuda.h>
#include <vector_types.h>
#include <experimental/filesystem>
#include <mutex>

namespace lightdb::video {

class GPUScale {
public:
    class NV12 {
    public:
        NV12(const GPUContext &context, const std::experimental::filesystem::path &module_path)
              : module_(nullptr),
                function_(nullptr),
                luma2DTexture_(nullptr),
                chroma2DTexture_(nullptr),
                owned_(true) {
            CUresult result;

            printf("%s\n", std::experimental::filesystem::absolute(module_path / module_filename).c_str());
            if((result = cuModuleLoad(&module_, std::experimental::filesystem::absolute(module_path / module_filename).c_str())) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("Could not load NV12 scale module", result);
            else if((result = cuModuleGetFunction(&function_, module_, function_name)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("Could not load NV12 scale function", result);
            else if((luma2DTexture_ = create_texture(module_, "luma_tex", CU_AD_FORMAT_UNSIGNED_INT8, 1)) == nullptr)
                throw GpuCudaRuntimeError("Unable to allocate NV12 luma texture", result);
            else if((chroma2DTexture_ = create_texture(module_, "chroma_tex", CU_AD_FORMAT_UNSIGNED_INT8, 2)) == nullptr)
                throw GpuCudaRuntimeError("Unable to allocate NV12 chrome texture", result);
        }

        NV12(const NV12&) = delete;
        NV12(NV12&& other) noexcept
            : module_(other.module_),
              function_(other.function_),
              luma2DTexture_(other.luma2DTexture_),
              chroma2DTexture_(other.chroma2DTexture_),
              owned_(true)
        { other.owned_ = false; }

        ~NV12() {
            CUresult result;

            if(owned_ && luma2DTexture_ != nullptr && (result = cuTexRefDestroy(luma2DTexture_)) != CUDA_SUCCESS)
                LOG(WARNING) << "Swallowed attempt to destroy NV12 scale luma texture: " << result;
            if(owned_ && chroma2DTexture_ != nullptr && (result = cuTexRefDestroy(chroma2DTexture_)) != CUDA_SUCCESS)
                LOG(WARNING) << "Swallowed attempt to destroy NV12 scale chroma texture: " << result;
            if(owned_ && module_ != nullptr && (result = cuModuleUnload(module_)) != CUDA_SUCCESS)
                LOG(WARNING) << "Swallowed attempt to unload NV12 scale module: " << result;
        }

        void scale(VideoLock &lock, const CudaFrameReference &input, CudaFrameReference &output) const {
            scale(lock,
                  input->handle(), output->handle(),
                  input->width(),  input->pitch(),  input->height(),
                  output->width(), output->pitch(), output->height(),
                  input->width(),  input->height());
        }

        void scale(VideoLock &lock, CUdeviceptr input, CUdeviceptr output,
                   unsigned int srcWidth, unsigned int srcPitch, unsigned int srcHeight,
                   unsigned int dstWidth, unsigned int dstPitch, unsigned int dstHeight,
                   unsigned int maxWidth, unsigned int maxHeight) const {
        std::lock_guard mlock(lock);

        CUresult result;
        auto luma_channels = 1u, chroma_channels = 2u;
        auto srcLeft = 0u, srcTop = 0u, srcRight = srcWidth, srcBottom = srcHeight;
        auto dstLeft = 0u, dstTop = 0u, dstRight = dstWidth, dstBottom = dstHeight;

        CHECK_NE(input, 0);
        CHECK_NE(output, 0);

        auto left = (float)srcLeft;
        auto right = (float)(srcRight - 1);

        auto xScale = (float)(srcRight - srcLeft) / (float)(dstRight - dstLeft);
        auto xOffset = std::min(0.5f*xScale - 0.5f, 0.5f) + left;

        auto yScale = (float)(srcBottom - srcTop) / (float)(dstBottom - dstTop);
        auto yOffset = std::min(0.5f*yScale - 0.5f, 0.5f);

        CUDA_ARRAY_DESCRIPTOR luma_description{
                .Width = srcPitch / luma_channels,
                .Height = srcBottom - srcTop,
                .Format = CU_AD_FORMAT_UNSIGNED_INT8,
                .NumChannels = luma_channels
        };

        if((result = cuTexRefSetFilterMode(luma2DTexture_, CU_TR_FILTER_MODE_LINEAR)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Scale luma call to cuTexRefSetFilterMode", result);
        else if((result = cuTexRefSetAddress2D(luma2DTexture_, &luma_description, input + srcTop*srcPitch, srcPitch)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Scale luma call to cuTexRefSetAddress2D", result);

        CUDA_ARRAY_DESCRIPTOR chroma_description{
                .Width = srcPitch / chroma_channels,
                .Height = (srcBottom - srcTop) >> 1,
                .Format = CU_AD_FORMAT_UNSIGNED_INT8,
                .NumChannels = chroma_channels
        };

        if((result = cuTexRefSetFilterMode(chroma2DTexture_, CU_TR_FILTER_MODE_LINEAR)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Scale chroma call to cuTexRefSetFilterMode", result);
        if((result = cuTexRefSetAddress2D(chroma2DTexture_, &chroma_description, input + (maxHeight + srcTop/2)*srcPitch, srcPitch)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Scale chroma call to chroma2DTexture_", result);


        auto dstUVOffset = dstHeight * dstPitch;// maxHeight * srcPitch;
        auto x_Offset = xOffset - dstLeft*xScale;
        auto y_Offset = yOffset + 0.5f - dstTop*yScale;
        auto xc_offset = xOffset - dstLeft*xScale*0.5f;
        auto yc_offset = yOffset + 0.5f - dstTop*yScale*0.5f;

        void *args[] = { &output, &dstUVOffset, &dstWidth, &dstHeight, &dstPitch,
                         &left, &right, &x_Offset, &y_Offset,
                         &xc_offset, &yc_offset, &xScale, &yScale };

        dim3 block(256, 1, 1);
        dim3 grid((dstRight + 255) >> 8, (dstBottom + 1) >> 1, 1);

        if((result = cuLaunchKernel(function_, grid.x, grid.y, grid.z,
                                block.x, block.y, block.z,
                                0,
                                nullptr, args, nullptr)) != CUDA_SUCCESS)
            throw GpuCudaRuntimeError("Scale kernel failed", result);
        else if((result = cuStreamQuery(nullptr)) != CUDA_SUCCESS && result != CUDA_ERROR_NOT_READY)
            throw GpuCudaRuntimeError("Scale cuStreamQuery", result);
        }

    private:

        static CUtexref create_texture(CUmodule hModule, const char *name, CUarray_format format, int components,
                                       unsigned int flags = CU_TRSF_READ_AS_INTEGER) {
            CUresult result;
            CUtexref texture;

            if((result = cuModuleGetTexRef(&texture, hModule, name)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("Error in NV12 scale cuModuleGetTexRef", result);
            else if((result = cuTexRefSetFormat(texture, format, components)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("Error in NV12 scale cuTexRefSetFormat", result);
            else if((result = cuTexRefSetFlags(texture, flags)) != CUDA_SUCCESS)
                throw GpuCudaRuntimeError("Error in NV12 scale cuTexRefSetFlags", result);
            else
                return texture;
        }

        constexpr static const char* function_name = "Scale_Bilinear_NV12";
        constexpr static const char* module_filename =
            #if INTPTR_MAX == INT64_MAX
                            "preproc64_lowlat.ptx";
            #elif INTPTR_MAX == INT32_MAX
                    "preproc32_lowlat.ptx";
            #else
                #error Unknown pointer size or missing size macros!
            #endif

        CUmodule module_;
        CUfunction function_;
        CUtexref luma2DTexture_;   // YYYY 2D PL texture (uchar1)
        CUtexref chroma2DTexture_; // UVUV 2D PL texture (uchar2)
        bool owned_;
    };

    explicit GPUScale(const GPUContext &context)
            : GPUScale(context, "modules")
    { }

    GPUScale(const GPUContext &context, const std::experimental::filesystem::path &module_path)
            : nv12_(context, module_path)
    { }


    const NV12 &nv12() const { return nv12_; }

private:
    NV12 nv12_;
};

} // lightdb::video

#endif //LIGHTDB_SCALE_H
