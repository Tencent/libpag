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
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

class FontConfig;

/**
 * Export options for SVGExporter.
 */
struct SVGExportOptions {
  /**
   * Indentation spaces for the output SVG. The default value is 2.
   */
  int indent = 2;

  /**
   * Whether to include the XML declaration (<?xml ...?>) at the top. The default value is true.
   */
  bool xmlDeclaration = true;

  /**
   * Whether to convert text elements to path elements using pre-shaped glyph outlines. When
   * enabled, text with GlyphRun data is rendered as SVG <path> elements instead of <text> elements,
   * ensuring identical rendering across platforms without font dependency. Falls back to <text>
   * elements when glyph outline data is unavailable. The default value is false.
   *
   * Note: the default changed from true to false. Downstream callers that want
   * the previous outline-based output must now opt in explicitly by setting
   * this field to true.
   */
  bool convertTextToPath = false;

  /**
   * Optional FontConfig for text layout. When provided, registered and fallback fonts from the
   * FontConfig are used for text shaping and layout. When nullptr, the exporter falls back to
   * system fonts via platform-native font lookup.
   */
  FontConfig* fontConfig = nullptr;

  /**
   * Whether to resolve path-modifier elements (TrimPath, RoundCorner, MergePath, Polystar) into
   * editable geometry by applying the modifier through tgfx::Path operations and emitting the
   * resulting path as a regular <path>. When disabled, modifiers are dropped on the floor. When
   * enabled, the modifier itself is no longer live but the resulting shape remains a fully editable
   * SVG path. Naming is aligned with PPTExportOptions::resolveModifiers. The default value is true.
   */
  bool resolveModifiers = true;

  /**
   * Whether to bake layers that carry features SVG cannot losslessly represent (TextPath,
   * TextModifier, diamond/conic gradient) into embedded PNG patches. When true, such a layer is
   * baked to an embedded PNG and emitted as an &lt;image&gt; so the visual result is preserved.
   * When false, the exporter falls through to the vector path and those features degrade silently
   * (TextPath / TextModifier drop, diamond/conic gradient fall back to radial). BackgroundBlurStyle
   * is always kept as vector output with the blur effect silently dropped, since SVG has no
   * portable backdrop-blur primitive. Naming is aligned with PPTExportOptions::bakeUnsupported.
   * The default value is true.
   */
  bool bakeUnsupported = true;

  /**
   * Oversampling factor applied when a layer or tiled pattern has to be baked to PNG. The Surface
   * behind every bake is sized at `ceil(logicalSize * rasterScale)` physical pixels, while the
   * placed &lt;image&gt; keeps using logical dimensions so consumers stretch the denser bitmap
   * over the same visible extent, yielding a crisper result.
   *
   * Valid range: (0, 4]. Values outside this range are clamped at the entry point — passing 0 or
   * a negative value would otherwise silently disable rasterization (the bake would produce a
   * zero-pixel surface and the exporter would fall through to the vector path), contradicting the
   * intent of `bakeUnsupported = true`. The default is 2.0 (i.e. @2x assets), matching the
   * default of HTMLExportOptions::rasterScale.
   */
  float rasterScale = 2.0f;

  /**
   * Whether to embed vector Font resources as WOFF2 @font-face rules with base64 data URIs and
   * render their Text elements via &lt;text&gt; with PUA Unicode characters. When enabled, Text
   * nodes whose GlyphRun references an embeddable vector Font become real &lt;text&gt; elements —
   * selectable, searchable, and animatable per character — instead of opaque outline &lt;path&gt;
   * elements. Bitmap (CBDT) fonts and GlyphRuns that carry per-glyph scales / skews remain on the
   * outline path because plain SVG &lt;text&gt; cannot express them. When disabled, every Text
   * with GlyphRun data is emitted as &lt;path&gt; (the legacy behaviour). Has no effect when
   * `convertTextToPath` is true (the user has explicitly requested outline geometry). The default
   * value is true.
   */
  bool embedFontsAsWoff2 = true;
};

/**
 * SVGExporter converts a PAGXDocument into SVG format.
 * This is the reverse of SVGImporter: it maps PAGX node types back to SVG elements.
 */
class SVGExporter {
 public:
  using Options = SVGExportOptions;

  /**
   * Exports a PAGXDocument to an SVG string.
   * @param document the PAGXDocument to export. Passed as non-const because applyLayout() is run
   *        on first use to resolve renderPosition() for layers and groups.
   * @param options export options controlling text rendering, modifier resolution, and rasterizer
   *        behaviour.
   * @param warnings optional pointer to receive human-readable warnings whenever a layer or asset
   *        had to be degraded — image data could not be loaded, a tiled pattern bake failed, a
   *        layer rasterization failed, or an SVG-unsupported feature was emitted as vector
   *        because `bakeUnsupported` is false. Useful for CI / batch pipelines to detect silent
   *        degradation. May be nullptr to suppress collection.
   */
  static std::string ToSVG(PAGXDocument& document, const Options& options = {},
                           std::vector<std::string>* warnings = nullptr);

  /**
   * Exports a PAGXDocument to an SVG file.
   * @param document the PAGXDocument to export. Passed as non-const because applyLayout() is run
   *        on first use to resolve renderPosition() for layers and groups.
   * @param warnings same semantics as ToSVG(); see that method for details.
   * @return true on success.
   */
  static bool ToFile(PAGXDocument& document, const std::string& filePath,
                     const Options& options = {},
                     std::vector<std::string>* warnings = nullptr);
};

}  // namespace pagx
