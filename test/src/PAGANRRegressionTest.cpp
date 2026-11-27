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
#include "rendering/sequences/SequenceInfo.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/video/FrameRead.h"
#include "tgfx/core/Bitmap.h"
#include "utils/TestUtils.h"

namespace pag {
#ifdef PAG_TEST_ACCESS_PRIVATE
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
  animator->doUpdate(true, true);
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

class ANRDistinctSampleDemuxer : public ANRVideoDemuxer {
 public:
  int64_t getSampleTimeAt(int64_t time) override {
    return time;
  }
};

class ANRDeliveryBuffer : public tgfx::ImageBuffer {
 public:
  int width() const override {
    return 2;
  }
  int height() const override {
    return 2;
  }
  bool isAlphaOnly() const override {
    return false;
  }
  bool expired() const override {
    return delivery == tgfx::ImageBufferStatus::Expired;
  }
  tgfx::ImageBufferStatus status() const override {
    return delivery;
  }
  const std::shared_ptr<tgfx::ColorSpace>& colorSpace() const override {
    return space;
  }
  tgfx::ImageBufferStatus delivery = tgfx::ImageBufferStatus::Pending;

 protected:
  std::shared_ptr<tgfx::TextureView> onMakeTexture(tgfx::Context*, bool) const override {
    return nullptr;
  }

 private:
  std::shared_ptr<tgfx::ColorSpace> space = nullptr;
};

class ANRFrameMetadataDecoder : public ANRVideoDecoder {
 public:
  explicit ANRFrameMetadataDecoder(std::atomic_int* decodeCount, bool exactTimestamp = true)
      : ANRVideoDecoder(false, decodeCount), exactTimestamp(exactTimestamp) {
  }

  DecodingResult onDecodeFrame() override {
    if (failDecode) {
      return DecodingResult::Error;
    }
    auto status = ANRVideoDecoder::onDecodeFrame();
    outputPending = status == DecodingResult::Success;
    return status;
  }

  void onFlush() override {
    flushCount++;
    generation->fetch_add(1);
    outputPending = false;
  }

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override {
    if (outputPending) {
      outputReleaseCount++;
      outputPending = false;
    }
    if (frame != nullptr && frame->buffer != nullptr) {
      auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(frame->buffer);
      previous->delivery = tgfx::ImageBufferStatus::Expired;
    }
    renderCount++;
    frame = std::make_shared<FrameRead>();
    if (exactTimestamp) {
      frame->options->targetTimestamp = presentationTime() * 1000;
      frame->verifiedTargetTimestamp = frame->options->targetTimestamp;
    }
    frame->sourceGeneration = generation;
    frame->generation = generation->load();
    frame->buffer = std::make_shared<ANRDeliveryBuffer>();
    return frame->buffer;
  }

  std::shared_ptr<FrameRead> getFrameRead() override {
    return provideFrameRead ? frame : nullptr;
  }

  int renderCount = 0;
  int flushCount = 0;
  int outputReleaseCount = 0;
  bool failDecode = false;
  bool provideFrameRead = true;
  std::shared_ptr<std::atomic<uint64_t>> generation = std::make_shared<std::atomic<uint64_t>>(0);

 private:
  bool exactTimestamp = true;
  bool outputPending = false;
  std::shared_ptr<FrameRead> frame = nullptr;
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

class ANRLateErrorDecoder : public VideoDecoder {
 public:
  DecodingResult onSendBytes(void*, size_t, int64_t) override {
    return DecodingResult::Success;
  }

  DecodingResult onEndOfStream() override {
    return DecodingResult::Success;
  }

  DecodingResult onDecodeFrame() override {
    // This delay must exceed VideoReader's total decode budget so fallback is deferred.
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    return DecodingResult::Error;
  }

  void onFlush() override {
  }

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override {
    return nullptr;
  }

  int64_t presentationTime() override {
    return -1;
  }
};

class ANRLateErrorDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

  int createCount() const {
    return decoderCreateCount.load();
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat&) const override {
    decoderCreateCount++;
    return std::make_unique<ANRLateErrorDecoder>();
  }

 private:
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
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(
      Frame, const std::shared_ptr<SequenceReadResult>&) override {
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
    if (rejectImage) {
      return nullptr;
    }
    auto generator =
        std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame, std::move(result));
    return tgfx::Image::MakeFrom(std::move(generator));
  }

  Frame duration() const override {
    return 2;
  }

  bool staticContent() const override {
    return false;
  }

  ID uniqueID() const override {
    return 1;
  }

  bool isVideo() const override {
    return true;
  }

  bool rejectImage = false;
};

struct ANRStaticVideoReaderState {
  int createCount = 0;
  int readCount = 0;
  int destroyCount = 0;
};

class ANRStaticVideoReader : public SequenceReader {
 public:
  explicit ANRStaticVideoReader(std::shared_ptr<ANRStaticVideoReaderState> state)
      : state(std::move(state)) {
    this->state->createCount++;
  }

  ~ANRStaticVideoReader() override {
    generation->fetch_add(1, std::memory_order_release);
    state->destroyCount++;
  }

  int width() const override {
    return 2;
  }

  int height() const override {
    return 2;
  }

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(
      Frame targetFrame, const std::shared_ptr<SequenceReadResult>& result) override {
    state->readCount++;
    auto frame = std::make_shared<FrameRead>();
    frame->options->targetTimestamp = targetFrame;
    frame->verifiedTargetTimestamp = targetFrame;
    frame->sourceGeneration = generation;
    frame->generation = generation->load(std::memory_order_acquire);
    frame->buffer = std::make_shared<ANRDeliveryBuffer>();
    result->setFrameRead(frame);
    return frame->buffer;
  }

  void onReportPerformance(Performance*, int64_t) override {
  }

 private:
  std::shared_ptr<ANRStaticVideoReaderState> state = nullptr;
  std::shared_ptr<std::atomic<uint64_t>> generation = std::make_shared<std::atomic<uint64_t>>(0);
};

