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

#include "VideoDecoderFactory.h"
#include "SoftAVCDecoder.h"
#include "SoftwareDecoderWrapper.h"
#include "base/utils/USE.h"
#include "pag/pag.h"

namespace pag {
static std::mutex factoryLocker = {};
static SoftwareDecoderFactory* softwareDecoderFactory = {nullptr};

void PAGVideoDecoder::RegisterSoftwareDecoderFactory(SoftwareDecoderFactory* decoderFactory) {
  std::lock_guard<std::mutex> autoLock(factoryLocker);
  softwareDecoderFactory = decoderFactory;
}

class ExternalDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    std::lock_guard<std::mutex> autoLock(factoryLocker);
    if (softwareDecoderFactory == nullptr) {
      return nullptr;
    }
    return SoftwareDecoderWrapper::Wrap(softwareDecoderFactory->createSoftwareDecoder(), format);
  }
};

class SoftwareAVCDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return false;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    std::unique_ptr<VideoDecoder> videoDecoder = nullptr;
#ifdef PAG_USE_LIBAVC
    videoDecoder = SoftwareDecoderWrapper::Wrap(std::make_shared<SoftAVCDecoder>(), format);
    if (videoDecoder != nullptr) {
      LOGI("All other video decoders are not available, fallback to SoftAVCDecoder!");
    }
#else
    USE(format);
#endif
    return videoDecoder;
  }
};

static ExternalDecoderFactory externalDecoderFactory = {};
static SoftwareAVCDecoderFactory softwareAVCDecoderFactory = {};

const VideoDecoderFactory* VideoDecoderFactory::ExternalDecoderFactory() {
  return &externalDecoderFactory;
}

const VideoDecoderFactory* VideoDecoderFactory::SoftwareAVCDecoderFactory() {
  return &softwareAVCDecoderFactory;
}

bool VideoDecoderFactory::HasExternalSoftwareDecoder() {
  std::lock_guard<std::mutex> autoLock(factoryLocker);
  return softwareDecoderFactory != nullptr;
}

std::unique_ptr<VideoDecoder> VideoDecoderFactory::createDecoder(const VideoFormat& format) const {
  if (isHardwareBacked() && !VideoDecoder::HardwareDecoderCountAvailable()) {
    return nullptr;
  }
  auto decoder = onCreateDecoder(format);
  if (decoder != nullptr) {
    decoder->hardwareBacked = isHardwareBacked();
  }
  return decoder;
}

}  // namespace pag
