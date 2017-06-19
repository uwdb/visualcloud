#include "Transcoder.h"
#include <gtest/gtest.h>

class TranscoderTestFixture : public testing::Test {
public:
  TranscoderTestFixture()
          : transcoder(1080, 1920, NV_ENC_HEVC, "hq", 30, 30, 1024, 0, 0)
  {}

    void SetUp() override {
  }

  void TearDown() override {
  }

protected:
    Transcoder transcoder;

};

TEST_F(TranscoderTestFixture, testConstructor) {
}

TEST_F(TranscoderTestFixture, testInitialize) {
  ASSERT_EQ(transcoder.initialize(), NV_ENC_SUCCESS);
}