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
#include <utility>
#include "pagx/nodes/Resource.h"
#include "pagx/types/Data.h"

namespace pagx {

/**
 * ImageResource stores encoded image bytes supplied by ResourceLoader. A loader may create an empty
 * resource and call setData() or setBytes() later; all registered ResourceReferencer objects are
 * notified when data is set.
 */
class ImageResource : public Resource {
 public:
  /**
   * Creates an empty image resource for delayed loading.
   */
  static std::shared_ptr<ImageResource> MakeEmpty();

  /**
   * Creates an ImageResource by copying encoded image bytes.
   * @param bytes pointer to encoded image bytes.
   * @param length byte length.
   */
  static std::shared_ptr<ImageResource> FromBytes(const void* bytes, size_t length);

  /**
   * Creates an ImageResource from an existing Data object. Returns nullptr if data is null or empty.
   * @param data encoded image bytes.
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

  /**
   * Sets encoded image bytes and notifies all referencers.
   * @param data encoded image bytes.
   * @return true if data is non-null and non-empty.
   */
  bool setData(std::shared_ptr<Data> data);

  /**
   * Copies encoded image bytes into this resource and notifies all referencers.
   * @param bytes pointer to encoded image bytes.
   * @param length byte length.
   * @return true if bytes were copied.
   */
  bool setBytes(const void* bytes, size_t length);

 private:
  ImageResource() = default;
  explicit ImageResource(std::shared_ptr<Data> data) : imageData(std::move(data)) {
  }

  std::shared_ptr<Data> imageData = nullptr;
};

}  // namespace pagx
