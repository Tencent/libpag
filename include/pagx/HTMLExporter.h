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
 *
 * ABI stability: this struct is part of the public API. New optional fields must be appended at
 * the end with sensible defaults so that callers compiled against an older header continue to
 * link and behave correctly. Reordering, removing, or changing the type of an existing field is
 * a breaking change.
 */
struct HTMLExportOptions {
  /**
   * Oversampling factor applied when rasterizing static images. The exporter sizes the
   * resulting PNG at `ceil(cssSize * rasterScale)` physical pixels while the HTML still
   * references it at the original CSS size, so high-DPI screens can use the extra pixels
   * for crisper rendering. The default is 2 (i.e. @2x assets), matching the typical
   * browser comparison output. This is unrelated to the runtime device pixel ratio — it
   * is a build-time decision baked into the PNG and cannot adapt to the viewer's display.
   * Valid range: (0, 4]. Values outside this range are clamped.
   */
  float rasterScale = 2.0f;

  /**
   * When true, all inline `style="..."` attributes are consolidated into a
   * single internal stylesheet placed in the document <head>, and elements
   * reference styles via generated class names with semantic prefixes derived
   * from element tag names (e.g., `.blend0`, `.layer1`, `.text2`, `.bg3`).
   * This typically reduces HTML size by 10-25% for documents with repeated
   * style declarations. When false, every style remains inline.
   *
   * The document's <body> `style` attribute is always kept inline because it
   * is unique per document and gains nothing from extraction.
   *
   * The default is true.
   */
  bool extractStyleSheet = true;

  /**
   * When true, `ToFile` and `ToHTML` produce a complete HTML document (with `<!DOCTYPE html>`,
   * `<html>`, `<head>`, and `<body>` tags) that can be opened directly in a browser. When false,
   * only the inner HTML fragment is returned — callers are responsible for wrapping it.
   *
   * The default is true for `ToFile` (the most common use case is producing a standalone file)
   * and false for `ToHTML` (the caller usually needs the fragment for embedding).
   *
   * Note: when this is not set explicitly, `ToFile` defaults to true and `ToHTML` defaults to
   * false. Setting it explicitly overrides the per-method default. Use the named constants
   * below (WrapDefault / WrapOff / WrapOn) instead of raw integer literals.
   */
  static constexpr int WrapDefault = -1;
  static constexpr int WrapOff = 0;
  static constexpr int WrapOn = 1;
  int wrapFullDocument = WrapDefault;
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
   *
   * Resource directory contract: `resourceDir` is a mandatory absolute directory path where the
   * exporter writes auxiliary image files. Two kinds of files land there:
   *   1. PNGs rasterized from shapes whose color sources CSS cannot express natively
   *      (DiamondGradient, tiled ImagePattern, PlusDarker backdrops).
   *   2. Copies of external image files referenced by Image resources via `filePath`.
   * The exporter creates the directory if it does not exist. The generated HTML references these
   * assets via a relative URL whose prefix is the basename of `resourceDir` followed by `/`
   * (e.g. `resourceDir = "/tmp/abc"` produces `<img src="abc/dgc0.png">`). Callers must write
   * the returned HTML string to a file located in `resourceDir`'s parent directory for those
   * relative URLs to resolve; ToFile enforces this layout automatically.
   *
   * Passing an empty `resourceDir` is treated as a usage error: the exporter logs an error to
   * stderr and returns an empty string. Use ToFile if you want resource-directory derivation
   * handled for you.
   *
   * Precondition: the exporter reads layout-resolved geometry (positions, sizes, text runs)
   * produced by PAGXDocument::applyLayout. If `document.isLayoutApplied()` returns false when
   * ToHTML is called, the exporter automatically calls `document.applyLayout()` before
   * proceeding — matching the behavior of SVGExporter and PPTExporter.
   *
   * Thread safety: this is a stateless static method and may be called concurrently from
   * multiple threads, provided that each thread passes a distinct document (or a document that
   * is guaranteed not to be mutated for the duration of the call) and a distinct resourceDir
   * (so that concurrent writers do not race on the same filesystem location).
   *
   * Failure semantics: returns an empty string on internal failure, when the document has no
   * visible content, or when `resourceDir` is empty. If `errorMsg` is not null, a human-readable
   * description of the failure is written to `*errorMsg`.
   *
   * Ownership: `document` is passed by non-const reference so the exporter can call applyLayout
   * if needed, but the exporter does not retain any pointer or reference to the document or its
   * internal nodes beyond the duration of this call.
   *
   * @param document The PAGX document to export. Layout is applied automatically if needed.
   * @param resourceDir Absolute directory path for auxiliary PNGs and copied images. Must not
   *                    be empty.
   * @param options Export options controlling output formatting.
   * @param errorMsg Optional pointer to receive a human-readable error description on failure.
   * @return The generated HTML string, or an empty string on failure.
   */
  static std::string ToHTML(PAGXDocument& document, const std::string& resourceDir,
                            const Options& options = {}, std::string* errorMsg = nullptr);

  /**
   * Exports a PAGXDocument to an HTML file. Creates parent directories if they do not exist.
   *
   * Implementation note: this method derives the resource directory automatically as
   * `parent_path(filePath) / stem(filePath)` — i.e. a sibling directory of the HTML file whose
   * name matches the HTML file's stem. For example, `filePath = "/tmp/abc.html"` writes
   * auxiliary assets to `/tmp/abc/` and the HTML references them via `<img src="abc/...">`.
   * Internally this method calls ToHTML with the derived resource directory and writes the
   * resulting string to disk.
   *
   * File handling: the output file is opened with std::ios::binary and truncated; if a file
   * already exists at `filePath` it is overwritten without prompting. `filePath` is used
   * verbatim — the library does not validate, sanitise, or restrict it, so protecting against
   * path traversal and ensuring the location is writable are the caller's responsibility.
   *
   * Precondition, thread safety, failure semantics, and document ownership follow the same
   * rules as ToHTML; see that method for details.
   *
   * @param document The PAGX document to export. Layout is applied automatically if needed.
   * @param filePath The output file path, used verbatim (no validation performed by the
   *                 library).
   * @param options Export options controlling output formatting.
   * @param errorMsg Optional pointer to receive a human-readable error description on failure.
   * @return True on success, false if the file could not be written or the document has no
   *         content.
   */
  static bool ToFile(PAGXDocument& document, const std::string& filePath,
                     const Options& options = {}, std::string* errorMsg = nullptr);
};

}  // namespace pagx
