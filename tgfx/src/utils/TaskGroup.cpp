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

#include "TaskGroup.h"
#include <algorithm>
#include <cstdlib>
#include "utils/Log.h"

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace tgfx {
static constexpr auto THREAD_TIMEOUT = std::chrono::seconds(10);

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

TaskGroup* TaskGroup::GetInstance() {
  static auto& taskGroup = *new TaskGroup();
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

static void ReleaseThread(std::thread* thread) {
  if (thread->joinable()) {
    thread->join();
  }
  delete thread;
}

void OnAppExit() {
  // Forces all pending tasks to be finished when the app is exiting to prevent accessing wild
  // pointers.
  TaskGroup::GetInstance()->exit();
}

TaskGroup::TaskGroup() {
  std::atexit(OnAppExit);
}

bool TaskGroup::checkThreads() {
  static const int CPUCores = GetCPUCores();
  static const int MaxThreads = CPUCores > 16 ? 16 : CPUCores;
  while (!timeoutThreads.empty()) {
    auto threadID = timeoutThreads.back();
    timeoutThreads.pop_back();
    auto result = std::find_if(threads.begin(), threads.end(),
                               [=](std::thread* thread) { return thread->get_id() == threadID; });
    if (result != threads.end()) {
      ReleaseThread(*result);
      threads.erase(result);
    }
  }
  auto totalThreads = static_cast<int>(threads.size());
  if (activeThreads < totalThreads || totalThreads >= MaxThreads) {
    return true;
  }
  auto thread = new (std::nothrow) std::thread(&TaskGroup::RunLoop, this);
  if (thread) {
    activeThreads++;
    threads.push_back(thread);
    //    LOGI("TaskGroup: A task thread is created, the current number of threads : %lld",
    //         threads.size());
  }
  return !threads.empty();
}

bool TaskGroup::pushTask(std::shared_ptr<Task> task) {
  std::lock_guard<std::mutex> autoLock(locker);
#ifdef TGFX_BUILD_FOR_WEB
  return false;
#endif
  if (exited || !checkThreads()) {
    return false;
  }
  tasks.push_back(std::move(task));
  condition.notify_one();
  return true;
}

std::shared_ptr<Task> TaskGroup::popTask() {
  std::unique_lock<std::mutex> autoLock(locker);
  activeThreads--;
  while (true) {
    if (tasks.empty()) {
      static auto res = condition.wait_for(autoLock, THREAD_TIMEOUT);
      // When wait_for returns, Task may be not empty, so the return value of wait_for
      // should be used to make the judgment.
      if (exited || res == std::cv_status::timeout) {
        auto threadID = std::this_thread::get_id();
        timeoutThreads.push_back(threadID);
        //        LOGI("TaskGroup: A task thread is exited, the current number of threads : %lld",
        //             threads.size() - expiredThreads.size());
        return nullptr;
      }
    } else {
      auto task = tasks.front();
      tasks.pop_front();
      activeThreads++;
      //      LOGI("TaskGroup: A task is running, the current active threads : %lld",
      //      activeThreads);
      return task;
    }
  }
}

bool TaskGroup::removeTask(Task* target) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto position = std::find_if(tasks.begin(), tasks.end(),
                               [=](std::shared_ptr<Task> task) { return task.get() == target; });
  if (position == tasks.end()) {
    return false;
  }
  tasks.erase(position);
  return true;
}

void TaskGroup::exit() {
  locker.lock();
  exited = true;
  tasks.clear();
  condition.notify_all();
  locker.unlock();
  for (auto& thread : threads) {
    ReleaseThread(thread);
  }
  threads.clear();
}
}  // namespace tgfx
