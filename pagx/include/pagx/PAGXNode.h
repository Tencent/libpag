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
#include "pagx/PAGXTypes.h"
#include "pagx/PathData.h"

namespace pagx {

/**
 * Types of nodes in a PAGX document.
 */
enum class PAGXNodeType {
  // Document root
  Document,
  Resources,

  // Layers
  Layer,

  // Vector Elements
  Group,
  Rectangle,
  Ellipse,
  Polystar,
  Path,
  Text,

  // Styles
  Fill,
  Stroke,
  TrimPath,
  RoundCorner,
  MergePath,
  Repeater,

  // Color Sources
  SolidColor,
  LinearGradient,
  RadialGradient,
  ConicGradient,
  DiamondGradient,
  ImagePattern,

  // Effects
  BlurFilter,
  DropShadowFilter,
  DropShadowStyle,
  InnerShadowStyle,

  // Text
  TextSpan,

  // Masks
  Mask,
  ClipPath,

  // Image
  Image,

  // Unknown/Custom
  Unknown
};

/**
 * Returns the string name of a node type.
 */
const char* PAGXNodeTypeName(PAGXNodeType type);

/**
 * Parses a node type from its string name.
 */
PAGXNodeType PAGXNodeTypeFromName(const std::string& name);

/**
 * PAGXNode represents a node in the PAGX document tree.
 * Each node has a type, attributes, and optional children.
 */
class PAGXNode {
 public:
  virtual ~PAGXNode() = default;

  /**
   * Creates a new node with the specified type.
   */
  static std::unique_ptr<PAGXNode> Make(PAGXNodeType type);

  /**
   * Returns the type of this node.
   */
  PAGXNodeType type() const {
    return _type;
  }

  /**
   * Returns the ID of this node.
   */
  const std::string& id() const {
    return _id;
  }

  /**
   * Sets the ID of this node.
   */
  void setId(const std::string& id) {
    _id = id;
  }

  // ============== Attribute Access ==============

  /**
   * Returns true if the node has an attribute with the given name.
   */
  bool hasAttribute(const std::string& name) const;

  /**
   * Returns the string value of an attribute.
   */
  std::string getAttribute(const std::string& name, const std::string& defaultValue = "") const;

  /**
   * Returns the float value of an attribute.
   */
  float getFloatAttribute(const std::string& name, float defaultValue = 0.0f) const;

  /**
   * Returns the integer value of an attribute.
   */
  int getIntAttribute(const std::string& name, int defaultValue = 0) const;

  /**
   * Returns the boolean value of an attribute.
   */
  bool getBoolAttribute(const std::string& name, bool defaultValue = true) const;

  /**
   * Returns the color value of an attribute.
   */
  Color getColorAttribute(const std::string& name, const Color& defaultValue = Color::Black()) const;

  /**
   * Returns the matrix value of an attribute.
   */
  Matrix getMatrixAttribute(const std::string& name) const;

  /**
   * Returns the path data value of an attribute.
   */
  PathData getPathAttribute(const std::string& name) const;

  /**
   * Sets a string attribute.
   */
  void setAttribute(const std::string& name, const std::string& value);

  /**
   * Sets a float attribute.
   */
  void setFloatAttribute(const std::string& name, float value);

  /**
   * Sets an integer attribute.
   */
  void setIntAttribute(const std::string& name, int value);

  /**
   * Sets a boolean attribute.
   */
  void setBoolAttribute(const std::string& name, bool value);

  /**
   * Sets a color attribute.
   */
  void setColorAttribute(const std::string& name, const Color& color);

  /**
   * Sets a matrix attribute.
   */
  void setMatrixAttribute(const std::string& name, const Matrix& matrix);

  /**
   * Sets a path data attribute.
   */
  void setPathAttribute(const std::string& name, const PathData& path);

  /**
   * Removes an attribute.
   */
  void removeAttribute(const std::string& name);

  /**
   * Returns all attribute names.
   */
  std::vector<std::string> getAttributeNames() const;

  // ============== Children ==============

  /**
   * Returns the child nodes.
   */
  const std::vector<std::unique_ptr<PAGXNode>>& children() const {
    return _children;
  }

  /**
   * Returns the number of children.
   */
  size_t childCount() const {
    return _children.size();
  }

  /**
   * Returns the child at the specified index.
   */
  PAGXNode* childAt(size_t index) const;

  /**
   * Appends a child node.
   */
  void appendChild(std::unique_ptr<PAGXNode> child);

  /**
   * Inserts a child node at the specified index.
   */
  void insertChild(size_t index, std::unique_ptr<PAGXNode> child);

  /**
   * Removes and returns the child at the specified index.
   */
  std::unique_ptr<PAGXNode> removeChild(size_t index);

  /**
   * Removes all children.
   */
  void clearChildren();

  // ============== Parent ==============

  /**
   * Returns the parent node (weak reference).
   */
  PAGXNode* parent() const {
    return _parent;
  }

  // ============== Traversal ==============

  /**
   * Finds the first descendant node with the given ID.
   */
  PAGXNode* findById(const std::string& id);

  /**
   * Finds all descendant nodes of the given type.
   */
  std::vector<PAGXNode*> findByType(PAGXNodeType type);

  /**
   * Creates a deep copy of this node and all its children.
   */
  std::unique_ptr<PAGXNode> clone() const;

 protected:
  explicit PAGXNode(PAGXNodeType type) : _type(type) {
  }

 private:
  PAGXNodeType _type = PAGXNodeType::Unknown;
  std::string _id = {};
  std::unordered_map<std::string, std::string> _attributes = {};
  std::vector<std::unique_ptr<PAGXNode>> _children = {};
  PAGXNode* _parent = nullptr;
};

}  // namespace pagx
