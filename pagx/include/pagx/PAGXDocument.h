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
#include "pagx/PAGXNode.h"

namespace pagx {

/**
 * PAGXDocument is the central data structure for the PAGX format.
 * It can be created from various sources (XML, SVG, PDF) and exported
 * to various formats (XML, PAG binary).
 */
class PAGXDocument {
 public:
  /**
   * Creates an empty PAGX document with the specified dimensions.
   */
  static std::shared_ptr<PAGXDocument> Make(float width, float height);

  /**
   * Creates a PAGX document from a file.
   * Supports .pagx (XML) and .svg files.
   */
  static std::shared_ptr<PAGXDocument> FromFile(const std::string& filePath);

  /**
   * Creates a PAGX document from XML content.
   */
  static std::shared_ptr<PAGXDocument> FromXML(const std::string& xmlContent);

  /**
   * Creates a PAGX document from XML data.
   */
  static std::shared_ptr<PAGXDocument> FromXML(const uint8_t* data, size_t length);

  /**
   * Returns the width of the document.
   */
  float width() const {
    return _width;
  }

  /**
   * Returns the height of the document.
   */
  float height() const {
    return _height;
  }

  /**
   * Sets the document dimensions.
   */
  void setSize(float width, float height);

  /**
   * Returns the PAGX version string.
   */
  const std::string& version() const {
    return _version;
  }

  /**
   * Returns the root node of the document tree.
   */
  PAGXNode* root() const {
    return _root.get();
  }

  /**
   * Sets the root node of the document.
   */
  void setRoot(std::unique_ptr<PAGXNode> root);

  /**
   * Creates a new node with the specified type.
   */
  std::unique_ptr<PAGXNode> createNode(PAGXNodeType type);

  // ============== Resource Management ==============

  /**
   * Returns a resource node by its ID.
   */
  PAGXNode* getResourceById(const std::string& id) const;

  /**
   * Adds a resource to the document.
   * The resource must have a unique ID.
   */
  void addResource(std::unique_ptr<PAGXNode> resource);

  /**
   * Returns all resource IDs.
   */
  std::vector<std::string> getResourceIds() const;

  /**
   * Returns the resources node.
   */
  PAGXNode* resources() const {
    return _resources.get();
  }

  // ============== Export ==============

  /**
   * Exports the document to PAGX XML format.
   */
  std::string toXML() const;

  /**
   * Saves the document to a file.
   */
  bool saveToFile(const std::string& filePath) const;

  // ============== Base Path ==============

  /**
   * Returns the base path for resolving relative resource paths.
   */
  const std::string& basePath() const {
    return _basePath;
  }

  /**
   * Sets the base path for resolving relative resource paths.
   */
  void setBasePath(const std::string& path) {
    _basePath = path;
  }

 private:
  PAGXDocument() = default;

  float _width = 0.0f;
  float _height = 0.0f;
  std::string _version = "1.0";
  std::string _basePath = {};
  std::unique_ptr<PAGXNode> _root = nullptr;
  std::unique_ptr<PAGXNode> _resources = nullptr;
  std::unordered_map<std::string, PAGXNode*> _resourceMap = {};

  friend class PAGXXMLParser;
};

}  // namespace pagx
