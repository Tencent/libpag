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

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include "pagx/nodes/ColorStop.h"
#include "pagx/types/Color.h"

namespace pagx {

class ColorSource;
class ConicGradient;
class HTMLDiagnosticSink;
class LinearGradient;
class PAGXDocument;
class RadialGradient;

/**
 * Parses CSS string values into typed PAGX values.
 *
 * The parser is per-import: it borrows the importer's diagnostic sink, document handle and
 * canvas dimensions, all of which are populated incrementally during `HTMLParserContext::parseDOM`.
 * `canvasWidth` / `canvasHeight` are kept as `const float&` so that vw / vh resolution sees the
 * value as soon as `resolveCanvasSize` writes it; `document` is a deferred-bind pointer because
 * gradient nodes have to be allocated through `PAGXDocument::makeNode<>()`.
 */
class HTMLValueParser {
 public:
  /**
   * Container for a single resolved `box-shadow` / `drop-shadow` entry. `inset` distinguishes
   * inner shadows. CSS `spread` is intentionally not modelled — PAGX has no spread axis on its
   * shadow styles, so the parser emits a diagnostic when a non-zero spread is present and drops
   * the value rather than carrying a field downstream consumers cannot honour.
   */
  struct ShadowSpec {
    float offsetX = 0;
    float offsetY = 0;
    float blur = 0;
    Color color = {};
    bool inset = false;
  };

  /**
   * One step of a CSS `filter` / `backdrop-filter` chain. `SvgRef` carries a `url(#id)`
   * reference to an SVG `<filter>` def, whose `refId` the caller resolves through the
   * shared-defs table. `ColorMatrix` carries the 4x5 (20-element, row-major, normalised colour
   * space) transform that the colour-adjustment functions — `brightness`, `contrast`, `saturate`,
   * `grayscale`, `sepia`, `invert`, `opacity`, `hue-rotate` — resolve to. `Unsupported` is emitted
   * for anything outside the supported subset; the caller can surface a diagnostic using `raw`.
   */
  struct FilterStep {
    enum class Kind { Blur, DropShadow, SvgRef, ColorMatrix, Unsupported };
    Kind kind = Kind::Unsupported;
    float blurX = 0;
    float blurY = 0;
    ShadowSpec shadow = {};
    std::string refId = {};
    std::array<float, 20> matrix = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    std::string raw = {};
  };

  using GradientStops = std::vector<std::pair<float, Color>>;

  HTMLValueParser(HTMLDiagnosticSink& sink, const float& canvasWidth, const float& canvasHeight);

  /** Wires the document handle so gradient nodes can be allocated. May be called once after the
   *  document has been constructed; subsequent calls overwrite the stored pointer. */
  void bindDocument(PAGXDocument* document);

  Color parseColor(const std::string& valueRaw);
  float parseAbsoluteLengthPx(const std::string& valueRaw);
  float resolveLineHeightPx(const std::string& valueRaw, float fontSizePx);

  std::vector<ShadowSpec> parseShadowList(const std::string& value);
  std::vector<FilterStep> parseFilterChain(const std::string& value);

