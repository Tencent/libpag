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
 * Output framework for HTML export.
 */
enum class HTMLFramework {
  Native,  // Standard HTML output
  React,   // React JSX output
  Vue,     // Vue 3 SFC output
};

/**
 * Export options for HTMLExporter.
 */
struct HTMLExportOptions {
  /**
   * Indentation spaces for the output HTML. The default value is 2.
   */
  int indent = 2;

  /**
   * Output framework format. The default is Native HTML.
   */
  HTMLFramework framework = HTMLFramework::Native;

  /**
   * Component name for React/Vue output. The default is "PagxComponent".
   * This option is only used when framework is React or Vue.
   */
  std::string componentName = "PagxComponent";
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
