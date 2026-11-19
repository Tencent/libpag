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

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include "rendering/caches/DiskIOWorker.h"

namespace pag {

/**
 * Occupies the single DiskIOWorker thread with a blocking task, so that every task submitted
 * afterwards (e.g. the asynchronous close of a SequenceFile) stays queued until release() is
 * called. This makes close/reopen orderings reproducible instead of timing dependent. A case built
 * on it covers the "close has been queued but has not started yet" ordering only: the "close is
 * already running on the IO thread" ordering cannot be paused deterministically without
 * instrumenting the worker.
 */
class ScopedDiskIOWorkerBlock {
 public:
  ScopedDiskIOWorkerBlock() {
    DiskIOWorker::GetInstance()->submit(std::bind(&ScopedDiskIOWorkerBlock::block, this));
    std::unique_lock<std::mutex> lock(locker);
    condition.wait(lock, std::bind(&ScopedDiskIOWorkerBlock::hasStarted, this));
  }

  ~ScopedDiskIOWorkerBlock() {
    release();
  }

  ScopedDiskIOWorkerBlock(const ScopedDiskIOWorkerBlock&) = delete;
  ScopedDiskIOWorkerBlock& operator=(const ScopedDiskIOWorkerBlock&) = delete;

  /**
   * Releases the IO worker and waits for all queued operations to complete.
   */
  void release() {
    {
      std::lock_guard<std::mutex> lock(locker);
      released = true;
    }
    condition.notify_all();
    DiskIOWorker::GetInstance()->waitAll();
  }

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool started = false;
  bool released = false;

  bool hasStarted() {
    return started;
  }

  bool hasReleased() {
    return released;
  }

  void block() {
    std::unique_lock<std::mutex> lock(locker);
    started = true;
    condition.notify_all();
    condition.wait(lock, std::bind(&ScopedDiskIOWorkerBlock::hasReleased, this));
  }
};

}  // namespace pag
