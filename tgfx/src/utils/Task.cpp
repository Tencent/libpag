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

#ifndef TGFX_BUILD_FOR_WEB

#include "tgfx/utils/Task.h"
#include "TaskGroup.h"

namespace tgfx {
std::shared_ptr<Task> Task::Run(std::function<void()> block) {
  if (block == nullptr) {
    return nullptr;
  }
  auto task = std::shared_ptr<Task>(new Task(std::move(block)));
  TaskGroup::GetInstance()->pushTask(task);
  return task;
}

Task::Task(std::function<void()> block) : block(std::move(block)) {
}

bool Task::isRunning() {
  std::lock_guard<std::mutex> autoLock(locker);
  return running;
}

void Task::wait() {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!running) {
    return;
  }
  condition.wait(autoLock);
}

void Task::cancel() {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!running) {
    return;
  }
  if (TaskGroup::GetInstance()->removeTask(this)) {
    running = false;
  }
}

void Task::execute() {
  block();
  std::lock_guard<std::mutex> auoLock(locker);
  running = false;
  condition.notify_all();
}
}  // namespace tgfx

#endif
