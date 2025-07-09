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
#include "HardwareDecoder.h"
#include "NativeDisplayLink.h"

namespace pag {
static std::atomic<NALUType> defaultType = {NALUType::AVCC};

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

NALUType NativePlatform::naluType() const {
  return defaultType;
}

void NativePlatform::setNALUType(NALUType type) const {
  defaultType = type;
}

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
  return {HardwareDecoder::Factory(), VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
}

std::shared_ptr<DisplayLink> NativePlatform::createDisplayLink(
    std::function<void()> callback) const {
  return std::make_shared<NativeDisplayLink>(std::move(callback));
}
}  // namespace pag
