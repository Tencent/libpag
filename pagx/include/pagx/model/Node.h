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

namespace pagx {

/**
 * Node types in PAGX document.
 */
enum class NodeType {
  // Color sources
  SolidColor,
  LinearGradient,
  RadialGradient,
  ConicGradient,
  DiamondGradient,
  ImagePattern,
  ColorStop,

  // Geometry elements
  Rectangle,
  Ellipse,
  Polystar,
  Path,
  TextSpan,

  // Painters
  Fill,
  Stroke,

  // Shape modifiers
  TrimPath,
  RoundCorner,
  MergePath,

  // Text modifiers
  TextModifier,
  TextPath,
  TextLayout,
  RangeSelector,

  // Repeater
  Repeater,

  // Container
  Group,

  // Layer styles
  DropShadowStyle,
  InnerShadowStyle,
  BackgroundBlurStyle,

  // Layer filters
  BlurFilter,
  DropShadowFilter,
  InnerShadowFilter,
  BlendFilter,
  ColorMatrixFilter,

  // Resources
  Image,
  PathData,
  Composition,

  // Layer
  Layer
};

/**
 * Returns the string name of a node type.
 */
const char* NodeTypeName(NodeType type);

/**
 * Base class for all PAGX nodes.
 */
class Node {
 public:
  virtual ~Node() = default;

  /**
   * Returns the type of this node.
   */
  virtual NodeType type() const = 0;

  /**
   * Returns a deep clone of this node.
   */
  virtual std::unique_ptr<Node> clone() const = 0;

 protected:
  Node() = default;
};

}  // namespace pagx
