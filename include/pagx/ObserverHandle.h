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

namespace pagx {

class PAGViewModelValue;
class PAGStateMachine;

/**
 * ObserverHandle is an RAII handle for a registered value-change observer. The observer is
 * automatically removed when the handle is destroyed. Call detach() to remove the observer
 * early. Move-only; copying is forbidden.
 */
class ObserverHandle {
 public:
  ObserverHandle();
  ~ObserverHandle();

  ObserverHandle(const ObserverHandle&) = delete;
  ObserverHandle& operator=(const ObserverHandle&) = delete;

  ObserverHandle(ObserverHandle&& other) noexcept;
  ObserverHandle& operator=(ObserverHandle&& other) noexcept;

  /**
   * Removes the observer immediately. Safe to call multiple times; subsequent calls are no-ops.
   * Implicitly called by the destructor.
   */
  void detach();

  /** Creates a handle for a PAGViewModelValue observer. */
  ObserverHandle(std::shared_ptr<PAGViewModelValue> source, int observerId);

  /** Creates a handle for a PAGStateMachine state-change listener. */
  ObserverHandle(std::shared_ptr<PAGStateMachine> source, int listenerId);

 private:
  enum class SourceType { None, ViewModel, StateMachine };
  SourceType type = SourceType::None;
  std::weak_ptr<PAGViewModelValue> vmSource;
  std::weak_ptr<PAGStateMachine> smSource;
  int id = -1;
};

}  // namespace pagx
