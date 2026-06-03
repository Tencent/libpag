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

#include <cstdint>

namespace pagx {

/**
 * ResourceType identifies the document-load resource kind requested from a ResourceLoader.
 * Resource is a data/load-time concept, not a runtime object, so it intentionally has no PAG
 * prefix. Runtime objects remain PAGFile/PAGTimeline/PAGSurface.
 */
enum class ResourceType : uint8_t {
  Image = 0,
  Font = 1,
  Composition = 2,
};

/**
 * Base class for resources returned by ResourceLoader during PAGXDocument import.
 *
 * A Resource represents resolved document data (for example encoded image bytes, font bytes, or
 * PAGX XML bytes for a composition). Returning nullptr from ResourceLoader means the loader chose
 * not to handle the request, and the importer will continue with the built-in fallback path.
 */
class Resource {
 public:
  virtual ~Resource() = default;

  /**
   * Returns the concrete resource kind. Importer uses this value to validate that a loader returned
   * a resource matching the request type, without relying on RTTI.
   */
  virtual ResourceType resourceType() const = 0;
};

}  // namespace pagx
