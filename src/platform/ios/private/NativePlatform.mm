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
#include "GPUDecoder.h"
#include "base/utils/USE.h"

namespace pag {
const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

std::unique_ptr<VideoDecoder> NativePlatform::makeHardwareDecoder(const VideoConfig& config) const {
#if !TARGET_IPHONE_SIMULATOR
  auto decoder = new GPUDecoder(config);
  if (!decoder->isInitialized) {
    delete decoder;
    return nullptr;
  }
  return std::unique_ptr<VideoDecoder>(decoder);
#else
  USE(config);
  return nullptr;
#endif
}

void NativePlatform::reportStatisticalData(
    std::unordered_map<std::string, std::string>&) const {
}
}
