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
#include "pagx/model/Model.h"

namespace pagx {

class PAGXXMLParser;

/**
 * Document is the root container for a PAGX document.
 * It contains resources and layers, and provides methods for loading, saving, and manipulating
 * the document.
 */
class Document {
 public:
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
   * Resources (images, gradients, compositions, etc.).
   * These can be referenced by "#id" in the document.
   */
  std::vector<std::unique_ptr<Resource>> resources = {};

  /**
   * Top-level layers.
   */
  std::vector<std::unique_ptr<Layer>> layers = {};

  /**
   * Base path for resolving relative resource paths.
   */
  std::string basePath = {};

  /**
   * Creates an empty document with the specified size.
   */
  static std::shared_ptr<Document> Make(float width, float height);

  /**
   * Loads a document from a file.
   * Returns nullptr if the file cannot be loaded.
   */
  static std::shared_ptr<Document> FromFile(const std::string& filePath);

  /**
   * Parses a document from XML content.
   * Returns nullptr if parsing fails.
   */
  static std::shared_ptr<Document> FromXML(const std::string& xmlContent);

  /**
   * Parses a document from XML data.
   * Returns nullptr if parsing fails.
   */
  static std::shared_ptr<Document> FromXML(const uint8_t* data, size_t length);

  /**
   * Exports the document to XML format.
   */
  std::string toXML() const;

  /**
   * Finds a resource by ID.
   * Returns nullptr if not found.
   */
  Resource* findResource(const std::string& id) const;

  /**
   * Finds a layer by ID (searches recursively).
   * Returns nullptr if not found.
   */
  Layer* findLayer(const std::string& id) const;

 private:
  friend class PAGXXMLParser;
  Document() = default;

  mutable std::unordered_map<std::string, Resource*> resourceMap = {};
  mutable bool resourceMapDirty = true;

  void rebuildResourceMap() const;
  static Layer* findLayerRecursive(const std::vector<std::unique_ptr<Layer>>& layers,
                                       const std::string& id);
};

}  // namespace pagx
