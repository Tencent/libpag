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

#include "JNIEnvironment.h"

template<typename T>
class Global {
 public:
  Global() : mEnv(nullptr), mRef(nullptr) {
  }

  Global(JNIEnv* env, T ref) : mEnv(nullptr), mRef(nullptr) {
    reset(env, ref);
  }

  ~Global() {
    reset(nullptr, nullptr);
  }

  void reset(JNIEnv* env, T ref) {
    if (mRef == ref) {
      return;
    }
    if (mRef != nullptr) {
      if (env == nullptr) {
        env = JNIEnvironment::Current();
        if (env == nullptr) {
          return;
        }
      }

      env->DeleteGlobalRef(mRef);
      mRef = nullptr;
    }
    mEnv = env;
    if (ref == nullptr) {
      mRef = nullptr;
    } else {
      mRef = (T)mEnv->NewGlobalRef(ref);
    }
  }

  T get() const {
    return mRef;
  }

 private:
  JNIEnv* mEnv;
  T mRef;
};
