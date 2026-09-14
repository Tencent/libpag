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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions and
//  limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <vector>
#include "rendering/PAGAnimator.h"
#include "rendering/sequences/SequenceImageQueue.h"
#include "rendering/sequences/VideoReader.h"
#include "tgfx/core/Bitmap.h"
#include "utils/TestUtils.h"

namespace pag {
class ANRAnimatorListener : public PAGAnimator::Listener {
 public:
  void blockFirstUpdate() {
    std::lock_guard<std::mutex> autoLock(locker);
    shouldBlockFirstUpdate = true;
  }

  void releaseFirstUpdate() {
    std::lock_guard<std::mutex> autoLock(locker);
    firstUpdateReleased = true;
    condition.notify_all();
  }

  bool waitForUpdateCount(size_t count) {
    std::unique_lock<std::mutex> autoLock(locker);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (updateThreads.size() < count) {
      if (condition.wait_until(autoLock, deadline) == std::cv_status::timeout) {
        return updateThreads.size() >= count;
      }
    }
    return true;
  }

  size_t updateCount() {
    std::lock_guard<std::mutex> autoLock(locker);
    return updateThreads.size();
  }

  int maximumConcurrentUpdates() {
    std::lock_guard<std::mutex> autoLock(locker);
    return maximumActiveUpdates;
  }

  std::thread::id firstUpdateThread() {
    std::lock_guard<std::mutex> autoLock(locker);
    return updateThreads.front();
  }

 protected:
  void onAnimationUpdate(PAGAnimator*) override {
    std::unique_lock<std::mutex> autoLock(locker);
    activeUpdates++;
    maximumActiveUpdates = std::max(maximumActiveUpdates, activeUpdates);
    updateThreads.push_back(std::this_thread::get_id());
    condition.notify_all();
    while (shouldBlockFirstUpdate && updateThreads.size() == 1 && !firstUpdateReleased) {
      condition.wait(autoLock);
    }
    activeUpdates--;
    condition.notify_all();
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  std::vector<std::thread::id> updateThreads = {};
  int activeUpdates = 0;
  int maximumActiveUpdates = 0;
  bool shouldBlockFirstUpdate = false;
  bool firstUpdateReleased = false;
};

static std::shared_ptr<PAGAnimator> MakeANRAnimator(
    const std::shared_ptr<ANRAnimatorListener>& listener) {
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(listener));
  animator->weakThis = animator;
  return animator;
}

class ANRVideoDemuxer : public VideoDemuxer {
 public:
  VideoFormat getFormat() override {
    VideoFormat format = {};
    format.width = 1000;
    format.height = 1000;
    format.duration = 1000000;
    format.frameRate = 30;
    return format;
  }

  VideoSample nextSample() override {
    if (sampleSent) {
      return {};
    }
    sampleSent = true;
    return {sampleData, sizeof(sampleData), 0};
  }

  int64_t getSampleTimeAt(int64_t) override {
    return 0;
  }

  bool needSeeking(int64_t, int64_t) override {
    return false;
  }

  void seekTo(int64_t) override {
  }

  void reset() override {
    sampleSent = false;
  }

 private:
  uint8_t sampleData[4] = {0, 0, 0, 1};
  bool sampleSent = false;
};

class ANRVideoDecoder : public VideoDecoder {
 public:
  ANRVideoDecoder(bool stalled, std::atomic_int* decodeCount)
      : stalled(stalled), decodeCount(decodeCount) {
  }

  DecodingResult onSendBytes(void*, size_t, int64_t) override {
    return DecodingResult::Success;
  }

  DecodingResult onEndOfStream() override {
    return DecodingResult::Success;
  }

  DecodingResult onDecodeFrame() override {
    (*decodeCount)++;
    return stalled ? DecodingResult::TryAgainLater : DecodingResult::Success;
  }

  void onFlush() override {
  }

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override {
    if (stalled) {
      return nullptr;
    }
    tgfx::Bitmap bitmap = {};
    if (!bitmap.allocPixels(2, 2, false, false)) {
      return nullptr;
    }
    return bitmap.makeBuffer();
  }

  int64_t presentationTime() override {
    return stalled ? -1 : 0;
  }

 private:
  bool stalled = false;
  std::atomic_int* decodeCount = nullptr;
};

class ANRVideoDecoderFactory : public VideoDecoderFactory {
 public:
  ANRVideoDecoderFactory(bool stalled, std::atomic_int* decodeCount)
      : stalled(stalled), decodeCount(decodeCount) {
  }

