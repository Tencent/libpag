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
#include <string>

namespace tgfx {
/**
 * Attach permanently a JNI environment to the current thread and retrieve it. If successfully
 * attached, the JNI environment will automatically be detached at thread destruction.
 */
JNIEnv* CurrentJNIEnv();

/**
 * There is a known bug in env->NewStringUTF which will lead to crash in some devices.
 * So we use this method instead.
 */
jstring SafeToJString(JNIEnv* env, const std::string& text);

template <typename T>
class Global {
 public:
  Global() = default;

  Global(const Global<T>& that) = delete;

  Global<T>& operator=(const Global<T>& that) = delete;

  Global(JNIEnv* env, T ref) {
    reset(env, ref);
  }

  ~Global() {
    reset(nullptr, nullptr);
  }

  void reset(JNIEnv* env, T ref) {
    if (_ref == ref) {
      return;
    }
    if (env == nullptr) {
      env = CurrentJNIEnv();
      if (env == nullptr) {
        return;
      }
    }
    if (_ref != nullptr) {
      env->DeleteGlobalRef(_ref);
      _ref = nullptr;
    }
    if (ref == nullptr) {
      _ref = nullptr;
    } else {
      _ref = static_cast<T>(env->NewGlobalRef(ref));
    }
  }

  bool empty() const {
    return _ref == nullptr;
  }

  T get() const {
    return _ref;
  }

 private:
  T _ref = nullptr;
};

template <typename T>
class Local {
 public:
  Local() = default;

  Local(const Local<T>& that) = delete;

  Local<T>& operator=(const Local<T>& that) = delete;

  Local(JNIEnv* env, T ref) : _env(env), _ref(ref) {
  }

  ~Local() {
    if (_env != nullptr) {
      _env->DeleteLocalRef(_ref);
    }
  }

  void reset(JNIEnv* env, T ref) {
    if (ref == _ref) {
      return;
    }
    if (_env != nullptr && _ref != nullptr) {
      _env->DeleteLocalRef(_ref);
    }
    _env = env;
    _ref = ref;
  }

  bool empty() const {
    return _ref == nullptr;
  }

  T get() const {
    return _ref;
  }

 private:
  JNIEnv* _env = nullptr;
  T _ref = nullptr;
};

}  // namespace tgfx
