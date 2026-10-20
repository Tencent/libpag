/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <map>
#include "rendering/sequences/SequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/video/SoftwareData.h"
#include "rendering/video/VideoDecoderFactory.h"
#include "rendering/video/VideoDemuxer.h"
#include "tgfx/core/Bitmap.h"
#include "utils/TestUtils.h"

namespace pag {

static constexpr int FallbackVideoFrameCount = 10;
static constexpr int64_t FallbackVideoFrameInterval = 100000;

// A demuxer serving 10 samples at a fixed 100ms interval (a 10fps video of 1 second).
class FallbackVideoDemuxer : public VideoDemuxer {
 public:
  VideoFormat getFormat() override {
    VideoFormat format = {};
    format.width = 100;
    format.height = 100;
    format.duration = FallbackVideoFrameCount * FallbackVideoFrameInterval;
    format.frameRate = 10;
    return format;
  }

  VideoSample nextSample() override {
    if (sampleIndex >= FallbackVideoFrameCount) {
      return {};
    }
    auto time = sampleIndex * FallbackVideoFrameInterval;
    sampleIndex++;
    return {sampleData, sizeof(sampleData), time};
  }

  int64_t getSampleTimeAt(int64_t targetTime) override {
    auto index = targetTime / FallbackVideoFrameInterval;
    if (index < 0) {
      index = 0;
    }
    if (index >= FallbackVideoFrameCount) {
      index = FallbackVideoFrameCount - 1;
    }
    return index * FallbackVideoFrameInterval;
  }

  bool needSeeking(int64_t currentTime, int64_t targetTime) override {
    return currentTime != targetTime;
  }

  void seekTo(int64_t targetTime) override {
    sampleIndex = static_cast<int>(targetTime / FallbackVideoFrameInterval);
    if (sampleIndex < 0) {
      sampleIndex = 0;
    }
    if (sampleIndex >= FallbackVideoFrameCount) {
      sampleIndex = FallbackVideoFrameCount - 1;
    }
  }

  void reset() override {
    sampleIndex = 0;
  }

 private:
  uint8_t sampleData[4] = {0, 0, 0, 1};
  int sampleIndex = 0;
};

// A controllable decoder simulating the iOS hardware decoder fast-failing while the app is
// inactive (issue #3715): onDecodeFrame() returns Error while decodingDisabled is set.
class FallbackVideoDecoder : public VideoDecoder {
 public:
  static bool decodingDisabled;

  DecodingResult onSendBytes(void*, size_t, int64_t time) override {
    pendingTime = time;
    hasPending = true;
    return DecodingResult::Success;
  }

  DecodingResult onEndOfStream() override {
    return DecodingResult::Success;
  }

  DecodingResult onDecodeFrame() override {
    if (decodingDisabled) {
      return DecodingResult::Error;
    }
    if (!hasPending) {
      return DecodingResult::TryAgainLater;
    }
    decodedTime = pendingTime;
    hasPending = false;
    return DecodingResult::Success;
  }

  void onFlush() override {
    hasPending = false;
  }

  int64_t presentationTime() override {
    return decodedTime;
  }

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override {
    auto result = buffers.find(decodedTime);
    if (result != buffers.end()) {
      return result->second;
    }
    tgfx::Bitmap bitmap = {};
    if (!bitmap.allocPixels(2, 2, false, false)) {
      return nullptr;
    }
    auto buffer = bitmap.makeBuffer();
    buffers[decodedTime] = buffer;
    return buffer;
  }

 private:
  bool hasPending = false;
  int64_t pendingTime = 0;
  int64_t decodedTime = -1;
  std::map<int64_t, std::shared_ptr<tgfx::ImageBuffer>> buffers = {};
};

bool FallbackVideoDecoder::decodingDisabled = false;

class FallbackVideoDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat&) const override {
    return std::make_unique<FallbackVideoDecoder>();
  }
};

static std::shared_ptr<SequenceReadResult> MakeFallbackReadResult(Frame targetFrame) {
  auto result = std::make_shared<SequenceReadResult>();
  result->requestID = 1;
  result->targetFrame = targetFrame;
  return result;
}

static std::shared_ptr<VideoReader> MakeFallbackVideoReader(FallbackVideoDecoderFactory* factory) {
  auto demuxer = std::make_unique<FallbackVideoDemuxer>();
  return std::make_shared<VideoReader>(std::move(demuxer),
                                       std::vector<const VideoDecoderFactory*>{factory});
}

/**
 * 用例描述: 解码失败时返回上一次成功解码的帧（fallback buffer），并以 Fallback 状态上报，
 * 而不是返回空指针或误报成功。
 */