class ANRStaticVideoSequenceInfo : public SequenceInfo {
 public:
  ANRStaticVideoSequenceInfo() : SequenceInfo(nullptr) {
  }

  std::shared_ptr<SequenceReader> makeReader(std::shared_ptr<File>, PAGFile*, bool) override {
    auto reader = std::make_shared<ANRStaticVideoReader>(state);
    weakReader = reader;
    return reader;
  }

  bool isVideo() const override {
    return true;
  }

  std::shared_ptr<ANRStaticVideoReaderState> state = std::make_shared<ANRStaticVideoReaderState>();
  std::weak_ptr<SequenceReader> weakReader = {};
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

  animator->doUpdate(true, true);
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
  animator->setSync(true);

  animator->doUpdate(true, true);

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

PAG_TEST(PAGANRRegressionTest, TickerUpdateDuringAsyncFlushIsThrottled) {
  auto listener = std::make_shared<ANRAnimatorListener>();
  listener->blockFirstUpdate();
  auto animator = MakeANRAnimator(listener);
  animator->_isRunning = true;

  animator->doUpdate(true, true);
  ASSERT_TRUE(listener->waitForUpdateCount(1));

  // Ticker-driven updates arriving while the first flush is still running are dropped instead of
  // being merged into an immediate re-flush. Merging used to keep the renderer 100% busy without
  // idle time once a single flush took longer than one vsync period (issue #3738).
  RunANRAnimatorUpdate(animator.get());
  RunANRAnimatorUpdate(animator.get());
  EXPECT_EQ(listener->updateCount(), 1U);

  listener->releaseFirstUpdate();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(listener->updateCount(), 1U);

  // The next ticker tick re-drives the flush once the previous task has exited.
  animator->doUpdate(true, true);
  ASSERT_TRUE(listener->waitForUpdateCount(2));
  animator->cancel();
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
  EXPECT_GE(stalledDecodeCount.load(), 1);

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

PAG_TEST(PAGANRRegressionTest, DecoderErrorAfterDeadlineFallsBackOnNextRequest) {
  std::atomic_int fallbackDecodeCount = 0;
  ANRLateErrorDecoderFactory lateErrorFactory = {};
  ANRVideoDecoderFactory fallbackFactory(false, &fallbackDecodeCount);
  std::vector<const VideoDecoderFactory*> factories = {&lateErrorFactory, &fallbackFactory};
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), std::move(factories));

  EXPECT_EQ(reader.readBuffer(0), nullptr);
  EXPECT_EQ(lateErrorFactory.createCount(), 1);
  EXPECT_EQ(fallbackFactory.createCount(), 0);

  EXPECT_NE(reader.readBuffer(0), nullptr);
  EXPECT_EQ(lateErrorFactory.createCount(), 1);
  EXPECT_EQ(fallbackFactory.createCount(), 1);
  EXPECT_EQ(fallbackDecodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, StaticVideoGeneratorRetainsReaderUntilDeferredDelivery) {
  auto info = std::make_shared<ANRStaticVideoSequenceInfo>();
  auto result = std::make_shared<SequenceReadResult>();
  auto generator = std::make_shared<StaticSequenceGenerator>(nullptr, info, 2, 2, false, result);

  EXPECT_EQ(info->state->createCount, 1);
  EXPECT_EQ(info->state->readCount, 0);
  EXPECT_EQ(info->state->destroyCount, 0);
  EXPECT_FALSE(info->weakReader.expired());
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Pending);

  auto buffer = std::static_pointer_cast<ANRDeliveryBuffer>(generator->makeBuffer());
  ASSERT_NE(buffer, nullptr);
  auto frame = result->getFrameRead();
  ASSERT_NE(frame->sourceGeneration, nullptr);
  EXPECT_EQ(frame->buffer, buffer);
  EXPECT_EQ(frame->options->targetTimestamp, 0);
  EXPECT_EQ(frame->generation, 0U);
  EXPECT_EQ(info->state->readCount, 1);
  EXPECT_EQ(info->state->destroyCount, 0);
  EXPECT_FALSE(info->weakReader.expired());
  EXPECT_EQ(result->status.load(std::memory_order_acquire), SequenceReadStatus::Succeeded);
  EXPECT_EQ(buffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_TRUE(frame->isCurrent());
  EXPECT_FALSE(frame->hasValidBuffer());
  EXPECT_FALSE(frame->matchesTarget());

  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  EXPECT_TRUE(frame->isCurrent());
  EXPECT_TRUE(frame->hasValidBuffer());
  EXPECT_TRUE(frame->matchesTarget());
  EXPECT_EQ(info->state->createCount, 1);
  EXPECT_EQ(info->state->destroyCount, 0);

  generator = nullptr;
  EXPECT_TRUE(info->weakReader.expired());
  EXPECT_EQ(info->state->destroyCount, 1);
  EXPECT_EQ(frame->sourceGeneration->load(std::memory_order_acquire), 1U);
  EXPECT_EQ(buffer->status(), tgfx::ImageBufferStatus::Ready);
  EXPECT_FALSE(frame->isCurrent());
  EXPECT_FALSE(frame->hasValidBuffer());
  EXPECT_FALSE(frame->matchesTarget());
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

PAG_TEST(PAGANRRegressionTest, StaticSequenceUsesAssetMipmaps) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  constexpr ID assetID = 1;
  stage->scaleFactorCache[assetID] = {1.0f, 0.1f};
  auto image = MakeANRTestImage();
  ASSERT_NE(image, nullptr);
  EXPECT_FALSE(image->hasMipmaps());

  auto mipmapped = cache.applyAssetMipmaps(assetID, image);

  ASSERT_NE(mipmapped, nullptr);
  EXPECT_TRUE(mipmapped->hasMipmaps());
}

PAG_TEST(PAGANRRegressionTest, DisablingVideoPreservesStaticBitmapCache) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto image = MakeANRTestImage();
  ASSERT_NE(image, nullptr);
  constexpr ID videoAssetID = 1;
  constexpr ID bitmapAssetID = 2;
  cache.assetImages[videoAssetID] = image;
  cache.assetImages[bitmapAssetID] = image;
  cache.staticSequenceResults[videoAssetID] = std::make_shared<SequenceReadResult>();
  cache.staticSequenceResults[bitmapAssetID] = std::make_shared<SequenceReadResult>();
  cache.staticVideoSequenceIDs.insert(videoAssetID);

