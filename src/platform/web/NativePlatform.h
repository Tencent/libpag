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

#pragma once

#include "platform/Platform.h"

namespace pag {
class NativePlatform : public Platform {
 public:
  void traceImage(const tgfx::ImageInfo& info, const void* pixels,
                  const std::string& tag) const override;

  std::optional<PositionedGlyphs> shapeText(
      const std::string& text, const std::shared_ptr<tgfx::Typeface>& typeface) const override;

  std::unique_ptr<VideoDecoder> makeHardwareDecoder(const VideoFormat& format) const override;
};
}  // namespace pag
