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

namespace pagx {

/**
 * NodeType enumerates all types of nodes in a PAGX document.
 * This unified type system is used for type checking across different categories of nodes.
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

}  // namespace pagx
