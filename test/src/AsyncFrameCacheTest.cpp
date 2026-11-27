/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "rendering/caches/DiskCache.h"
#include "rendering/caches/DiskIOWorker.h"
#include "rendering/caches/SequenceFile.h"
#include "tgfx/core/Task.h"
#include "utils/TestUtils.h"

namespace pag {
namespace {
class ScopedFrameCacheDrain {
 public:
  ~ScopedFrameCacheDrain() {
    DiskIOWorker::GetFrameCacheInstance()->waitAll();
    DiskIOWorker::GetInstance()->waitAll();
  }
};

// clang-format off
PAG_PRIVATE_ACCESS(
static std::shared_ptr<SequenceFile> MakeAsyncTestFile(const std::string& name,
                                                     const tgfx::ImageInfo& info, int frames = 2,
                                                     const std::vector<TimeRange>& ranges = {}) {
  auto path = ProjectPath::Absolute("test/out/AsyncFrameCacheTest/" + name + ".bin");
  std::remove(path.c_str());
  return SequenceFile::Open(path, info, frames, 25, ranges);
}
)
// clang-format on

static std::shared_ptr<SequenceFile> MakeTemporaryAsyncTestFile(
    const tgfx::ImageInfo& info, int frames = 2, const std::vector<TimeRange>& ranges = {}) {
  return DiskCache::OpenSequence("", info, frames, 25, ranges);
}

class BlockingCacheTask {
 public:
  /** Blocks a worker until the test releases it. */
  void run() {
    std::unique_lock<std::mutex> lock(locker);
    entered = true;
    condition.notify_all();
    while (!released) {
      condition.wait(lock);
    }
  }

  /** Waits until the worker starts running this task. */
  void waitForEntry() {
    std::unique_lock<std::mutex> lock(locker);
    while (!entered) {
      condition.wait(lock);
    }
  }

  /** Releases the blocked worker. */
  void release() {
    {
      std::lock_guard<std::mutex> lock(locker);
      released = true;
    }
    condition.notify_all();
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool entered = false;
  bool released = false;
};

class ScopedFrameWriterBlock {
 public:
  ScopedFrameWriterBlock() {
    DiskIOWorker::GetFrameCacheInstance()->submit(std::bind(&BlockingCacheTask::run, &task));
    task.waitForEntry();
  }

  ~ScopedFrameWriterBlock() {
    task.release();
    DiskIOWorker::GetFrameCacheInstance()->waitAll();
  }

 private:
  BlockingCacheTask task = {};
};

class ThreadCheckingEncoder : public LZ4Encoder {
 public:
  ThreadCheckingEncoder(std::thread::id caller, std::atomic<bool>* encodedOnCaller)
      : caller(caller), encodedOnCaller(encodedOnCaller), encoder(LZ4Encoder::Make()) {
  }

  size_t encode(uint8_t* output, size_t capacity, const uint8_t* pixels,
                size_t size) const override {
    encodedOnCaller->store(std::this_thread::get_id() == caller);
    return encoder->encode(output, capacity, pixels, size);
  }

 private:
  const std::thread::id caller;
  std::atomic<bool>* encodedOnCaller;
  std::unique_ptr<LZ4Encoder> encoder;
};

class FailingFrameEncoder : public LZ4Encoder {
 public:
  size_t encode(uint8_t*, size_t, const uint8_t*, size_t) const override {
    return 0;
  }
};
}  // namespace

PAG_TEST_PRIVATE(AsyncFrameCacheTest, WriterNeverRunsInlineWhileTaskPoolDrains) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 37);
  auto file = MakeAsyncTestFile("no-inline", info, 1);
  ASSERT_NE(file, nullptr);
  std::atomic<bool> encodedOnCaller = true;
  file->encoder =
      std::make_unique<ThreadCheckingEncoder>(std::this_thread::get_id(), &encodedOnCaller);
  BlockingCacheTask task = {};
  tgfx::Task::Run(std::bind(&BlockingCacheTask::run, &task));
  task.waitForEntry();
  std::thread draining(&tgfx::Task::ReleaseThreads);
  bool accepted = false;
  {
    ScopedFrameWriterBlock block;
    accepted = file->writeFrameAsync(0, BitmapBuffer::Wrap(info, pixels.data()));
    EXPECT_FALSE(file->isComplete());
  }
  task.release();
  draining.join();
  EXPECT_TRUE(accepted);
  EXPECT_FALSE(encodedOnCaller.load());
  EXPECT_TRUE(file->isComplete());
  )
  // clang-format on
}

