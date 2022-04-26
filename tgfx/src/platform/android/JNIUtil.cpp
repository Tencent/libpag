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

#include "JNIUtil.h"
#include <pthread.h>
#include <mutex>
#include "JNIInit.h"
#include "core/utils/Log.h"
#include "tgfx/platform/android/SetJavaVM.h"

namespace tgfx {
static std::mutex globalLocker = {};
static JavaVM* globalJavaVM = nullptr;
static pthread_key_t envKey = 0;

static void JNI_Thread_Destroy(void*) {
  if (globalJavaVM != nullptr) {
    globalJavaVM->DetachCurrentThread();
  }
}

bool SetJavaVM(void* javaVM) {
  {
    std::lock_guard<std::mutex> autoLock(globalLocker);
    if (globalJavaVM != nullptr && globalJavaVM != javaVM) {
      return false;
    }
    globalJavaVM = reinterpret_cast<JavaVM*>(javaVM);
    pthread_key_create(&envKey, JNI_Thread_Destroy);
  }
  auto env = CurrentJNIEnv();
  JNIInit(env);
  return true;
}

JNIEnv* CurrentJNIEnv() {
  std::lock_guard<std::mutex> autoLock(globalLocker);
  if (globalJavaVM == nullptr) {
    return nullptr;
  }
  auto env = reinterpret_cast<JNIEnv*>(pthread_getspecific(envKey));
  if (env != nullptr) {
    return env;
  }
  auto result = globalJavaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4);
  switch (result) {
    case JNI_EDETACHED: {
      JavaVMAttachArgs args = {JNI_VERSION_1_4, "TGFX_JNIEnv", nullptr};
      if (globalJavaVM->AttachCurrentThread(&env, &args) != JNI_OK) {
        LOGE("GetJNIEnv(): Failed to attach the JNI environment to the current thread!");
        env = nullptr;
      } else {
        pthread_setspecific(envKey, env);
      }
    }
    case JNI_OK:
      break;
    case JNI_EVERSION:
      LOGE("GetJNIEnv(): The specified JNI version is not supported!");
      break;
    default:
      LOGE("GetJNIEnv(): Failed to get the JNI environment attached to this thread!");
      break;
  }
  return env;
}

jstring SafeToJString(JNIEnv* env, const std::string& text) {
  static Global<jclass> StringClass(env, env->FindClass("java/lang/String"));
  static jmethodID StringConstructID =
      env->GetMethodID(StringClass.get(), "<init>", "([BLjava/lang/String;)V");
  Local<jbyteArray> array = {env, env->NewByteArray(text.size())};
  env->SetByteArrayRegion(array.get(), 0, text.size(),
                          reinterpret_cast<const jbyte*>(text.c_str()));
  Local<jstring> stringUTF = {env, env->NewStringUTF("UTF-8")};
  return (jstring)env->NewObject(StringClass.get(), StringConstructID, array.get(),
                                 stringUTF.get());
}
}  // namespace tgfx
