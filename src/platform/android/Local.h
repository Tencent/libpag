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

#pragma once

#include <jni.h>

template <typename T>
class Local {
 public:
  Local() = default;

  Local(const Local<T>& that) = delete;

  Local<T>& operator=(const Local<T>& that) = delete;

  Local(JNIEnv* env, T ref) : _env(env), ref(ref) {
  }

  ~Local() {
    if (_env != nullptr) {
      _env->DeleteLocalRef(ref);
    }
  }

  void reset(JNIEnv* env, T reference) {
    if (reference == ref) {
      return;
    }
    if (_env != nullptr) {
      _env->DeleteLocalRef(ref);
    }
    _env = env;
    ref = reference;
  }

  bool empty() const {
    return ref == nullptr;
  }

  T get() const {
    return ref;
  }

  JNIEnv* env() const {
    return _env;
  }

 private:
  JNIEnv* _env = nullptr;
  T ref = nullptr;
};