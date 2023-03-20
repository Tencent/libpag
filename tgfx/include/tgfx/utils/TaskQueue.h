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

#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace tgfx {
class Task;

/**
 * TaskQueue is a FIFO queue that manages the execution of tasks serially on an asynchronous thread.
 */
class TaskQueue {
 public:
  /**
   * Creates a new TaskQueue to which you can submit code blocks.
   * @param name  A string label to attach to the queue to uniquely identify it.
   */
  static std::shared_ptr<TaskQueue> Make(const std::string& name);

  /**
   * Returns the string label to attach to the queue to uniquely identify it.
   */
  std::string name() const {
    return _name;
  }

 private:
  std::mutex locker = {};
  std::string _name;
  std::weak_ptr<TaskQueue> weakThis;
  std::shared_ptr<Task> runningTask = nullptr;
  std::list<std::shared_ptr<Task>> tasks = {};

  explicit TaskQueue(const std::string& name);
  bool pushTask(std::shared_ptr<Task> task);
  std::shared_ptr<Task> popTask();
  bool removeTask(Task* task);
  void execute();

  friend class Task;
  friend class TaskGroup;
};
}  // namespace tgfx
