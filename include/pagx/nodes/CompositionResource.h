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
 * CompositionResource stores PAGX XML bytes supplied by ResourceLoader. Importer-owned Layer nodes
 * parse the bytes into external PAGXDocument wrappers when the resource is set or updated.
 */
class CompositionResource : public Resource {
 public:
  /**
   * Creates an empty composition resource for delayed loading.
   */
  static std::shared_ptr<CompositionResource> MakeEmpty();

  /**
   * Creates a CompositionResource by copying PAGX XML bytes.
   * @param bytes pointer to PAGX XML bytes.
   * @param length byte length.
   */
  static std::shared_ptr<CompositionResource> FromBytes(const void* bytes, size_t length);

  /**
   * Creates a CompositionResource from an existing Data object.
   * @param data PAGX XML bytes.
   */
  static std::shared_ptr<CompositionResource> FromData(std::shared_ptr<Data> data);

  ResourceType resourceType() const override {
    return ResourceType::Composition;
  }

  /**
   * Returns PAGX XML bytes.
   */
  std::shared_ptr<Data> data() const {
    return compositionData;
  }

  /**
   * Sets PAGX XML bytes and notifies all referencers.
   * @param data PAGX XML bytes.
   * @return true if data is non-null and non-empty.
   */
  bool setData(std::shared_ptr<Data> data);

  /**
   * Copies PAGX XML bytes into this resource and notifies all referencers.
   * @param bytes pointer to PAGX XML bytes.
   * @param length byte length.
   * @return true if bytes were copied.
   */
  bool setBytes(const void* bytes, size_t length);

 private:
  CompositionResource() = default;
  explicit CompositionResource(std::shared_ptr<Data> data) : compositionData(std::move(data)) {
  }

  std::shared_ptr<Data> compositionData = nullptr;
};

}  // namespace pagx
