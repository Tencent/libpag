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

#include <cmath>
#include <string>
#include <vector>
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/MaskType.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/types/Padding.h"
#include "pagx/types/Point.h"
#include "pagx/types/Rect.h"

namespace pagx {

class Composition;

/**
 * Layer represents a layer node that can contain vector elements, layer styles, filters, and child
 * layers. It is the main building block for composing visual content in a PAGX document.
 */
class Layer : public Node, public LayoutNode {
 public:
  /**
   * The display name of the layer.
   */
  std::string name = {};

  /**
   * Whether the layer is visible. The default value is true.
   */
  bool visible = true;

  /**
   * The opacity of the layer, ranging from 0 to 1. The default value is 1.
   */
  float alpha = 1.0f;

  /**
   * The blend mode used when compositing the layer. The default value is Normal.
   */
  BlendMode blendMode = BlendMode::Normal;

  /**
   * The x-coordinate of the layer position. The default value is 0.
   */
  float x = 0.0f;

  /**
   * The y-coordinate of the layer position. The default value is 0.
   */
  float y = 0.0f;

  /**
   * The 2D transformation matrix of the layer.
   */
  Matrix matrix = {};

  /**
   * The 3D transformation matrix.
   */
  Matrix3D matrix3D = {};

  /**
   * Whether to preserve 3D transformations for child layers. The default value is false.
   */
  bool preserve3D = false;

  /**
   * Whether to apply antialiasing to the layer edges. The default value is true.
   */
  bool antiAlias = true;

  /**
   * Whether to use group opacity mode for the layer and its children. The default value is true.
   */
  bool groupOpacity = true;

  /**
   * Whether layer effects pass through to the background. The default value is true.
   */
  bool passThroughBackground = true;

  /**
   * The scroll rectangle for clipping the layer content.
   */
  Rect scrollRect = {};

  /**
   * Whether a scroll rectangle is applied to the layer. The default value is false.
   */
  bool hasScrollRect = false;

  /**
   * Whether the layer clips its content to its own bounds. When true, auto layout expands this into
   * a scrollRect using the layer's resolved width and height. Ignored if the layer already has a
   * scrollRect or if both dimensions are unresolved. The default value is false.
   */
  bool clipToBounds = false;

  /**
   * A reference to a mask layer.
   */
  Layer* mask = nullptr;

  /**
   * The type of masking to apply (Alpha, Luminance, or Contour).
   * The default value is Alpha.
   */
  MaskType maskType = MaskType::Alpha;

  /**
   * A reference to a composition used as the layer content.
   */
  Composition* composition = nullptr;

  /**
   * The vector elements contained in this layer (shapes, painters, modifiers, etc.).
   */
  std::vector<Element*> contents = {};

  /**
   * The layer styles applied to this layer (drop shadow, inner shadow, etc.).
   */
  std::vector<LayerStyle*> styles = {};

  /**
   * The filters applied to this layer (blur, color matrix, etc.).
   */
  std::vector<LayerFilter*> filters = {};

  /**
   * The child layers contained in this layer.
   */
  std::vector<Layer*> children = {};

  /**
   * The container layout mode for arranging child layers. When set to Horizontal or Vertical,
   * child layers are automatically positioned along this axis. The default value is None.
   */
  LayoutMode layout = LayoutMode::None;

  /**
   * The spacing between adjacent child layers in the layout direction. The default value is 0.
   */
  float gap = 0.0f;

  /**
   * The flex grow factor for this layer within a container layout. When a child has no explicit
   * main-axis size: flex=0 (default) uses content-measured size; flex>0 distributes remaining space
   * proportionally among flex children by weight. Ignored when an explicit width/height is set.
   */
  float flex = 0.0f;

  /**
   * The inner padding of the layout container. The default value is zero on all sides.
   */
  Padding padding = {};

  /**
   * The alignment of child elements along the cross axis. The default value is Stretch.
   */
  Alignment alignment = Alignment::Stretch;

  /**
   * The arrangement of child elements along the main axis. The default value is Start.
   */
  Arrangement arrangement = Arrangement::Start;

  /**
   * Whether this layer participates in its parent's container layout. When false, the layer is
   * excluded from the layout flow (not measured, not positioned) but remains visible at its own
   * x/y coordinates. The default value is true.
   */
  bool includeInLayout = true;

  // ── Import directives (resolved by `pagx resolve`) ──

  /**
   * Describes an import directive on this Layer. When source or content is non-empty, the Layer
   * contains unresolved imported content that must be expanded by `pagx resolve`.
   */
  struct ImportDirective {
    /**
     * Path to an external file to import (e.g., SVG). Relative to the PAGX file.
     */
    std::string source = {};

    /**
     * Forced import format (e.g., "svg"). When empty, inferred from file extension (external)
     * or child element tag name (inline).
     */
    std::string format = {};

    /**
     * Raw XML text of inline import content (e.g., `<svg>...</svg>`). Populated by the parser
     * when inline content such as `<svg>` is found inside the Layer.
     */
    std::string content = {};

    /**
     * Description of the import source after resolution (e.g., "inline svg",
     * "assets/logo.svg"). Set by `pagx resolve`; used by the exporter to emit an XML comment.
     */
    std::string resolvedFrom = {};
  };

  ImportDirective importDirective = {};

  NodeType nodeType() const override {
    return NodeType::Layer;
  }

  /** Returns the layer position adjusted to the layout bounds. */
  Point renderPosition() const;

 private:
  Layer() = default;

  void performContainerLayout(LayoutContext* context);
  void updateScrollRect();

  void updateSize(LayoutContext* context) override;
  void onMeasure(LayoutContext* context) override;
  void setLayoutSize(LayoutContext* context, float targetWidth, float targetHeight) override;
  void updateLayout(LayoutContext* context) override;

  friend class PAGXDocument;
};

}  // namespace pagx
