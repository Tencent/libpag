/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include <string>
#include <utility>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
namespace exporter {

class ScopedTimeSetter {
 public:
  ScopedTimeSetter(const AEGP_ItemH& itemHandle, float time);
  ~ScopedTimeSetter();

 private:
  void* address = nullptr;
  const AEGP_ItemH& itemHandle = nullptr;
  A_Time orgTime = {0, 100};
};

template <typename T>
class ScopedAssign {
 public:
  explicit ScopedAssign(T& storeValue, T newValue = T()) : value(storeValue), pAddr(&storeValue) {
    storeValue = std::move(newValue);
  }

  ScopedAssign(const ScopedAssign&) = delete;
  ScopedAssign& operator=(const ScopedAssign&) = delete;

  ScopedAssign(ScopedAssign&& other) noexcept : value(std::move(other.value)), pAddr(other.pAddr) {
    other.pAddr = nullptr;
  }

  ScopedAssign& operator=(ScopedAssign&& other) noexcept {
    if (this != &other) {
      value = std::move(other.value);
      pAddr = other.pAddr;
      other.pAddr = nullptr;
    }
    return *this;
  }

  ~ScopedAssign() {
    if (pAddr) {
      *pAddr = value;
    }
  }

 private:
  T value;
  T* pAddr = nullptr;
};

}  // namespace exporter