  /** Parses a CSS `linear-gradient(...)`. `boxWidth` / `boxHeight` are the painted box size in px;
   *  when both are known the gradient line is resolved in absolute pixel space (using the CSS
   *  magic-corner extent) with `fitsToGeometry=false`, so the equal-color lines stay perpendicular
   *  on any aspect ratio. Pass NaN when the box size is unknown to keep the geometry-normalised
   *  (0,0)-(1,1) space, which is only exact for axis-aligned gradients. When `repeating` is set
   *  (CSS `repeating-linear-gradient`) and the box size is known, the authored period of stops is
   *  tiled across the whole gradient line as explicit color stops (PAGX gradients have no native
   *  spread/repeat mode); the box-unknown case degrades to a single non-repeating period. */
  LinearGradient* parseLinearGradient(const std::string& value, float boxWidth = NAN,
                                      float boxHeight = NAN, bool repeating = false);
  /**
   * Builds a `repeating-linear-gradient(...)` as a tiled `ImagePattern` rather than a gradient.
   * PAGX/tgfx gradients bake >16 stops into a fixed 256px color texture, so a fine repeat (e.g. a
   * 2px ruled line every 49px) undersamples and drops most lines. A one-period tile rendered at
   * native resolution and repeated reproduces the pattern crisply at any density. Returns nullptr
   * when the box size is unknown, the gradient direction is not axis-aligned (only horizontal /
   * vertical patterns tile seamlessly with a 1D strip), or the period is degenerate — the caller
   * then falls back to the coarser gradient-stop tiling of `parseLinearGradient`.
   */
  ColorSource* parseRepeatingLinearGradientPattern(const std::string& value, float boxWidth,
                                                   float boxHeight);
  /** Parses a CSS `radial-gradient(...)`. `boxWidth` / `boxHeight` are the painted box size in px
   *  used to normalise the `<size> at <position>` descriptor back into the gradient's (0,0)-(1,1)
   *  geometry space; pass NaN when unknown to keep the centered, box-filling default. `repeating`
   *  (CSS `repeating-radial-gradient`) tiles the authored period across the normalised radius. */
  RadialGradient* parseRadialGradient(const std::string& value, float boxWidth = NAN,
                                      float boxHeight = NAN, bool repeating = false);
  /** Parses a CSS `conic-gradient(...)`. `repeating` (CSS `repeating-conic-gradient`) tiles the
   *  authored angular period across the full turn. */
  ConicGradient* parseConicGradient(const std::string& value, bool repeating = false);

  /** Parses the comma-separated tail of a gradient call into (offset, color) pairs. Offsets that
   *  fail to parse are left as NaN for `finaliseGradientStops` to fill in. */
  GradientStops parseGradientStops(const std::vector<std::string>& parts, size_t startIndex,
                                   bool interpretAngularOffset);

  /** Fills NaN offsets with sensible defaults (first/last → 0/1, intermediate gaps spread
   *  evenly). Returns false when the list is empty. */
  static bool finaliseGradientStops(GradientStops& stops);

  /** Appends the resolved stops onto a gradient node's `colorStops` field. */
  template <typename T>
  void emitColorStops(T& targetStops, const GradientStops& stops);

 private:
  // Builds the tiled color stops of a `repeating-linear-gradient` in the gradient line's
  // normalised (0..1) space. One authored period is parsed into pixel positions along the line
  // (`lineLength` px), then repeated forward until the far edge is covered. Returns false when the
  // stops cannot form a repeating pattern (fewer than two stops or a non-positive period), so the
  // caller can fall back to a non-repeating gradient.
  bool buildRepeatingLinearStops(const std::vector<std::string>& parts, size_t stopStart,
                                 float lineLength, GradientStops& out);
  // Tiles one authored period of `stops` (already expressed in a normalised 0..1 domain, with the
  // last stop's offset defining the period) across the full [0,1] range, replacing `stops` with
  // the expanded list. Used for the angular (conic) and radial repeating variants. Returns false
  // when the stops cannot form a repeating pattern.
  bool tileRepeatingStops(GradientStops& stops);
  // Parses a radial-gradient leading descriptor ("circle 50px at 50px 50px") and writes the
  // recovered center / radius onto `grad`, normalised against the box size.
  void parseRadialDescriptor(const std::string& descriptor, float boxWidth, float boxHeight,
                             RadialGradient* grad);
  // Resolves a single radial size/position length token to a value normalised against `boxAxis`.
  // Handles px lengths and `<pct>%`; returns NaN when the token is not a length.
  float resolveRadialLength(const std::string& token, float boxAxis);

  HTMLDiagnosticSink& _diagnostics;
  PAGXDocument* _document = nullptr;
  const float& _canvasWidth;
  const float& _canvasHeight;
};

}  // namespace pagx
