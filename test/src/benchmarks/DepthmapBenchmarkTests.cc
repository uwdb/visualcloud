#include "Operators.h"
#include "Physical.h"
#include <gtest/gtest.h>
#include <AssertVideo.h>

using namespace visualcloud;
using namespace std::chrono;

class DepthmapBenchmarkTestFixture: public testing::Test {
public:
    DepthmapBenchmarkTestFixture()
        : name("result"),
          dataset("timelapse"),
          source{std::string("../../benchmarks/datasets/") + dataset + '/' + dataset + "4K.h264"},
          height(2048), width(3840), frames(2701)
    { }

    const char *name;
    const char *dataset;
    const std::string source;
    const size_t height, width, frames;
};

TEST_F(DepthmapBenchmarkTestFixture, testDepthmap_CPU) {
    auto start = steady_clock::now();

    Decode<EquirectangularGeometry>(source)
            >> Map(visualcloud::DepthmapCPU())
            >> Encode<YUVColorSpace>()
            >> Store(name);

    LOG(INFO) << source << " time:" << ::duration_cast<milliseconds>(steady_clock::now() - start).count() << "ms";

    EXPECT_VIDEO_VALID(name);
    EXPECT_VIDEO_FRAMES(name, frames);
    EXPECT_VIDEO_RESOLUTION(name, height, width);
    EXPECT_EQ(remove(name), 0);
}

TEST_F(DepthmapBenchmarkTestFixture, testDepthmap_GPU) {
    auto start = steady_clock::now();

    Decode<EquirectangularGeometry>(source)
            >> Map(visualcloud::DepthmapGPU())
            >> Encode<YUVColorSpace>()
            >> Store(name);

    LOG(INFO) << source << " time:" << ::duration_cast<milliseconds>(steady_clock::now() - start).count() << "ms";

    EXPECT_VIDEO_VALID(name);
    EXPECT_VIDEO_FRAMES(name, frames);
    EXPECT_VIDEO_RESOLUTION(name, height, width);
    EXPECT_EQ(remove(name), 0);
}

TEST_F(DepthmapBenchmarkTestFixture, testDepthmap_FPGA) {
    auto start = steady_clock::now();

    Decode<EquirectangularGeometry>(source)
            >> Map(visualcloud::DepthmapFPGA())
            >> Encode<YUVColorSpace>()
            >> Store(name);

    LOG(INFO) << source << " time:" << ::duration_cast<milliseconds>(steady_clock::now() - start).count() << "ms";

    EXPECT_VIDEO_VALID(name);
    EXPECT_VIDEO_FRAMES(name, frames);
    EXPECT_VIDEO_RESOLUTION(name, height, width);
    EXPECT_EQ(remove(name), 0);
}