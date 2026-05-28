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
   * inner shadows; `spread` is parsed but unsupported by PAGX (a diagnostic is emitted when
   * non-zero).
   */
  struct ShadowSpec {
    float offsetX = 0;
    float offsetY = 0;
    float blur = 0;
    float spread = 0;
    Color color = {};
    bool inset = false;
  };

  /**
   * One step of a CSS `filter` / `backdrop-filter` chain. `Unsupported` is emitted for
   * anything outside the {`blur`, `drop-shadow`} subset; the caller can surface a diagnostic
   * using `raw`.
   */
  struct FilterStep {
    enum class Kind { Blur, DropShadow, Unsupported };
    Kind kind = Kind::Unsupported;
    float blurX = 0;
    float blurY = 0;
    ShadowSpec shadow = {};
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

  LinearGradient* parseLinearGradient(const std::string& value);
  RadialGradient* parseRadialGradient(const std::string& value);
  ConicGradient* parseConicGradient(const std::string& value);

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
  HTMLDiagnosticSink& _diagnostics;
  PAGXDocument* _document = nullptr;
  const float& _canvasWidth;
  const float& _canvasHeight;
};

}  // namespace pagx
