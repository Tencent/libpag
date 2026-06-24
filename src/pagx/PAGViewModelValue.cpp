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
#include "pagx/PAGViewModelValueImage.h"
#include "pagx/PAGViewModelValueNumber.h"
#include "pagx/PAGViewModelValueString.h"
#include "pagx/PropertyData.h"
#include "pagx/SuppressDelegation.h"

namespace pagx {

// ---- Observer registration ---------------------------------------------------

ObserverHandle PAGViewModelValue::addObserver(Observer observer) {
  int id = nextObserverId++;
  observers.push_back({id, std::move(observer)});
  return ObserverHandle(shared_from_this(), id);
}

void PAGViewModelValue::removeObserver(int id) {
  for (auto it = observers.begin(); it != observers.end();) {
    if (it->id == id) {
      it = observers.erase(it);
    } else {
      ++it;
    }
  }
}

// ---- Observer notification ---------------------------------------------------

void PAGViewModelValue::notifyValueChanged() {
  auto s = scene.lock();
  if (s != nullptr && SuppressDelegation::isSuppressed(s)) {
    SuppressDelegation::addPendingNotification(s, this);
  } else {
    notifyObservers();
  }
}

void PAGViewModelValue::notifyObservers() {
  if (notifying) {
    return;  // Prevent recursive notification (Rive parity).
  }
  dirty = true;
  notifying = true;
  // Snapshot observers: callbacks may addObserver/removeObserver and invalidate the live vector.
  auto snapshot = observers;
  for (const auto& entry : snapshot) {
    entry.callback();
  }
  notifying = false;
}

// ---- Dirty lifecycle ---------------------------------------------------------

void PAGViewModelValue::advanced() {
  dirty = false;
}

// ---- Dependent tracking (DataBindRuntime) -----------------------------------

void PAGViewModelValue::notifyDependents() {
  for (auto* dep : dependents) {
    if (dep != nullptr) {
      dep->markDirtyForValue(this);
    }
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
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  dirty = true;
  notifyValueChanged();
  notifyDependents();
}

void PAGViewModelValueString::value(const std::string& v) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  dirty = true;
  notifyValueChanged();
  notifyDependents();
}

void PAGViewModelValueBoolean::value(bool v) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  dirty = true;
  notifyValueChanged();
  notifyDependents();
}

void PAGViewModelValueColor::value(const Color& v) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = v;
  dirty = true;
  notifyValueChanged();
  notifyDependents();
}

void PAGViewModelValueImage::value(std::shared_ptr<PAGImage> v) {
  if (propertyValue == v) {
    return;
  }
  propertyValue = std::move(v);
  dirty = true;
  notifyValueChanged();
  notifyDependents();
}

// ---- ObserverHandle ----------------------------------------------------------

ObserverHandle::ObserverHandle() = default;

ObserverHandle::ObserverHandle(std::shared_ptr<PAGViewModelValue> src, int id)
    : source(std::move(src)), observerId(id) {
}

ObserverHandle::~ObserverHandle() {
  detach();
}

ObserverHandle::ObserverHandle(ObserverHandle&& other) noexcept
    : source(std::move(other.source)), observerId(other.observerId) {
  other.observerId = 0;
}

ObserverHandle& ObserverHandle::operator=(ObserverHandle&& other) noexcept {
  if (this != &other) {
    detach();
    source = std::move(other.source);
    observerId = other.observerId;
    other.observerId = 0;
  }
  return *this;
}

void ObserverHandle::detach() {
  if (observerId == 0) {
    return;
  }
  auto src = source.lock();
  if (src) {
    src->removeObserver(observerId);
  }
  observerId = 0;
  source.reset();
}

}  // namespace pagx
