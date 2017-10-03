#include "VideoEncoderSession.h"
#include "AssertVideo.h"
#include <gtest/gtest.h>

#define FILENAME "resources/result-VideoEncoderSessionTestFixture.h265"

class VideoEncoderSessionTestFixture : public testing::Test {
public:
    VideoEncoderSessionTestFixture()
        : context(0),
          configuration(1080, 1920, NV_ENC_HEVC, 30, 30, 1024*1024),
          lock(context),
          encoder(context, configuration, lock),
          writer(encoder, FILENAME),
          session(encoder, writer)
    {}

protected:
    GPUContext context;
    EncodeConfiguration configuration;
    VideoLock lock;
    VideoEncoder encoder;
    FileEncodeWriter writer;
    VideoEncoderSession session;
};


TEST_F(VideoEncoderSessionTestFixture, testConstructor) {
    EXPECT_EQ(session.frameCount(), 0);
}

TEST_F(VideoEncoderSessionTestFixture, testFlush) {
    ASSERT_NO_THROW(session.Flush());
    EXPECT_EQ(session.frameCount(), 0);
}

TEST_F(VideoEncoderSessionTestFixture, testEncodeSingleFrame) {
    CUdeviceptr handle;
    size_t pitch;

    ASSERT_EQ(cuMemAllocPitch(
        &handle,
        &pitch,
        configuration.width,
        configuration.height * (3/2) * 2,
        16), CUDA_SUCCESS);

    Frame inputFrame(handle, static_cast<unsigned int>(pitch), configuration, NV_ENC_PIC_STRUCT_FRAME);

    ASSERT_NO_THROW(session.Encode(inputFrame));

    EXPECT_EQ(session.frameCount(), 1);

    EXPECT_NO_THROW(session.Flush());

    EXPECT_VIDEO_VALID(FILENAME);
    EXPECT_VIDEO_FRAMES(FILENAME, 1);
    EXPECT_VIDEO_RESOLUTION(FILENAME, configuration.height, configuration.width);

    EXPECT_EQ(remove(FILENAME), 0);
    EXPECT_EQ(cuMemFree(handle), CUDA_SUCCESS);
}

TEST_F(VideoEncoderSessionTestFixture, testEncodeMultipleFrames) {
    auto count = 60;
    CUdeviceptr handle;
    size_t pitch;

    ASSERT_EQ(cuMemAllocPitch(
            &handle,
            &pitch,
            configuration.width,
            configuration.height * (3/2) * 2,
            16), CUDA_SUCCESS);

    Frame inputFrame(handle, static_cast<unsigned int>(pitch), configuration, NV_ENC_PIC_STRUCT_FRAME);

    for(int i = 0; i < count; i++) {
        ASSERT_NO_THROW(session.Encode(inputFrame));
    }

    EXPECT_EQ(session.frameCount(), count);

    EXPECT_NO_THROW(session.Flush());
    EXPECT_VIDEO_VALID(FILENAME);
    EXPECT_VIDEO_FRAMES(FILENAME, 60);
    EXPECT_VIDEO_RESOLUTION(FILENAME, configuration.height, configuration.width);

    EXPECT_EQ(remove(FILENAME), 0);
    EXPECT_EQ(cuMemFree(handle), CUDA_SUCCESS);
}