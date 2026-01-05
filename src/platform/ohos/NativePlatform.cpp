/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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
#include "platform/ohos/NativeDisplayLink.h"
#include "platform/ohos/OHOSSoftwareDecoderWrapper.h"

namespace pag {
class HardwareDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return true;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    return OHOSVideoDecoder::MakeHardwareDecoder(format);
  }
};

class OHOSSoftwareDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    auto decoder = OHOSVideoDecoder::MakeSoftwareDecoder(format);
    return OHOSSoftwareDecoderWrapper::Wrap(decoder);
  }
};

static HardwareDecoderFactory hardwareDecoderFactory = {};
static OHOSSoftwareDecoderFactory softwareDecoderFactory = {};

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
  return {&hardwareDecoderFactory, &softwareDecoderFactory,
          VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
}

bool NativePlatform::registerFallbackFonts() const {
  // Since it is not possible to call ArkTs code from C++, the registration of system fonts on the
  // HarmonyOS platform is handled at the ArkTs code level.
  return false;
}

void NativePlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                                const std::string& tag) const {
  // todo: kevingpqi
  if (info.isEmpty() || pixels || tag.c_str()) {
  }
}

std::string NativePlatform::getCacheDir() const {
  return _cacheDir;
}

std::shared_ptr<DisplayLink> NativePlatform::createDisplayLink(
    std::function<void()> callback) const {
  if (!callback) {
    return nullptr;
  }
  return std::make_shared<NativeDisplayLink>(callback);
}

void NativePlatform::setCacheDir(const std::string& cacheDir) {
  _cacheDir = cacheDir;
}

}  // namespace pag
