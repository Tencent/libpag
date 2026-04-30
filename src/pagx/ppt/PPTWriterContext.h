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
#include <vector>
#include "pagx/utils/ExporterUtils.h"
#include "tgfx/core/Data.h"

namespace pagx {

class Image;

struct ImageEntry {
  const Image* image = nullptr;
  std::shared_ptr<tgfx::Data> cachedData = nullptr;
  std::string relId;
  std::string mediaPath;
  bool isJPEG = false;
};

class PPTWriterContext {
 public:
  int nextShapeId() {
    return _shapeId++;
  }

  std::string addImage(const Image* image) {
    auto it = _imageMap.find(image);
    if (it != _imageMap.end()) {
      return it->second;
    }
    auto data = GetImageData(image);
    if (!data || data->size() == 0) {
      return {};
    }
    bool jpeg = IsJPEG(data->bytes(), data->size());
    if (!jpeg && IsWebP(data->bytes(), data->size())) {
      data = ConvertWebPToPNG(data);
      if (!data) {
        return {};
      }
    }
    auto relId = registerImage(image, std::move(data), jpeg);
    _imageMap.emplace(image, relId);
    return relId;
  }

  std::string addRawImage(std::shared_ptr<tgfx::Data> pngData) {
    return registerImage(nullptr, std::move(pngData), /*jpeg=*/false);
  }

  const std::vector<ImageEntry>& images() const {
    return _images;
  }

  bool hasJPEG() const {
    return _hasJPEG;
  }

  bool hasPNG() const {
    return _hasPNG;
  }

 private:
  // Allocates a new media entry (rId + mediaPath) for `data` and records the
  // corresponding content-type bucket. Shared by addImage (PAGX-backed entries,
  // the `image` pointer is kept so the caller's _imageMap can deduplicate) and
  // addRawImage (pre-encoded PNG blobs with no source Image, e.g. layer bakes /
  // tiled-pattern bakes).
  std::string registerImage(const Image* image, std::shared_ptr<tgfx::Data> data, bool jpeg) {
    int idx = static_cast<int>(_images.size()) + 1;
    std::string relId = "rId" + std::to_string(_nextRelId++);
    const char* ext = jpeg ? "jpeg" : "png";
    std::string mediaPath = "ppt/media/image" + std::to_string(idx) + "." + ext;
    _images.push_back({image, std::move(data), relId, mediaPath, jpeg});
    (jpeg ? _hasJPEG : _hasPNG) = true;
    return relId;
  }

  // Shape ID 1 is reserved for the slide's root group (p:grpSpPr).
  int _shapeId = 2;
  // Relationship ID rId1 is reserved for the slideLayout reference.
  int _nextRelId = 2;
  bool _hasJPEG = false;
  bool _hasPNG = false;
  std::vector<ImageEntry> _images;
  std::unordered_map<const Image*, std::string> _imageMap;
};

}  // namespace pagx
