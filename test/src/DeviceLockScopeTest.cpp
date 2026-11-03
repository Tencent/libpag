/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <thread>
#include "base/PAGTest.h"

namespace pag {
PAG_TEST(DeviceLockScopeTest, NullDeviceDoesNotAcquireOrUnlock) {
  DeviceLockScope scope(nullptr);
  EXPECT_FALSE(static_cast<bool>(scope));
  EXPECT_EQ(scope.context(), nullptr);
}

PAG_TEST(DeviceLockScopeTest, DeviceCanBeRelockedAfterScopeExit) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  {
    DeviceLockScope first(device);
    ASSERT_TRUE(static_cast<bool>(first));
  }
  DeviceLockScope second(device);
  EXPECT_TRUE(static_cast<bool>(second));
}

PAG_TEST(DeviceLockScopeTest, AnotherThreadCanWaitForTheDevice) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  std::atomic<bool> started{false};
  bool acquired = false;
  std::thread worker;
  {
    DeviceLockScope first(device);
    ASSERT_TRUE(static_cast<bool>(first));
    worker = std::thread([&] {
      started.store(true);
      DeviceLockScope second(device);
      acquired = static_cast<bool>(second);
    });
    while (!started.load()) {
      std::this_thread::yield();
    }
  }
  worker.join();
  EXPECT_TRUE(acquired);
}

#if DEBUG && !defined(NO_LOG) && GTEST_HAS_DEATH_TEST
PAG_TEST(DeviceLockScopeDeathTest, SameThreadReentryFailsBeforeWaitingOnMutex) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        auto device = Devices::MakeDefault();
        DeviceLockScope first(device);
        if (first) {
          DeviceLockScope second(device);
        }
      },
      "HeldDeviceScopes");
}

PAG_TEST(DeviceLockScopeDeathTest, WrongThreadDestructionFailsBeforeUnlocking) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        auto scope = std::make_unique<DeviceLockScope>(Devices::MakeDefault());
        if (*scope) {
          std::thread worker([scope = std::move(scope)]() mutable { scope.reset(); });
          worker.join();
        }
      },
      "HeldDeviceScopes");
}
#endif
}  // namespace pag
