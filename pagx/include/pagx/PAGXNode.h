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
#include <vector>
#include "pagx/PAGXTypes.h"
#include "pagx/PathData.h"

namespace pagx {

class PAGXNode;
class ColorSourceNode;
class VectorElementNode;
class LayerStyleNode;
class LayerFilterNode;
struct LayerNode;

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
class PAGXNode {
 public:
  virtual ~PAGXNode() = default;

  /**
   * Returns the type of this node.
   */
  virtual NodeType type() const = 0;

  /**
   * Returns a deep clone of this node.
   */
  virtual std::unique_ptr<PAGXNode> clone() const = 0;

 protected:
  PAGXNode() = default;
};

//==============================================================================
// Resource Node (base class - must be defined before ColorSourceNode)
//==============================================================================

/**
 * Base class for resource nodes.
 */
class ResourceNode : public PAGXNode {
 public:
  std::string id = {};
};

//==============================================================================
// Color Source Nodes
//==============================================================================

/**
 * A color stop in a gradient.
 */
struct ColorStopNode : public PAGXNode {
  float offset = 0;
  Color color = {};

  NodeType type() const override {
    return NodeType::ColorStop;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<ColorStopNode>(*this);
  }
};

/**
 * Base class for color source nodes.
 * Color sources can be stored as resources (with an id) or inline.
 */
class ColorSourceNode : public ResourceNode {
};

/**
 * A solid color.
 */
struct SolidColorNode : public ColorSourceNode {
  Color color = {};

  NodeType type() const override {
    return NodeType::SolidColor;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<SolidColorNode>(*this);
  }
};

/**
 * A linear gradient.
 */
struct LinearGradientNode : public ColorSourceNode {
  Point startPoint = {};
  Point endPoint = {};
  Matrix matrix = {};
  std::vector<ColorStopNode> colorStops = {};

  NodeType type() const override {
    return NodeType::LinearGradient;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<LinearGradientNode>(*this);
  }
};

/**
 * A radial gradient.
 */
struct RadialGradientNode : public ColorSourceNode {
  Point center = {};
  float radius = 0;
  Matrix matrix = {};
  std::vector<ColorStopNode> colorStops = {};

  NodeType type() const override {
    return NodeType::RadialGradient;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<RadialGradientNode>(*this);
  }
};

/**
 * A conic (sweep) gradient.
 */
struct ConicGradientNode : public ColorSourceNode {
  Point center = {};
  float startAngle = 0;
  float endAngle = 360;
  Matrix matrix = {};
  std::vector<ColorStopNode> colorStops = {};

  NodeType type() const override {
    return NodeType::ConicGradient;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<ConicGradientNode>(*this);
  }
};

/**
 * A diamond gradient.
 */
struct DiamondGradientNode : public ColorSourceNode {
  Point center = {};
  float halfDiagonal = 0;
  Matrix matrix = {};
  std::vector<ColorStopNode> colorStops = {};

  NodeType type() const override {
    return NodeType::DiamondGradient;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<DiamondGradientNode>(*this);
  }
};

/**
 * An image pattern.
 */
struct ImagePatternNode : public ColorSourceNode {
  std::string image = {};
  TileMode tileModeX = TileMode::Clamp;
  TileMode tileModeY = TileMode::Clamp;
  SamplingMode sampling = SamplingMode::Linear;
  Matrix matrix = {};

  NodeType type() const override {
    return NodeType::ImagePattern;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<ImagePatternNode>(*this);
  }
};

//==============================================================================
// Vector Element Nodes
//==============================================================================

/**
 * Base class for vector element nodes.
 */
class VectorElementNode : public PAGXNode {};

/**
 * A rectangle shape.
 */
struct RectangleNode : public VectorElementNode {
  Point center = {};
  Size size = {100, 100};
  float roundness = 0;
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Rectangle;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<RectangleNode>(*this);
  }
};

/**
 * An ellipse shape.
 */
struct EllipseNode : public VectorElementNode {
  Point center = {};
  Size size = {100, 100};
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Ellipse;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<EllipseNode>(*this);
  }
};

/**
 * A polygon or star shape.
 */
struct PolystarNode : public VectorElementNode {
  Point center = {};
  PolystarType polystarType = PolystarType::Star;
  float pointCount = 5;
  float outerRadius = 100;
  float innerRadius = 50;
  float rotation = 0;
  float outerRoundness = 0;
  float innerRoundness = 0;
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Polystar;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<PolystarNode>(*this);
  }
};

/**
 * A path shape.
 */