  cache.setVideoEnabled(false);

  EXPECT_EQ(cache.assetImages.count(videoAssetID), 0U);
  EXPECT_EQ(cache.staticSequenceResults.count(videoAssetID), 0U);
  EXPECT_EQ(cache.assetImages.count(bitmapAssetID), 1U);
  EXPECT_EQ(cache.staticSequenceResults.count(bitmapAssetID), 1U);
  EXPECT_TRUE(cache.sequenceCacheInvalidated());
}

PAG_TEST(PAGANRRegressionTest, FailedSequenceRequestsBackOffAndRecover) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  constexpr ID assetID = 1;

  cache.recordSequenceFailure(assetID);
  cache.beginFrame();
  EXPECT_FALSE(cache.canRetrySequence(assetID));
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  cache.sequenceFailureStates[assetID].retryAfterTime = 0;
  EXPECT_TRUE(cache.canRetrySequence(assetID));

  cache.recordSequenceSuccess(assetID);
  EXPECT_EQ(cache.sequenceFailureStates.count(assetID), 0U);
}

PAG_TEST(PAGANRRegressionTest, FailedSequenceRequestCanRetry) {
  auto sequence = std::make_shared<ANRSequenceInfo>();
  auto reader = std::make_shared<ANRFailThenSucceedReader>();
  SequenceImageQueue queue(sequence, reader, 0, false);

  std::shared_ptr<SequenceReadResult> firstResult = nullptr;
  ASSERT_NE(queue.getImage(0, &firstResult), nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(firstResult, SequenceReadStatus::Failed));

  std::shared_ptr<SequenceReadResult> secondResult = nullptr;
  ASSERT_NE(queue.getImage(0, &secondResult), nullptr);
  ASSERT_NE(firstResult->requestID, secondResult->requestID);
  EXPECT_TRUE(WaitForANRSequenceResult(secondResult, SequenceReadStatus::Succeeded));
  EXPECT_EQ(reader->readCount(), 2);
}

PAG_TEST(PAGANRRegressionTest, FailedPreparedSameFrameReacquiresWithoutExternalInvalidation) {
  auto sequence = std::make_shared<ANRSequenceInfo>();
  auto reader = std::make_shared<ANRFailThenSucceedReader>();
  SequenceImageQueue queue(sequence, reader, 0, false);
  std::shared_ptr<SequenceReadResult> first = nullptr;
  queue.prepare(0, &first);
  ASSERT_NE(first, nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(first, SequenceReadStatus::Failed));
  std::shared_ptr<SequenceReadResult> retry = nullptr;
  queue.prepare(0, &retry);
  ASSERT_NE(retry, nullptr);
  EXPECT_NE(first->requestID, retry->requestID);
  ASSERT_TRUE(WaitForANRSequenceResult(retry, SequenceReadStatus::Succeeded));
  EXPECT_TRUE(retry->getFrameRead()->hasValidBuffer());
  EXPECT_NE(queue.getImage(0), nullptr);
  EXPECT_EQ(queue.currentResult, retry);
  EXPECT_EQ(reader->readCount(), 2);
}

PAG_TEST(PAGANRRegressionTest, PendingTextureInvalidatesFrameWithoutDecodeBackoff) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto sequence = std::make_shared<ANRSequenceInfo>();
  auto reader = std::make_shared<ANRFailThenSucceedReader>();
  SequenceImageQueue queue(sequence, reader, 0, false);
  constexpr ID assetID = 1;
  auto image = MakeANRTestImage();
  ASSERT_NE(image, nullptr);
  auto result = std::make_shared<SequenceReadResult>();
  result->requestID = 1;
  result->targetFrame = 1;
  result->status.store(SequenceReadStatus::Succeeded);
  auto frame = result->getFrameRead();
  frame->options->targetTimestamp = 1000;
  frame->verifiedTargetTimestamp = 1000;
  frame->buffer = std::make_shared<ANRDeliveryBuffer>();
  queue.currentFrame = 1;
  queue.currentImage = image;
  queue.currentResult = result;
  auto& usage = cache.usedSequences[assetID][1];
  usage.queue = &queue;
  usage.result = result;
  usage.drawn = true;
  AddANRTestSnapshot(&cache, assetID, image);
  constexpr ID parentAssetID = 2;
  constexpr ID validAssetID = 3;
  AddANRTestSnapshot(&cache, parentAssetID, image);
  AddANRTestSnapshot(&cache, validAssetID, image);
  cache.frameSnapshotIDs.insert(parentAssetID);

  cache.checkSequenceDecodeFailure();
  cache.prepareNextFrame();

  EXPECT_TRUE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_TRUE(cache.sequenceCacheInvalidated());
  EXPECT_EQ(cache.sequenceFailureStates.count(assetID), 0U);
  EXPECT_FALSE(cache.hasSnapshot(assetID));
  EXPECT_FALSE(cache.hasSnapshot(parentAssetID));
  EXPECT_TRUE(cache.hasSnapshot(validAssetID));
  EXPECT_EQ(queue.currentFrame, -1);
  EXPECT_EQ(queue.currentImage, nullptr);
  EXPECT_EQ(queue.preparedImage, nullptr);
  EXPECT_EQ(reader->readCount(), 0);
  EXPECT_EQ(frame->buffer->status(), tgfx::ImageBufferStatus::Pending);

  cache.beginFrame();
  auto recovered = std::make_shared<SequenceReadResult>();
  recovered->status.store(SequenceReadStatus::Succeeded);
  recovered->setFrameRead(frame);
  auto delivered = std::static_pointer_cast<ANRDeliveryBuffer>(frame->buffer);
  delivered->delivery = tgfx::ImageBufferStatus::Ready;
  auto& recoveredUsage = cache.usedSequences[assetID][1];
  recoveredUsage.queue = &queue;
  recoveredUsage.result = recovered;
  recoveredUsage.drawn = true;
  cache.checkSequenceDecodeFailure();

  EXPECT_FALSE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_TRUE(cache.canRetrySequence(assetID));
}

