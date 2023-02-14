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

#include "JVideoSurface.h"
#include "JNIHelper.h"

namespace pag {
static jfieldID VideoSurface_nativeContext;

std::shared_ptr<tgfx::ImageBufferQueue> JVideoSurface::GetBufferQueue(JNIEnv* env,
                                                                      jobject videoSurface) {
  if (videoSurface == nullptr) {
    return nullptr;
  }
  auto surface =
      reinterpret_cast<JVideoSurface*>(env->GetLongField(videoSurface, VideoSurface_nativeContext));
  if (surface == nullptr) {
    return nullptr;
  }
  return surface->get();
}
}  // namespace pag

using namespace pag;

void setBufferQueue(JNIEnv* env, jobject thiz, JVideoSurface* surface) {
  auto old = reinterpret_cast<JVideoSurface*>(env->GetLongField(thiz, VideoSurface_nativeContext));
  delete old;
  env->SetLongField(thiz, VideoSurface_nativeContext, (jlong)surface);
}

extern "C" {

PAG_API void Java_org_libpag_VideoSurface_nativeInit(JNIEnv* env, jclass clazz) {
  VideoSurface_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_VideoSurface_nativeRelease(JNIEnv* env, jobject thiz) {
  auto surface =
      reinterpret_cast<JVideoSurface*>(env->GetLongField(thiz, VideoSurface_nativeContext));
  if (surface != nullptr) {
    surface->clear();
  }
}

PAG_API void Java_org_libpag_VideoSurface_nativeFinalize(JNIEnv* env, jobject thiz) {
  setBufferQueue(env, thiz, nullptr);
}

PAG_API void Java_org_libpag_VideoSurface_nativeSetup(JNIEnv* env, jobject thiz,
                                                      jobject surfaceTexture, jint width,
                                                      jint height) {
  auto bufferQueue = tgfx::ImageBufferQueue::MakeFrom(surfaceTexture, width, height);
  setBufferQueue(env, thiz, new JVideoSurface(bufferQueue));
}

PAG_API void Java_org_libpag_VideoSurface_notifyFrameAvailable(JNIEnv* env, jobject thiz) {
  auto bufferQueue = JVideoSurface::GetBufferQueue(env, thiz);
  if (bufferQueue == nullptr) {
    return;
  }
  bufferQueue->notifyFrameAvailable();
}
}