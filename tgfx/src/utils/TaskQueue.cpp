/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/utils/TaskQueue.h"
#include <mutex>
#include <unordered_map>
#include "TaskGroup.h"

namespace tgfx {
std::shared_ptr<TaskQueue> TaskQueue::Make(const std::string& name) {
  auto queue = std::shared_ptr<TaskQueue>(new TaskQueue(name));
  queue->weakThis = queue;
  return queue;
}

TaskQueue::TaskQueue(const std::string& name) : _name(name) {
}

bool TaskQueue::pushTask(std::shared_ptr<Task> task) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto queue = weakThis.lock();
  if (runningTask == nullptr && tasks.empty()) {
    auto queueTask = std::shared_ptr<Task>(new Task([=] { queue->execute(); }));
    auto success = TaskGroup::GetInstance()->pushTask(std::move(queueTask));
    if (!success) {
      return false;
    }
  }
  task->queue = queue;
  tasks.push_back(std::move(task));
  return true;
}

std::shared_ptr<Task> TaskQueue::popTask() {
  std::lock_guard<std::mutex> autoLock(locker);
  runningTask = nullptr;
  if (tasks.empty()) {
    return nullptr;
  }
  runningTask = tasks.front();
  tasks.pop_front();
  return runningTask;
}

bool TaskQueue::removeTask(Task* target) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto position = std::find_if(tasks.begin(), tasks.end(),
                               [=](std::shared_ptr<Task> task) { return task.get() == target; });
  if (position == tasks.end()) {
    return false;
  }
  tasks.erase(position);
  return true;
}

void TaskQueue::execute() {
  while (true) {
    auto task = popTask();
    if (!task) {
      break;
    }
    task->execute();
  }
}
}  // namespace tgfx
