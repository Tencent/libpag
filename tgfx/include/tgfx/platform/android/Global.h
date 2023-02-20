/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "JNIEnvironment.h"

namespace tgfx {
/**
 * The Global class manages the lifetime of JNI objects with global references.
 */
template<typename T>
class Global {
 public:
  Global() = default;

  Global(const Global<T>& that) = delete;

  Global<T>& operator=(const Global<T>& that) = delete;

  Global<T>& operator=(const T& that) {
    reset(that);
    return *this;
  }

  Global(T ref) {
    reset(ref);
  }

  ~Global() {
    reset();
  }

  void reset() {
    reset(nullptr);
  }

  void reset(T localRef) {
    if (ref == localRef) {
      return;
    }
    JNIEnvironment environment;
  auto env = environment.current();
    if (env == nullptr) {
      return;
    }
    if (ref != nullptr) {
      env->DeleteGlobalRef(ref);
      ref = nullptr;
    }
    if (localRef != nullptr) {
      ref = static_cast<T>(env->NewGlobalRef(localRef));
    }
  }

  T get() const {
    return ref;
  }

 private:
  T ref = nullptr;
};
}  // namespace tgfx
