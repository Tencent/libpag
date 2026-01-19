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

#include "NativePlatform.h"
#include <jni.h>
#include <sys/system_properties.h>
#include <cstdarg>
#include "FontConfigAndroid.h"
#include "HardwareDecoder.h"
#include "JPAGDiskCache.h"
#include "JTraceImage.h"
#include "NativeDisplayLink.h"
#include "PAGText.h"

#define LOG_TAG "libpag"

namespace pag {
class HardwareDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return true;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    auto decoder = new HardwareDecoder(format);
    if (!decoder->isValid) {
      delete decoder;
      return nullptr;
    }
    return std::unique_ptr<VideoDecoder>(decoder);
  }
};

static HardwareDecoderFactory hardwareDecoderFactory = {};

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

void NativePlatform::InitJNI() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  initialized = true;
  JTraceImage::InitJNI(env);
  FontConfigAndroid::InitJNI(env);
  JVideoSurface::InitJNI(env);
  InitPAGTextJNI(env);
  JPAGDiskCache::InitJNI(env);
  NativeDisplayLink::InitJNI(env);
  env->ExceptionClear();
}

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
  return {&hardwareDecoderFactory, VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
}

bool NativePlatform::registerFallbackFonts() const {
  return FontConfigAndroid::RegisterFallbackFonts();
}

void NativePlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                                const std::string& tag) const {
  JTraceImage::Trace(info, pixels, tag);
}

std::string NativePlatform::getCacheDir() const {
  return JPAGDiskCache::GetCacheDir();
}

std::shared_ptr<DisplayLink> NativePlatform::createDisplayLink(
    std::function<void()> callback) const {
  return NativeDisplayLink::Make(std::move(callback));
}

}  // namespace pag