PAG_TEST(PAGANRRegressionTest, PreparedTextureDoesNotBlockCurrentFrame) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto result = std::make_shared<SequenceReadResult>();
  result->status.store(SequenceReadStatus::Succeeded);
  auto frame = result->getFrameRead();
  frame->options->targetTimestamp = 1000;
  frame->verifiedTargetTimestamp = 1000;
  frame->buffer = std::make_shared<ANRDeliveryBuffer>();
  cache.usedSequences[1][1].result = result;

  cache.checkSequenceDecodeFailure();

  EXPECT_FALSE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Succeeded);
}

PAG_TEST(PAGANRRegressionTest, VideoTexturesShareOneWaitingDeadline) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto first = std::make_shared<SequenceReadResult>();
  auto second = std::make_shared<SequenceReadResult>();
  auto staticResult = std::make_shared<SequenceReadResult>();
  cache.usedSequences[1][0].result = first;
  cache.usedSequences[1][0].drawn = true;
  cache.usedSequences[2][0].result = second;
  cache.usedSequences[2][0].drawn = true;
  cache.usedStaticSequences[3] = staticResult;
  constexpr int64_t waitTime = 30000;
  cache.setSequenceFrameWaitTime(waitTime);
  auto start = tgfx::Clock::Now();

  cache.prepareSequenceFrameReads();

  auto deadline = first->getFrameRead()->options->deadline.load();
  EXPECT_GE(deadline, start + waitTime);
  EXPECT_LE(deadline, tgfx::Clock::Now() + waitTime);
  EXPECT_EQ(second->getFrameRead()->options->deadline.load(), deadline);
  EXPECT_EQ(staticResult->getFrameRead()->options->deadline.load(), deadline);
  cache.setSequenceFrameWaitTime(0);
  cache.prepareSequenceFrameReads();
  EXPECT_EQ(first->getFrameRead()->options->deadline.load(), 0);
  EXPECT_EQ(second->getFrameRead()->options->deadline.load(), 0);
  EXPECT_EQ(staticResult->getFrameRead()->options->deadline.load(), 0);
}

PAG_TEST(PAGANRRegressionTest, DeferredFrameQuerySurvivesQueueRetry) {
  std::atomic_int decodeCount = 0;
  ANRVideoDecoderFactory factory(false, &decodeCount);
  auto reader = std::make_shared<VideoReader>(std::make_unique<ANRVideoDemuxer>(),
                                              std::vector<const VideoDecoderFactory*>{&factory});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader->videoDecoder = decoder;
  auto sequence = std::make_shared<ANRSequenceInfo>();
  SequenceImageQueue queue(sequence, reader, 0, false);
  std::shared_ptr<SequenceReadResult> first = nullptr;
  ASSERT_NE(queue.getImage(0, &first), nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(first, SequenceReadStatus::Succeeded));
  auto frame = first->getFrameRead();
  EXPECT_EQ(frame, decoder->getFrameRead());
  EXPECT_EQ(frame->options->targetTimestamp, 0);
  ASSERT_NE(frame->buffer, nullptr);
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto& usage = cache.usedSequences[1][0];
  usage.queue = &queue;
  usage.result = first;
  usage.drawn = true;
  cache.checkSequenceDecodeFailure();
  cache.prepareNextFrame();
  ASSERT_TRUE(cache.hasSequenceFramePending());
  EXPECT_EQ(queue.currentImage, nullptr);
  EXPECT_EQ(cache.sequenceFailureStates.size(), 0u);

  std::shared_ptr<SequenceReadResult> retry = nullptr;
  ASSERT_NE(queue.getImage(0, &retry), nullptr);
  retry->setFrameReadDeadline(30000);
  ASSERT_TRUE(WaitForANRSequenceResult(retry, SequenceReadStatus::Succeeded));
  auto retryFrame = retry->getFrameRead();
  EXPECT_NE(first->requestID, retry->requestID);
  EXPECT_EQ(retryFrame, frame);
  EXPECT_EQ(retryFrame->options->deadline.load(), 30000);
  EXPECT_EQ(retryFrame->buffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_EQ(decoder->renderCount, 1);
  EXPECT_EQ(decodeCount.load(), 1);

  auto delivered = std::static_pointer_cast<ANRDeliveryBuffer>(retryFrame->buffer);
  delivered->delivery = tgfx::ImageBufferStatus::Ready;
  cache.beginFrame();
  auto& recovered = cache.usedSequences[1][0];
  recovered.queue = &queue;
  recovered.result = retry;
  recovered.drawn = true;
  cache.checkSequenceDecodeFailure();
  EXPECT_FALSE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(queue.currentResult, retry);
}

PAG_TEST(PAGANRRegressionTest, SameSampleRetryableStatusReacquiresWithoutReleasingOutputTwice) {
  for (auto status : {tgfx::ImageBufferStatus::Timeout, tgfx::ImageBufferStatus::Failed}) {
    std::atomic_int decodeCount = 0;
    VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
    auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
    reader.videoDecoder = decoder;
    auto first = std::make_shared<SequenceReadResult>();
    auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
    ASSERT_NE(previous, nullptr);
    previous->delivery = status;
    EXPECT_FALSE(previous->expired());
    EXPECT_FALSE(first->getFrameRead()->matchesTarget());

    auto retry = std::make_shared<SequenceReadResult>();
    auto next = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, retry));
    ASSERT_NE(next, nullptr);
    EXPECT_NE(next, previous);
    EXPECT_NE(retry->getFrameRead(), first->getFrameRead());
    EXPECT_TRUE(previous->expired());
    EXPECT_EQ(retry->getFrameRead()->verifiedTargetTimestamp, 0);
    EXPECT_EQ(decoder->renderCount, 2);
    EXPECT_EQ(decoder->outputReleaseCount, 1);
    EXPECT_EQ(decoder->flushCount, 0);
    EXPECT_EQ(decodeCount.load(), 1);

    next->delivery = tgfx::ImageBufferStatus::Ready;
    auto delivered = std::make_shared<SequenceReadResult>();
    EXPECT_EQ(reader.readBuffer(0, delivered), next);
    EXPECT_TRUE(delivered->getFrameRead()->matchesTarget());
    EXPECT_EQ(decoder->renderCount, 2);
    EXPECT_EQ(decoder->outputReleaseCount, 1);
  }
}

