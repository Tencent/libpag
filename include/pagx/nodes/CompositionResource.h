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
 * Resolved composition resource returned by ResourceLoader. The data is PAGX XML bytes. Importer
 * parses the bytes into an internal PAGXDocument and attaches it through the existing sealed
 * externalDoc Composition wrapper, keeping the external document's internal IDs out of the host
 * document's nodeMap.
 */
class CompositionResource : public Resource {
 public:
  /**
   * Creates a CompositionResource by copying PAGX XML bytes.
   * @param bytes pointer to PAGX XML bytes. Must not be null unless length is zero.
   * @param length byte length.
   */
  static std::shared_ptr<CompositionResource> FromBytes(const void* bytes, size_t length);

  /**
   * Creates a CompositionResource from an existing immutable Data object.
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

 private:
  explicit CompositionResource(std::shared_ptr<Data> data) : compositionData(std::move(data)) {
  }

  std::shared_ptr<Data> compositionData = nullptr;
};

}  // namespace pagx
