/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"

namespace pagx {

/**
 * PAGXDocument is the root container for a PAGX document.
 * It contains resources and layers. This is a pure data structure class.
 * Use PAGXImporter to load documents and PAGXExporter to save documents.
 */
class PAGXDocument {
 public:
  /**
   * Creates an empty document with the specified size.
   */
  static std::shared_ptr<PAGXDocument> Make(float width, float height);

  /**
   * Format version.
   */
  std::string version = "1.0";

  /**
   * Canvas width.
   */
  float width = 0;

  /**
   * Canvas height.
   */
  float height = 0;

  /**
   * Resources (images, compositions, color sources, etc.).
   * These can be referenced by "@id" in the document.
   */
  std::vector<std::unique_ptr<Node>> resources = {};

  /**
   * Top-level layers.
   */
  std::vector<std::unique_ptr<Layer>> layers = {};

  /**
   * Base path for resolving relative resource paths.
   */
  std::string basePath = {};

  /**
   * Finds a resource by ID.
   * Returns nullptr if not found.
   */
  Node* findResource(const std::string& id);

  /**
   * Finds a layer by ID (searches recursively).
   * Returns nullptr if not found.
   */
  Layer* findLayer(const std::string& id) const;

 private:
  std::unordered_map<std::string, Node*> resourceMap = {};
  bool resourceMapDirty = true;

  void rebuildResourceMap();
  static Layer* findLayerRecursive(const std::vector<std::unique_ptr<Layer>>& layers,
                                   const std::string& id);
};

}  // namespace pagx
