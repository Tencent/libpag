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

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include "tgfx/utils/TaskQueue.h"

namespace tgfx {
class TaskGroup;

/**
 * The Task class manages the concurrent execution of one or more code blocks.
 */
class Task {
 public:
  /**
   * Submits a code block for asynchronous execution immediately and returns a Task wraps the code
   * block. Hold a reference to the returned Task if you want to cancel it or wait for it finishes
   * execution. Returns nullptr if the block is nullptr.
   */
  static std::shared_ptr<Task> Run(std::function<void()> block);

  /**
   * Submits a code block for asynchronous execution immediately on a TaskQueue and returns a Task
   * wraps the code block. Hold a reference to the returned Task if you want to cancel it or wait
   * for it finishes execution. Returns nullptr if the queue or the block is nullptr.
   */
  static std::shared_ptr<Task> Run(std::shared_ptr<TaskQueue> queue, std::function<void()> block);

  /**
   * Returns true if the Task is currently executing its code block.
   */
  bool executing();

  /**
   * Returns true if the Task has been cancelled
   */
  bool cancelled();

  /**
   * Returns true if the Task has finished executing its code block.
   */
  bool finished();

  /**
   * Advises the Task that it should stop executing its code block. Cancellation does not affect the
   * execution of a Task that has already begun.
   */
  void cancel();

  /**
   * Blocks the current thread until the Task finishes its execution. Returns immediately if the
   * Task is finished or canceled.
   */
  void wait();

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  bool _executing = true;
  bool _cancelled = false;
  std::weak_ptr<TaskQueue> queue;
  std::function<void()> block = nullptr;

  explicit Task(std::function<void()> block);
  void execute();

  friend class TaskGroup;
  friend class TaskQueue;
};

}  // namespace tgfx
