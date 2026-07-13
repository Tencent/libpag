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

#include "pagx/PAGViewModelValue.h"

namespace pagx {

/**
 * PAGViewModelValueTrigger holds a trigger ViewModel property. A trigger is a one-shot event with
 * no persistent value: calling fire() notifies observers (such as a bound StateMachine input) that
 * the event occurred. Unlike Boolean, there is no stored state to read back — each fire() is an
 * independent pulse.
 */
class PAGViewModelValueTrigger : public PAGViewModelValue {
 public:
  /**
   * Fires the trigger, notifying all registered observers. Each call is an independent event;
   * repeated calls produce repeated notifications (subject to consumption by the receiver, e.g. a
   * StateMachine consumes a trigger at most once per advance).
   *
   * Caveat: within an active SuppressDelegation scope, observer notifications are deferred and
   * de-duplicated per value, so multiple fire() calls on the same trigger inside one suppressed
   * scope coalesce into a single notification when the scope exits.
   */
  void fire();

  friend class PAGViewModel;
  friend class PAGScene;
};

}  // namespace pagx
