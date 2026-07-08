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
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLAnimationBuilder.h"
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLImageResources.h"
#include "pagx/html/importer/HTMLInlineSvgEmitter.h"
#include "pagx/html/importer/HTMLLayerBuilder.h"
#include "pagx/html/importer/HTMLStyleCascade.h"
#include "pagx/html/importer/HTMLTextFragmentBuilder.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/types/Color.h"

namespace pagx {

class Image;
class Layer;
struct DOMNode;
class XMLDOM;

/**
 * Internal HTML parser implementation. Builds a PAGXDocument from an HTML DOM, applying
 * the restricted subset rules documented in `spec/html_subset.md`.
 */
class HTMLParserContext {
 public:
  explicit HTMLParserContext(const HTMLImporter::Options& options);

  /**
   * Parses HTML bytes. Returns nullptr on hard error.
   */
  std::shared_ptr<PAGXDocument> parse(const uint8_t* data, size_t length);

  /**
   * Parses an HTML file. Relative image paths in <img> are resolved against this file's
   * parent directory.
   */
  std::shared_ptr<PAGXDocument> parseFile(const std::string& filePath);

 private:
  std::shared_ptr<PAGXDocument> parseDOM(const std::shared_ptr<XMLDOM>& dom);

  // High-level traversal --------------------------------------------------------------
  // body / canvas size detection
  bool resolveCanvasSize(const std::shared_ptr<DOMNode>& body, float& outW, float& outH);

  // Element conversion ----------------------------------------------------------------
  Layer* convertBody(const std::shared_ptr<DOMNode>& body, float canvasW, float canvasH);

  // Converts a single HTML element to a Layer. Returns nullptr for skipped/unsupported
  // elements (diagnostic is emitted internally). When the element is purely a text
  // leaf, the returned Layer contains a TextBox/Text + Fill.
  Layer* convertElement(const std::shared_ptr<DOMNode>& element,
                        const HTMLInheritedStyle& inherited, int depth);

  // Container conversion (for <div>-like elements). Emits the standard "outer
  // background + inner padded container" double-layer when both background and
  // padding/layout are present.
  Layer* convertContainer(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                          const HTMLInheritedStyle& inherited, int depth);

  // Text leaf conversion (for <p>, <h1..6>, <span>, <a>). Thin forwarder to
  // `_textFragmentBuilder->convertTextLeaf`.
  Layer* convertTextLeaf(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                         const HTMLInheritedStyle& inherited);

  // <img> conversion.
  Layer* convertImage(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

  // Recovers a CSS `url(...)` background into an `ImagePattern` Fill appended to `layer`
  // (the inverse of `HTMLWriter`'s ImagePattern emission). `background-size` selects the
  // scaleMode (contain/cover/100% 100% → LetterBox/Zoom/Stretch); any explicit pixel size or a
  // tiling `background-repeat` falls back to ScaleMode::None with the pattern matrix carrying the
  // per-axis scale and the position offset. Returns true when a fill was emitted. The layer's
  // background geometry must already be present (added by `applyBackgroundVisuals`).
  bool applyBackgroundImageFill(const HTMLBoxAttributes& box, Layer* layer);

  // Folds the standard CSS rounded-image wrapper pattern (a container whose only role is
  // to round-clip a single <img> child via `border-radius` + `overflow: hidden`) into a
  // single Layer whose rounded Rectangle is filled directly by the image. PAGX's only
  // clip primitive (`clipToBounds`) is rectangular, so a naive translation lets the
  // image leak past the wrapper's rounded corners. Returns true (with `layer`
  // populated) on a successful fold; otherwise leaves `layer` unchanged for the
  // caller's normal container-emission path.
  bool foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                               const HTMLBoxAttributes& box, Layer* layer);

  // Inline <svg> conversion.
  Layer* convertInlineSvg(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                          const HTMLInheritedStyle& inherited);

  // Walks an inline `<svg>` subtree (before it is serialised into the layer's import directive) and
  // records every shape descendant carrying an inline `style="animation:…"`. Each such shape is
  // given a stable DOM `id` (minted when absent) so the SVG importer can derive its Fill / Stroke
  // painter ids from it, and a `PendingSvgShapeAnimation` is queued targeting those derived ids.
  // `stroke-dashoffset` scaling (real path length / author `pathLength`) is resolved here where the
  // geometry `d` is available, mirroring the static dash scaling in the SVG importer.
  void collectInlineSvgShapeAnimations(const std::shared_ptr<DOMNode>& node);

  // Reserves the element's `id` on the allocator, then records the element when it carries an
  // `animation` declaration so the post-tree-build animation pass can emit a PAGX animation
  // bound to this layer. All `<img>`, inline `<svg>`, and container conversion paths funnel
  // their final outer Layer through this helper to ensure animation capture is exhaustive.
  void assignElementId(Layer* layer, const std::shared_ptr<DOMNode>& element);

