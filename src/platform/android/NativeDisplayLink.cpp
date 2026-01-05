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

#include "NativeDisplayLink.h"

namespace pag {
static Global<jclass> DisplayLinkClass;
static jmethodID DisplayLink_Create;
static jmethodID DisplayLink_start;
static jmethodID DisplayLink_stop;

void NativeDisplayLink::InitJNI(JNIEnv* env) {
  DisplayLinkClass = env->FindClass("org/libpag/DisplayLink");
  if (DisplayLinkClass.get() == nullptr) {
    LOGE("Could not run NativeDisplayLink.InitJNI(), DisplayLinkClass is not found!");
    return;
  }
  DisplayLink_Create =
      env->GetStaticMethodID(DisplayLinkClass.get(), "Create", "(J)Lorg/libpag/DisplayLink;");
  DisplayLink_start = env->GetMethodID(DisplayLinkClass.get(), "start", "()V");
  DisplayLink_stop = env->GetMethodID(DisplayLinkClass.get(), "stop", "()V");
}

std::shared_ptr<DisplayLink> NativeDisplayLink::Make(std::function<void()> callback) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto displayLink = std::shared_ptr<NativeDisplayLink>(new NativeDisplayLink(std::move(callback)));
  displayLink->animator = env->CallStaticObjectMethod(DisplayLinkClass.get(), DisplayLink_Create,
                                                      reinterpret_cast<jlong>(displayLink.get()));
  if (displayLink->animator.isEmpty()) {
    return nullptr;
  }
  return displayLink;
}

NativeDisplayLink::NativeDisplayLink(std::function<void()> callback)
    : callback(std::move(callback)) {
}

NativeDisplayLink::~NativeDisplayLink() {
  stop();
}

void NativeDisplayLink::start() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || animator.isEmpty()) {
    return;
  }
  env->CallVoidMethod(animator.get(), DisplayLink_start);
}

void NativeDisplayLink::stop() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || animator.isEmpty()) {
    return;
  }
  env->CallVoidMethod(animator.get(), DisplayLink_stop);
}

void NativeDisplayLink::update() {
  callback();
}
}  // namespace pag

extern "C" {

PAG_API void Java_org_libpag_DisplayLink_onUpdate(JNIEnv*, jobject, jlong context) {
  auto displayLink = reinterpret_cast<pag::NativeDisplayLink*>(context);
  if (displayLink == nullptr) {
    return;
  }
  displayLink->update();
}
}
