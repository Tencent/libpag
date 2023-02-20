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

#include "tgfx/platform/android/JNIEnvironment.h"
#include <pthread.h>
#include <atomic>
#include <mutex>
#include "HandlerThread.h"
#include "NativeCodec.h"
#include "NativeImageReader.h"
#include "core/utils/Log.h"

namespace tgfx {
// https://android.googlesource.com/platform/art/+/android-8.0.0_r36/runtime/jni_env_ext.h#39
static constexpr size_t LOCALS_CAPACITY = 512;

static std::atomic<JavaVM*> globalJavaVM = nullptr;
static pthread_key_t envKey = 0;

static void JNIInit() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  initialized = true;
  NativeCodec::JNIInit(env);
  HandlerThread::JNIInit(env);
  NativeImageReader::JNIInit(env);
}

static void JNI_Thread_Destroy(void*) {
  JavaVM* javaVM = globalJavaVM;
  if (javaVM != nullptr) {
    javaVM->DetachCurrentThread();
    pthread_setspecific(envKey, nullptr);
  }
}

bool JNIEnvironment::SetJavaVM(JavaVM* javaVM) {
  if (globalJavaVM == javaVM) {
    return true;
  }
  if (globalJavaVM != nullptr && javaVM != nullptr && globalJavaVM != javaVM) {
    return false;
  }
  globalJavaVM = javaVM;
  if (globalJavaVM != nullptr) {
    pthread_key_create(&envKey, JNI_Thread_Destroy);
    JNIInit();
  } else {
    pthread_key_delete(envKey);
  }
  return true;
}

JNIEnv* JNIEnvironment::Current() {
  JavaVM* javaVM = globalJavaVM;
  if (javaVM == nullptr) {
    LOGE(
        "JNIEnvironment::Current(): The global JavaVM has not been set yet! "
        "Please use JNIEnvironment::SetJavaVM() to initialize the global JavaVM.");
    return nullptr;
  }
  JNIEnv* env = nullptr;
  auto result = javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4);
  if (result == JNI_EDETACHED || env == nullptr) {
    JavaVMAttachArgs args = {JNI_VERSION_1_4, "tgfx_JNIEnvironment", nullptr};
    if (javaVM->AttachCurrentThread(&env, &args) == JNI_OK) {
      pthread_setspecific(envKey, (void*)env);
    }
  }
  return env;
}

JNIEnvironment::JNIEnvironment() {
  env = JNIEnvironment::Current();
  if (env != nullptr) {
    env->PushLocalFrame(LOCALS_CAPACITY);
  }
}

JNIEnvironment::~JNIEnvironment() {
  if (env != nullptr) {
    env->PopLocalFrame(nullptr);
  }
}
}  // namespace tgfx
