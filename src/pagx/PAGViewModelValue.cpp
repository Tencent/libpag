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

#include "pagx/PAGViewModelValue.h"
#include <algorithm>
#include "pagx/DataBindRuntime.h"
#include "pagx/ObserverHandle.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGViewModelValueBoolean.h"
#include "pagx/PAGViewModelValueColor.h"
#include "pagx/PAGViewModelValueEnum.h"
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PAGViewModelValueTrigger.h"
#include "pagx/PropertyData.h"
#include "pagx/SuppressDelegation.h"

namespace pagx {

// ---- Observer registration ---------------------------------------------------

ObserverHandle PAGViewModelValue::addObserver(Observer observer) {
  auto self = weak_from_this().lock();
  if (self == nullptr) {
    return ObserverHandle();
  }
  int id = nextObserverId++;
  observers.push_back({id, std::move(observer)});
  return ObserverHandle(self, id);
}

void PAGViewModelValue::removeObserver(int id) {
  for (auto it = observers.begin(); it != observers.end();) {
    if (it->id == id) {
      observers.erase(it);
      break;
    } else {
      ++it;
    }
  }
}

// ---- Observer notification ---------------------------------------------------

void PAGViewModelValue::notifyValueChanged() {
  auto s = scene.lock();
  if (s != nullptr && SuppressDelegation::IsSuppressed(s)) {
    SuppressDelegation::AddPendingNotification(s, this);
  } else {
    notifyObservers();
  }
}

void PAGViewModelValue::notifyObservers() {
  if (notifying) {
    return;  // Prevent recursive notification (Rive parity).
  }
  notifying = true;
  // Snapshot observers: callbacks may addObserver/removeObserver and invalidate the live vector.
  auto snapshot = observers;
  for (const auto& entry : snapshot) {
    entry.callback();
  }
  notifying = false;
}

void PAGViewModelValue::notifyChanged(bool fromVM) {
  if (fromVM) {
    dirty = true;
    notifyValueChanged();
    notifyDependents();
  } else {
    notifyValueChanged();
  }
}

// ---- Dirty lifecycle ---------------------------------------------------------

void PAGViewModelValue::clearDirty() {
  dirty = false;
}

// ---- Dependent tracking (DataBindRuntime) -----------------------------------

void PAGViewModelValue::notifyDependents() {
  for (auto* dep : dependents) {
    dep->markDirtyForValue(this);
  }
}

void PAGViewModelValue::addDependent(DataBindRuntime* runtime) {
  if (runtime == nullptr) {
    return;
  }
  for (auto* existing : dependents) {
    if (existing == runtime) {
      return;
    }
  }
  dependents.push_back(runtime);
}

void PAGViewModelValue::removeDependent(DataBindRuntime* runtime) {
  dependents.erase(std::remove(dependents.begin(), dependents.end(), runtime), dependents.end());
}

// ---- Typed value setters -----------------------------------------------------

void PAGViewModelValueNumber::value(float v) {
  setValueInternal(v, true);
}

void PAGViewModelValueNumber::setValueInternal(float v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  notifyChanged(fromVM);
}

void PAGViewModelValueString::value(const std::string& v) {
  setValueInternal(v, true);
}

void PAGViewModelValueString::setValueInternal(const std::string& v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  notifyChanged(fromVM);
}

void PAGViewModelValueBoolean::value(bool v) {
  setValueInternal(v, true);
}

void PAGViewModelValueBoolean::setValueInternal(bool v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  notifyChanged(fromVM);
}

void PAGViewModelValueEnum::value(int v) {
  setValueInternal(v, true);
}

void PAGViewModelValueEnum::setValueInternal(int v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  notifyChanged(fromVM);
}

void PAGViewModelValueColor::value(const Color& v) {
  setValueInternal(v, true);
}

void PAGViewModelValueColor::setValueInternal(const Color& v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  notifyChanged(fromVM);
}

void PAGViewModelValueImage::value(std::shared_ptr<PAGImage> v) {
  // Mark as business-assigned even when the value is unchanged: an explicit assignment (including
  // clearing to null) claims ownership so a later resource load does not overwrite it.
  userAssigned = true;
  setValueInternal(std::move(v), true);
}

void PAGViewModelValueImage::setValueInternal(std::shared_ptr<PAGImage> v, bool fromVM) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = std::move(v);
  notifyChanged(fromVM);
}

void PAGViewModelValueTrigger::fire() {
  // A trigger is a value-less pulse: every call is a distinct event, so there is no same-value
  // short-circuit. The dirty flag is set so dependent data bindings are notified, though triggers
  // are typically consumed by observers (e.g. StateMachine inputs) rather than data bindings.
  dirty = true;
  notifyChanged(true);
}

// ---- ObserverHandle ----------------------------------------------------------

ObserverHandle::ObserverHandle() = default;

ObserverHandle::ObserverHandle(std::shared_ptr<Observable> src, int observerId)
    : source(std::move(src)), id(observerId) {
}

ObserverHandle::~ObserverHandle() {
  detach();
}

ObserverHandle::ObserverHandle(ObserverHandle&& other) noexcept
    : source(std::move(other.source)), id(other.id) {
  other.id = -1;
}

ObserverHandle& ObserverHandle::operator=(ObserverHandle&& other) noexcept {
  if (this != &other) {
    detach();
    source = std::move(other.source);
    id = other.id;
    other.id = -1;
  }
  return *this;
}

void ObserverHandle::detach() {
  if (id < 0) {
    return;
  }
  auto src = source.lock();
  if (src) {
    src->removeObserver(id);
  }
  id = -1;
  source.reset();
}

}  // namespace pagx
