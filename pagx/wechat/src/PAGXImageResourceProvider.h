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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "pagx/ImageResourceProvider.h"
#include "tgfx/core/Image.h"

namespace pagx {

/**
 * WeChat MiniProgram image resource provider implementing progressive loading with two-tier
 * caching (full-quality + thumbnail). Returns the best available quality when queried by the
 * renderer: full-quality first, thumbnail as fallback, nullptr if neither is available.
 */
class PAGXImageResourceProvider : public ImageResourceProvider {
 public:
  std::shared_ptr<tgfx::Image> resolveImage(const std::string& filePath) override {
    auto fullIt = fullImages.find(filePath);
    if (fullIt != fullImages.end()) {
      return fullIt->second;
    }
    auto thumbIt = thumbnailImages.find(filePath);
    if (thumbIt != thumbnailImages.end()) {
      return thumbIt->second;
    }
    return nullptr;
  }

  bool hasImage(const std::string& filePath) const override {
    return fullImages.count(filePath) > 0 || thumbnailImages.count(filePath) > 0;
  }

  void putFullImage(const std::string& filePath, std::shared_ptr<tgfx::Image> image) {
    fullImages[filePath] = std::move(image);
  }

  void putThumbnailImage(const std::string& filePath, std::shared_ptr<tgfx::Image> image) {
    thumbnailImages[filePath] = std::move(image);
  }

  void clearFullImage(const std::string& filePath) {
    fullImages.erase(filePath);
  }

  void clearThumbnailImage(const std::string& filePath) {
    thumbnailImages.erase(filePath);
  }

  bool hasFullImage(const std::string& filePath) const {
    return fullImages.count(filePath) > 0;
  }

  bool hasThumbnailImage(const std::string& filePath) const {
    return thumbnailImages.count(filePath) > 0;
  }

  void clear() {
    fullImages.clear();
    thumbnailImages.clear();
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<tgfx::Image>> fullImages;
  std::unordered_map<std::string, std::shared_ptr<tgfx::Image>> thumbnailImages;
};

}  // namespace pagx
