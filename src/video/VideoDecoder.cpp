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

#include "VideoDecoder.h"
#include <atomic>
#include <mutex>
#include "SoftAVCDecoder.h"
#include "SoftwareDecoderWrapper.h"
#include "pag/pag.h"
#include "platform/Platform.h"

namespace pag {
static std::atomic<SoftwareDecoderFactory*> softwareDecoderFactory = {nullptr};
static std::atomic_int maxHardwareDecoderCount = {65535};
static std::atomic_int globalGPUDecoderCount = {0};

void PAGVideoDecoder::SetMaxHardwareDecoderCount(int count) {
  maxHardwareDecoderCount = count;
}

void PAGVideoDecoder::RegisterSoftwareDecoderFactory(SoftwareDecoderFactory* decoderFactory) {
  softwareDecoderFactory = decoderFactory;
}

bool VideoDecoder::HasHardwareDecoder() {
  return Platform::Current()->hasHardwareDecoder();
}

int VideoDecoder::GetMaxHardwareDecoderCount() {
  return maxHardwareDecoderCount;
}

bool VideoDecoder::HasSoftwareDecoder() {
#ifdef PAG_USE_LIBAVC
  return true;
#else
  return softwareDecoderFactory != nullptr;
#endif
}

bool VideoDecoder::HasExternalSoftwareDecoder() {
  return softwareDecoderFactory != nullptr;
}

SoftwareDecoderFactory* VideoDecoder::GetExternalSoftwareDecoderFactory() {
  return softwareDecoderFactory;
}

std::unique_ptr<VideoDecoder> VideoDecoder::CreateHardwareDecoder(const VideoConfig& config) {
  std::unique_ptr<VideoDecoder> videoDecoder = nullptr;
  if (globalGPUDecoderCount < VideoDecoder::GetMaxHardwareDecoderCount()) {
    videoDecoder = Platform::Current()->makeHardwareDecoder(config);
  }
  if (videoDecoder != nullptr) {
    globalGPUDecoderCount++;
    videoDecoder->hardwareBacked = true;
  }
  return videoDecoder;
}

std::unique_ptr<VideoDecoder> VideoDecoder::CreateSoftwareDecoder(const VideoConfig& config) {
  std::unique_ptr<VideoDecoder> videoDecoder = nullptr;
  SoftwareDecoderFactory* factory = softwareDecoderFactory;
  if (factory != nullptr) {
    videoDecoder = SoftwareDecoderWrapper::Wrap(factory->createSoftwareDecoder(), config);
  }

#ifdef PAG_USE_LIBAVC
  if (videoDecoder == nullptr) {
    videoDecoder = SoftwareDecoderWrapper::Wrap(std::make_unique<SoftAVCDecoder>(), config);
    if (videoDecoder != nullptr) {
      LOGI("All other video decoders are not available, fallback to SoftAVCDecoder!");
    }
  }
#endif

  if (videoDecoder != nullptr) {
    videoDecoder->hardwareBacked = false;
  } else {
    LOGE("Failed on creating a software video decoder!");
  }
  return videoDecoder;
}

std::unique_ptr<VideoDecoder> VideoDecoder::Make(const VideoConfig& config, bool useHardware) {
  return useHardware ? CreateHardwareDecoder(config) : CreateSoftwareDecoder(config);
}

VideoDecoder::~VideoDecoder() {
  if (hardwareBacked) {
    globalGPUDecoderCount--;
  }
}

}  // namespace pag