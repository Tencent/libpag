/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "rendering/sequences/SequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "rendering/video/VideoDecoderFactory.h"
#include "tgfx/platform/HardwareBuffer.h"
#include "utils/TestUtils.h"

namespace pag {

// A controllable decoder simulating the iOS hardware decoder fast-failing while the app is
// inactive (issue #3715): onDecodeFrame() returns Error while decodingDisabled is set.
class FlakyVideoDecoder : public VideoDecoder {
 public:
  static bool decodingDisabled;

  explicit FlakyVideoDecoder(const VideoFormat& format) {
    auto hardwareBuffer = tgfx::HardwareBufferAllocate(format.width, format.height);
    buffer = tgfx::ImageBuffer::MakeFrom(hardwareBuffer);
    tgfx::HardwareBufferRelease(hardwareBuffer);
  }

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override {
    static_cast<void>(bytes);
    static_cast<void>(length);
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
    return buffer;
  }

 private:
  std::shared_ptr<tgfx::ImageBuffer> buffer = nullptr;
  int64_t pendingTime = 0;
  int64_t decodedTime = -1;
  bool hasPending = false;
};

bool FlakyVideoDecoder::decodingDisabled = false;

class FlakyDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    // Reports a software decoder so the VideoReader does not skip this factory when software
    // decoding is preferred for small videos.
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    return std::make_unique<FlakyVideoDecoder>(format);
  }
};

static VideoSequence* FindFirstVideoSequence(const std::shared_ptr<File>& file) {
  for (auto& composition : file->compositions) {
    if (composition->type() == CompositionType::Video) {
      auto videoComposition = static_cast<VideoComposition*>(composition);
      if (!videoComposition->sequences.empty()) {
        return videoComposition->sequences[0];
      }
    }
  }
  return nullptr;
}

/**
 * 用例描述: 模拟 iOS app inactive 期间硬件解码器 fast-fail（issue #3715），验证 VideoReader
 * 在解码失败时返回最后一次成功解码的帧而不是空缓冲，且恢复后能正常解码新帧。
 */
PAG_TEST(PAGIssue3715, VideoReaderKeepsLastFrameOnDecodeFailure) {
  auto data = ReadFile("resources/apitest/room_bottomMenu_gift.pag");
  ASSERT_TRUE(data != nullptr);
  auto file = File::Load(data->data(), data->size());
  ASSERT_TRUE(file != nullptr);
  auto sequence = FindFirstVideoSequence(file);
  ASSERT_TRUE(sequence != nullptr);

  auto demuxer = std::make_unique<VideoSequenceDemuxer>(file, sequence);
  FlakyDecoderFactory factory = {};
  VideoReader reader(std::move(demuxer), std::vector<const VideoDecoderFactory*>{&factory});
  FlakyVideoDecoder::decodingDisabled = false;

  auto firstBuffer = reader.readBuffer(0);
  EXPECT_TRUE(firstBuffer != nullptr);

  // Simulate the app becoming inactive: every decode attempt fails immediately.
  FlakyVideoDecoder::decodingDisabled = true;
  auto readResult = std::make_shared<SequenceReadResult>();
  auto fallbackBuffer = reader.readBuffer(1, readResult);
  EXPECT_TRUE(fallbackBuffer != nullptr);
  EXPECT_EQ(fallbackBuffer.get(), firstBuffer.get());
  EXPECT_EQ(readResult->status.load(), SequenceReadStatus::Succeeded);

  auto fallbackBuffer2 = reader.readBuffer(2);
  EXPECT_TRUE(fallbackBuffer2 != nullptr);
  EXPECT_EQ(fallbackBuffer2.get(), firstBuffer.get());

  // Decoding recovers once the app becomes active again.
  FlakyVideoDecoder::decodingDisabled = false;
  auto recoveredBuffer = reader.readBuffer(3);
  EXPECT_TRUE(recoveredBuffer != nullptr);
  EXPECT_NE(recoveredBuffer.get(), firstBuffer.get());

  FlakyVideoDecoder::decodingDisabled = false;
}

static constexpr int LOOP_COUNT = 4;
static constexpr int DIFF_THRESHOLD = 60;
static constexpr double ANOMALY_RATIO = 0.02;

struct DiffResult {
  int diffPixels = 0;
  int totalPixels = 0;
};

static DiffResult CompareBitmaps(const tgfx::Bitmap& a, const tgfx::Bitmap& b) {
  DiffResult result = {};
  if (a.isEmpty() || b.isEmpty() || a.width() != b.width() || a.height() != b.height()) {
    result.totalPixels = -1;
    return result;
  }
  auto width = a.width();
  auto height = a.height();
  auto rowBytesA = a.rowBytes();
  auto rowBytesB = b.rowBytes();
  auto baseA = static_cast<const uint8_t*>(a.lockPixels());
  auto baseB = static_cast<const uint8_t*>(b.lockPixels());
  a.unlockPixels();
  b.unlockPixels();
  result.totalPixels = width * height;
  for (int y = 0; y < height; y++) {
    auto rowA = baseA + static_cast<size_t>(y) * rowBytesA;
    auto rowB = baseB + static_cast<size_t>(y) * rowBytesB;
    for (int x = 0; x < width; x++) {
      for (int c = 0; c < 4; c++) {
        auto va = rowA[x * 4 + c];
        auto vb = rowB[x * 4 + c];
        int diff = va > vb ? va - vb : vb - va;
        if (diff > DIFF_THRESHOLD) {
          result.diffPixels++;
          break;
        }
      }
    }
  }
  return result;
}

