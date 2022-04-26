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

#include "JNIEnvironment.h"
#include <pthread.h>
#include "tgfx/platform/Print.h"
#include "tgfx/platform/android/SetJavaVM.h"

static JavaVM* globalJavaVM = nullptr;
static pthread_key_t threadKey = 0;

static void JNI_Thread_Destroy(void* value) {
  JNIEnv* jniEnv = (JNIEnv*)value;
  if (jniEnv != nullptr && globalJavaVM != nullptr) {
    globalJavaVM->DetachCurrentThread();
    pthread_setspecific(threadKey, nullptr);
  }
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
  globalJavaVM = vm;
  tgfx::SetJavaVM(vm);
  pthread_key_create(&threadKey, JNI_Thread_Destroy);
  return JNI_VERSION_1_4;
}

extern "C" void JNI_OnUnload(JavaVM*, void*) {
  pthread_key_delete(threadKey);
}

JNIEnv* JNIEnvironment::Current() {
  if (globalJavaVM == nullptr) {
    return nullptr;
  }
  JNIEnv* jniEnv = nullptr;
  auto result = globalJavaVM->GetEnv(reinterpret_cast<void**>(&jniEnv), JNI_VERSION_1_4);

  if (result == JNI_EDETACHED || jniEnv == nullptr) {
    JavaVMAttachArgs args = {JNI_VERSION_1_4, "PAG_JNIEnvironment", nullptr};
    if (globalJavaVM->AttachCurrentThread(&jniEnv, &args) != JNI_OK) {
      return jniEnv;
    }
    pthread_setspecific(threadKey, (void*)jniEnv);
    return jniEnv;
  } else if (result == JNI_OK) {
    return jniEnv;
  }
  return jniEnv;
}
