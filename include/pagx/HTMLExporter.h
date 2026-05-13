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

/**
 * Specifies how a font file is referenced in the generated @font-face rule.
 */
enum class FontEmbedMode {
  /**
   * The uri field is emitted as-is inside a CSS `url()`. Use this for CDN links
   * (e.g. "https://cdn.example.com/font.woff2") or relative paths that will be resolved
   * against the HTML document's location (e.g. "fonts/NotoSansSC-Bold.ttf").
   */
  URL,

  /**
   * The uri field is a local file path. The exporter reads the file, base64-encodes its
   * contents, and emits an inline `url(data:font/...;base64,...)` data URI. This produces
   * a fully self-contained HTML document at the cost of a ~33% size increase for the font data.
   */
  Base64,

  /**
   * The uri field is a local file path that is emitted as a `file://` URL. Useful for local
   * debugging and preview; browsers typically block file:// URLs from non-file:// pages, so
   * this mode is not suitable for production delivery.
   */
  FilePath,
};

/**
 * One entry in a @font-face rule's `src` list. A single FontFaceRule may carry multiple
 * sources; the browser tries them in order and falls back to the next one if the current
 * source fails to load (e.g. local file missing, network error). This enables patterns like
 * "prefer bundled local font, fall back to CDN" in a single declaration.
 */
struct FontFaceSource {
  /**
   * The font source: a URL, relative path, or local file path depending on `mode`.
   */
  std::string uri;

  /**
   * How the uri should be embedded in the generated CSS.
   */
  FontEmbedMode mode = FontEmbedMode::URL;
};

/**
 * Describes a single @font-face rule to inject into the HTML output.
 */
struct FontFaceRule {
  /**
   * The CSS font-family name to declare (e.g. "Noto Sans SC"). Must match the font-family
   * names used by the PAGX document's Text nodes.
   */
  std::string fontFamily;

  /**
   * One or more font sources. Browsers try each entry in order — typical usage lists a local
   * bundled file first and a CDN URL as fallback. Must contain at least one source; rules
   * with an empty sources list are skipped during CSS emission.
   */
  std::vector<FontFaceSource> sources;

  /**
   * Optional CSS font-weight value (e.g. "400", "700", "bold"). When empty, the property is
   * omitted and the browser defaults to "normal" (400).
   */
  std::string fontWeight;

  /**
   * Optional CSS font-style value (e.g. "normal", "italic"). When empty, the property is
   * omitted and the browser defaults to "normal".
   */
  std::string fontStyle;

  /**
   * Optional CSS `unicode-range` value (e.g. "U+0590-05FF" for Hebrew, or
   * "U+1F300-1F9FF,U+1F1E6-1F1FF" for emoji). When set, the browser only consults this
   * @font-face for codepoints inside the range — letting multiple @font-face rules under
   * the same font-family name cover different scripts (e.g. a Noto Sans SC rule for
   * Latin/CJK paired with a Noto Color Emoji rule scoped to emoji codepoints, so Chromium
   * picks the right face per glyph instead of using the OS fallback emoji font). When
   * empty, the property is omitted and the rule applies to every codepoint the source
   * font file actually contains.
   */
  std::string unicodeRange;
};

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
   */
  float rasterScale = 2.0f;

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

  /**
   * @font-face rules to inject into the generated HTML. The exporter produces a complete
   * @font-face CSS declaration for each entry, using the `mode` field to decide how the
   * font file is referenced (inline URL, base64 data URI, or file:// path). When this
   * vector is empty (the default), no @font-face rules are emitted and the browser falls
   * back to system fonts or CSS font synthesis for any weight/style that is not installed.
   */
  std::vector<FontFaceRule> fontFaceRules = {};
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
   * Precondition: `document.isLayoutApplied()` must return true — the exporter reads
   * layout-resolved geometry (positions, sizes, text runs) produced by PAGXDocument::applyLayout
   * and cannot derive them on the fly. Passing a document whose layout has not been applied
   * silently produces an empty or degenerate HTML string rather than throwing; callers are
   * responsible for invoking applyLayout() themselves.
   *
   * Thread safety: this is a stateless static method and may be called concurrently from
   * multiple threads, provided that each thread passes a distinct document (or a document that
   * is guaranteed not to be mutated for the duration of the call) and a distinct resourceDir
   * (so that concurrent writers do not race on the same filesystem location).
   *
   * Failure semantics: returns an empty string on internal failure, when the document has no
   * visible content, or when `resourceDir` is empty. The distinct causes are not encoded in
   * the return value; callers that need to tell them apart should validate inputs themselves
   * before invoking ToHTML.
   *
   * Ownership: `document` is consumed by const reference. The exporter does not retain any
   * pointer or reference to the document or its internal nodes beyond the duration of this
   * call, so the caller is free to destroy or mutate `document` as soon as ToHTML returns.
   *
   * @param document The PAGX document to export. Must have had applyLayout() called.
   * @param resourceDir Absolute directory path for auxiliary PNGs and copied images. Must not
   *                    be empty.
   * @param options Export options controlling output formatting.
   * @return The generated HTML string, or an empty string if resourceDir is empty, the document
   *         has no content, or the exporter encountered an internal failure.
   */
  static std::string ToHTML(const PAGXDocument& document, const std::string& resourceDir,
                            const Options& options = {});

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
   * @param document The PAGX document to export. Must have had applyLayout() called.
   * @param filePath The output file path, used verbatim (no validation performed by the
   *                 library).
   * @param options Export options controlling output formatting.
   * @return True on success, false if the file could not be written or the document has no
   *         content.
   */
  static bool ToFile(const PAGXDocument& document, const std::string& filePath,
                     const Options& options = {});
};

}  // namespace pagx
