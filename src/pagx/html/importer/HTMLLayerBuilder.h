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

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include "pagx/html/importer/HTMLBackgroundLayout.h"
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLSvgFilterDecoder.h"
#include "pagx/types/BlendMode.h"

namespace pagx {

struct DOMNode;
struct Color;
class ColorSource;
class Element;
class Fill;
class HTMLDiagnosticSink;
class HTMLIdAllocator;
class HTMLInlineSvgEmitter;
class HTMLValueParser;
class Layer;
class PAGXDocument;

struct HTMLBackgroundImage {
  ColorSource* color = nullptr;
  float intrinsicWidth = NAN;
  float intrinsicHeight = NAN;
  // SVG stays vector. The factory creates a fresh import host for each visible tile.
  std::function<Layer*(float, float)> vectorTile;
};

/**
 * Applies typed `HTMLBoxAttributes` onto PAGX `Layer` instances and synthesises the geometry /
 * fill / stroke / shadow chain needed for HTML containers and replaced elements.
 *
 * The builder is layer-side: its public surface treats elements as opaque DOM nodes (used only
 * for `data-*` attribute forwarding in `applyLayerAttributes`) and never decides which element
 * becomes which kind of layer — that classification stays in `HTMLParserContext`. Borrows the
 * importer's diagnostic sink, value parser and ID allocator; the document handle is bound via
 * `bindDocument` after `PAGXDocument::Make` runs.
 */
class HTMLLayerBuilder {
 public:
  HTMLLayerBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                   HTMLInlineSvgEmitter& svgEmitter, HTMLIdAllocator& idAllocator);

  void bindDocument(PAGXDocument* document);
  void setBackgroundImageResolver(std::function<HTMLBackgroundImage(const std::string&)> resolver);
  static bool usesBackgroundLayout(const HTMLBoxAttributes& box);

