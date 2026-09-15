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
#include "rendering/caches/RenderCache.h"
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

  bool hasEnded() {
    std::lock_guard<std::mutex> autoLock(locker);
    return ended;
  }

  bool endArrivedAfterUpdate() {
    std::lock_guard<std::mutex> autoLock(locker);
    return endAfterUpdate;
  }

  void restartOnEnd(PAGAnimator* animator) {
    std::lock_guard<std::mutex> autoLock(locker);
    animatorToRestart = animator;
  }

 protected:
  void onAnimationEnd(PAGAnimator*) override {
    PAGAnimator* animator = nullptr;
    {
      std::lock_guard<std::mutex> autoLock(locker);
      ended = true;
      endAfterUpdate = !updateThreads.empty();
      animator = animatorToRestart;
      animatorToRestart = nullptr;
      condition.notify_all();
    }
    if (animator != nullptr) {
      animator->setDuration(1000000);
      animator->start();
    }
  }

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
  bool ended = false;
  bool endAfterUpdate = false;
  PAGAnimator* animatorToRestart = nullptr;
};

static std::shared_ptr<PAGAnimator> MakeANRAnimator(
    const std::shared_ptr<ANRAnimatorListener>& listener) {
  auto animator = std::shared_ptr<PAGAnimator>(new PAGAnimator(listener));
  animator->weakThis = animator;
  return animator;
}

static void RunANRAnimatorUpdate(PAGAnimator* animator) {
  animator->doUpdate(true);
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

class ANRErrorThenStallDecoder : public VideoDecoder {
 public:
  DecodingResult onSendBytes(void*, size_t, int64_t) override {
    return DecodingResult::Success;
  }

  DecodingResult onEndOfStream() override {
    return DecodingResult::Success;
  }

  DecodingResult onDecodeFrame() override {
    return decodeCount++ == 0 ? DecodingResult::Error : DecodingResult::TryAgainLater;
  }

  void onFlush() override {
  }

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override {
    return nullptr;
  }

  int64_t presentationTime() override {
    return -1;
  }

 private:
  int decodeCount = 0;
};

class ANRErrorThenStallDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat&) const override {
    return std::make_unique<ANRErrorThenStallDecoder>();
  }
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

static std::shared_ptr<tgfx::Image> MakeANRTestImage() {
  tgfx::Bitmap bitmap = {};
  if (!bitmap.allocPixels(2, 2, false, false)) {
    return nullptr;
  }
  return tgfx::Image::MakeFrom(bitmap);
}

static void AddANRTestSnapshot(RenderCache* cache, ID assetID,
                               const std::shared_ptr<tgfx::Image>& image) {
  auto snapshot = new Snapshot(image, tgfx::Matrix::I());
  snapshot->assetID = assetID;
  cache->graphicsMemory += snapshot->memoryUsage();
  cache->snapshotCaches[assetID] = snapshot;
  cache->snapshotLRU.push_front(snapshot);
  cache->snapshotPositions[snapshot] = cache->snapshotLRU.begin();
}

PAG_TEST(PAGANRRegressionTest, ManualUpdateRunsOffCallingThread) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  auto callingThread = std::this_thread::get_id();

  animator->update();

  ASSERT_TRUE(listener->waitForUpdateCount(1));
  EXPECT_NE(listener->firstUpdateThread(), callingThread);
}

PAG_TEST(PAGANRRegressionTest, FinalUpdateRunsOffCallingThread) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  auto callingThread = std::this_thread::get_id();
  animator->_duration = 1;
  animator->_repeatCount = 1;
  animator->_isRunning = true;
  animator->_startTime = 0;

  animator->advance();

  ASSERT_TRUE(listener->waitForUpdateCount(1));
  EXPECT_NE(listener->firstUpdateThread(), callingThread);
  EXPECT_FALSE(listener->hasEnded());

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!listener->hasEnded() && std::chrono::steady_clock::now() < deadline) {
    animator->advance();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  EXPECT_TRUE(listener->hasEnded());
  EXPECT_TRUE(listener->endArrivedAfterUpdate());
}

PAG_TEST(PAGANRRegressionTest, EndWaitsUntilFinalUpdateIsSubmitted) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  animator->_duration = 1;
  animator->_repeatCount = 1;
  animator->_isRunning = true;
  animator->_startTime = 0;

  auto updateEvents = animator->doAdvance();
  EXPECT_EQ(updateEvents.size(), 1U);
  EXPECT_TRUE(animator->doAdvance().empty());

  animator->doUpdate(true);
  ASSERT_TRUE(listener->waitForUpdateCount(1));
  std::vector<int> endEvents = {};
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (endEvents.empty() && std::chrono::steady_clock::now() < deadline) {
    endEvents = animator->doAdvance();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(endEvents.size(), 1U);
}

PAG_TEST(PAGANRRegressionTest, EndCallbackCanRestartAnimation) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  animator->_duration = 1;
  animator->_repeatCount = 1;
  animator->_isRunning = true;
  animator->_startTime = 0;
  listener->restartOnEnd(animator.get());

  animator->advance();
  ASSERT_TRUE(listener->waitForUpdateCount(1));
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!listener->hasEnded() && std::chrono::steady_clock::now() < deadline) {
    animator->advance();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  EXPECT_TRUE(listener->hasEnded());
  EXPECT_TRUE(animator->isRunning());
  animator->cancel();
}

