/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include "pagx/types/Data.h"

namespace tgfx {
class Image;
}

namespace pagx {

class LayerBuilder;

/**
 * PAGImage wraps a decoded image with its source path or data URI. It is the value type carried by
 * image-valued ViewModel properties and DataBind channels, so that writers receive a ready-to-
 * render image object instead of a raw path string that would need per-frame decoding.
 */
class PAGImage {
 public:
  /**
   * Creates a PAGImage from a file path or a data URI (e.g. "data:image/png;base64,...").
   * @param path a file path or data URI.
   * @return a PAGImage, or nullptr if the image could not be decoded.
   */
  static std::shared_ptr<PAGImage> MakeFromPath(const std::string& path);

  /**
   * Creates a PAGImage from raw encoded image data (PNG, JPEG, WebP, etc.).
   * @param data the encoded image bytes.
   * @return a PAGImage, or nullptr if the image could not be decoded.
   */
  static std::shared_ptr<PAGImage> MakeFromData(const std::shared_ptr<Data>& data);

  /**
   * Returns the source string (file path or data URI) this PAGImage was created from, or an empty
   * string if created from raw data.
   */
  const std::string& source() const;

 private:
  PAGImage(std::shared_ptr<tgfx::Image> image, std::string source);
  std::shared_ptr<tgfx::Image> _tgfxImage = nullptr;
  std::string _source = {};

  friend class LayerBuilder;
};

}  // namespace pagx
