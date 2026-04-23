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
 * Export options for HTMLExporter.
 */
struct HTMLExportOptions {
  /**
   * Indentation spaces for the output HTML. The default value is 2.
   */
  int indent = 2;

  /**
   * Absolute directory path where the exporter writes PNG files for shapes filled with color
   * sources that CSS cannot express natively (DiamondGradient and tiled ImagePattern). The
   * exporter creates the directory if it does not exist. When left empty, such shapes are
   * silently skipped in the HTML output.
   */
  std::string staticImgDir = {};

  /**
   * URL prefix used for <img src="..."> references to the generated static images, relative to
   * the HTML document's location. The default is "static-img/", matching the convention that
   * static-img/ sits next to the HTML file.
   */
  std::string staticImgUrlPrefix = "static-img/";

  /**
   * Filename prefix applied to every generated static image, useful when multiple HTML documents
   * share a single static-img directory and need to avoid name collisions (e.g., "doc_name-").
   */
  std::string staticImgNamePrefix = {};

  /**
   * Device pixel ratio used when rasterizing static images. A value of 2 produces @2x assets,
   * which keeps visual parity with the browser's 2x comparison output. The default is 2.
   */
  float staticImgPixelRatio = 2.0f;

  /**
   * When true, all inline `style="..."` attributes are consolidated into a
   * single internal stylesheet placed in the document <head>, and elements
   * reference styles via generated class names (`.ps0`, `.ps1`, ...). This
   * typically reduces HTML size by 10-25% for documents with repeated style
   * declarations. When false, every style remains inline.
   *
   * The document's <body> `style` attribute is always kept inline because it
   * is unique per document and gains nothing from extraction.
   *
   * The default is true.
   */
  bool extractStyleSheet = true;
};

/**
 * HTMLExporter converts a PAGXDocument into HTML/CSS/SVG format.
 * The output is a standalone HTML fragment that renders the PAGX content visually
 * equivalent to the native PAGX renderer in modern browsers.
 */
class HTMLExporter {
 public:
  using Options = HTMLExportOptions;

  /**
   * Exports a PAGXDocument to an HTML string. The output is a standalone HTML fragment containing
   * CSS styles and SVG elements that visually reproduce the PAGX content in modern browsers.
   * @param document The PAGX document to export.
   * @param options Export options controlling output formatting.
   * @return The generated HTML string, or an empty string if the document has no content.
   */
  static std::string ToHTML(const PAGXDocument& document, const Options& options = {});

  /**
   * Exports a PAGXDocument to an HTML file. Creates parent directories if they do not exist.
   * @param document The PAGX document to export.
   * @param filePath The output file path.
   * @param options Export options controlling output formatting.
   * @return True on success, false if the file could not be written.
   */
  static bool ToFile(const PAGXDocument& document, const std::string& filePath,
                     const Options& options = {});
};

}  // namespace pagx
