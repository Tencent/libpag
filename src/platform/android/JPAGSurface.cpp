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
#include "GPUDrawable.h"
#include "JNIHelper.h"
#include "NativePlatform.h"
#include "base/utils/TGFXCast.h"
#include "tgfx/src/platform/android/AHardwareBufferUtil.h"
#include "tgfx/src/platform/android/HardwareBufferInterface.h"

namespace pag {
static jfieldID PAGSurface_nativeSurface;
static Global<jclass> Bitmap_Class;
static jmethodID Bitmap_createBitmap;
static jmethodID Bitmap_isRecycled;
static jmethodID Bitmap_getConfig;
static jmethodID BitmapConfig_equals;
static Global<jclass> Config_Class;
static jfieldID Config_ARGB_888;
static jfieldID Config_HARDWARE;
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
  Bitmap_Class = env->FindClass("android/graphics/Bitmap");
  Bitmap_createBitmap =
      env->GetStaticMethodID(Bitmap_Class.get(), "createBitmap",
                             "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  Bitmap_isRecycled = env->GetMethodID(Bitmap_Class.get(), "isRecycled", "()Z");
  Bitmap_getConfig =
      env->GetMethodID(Bitmap_Class.get(), "getConfig", "()Landroid/graphics/Bitmap$Config;");
  Config_Class = env->FindClass("android/graphics/Bitmap$Config");
  Config_ARGB_888 =
      env->GetStaticFieldID(Config_Class.get(), "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
  Config_HARDWARE =
      env->GetStaticFieldID(Config_Class.get(), "HARDWARE", "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_equals = env->GetMethodID(Config_Class.get(), "equals", "(Ljava/lang/Object;)Z");
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
  if (old != nullptr) {
    delete old;
  }
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
  int width = surface->width();
  int height = surface->height();
  unsigned char* newBitmapPixels;
  auto config = env->GetStaticObjectField(Config_Class.get(), Config_ARGB_888);
  jobject newBitmap =
      env->CallStaticObjectMethod(Bitmap_Class.get(), Bitmap_createBitmap, width, height, config);
  int ret;
  if ((ret = AndroidBitmap_lockPixels(env, newBitmap, (void**)&newBitmapPixels)) < 0) {
    LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    return nullptr;
  }
  AndroidBitmapInfo info;
  AndroidBitmap_getInfo(env, newBitmap, &info);
  int stride = info.stride;
  if (stride == 0) {
    stride = width * 4;
  }
  bool status = surface->readPixels(pag::ColorType::RGBA_8888, pag::AlphaType::Premultiplied,
                                    newBitmapPixels, stride);

  AndroidBitmap_unlockPixels(env, newBitmap);
  if (!status) {
    LOGE("ReadPixels failed!");
    return nullptr;
  }
  return newBitmap;
}

extern "C" JNIEXPORT jlong JNICALL Java_org_libpag_PAGSurface_SetupOffscreen(JNIEnv*, jclass,
                                                                             jint width,
                                                                             jint height) {
  auto surface = PAGSurface::MakeOffscreen(width, height);
  if (surface == nullptr) {
    LOGE("PAGSurface.SetupOffscreen(): Failed to create a offscreen PAGSurface!");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGSurface(surface));
}

static bool readPixelsToHardwareBuffer(JNIEnv* env, std::shared_ptr<PAGSurface> surface,
                                       jobject jBitmap) {
  auto buffer = tgfx::HardwareBufferInterface::AHardwareBuffer_fromBitmap(env, jBitmap);

  uint8_t* pixels = nullptr;
  if (0 != tgfx::HardwareBufferInterface::Lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                               nullptr, reinterpret_cast<void**>(&pixels))) {
    LOGE("Failed to AHardwareBuffer_lock");
    return false;
  }
  bool res = false;
  if (pixels) {
    auto imageInfo = tgfx::GetImageInfo(buffer);
    if (imageInfo.width() != surface->width() || imageInfo.height() != surface->height()) {
      tgfx::HardwareBufferInterface::Unlock(buffer, nullptr);
      return false;
    }
    res = surface->readPixels(ToPAG(imageInfo.colorType()), ToPAG(imageInfo.alphaType()), pixels,
                              imageInfo.rowBytes());
  }
  tgfx::HardwareBufferInterface::Unlock(buffer, nullptr);
  return res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_org_libpag_PAGSurface_copyPixelsTo(JNIEnv* env,
                                                                              jobject thiz,
                                                                              jobject jBitmap) {
  if (thiz == nullptr || jBitmap == nullptr) {
    return false;
  }
  auto surface = getPAGSurface(env, thiz);
  if (surface == nullptr) {
    return false;
  }
  bool isRecycled = env->CallBooleanMethod(jBitmap, Bitmap_isRecycled);
  if (isRecycled) {
    return false;
  }
  auto config = env->CallObjectMethod(jBitmap, Bitmap_getConfig);
  static Global<jobject> HardwareConfig =
      env->GetStaticObjectField(Config_Class.get(), Config_HARDWARE);
  if (env->CallBooleanMethod(config, BitmapConfig_equals, HardwareConfig.get())) {
    return readPixelsToHardwareBuffer(env, surface, jBitmap);
  }
  unsigned char* newBitmapPixels;
  auto imageInfo = GetImageInfo(env, jBitmap);
  if (imageInfo.width() != surface->width() || imageInfo.height() != surface->height()) {
    return false;
  }
  int ret;
  if ((ret = AndroidBitmap_lockPixels(env, jBitmap, (void**)&newBitmapPixels)) < 0) {
    LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
    return false;
  }
  bool status = surface->readPixels(ToPAG(imageInfo.colorType()), ToPAG(imageInfo.alphaType()),
                                    newBitmapPixels, imageInfo.rowBytes());
  AndroidBitmap_unlockPixels(env, jBitmap);
  if (!status) {
    LOGE("ReadPixels failed!");
  }
  return status;
}