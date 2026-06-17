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

class FontConfig;

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

    /**
     * When true (default), the parsed DOM is run through `HTMLSubsetTransformer` before
     * traversal. The transformer normalises the document skeleton, resolves `<style>` cascade
     * into inline styles, drops disallowed properties, and prunes unsupported elements so the
     * importer only ever sees subset-compliant HTML. Disable for debugging or when you have
     * already pre-normalised the input.
     */
    bool autoNormalize = true;

    /**
     * When true, the auto-normalizer also runs the `AbsoluteToFlexInference` pass, which
     * recovers `display: flex` semantics from a tree where every visual element is
     * `position: absolute` with explicit `left/top/width/height` (the canonical output of
     * `tools/html-snapshot/snapshot.js`). Containers whose children form a clean 1D row or
     * column with consistent gaps and uniform alignment are rewritten into flex; containers
     * that don't admit a clean inference are left untouched. Default false (lossy
     * heuristic — opt in explicitly). No effect when `autoNormalize` is false.
     */
    bool inferFlexFromAbsolute = false;

    /**
     * Optional FontConfig seeded into the produced document's `fontConfig()`. When provided,
     * the entire config (registered typefaces, deferred fallback fonts, raw fallback typefaces)
     * is copied onto the new document before traversal begins, so the caller's custom fonts
     * are available at layout time. The importer then layers the concrete family names it
     * discovers in CSS `font-family` stacks on top as additional deferred fallbacks (so
     * per-glyph fallback in `LayoutContext` can still find them). Pass nullptr (default) to
     * start the document with an empty font config — the importer will still populate it with
     * the discovered fallback families.
     *
     * Note: only the contents at call time are copied; the pointer does not need to outlive
     * the returned document.
     */
    const FontConfig* fontConfig = nullptr;

    /**
     * Anchor directory used to resolve relative `<img src="...">` paths when parsing from
     * in-memory bytes (`Parse(const uint8_t*, size_t, ...)` / `ParseString`). The file-based
     * `Parse(const std::string&, ...)` overload derives this from the input file's parent
     * directory and ignores this option. Empty (default) means "no base path"; relative
     * sources then pass through unchanged, which only works when every `<img>` already uses
     * an absolute path or `data:` URI (the canonical output of `tools/html-snapshot`).
     */
    std::string basePath = {};

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
   * Parses an HTML string and creates a PAGX Document. The content is treated as a UTF-8
   * encoded byte sequence (matching `XMLDOM::Make`); embedded NUL bytes are permitted as long
   * as the underlying XML parser accepts them. Other encodings (UTF-16, GBK, …) must be
   * transcoded to UTF-8 before being passed in.
   */
  static std::shared_ptr<PAGXDocument> ParseString(const std::string& htmlContent,
                                                   const Options& options = Options());
};

}  // namespace pagx