  // Rebuilds a PAGX mask layer from the element's CSS `mask-image` (alpha / luminance) or
  // `clip-path: url(#id)` (contour) and attaches it to `layer` as `layer->mask` / `maskType`
  // (the inverse of `HTMLWriter::writeMaskCSS` / `writeClipDef`). The mask geometry SVG is parsed
  // through `SVGImporter`, and its nodes are transplanted into `_document`. The mask layer is added
  // as an invisible, layout-excluded child of `layer` so it shares the masked layer's local
  // coordinate space and is reachable by the renderer's mask lookup. No-op when the box carries
  // neither a mask nor a clip-path reference. `box` supplies the masked layer's resolved size used
  // to frame a contour clip-path SVG.
  void applyMaskOrClip(Layer* layer, const HTMLBoxAttributes& box);

  // Rebuilds an alpha / luminance mask layer from a raster `mask-image: url(...)` (a PNG / JPEG /
  // WebP referenced by file path, `http(s)` URL, or `data:image/<raster>` URI) and attaches it to
  // `layer`. The image is loaded into an `ImagePattern` fill on a Rectangle sized to the image's
  // native pixels, then `mask-size` / `mask-position` scale and offset it onto the masked box (via
  // `applyMaskSizeAndPosition`), mirroring the SVG-mask path. Returns false when `url` is empty or
  // the image cannot be loaded / decoded, so the caller can fall back to warning and dropping the
  // mask. The complement of the SVG data-URI branch handled directly in `applyMaskOrClip`.
  bool applyRasterImageMask(Layer* layer, const HTMLBoxAttributes& box, const std::string& url);

  // Applies the CSS `mask-size` / `mask-position` transform onto a rebuilt alpha/luminance mask
  // layer (the inverse of the size/position emission in `HTMLWriter::writeMaskCSS`). The mask SVG
  // is imported at its intrinsic pixel size; CSS then scales it to `mask-size` and offsets it by
  // `mask-position`, both anchored at the masked element's top-left origin and resolved against
  // its box for percentage / keyword values. `intrinsicW` / `intrinsicH` are the mask SVG's own
  // dimensions; `box` supplies the masked element's size. No-op when size and position are absent
  // or `auto`, or when the intrinsic size is degenerate.
  void applyMaskSizeAndPosition(Layer* maskLayer, const HTMLBoxAttributes& box, float intrinsicW,
                                float intrinsicH);

  // Resolves one axis of CSS `mask-position` into a top-left pixel offset. `token` is a length, a
  // percentage, or an edge keyword (`left`/`right`/`top`/`bottom`/`center`); percentages and
  // keywords resolve against `(boxAxis - maskAxis)` per the CSS background/mask positioning model,
  // while a bare length is the offset from the box's leading edge.
  float resolveMaskPositionAxis(const std::string& token, float boxAxis, float maskAxis);

  // Image resource registration. Thin forwarder to `_imageResources->registerResource`.
  Image* registerImageResource(const std::string& imageSource);

  // Decodes an `Image` node's native pixel size (from inline data, a `data:` URI, or a file
  // path). Returns {0, 0} when the bytes cannot be decoded. Used to recover the per-axis scale
  // of a tiled `background-image` (CSS tile px / native px).
  std::pair<int, int> decodeImageNativeSize(const Image* image);

  // Resolves a raw `<img src>` URL via `_imageResources->resolveSource`.
  std::string resolveImageSource(const std::string& src) const;

  // Inline SVG capture -----------------------------------------------------------------
  // Serialises the given <svg> DOM node back into XML so that we can use it as a PAGX
  // import directive content.
  std::string serializeSvg(const std::shared_ptr<DOMNode>& svgNode);

  // Post-tree cleanup: drops `BackgroundBlurStyle` (CSS `backdrop-filter: blur`) from any layer
  // that sits under an ancestor (or is itself) whose opacity is animated below 1. PAGX renders an
  // `opacity < 1` group into an isolated offscreen surface, so a descendant backdrop-filter samples
  // that empty/self surface instead of the page behind and tints the box with its own colour.
  // Chromium samples the real backdrop, so the blur is invisible when nothing sits behind the box;
  // dropping it matches the baseline far better than the tint artifact. Runs after the animation
  // pass so the fading layers are known. Static `opacity < 1` is handled separately by the
  // box-shadow fallback path in the HTML writer.
  void suppressBackdropBlurUnderOpacityFade();

  // Diagnostics ------------------------------------------------------------------------
  // Short forwarders to `_diagnostics`; kept for the very common warn / hardError call
  // sites where the unique_ptr-deref boilerplate would dominate the surrounding code.
  void warn(const std::string& message);
  void hardError(const std::string& message);

  // Member fields ----------------------------------------------------------------------
  HTMLImporter::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;

