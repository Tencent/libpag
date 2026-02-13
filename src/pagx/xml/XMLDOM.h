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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace pagx {

/**
 * Represents an XML attribute with name and value.
 */
struct DOMAttribute {
  std::string name;
  std::string value;
};

/**
 * Type of DOM node.
 */
enum class DOMNodeType {
  Element,
  Text,
};

/**
 * Represents a node in the XML DOM tree.
 */
struct DOMNode {
  std::string name;
  std::shared_ptr<DOMNode> firstChild;
  std::shared_ptr<DOMNode> nextSibling;
  std::vector<DOMAttribute> attributes;
  DOMNodeType type = DOMNodeType::Element;

  ~DOMNode();

  /**
   * Gets the first child node, optionally filtered by name.
   * @param name Optional filter by element name. Empty string matches all.
   * @return The first child node, or nullptr if not found.
   */
  std::shared_ptr<DOMNode> getFirstChild(const std::string& name = "") const;

  /**
   * Gets the next sibling node, optionally filtered by name.
   * @param name Optional filter by element name. Empty string matches all.
   * @return The next sibling node, or nullptr if not found.
   */
  std::shared_ptr<DOMNode> getNextSibling(const std::string& name = "") const;

  /**
   * Finds an attribute value by name.
   * @param attrName The attribute name to find.
   * @return A tuple of (found, value). If not found, returns (false, "").
   */
  const std::string* findAttribute(const std::string& attrName) const;

  /**
   * Counts the number of children, optionally filtered by name.
   * @param name Optional filter by element name. Empty string matches all.
   * @return The count of matching children.
   */
  int countChildren(const std::string& name = "") const;
};

/**
 * Represents an XML DOM tree.
 */
class DOM {
 public:
  ~DOM();

  /**
   * Constructs a DOM tree from XML data in memory.
   * @param data Pointer to XML data.
   * @param length Length of the data in bytes.
   * @return The DOM tree, or nullptr if parsing fails.
   */
  static std::shared_ptr<DOM> Make(const uint8_t* data, size_t length);

  /**
   * Constructs a DOM tree from an XML file.
   * @param filePath Path to the XML file.
   * @return The DOM tree, or nullptr if parsing fails.
   */
  static std::shared_ptr<DOM> MakeFromFile(const std::string& filePath);

  /**
   * Gets the root node of the DOM tree.
   * @return The root node.
   */
  std::shared_ptr<DOMNode> getRootNode() const;

 private:
  explicit DOM(std::shared_ptr<DOMNode> root);

  std::shared_ptr<DOMNode> _root = nullptr;
};

}  // namespace pagx
