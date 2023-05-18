/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/utils/Task.h"
#include "utils/Log.h"
#include "utils/TaskGroup.h"

namespace tgfx {
std::shared_ptr<Task> Task::Run(std::function<void()> block) {
  if (block == nullptr) {
    return nullptr;
  }
  auto task = std::shared_ptr<Task>(new Task(std::move(block)));
  if (!TaskGroup::GetInstance()->pushTask(task)) {
    task->execute();
  }
  return task;
}

Task::Task(std::function<void()> block) : block(std::move(block)) {
}

bool Task::executing() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _executing;
}

bool Task::cancelled() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _cancelled;
}

bool Task::finished() {
  std::lock_guard<std::mutex> autoLock(locker);
  return !_executing && !_cancelled;
}

void Task::wait() {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!_executing) {
    return;
  }
  // Try to remove the task from the queue. Execute it directly on the current thread if the task is
  // not in the queue. This is to avoid the deadlock situation.
  if (removeTask()) {
    block();
    _executing = false;
    condition.notify_all();
    return;
  }
  condition.wait(autoLock);
}

void Task::cancel() {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!_executing) {
    return;
  }
  if (removeTask()) {
    _executing = false;
    _cancelled = true;
  }
}

bool Task::removeTask() {
  return TaskGroup::GetInstance()->removeTask(this);
}

void Task::execute() {
  block();
  std::lock_guard<std::mutex> auoLock(locker);
  _executing = false;
  condition.notify_all();
}
}  // namespace tgfx
