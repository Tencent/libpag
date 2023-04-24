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

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "codec/NALUType.h"
#include "rendering/utils/shaper/PositionedGlyphs.h"
#include "rendering/video/VideoDecoderFactory.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Typeface.h"

namespace pag {
/**
 * Defines methods for native platforms to implement.
 */
class Platform {
 public:
  /**
   * Returns the instance of the current platform.
   */
  static const Platform* Current();

  virtual ~Platform() = default;

  /**
   * Returns the VideoDecoderFactory list of the current platform. The first one has the highest
   * priority and then fallback to other factories.
   */
  virtual std::vector<const VideoDecoderFactory*> getVideoDecoderFactories() const;

  /**
   * Implement this method to register the default fallback font list. User should call
   * PAGFont::SetFallbackFontPaths() or PAGFont::SetFallbackFontNames() manually in host app if this
   * method is not implemented on the current platform.
   */
  virtual bool registerFallbackFonts() const;

  /**
   * Returns the default NALU start code type of the current platform.
   */
  virtual NALUType naluType() const;

  /**
   * Provides a utility to view the PixelMap data.
   */
  virtual void traceImage(const tgfx::ImageInfo& info, const void* pixels,
                          const std::string& tag) const;

  /**
   * Returns the absolute path to the platform-specific cache directory on the filesystem.
   */
  virtual std::string getCacheDir() const;

  virtual std::optional<PositionedGlyphs> shapeText(const std::string&,
                                                    const std::shared_ptr<tgfx::Typeface>&) const {
    return std::nullopt;
  }

  /**
   * Get the relative path of the file, which contains variable factors in the iOS platform.
   */
  virtual std::string getRelativePathFrom(std::string filePath) const {
    return filePath;
  }
};
}  // namespace pag
