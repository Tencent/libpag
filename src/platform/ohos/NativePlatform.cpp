/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "NativePlatform.h"
#include "platform/ohos/HardwareDecoder.h"


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

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
  return {&hardwareDecoderFactory, VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
}

bool NativePlatform::registerFallbackFonts() const {
  // to do: kevingpqi  
  return false;
}

void NativePlatform::traceImage(const tgfx::ImageInfo& info, const void* pixels,
                                const std::string& tag) const {
  if (info.isEmpty() || pixels || tag.c_str()) {}
    
}

std::string NativePlatform::getCacheDir() const {
  // to do: kevingpqi    
  return "";
}

std::shared_ptr<DisplayLink> NativePlatform::createDisplayLink(
    std::function<void()> callback) const {
   // to do: kevingpqi    
  if (callback) {
     return nullptr;
  }
  return nullptr;
}

}