PAG_TEST(PAGANRRegressionTest, SameSampleExpiredOrdinaryBufferReacquiresWithoutExactPTS) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount, false);
  decoder->provideFrameRead = false;
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
  ASSERT_NE(previous, nullptr);
  EXPECT_EQ(first->getFrameRead()->buffer, previous);
  EXPECT_EQ(first->getFrameRead()->options->targetTimestamp, -1);
  EXPECT_FALSE(first->getFrameRead()->matchesTarget());
  previous->delivery = tgfx::ImageBufferStatus::Expired;

  auto retry = std::make_shared<SequenceReadResult>();
  auto next = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, retry));
  ASSERT_NE(next, nullptr);
  EXPECT_NE(next, previous);
  EXPECT_EQ(retry->getFrameRead()->buffer, next);
  EXPECT_EQ(retry->getFrameRead()->options->targetTimestamp, -1);
  next->delivery = tgfx::ImageBufferStatus::Ready;
  EXPECT_TRUE(retry->getFrameRead()->matchesTarget());
  EXPECT_EQ(decoder->renderCount, 2);
  EXPECT_EQ(decoder->outputReleaseCount, 1);
  EXPECT_EQ(decoder->flushCount, 0);
  EXPECT_EQ(decodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, SameSampleGenerationChangeCannotReusePreviousDelivery) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
  ASSERT_NE(previous, nullptr);
  previous->delivery = tgfx::ImageBufferStatus::Ready;
  ASSERT_TRUE(first->getFrameRead()->matchesTarget());
  decoder->generation->fetch_add(1);

  auto retry = std::make_shared<SequenceReadResult>();
  auto next = reader.readBuffer(0, retry);
  EXPECT_NE(next, previous);
  EXPECT_FALSE(first->getFrameRead()->isCurrent());
  EXPECT_EQ(retry->getFrameRead()->generation, 1U);
  EXPECT_EQ(decoder->renderCount, 2);
  EXPECT_EQ(decoder->outputReleaseCount, 2);
  EXPECT_EQ(decodeCount.load(), 2);
}

PAG_TEST(PAGANRRegressionTest, PendingPreviousBufferIsNotADecodeFallback) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto pending = reader.readBuffer(0, first);
  ASSERT_NE(pending, nullptr);
  ASSERT_EQ(pending->status(), tgfx::ImageBufferStatus::Pending);
  decoder->failDecode = true;
  reader.currentDecodedTime = INT64_MIN;
  reader.currentRenderedTime = INT64_MIN;

  auto retry = std::make_shared<SequenceReadResult>();
  EXPECT_EQ(reader.readBuffer(0, retry), nullptr);
  EXPECT_EQ(retry->status.load(), SequenceReadStatus::Failed);
  EXPECT_EQ(reader.lastBuffer, nullptr);
  EXPECT_FALSE(first->getFrameRead()->hasValidBuffer());
}

PAG_TEST(PAGANRRegressionTest, OldGenerationReadyBufferIsNotADecodeFallback) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
  ASSERT_NE(previous, nullptr);
  previous->delivery = tgfx::ImageBufferStatus::Ready;
  decoder->generation->fetch_add(1);
  decoder->failDecode = true;

  auto retry = std::make_shared<SequenceReadResult>();
  EXPECT_EQ(reader.readBuffer(0, retry), nullptr);
  EXPECT_EQ(retry->status.load(), SequenceReadStatus::Failed);
  EXPECT_FALSE(first->getFrameRead()->isCurrent());
}

