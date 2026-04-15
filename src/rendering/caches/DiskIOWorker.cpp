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

#include "DiskIOWorker.h"

namespace pag {

DiskIOWorker* DiskIOWorker::GetInstance() {
  // Intentional leak: using *new to avoid static destruction order issues at program exit.
  // The destructor is implemented correctly but will never be called in this singleton pattern.
  static auto& instance = *new DiskIOWorker();
  return &instance;
}

DiskIOWorker::DiskIOWorker() {
  workerThread = std::thread(&DiskIOWorker::runLoop, this);
}

DiskIOWorker::~DiskIOWorker() {
  {
    std::lock_guard<std::mutex> lock(locker);
    stopped = true;
  }
  condition.notify_one();
  if (workerThread.joinable()) {
    workerThread.join();
  }
}

void DiskIOWorker::submit(std::function<void()> task) {
  if (task == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(locker);
    tasks.push(std::move(task));
  }
  condition.notify_one();
}

void DiskIOWorker::waitAll() {
  std::unique_lock<std::mutex> lock(locker);
  // Wait until the queue is empty AND no task is currently executing.
  idleCondition.wait(lock, [this] { return tasks.empty() && !executing; });
}

void DiskIOWorker::runLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(locker);
      condition.wait(lock, [this] { return stopped || !tasks.empty(); });
      if (stopped && tasks.empty()) {
        return;
      }
      task = std::move(tasks.front());
      tasks.pop();
      executing = true;
    }
    if (task) {
      task();
    }
    {
      std::lock_guard<std::mutex> lock(locker);
      executing = false;
      if (tasks.empty()) {
        idleCondition.notify_all();
      }
    }
  }
}

}  // namespace pag