  // Geometry / fills ------------------------------------------------------------------
  /** Writes width/height (px or %) and absolute-positioning anchors onto `layer`. Also
   *  forwards `flex` since it is an "as-a-child" property that applies regardless of the
   *  layer's own role. */
  void applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box);

  /** Writes layout-mode (horizontal/vertical), gap, padding, alignment and arrangement onto
   *  `layer`. Unsupported keywords are warned via `_diagnostics`. */
  void applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box);

  /** Builds the background geometry node (Rectangle when corner radii are uniform, Path when
   *  the author requested per-corner radii on a sized box). */
  Element* buildBackgroundGeometry(const HTMLBoxAttributes& box);

  /** Emits the standard background visual chain (geometry + fill + stroke + shadows +
   *  backdrop filter). When `foregroundFill` is supplied, it is painted after the CSS background
   *  and before the border; this is used for replaced elements such as `<img>`. Returns true when
   *  any visual was emitted. */
  bool applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box,
                              Fill* foregroundFill = nullptr);

  /** Applies the resolved CSS `transform` (incl. `transform-origin`) onto `layer->matrix`. */
  void applyBoxTransform(Layer* layer, const HTMLBoxAttributes& box,
                         const std::shared_ptr<DOMNode>& element);

  /** Layer-level attributes that don't depend on background: opacity, blend mode, filter
   *  chain, overflow clip, data-* attributes. */
  void applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                            const HTMLBoxAttributes& box);

  /** Copies the element's `data-*` attributes into `layer->customData` (invalid keys and
   *  conflicting overwrites are warned). Split out from `applyLayerAttributes` so the
   *  folded-image path — which reuses a parent layer and skips the rest of the layer-attribute
   *  chain — can still forward the inner `<img>`'s custom data. */
  void forwardDataAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element);

  /** Builds a `Fill` chain whose colour is a single solid `color`. */
  Fill* buildSolidFill(const Color& color);

  /** Maps a CSS `background-blend-mode` value onto a PAGX `BlendMode` (the first top-level
   *  keyword when a per-layer list is given). Returns `BlendMode::Normal` for an empty,
   *  `normal`, or unrecognised value. */
  static BlendMode resolveBackgroundBlendMode(const std::string& value);

  /** Resolves a CSS gradient string into a registered gradient node. `boxWidth` / `boxHeight`
   *  are the painted box size in px, used to normalise radial-gradient size/position descriptors;
   *  pass NaN when unknown. */
  ColorSource* parseGradientByValue(const std::string& value, float boxWidth = NAN,
                                    float boxHeight = NAN);

  // Paint background-clip:text after the descendant text geometry is available.
  void applyTextBackground(Layer* layer, const HTMLBoxAttributes& box);

  // Inner host / wrappers --------------------------------------------------------------
  /** Returns true when the box carries any visual that requires a Rectangle/Fill/Stroke
   *  chain on the outer Layer (used to decide whether a double-host split is needed). */
  static bool hasBackgroundVisuals(const HTMLBoxAttributes& box);

  /** Returns true when the box carries authored container-layout semantics. This is broader than
   *  requiresInnerHost(): zero padding and zero-gap flex still affect structural folds even though
   *  they do not require a separate background host. */
  static bool hasLayoutHostAttributes(const HTMLBoxAttributes& box);

  /** Returns true when the final layout host has a non-zero content inset, requiring the outer
   *  Layer to carry border-box paint and an inner Layer to carry padding / layout. */
  static bool requiresInnerHost(const HTMLBoxAttributes& box);

  /** Creates the inner host Layer for the standard "outer background + inner padded
   *  container" double-layer pattern, including the border inset of flex containers. */
  Layer* createInnerHost(Layer* outer, const HTMLBoxAttributes& box);

  // Keep CSS absolute children relative to the padding box, independently of the
  // content inset used by PAGX flow layout. Each child keeps its paint-order slot.
  void appendCSSChild(Layer* host, Layer* child, const HTMLBoxAttributes& box);

  /** Places geometric outer box shadows behind `inner`, outside its overflow clip.
   *  Also hoists legacy DropShadowStyle entries when clipping requires a wrapper.
   *  Returns `inner` unchanged when no split is needed. */
  Layer* maybeSplitBoxShadowFromClip(Layer* inner);

  /** Materialises CSS `margin` on `inner` through positioning / padding (PAGX has no
   *  margin concept on Layer). Returns `inner` unchanged when every side is zero. */
  Layer* wrapForMargin(Layer* inner, const HTMLBoxAttributes& box);

  /** Emits a single text-decoration line (underline or line-through) onto `host`. */
  void emitTextDecorationLine(Layer* host, const Color& textColor, const Color& decorationColor,
                              bool decorationColorDiffers, float bottom, float centerY);

 private:
  bool applyBorderImage(Layer* layer, const HTMLBoxAttributes& box);
  bool applyBackgroundLayers(Layer* layer, const HTMLBoxAttributes& box);
  Layer* makeBackgroundClip(const HTMLBoxAttributes& box, const HTMLBackgroundRect& rect);
  void applyBackgroundFill(Layer* layer, const HTMLBoxAttributes& box, Element* geometry,
                           bool& emitted);
  void emitInsetBackgroundLayer(Layer* layer, const HTMLBoxAttributes& box, ColorSource* color,
                                const Padding& inset, BlendMode blendMode);
  void applyBorderStroke(Layer* layer, const HTMLBoxAttributes& box, Element* geometry,
                         bool& emitted);
  void applyBoxShadows(Layer* layer, const HTMLBoxAttributes& box, bool& emitted);
  void applyBackdropFilter(Layer* layer, const HTMLBoxAttributes& box, bool& emitted);

  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  HTMLIdAllocator& _idAllocator;
  HTMLSvgFilterDecoder _filterDecoder;
  PAGXDocument* _document = nullptr;
  std::unordered_map<Layer*, std::vector<Layer*>> _outerBoxShadows;
  std::unordered_map<Layer*, std::vector<Layer*>> _textBackgrounds;
  std::function<HTMLBackgroundImage(const std::string&)> _backgroundImageResolver;
};

}  // namespace pagx