PAG_TEST(PAGANRRegressionTest, SameFrameExpiredQueueCacheReacquiresAndRecovers) {
  std::atomic_int decodeCount = 0;
  auto reader = std::make_shared<VideoReader>(std::make_unique<ANRVideoDemuxer>(),
                                              std::vector<const VideoDecoderFactory*>{});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader->videoDecoder = decoder;
  SequenceImageQueue queue(std::make_shared<ANRSequenceInfo>(), reader, 0, false);
  std::shared_ptr<SequenceReadResult> first = nullptr;
  auto firstImage = queue.getImage(0, &first);
  ASSERT_NE(firstImage, nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(first, SequenceReadStatus::Succeeded));
  EXPECT_EQ(queue.getCachedImage(0), nullptr);
  auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(first->getFrameRead()->buffer);
  ASSERT_NE(previous, nullptr);
  previous->delivery = tgfx::ImageBufferStatus::Ready;
  EXPECT_EQ(queue.getCachedImage(0), firstImage);
  previous->delivery = tgfx::ImageBufferStatus::Expired;
  EXPECT_EQ(queue.getCachedImage(0), nullptr);

  std::shared_ptr<SequenceReadResult> retry = nullptr;
  ASSERT_NE(queue.getImage(0, &retry), nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(retry, SequenceReadStatus::Succeeded));
  EXPECT_NE(retry->requestID, first->requestID);
  auto next = std::static_pointer_cast<ANRDeliveryBuffer>(retry->getFrameRead()->buffer);
  ASSERT_NE(next, nullptr);
  EXPECT_NE(next, previous);
  next->delivery = tgfx::ImageBufferStatus::Ready;
  std::shared_ptr<SequenceReadResult> cached = nullptr;
  EXPECT_NE(queue.getCachedImage(0, &cached), nullptr);
  EXPECT_EQ(cached, retry);
  EXPECT_EQ(queue.getImage(0), queue.currentImage);
  EXPECT_EQ(decoder->renderCount, 2);
  EXPECT_EQ(decoder->outputReleaseCount, 1);
  EXPECT_EQ(decodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, SameFrameRetryableQueueCacheReacquiresAndRecovers) {
  for (auto status : {tgfx::ImageBufferStatus::Timeout, tgfx::ImageBufferStatus::Failed}) {
    std::atomic_int decodeCount = 0;
    auto reader = std::make_shared<VideoReader>(std::make_unique<ANRVideoDemuxer>(),
                                                std::vector<const VideoDecoderFactory*>{});
    auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
    reader->videoDecoder = decoder;
    SequenceImageQueue queue(std::make_shared<ANRSequenceInfo>(), reader, 0, false);
    std::shared_ptr<SequenceReadResult> first = nullptr;
    auto firstImage = queue.getImage(0, &first);
    ASSERT_NE(firstImage, nullptr);
    ASSERT_TRUE(WaitForANRSequenceResult(first, SequenceReadStatus::Succeeded));
    auto previous = std::static_pointer_cast<ANRDeliveryBuffer>(first->getFrameRead()->buffer);
    ASSERT_NE(previous, nullptr);
    previous->delivery = status;
    EXPECT_FALSE(previous->expired());
    EXPECT_EQ(queue.getCachedImage(0), nullptr);
    queue.prepareNextImage();
    EXPECT_EQ(queue.preparedImage, nullptr);

    std::shared_ptr<SequenceReadResult> retry = nullptr;
    auto retryImage = queue.getImage(0, &retry);
    ASSERT_NE(retryImage, nullptr);
    EXPECT_NE(retryImage, firstImage);
    ASSERT_TRUE(WaitForANRSequenceResult(retry, SequenceReadStatus::Succeeded));
    EXPECT_NE(retry->requestID, first->requestID);
    auto next = std::static_pointer_cast<ANRDeliveryBuffer>(retry->getFrameRead()->buffer);
    ASSERT_NE(next, nullptr);
    EXPECT_NE(next, previous);
    EXPECT_EQ(next->status(), tgfx::ImageBufferStatus::Pending);
    next->delivery = tgfx::ImageBufferStatus::Ready;
    EXPECT_TRUE(retry->getFrameRead()->matchesTarget());
    EXPECT_EQ(queue.getCachedImage(0), retryImage);
    EXPECT_EQ(queue.getImage(0), retryImage);
    EXPECT_EQ(decoder->renderCount, 2);
    EXPECT_EQ(decoder->outputReleaseCount, 1);
    EXPECT_EQ(decodeCount.load(), 1);
  }
}

PAG_TEST(PAGANRRegressionTest, FailedNewImageCannotReusePreviousCachedPixels) {
  std::atomic_int decodeCount = 0;
  auto reader = std::make_shared<VideoReader>(std::make_unique<ANRVideoDemuxer>(),
                                              std::vector<const VideoDecoderFactory*>{});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader->videoDecoder = decoder;
  auto sequence = std::make_shared<ANRSequenceInfo>();
  SequenceImageQueue queue(sequence, reader, 0, false);
  std::shared_ptr<SequenceReadResult> first = nullptr;
  auto image = queue.getImage(0, &first);
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(first, SequenceReadStatus::Succeeded));
  auto buffer = std::static_pointer_cast<ANRDeliveryBuffer>(first->getFrameRead()->buffer);
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  ASSERT_EQ(queue.getCachedImage(0), image);
  sequence->rejectImage = true;

  std::shared_ptr<SequenceReadResult> failed = nullptr;
  EXPECT_EQ(queue.getImage(1, &failed), nullptr);
  ASSERT_NE(failed, nullptr);
  EXPECT_EQ(failed->status.load(), SequenceReadStatus::Failed);
  EXPECT_EQ(queue.currentImage, nullptr);
  EXPECT_EQ(queue.currentResult, failed);
  EXPECT_EQ(queue.getCachedImage(1), nullptr);
  EXPECT_EQ(queue.getCachedImage(0), nullptr);
}

PAG_TEST(PAGANRRegressionTest, DifferentSampleFallbackCannotBorrowPreviousTargetVerification) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRDistinctSampleDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto buffer = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
  ASSERT_NE(buffer, nullptr);
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  auto original = first->getFrameRead();
  ASSERT_TRUE(original->matchesTarget());
  decoder->failDecode = true;

  auto fallback = std::make_shared<SequenceReadResult>();
  EXPECT_EQ(reader.readBuffer(1, fallback), buffer);
  EXPECT_EQ(fallback->status.load(), SequenceReadStatus::Fallback);
  auto frame = fallback->getFrameRead();
  EXPECT_NE(frame, original);
  EXPECT_TRUE(frame->hasValidBuffer());
  EXPECT_FALSE(frame->matchesTarget());
  EXPECT_EQ(frame->options->targetTimestamp, original->options->targetTimestamp);
  EXPECT_EQ(frame->verifiedTargetTimestamp, -1);
  EXPECT_EQ(original->verifiedTargetTimestamp, 0);
  EXPECT_TRUE(original->matchesTarget());
}

PAG_TEST(PAGANRRegressionTest, SameSampleFallbackPreservesOriginalTargetVerification) {
  std::atomic_int decodeCount = 0;
  VideoReader reader(std::make_unique<ANRVideoDemuxer>(), {});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader.videoDecoder = decoder;
  auto first = std::make_shared<SequenceReadResult>();
  auto buffer = std::static_pointer_cast<ANRDeliveryBuffer>(reader.readBuffer(0, first));
  ASSERT_NE(buffer, nullptr);
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  auto original = first->getFrameRead();
  ASSERT_TRUE(original->matchesTarget());
  reader.fallbackPending = true;

  auto fallback = std::make_shared<SequenceReadResult>();
  EXPECT_EQ(reader.readBuffer(1, fallback), buffer);
  EXPECT_EQ(fallback->status.load(), SequenceReadStatus::Fallback);
  EXPECT_EQ(fallback->getFrameRead(), original);
  EXPECT_TRUE(fallback->getFrameRead()->matchesTarget());
  EXPECT_EQ(original->verifiedTargetTimestamp, 0);
}

