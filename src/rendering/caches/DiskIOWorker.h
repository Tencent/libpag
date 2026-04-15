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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace pag {

/**
 * A dedicated background worker thread that executes disk IO tasks serially.
 * Tasks are queued and executed in order on a single thread, avoiding main thread blocking.
 *
 * Lock ordering: DiskIOWorker::locker must be acquired BEFORE DiskCache::locker.
 * IMPORTANT: Never call waitAll() while holding DiskCache::locker, as tasks executed by
 * DiskIOWorker (e.g., CloseFileTask) may acquire DiskCache::locker, causing deadlock.
 */
class DiskIOWorker {
 public:
  static DiskIOWorker* GetInstance();

  ~DiskIOWorker();

  /**
   * Submits a task for asynchronous execution on the background thread.
   */
  void submit(std::function<void()> task);

  /**
   * Waits for all pending tasks to complete. Useful during app shutdown.
   */
  void waitAll();

 private:
  DiskIOWorker();

  void runLoop();

  std::mutex locker = {};
  std::condition_variable condition = {};
  std::condition_variable idleCondition = {};
  std::queue<std::function<void()>> tasks = {};
  std::thread workerThread = {};
  std::atomic<bool> stopped{false};
  // Tracks if a task is currently being executed. Always accessed under locker, no atomic needed.
  bool executing = false;
};

}  // namespace pag
