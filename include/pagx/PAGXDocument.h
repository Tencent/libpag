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

#include <cstdio>
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
   * Top-level layers (raw pointers, owned by nodes).
   */
  std::vector<Layer*> layers = {};

  /**
   * Creates a node of the specified type and adds it to the document management.
   * If an ID is provided, the node will be indexed for lookup.
   * If the ID already exists, an error will be logged and the new node will replace the old one in
   * the index.
   */
  template <typename T>
  T* makeNode(const std::string& id = "") {
    auto node = std::unique_ptr<T>(new T());
    auto* result = node.get();
    if (!id.empty()) {
      auto it = nodeMap.find(id);
      if (it != nodeMap.end()) {
        fprintf(stderr, "PAGXDocument::makeNode(): Duplicate node id '%s', overwriting.\n",
                id.c_str());
      }
      result->id = id;
      nodeMap[id] = result;
    }
    nodes.push_back(std::move(node));
    return result;
  }

  /**
   * Finds a node by ID.
   * Returns nullptr if not found.
   */
  Node* findNode(const std::string& id) const;

  /**
   * Finds a node of the specified type by ID.
   * Returns nullptr if not found.
   */
  template <typename T>
  T* findNode(const std::string& id) const {
    return static_cast<T*>(findNode(id));
  }

  /**
   * All nodes in the document (owned by the document).
   */
  std::vector<std::unique_ptr<Node>> nodes = {};

 private:
  PAGXDocument() = default;

  std::unordered_map<std::string, Node*> nodeMap = {};

  friend class PAGXImporter;
  friend class PAGXExporter;
  friend class TypesetterContext;
};

}  // namespace pagx