PAG_TEST(PAGANRRegressionTest, PrefetchCannotReplacePendingCurrentCredential) {
  std::atomic_int decodeCount = 0;
  auto reader = std::make_shared<VideoReader>(std::make_unique<ANRVideoDemuxer>(),
                                              std::vector<const VideoDecoderFactory*>{});
  auto decoder = new ANRFrameMetadataDecoder(&decodeCount);
  reader->videoDecoder = decoder;
  SequenceImageQueue queue(std::make_shared<ANRSequenceInfo>(), reader, 0, false);
  std::shared_ptr<SequenceReadResult> result = nullptr;
  ASSERT_NE(queue.getImage(0, &result), nullptr);
  ASSERT_TRUE(WaitForANRSequenceResult(result, SequenceReadStatus::Succeeded));
  auto frame = result->getFrameRead();
  ASSERT_EQ(frame->buffer->status(), tgfx::ImageBufferStatus::Pending);

  queue.prepareNextImage();

  EXPECT_EQ(queue.preparedImage, nullptr);
  EXPECT_EQ(queue.preparedResult, nullptr);
  EXPECT_EQ(queue.nextRequestID, 1U);
  EXPECT_EQ(queue.currentResult->getFrameRead(), frame);
  EXPECT_EQ(frame->buffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_FALSE(frame->buffer->expired());
  EXPECT_EQ(decoder->renderCount, 1);
  EXPECT_EQ(decoder->outputReleaseCount, 1);
  EXPECT_EQ(decodeCount.load(), 1);
}

PAG_TEST(PAGANRRegressionTest, DrawnUnknownPTSSourceMustDeliverPixels) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto result = std::make_shared<SequenceReadResult>();
  result->status.store(SequenceReadStatus::Succeeded);
  auto buffer = std::make_shared<ANRDeliveryBuffer>();
  result->getFrameRead()->buffer = buffer;
  cache.usedSequences[1][0].result = result;
  cache.usedSequences[1][0].drawn = true;

  cache.checkSequenceDecodeFailure();

  EXPECT_TRUE(cache.hasSequenceFramePending());
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Failed);
  EXPECT_EQ(cache.sequenceFailureStates.size(), 0U);
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  result->status.store(SequenceReadStatus::Succeeded);
  cache.beginFrame();
  cache.usedSequences[1][0].result = result;
  cache.usedSequences[1][0].drawn = true;
  cache.checkSequenceDecodeFailure();
  EXPECT_FALSE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceFrameUnavailable());
}

PAG_TEST(PAGANRRegressionTest, DrawnPendingPreparationCannotPassDefaultFrameRead) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto result = std::make_shared<SequenceReadResult>();
  cache.usedSequences[1][0].result = result;
  cache.usedSequences[1][0].drawn = true;

  cache.checkSequenceDecodeFailure();

  EXPECT_TRUE(cache.hasSequenceFramePending());
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_FALSE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Pending);
}

PAG_TEST(PAGANRRegressionTest, SucceededPreparationWithoutPixelsCannotPublish) {
  for (auto staticSequence : {false, true}) {
    auto stage = PAGStage::Make(2, 2);
    RenderCache cache(stage.get());
    auto result = std::make_shared<SequenceReadResult>();
    result->status.store(SequenceReadStatus::Succeeded);
    if (staticSequence) {
      cache.usedStaticSequences[1] = result;
    } else {
      cache.usedSequences[1][0].result = result;
      cache.usedSequences[1][0].drawn = true;
    }

    cache.checkSequenceDecodeFailure();

    EXPECT_TRUE(cache.hasSequenceFramePending());
    EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
    EXPECT_FALSE(cache.hasSequenceDecodeFailure());
    EXPECT_EQ(result->status.load(), SequenceReadStatus::Failed);
    EXPECT_EQ(cache.sequenceFailureStates.size(), 0U);
  }
}

PAG_TEST(PAGANRRegressionTest, FallbackPublicationRequiresValidDeliveryAndVerifiedKnownTarget) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto result = std::make_shared<SequenceReadResult>();
  result->status.store(SequenceReadStatus::Fallback);
  auto buffer = std::make_shared<ANRDeliveryBuffer>();
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  result->getFrameRead()->buffer = buffer;
  cache.usedSequences[1][0].result = result;
  cache.usedSequences[1][0].drawn = true;

  cache.checkSequenceDecodeFailure();

  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_FALSE(cache.hasSequenceFramePending());
  EXPECT_FALSE(cache.hasSequenceFrameUnavailable());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Fallback);
  result->getFrameRead()->options->targetTimestamp = 1000;
  cache.beginFrame();
  cache.usedSequences[1][0].result = result;
  cache.usedSequences[1][0].drawn = true;
  cache.checkSequenceDecodeFailure();
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_TRUE(cache.hasSequenceFramePending());
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Failed);
  EXPECT_EQ(result->getFrameRead()->verifiedTargetTimestamp, -1);
}

PAG_TEST(PAGANRRegressionTest, PendingOrExpiredFallbackCannotPublish) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto result = std::make_shared<SequenceReadResult>();
  result->status.store(SequenceReadStatus::Fallback);
  auto buffer = std::make_shared<ANRDeliveryBuffer>();
  result->getFrameRead()->buffer = buffer;
  cache.usedStaticSequences[1] = result;

  cache.checkSequenceDecodeFailure();

  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_EQ(result->status.load(), SequenceReadStatus::Failed);
  buffer->delivery = tgfx::ImageBufferStatus::Expired;
  result->status.store(SequenceReadStatus::Fallback);
  cache.beginFrame();
  cache.usedStaticSequences[1] = result;
  cache.checkSequenceDecodeFailure();
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
}

