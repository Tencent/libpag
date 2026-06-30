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
#include <vector>
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/types/Color.h"

namespace pagx {

struct DOMNode;
class Fill;
class HTMLDiagnosticSink;
class HTMLIdAllocator;
class HTMLLayerBuilder;
class HTMLStyleCascade;
class HTMLValueParser;
class Layer;
class PAGXDocument;
class Stroke;
class Text;

/**
 * Converts HTML text-leaf elements (`<p>`, `<h1..6>`, `<span>`, `<a>`) into PAGX `Layer`
 * subtrees. Walks the inline DOM cascade to produce a list of style-uniform `TextFragment`s,
 * then materialises them as either a bare `<Text>+<Fill>` pair (single-style runs) or a
 * full `TextBox` whose elements are per-fragment `Group`s.
 *
 * Borrows the importer's diagnostic sink, value parser, layer builder, style cascade and id
 * allocator. The document handle is bound after `PAGXDocument::Make` runs in
 * `HTMLParserContext::parseDOM`.
 */
class HTMLTextFragmentBuilder {
 public:
  /**
   * A stretch of text content with a single resolved style. Multiple fragments produced by
   * walking inline children compose a rich-text `TextBox`.
   */
  struct TextFragment {
    std::string text = {};
    std::string fontFamily = {};
    std::string fontStyleName = {};  // real-face style label, e.g. "Light" / "Medium" / ""
    // Synthetic weight / slant baked in from the CSS request (see `ResolveFontStyleSynthesis`).
    // Surface as `Text::fauxBold` / `Text::fauxItalic` so authored bold / italic survives a
    // missing styled face on the render host.
    bool fauxBold = false;
    bool fauxItalic = false;
    float fontSize = HTML_DEFAULT_FONT_SIZE;
    float letterSpacing = 0.0f;
    Color color = {0, 0, 0, 1, ColorSpace::SRGB};
    std::string textDecoration = {};
    // CSS `-webkit-text-stroke` resolved into a stroke width (px) and colour. `strokeWidth` is
    // NaN (or <= 0) when the run carries no stroke; a positive value drives a PAGX text
    // `<Stroke>` emitted after the Fill so it paints over the fill (CSS default paint-order).
    float strokeWidth = NAN;
    Color strokeColor = {0, 0, 0, 1, ColorSpace::SRGB};
    // Gradient string copied from `HTMLInheritedStyle::textFillImage`. When non-empty,
    // `buildTextFill` emits a gradient Fill instead of a solid Fill (matching CSS
    // `background-clip: text` semantics from the nearest clip-to-text ancestor).
    std::string fillImage = {};
    // Resolved CSS line-height in pixels for this run, captured at fragment creation time so
    // an inner-span override (e.g. `<span style="line-height:20px">` inside a fixed-height
    // outer span used as a digit/badge box) survives to the TextBox, where it determines the
    // line-box height. NaN means the run inherited an empty / `normal` value and contributes
    // no explicit line-height — `convertTextLeaf` then leaves `TextBox.lineHeight` at its
    // auto default.
    float lineHeight = NAN;
    // Derived from the run's CSS `white-space`. `collapseWhitespace` folds runs of
    // spaces / tabs into a single space (normal / nowrap / pre-line); when false
    // (pre / pre-wrap) the source spaces are emitted verbatim. `preserveNewlines` keeps a
    // source `\n` as a hard line break (pre / pre-wrap / pre-line); when false
    // (normal / nowrap) newlines collapse to spaces like any other whitespace. Defaults
    // match `white-space: normal`.
    bool collapseWhitespace = true;
    bool preserveNewlines = false;
  };

  HTMLTextFragmentBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                          HTMLLayerBuilder& layerBuilder, HTMLStyleCascade& styleCascade,
                          HTMLIdAllocator& idAllocator);

  /** Wires the document handle. Must be called once after the document has been constructed. */
  void bindDocument(PAGXDocument* document);

  /** Converts a text-leaf element (`<p>`, `<h1..6>`, `<span>`, `<a>`) into a Layer subtree. */
  Layer* convertTextLeaf(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                         const HTMLInheritedStyle& inherited);

