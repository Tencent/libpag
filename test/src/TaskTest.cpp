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

#include <thread>
#include "tgfx/utils/Task.h"
#include "utils/Log.h"
#include "utils/TestUtils.h"

using namespace pag;

namespace tgfx {

/**
 * 用例描述: 测试异步 Task 执行
 */
PAG_TEST(TaskTest, Task) {
  std::vector<std::shared_ptr<Task>> tasks = {};
  for (int i = 0; i < 17; i++) {
    auto task = Task::Run([=] {
      LOGI("Task %d is executing...", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      LOGI("Task %d is finished", i);
    });
    tasks.push_back(task);
  }
  // Wait a little moment for the tasks to be executed.
  std::this_thread::yield();
  auto task = tasks[0];
  EXPECT_TRUE(task->executing());
  task->cancel();
  auto taskCancelled = task->cancelled();
  task = tasks[16];
  EXPECT_TRUE(task->executing());
  task->cancel();
  EXPECT_TRUE(task->cancelled());
  for (auto& item : tasks) {
    item->wait();
    EXPECT_FALSE(item->executing());
  }
  task = tasks[0];
  EXPECT_NE(task->finished(), taskCancelled);
  task = tasks[16];
  EXPECT_FALSE(task->finished());
  tasks = {};
}
}  // namespace tgfx