PAG_TEST(PAGANRRegressionTest, UpdateRequestedDuringEndingRunsAfterEnd) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  animator->_duration = 1;
  animator->_repeatCount = 1;
  animator->_isRunning = true;
  animator->_startTime = 0;

  animator->advance();
  animator->setProgress(0.5);
  animator->update();
  ASSERT_TRUE(listener->waitForUpdateCount(1));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (listener->updateCount() < 2 && std::chrono::steady_clock::now() < deadline) {
    animator->advance();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  EXPECT_EQ(listener->updateCount(), 2U);
  EXPECT_TRUE(listener->hasEnded());
}

PAG_TEST(PAGANRRegressionTest, StartDuringEndingRunsAfterEnd) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  animator->_duration = 1000000;
  animator->isEnding = true;
  animator->isAnimating = true;

  animator->start();
  EXPECT_FALSE(animator->isRunning());
  animator->advance();

  EXPECT_TRUE(animator->isRunning());
  animator->cancel();
}

PAG_TEST(PAGANRRegressionTest, EndWaitsForSynchronousFinalUpdate) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  listener->blockFirstUpdate();
  auto animator = MakeANRAnimator(listener);
  animator->isEnding = true;
  animator->endingUpdatePending = true;
  animator->endingFlushSynchronously = true;
  animator->_isSync = true;

  std::thread updateThread(RunANRAnimatorUpdate, animator.get());
  auto updateStarted = listener->waitForUpdateCount(1);
  EXPECT_TRUE(updateStarted);
  if (!updateStarted) {
    listener->releaseFirstUpdate();
    updateThread.join();
    return;
  }
  EXPECT_TRUE(animator->doAdvance().empty());
  listener->releaseFirstUpdate();
  updateThread.join();

  auto endEvents = animator->doAdvance();
  EXPECT_EQ(endEvents.size(), 1U);
}

PAG_TEST(PAGANRRegressionTest, EndingUpdateStaysAsynchronousAfterSyncChange) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  auto animator = MakeANRAnimator(listener);
  auto callingThread = std::this_thread::get_id();
  animator->isEnding = true;
  animator->_isSync = true;

  animator->doUpdate(true);

  ASSERT_TRUE(listener->waitForUpdateCount(1));
  EXPECT_NE(listener->firstUpdateThread(), callingThread);
}

PAG_TEST(PAGANRRegressionTest, FinalUpdateSurvivesSyncChangeTimeout) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  listener->blockFirstUpdate();
  auto animator = MakeANRAnimator(listener);
  animator->_duration = 1;
  animator->_repeatCount = 1;
  animator->_isRunning = true;

  animator->update();
  ASSERT_TRUE(listener->waitForUpdateCount(1));
  animator->setSync(true);
  animator->_startTime = 0;
  animator->advance();
  listener->releaseFirstUpdate();

  ASSERT_TRUE(listener->waitForUpdateCount(2));
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!listener->hasEnded() && std::chrono::steady_clock::now() < deadline) {
    animator->advance();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_TRUE(listener->hasEnded());
  EXPECT_TRUE(listener->endArrivedAfterUpdate());
  EXPECT_EQ(listener->maximumConcurrentUpdates(), 1);
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

PAG_TEST(PAGANRRegressionTest, DecoderStalledDuringRetryFallsBackOnNextRequest) {
  std::atomic_int fallbackDecodeCount = 0;
  ANRErrorThenStallDecoderFactory errorThenStallFactory = {};
  ANRVideoDecoderFactory fallbackFactory(false, &fallbackDecodeCount);
  std::vector<const VideoDecoderFactory*> factories = {&errorThenStallFactory, &fallbackFactory};
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), std::move(factories));

  EXPECT_EQ(reader.readBuffer(0), nullptr);
  EXPECT_EQ(fallbackFactory.createCount(), 0);

  EXPECT_NE(reader.readBuffer(0), nullptr);
  EXPECT_EQ(fallbackFactory.createCount(), 1);
  EXPECT_EQ(fallbackDecodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, FailedStaticSequenceRequestRemovesSnapshot) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto image = MakeANRTestImage();
  ASSERT_NE(image, nullptr);
  constexpr ID assetID = 1;
  AddANRTestSnapshot(&cache, assetID, image);
  auto result = std::make_shared<SequenceReadResult>();
  result->status.store(SequenceReadStatus::Failed, std::memory_order_release);
  cache.usedStaticSequences[assetID] = result;

  cache.checkSequenceDecodeFailure();

  EXPECT_FALSE(cache.hasSnapshot(assetID));
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_TRUE(cache.sequenceCacheInvalidated());
}

PAG_TEST(PAGANRRegressionTest, ClearAllSequenceCachesRemovesStaticSequenceImages) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto image = MakeANRTestImage();
  ASSERT_NE(image, nullptr);
  constexpr ID assetID = 1;
  constexpr ID unrelatedAssetID = 2;
  cache.assetImages[assetID] = image;
  cache.assetImages[unrelatedAssetID] = image;
  cache.decodedAssetImages[assetID] = image;
  cache.staticSequenceResults[assetID] = std::make_shared<SequenceReadResult>();
  AddANRTestSnapshot(&cache, assetID, image);

  cache.clearAllSequenceCaches();

  EXPECT_EQ(cache.assetImages.count(assetID), 0U);
  EXPECT_EQ(cache.decodedAssetImages.count(assetID), 0U);
  EXPECT_EQ(cache.staticSequenceResults.count(assetID), 0U);
  EXPECT_FALSE(cache.hasSnapshot(assetID));
  EXPECT_EQ(cache.assetImages.count(unrelatedAssetID), 1U);
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