struct PathNode : public VectorElementNode {
  PathData data = {};
  bool reversed = false;

  NodeType type() const override {
    return NodeType::Path;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<PathNode>(*this);
  }
};

/**
 * A text span.
 */
struct TextSpanNode : public VectorElementNode {
  float x = 0;
  float y = 0;
  std::string font = {};
  float fontSize = 12;
  int fontWeight = 400;
  FontStyle fontStyle = FontStyle::Normal;
  float tracking = 0;
  float baselineShift = 0;
  TextAnchor textAnchor = TextAnchor::Start;
  std::string text = {};

  NodeType type() const override {
    return NodeType::TextSpan;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<TextSpanNode>(*this);
  }
};

//==============================================================================
// Painter Nodes
//==============================================================================

/**
 * A fill painter.
 * The color can be a simple color string ("#FF0000"), a reference ("#gradientId"),
 * or an inline color source node.
 */
struct FillNode : public VectorElementNode {
  std::string color = {};
  std::unique_ptr<ColorSourceNode> colorSource = nullptr;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  FillRule fillRule = FillRule::Winding;
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Fill;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    auto node = std::make_unique<FillNode>();
    node->color = color;
    if (colorSource) {
      node->colorSource.reset(static_cast<ColorSourceNode*>(colorSource->clone().release()));
    }
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->fillRule = fillRule;
    node->placement = placement;
    return node;
  }
};

/**
 * A stroke painter.
 */
struct StrokeNode : public VectorElementNode {
  std::string color = {};
  std::unique_ptr<ColorSourceNode> colorSource = nullptr;
  float strokeWidth = 1;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  LineCap cap = LineCap::Butt;
  LineJoin join = LineJoin::Miter;
  float miterLimit = 4;
  std::vector<float> dashes = {};
  float dashOffset = 0;
  StrokeAlign align = StrokeAlign::Center;
  Placement placement = Placement::Background;

  NodeType type() const override {
    return NodeType::Stroke;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    auto node = std::make_unique<StrokeNode>();
    node->color = color;
    if (colorSource) {
      node->colorSource.reset(static_cast<ColorSourceNode*>(colorSource->clone().release()));
    }
    node->strokeWidth = strokeWidth;
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->cap = cap;
    node->join = join;
    node->miterLimit = miterLimit;
    node->dashes = dashes;
    node->dashOffset = dashOffset;
    node->align = align;
    node->placement = placement;
    return node;
  }
};

//==============================================================================
// Shape Modifier Nodes
//==============================================================================

/**
 * Trim path modifier.
 */
struct TrimPathNode : public VectorElementNode {
  float start = 0;
  float end = 1;
  float offset = 0;
  TrimType trimType = TrimType::Separate;

  NodeType type() const override {
    return NodeType::TrimPath;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<TrimPathNode>(*this);
  }
};

/**
 * Round corner modifier.
 */
struct RoundCornerNode : public VectorElementNode {
  float radius = 10;

  NodeType type() const override {
    return NodeType::RoundCorner;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<RoundCornerNode>(*this);
  }
};

/**
 * Merge path modifier.
 */
struct MergePathNode : public VectorElementNode {
  MergePathMode mode = MergePathMode::Append;

  NodeType type() const override {
    return NodeType::MergePath;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<MergePathNode>(*this);
  }
};

//==============================================================================
// Text Modifier Nodes
//==============================================================================

/**
 * Range selector for text modifier.
 */
struct RangeSelectorNode : public PAGXNode {
  float start = 0;
  float end = 1;
  float offset = 0;
  SelectorUnit unit = SelectorUnit::Percentage;
  SelectorShape shape = SelectorShape::Square;
  float easeIn = 0;
  float easeOut = 0;
  SelectorMode mode = SelectorMode::Add;
  float weight = 1;
  bool randomizeOrder = false;
  int randomSeed = 0;

  NodeType type() const override {
    return NodeType::RangeSelector;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<RangeSelectorNode>(*this);
  }
};

/**
 * Text modifier.
 */
struct TextModifierNode : public VectorElementNode {
  Point anchorPoint = {};
  Point position = {};
  float rotation = 0;
  Point scale = {1, 1};
  float skew = 0;
  float skewAxis = 0;
  float alpha = 1;
  std::string fillColor = {};
  std::string strokeColor = {};
  float strokeWidth = -1;
  std::vector<RangeSelectorNode> rangeSelectors = {};

  NodeType type() const override {
    return NodeType::TextModifier;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<TextModifierNode>(*this);
  }
};

/**
 * Text path modifier.
 */
