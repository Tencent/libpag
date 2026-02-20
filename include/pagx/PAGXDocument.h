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
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Data.h"

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
   * @param width the canvas width in pixels
   * @param height the canvas height in pixels
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
   * @param id an optional unique identifier for the node, used for lookup via findNode(). If empty,
   * the node is not indexed.
   */
  template <typename T>
  T* makeNode(const std::string& id = "") {
    auto node = std::unique_ptr<T>(new T());
    auto* result = node.get();
    registerNode(result, id);
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
   * The caller must ensure T matches the actual node type, otherwise behavior is undefined.
   * @param id The unique identifier of the node.
   * @return A pointer to the node cast to type T, or nullptr if not found.
   */
  template <typename T>
  T* findNode(const std::string& id) const {
    return static_cast<T*>(findNode(id));
  }

  /**
   * All nodes in the document (owned by the document).
   */
  std::vector<std::unique_ptr<Node>> nodes = {};

  /**
   * Errors collected during parsing. Non-empty errors indicate structural issues in the source
   * document but do not prevent the document from being returned. The parsed content may be
   * incomplete where errors occurred.
   */
  std::vector<std::string> errors = {};

  /**
   * Returns a list of external file paths referenced by Image nodes that have no embedded data.
   * Data URIs (paths starting with "data:") are excluded.
   */
  std::vector<std::string> getExternalFilePaths() const;

  /**
   * Loads external file data for an Image node matching the given file path. Once loaded, the
   * Image's data field is populated and its filePath is cleared, so the renderer uses embedded data
   * instead of file I/O.
   * @param filePath the external file path to match against Image nodes
   * @param data the file content to embed into the matching Image node
   * @return true if a matching Image node was found and its data was loaded successfully
   */
  bool loadFileData(const std::string& filePath, std::shared_ptr<Data> data);

 private:
  PAGXDocument() = default;

  void registerNode(Node* node, const std::string& id);

  std::unordered_map<std::string, Node*> nodeMap = {};

  friend class PAGXImporter;
  friend class PAGXExporter;
  friend class TextLayoutContext;
};

}  // namespace pagx