PAG_TEST(VideoSequenceFallback, FallbackKeepsLastDecodedFrame) {
  FallbackVideoDecoderFactory factory = {};
  auto reader = MakeFallbackVideoReader(&factory);

  auto result = MakeFallbackReadResult(0);
  auto firstBuffer = reader->readBuffer(0, result);
  ASSERT_TRUE(firstBuffer != nullptr);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Succeeded);

  FallbackVideoDecoder::decodingDisabled = true;
  result = MakeFallbackReadResult(3);
  auto fallbackBuffer = reader->readBuffer(3, result);
  EXPECT_EQ(fallbackBuffer, firstBuffer);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Fallback);

  // Consecutive failures keep returning the last visible frame.
  result = MakeFallbackReadResult(5);
  EXPECT_EQ(reader->readBuffer(5, result), firstBuffer);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Fallback);

  // A repeated request for a frame that failed before also reports a fallback.
  result = MakeFallbackReadResult(3);
  EXPECT_EQ(reader->readBuffer(3, result), firstBuffer);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Fallback);

  FallbackVideoDecoder::decodingDisabled = false;
  result = MakeFallbackReadResult(3);
  auto recoveredBuffer = reader->readBuffer(3, result);
  ASSERT_TRUE(recoveredBuffer != nullptr);
  EXPECT_NE(recoveredBuffer, firstBuffer);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Succeeded);

  // A repeated request for the recovered frame returns the fresh buffer as a real success.
  result = MakeFallbackReadResult(3);
  EXPECT_EQ(reader->readBuffer(3, result), recoveredBuffer);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Succeeded);
}

/**
 * 用例描述: 首帧解码失败时没有可回退的帧，行为与失败上报一致（返回空指针并报 Failed）。
 */
PAG_TEST(VideoSequenceFallback, FirstFrameFailureHasNoFallback) {
  FallbackVideoDecoderFactory factory = {};
  auto reader = MakeFallbackVideoReader(&factory);

  FallbackVideoDecoder::decodingDisabled = true;
  auto result = MakeFallbackReadResult(0);
  EXPECT_EQ(reader->readBuffer(0, result), nullptr);
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Failed);
  FallbackVideoDecoder::decodingDisabled = false;
}

/**
 * 用例描述: 软件解码路径返回的 YUV 数据必须是自持拷贝，decoder 复用内部 buffer 后
 * 已返回的 ImageBuffer 内容不变。
 */
PAG_TEST(VideoSequenceFallback, SoftwareFrameCopySurvivesDecoderReuse) {
  const int width = 64;
  const int height = 32;
  std::vector<uint8_t> yPlane(static_cast<size_t>(width * height));
  std::vector<uint8_t> uPlane(static_cast<size_t>(width * height / 4));
  std::vector<uint8_t> vPlane(static_cast<size_t>(width * height / 4));
  for (size_t i = 0; i < yPlane.size(); i++) {
    yPlane[i] = static_cast<uint8_t>(i);
  }
  for (size_t i = 0; i < uPlane.size(); i++) {
    uPlane[i] = static_cast<uint8_t>(i * 2);
  }
  for (size_t i = 0; i < vPlane.size(); i++) {
    vPlane[i] = static_cast<uint8_t>(i * 3);
  }
  // The decoder's internal buffers may use a larger stride than the plane width.
  const int lineSize[3] = {width + 16, width / 2 + 8, width / 2 + 8};
  std::vector<uint8_t> yPadded(static_cast<size_t>(lineSize[0] * height));
  std::vector<uint8_t> uPadded(static_cast<size_t>(lineSize[1] * height / 2));
  std::vector<uint8_t> vPadded(static_cast<size_t>(lineSize[2] * height / 2));
  for (int row = 0; row < height; row++) {
    memcpy(yPadded.data() + static_cast<size_t>(row) * lineSize[0],
           yPlane.data() + static_cast<size_t>(row) * width, static_cast<size_t>(width));
  }
  for (int row = 0; row < height / 2; row++) {
    memcpy(uPadded.data() + static_cast<size_t>(row) * lineSize[1],
           uPlane.data() + static_cast<size_t>(row) * width / 2, static_cast<size_t>(width / 2));
    memcpy(vPadded.data() + static_cast<size_t>(row) * lineSize[2],
           vPlane.data() + static_cast<size_t>(row) * width / 2, static_cast<size_t>(width / 2));
  }
  uint8_t* data[3] = {yPadded.data(), uPadded.data(), vPadded.data()};
  auto yuvData = CopiedYUVData::Make(width, height, data, lineSize, 3);
  ASSERT_TRUE(yuvData != nullptr);
  EXPECT_EQ(yuvData->width(), width);
  EXPECT_EQ(yuvData->height(), height);
  EXPECT_EQ(yuvData->planeCount(), 3U);
  EXPECT_EQ(yuvData->getRowBytesAt(0), static_cast<size_t>(width));
  EXPECT_EQ(yuvData->getRowBytesAt(1), static_cast<size_t>(width / 2));
  EXPECT_EQ(yuvData->getRowBytesAt(2), static_cast<size_t>(width / 2));

  // Simulate the decoder recycling its internal buffers for a later decode call.
  memset(yPadded.data(), 0xAA, yPadded.size());
  memset(uPadded.data(), 0xAA, uPadded.size());
  memset(vPadded.data(), 0xAA, vPadded.size());

  auto yAddress = static_cast<const uint8_t*>(yuvData->getBaseAddressAt(0));
  auto uAddress = static_cast<const uint8_t*>(yuvData->getBaseAddressAt(1));
  auto vAddress = static_cast<const uint8_t*>(yuvData->getBaseAddressAt(2));
  EXPECT_EQ(memcmp(yAddress, yPlane.data(), yPlane.size()), 0);
  EXPECT_EQ(memcmp(uAddress, uPlane.data(), uPlane.size()), 0);
  EXPECT_EQ(memcmp(vAddress, vPlane.data(), vPlane.size()), 0);
}
}  // namespace pag
