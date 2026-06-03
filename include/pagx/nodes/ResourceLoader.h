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
#include "pagx/nodes/Resource.h"

namespace pagx {

/**
 * Describes one resource load request made during PAGXDocument import. The loader receives the
 * resource type, XML id, original source string, and custom data (`data-*` attributes without the
 * prefix) so business code can decide whether to handle the resource.
 */
struct ResourceLoadRequest {
  ResourceType type = ResourceType::Image;
  std::string id = {};
  std::string source = {};
  std::unordered_map<std::string, std::string> customData = {};
};

/**
 * Per-document-load resource loader. It mirrors Rive's FileAssetLoader model: loader is passed to
 * the import call, is not global, and can return nullptr to skip a request and let the importer
 * continue with its built-in fallback path.
 */
class ResourceLoader {
 public:
  virtual ~ResourceLoader() = default;

  /**
   * Attempts to load the requested resource. Return nullptr to indicate that this loader chooses
   * not to handle the request; the importer will then continue its internal loading logic.
   */
  virtual std::shared_ptr<Resource> load(const ResourceLoadRequest& request) = 0;
};

}  // namespace pagx
