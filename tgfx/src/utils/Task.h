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

#pragma once

#ifndef TGFX_BUILD_FOR_WEB

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

namespace tgfx {
class Executor {
 public:
  virtual ~Executor() = default;

 private:
  virtual void execute() = 0;

  friend class Task;
};

class TaskGroup;

class Task {
 public:
  static std::shared_ptr<Task> Make(Executor* executor);
  ~Task();

  void run();
  bool isRunning();
  void wait();
  void cancel();

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool running = false;
  Executor* executor = nullptr;

  explicit Task(Executor* executor);
  void execute();

  friend class TaskGroup;
};

class TaskGroup {
 public:
  ~TaskGroup();

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  int activeThreads = 0;
  bool exited = false;
  std::list<Task*> tasks = {};
  std::vector<std::thread*> threads = {};

  static TaskGroup* GetInstance();
  static void RunLoop(TaskGroup* taskGroup);

  TaskGroup() = default;
  void initThreads();
  void pushTask(Task* task);
  Task* popTask();
  bool removeTask(Task* task);
  void exit();

  friend class Task;
};
}  // namespace tgfx
#else

#include <memory>

namespace tgfx {
class Executor {
 public:
  virtual ~Executor() = default;

 private:
  virtual void execute() = 0;

  friend class Task;
};

class Task {
 public:
  static std::shared_ptr<Task> Make(Executor* executor) {
    return std::shared_ptr<Task>(new Task(executor));
  }

  void run() {
    running = true;
  }

  bool isRunning() const {
    return running;
  }

  void wait() {
    if (!running) {
      return;
    }
    executor->execute();
    running = false;
  }

  void cancel() {
    running = false;
  }

 private:
  bool running = false;
  Executor* executor = nullptr;

  explicit Task(Executor* executor) : executor(executor) {
  }
};
}  // namespace tgfx

#endif
