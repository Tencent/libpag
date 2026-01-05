/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <jni.h>
#include "JNIHelper.h"
#include "tgfx/core/Task.h"

extern "C" PAG_API void Java_org_libpag_NativeTask_Run(JNIEnv* env, jclass, jobject runnable) {
  static pag::Global<jclass> runnableClass = env->FindClass("java/lang/Runnable");
  if (runnableClass.get() == nullptr || runnable == nullptr) {
    return;
  }
  static jmethodID runMethod = env->GetMethodID(runnableClass.get(), "run", "()V");
  auto gRunnable = env->NewGlobalRef(runnable);
  tgfx::Task::Run([gRunnable]() {
    tgfx::JNIEnvironment environment;
    auto jniEnv = environment.current();
    jniEnv->CallVoidMethod(gRunnable, runMethod);
    jniEnv->DeleteGlobalRef(gRunnable);
  });
}