struct TextPathNode : public VectorElementNode {
  std::string path = {};
  TextPathAlign textPathAlign = TextPathAlign::Start;
  float firstMargin = 0;
  float lastMargin = 0;
  bool perpendicularToPath = true;
  bool reversed = false;
  bool forceAlignment = false;

  NodeType type() const override {
    return NodeType::TextPath;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<TextPathNode>(*this);
  }
};

/**
 * Text layout modifier.
 */
struct TextLayoutNode : public VectorElementNode {
  float width = 0;
  float height = 0;
  TextAlign textAlign = TextAlign::Left;
  VerticalAlign verticalAlign = VerticalAlign::Top;
  float lineHeight = 1.2f;
  float indent = 0;
  Overflow overflow = Overflow::Clip;

  NodeType type() const override {
    return NodeType::TextLayout;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<TextLayoutNode>(*this);
  }
};

//==============================================================================
// Repeater Node
//==============================================================================

/**
 * Repeater modifier.
 */
struct RepeaterNode : public VectorElementNode {
  float copies = 3;
  float offset = 0;
  RepeaterOrder order = RepeaterOrder::BelowOriginal;
  Point anchorPoint = {};
  Point position = {100, 100};
  float rotation = 0;
  Point scale = {1, 1};
  float startAlpha = 1;
  float endAlpha = 1;

  NodeType type() const override {
    return NodeType::Repeater;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<RepeaterNode>(*this);
  }
};

//==============================================================================
// Group Node
//==============================================================================

/**
 * Group container.
 */
struct GroupNode : public VectorElementNode {
  std::string name = {};
  Point anchorPoint = {};
  Point position = {};
  float rotation = 0;
  Point scale = {1, 1};
  float skew = 0;
  float skewAxis = 0;
  float alpha = 1;
  std::vector<std::unique_ptr<VectorElementNode>> elements = {};

  NodeType type() const override {
    return NodeType::Group;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    auto node = std::make_unique<GroupNode>();
    node->name = name;
    node->anchorPoint = anchorPoint;
    node->position = position;
    node->rotation = rotation;
    node->scale = scale;
    node->skew = skew;
    node->skewAxis = skewAxis;
    node->alpha = alpha;
    for (const auto& element : elements) {
      node->elements.push_back(
          std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(element->clone().release())));
    }
    return node;
  }
};

//==============================================================================
// Layer Style Nodes
//==============================================================================

/**
 * Base class for layer style nodes.
 */
class LayerStyleNode : public PAGXNode {
 public:
  BlendMode blendMode = BlendMode::Normal;
};

/**
 * Drop shadow style.
 */
struct DropShadowStyleNode : public LayerStyleNode {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool showBehindLayer = true;

  NodeType type() const override {
    return NodeType::DropShadowStyle;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<DropShadowStyleNode>(*this);
  }
};

/**
 * Inner shadow style.
 */
struct InnerShadowStyleNode : public LayerStyleNode {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};

  NodeType type() const override {
    return NodeType::InnerShadowStyle;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<InnerShadowStyleNode>(*this);
  }
};

/**
 * Background blur style.
 */
struct BackgroundBlurStyleNode : public LayerStyleNode {
  float blurrinessX = 0;
  float blurrinessY = 0;
  TileMode tileMode = TileMode::Mirror;

  NodeType type() const override {
    return NodeType::BackgroundBlurStyle;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<BackgroundBlurStyleNode>(*this);
  }
};

//==============================================================================
// Layer Filter Nodes
//==============================================================================

/**
 * Base class for layer filter nodes.
 */
class LayerFilterNode : public PAGXNode {};

/**
 * Blur filter.
 */
struct BlurFilterNode : public LayerFilterNode {
  float blurrinessX = 0;
  float blurrinessY = 0;
  TileMode tileMode = TileMode::Decal;

  NodeType type() const override {
    return NodeType::BlurFilter;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<BlurFilterNode>(*this);
  }
};

/**
 * Drop shadow filter.
 */
struct DropShadowFilterNode : public LayerFilterNode {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool shadowOnly = false;

  NodeType type() const override {
    return NodeType::DropShadowFilter;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<DropShadowFilterNode>(*this);
  }
};

/**
 * Inner shadow filter.
 */
struct InnerShadowFilterNode : public LayerFilterNode {
  float offsetX = 0;
  float offsetY = 0;
  float blurrinessX = 0;
  float blurrinessY = 0;
  Color color = {};
  bool shadowOnly = false;