PAG_TEST(AsyncFrameCacheTest, PendingWriteCannotBeReplacedBySynchronousWrite) {
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> first(info.byteSize(), 31);
  std::vector<uint8_t> second(info.byteSize(), 99);
  std::vector<uint8_t> output(info.byteSize(), 0);
  auto file = MakeTemporaryAsyncTestFile(info, 2);
  ASSERT_NE(file, nullptr);
  auto destination = BitmapBuffer::Wrap(info, output.data());
  {
    ScopedFrameWriterBlock block;
    ASSERT_TRUE(file->writeFrameAsync(0, BitmapBuffer::Wrap(info, first.data())));
    EXPECT_FALSE(file->writeFrame(0, BitmapBuffer::Wrap(info, second.data())));
    EXPECT_TRUE(file->writeFrame(1, BitmapBuffer::Wrap(info, second.data())));
    EXPECT_TRUE(file->tryReadFrame(0, destination));
    EXPECT_EQ(output.front(), 31);
    EXPECT_FALSE(file->isComplete());
  }
  EXPECT_TRUE(file->isComplete());
  EXPECT_TRUE(file->tryReadFrame(0, destination));
  EXPECT_EQ(output.front(), 31);
  EXPECT_TRUE(file->tryReadFrame(1, destination));
  EXPECT_EQ(output.front(), 99);
}

PAG_TEST_PRIVATE(AsyncFrameCacheTest, CommittedFrameTakesPrecedenceOverPendingSnapshot) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> first(info.byteSize(), 31);
  std::vector<uint8_t> second(info.byteSize(), 99);
  std::vector<uint8_t> output(info.byteSize(), 0);
  auto file = MakeAsyncTestFile("committed-priority", info, 2);
  ASSERT_NE(file, nullptr);
  auto destination = BitmapBuffer::Wrap(info, output.data());
  {
    ScopedFrameWriterBlock block;
    ASSERT_TRUE(file->writeFrameAsync(0, BitmapBuffer::Wrap(info, first.data())));
    auto snapshot = file->pendingSnapshot;
    EXPECT_TRUE(file->writeFrameInternal(0, BitmapBuffer::Wrap(info, second.data()), snapshot));
    EXPECT_TRUE(file->tryReadFrame(0, destination));
    EXPECT_EQ(output.front(), 99);
  }
  EXPECT_TRUE(file->tryReadFrame(0, destination));
  EXPECT_EQ(output.front(), 99);
  )
  // clang-format on
}

PAG_TEST_PRIVATE(AsyncFrameCacheTest, PendingSnapshotIsImmutableAndReadableWithoutWriter) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info =
      tgfx::ImageInfo::Make(17, 2, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied, 80);
  std::vector<uint8_t> pixels(info.byteSize(), 31);
  std::vector<uint8_t> destination(info.byteSize(), 0);
  auto input = BitmapBuffer::Wrap(info, pixels.data());
  auto output = BitmapBuffer::Wrap(info, destination.data());
  auto file = MakeAsyncTestFile("immutable", info);
  ASSERT_NE(file, nullptr);
  {
    std::unique_lock<std::mutex> writerLock(file->ioLocker);
    ASSERT_TRUE(file->writeFrameAsync(0, input));
    std::fill(pixels.begin(), pixels.end(), 99);
    EXPECT_TRUE(file->tryReadFrame(0, output));
    for (int row = 0; row < info.height(); row++) {
      for (size_t column = 0; column < info.minRowBytes(); column++) {
        EXPECT_EQ(destination[static_cast<size_t>(row) * info.rowBytes() + column], 31);
      }
    }
    EXPECT_FALSE(file->writeFrameAsync(1, input));
    EXPECT_FALSE(file->tryReadFrame(1, output));
    EXPECT_FALSE(file->isComplete());
    EXPECT_EQ(file->fileSize(), 0u);
  }
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  std::fill(destination.begin(), destination.end(), 0);
  EXPECT_TRUE(file->readFrame(0, output));
  EXPECT_EQ(destination.front(), 31);
  EXPECT_FALSE(file->isComplete());
  ASSERT_TRUE(file->writeFrameAsync(1, input));
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  EXPECT_TRUE(file->isComplete());
  auto path = ProjectPath::Absolute("test/out/AsyncFrameCacheTest/immutable.bin");
  file = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  file = SequenceFile::Open(path, info, 2, 25, {});
  ASSERT_NE(file, nullptr);
  EXPECT_TRUE(file->isComplete());
  EXPECT_TRUE(file->readFrame(1, output));
  EXPECT_EQ(destination.front(), 99);
  )
  // clang-format on
}

PAG_TEST(AsyncFrameCacheTest, GlobalBackpressureDropsWritesNotPixels) {
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 71);
  auto bitmap = BitmapBuffer::Wrap(info, pixels.data());
  auto first = MakeTemporaryAsyncTestFile(info);
  auto second = MakeTemporaryAsyncTestFile(info);
  auto third = MakeTemporaryAsyncTestFile(info);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(third, nullptr);
  {
    ScopedFrameWriterBlock block;
    ASSERT_TRUE(first->writeFrameAsync(0, bitmap));
    ASSERT_TRUE(second->writeFrameAsync(0, bitmap));
    EXPECT_FALSE(third->writeFrameAsync(0, bitmap));
    EXPECT_FALSE(third->isComplete());
    PAG_PRIVATE_ACCESS(EXPECT_EQ(third->pendingIndex, -1);)
    EXPECT_EQ(pixels.front(), 71);
  }
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  EXPECT_TRUE(third->writeFrameAsync(0, bitmap));
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  EXPECT_GT(third->fileSize(), 0u);
}

