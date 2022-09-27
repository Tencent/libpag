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

#ifndef PAG_BUILD_FOR_WEB

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

namespace pag {
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
  static std::shared_ptr<Task> Make(std::unique_ptr<Executor> executor);
  ~Task();

  void run();
  bool isRunning();
  Executor* wait();
  void cancel();

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool running = false;
  TaskGroup* taskGroup = nullptr;
  std::unique_ptr<Executor> executor = nullptr;

  explicit Task(std::unique_ptr<Executor> executor);
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
  std::vector<std::thread> threads = {};

  static TaskGroup* GetInstance();
  static void RunLoop(TaskGroup* taskGroup);

  TaskGroup();
  void initThreads();
  void pushTask(Task* task);
  Task* popTask();
  bool removeTask(Task* task);
  void exit();

  friend class Task;
};
}  // namespace pag
#else

#include <memory>

namespace pag {
class Executor {
 public:
  virtual ~Executor() = default;

 private:
  virtual void execute() = 0;

  friend class Task;
};

class Task {
 public:
  static std::shared_ptr<Task> Make(std::unique_ptr<Executor> executor) {
    return std::shared_ptr<Task>(new Task(std::move(executor)));
  }

  void run() {
  }

  bool isRunning() const {
    return running;
  }

  Executor* wait() {
    running = true;
    executor->execute();
    running = false;
    return executor.get();
  }

  void cancel() {
  }

 private:
  bool running = false;
  std::unique_ptr<Executor> executor = nullptr;

  explicit Task(std::unique_ptr<Executor> executor) : executor(std::move(executor)) {
  }
};
}  // namespace pag

#endif