  NodeType type() const override {
    return NodeType::InnerShadowFilter;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<InnerShadowFilterNode>(*this);
  }
};

/**
 * Blend filter.
 */
struct BlendFilterNode : public LayerFilterNode {
  Color color = {};
  BlendMode filterBlendMode = BlendMode::Normal;

  NodeType type() const override {
    return NodeType::BlendFilter;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<BlendFilterNode>(*this);
  }
};

/**
 * Color matrix filter.
 */
struct ColorMatrixFilterNode : public LayerFilterNode {
  std::array<float, 20> matrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

  NodeType type() const override {
    return NodeType::ColorMatrixFilter;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<ColorMatrixFilterNode>(*this);
  }
};

//==============================================================================
// Other Resource Nodes
//==============================================================================

/**
 * Image resource.
 */
struct ImageNode : public ResourceNode {
  std::string source = {};

  NodeType type() const override {
    return NodeType::Image;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<ImageNode>(*this);
  }
};

/**
 * PathData resource - stores reusable path data.
 */
struct PathDataNode : public ResourceNode {
  std::string data = {};  // SVG path data string

  NodeType type() const override {
    return NodeType::PathData;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    return std::make_unique<PathDataNode>(*this);
  }
};

/**
 * Composition resource.
 */
struct CompositionNode : public ResourceNode {
  float width = 0;
  float height = 0;
  std::vector<std::unique_ptr<LayerNode>> layers = {};

  NodeType type() const override {
    return NodeType::Composition;
  }

  std::unique_ptr<PAGXNode> clone() const override;
};

//==============================================================================
// Layer Node
//==============================================================================

/**
 * Layer node.
 */
struct LayerNode : public PAGXNode {
  std::string id = {};
  std::string name = {};
  bool visible = true;
  float alpha = 1;
  BlendMode blendMode = BlendMode::Normal;
  float x = 0;
  float y = 0;
  Matrix matrix = {};
  std::vector<float> matrix3D = {};
  bool preserve3D = false;
  bool antiAlias = true;
  bool groupOpacity = false;
  bool passThroughBackground = true;
  bool excludeChildEffectsInLayerStyle = false;
  Rect scrollRect = {};
  bool hasScrollRect = false;
  std::string mask = {};
  MaskType maskType = MaskType::Alpha;
  std::string composition = {};

  std::vector<std::unique_ptr<VectorElementNode>> contents = {};
  std::vector<std::unique_ptr<LayerStyleNode>> styles = {};
  std::vector<std::unique_ptr<LayerFilterNode>> filters = {};
  std::vector<std::unique_ptr<LayerNode>> children = {};

  NodeType type() const override {
    return NodeType::Layer;
  }

  std::unique_ptr<PAGXNode> clone() const override {
    auto node = std::make_unique<LayerNode>();
    node->id = id;
    node->name = name;
    node->visible = visible;
    node->alpha = alpha;
    node->blendMode = blendMode;
    node->x = x;
    node->y = y;
    node->matrix = matrix;
    node->matrix3D = matrix3D;
    node->preserve3D = preserve3D;
    node->antiAlias = antiAlias;
    node->groupOpacity = groupOpacity;
    node->passThroughBackground = passThroughBackground;
    node->excludeChildEffectsInLayerStyle = excludeChildEffectsInLayerStyle;
    node->scrollRect = scrollRect;
    node->hasScrollRect = hasScrollRect;
    node->mask = mask;
    node->maskType = maskType;
    node->composition = composition;
    for (const auto& element : contents) {
      node->contents.push_back(
          std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(element->clone().release())));
    }
    for (const auto& style : styles) {
      node->styles.push_back(
          std::unique_ptr<LayerStyleNode>(static_cast<LayerStyleNode*>(style->clone().release())));
    }
    for (const auto& filter : filters) {
      node->filters.push_back(
          std::unique_ptr<LayerFilterNode>(static_cast<LayerFilterNode*>(filter->clone().release())));
    }
    for (const auto& child : children) {
      node->children.push_back(
          std::unique_ptr<LayerNode>(static_cast<LayerNode*>(child->clone().release())));
    }
    return node;
  }
};

// Implementation of CompositionNode::clone
inline std::unique_ptr<PAGXNode> CompositionNode::clone() const {
  auto node = std::make_unique<CompositionNode>();
  node->id = id;
  node->width = width;
  node->height = height;
  for (const auto& layer : layers) {
    node->layers.push_back(
        std::unique_ptr<LayerNode>(static_cast<LayerNode*>(layer->clone().release())));
  }
  return node;
}

}  // namespace pagx
