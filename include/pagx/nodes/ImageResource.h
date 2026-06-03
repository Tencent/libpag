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
#include "pagx/nodes/Resource.h"
#include "pagx/types/Data.h"

namespace pagx {

/**
 * Resolved image resource returned by ResourceLoader. The data is encoded image bytes such as PNG,
 * JPEG, or WebP. Importer writes the data back to the corresponding Image node, so existing
 * LayerBuilder image paths can consume it without knowing whether it came from XML, a file path,
 * or business code.
 */
class ImageResource : public Resource {
 public:
  /**
   * Creates an ImageResource by copying encoded image bytes.
   * @param bytes pointer to encoded image bytes. Must not be null unless length is zero.
   * @param length byte length.
   */
  static std::shared_ptr<ImageResource> FromBytes(const void* bytes, size_t length);

  /**
   * Creates an ImageResource from an existing immutable Data object. Returns nullptr if data is
   * null or empty.
   */
  static std::shared_ptr<ImageResource> FromData(std::shared_ptr<Data> data);

  ResourceType resourceType() const override {
    return ResourceType::Image;
  }

  /**
   * Returns encoded image bytes owned by this resource.
   */
  std::shared_ptr<Data> data() const {
    return imageData;
  }

 private:
  explicit ImageResource(std::shared_ptr<Data> data) : imageData(std::move(data)) {
  }

  std::shared_ptr<Data> imageData = nullptr;
};

}  // namespace pagx
