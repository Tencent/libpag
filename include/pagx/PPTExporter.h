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

class FontConfig;

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
   * Whether to rasterize layers that use features OOXML cannot represent natively — masks,
   * scrollRect clipping, blend modes outside of Normal/Multiply/Screen/Darken/Lighten, and
   * wide-gamut color sources (DisplayP3, etc.). When true, the layer is rendered to a PNG patch
   * so the visual result matches the tgfx renderer; for unsupported blend modes the backdrop
   * beneath the layer is baked into the patch as well, turning any native content under it into
   * pixels so the blend composites correctly. When false, these features are silently dropped
   * and the layer is emitted as editable vector shapes (the mask effect is ignored, the
   * scrollRect is dropped, the blend mode falls back to Normal, and wide-gamut colors are
   * clamped to sRGB), preserving full editability at the cost of visual fidelity. Tiled image
   * patterns are always rasterized regardless of this flag because the native OOXML a:tile
   * mechanism produces inconsistent scaling across PowerPoint and Keynote. Features with no
   * meaningful vector fallback (TextPath, TextModifier, ColorMatrix, conic/diamond gradients,
   * shear transforms) are always rasterized regardless of this flag. The default value is false.
   */
  bool rasterizeUnsupported = false;

  /**
   * Pixel DPI used when a layer has to be rasterized to PNG. The Surface behind every bake (masked
   * layer, scrollRect fallback, blend/wide-gamut fallback, tiled pattern) is sized by
   * `rasterDPI / 96` relative to the layer's logical extent, while the placed picture keeps the
   * logical EMU dimensions — the consumer stretches the denser bitmap over the same visible area,
   * giving a crisper result at the cost of file size. The default value is 192 (2x of the default
   * 96 DPI).
   */
  int rasterDPI = 192;

  /**
   * Optional FontConfig for text layout. When provided, registered and fallback fonts from the
   * FontConfig are used for text shaping and measurement, producing accurate text bounding boxes.
   * When nullptr, the exporter falls back to system fonts via platform-native font lookup.
   */
  FontConfig* fontConfig = nullptr;
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
