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
  /**
   * The kind of resource being requested.
   */
  ResourceType type = ResourceType::Image;

  /**
   * The XML id of the node requesting the resource. May be empty if the source node has no id.
   */
  std::string id = {};

  /**
   * The original source string from the XML, such as an image path, font source, or PAGX path.
   */
  std::string source = {};

  /**
   * Custom `data-*` attributes from the source node, stored without the `data-` prefix.
   */
  std::unordered_map<std::string, std::string> customData = {};
};

/**
 * Per-document-load resource loader. It mirrors Rive's FileAssetLoader model: the importer creates
 * a retained Resource placeholder and passes it to the loader. Return true after filling or taking
 * over the resource; the importer will not run its built-in fallback. Return false to let the
 * importer continue with its normal fallback path.
 *
 * Returning true does not require the resource to be fully loaded immediately. A loader may retain
 * the shared Resource and call its setter methods later; importer-owned ResourceReferencer nodes
 * will consume the update when data arrives.
 */
class ResourceLoader {
 public:
  virtual ~ResourceLoader() = default;

  /**
   * Attempts to handle or take over a resource request.
   * @param request metadata describing the source resource.
   * @param resource retained Resource placeholder created by the importer. The concrete type matches
   *                 request.type and exposes type-specific setter methods.
   * @return true if the loader handles this resource, false to use importer fallback.
   */
  virtual bool load(const ResourceLoadRequest& request, std::shared_ptr<Resource> resource) {
    (void)request;
    (void)resource;
    return false;
  }
};

}  // namespace pagx
