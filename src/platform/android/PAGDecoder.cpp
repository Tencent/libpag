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

#include "PAGDecoder.h"
#include <android/bitmap.h>
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/opengl/GLSampler.h"
#include "HardwareBufferDrawable.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
#include "tgfx/gpu/opengl/GLSampler.h"
#include "tgfx/src/platform/android/HardwareBufferInterface.h"

namespace pag {

static Global<jclass> Bitmap_Class;
static jmethodID Bitmap_createBitmap;
static jmethodID Bitmap_wrapHardwareBuffer;
static jmethodID Bitmap_isRecycled;
static Global<jclass> Config_Class;
static jfieldID Config_ARGB_888;

PAGDecoder::~PAGDecoder() {
  if (device && textureInfo.id > 0) {
    auto context = device->lockContext();
    if (context) {
      auto gl = tgfx::GLFunctions::Get(context);
      gl->deleteTextures(1, &textureInfo.id);
      device->unlock();
    }
  }
}

void PAGDecoder::initJNI(JNIEnv *env) {
  Bitmap_Class.reset(env, env->FindClass("android/graphics/Bitmap"));
  Bitmap_createBitmap =
      env->GetStaticMethodID(Bitmap_Class.get(), "createBitmap",
                             "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  Bitmap_isRecycled = env->GetMethodID(Bitmap_Class.get(), "isRecycled", "()Z");
  Config_Class.reset(env, env->FindClass("android/graphics/Bitmap$Config"));
  Config_ARGB_888 =
      env->GetStaticFieldID(Config_Class.get(), "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
  Bitmap_wrapHardwareBuffer = env->GetStaticMethodID(
      Bitmap_Class.get(), "wrapHardwareBuffer",
      "(Landroid/hardware/HardwareBuffer;Landroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;");
}

void PAGDecoder::checkBitmap() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return;
  }
  if (bitmap.get() != nullptr && !env->CallBooleanMethod(bitmap.get(), Bitmap_isRecycled)) {
    return;
  }
  auto config = env->GetStaticObjectField(Config_Class.get(), Config_ARGB_888);
  bitmap.reset(env, env->CallStaticObjectMethod(Bitmap_Class.get(), Bitmap_createBitmap, width,
                                                height, config));
}

static bool CreateGLTexture(tgfx::Context *context, int width, int height,
                            tgfx::GLSampler *texture) {
  texture->target = GL_TEXTURE_2D;
  texture->format = tgfx::PixelFormat::RGBA_8888;
  auto gl = tgfx::GLFunctions::Get(context);
  gl->genTextures(1, &texture->id);
  if (texture->id <= 0) {
    return false;
  }
  gl->bindTexture(texture->target, texture->id);
  gl->texParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->texImage2D(texture->target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  gl->bindTexture(texture->target, 0);
  return true;
}

void PAGDecoder::createSurface() {
  device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    return;
  }
  auto context = device->lockContext();
  if (!context) {
    return;
  }
  auto drawable =
      HardwareBufferDrawable::Make(static_cast<int>(width), static_cast<int>(height), device);
  if (drawable) {
    pagSurface = PAGSurface::MakeFrom(drawable);
    auto env = JNIEnvironment::Current();
    auto jHardwareBuffer = tgfx::HardwareBufferInterface::AHardwareBuffer_toHardwareBuffer(
        env, drawable->aHardwareBuffer());
    bitmap.reset(env, env->CallStaticObjectMethod(Bitmap_Class.get(), Bitmap_wrapHardwareBuffer,
                                                  jHardwareBuffer, nullptr));
  } else {
    CreateGLTexture(context, width, height, &textureInfo);
    pagSurface = PAGSurface::MakeFrom(BackendTexture{{textureInfo.id, textureInfo.target}, width, height}, ImageOrigin::BottomLeft);
  }
  device->unlock();
}

std::shared_ptr<PAGDecoder> PAGDecoder::Make(std::shared_ptr<PAGComposition> pagComposition,
                                             int width, int height) {
  if (!pagComposition || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto decoder = std::make_shared<PAGDecoder>();
  decoder->width = width;
  decoder->height = height;
  decoder->pagComposition = pagComposition;
  decoder->createSurface();
  if (!decoder->pagSurface) {
    return nullptr;
  }
  decoder->pagPlayer = std::make_shared<PAGPlayer>();
  decoder->pagPlayer->setComposition(decoder->pagComposition);
  decoder->pagPlayer->setSurface(decoder->pagSurface);
  return decoder;
}

jobject PAGDecoder::decode(double value) {
  pagPlayer->setProgress(value);
  pagPlayer->flush();
  if (tgfx::HardwareBufferInterface::Available()) {
    return bitmap.get();
  }
  auto env = JNIEnvironment::Current();
  void *pixels;
  checkBitmap();
  if (AndroidBitmap_lockPixels(env, bitmap.get(), &pixels) >= 0) {
    pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixels,
                           width * 4);
    AndroidBitmap_unlockPixels(env, bitmap.get());
  }
  return bitmap.get();
}

}  // namespace pag
