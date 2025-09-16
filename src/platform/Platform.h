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

#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include "codec/NALUType.h"
#include "rendering/utils/DisplayLink.h"
#include "rendering/utils/shaper/ShapedGlyph.h"
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

  /**
    * Returns the shaped glyphs of the given text and typeface.
    */
  virtual std::vector<ShapedGlyph> shapeText(const std::string& text,
                                             std::shared_ptr<tgfx::Typeface> typeface) const;

  /**
   * Returns the corresponding sandbox path from the absolute file path, which usually starts with
   * "app://" or "home://". Returns the original path if the platform does not support sandbox.
   */
  virtual std::string getSandboxPath(std::string filePath) const;

  /**
   * Creates a display link with the given callback. Returns nullptr if the current platform does
   * not have display link support.
   */
  virtual std::shared_ptr<DisplayLink> createDisplayLink(std::function<void()> callback) const;
};
}  // namespace pag
