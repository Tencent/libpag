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

#include "NativePlatform.h"
#include <android/log.h>
#include <jni.h>
#include <sys/system_properties.h>
#include <cstdarg>
#include "FontConfigAndroid.h"
#include "GPUDecoder.h"
#include "Global.h"
#include "JNIEnvironment.h"
#include "JTraceImage.h"
#include "NativeGraphicBuffer.h"
#include "NativeHardwareBuffer.h"
#include "NativeImage.h"
#include "VideoSurface.h"

#define LOG_TAG "libpag"

namespace pag {

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

void NativePlatform::InitJNI(JNIEnv* env) {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  JTraceImage::InitJNI(env);
  NativeImage::InitJNI(env);
  FontConfigAndroid::InitJNI(env);
  GPUDecoder::InitJNI(env, "org/libpag/GPUDecoder");
  VideoSurface::InitJNI(env, "org/libpag/VideoSurface");
}

std::unique_ptr<VideoDecoder> NativePlatform::makeHardwareDecoder(
    const pag::VideoConfig& config) const {
  auto decoder = new GPUDecoder(config);
  if (!decoder->isValid()) {
    delete decoder;
    return nullptr;
  }
  return std::unique_ptr<VideoDecoder>(decoder);
}

std::shared_ptr<Image> NativePlatform::makeImage(const std::string& filePath) const {
  return NativeImage::MakeFrom(filePath);
}

std::shared_ptr<Image> NativePlatform::makeImage(std::shared_ptr<Data> imageBytes) const {
  return NativeImage::MakeFrom(imageBytes);
}

PAGFont NativePlatform::parseFont(const std::string& fontPath, int ttcIndex) const {
  return FontConfigAndroid::Parse(fontPath, ttcIndex);
}

PAGFont NativePlatform::parseFont(const void* data, size_t length, int ttcIndex) const {
  return FontConfigAndroid::Parse(data, length, ttcIndex);
}

void NativePlatform::printLog(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, args);
  va_end(args);
}

void NativePlatform::printError(const char* format, ...) const {
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, args);
  va_end(args);
}

bool NativePlatform::registerFallbackFonts() const {
  return FontConfigAndroid::RegisterFallbackFonts();
}

void NativePlatform::traceImage(const pag::PixelMap& pixelMap, const std::string& tag) const {
  JTraceImage::Trace(pixelMap, tag);
}

static int GetSDKVersion() {
  char sdk[PROP_VALUE_MAX] = "0";
  __system_property_get("ro.build.version.sdk", sdk);
  return atoi(sdk);
}

std::shared_ptr<PixelBuffer> NativePlatform::makeHardwareBuffer(int width, int height,
                                                                bool alphaOnly) const {
  static auto version = GetSDKVersion();
  if (version >= 26) {
    return NativeHardwareBuffer::Make(width, height, alphaOnly);
  } else if (version <= 23) {
    return NativeGraphicBuffer::Make(width, height, alphaOnly);
  }
  return nullptr;
}

}  // namespace pag