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
   * Whether to bridge nested contours within a single path element. When enabled, contours that
   * contain inner holes are connected by bridge edges so the hole is expressed as a single
   * self-intersecting sub-path, which some renderers require for correct even-odd fill. When
   * disabled, each contour is emitted as a separate sub-path inside the path list. The default
   * value is false.
   */
  bool bridgeContours = false;
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