PAG_TEST(PAGANRRegressionTest, CachedFallbackKeepsCooldownWithoutBecomingDecodeSuccess) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto sequence = std::make_shared<ANRSequenceInfo>();
  SequenceImageQueue queue(sequence, std::make_shared<ANRFailThenSucceedReader>(), 0, false);
  auto original = std::make_shared<SequenceReadResult>();
  original->requestID = 1;
  original->targetFrame = 0;
  original->status.store(SequenceReadStatus::Succeeded);
  auto buffer = std::make_shared<ANRDeliveryBuffer>();
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  original->getFrameRead()->buffer = buffer;
  queue.currentImage = MakeANRTestImage();
  queue.currentFrame = 0;
  queue.currentResult = original;
  cache.recordSequenceFailure(1);
  auto retryTime = cache.sequenceFailureStates[1].retryAfterTime;
  auto failures = cache.sequenceFailureStates[1].consecutiveFailures;
  cache.usedSequences[1][1].queue = &queue;

  EXPECT_EQ(cache.getSequenceImage(sequence, 1), queue.currentImage);
  auto& usage = cache.usedSequences[1][1];
  EXPECT_TRUE(usage.drawn);
  EXPECT_TRUE(usage.cached);
  EXPECT_NE(usage.result, original);
  EXPECT_EQ(usage.result->targetFrame, 1);
  EXPECT_EQ(usage.result->status.load(), SequenceReadStatus::Fallback);
  EXPECT_EQ(original->status.load(), SequenceReadStatus::Succeeded);
  cache.checkSequenceDecodeFailure();

  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_FALSE(cache.hasSequenceFrameUnavailable());
  EXPECT_EQ(cache.sequenceFailureStates[1].retryAfterTime, retryTime);
  EXPECT_EQ(cache.sequenceFailureStates[1].consecutiveFailures, failures);
  buffer->delivery = tgfx::ImageBufferStatus::Expired;
  cache.beginFrame();
  cache.usedSequences[1][1].queue = &queue;
  EXPECT_EQ(cache.getSequenceImage(sequence, 1), nullptr);
  cache.checkSequenceDecodeFailure();
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_EQ(cache.sequenceFailureStates[1].retryAfterTime, retryTime);
  EXPECT_EQ(cache.sequenceFailureStates[1].consecutiveFailures, failures);
}

PAG_TEST(PAGANRRegressionTest, KnownTargetCooldownFallbackCannotValidateAnotherFrame) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto sequence = std::make_shared<ANRSequenceInfo>();
  SequenceImageQueue queue(sequence, std::make_shared<ANRFailThenSucceedReader>(), 0, false);
  auto original = std::make_shared<SequenceReadResult>();
  original->requestID = 1;
  original->targetFrame = 0;
  original->status.store(SequenceReadStatus::Succeeded);
  auto frame = original->getFrameRead();
  frame->options->targetTimestamp = 0;
  frame->verifiedTargetTimestamp = 0;
  auto buffer = std::make_shared<ANRDeliveryBuffer>();
  buffer->delivery = tgfx::ImageBufferStatus::Ready;
  frame->buffer = buffer;
  queue.currentImage = MakeANRTestImage();
  queue.currentFrame = 0;
  queue.currentResult = original;
  cache.recordSequenceFailure(1);
  cache.usedSequences[1][1].queue = &queue;

  ASSERT_EQ(cache.getSequenceImage(sequence, 1), queue.currentImage);
  auto fallback = cache.usedSequences[1][1].result;
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(fallback->status.load(), SequenceReadStatus::Fallback);
  EXPECT_NE(fallback->getFrameRead(), frame);
  EXPECT_EQ(fallback->getFrameRead()->options->targetTimestamp, 0);
  EXPECT_EQ(fallback->getFrameRead()->verifiedTargetTimestamp, -1);
  EXPECT_TRUE(fallback->getFrameRead()->hasValidBuffer());
  EXPECT_FALSE(fallback->getFrameRead()->matchesTarget());
  cache.checkSequenceDecodeFailure();
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
  EXPECT_EQ(original->status.load(), SequenceReadStatus::Succeeded);
  EXPECT_EQ(frame->options->targetTimestamp, 0);
  EXPECT_EQ(frame->verifiedTargetTimestamp, 0);
  EXPECT_TRUE(frame->matchesTarget());
}

PAG_TEST(PAGANRRegressionTest, StaticSequenceCooldownWithoutPixelsCannotPublish) {
  auto stage = PAGStage::Make(2, 2);
  RenderCache cache(stage.get());
  auto sequence = std::make_shared<ANRSequenceInfo>();
  cache.recordSequenceFailure(1);
  EXPECT_EQ(cache.getStaticSequenceImage(sequence), nullptr);
  EXPECT_TRUE(cache.hasSequenceDecodeFailure());
  EXPECT_TRUE(cache.hasSequenceFrameUnavailable());
}

PAG_TEST(PAGANRRegressionTest, ReusedBufferRetainsRequestWaitingDeadline) {
  auto result = std::make_shared<SequenceReadResult>();
  auto frame = std::make_shared<FrameRead>();
  result->setFrameReadDeadline(30000);
  EXPECT_EQ(result->getFrameRead()->options->deadline.load(), 30000);
  EXPECT_EQ(frame->options->deadline.load(), 0);
  std::thread frameReadThread(&SequenceReadResult::setFrameRead, result.get(), frame);
  frameReadThread.join();

  EXPECT_EQ(result->getFrameRead(), frame);
  EXPECT_EQ(frame->options->deadline.load(), 30000);
  result->setFrameReadDeadline(60000);
  EXPECT_EQ(frame->options->deadline.load(), 60000);
  result->setFrameReadDeadline(0);
  EXPECT_EQ(frame->options->deadline.load(), 0);

  result->setFrameReadDeadline(60000);
  auto longerFrame = std::make_shared<FrameRead>();
  longerFrame->options->deadline.store(90000);
  std::thread longerFrameReadThread(&SequenceReadResult::setFrameRead, result.get(), longerFrame);
  longerFrameReadThread.join();
  EXPECT_EQ(result->getFrameRead(), longerFrame);
  EXPECT_EQ(longerFrame->options->deadline.load(), 90000);
  result->setFrameReadDeadline(0);
  EXPECT_EQ(longerFrame->options->deadline.load(), 0);
}
#endif
}  // namespace pag