  /** Builds a `Text` element from a fragment. Exposed so the synthetic anonymous-text-leaf
   *  path in `HTMLParserContext` can reuse it. */
  Text* buildTextElement(const TextFragment& fragment);

  /** Builds a Fill from a fragment: emits a gradient Fill when `fragment.fillImage` is set,
   *  otherwise falls back to `_layerBuilder.buildSolidFill(fragment.color)`. */
  Fill* buildTextFill(const TextFragment& fragment);

  /** Builds a solid `Stroke` from a fragment's `-webkit-text-stroke` resolution, or nullptr
   *  when the run carries no stroke (`strokeWidth` NaN / <= 0). Uses `StrokeAlign::Center` to
   *  match CSS, which always centres the stroke band on the glyph edge. */
  Stroke* buildTextStroke(const TextFragment& fragment);

 private:
  TextFragment makeFragment(const HTMLInheritedStyle& inherited);

  /** Derives the two whitespace-handling flags from a CSS `white-space` value:
   *  `outCollapseWhitespace` is false for `pre` / `pre-wrap` (source spaces kept verbatim),
   *  `outPreserveNewlines` is true for `pre` / `pre-wrap` / `pre-line` (source newlines kept
   *  as hard breaks). Unrecognised values fall back to `white-space: normal`. */
  static void ResolveWhiteSpaceFlags(const std::string& whiteSpace, bool& outCollapseWhitespace,
                                     bool& outPreserveNewlines);

  /** Returns true when two fragments share every style fingerprint (everything except `text`),
   *  and therefore can be merged into a single run. */
  static bool fragmentsShareStyle(const TextFragment& a, const TextFragment& b);

  /** Fast fingerprint check used by `appendFragment` to skip the full `makeFragment`
   *  materialisation when the new run shares style with the previous fragment. */
  static bool fragmentMatchesInherited(const TextFragment& a, const HTMLInheritedStyle& inherited);

  /** Appends `text` to the fragment list, merging into the previous fragment when their style
   *  fingerprints match. */
  void appendFragment(std::vector<TextFragment>& out, const HTMLInheritedStyle& inherited,
                      std::string text);

  /** Walks `element`'s inline children and accumulates fragments. */
  void collectFragments(const std::shared_ptr<DOMNode>& element,
                        const HTMLInheritedStyle& inherited, std::vector<TextFragment>& out,
                        int depth = 0);

  /** Collapses HTML whitespace across the fragment run as if it were a single CSS inline-
   *  formatting context (the per-fragment font size differs, so the trim must respect
   *  fragment boundaries). Empty fragments are pruned in place. */
  static void collapseFragmentWhitespace(std::vector<TextFragment>& fragments);

  /** Builds the outer / wrapper / text-host Layer chain for the leaf, applying size,
   *  position, transforms, background visuals (when present), padding, and the
   *  shadow-from-clip split. Returns the wrapper Layer that the caller publishes upwards;
   *  writes the inner host (where text contents land) into `outTextHost`. */
  Layer* buildTextHostLayers(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                             bool hasBgVisuals, Layer*& outTextHost);

  /** Populates `textHost->contents` with either a bare `<Text>+<Fill>` pair (single-style
   *  run) or a `TextBox` carrying per-fragment `Group`s. `inherited` and `box` supply the
   *  TextBox-only attributes (text-align, line-height, word-wrap, overflow, writing-mode). */
  void populateTextHostContents(Layer* textHost, const std::vector<TextFragment>& fragments,
                                const HTMLInheritedStyle& inherited, const HTMLBoxAttributes& box,
                                bool needsTextBox, bool isVertical, bool hasNoWrap);

  /** Emits underline / line-through decoration lines onto `textHost` derived from the
   *  inherited `text-decoration` and `text-decoration-color`. `<overline>` is unsupported
   *  and produces a diagnostic. */
  void emitTextDecorations(Layer* textHost, const std::vector<TextFragment>& fragments,
                           const HTMLInheritedStyle& inherited);

  HTMLDiagnosticSink& _diagnostics;
  HTMLValueParser& _valueParser;
  HTMLLayerBuilder& _layerBuilder;
  HTMLStyleCascade& _styleCascade;
  HTMLIdAllocator& _idAllocator;
  PAGXDocument* _document = nullptr;
};

}  // namespace pagx
