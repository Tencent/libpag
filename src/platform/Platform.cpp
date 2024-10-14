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
#include "rendering/video/VideoDecoder.h"

namespace pag {
std::vector<const VideoDecoderFactory*> Platform::getVideoDecoderFactories() const {
  return {VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
}

bool Platform::registerFallbackFonts() const {
  return false;
}

NALUType Platform::naluType() const {
  return NALUType::AnnexB;
}

void Platform::traceImage(const tgfx::ImageInfo&, const void*, const std::string&) const {
}

std::optional<PositionedGlyphs> Platform::shapeText(const std::string&,
                                                    const std::shared_ptr<tgfx::Typeface>&) const {
  return std::nullopt;
}

std::string Platform::getCacheDir() const {
  return "";
}

std::string Platform::getSandboxPath(std::string filePath) const {
  return filePath;
}

std::shared_ptr<DisplayLink> Platform::createDisplayLink(std::function<void()>) const {
  return nullptr;
}
}  // namespace pag
