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
//  license is distributed on an "AS is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

namespace pagx {

/**
 * Observable is a base class for objects that support observer registration and removal via
 * integer IDs. PAGViewModelValue and PAGStateMachine both inherit from it so that ObserverHandle
 * can manage any observer type through a single weak_ptr.
 */
class Observable {
 public:
  virtual ~Observable() = default;

  /**
   * Removes the observer identified by the given id. Safe to call with an unknown id.
   */
  virtual void removeObserver(int id) = 0;
};

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

  /** Creates a handle for an Observable source (PAGViewModelValue or PAGStateMachine). */
  ObserverHandle(std::shared_ptr<Observable> source, int observerId);

 private:
  std::weak_ptr<Observable> source;
  int id = -1;
};

}  // namespace pagx
