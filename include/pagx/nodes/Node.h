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

#include <string>
#include <unordered_map>

namespace pagx {

/**
 * NodeType enumerates all types of nodes that can be stored in a PAGX document. This includes
 * resources (Image, Composition, ColorSources) and elements (shapes, painters, modifiers, etc.).
 */
enum class NodeType {
  // Resources
  /**
   * A reusable path data resource.
   */
  PathData,
  /**
   * An image resource.
   */
  Image,
  /**
   * A composition resource containing layers.
   */
  Composition,
  /**
   * A solid color source.
   */
  SolidColor,
  /**
   * A linear gradient color source.
   */
  LinearGradient,
  /**
   * A radial gradient color source.
   */
  RadialGradient,
  /**
   * A conic (sweep) gradient color source.
   */
  ConicGradient,
  /**
   * A diamond gradient color source.
   */
  DiamondGradient,
  /**
   * An image pattern color source.
   */
  ImagePattern,
  /**
   * A color stop in a gradient.
   */
  ColorStop,
  /**
   * An embedded font resource containing glyph data.
   */
  Font,
  /**
   * A single glyph definition in a Font.
   */
  Glyph,

  // Layer
  /**
   * A layer node that contains vector elements and child layers.
   */
  Layer,

  // Layer Styles
  /**
   * A drop shadow layer style.
   */
  DropShadowStyle,
  /**
   * An inner shadow layer style.
   */
  InnerShadowStyle,
  /**
   * A background blur layer style.
   */
  BackgroundBlurStyle,

  // Layer Filters
  /**
   * A blur filter.
   */
  BlurFilter,
  /**
   * A drop shadow filter.
   */
  DropShadowFilter,
  /**
   * An inner shadow filter.
   */
  InnerShadowFilter,
  /**
   * A blend filter.
   */
  BlendFilter,
  /**
   * A color matrix filter.
   */
  ColorMatrixFilter,

  // Elements (shapes, painters, modifiers, containers)
  /**
   * A rectangle shape with optional rounded corners.
   */
  Rectangle,
  /**
   * An ellipse shape defined by center point and size.
   */
  Ellipse,
  /**
   * A polygon or star shape with configurable points and roundness.
   */
  Polystar,
  /**
   * A freeform path shape defined by a PathData.
   */
  Path,
  /**
   * A text element that generates glyphs for rendering.
   */
  Text,
  /**
   * A fill painter that fills shapes with a color or gradient.
   */
  Fill,
  /**
   * A stroke painter that outlines shapes with a color or gradient.
   */
  Stroke,
  /**
   * A path modifier that trims paths to a specified range.
   */
  TrimPath,
  /**
   * A path modifier that rounds the corners of shapes.
   */
  RoundCorner,
  /**
   * A path modifier that merges multiple paths using boolean operations.
   */
  MergePath,
  /**
   * A text animator that modifies text appearance with range-based transformations.
   */
  TextModifier,
  /**
   * A text animator that places text along a path.
   */
  TextPath,
  /**
   * A text modifier that controls text layout and alignment.
   */
  TextLayout,
  /**
   * A container that groups multiple elements with its own transform.
   */
  Group,
  /**
   * A modifier that creates multiple copies of preceding elements.
   */
  Repeater,

  // Text Selectors
  /**
   * A range selector for text modifiers.
   */
  RangeSelector,
  /**
   * A precomposed glyph run for text rendering.
   */
  GlyphRun
};

/**
 * Node is the base class for all shareable nodes in a PAGX document. Nodes can be stored in the
 * document's resources list and referenced by ID (e.g., "@resourceId"). Each node has a unique
 * identifier and a type.
 */
class Node {
 public:
  /**
   * The unique identifier of this node. Used for referencing the node by ID (e.g., "@id").
   */
  std::string id = {};

  /**
   * Custom data attributes. The keys are stored without the "data-" prefix.
   */
  std::unordered_map<std::string, std::string> customData = {};

  virtual ~Node() = default;

  /**
   * Returns the node type of this node.
   */
  virtual NodeType nodeType() const = 0;

 protected:
  Node() = default;
};

}  // namespace pagx