  bool isHardwareBacked() const override {
    return false;
  }

  int createCount() const {
    return decoderCreateCount.load();
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat&) const override {
    decoderCreateCount++;
    return std::make_unique<ANRVideoDecoder>(stalled, decodeCount);
  }

 private:
  bool stalled = false;
  std::atomic_int* decodeCount = nullptr;
  mutable std::atomic_int decoderCreateCount = 0;
};

class ANRFailThenSucceedReader : public SequenceReader {
 public:
  int width() const override {
    return 2;
  }

  int height() const override {
    return 2;
  }

  int readCount() const {
    return reads.load();
  }

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(Frame) override {
    if (++reads == 1) {
      return nullptr;
    }
    tgfx::Bitmap bitmap = {};
    if (!bitmap.allocPixels(2, 2, false, false)) {
      return nullptr;
    }
    return bitmap.makeBuffer();
  }

  void onReportPerformance(Performance*, int64_t) override {
  }

 private:
  std::atomic_int reads = 0;
};

class ANRSequenceInfo : public SequenceInfo {
 public:
  ANRSequenceInfo() : SequenceInfo(nullptr) {
  }

  std::shared_ptr<tgfx::Image> makeFrameImage(std::shared_ptr<SequenceReader> reader,
                                              Frame targetFrame, bool,
                                              std::shared_ptr<SequenceReadResult> result) override {
    auto generator =
        std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame, std::move(result));
    return tgfx::Image::MakeFrom(std::move(generator));
  }

  Frame duration() const override {
    return 2;
  }
};

static bool WaitForANRSequenceResult(const std::shared_ptr<SequenceReadResult>& result,
                                     SequenceReadStatus expectedStatus) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    if (result->status.load(std::memory_order_acquire) == expectedStatus) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

PAG_TEST(PAGANRRegressionTest, ManualUpdateRunsOffCallingThread) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  auto callingThread = std::this_thread::get_id();

  animator->update();

  ASSERT_TRUE(listener->waitForUpdateCount(1));
  EXPECT_NE(listener->firstUpdateThread(), callingThread);
}

PAG_TEST(PAGANRRegressionTest, TimedOutUpdateRemainsSerialized) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  listener->blockFirstUpdate();
  auto animator = MakeANRAnimator(listener);
  animator->update();
  ASSERT_TRUE(listener->waitForUpdateCount(1));

  animator->cancel();
  animator->update();
  listener->releaseFirstUpdate();

  ASSERT_TRUE(listener->waitForUpdateCount(2));
  EXPECT_EQ(listener->maximumConcurrentUpdates(), 1);
}

PAG_TEST(PAGANRRegressionTest, StalledDecoderFallsBackOnNextRequest) {
  std::atomic_int stalledDecodeCount = 0;
  std::atomic_int fallbackDecodeCount = 0;
  ANRVideoDecoderFactory stalledFactory(true, &stalledDecodeCount);
  ANRVideoDecoderFactory fallbackFactory(false, &fallbackDecodeCount);
  std::vector<const VideoDecoderFactory*> factories = {&stalledFactory, &fallbackFactory};
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), std::move(factories));

  EXPECT_EQ(reader.readBuffer(0), nullptr);
  EXPECT_EQ(stalledFactory.createCount(), 1);
  EXPECT_EQ(fallbackFactory.createCount(), 0);
  EXPECT_EQ(stalledDecodeCount.load(), 100);

  EXPECT_NE(reader.readBuffer(0), nullptr);
  EXPECT_EQ(fallbackFactory.createCount(), 1);
  EXPECT_EQ(fallbackDecodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, FailedSequenceRequestCanRetry) {
  auto sequence = std::make_shared<ANRSequenceInfo>();
  auto reader = std::make_shared<ANRFailThenSucceedReader>();
  SequenceImageQueue queue(sequence, reader, 0, false);

  std::shared_ptr<SequenceReadResult> firstResult = nullptr;
  ASSERT_NE(queue.getImage(0, &firstResult), nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(firstResult, SequenceReadStatus::Failed));
  queue.invalidateFailedRequest(firstResult->requestID, firstResult->targetFrame);

  std::shared_ptr<SequenceReadResult> secondResult = nullptr;
  ASSERT_NE(queue.getImage(0, &secondResult), nullptr);
  ASSERT_NE(firstResult->requestID, secondResult->requestID);
  EXPECT_TRUE(WaitForANRSequenceResult(secondResult, SequenceReadStatus::Succeeded));
  EXPECT_EQ(reader->readCount(), 2);
}
}  // namespace pag
