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

#include "HandlerThread.h"
#include "core/utils/Log.h"

namespace tgfx {
static Global<jclass> HandlerThreadClass;
static jmethodID HandlerThread_Constructor;
static jmethodID HandlerThread_start;
static jmethodID HandlerThread_quit;
static jmethodID HandlerThread_getLooper;

void HandlerThread::JNIInit(JNIEnv* env) {
  HandlerThreadClass.reset(env, env->FindClass("android/os/HandlerThread"));
  HandlerThread_Constructor =
      env->GetMethodID(HandlerThreadClass.get(), "<init>", "(Ljava/lang/String;)V");
  HandlerThread_start = env->GetMethodID(HandlerThreadClass.get(), "start", "()V");
  HandlerThread_quit = env->GetMethodID(HandlerThreadClass.get(), "quit", "()Z");
  HandlerThread_getLooper =
      env->GetMethodID(HandlerThreadClass.get(), "getLooper", "()Landroid/os/Looper;");
}

std::shared_ptr<HandlerThread> HandlerThread::Make() {
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return nullptr;
  }
  Local<jstring> name = {env, env->NewStringUTF("tgfx_HandlerThread")};
  Local<jobject> thread = {
      env, env->NewObject(HandlerThreadClass.get(), HandlerThread_Constructor, name.get())};
  env->CallVoidMethod(thread.get(), HandlerThread_start);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("HandlerThread::Make(): failed to start the HandlerThread!");
    return nullptr;
  }
  return std::make_shared<HandlerThread>(env, thread.get());
}

HandlerThread::HandlerThread(JNIEnv* env, jobject handlerThread) {
  thread.reset(env, handlerThread);
  looper.reset(env, env->CallObjectMethod(handlerThread, HandlerThread_getLooper));
}

HandlerThread::~HandlerThread() {
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return;
  }
  env->CallBooleanMethod(thread.get(), HandlerThread_quit);
}
}  // namespace tgfx