static bool IsAnomaly(const DiffResult& result) {
  if (result.totalPixels <= 0) {
    return true;
  }
  return static_cast<double>(result.diffPixels) / static_cast<double>(result.totalPixels) >
         ANOMALY_RATIO;
}

/**
 * 用例描述: 模拟 PAGView 循环播放 issue#3715 的视频动画，跨循环边界逐帧 flush，
 * 检测视频层是否间歇性丢失（透明帧）。
 */
PAG_TEST(PAGIssue3715, LoopPlayback) {
  auto pagFile = LoadPAGFile("resources/apitest/room_bottomMenu_gift.pag");
  ASSERT_TRUE(pagFile != nullptr);
  auto frameDuration = pagFile->frameDuration();
  ASSERT_TRUE(frameDuration > 0);
  printf("[3715] file size: %dx%d, frameDuration: %d\n", pagFile->width(), pagFile->height(),
         static_cast<int>(frameDuration));
  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
  ASSERT_TRUE(pagSurface != nullptr);
  auto pagPlayer = std::make_shared<PAGPlayer>();
  pagPlayer->setSurface(pagSurface);
  pagPlayer->setComposition(pagFile);

  std::vector<tgfx::Bitmap> reference;
  int anomalyFrames = 0;
  int totalFrames = 0;
  for (int loop = 0; loop < LOOP_COUNT; loop++) {
    for (Frame frame = 0; frame < frameDuration; frame++) {
      auto progress = static_cast<double>(frame) / static_cast<double>(frameDuration);
      pagPlayer->setProgress(progress);
      pagPlayer->flush();
      auto snapshot = MakeSnapshot(pagSurface);
      ASSERT_FALSE(snapshot.isEmpty());
      if (loop == 0) {
        reference.push_back(snapshot);
        continue;
      }
      totalFrames++;
      auto diff = CompareBitmaps(snapshot, reference[static_cast<size_t>(frame)]);
      if (IsAnomaly(diff)) {
        anomalyFrames++;
        printf("[3715] ANOMALY LoopPlayback loop=%d frame=%d diffPixels=%d/%d\n", loop,
               static_cast<int>(frame), diff.diffPixels, diff.totalPixels);
        SaveImage(snapshot,
                  "PAGIssue3715/loop" + std::to_string(loop) + "_frame" + std::to_string(frame));
      }
    }
  }
  printf("[3715] LoopPlayback anomaly frames: %d / %d\n", anomalyFrames, totalFrames);
  EXPECT_EQ(anomalyFrames, 0);
}

/**
 * 用例描述: 模拟 PAGImageView(cacheAllFramesInMemory=false) 通过 PAGDecoder
 * 逐帧读取的路径，连续多轮读取同一序列，检测某帧内容是否间歇性缺失。
 */
PAG_TEST(PAGIssue3715, PAGDecoderSequentialRead) {
  auto pagFile = LoadPAGFile("resources/apitest/room_bottomMenu_gift.pag");
  ASSERT_TRUE(pagFile != nullptr);
  auto pagDecoder = PAGDecoder::MakeFrom(pagFile, 60.0f, 1.0f);
  ASSERT_TRUE(pagDecoder != nullptr);
  auto width = pagDecoder->width();
  auto height = pagDecoder->height();
  auto numFrames = pagDecoder->numFrames();
  printf("[3715] decoder size: %dx%d, numFrames: %d\n", width, height, numFrames);
  size_t rowBytes = static_cast<size_t>(width) * 4;
  std::vector<uint8_t> pixelsA(static_cast<size_t>(width) * height * 4);
  std::vector<uint8_t> pixelsB(static_cast<size_t>(width) * height * 4);
  std::vector<std::vector<uint8_t>> reference;
  int anomalyFrames = 0;
  int totalFrames = 0;
  for (int loop = 0; loop < LOOP_COUNT; loop++) {
    auto& pixels = loop % 2 == 0 ? pixelsA : pixelsB;
    for (int frame = 0; frame < numFrames; frame++) {
      auto success = pagDecoder->readFrame(frame, pixels.data(), rowBytes);
      ASSERT_TRUE(success);
      if (loop == 0) {
        reference.push_back(pixels);
        continue;
      }
      totalFrames++;
      auto& ref = reference[static_cast<size_t>(frame)];
      int diffPixels = 0;
      auto total = static_cast<size_t>(width) * height;
      for (size_t i = 0; i < total; i++) {
        bool diff = false;
        for (int c = 0; c < 4; c++) {
          auto va = pixels[i * 4 + c];
          auto vb = ref[i * 4 + c];
          int d = va > vb ? va - vb : vb - va;
          if (d > DIFF_THRESHOLD) {
            diff = true;
            break;
          }
        }
        if (diff) {
          diffPixels++;
        }
      }
      double ratio = static_cast<double>(diffPixels) / static_cast<double>(total);
      if (ratio > ANOMALY_RATIO) {
        anomalyFrames++;
        printf("[3715] ANOMALY PAGDecoder loop=%d frame=%d diffPixels=%d/%zu\n", loop, frame,
               diffPixels, total);
      }
    }
  }
  printf("[3715] PAGDecoder anomaly frames: %d / %d\n", anomalyFrames, totalFrames);
  EXPECT_EQ(anomalyFrames, 0);
}

}  // namespace pag