PAG_TEST(AsyncFrameCacheTest, ByteBudgetIsCheckedBeforeCopy) {
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(8192, 2048, tgfx::ColorType::RGBA_8888);
  auto file = MakeTemporaryAsyncTestFile(info);
  ASSERT_NE(file, nullptr);
  uint8_t sentinel = 17;
  EXPECT_FALSE(file->writeFrameAsync(0, BitmapBuffer::Wrap(info, &sentinel)));
  EXPECT_EQ(sentinel, 17);
  PAG_PRIVATE_ACCESS(EXPECT_EQ(file->pendingIndex, -1);)
  EXPECT_EQ(file->fileSize(), 0u);
}

PAG_TEST_PRIVATE(AsyncFrameCacheTest, FailedWriteReleasesSnapshotAndCanRetry) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 81);
  auto bitmap = BitmapBuffer::Wrap(info, pixels.data());
  auto file = MakeAsyncTestFile("failure", info, 1);
  ASSERT_NE(file, nullptr);
  file->encoder = std::make_unique<FailingFrameEncoder>();
  ASSERT_TRUE(file->writeFrameAsync(0, bitmap));
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  EXPECT_FALSE(file->isComplete());
  EXPECT_EQ(file->pendingIndex, -1);
  EXPECT_EQ(file->pendingSnapshot, nullptr);
  EXPECT_EQ(file->fileSize(), 0u);
  EXPECT_TRUE(file->writeFrameAsync(0, bitmap));
  std::weak_ptr<SequenceFile> retained = file;
  file = nullptr;
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_TRUE(retained.expired());
  auto path = ProjectPath::Absolute("test/out/AsyncFrameCacheTest/failure.bin");
  auto reopened = SequenceFile::Open(path, info, 1, 25, {});
  ASSERT_NE(reopened, nullptr);
  EXPECT_TRUE(reopened->isComplete());
  )
  // clang-format on
}

PAG_TEST_PRIVATE(AsyncFrameCacheTest, CorruptFrameLengthIsRejectedBeforeRead) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 91);
  auto bitmap = BitmapBuffer::Wrap(info, pixels.data());
  auto file = MakeAsyncTestFile("corrupt-length", info);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(file->writeFrame(0, bitmap));
  file->frames[0].size = LZ4Encoder::GetMaxOutputSize(info.byteSize()) + 100;
  EXPECT_FALSE(file->tryReadFrame(0, bitmap));
  EXPECT_EQ(pixels.front(), 91);
  )
  // clang-format on
}

PAG_TEST_PRIVATE(AsyncFrameCacheTest, CacheFailureDoesNotQueueOrModifyOutput) {
  // clang-format off
  PAG_PRIVATE_ACCESS(
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 101);
  auto file = MakeAsyncTestFile("failed-file", info);
  ASSERT_NE(file, nullptr);
  file->cacheWriteFailed = true;
  EXPECT_FALSE(file->writeFrameAsync(0, BitmapBuffer::Wrap(info, pixels.data())));
  EXPECT_EQ(pixels.front(), 101);
  EXPECT_EQ(file->pendingIndex, -1);
  EXPECT_FALSE(file->isComplete());
  )
  // clang-format on
}

PAG_TEST(AsyncFrameCacheTest, StaticRangeUsesOneSnapshotAndCommittedRecord) {
  ScopedFrameCacheDrain drain;
  auto info = tgfx::ImageInfo::Make(2, 2, tgfx::ColorType::RGBA_8888);
  std::vector<uint8_t> pixels(info.byteSize(), 91);
  std::vector<uint8_t> destination(info.byteSize(), 0);
  auto bitmap = BitmapBuffer::Wrap(info, pixels.data());
  auto output = BitmapBuffer::Wrap(info, destination.data());
  auto file = MakeTemporaryAsyncTestFile(info, 3, {{0, 2}});
  ASSERT_NE(file, nullptr);
  {
    ScopedFrameWriterBlock block;
    ASSERT_TRUE(file->writeFrameAsync(2, bitmap));
    EXPECT_FALSE(file->writeFrameAsync(0, bitmap));
    EXPECT_TRUE(file->tryReadFrame(1, output));
    EXPECT_EQ(destination.front(), 91);
    EXPECT_FALSE(file->isComplete());
  }
  DiskIOWorker::GetFrameCacheInstance()->waitAll();
  EXPECT_TRUE(file->isComplete());
  PAG_PRIVATE_ACCESS(EXPECT_EQ(file->cachedFrames, 3);)
  EXPECT_FALSE(file->writeFrameAsync(1, bitmap));
  PAG_PRIVATE_ACCESS(EXPECT_EQ(file->frames[0].offset, file->frames[2].offset);)
}
}  // namespace pag
