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

#include "JPAGSurface.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES3/gl3.h>
#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include "GPUDecoder.h"
#include "GPUDrawable.h"
#include "HardwareBufferDrawable.h"
#include "JNIHelper.h"
#include "NativePlatform.h"
#include "VideoSurface.h"

namespace pag {
static jfieldID PAGSurface_nativeSurface;
}

using namespace pag;

void setPAGSurface(JNIEnv* env, jobject thiz, JPAGSurface* surface) {
  auto old = reinterpret_cast<JPAGSurface*>(env->GetLongField(thiz, PAGSurface_nativeSurface));
  if (old != nullptr) {
    delete old;
  }
  env->SetLongField(thiz, PAGSurface_nativeSurface, (jlong)surface);
}

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
  // 调用堆栈源头从C++触发而不是Java触发的情况下，FindClass
  // 可能会失败，因此要提前初始化这部分反射方法。
  NativePlatform::InitJNI(env);
}

PAG_API void Java_org_libpag_PAGSurface_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPAGSurface =
      reinterpret_cast<JPAGSurface*>(env->GetLongField(thiz, PAGSurface_nativeSurface));
  if (jPAGSurface != nullptr) {
    jPAGSurface->clear();
  }
}

PAG_API void Java_org_libpag_PAGSurface_nativeFinalize(JNIEnv* env, jobject thiz) {
  setPAGSurface(env, thiz, nullptr);
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
  if (surface == NULL) {
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
}

extern "C" PAG_API jobject JNICALL Java_org_libpag_PAGSurface_makeSnapshot(JNIEnv* env,
                                                                           jobject thiz) {
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return nullptr;
  }
  int widht = surface->width();
  int height = surface->height();
  unsigned char* newBitmapPixels;
  jclass bitmapCls = env->FindClass("android/graphics/Bitmap");
  jmethodID createBitmapFunction = env->GetStaticMethodID(
      bitmapCls, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  jstring configName = env->NewStringUTF("ARGB_8888");
  jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
  jmethodID valueOfBitmapConfigFunction = env->GetStaticMethodID(
      bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
  jobject bitmapConfig =
      env->CallStaticObjectMethod(bitmapConfigClass, valueOfBitmapConfigFunction, configName);
  jobject newBitmap =
      env->CallStaticObjectMethod(bitmapCls, createBitmapFunction, widht, height, bitmapConfig);
  int ret;
  if ((ret = AndroidBitmap_lockPixels(env, newBitmap, (void**)&newBitmapPixels)) < 0) {
    LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    return nullptr;
  }
  bool status = surface->readPixels(pag::ColorType::RGBA_8888, pag::AlphaType::Premultiplied,
                                    newBitmapPixels, widht * 4);
  if (!status) {
    LOGE("ReadPixels failed!");
  }
  AndroidBitmap_unlockPixels(env, newBitmap);
  return newBitmap;
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_libpag_PAGSurface_SetupFromHardwareBuffer(JNIEnv* env, jclass, jint width, jint height) {
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  auto drawable =
      HardwareBufferDrawable::Make(static_cast<int>(width), static_cast<int>(height), device);
  auto surface = PAGSurface::MakeFrom(drawable);
  if (surface == nullptr) {
    LOGE("PAGSurface.SetupFromHardwareBuffer() fail.");
    return nullptr;
  }
  static auto PAGSurface_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGSurface"));
  static auto PAGSurface_Constructor = env->GetMethodID(PAGSurface_Class.get(), "<init>", "(J)V");
  auto surfaceObject = env->NewObject(PAGSurface_Class.get(), PAGSurface_Constructor,
                                      reinterpret_cast<jlong>(new JPAGSurface(surface)));
  static auto PAGSurface_HardwareBuffer = env->GetFieldID(PAGSurface_Class.get(), "hardwareBuffer",
                                                          "Landroid/hardware/HardwareBuffer;");
  env->SetObjectField(surfaceObject, PAGSurface_HardwareBuffer,
                      tgfx::HardwareBufferInterface::AHardwareBuffer_toHardwareBuffer(
                          env, drawable->aHardwareBuffer()));
  return surfaceObject;
}