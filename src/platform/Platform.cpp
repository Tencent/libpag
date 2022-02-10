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

#include "Platform.h"
#include "core/Image.h"
#include "gpu/opengl/GLProcGetter.h"
#include "video/VideoDecoder.h"

namespace pag {
bool Platform::hasHardwareDecoder() const {
  return false;
}

std::unique_ptr<VideoDecoder> Platform::makeHardwareDecoder(const VideoConfig&) const {
  return nullptr;
}

bool Platform::registerFallbackFonts() const {
  return false;
}

NALUType Platform::naluType() const {
  return NALUType::AnnexB;
}

void Platform::traceImage(const ImageInfo&, const void*, const std::string&) const {
}
}  // namespace pag