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

#pragma once

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include "tgfx/utils/Task.h"

namespace tgfx {
class TaskGroup {
 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  int activeThreads = 0;
  bool exited = false;
  std::list<std::shared_ptr<Task>> tasks = {};
  std::vector<std::thread*> threads = {};
  std::vector<std::thread::id> timeoutThreads = {};

  static TaskGroup* GetInstance();
  static void RunLoop(TaskGroup* taskGroup);

  TaskGroup();
  bool checkThreads();
  bool pushTask(std::shared_ptr<Task> task);
  std::shared_ptr<Task> popTask();
  bool removeTask(Task* task);
  void exit();

  friend class Task;
  friend void OnAppExit();
};
}  // namespace tgfx
