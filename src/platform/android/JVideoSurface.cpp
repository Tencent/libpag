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

std::shared_ptr<tgfx::SurfaceTexture> JVideoSurface::GetImageStream(JNIEnv* env,
                                                                    jobject videoSurface) {
  if (videoSurface == nullptr) {
    return nullptr;
  }
  auto stream =
      reinterpret_cast<JVideoSurface*>(env->GetLongField(videoSurface, VideoSurface_nativeContext));
  if (stream == nullptr) {
    return nullptr;
  }
  return stream->get();
}
}  // namespace pag

using namespace pag;

void setImageStream(JNIEnv* env, jobject thiz, JVideoSurface* surface) {
  auto old = reinterpret_cast<JVideoSurface*>(env->GetLongField(thiz, VideoSurface_nativeContext));
  delete old;
  env->SetLongField(thiz, VideoSurface_nativeContext, (jlong)surface);
}

extern "C" {

PAG_API void Java_org_libpag_VideoSurface_nativeInit(JNIEnv* env, jclass clazz) {
  VideoSurface_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_VideoSurface_nativeRelease(JNIEnv* env, jobject thiz) {
  auto stream =
      reinterpret_cast<JVideoSurface*>(env->GetLongField(thiz, VideoSurface_nativeContext));
  if (stream != nullptr) {
    stream->clear();
  }
}

PAG_API void Java_org_libpag_VideoSurface_nativeFinalize(JNIEnv* env, jobject thiz) {
  setImageStream(env, thiz, nullptr);
}

PAG_API void Java_org_libpag_VideoSurface_nativeSetup(JNIEnv* env, jobject thiz, jint width,
                                                      jint height) {
  auto imageStream = tgfx::SurfaceTexture::Make(width, height, thiz);
  if (imageStream == nullptr) {
    return;
  }
  setImageStream(env, thiz, new JVideoSurface(imageStream));
}

PAG_API jobject Java_org_libpag_VideoSurface_getInputSurface(JNIEnv* env, jobject thiz) {
  auto stream = JVideoSurface::GetImageStream(env, thiz);
  if (stream == nullptr) {
    return nullptr;
  }
  return stream->getInputSurface();
}

PAG_API void Java_org_libpag_VideoSurface_notifyFrameAvailable(JNIEnv* env, jobject thiz) {
  auto stream = JVideoSurface::GetImageStream(env, thiz);
  if (stream == nullptr) {
    return;
  }
  stream->notifyFrameAvailable();
}
}