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
#include <string>
#include <unordered_map>
#include "codec/NALUType.h"

namespace pag {
class PAGFont;
class GLProcGetter;
class Image;
class PixelBuffer;
class PixelMap;
struct VideoConfig;
class VideoDecoder;
class Data;

/**
 * Defines methods for native platforms to implement.
 */
class Platform {
 public:
  /**
   * Returns the instance of current platform.
   */
  static const Platform* Current();

  virtual ~Platform() = default;

  /**
   * Returns true if hardware video decoders are supported on current platform.
   */
  virtual bool hasHardwareDecoder() const;

  /**
   * Creates a hardware backed VideoDecoder with the specified video config. Returns nullptr if
   * current platform has no hardware decoder support.
   */
  virtual std::unique_ptr<VideoDecoder> makeHardwareDecoder(const VideoConfig& config) const;

  /**
   * Creates a hardware backed PixelBuffer with specified width and height. Returns nullptr if
   * current platform has no hardware buffer support. Hardware buffer is a low-level object
   * representing a memory buffer accessible by various hardware units. Hardware buffer allows
   * sharing buffers across CPU and GPU, which can be used to speed up the texture uploading.
   */
  virtual std::shared_ptr<PixelBuffer> makeHardwareBuffer(int width, int height,
                                                          bool alphaOnly) const;

  /**
   * If the file path represents an encoded image that current platform knows how to decode, returns
   * an Image that can decode it. Otherwise returns nullptr.
   */
  virtual std::shared_ptr<Image> makeImage(const std::string& filePath) const;

  /**
   * If the file bytes represents an encoded image that current platform knows how to decode,
   * returns an Image that can decode it. Otherwise returns nullptr.
   */
  virtual std::shared_ptr<Image> makeImage(std::shared_ptr<Data> imageBytes) const;

  /**
   * Parses the font family and style from specified file path. Returns a font data with empty
   * family and style names if the file path is not a valid font.
   */
  virtual PAGFont parseFont(const std::string& fontPath, int ttcIndex) const;

  /**
   * Parses the font family and style from specified file bytes. Returns a font data with empty
   * family and style names if the file bytes is not a valid font.
   */
  virtual PAGFont parseFont(const void* data, size_t length, int ttcIndex) const;

  /**
   * Writes the string pointed by format to the standard output (stdout).
   */
  virtual void printLog(const char format[], ...) const;

  /**
   * Writes the string pointed by format to the standard error (stderr).
   */
  virtual void printError(const char format[], ...) const;

  /**
   * Implement this method to register the default fallback font list. User should call
   * PAGFont::SetFallbackFontPaths() or PAGFont::SetFallbackFontNames() manually in host app if this
   * method is not implemented on current platform.
   */
  virtual bool registerFallbackFonts() const;

  /**
   * Reports the statistical data of pag.
   */
  virtual void reportStatisticalData(std::unordered_map<std::string, std::string>& reportMap) const;

  /**
   * Returns the default NALU start code type of the current platform.
   */
  virtual NALUType naluType() const;

  /**
   * Provides a utility to view the PixelMap data.
   */
  virtual void traceImage(const PixelMap& pixelMap, const std::string& tag) const;
};
}  // namespace pag