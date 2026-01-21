/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <string>

namespace pagx {

/**
 * NodeType enumerates all types of nodes that can be stored in a PAGX document.
 * This includes resources (Image, Composition) and other shared definitions.
 */
enum class NodeType {
  /**
   * An image resource.
   */
  Image,
  /**
   * A reusable path data resource.
   */
  PathData,
  /**
   * A composition resource containing layers.
   */
  Composition
};

/**
 * Returns the string name of a node type.
 */
const char* NodeTypeName(NodeType type);

/**
 * Node is the base class for all shareable nodes in a PAGX document. Nodes can be stored in the
 * document's resources list and referenced by ID (e.g., "#imageId"). Each node has a unique
 * identifier and a type.
 */
class Node {
 public:
  /**
   * The unique identifier of this node. Used for referencing the node by ID (e.g., "#id").
   */
  std::string id = {};

  virtual ~Node() = default;

  /**
   * Returns the node type of this node.
   */
  virtual NodeType nodeType() const = 0;

 protected:
  Node() = default;
};

}  // namespace pagx
