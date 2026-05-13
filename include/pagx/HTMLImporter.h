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
#include <memory>
#include <string>
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * HTMLImporter converts a restricted subset of HTML/CSS to a PAGX Document.
 *
 * The accepted subset is documented in `spec/html_subset.md`. The importer is designed to
 * be the bridge between LLM-generated visual designs (HTML being the format LLMs are
 * fluent in) and the PAGX canonical format used by the rest of the pipeline. The mapping
 * is engineered so that, when the input follows the subset, the produced PAGX is close to
 * what the PAGX skill would have authored by hand and can be polished further with PAGX
 * editing tools (MCP, `pagx` CLI) or exported losslessly to other PAGX-compatible
 * targets.
 *
 * Input that uses constructs outside the subset is accepted on a best-effort basis: the
 * offending elements/properties are skipped or downgraded and a diagnostic is emitted via
 * `PAGXDocument::errors` (CLI surfaces these as warnings).
 */
class HTMLImporter {
 public:
  struct Options {
    /**
     * Target document width. When not NaN, overrides the canvas size implied by the HTML
     * (`<body style="width:…">` or the body's intrinsic content size). Both targetWidth
     * and targetHeight must be set (non-NaN) to take effect.
     */
    float targetWidth = NAN;

    /**
     * Target document height. See targetWidth.
     */
    float targetHeight = NAN;

    /**
     * When true, the canvas size declared by `<body>` is preferred over `targetWidth`
     * / `targetHeight`. Default false (caller-provided target wins).
     */
    bool preferBodySize = false;

    /**
     * When true, treats unsupported elements/properties as hard errors. Default false
     * (downgrade with a warning).
     */
    bool strict = false;

    /**
     * When true, unknown HTML tags are kept as empty Layers tagged
     * `data-html-unknown="<tag>"` (useful when debugging). Default false (skip).
     */
    bool preserveUnknownElements = false;

    Options() {
    }
  };

  /**
   * Parses an HTML file and creates a PAGX Document. Relative image paths in `<img>`
   * elements are resolved against the file's parent directory.
   */
  static std::shared_ptr<PAGXDocument> Parse(const std::string& filePath,
                                             const Options& options = Options());

  /**
   * Parses HTML data and creates a PAGX Document.
   */
  static std::shared_ptr<PAGXDocument> Parse(const uint8_t* data, size_t length,
                                             const Options& options = Options());

  /**
   * Parses an HTML string and creates a PAGX Document.
   */
  static std::shared_ptr<PAGXDocument> ParseString(const std::string& htmlContent,
                                                   const Options& options = Options());
};

}  // namespace pagx