  // Allocates / reserves DOM-side ids and generates fresh non-colliding ones for synthetic
  // layers (e.g. registered images).
  std::unique_ptr<HTMLIdAllocator> _idAllocator = nullptr;

  // Parses CSS string values (color / length / shadow / filter / gradient) into typed PAGX
  // values. Borrows `_diagnostics`, `_canvasWidth` / `_canvasHeight` and `_document`.
  std::unique_ptr<HTMLValueParser> _valueParser = nullptr;

  // Owns the CSS rule tables, the resolved-style cache, the inheritance walk, and the
  // box-attribute parser. Borrows `_diagnostics` and `_valueParser`.
  std::unique_ptr<HTMLStyleCascade> _styleCascade = nullptr;

  // Maps the `@keyframes` + `animation` subset onto PAGX animations. Borrows `_diagnostics`,
  // `_valueParser` and `_idAllocator`; document handle + keyframes registry bound in parseDOM.
  std::unique_ptr<HTMLAnimationBuilder> _animationBuilder = nullptr;

  // Elements carrying an `animation` declaration, paired with their finalised outer Layer.
  // Recorded during `assignElementId` and processed after the whole tree is built (so background
  // fills used by `color` channels already exist).
  std::vector<std::pair<std::shared_ptr<DOMNode>, Layer*>> _pendingAnimations = {};

  // Inline-SVG shape descendants (`<path>`, `<rect>`, …) that declare a CSS animation. The wrapping
  // `<svg>` is serialised into a single import directive that is expanded (resolved) only after the
  // layer tree is built, so these shapes have no PAGX painter node yet. `convertInlineSvg` assigns
  // each such shape a stable DOM `id`; the SVG importer later derives its Fill / Stroke ids from it
  // (`<id>__fill` / `<id>__stroke`). The animation is built after the tree exists, targeting those
  // derived painter ids by string (the nodes materialise during resolve, before export).
  struct PendingSvgShapeAnimation {
    std::unordered_map<std::string, std::string> style = {};
    std::string fillTargetId = {};
    std::string strokeTargetId = {};
    float dashScale = 1.0f;
  };
  std::vector<PendingSvgShapeAnimation> _pendingSvgShapeAnimations = {};

  // Builds and mutates `Layer` instances from `HTMLBoxAttributes`. Borrows `_diagnostics`,
  // `_valueParser` and `_idAllocator`; document handle is bound after `PAGXDocument::Make`.
  std::unique_ptr<HTMLLayerBuilder> _layerBuilder = nullptr;

  // Converts text-leaf elements (`<p>`, `<h1..6>`, `<span>`, `<a>`) into Layer subtrees.
  // Borrows `_diagnostics`, `_valueParser`, `_layerBuilder`, `_styleCascade`, `_idAllocator`;
  // document handle is bound after `PAGXDocument::Make`.
  std::unique_ptr<HTMLTextFragmentBuilder> _textFragmentBuilder = nullptr;

  // Image deduplication, source-path resolution, and synthesis of the document-side `Image`
  // resource nodes used by `<img>` and CSS `background-image: url(...)` references.
  std::unique_ptr<HTMLImageResources> _imageResources = nullptr;

  // Captures inline `<svg>` subtrees as PAGX import-directive content (currentColor resolution
  // + XML serialisation).
  std::unique_ptr<HTMLInlineSvgEmitter> _svgEmitter = nullptr;

  // Routes warnings / hard errors. Owns the pre-document buffer and the strict-mode policy;
  // see `HTMLDiagnosticSink` for the binding contract.
  std::unique_ptr<HTMLDiagnosticSink> _diagnostics = nullptr;

  // Concrete (post-quote-strip, post-generic-map) family names seen anywhere in the
  // document's font-family stacks. Insertion-ordered for deterministic FontConfig
  // output; the set field deduplicates entries in O(1).
  std::vector<std::string> _fallbackFamilyNames = {};
  std::unordered_set<std::string> _fallbackFamilyNameSet = {};

  float _canvasWidth = 0;
  float _canvasHeight = 0;
  // Records concrete family names from a font-family stack into the document-wide
  // dedup set. Empty inputs and duplicates are silently skipped.
  void recordFontFallbacks(const std::vector<std::string>& chain);

  // Static trampoline that adapts the cascade's `FontFallbackThunk` (function pointer + opaque
  // user data) to the parser context's `recordFontFallbacks` member, so the cascade can stay
  // independent of `HTMLParserContext` and we avoid `std::bind` / lambda at the wiring site.
  static void RecordFontFallbacksThunk(void* userData, const std::vector<std::string>& chain);

  // Flushes `_fallbackFamilyNames` into `_document->fontConfig()` as deferred user
  // fallback fonts. Called once at the tail of `parseDOM` so every font-family stack
  // discovered during the walk is available for per-glyph fallback at layout time.
  void flushFontFallbacksToDocument();
};

}  // namespace pagx
