/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "JPAGSurface.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES3/gl3.h>
#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include "GPUDrawable.h"
#include "JNIHelper.h"
#include "NativePlatform.h"
#include "base/utils/TGFXCast.h"
#include "tgfx/platform/android/AndroidBitmap.h"

namespace pag {
static jfieldID PAGSurface_nativeSurface;
}  // namespace pag

using namespace pag;

std::shared_ptr<PAGSurface> getPAGSurface(JNIEnv* env, jobject thiz) {
  auto pagSurface =
      reinterpret_cast<JPAGSurface*>(env->GetLongField(thiz, PAGSurface_nativeSurface));
  if (pagSurface == nullptr) {
    return nullptr;
  }
  return pagSurface->get();
}

extern "C" {

PAG_API void Java_org_libpag_PAGSurface_nativeInit(JNIEnv* env, jclass clazz) {
  PAGSurface_nativeSurface = env->GetFieldID(clazz, "nativeSurface", "J");
}

PAG_API void Java_org_libpag_PAGSurface_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPAGSurface =
      reinterpret_cast<JPAGSurface*>(env->GetLongField(thiz, PAGSurface_nativeSurface));
  if (jPAGSurface != nullptr) {
    jPAGSurface->clear();
  }
}

PAG_API void Java_org_libpag_PAGSurface_nativeFinalize(JNIEnv* env, jobject thiz) {
  auto old = reinterpret_cast<JPAGSurface*>(env->GetLongField(thiz, PAGSurface_nativeSurface));
  delete old;
  env->SetLongField(thiz, PAGSurface_nativeSurface, (jlong)thiz);
}

PAG_API jint Java_org_libpag_PAGSurface_width(JNIEnv* env, jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return 0;
  }
  return surface->width();
}

PAG_API jint Java_org_libpag_PAGSurface_height(JNIEnv* env, jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return 0;
  }
  return surface->height();
}

PAG_API void Java_org_libpag_PAGSurface_updateSize(JNIEnv* env, jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return;
  }
  surface->updateSize();
}

PAG_API jboolean Java_org_libpag_PAGSurface_clearAll(JNIEnv* env, jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return static_cast<jboolean>(false);
  }
  auto changed = static_cast<jboolean>(surface->clearAll());
  return changed;
}

PAG_API void Java_org_libpag_PAGSurface_freeCache(JNIEnv* env, jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return;
  }
  surface->freeCache();
}

PAG_API jlong Java_org_libpag_PAGSurface_SetupFromSurfaceWithGLContext(JNIEnv* env, jclass,
                                                                       jobject surface,
                                                                       jlong shareContext) {
  if (surface == nullptr) {
    LOGE("PAGSurface.SetupFromSurface() Invalid surface specified.");
    return 0;
  }
  auto nativeWindow = ANativeWindow_fromSurface(env, surface);
  auto drawable = GPUDrawable::FromWindow(nativeWindow, reinterpret_cast<EGLContext>(shareContext));
  if (drawable == nullptr) {
    LOGE("PAGSurface.SetupFromSurface() Invalid surface specified.");
    return 0;
  }
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  return reinterpret_cast<jlong>(new JPAGSurface(pagSurface));
}

PAG_API jlong Java_org_libpag_PAGSurface_SetupFromTexture(JNIEnv*, jclass, jint textureID,
                                                          jint width, jint height, jboolean flipY,
                                                          jboolean forAsyncThread) {
  GLTextureInfo glInfo = {};
  glInfo.target = GL_TEXTURE_2D;
  glInfo.id = static_cast<unsigned>(textureID);
  glInfo.format = GL_RGBA8;
  BackendTexture glTexture(glInfo, width, height);
  auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;

  auto surface = PAGSurface::MakeFrom(glTexture, origin, forAsyncThread);
  if (surface == nullptr) {
    LOGE("PAGSurface.SetupFromTexture() Invalid texture specified.");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGSurface(surface));
}

PAG_API jlong JNICALL Java_org_libpag_PAGSurface_SetupOffscreen(JNIEnv*, jclass, jint width,
                                                                jint height) {
  auto surface = PAGSurface::MakeOffscreen(width, height);
  if (surface == nullptr) {
    LOGE("PAGSurface.SetupOffscreen(): Failed to create a offscreen PAGSurface!");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGSurface(surface));
}

PAG_API jboolean JNICALL Java_org_libpag_PAGSurface_copyPixelsTo(JNIEnv* env, jobject thiz,
                                                                 jobject bitmapObject) {
  if (thiz == nullptr || bitmapObject == nullptr) {
    return false;
  }
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return false;
  }
  auto hardwareBuffer = tgfx::AndroidBitmap::GetHardwareBuffer(env, bitmapObject);
  tgfx::Bitmap bitmap(hardwareBuffer);
  if (!bitmap.isEmpty()) {
    tgfx::Pixmap pixmap(bitmap);
    return surface->readPixels(ToPAG(pixmap.colorType()), ToPAG(pixmap.alphaType()),
                               pixmap.writablePixels(), pixmap.rowBytes());
  }
  auto info = tgfx::AndroidBitmap::GetInfo(env, bitmapObject);
  if (info.isEmpty()) {
    LOGE("PAGSurface::copyPixelsTo() Failed to read ImageInfo from the Java Bitmap!");
    return JNI_FALSE;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmapObject, &pixels) != 0) {
    env->ExceptionClear();
    LOGE("PAGSurface::copyPixelsTo() Failed to lock Pixels from the Java Bitmap!");
    return JNI_FALSE;
  }
  auto success = surface->readPixels(ToPAG(info.colorType()), ToPAG(info.alphaType()), pixels,
                                     info.rowBytes());
  AndroidBitmap_unlockPixels(env, bitmapObject);
  return success;
}
}
