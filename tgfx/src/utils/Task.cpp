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

#include "Task.h"
#include <algorithm>

#ifdef __APPLE__

#include <sys/sysctl.h>

#endif

namespace tgfx {

int GetCPUCores() {
  int cpuCores = 0;
#ifdef __APPLE__
  size_t len = sizeof(cpuCores);
  // We can get the exact number of physical CPUs on apple platforms.
  sysctlbyname("hw.physicalcpu", &cpuCores, &len, nullptr, 0);
#else
  cpuCores = static_cast<int>(std::thread::hardware_concurrency());
#endif
  if (cpuCores <= 0) {
    cpuCores = 8;
  }
  return cpuCores;
}

std::shared_ptr<Task> Task::Make(Executor* executor) {
  return std::shared_ptr<Task>(new Task(executor));
}

Task::Task(Executor* executor) : executor(executor) {
}

Task::~Task() {
  cancel();
}

void Task::run() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (running) {
    return;
  }
  running = true;
  TaskGroup::GetInstance()->pushTask(this);
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
    return;
  }
  condition.wait(autoLock);
}

void Task::execute() {
  executor->execute();
  std::lock_guard<std::mutex> auoLock(locker);
  running = false;
  condition.notify_all();
}

TaskGroup* TaskGroup::GetInstance() {
  static TaskGroup taskGroup = {};
  return &taskGroup;
}

void TaskGroup::RunLoop(TaskGroup* taskGroup) {
  while (true) {
    auto task = taskGroup->popTask();
    if (!task) {
      break;
    }
    task->execute();
  }
}

TaskGroup::~TaskGroup() {
  exit();
  for (auto& thread : threads) {
    if (thread->joinable()) {
      thread->join();
    }
    delete thread;
    thread = nullptr;
  }
  threads.clear();
}

void TaskGroup::initThreads() {
  static const int CPUCores = GetCPUCores();
  auto maxThreads = CPUCores > 16 ? 16 : CPUCores;
  for (int i = 0; i < maxThreads; i++) {
    auto thread = new (std::nothrow) std::thread(&TaskGroup::RunLoop, this);
    if (thread) {
      threads.emplace_back(thread);
    } else {
      break;
    }
  }
  activeThreads = static_cast<int>(threads.size());
}

void TaskGroup::pushTask(Task* task) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (threads.empty()) {
    initThreads();
  }
  tasks.push_back(task);
  condition.notify_one();
}

Task* TaskGroup::popTask() {
  std::unique_lock<std::mutex> autoLock(locker);
  activeThreads--;
  while (true) {
    if (tasks.empty()) {
      condition.wait(autoLock);
      if (exited) {
        return nullptr;
      }
    } else {
      auto task = tasks.front();
      tasks.pop_front();
      activeThreads++;
      // LOGI("TaskGroup.activeThreads : %d", activeThreads);
      return task;
    }
  }
}

bool TaskGroup::removeTask(Task* task) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto position = std::find(tasks.begin(), tasks.end(), task);
  if (position == tasks.end()) {
    return false;
  }
  tasks.erase(position);
  return true;
}

void TaskGroup::exit() {
  std::lock_guard<std::mutex> autoLock(locker);
  exited = true;
  condition.notify_all();
}
}  // namespace tgfx

#endif
