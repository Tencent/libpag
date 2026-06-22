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
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <vector>

namespace pagx {

class PAGScene;
class PAGViewModelValue;

/**
 * SuppressDelegation suppresses observer notifications for ViewModel value changes within a scope.
 * Property writes during the scope still mark DataBinds dirty (so rendering reflects the latest
 * data), but observer callbacks are coalesced into a single notification on destruction.
 *
 * Single-level only: creating another SuppressDelegation while one is active on the same scene
 * is a no-op for the inner guard.
 *
 * Usage:
 *   {
 *     SuppressDelegation guard(scene);
 *     vm->propertyNumber("enterSpeed")->value(0.5f);
 *     vm->propertyColor("theme")->value(Color{1, 0, 0, 1});
 *   } // observers fire once here
 */
class SuppressDelegation {
 public:
  explicit SuppressDelegation(const std::shared_ptr<PAGScene>& scene);
  ~SuppressDelegation();

  SuppressDelegation(const SuppressDelegation&) = delete;
  SuppressDelegation& operator=(const SuppressDelegation&) = delete;

 private:
  /**
   * Returns true if the given scene is currently suppressing observer notifications.
   */
  static bool isSuppressed(const std::shared_ptr<PAGScene>& scene);

  /**
   * Adds a value to the scene's pending notification list.
   */
  static void addPendingNotification(const std::shared_ptr<PAGScene>& scene,
                                     PAGViewModelValue* value);

  std::weak_ptr<PAGScene> scene;
  bool wasAlreadySuppressed = false;

  friend class PAGViewModelValue;
};

}  // namespace pagx
