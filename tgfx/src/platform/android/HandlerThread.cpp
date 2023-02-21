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
#include "utils/Log.h"

namespace tgfx {
static Global<jclass> HandlerThreadClass;
static jmethodID HandlerThread_Constructor;
static jmethodID HandlerThread_start;
static jmethodID HandlerThread_quit;
static jmethodID HandlerThread_getLooper;

void HandlerThread::JNIInit(JNIEnv* env) {
  HandlerThreadClass = env->FindClass("android/os/HandlerThread");
  HandlerThread_Constructor =
      env->GetMethodID(HandlerThreadClass.get(), "<init>", "(Ljava/lang/String;)V");
  HandlerThread_start = env->GetMethodID(HandlerThreadClass.get(), "start", "()V");
  HandlerThread_quit = env->GetMethodID(HandlerThreadClass.get(), "quit", "()Z");
  HandlerThread_getLooper =
      env->GetMethodID(HandlerThreadClass.get(), "getLooper", "()Landroid/os/Looper;");
}

std::shared_ptr<HandlerThread> HandlerThread::Make() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto name = env->NewStringUTF("tgfx_HandlerThread");
  auto thread = env->NewObject(HandlerThreadClass.get(), HandlerThread_Constructor, name);
  env->CallVoidMethod(thread, HandlerThread_start);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("HandlerThread::Make(): failed to start the HandlerThread!");
    return nullptr;
  }
  return std::make_shared<HandlerThread>(env, thread);
}

HandlerThread::HandlerThread(JNIEnv* env, jobject handlerThread) {
  thread = handlerThread;
  looper = env->CallObjectMethod(handlerThread, HandlerThread_getLooper);
}

HandlerThread::~HandlerThread() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  env->CallBooleanMethod(thread.get(), HandlerThread_quit);
}
}  // namespace tgfx
