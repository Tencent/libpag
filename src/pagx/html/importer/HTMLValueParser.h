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
#include <utility>
#include <vector>
#include "pagx/nodes/ColorStop.h"
#include "pagx/types/Color.h"

namespace pagx {

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
   * shared-defs table. `Unsupported` is emitted for anything outside the
   * {`blur`, `drop-shadow`, `url(#…)`} subset; the caller can surface a diagnostic using `raw`.
   */
  struct FilterStep {
    enum class Kind { Blur, DropShadow, SvgRef, Unsupported };
    Kind kind = Kind::Unsupported;
    float blurX = 0;
    float blurY = 0;
    ShadowSpec shadow = {};
    std::string refId = {};
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
   *  (0,0)-(1,1) space, which is only exact for axis-aligned gradients. `repeating` marks a
   *  `repeating-linear-gradient(...)` being downgraded to a single gradient; see
   *  `appendRepeatingReset`. */
  LinearGradient* parseLinearGradient(const std::string& value, float boxWidth = NAN,
                                      float boxHeight = NAN, bool repeating = false);
  /** Parses a CSS `radial-gradient(...)`. `boxWidth` / `boxHeight` are the painted box size in px
   *  used to normalise the `<size> at <position>` descriptor back into the gradient's (0,0)-(1,1)
   *  geometry space; pass NaN when unknown to keep the centered, box-filling default. `repeating`
   *  marks a `repeating-radial-gradient(...)` being downgraded; see `appendRepeatingReset`. */
  RadialGradient* parseRadialGradient(const std::string& value, float boxWidth = NAN,
                                      float boxHeight = NAN, bool repeating = false);
  ConicGradient* parseConicGradient(const std::string& value);

  /** Parses the comma-separated tail of a gradient call into (offset, color) pairs. Offsets that
   *  fail to parse are left as NaN for `finaliseGradientStops` to fill in. `pxOffsetScale` is the
   *  gradient's extent in px (line length for linear, radius for radial): a px stop offset is an
   *  absolute distance along the gradient ray, so it is divided by this to recover the normalised
   *  [0,1] offset PAGX color stops use. Pass NaN (the default) when the extent is unknown to keep
   *  the raw px value. */
  GradientStops parseGradientStops(const std::vector<std::string>& parts, size_t startIndex,
                                   bool interpretAngularOffset, float pxOffsetScale = NAN);

  /** Fills NaN offsets with sensible defaults (first/last → 0/1, intermediate gaps spread
   *  evenly). Returns false when the list is empty. */
  static bool finaliseGradientStops(GradientStops& stops);

  /** PAGX gradient nodes have no tile/repeat axis, so a `repeating-*-gradient(...)` is downgraded
   *  to a single gradient. Left alone, the region past the last stop would flood the whole box with
   *  the last color (e.g. a subtle 4px scanline overlay turns into a solid dark wash). Each CSS tile
   *  instead resets to the first color at the period boundary, so this appends a sharp reset back to
   *  the first stop's color just past the last stop, making the pattern render as one period at its
   *  true scale with the base (usually transparent) color filling the rest. */
  static void appendRepeatingReset(GradientStops& stops);

  /** Appends the resolved stops onto a gradient node's `colorStops` field. */
  template <typename T>
  void emitColorStops(T& targetStops, const GradientStops& stops);

 private:
  // The center / radius / coordinate-space recovered from a radial-gradient leading descriptor.
  // Kept as plain locals (rather than a RadialGradient node) so the extent can be resolved before
  // the node is allocated, which lets px stop offsets be normalised against it.
  struct RadialDescriptor {
    float centerX = 0.5f;
    float centerY = 0.5f;
    float radius = 0.5f;
    bool fitsToGeometry = true;
  };
  // Parses a radial-gradient leading descriptor ("circle 50px at 50px 50px") and writes the
  // recovered center / radius into `out`, normalised against the box size.
  void parseRadialDescriptor(const std::string& descriptor, float boxWidth, float boxHeight,
                             RadialDescriptor& out);
  // Resolves a single radial size/position length token to a value normalised against `boxAxis`.
  // Handles px lengths and `<pct>%`; returns NaN when the token is not a length.
  float resolveRadialLength(const std::string& token, float boxAxis);

  HTMLDiagnosticSink& _diagnostics;
  PAGXDocument* _document = nullptr;
  const float& _canvasWidth;
  const float& _canvasHeight;
};

}  // namespace pagx
