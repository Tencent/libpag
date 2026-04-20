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
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * Export options for PPTExporter.
 */
struct PPTExportOptions {
  /**
   * Whether to convert text elements to path geometry using pre-shaped glyph outlines. When
   * enabled, text with GlyphRun data is rendered as custom geometry paths instead of native PPTX
   * text runs, ensuring identical rendering across platforms without font dependency. Falls back to
   * native text when glyph outline data is unavailable. The default value is false.
   */
  bool convertTextToPath = false;

  /**
   * Whether to rasterize masked layers into bitmap images. When enabled, layers with masks are
   * rendered to PNG and embedded as picture elements. When disabled, the layer content is exported
   * as editable vector shapes and the mask effect is ignored. The default value is true.
   */
  bool bakeMask = true;

  /**
   * Whether to rasterize tiled image patterns into bitmap images. When enabled, ImagePattern fills
   * with repeat or mirror tile modes are pre-rendered at the shape size and embedded as stretched
   * images, ensuring identical rendering across PowerPoint and Keynote regardless of DPI settings.
   * When disabled, the native OOXML a:tile mechanism is used, which may produce inconsistent tile
   * scaling across applications. The default value is true.
   */
  bool bakeTiledPattern = true;

  /**
   * Whether to bridge nested contours within a single path element. When enabled, contours that
   * contain inner holes are connected by bridge edges so the hole is expressed as a single
   * self-intersecting sub-path, which some renderers require for correct even-odd fill. When
   * disabled, each contour is emitted as a separate sub-path inside the path list. The default
   * value is true.
   */
  bool bridgeContours = true;

  /**
   * Whether to resolve path-modifier elements (TrimPath, RoundCorner, MergePath, Polystar) into
   * editable custom geometry by applying the modifier through tgfx::Path operations and emitting
   * the resulting path as a:custGeom. When disabled, modifiers are dropped on the floor (matching
   * V1 behavior). When enabled, the modifier itself is no longer live but the resulting shape
   * remains a fully editable PPT shape. The default value is true.
   */
  bool resolveModifiers = true;

  /**
   * Whether to rasterize layers that use blend modes outside of Normal/Multiply/Screen/Darken/
   * Lighten (the only ones expressible in OOXML a:fillOverlay). When false, the blend mode is
   * silently dropped and the layer is rendered as Normal. The default value is true.
   */
  bool rasterizeUnsupportedBlend = true;

  /**
   * Whether to rasterize layers whose color sources reference wide-gamut color spaces (DisplayP3,
   * etc.) so the visual appearance is preserved. OOXML colors are always sRGB. When false, the
   * wide-gamut color is clamped to sRGB and rendered as a solid fill. The default value is true.
   */
  bool rasterizeWideGamut = true;

  /**
   * Pixel DPI used when a layer has to be rasterized to PNG. Higher values give crisper fallback
   * images at the cost of file size. The default value is 192 (2x of the default 96 DPI).
   */
  int rasterDPI = 192;
};

/**
 * PPTExporter converts a PAGXDocument into PPTX (PowerPoint) format.
 * All PAGX layers are placed in a single slide.
 */
class PPTExporter {
 public:
  using Options = PPTExportOptions;

  /**
   * Exports a PAGXDocument to a PPTX file at the specified path.
   * @param document the PAGXDocument to export. Passed as non-const because internal layout
   *        computation may cache intermediate results.
   * @param filePath the output file path. The file will be created or overwritten.
   * @param options export options controlling text rendering and mask handling.
   * @return true if the PPTX file was written successfully, false if the file could not be created
   *         or a write error occurred.
   */
  static bool ToFile(PAGXDocument& document, const std::string& filePath,
                     const Options& options = {});
};

}  // namespace pagx
