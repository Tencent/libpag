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

#include "JPAGDecoder.h"
#include <android/bitmap.h>
#include "JNIHelper.h"
#include "JPAGLayerHandle.h"
#include "NativePlatform.h"
#include "base/utils/TGFXCast.h"
#include "rendering/layers/ContentVersion.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/platform/android/AndroidBitmap.h"
#include "tgfx/platform/android/HardwareBufferJNI.h"

namespace pag {
static jfieldID PAGDecoder_nativeContext;
}

using namespace pag;

std::shared_ptr<PAGDecoder> getPAGDecoder(JNIEnv* env, jobject thiz) {
  auto decoder = reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, PAGDecoder_nativeContext));
  if (decoder == nullptr) {
    return nullptr;
  }
  return decoder->get();
}

extern "C" {

PAG_API void Java_org_libpag_PAGDecoder_nativeInit(JNIEnv* env, jclass clazz) {
  PAGDecoder_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_PAGDecoder_nativeRelease(JNIEnv* env, jobject thiz) {
  auto decoder = reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, PAGDecoder_nativeContext));
  if (decoder != nullptr) {
    decoder->clear();
  }
}

PAG_API void Java_org_libpag_PAGDecoder_nativeFinalize(JNIEnv* env, jobject thiz) {
  auto old = reinterpret_cast<JPAGDecoder*>(env->GetLongField(thiz, PAGDecoder_nativeContext));
  delete old;
  env->SetLongField(thiz, PAGDecoder_nativeContext, (jlong) nullptr);
}

PAG_API jlong JNICALL Java_org_libpag_PAGDecoder_MakeFrom(JNIEnv* env, jclass,
                                                          jobject newComposition,
                                                          jfloat maxFrameRate, jfloat scale) {
  auto composition = ToPAGCompositionNativeObject(env, newComposition);
  auto decoder = PAGDecoder::MakeFrom(std::move(composition), maxFrameRate, scale);
  if (decoder == nullptr) {
    LOGE("PAGDecoder::MakeFrom() Failed to create a PAGDecoder!");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGDecoder(decoder));
}

PAG_API jint Java_org_libpag_PAGDecoder_width(JNIEnv* env, jobject thiz) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->width();
}

PAG_API jint Java_org_libpag_PAGDecoder_height(JNIEnv* env, jobject thiz) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->height();
}

PAG_API jint Java_org_libpag_PAGDecoder_numFrames(JNIEnv* env, jobject thiz) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->numFrames();
}

PAG_API jfloat Java_org_libpag_PAGDecoder_frameRate(JNIEnv* env, jobject thiz) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->frameRate();
}

PAG_API jboolean Java_org_libpag_PAGDecoder_checkFrameChanged(JNIEnv* env, jobject thiz,
                                                              jint index) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->checkFrameChanged(index);
}

PAG_API jboolean Java_org_libpag_PAGDecoder_copyFrameTo(JNIEnv* env, jobject thiz,
                                                        jobject bitmapObject, jint index) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return JNI_FALSE;
  }
  auto hardwareBuffer = tgfx::AndroidBitmap::GetHardwareBuffer(env, bitmapObject);
  if (hardwareBuffer != nullptr) {
    return decoder->readFrame(index, hardwareBuffer);
  }
  auto info = tgfx::AndroidBitmap::GetInfo(env, bitmapObject);
  if (info.isEmpty()) {
    LOGE("PAGDecoder::copyFrameTo() Failed to read ImageInfo from the Java Bitmap!");
    return JNI_FALSE;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmapObject, &pixels) != 0) {
    env->ExceptionClear();
    LOGE("PAGDecoder::copyFrameTo() Failed to lock Pixels from the Java Bitmap!");
    return JNI_FALSE;
  }
  auto success = decoder->readFrame(index, pixels, info.rowBytes(), ToPAG(info.colorType()),
                                    ToPAG(info.alphaType()));
  AndroidBitmap_unlockPixels(env, bitmapObject);
  return success;
}

PAG_API jboolean Java_org_libpag_PAGDecoder_readFrame(JNIEnv* env, jobject thiz, jint index,
                                                      jobject hardwareBufferObject) {
  auto decoder = getPAGDecoder(env, thiz);
  if (decoder == nullptr) {
    return JNI_FALSE;
  }
  auto hardwareBuffer = tgfx::HardwareBufferFromJavaObject(env, hardwareBufferObject);
  if (hardwareBuffer == nullptr) {
    LOGE("PAGDecoder::readFrameTo() Invalid hardwareBuffer specified!");
    return JNI_FALSE;
  }
  return decoder->readFrame(index, hardwareBuffer);
}
}
