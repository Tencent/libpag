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
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLSvgFilterDecoder.h"

namespace pagx {

struct DOMNode;
struct Color;
class ColorSource;
class Element;
class Fill;
class HTMLDiagnosticSink;
class HTMLInlineSvgEmitter;
class HTMLValueParser;
class Layer;
class PAGXDocument;

/**
 * Applies typed `HTMLBoxAttributes` onto PAGX `Layer` instances and synthesises the geometry /
 * fill / stroke / shadow chain needed for HTML containers and replaced elements.
 *
 * The builder is layer-side: its public surface treats elements as opaque DOM nodes (used only
 * for `data-*` attribute forwarding in `applyLayerAttributes`) and never decides which element
 * becomes which kind of layer — that classification stays in `HTMLParserContext`. Borrows the
 * importer's diagnostic sink and value parser; the document handle is bound via `bindDocument`
 * after `PAGXDocument::Make` runs.
 */
class HTMLLayerBuilder {
 public:
  HTMLLayerBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                   HTMLInlineSvgEmitter& svgEmitter);

  void bindDocument(PAGXDocument* document);

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
   *  backdrop filter). Returns true when any visual was emitted. */
  bool applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box);

  /** Applies the resolved CSS `transform` (incl. `transform-origin`) onto `layer->matrix`. */
  void applyBoxTransform(Layer* layer, const HTMLBoxAttributes& box,
                         const std::shared_ptr<DOMNode>& element);

  /** Layer-level attributes that don't depend on background: opacity, blend mode, filter
   *  chain, overflow clip, data-* attributes. */
  void applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                            const HTMLBoxAttributes& box);

  /** Builds a `Fill` chain whose colour is a single solid `color`. */
  Fill* buildSolidFill(const Color& color);

  /** Resolves a CSS gradient string into a registered gradient node. `boxWidth` / `boxHeight`
   *  are the painted box size in px, used to normalise radial-gradient size/position descriptors;
   *  pass NaN when unknown. */
  ColorSource* parseGradientByValue(const std::string& value, float boxWidth = NAN,
                                    float boxHeight = NAN);

  // Inner host / wrappers --------------------------------------------------------------
  /** Returns true when the box carries any visual that requires a Rectangle/Fill/Stroke
   *  chain on the outer Layer (used to decide whether a double-host split is needed). */
  static bool hasBackgroundVisuals(const HTMLBoxAttributes& box);

  /** Returns true when an inner host Layer is needed (the outer carries the background and
   *  the inner carries padding / layout). */
  static bool requiresInnerHost(const HTMLBoxAttributes& box);

  /** Creates the inner host Layer for the standard "outer background + inner padded
   *  container" double-layer pattern. */
  Layer* createInnerHost(Layer* outer, const HTMLBoxAttributes& box);

  /** Hoists `DropShadowStyle` entries off `inner` onto a fresh outer wrapper Layer when
   *  `inner` would also clip its bounds. See the original docs in `HTMLParserContext` for
   *  the full rationale. Returns `inner` unchanged when no split is needed. */
  Layer* maybeSplitBoxShadowFromClip(Layer* inner);

  /** Materialises CSS `margin` on `inner` through positioning / padding (PAGX has no
   *  margin concept on Layer). Returns `inner` unchanged when every side is zero. */
  Layer* wrapForMargin(Layer* inner, const HTMLBoxAttributes& box);

  /** Emits a single text-decoration line (underline or line-through) onto `host`. */
  void emitTextDecorationLine(Layer* host, const Color& textColor, const Color& decorationColor,
                              bool decorationColorDiffers, float bottom, float centerY);

 private:
  void applyBackgroundFill(Layer* layer, const HTMLBoxAttributes& box, Element* geometry,
                           bool& emitted);
  void applyBorderStroke(Layer* layer, const HTMLBoxAttributes& box, Element* geometry,
                         bool& emitted);
  void applyBoxShadows(Layer* layer, const HTMLBoxAttributes& box, bool& emitted);
  void applyBackdropFilter(Layer* layer, const HTMLBoxAttributes& box, bool& emitted);

  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  HTMLSvgFilterDecoder _filterDecoder;
  PAGXDocument* _document = nullptr;
};

}  // namespace pagx
