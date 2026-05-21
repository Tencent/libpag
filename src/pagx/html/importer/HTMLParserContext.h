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
#include <vector>
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLBoxAttributes.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Color.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

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
  void collectStyles(const std::shared_ptr<DOMNode>& head);
  void parseStyleBlock(const std::shared_ptr<DOMNode>& styleNode);
  void collectAllIds(const std::shared_ptr<DOMNode>& node, int depth = 0);

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

  // Text leaf conversion (for <p>, <h1..6>, <span>, <a>).
  Layer* convertTextLeaf(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box,
                         const HTMLInheritedStyle& inherited);

  // Internal: a stretch of text content with a single resolved style. Multiple fragments
  // produced by walking inline children compose a rich-text TextBox.
  struct TextFragment {
    std::string text = {};
    std::string fontFamily = {};
    std::string fontStyleName = {};  // e.g. "Bold", "Italic", "Bold Italic"
    float fontSize = HTML_DEFAULT_FONT_SIZE;
    float letterSpacing = 0.0f;
    Color color = {0, 0, 0, 1, ColorSpace::SRGB};
    std::string textDecoration = {};
    // Gradient string copied from `HTMLInheritedStyle::textFillImage`. When non-empty,
    // `buildTextFill` emits a gradient Fill instead of a solid Fill (matching CSS
    // `background-clip: text` semantics from the nearest clip-to-text ancestor).
    std::string fillImage = {};
  };
  void collectTextFragments(const std::shared_ptr<DOMNode>& element,
                            const HTMLInheritedStyle& inherited, std::vector<TextFragment>& out,
                            int depth = 0);

  // Builds a TextFragment whose style fields mirror `inherited`. The `text` field is
  // left empty for the caller to fill in.
  TextFragment makeTextFragment(const HTMLInheritedStyle& inherited);

  // Returns true when two fragments share every style fingerprint (everything except
  // `text`), and therefore can be merged into a single run.
  static bool fragmentsShareStyle(const TextFragment& a, const TextFragment& b);

  // Appends `text` to the fragment list, merging into the previous fragment when their
  // style fingerprints match.
  void appendTextFragment(std::vector<TextFragment>& out, const HTMLInheritedStyle& inherited,
                          std::string text);

  // Builds a Text Element from a fragment.
  Text* buildTextElement(const TextFragment& fragment);

  // Builds a Fill Element with a SolidColor of `color`.
  Fill* buildSolidFill(const Color& color);

  // Builds a Fill from a TextFragment: emits a gradient Fill when `fragment.fillImage` is set,
  // otherwise falls back to `buildSolidFill(fragment.color)`. Used by `convertTextLeaf` so a
  // CSS `background-clip: text` ancestor can fill the glyphs with its gradient.
  Fill* buildTextFill(const TextFragment& fragment);

  // <img> conversion.
  Layer* convertImage(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

  // Folds the standard CSS rounded-image wrapper pattern (a container whose only role is
  // to round-clip a single <img> child via `border-radius` + `overflow: hidden`) into a
  // single Layer whose rounded Rectangle is filled directly by the image. PAGX's only
  // clip primitive (`clipToBounds`) is rectangular, so a naive translation lets the
  // image leak past the wrapper's rounded corners. Returns true (with `layer`
  // populated) on a successful fold; otherwise leaves `layer` unchanged for the
  // caller's normal container-emission path.
  bool foldRoundedImageWrapper(const std::shared_ptr<DOMNode>& element,
                               const HTMLBoxAttributes& box, Layer* layer);

  // Resolves a raw `src` URL to a final image source path: absolute URLs pass through,
  // relative paths are anchored at the document's `_basePath`.
  std::string resolveImageSource(const std::string& src) const;

  // Inline <svg> conversion.
  Layer* convertInlineSvg(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

  // Style / property helpers ----------------------------------------------------------
  const std::unordered_map<std::string, std::string>& getResolvedStyle(
      const std::shared_ptr<DOMNode>& node);

  HTMLBoxAttributes resolveBox(const std::shared_ptr<DOMNode>& element);

  // resolveBox sub-steps. Each consumes the shared resolved style map.
  void parseBoxSizing(HTMLBoxAttributes& box,
                      const std::unordered_map<std::string, std::string>& props);
  void parseBoxPositioning(HTMLBoxAttributes& box,
                           const std::unordered_map<std::string, std::string>& props);
  void parseBoxLayout(HTMLBoxAttributes& box,
                      const std::unordered_map<std::string, std::string>& props);
  void parseBoxVisuals(HTMLBoxAttributes& box,
                       const std::unordered_map<std::string, std::string>& props);

  HTMLInheritedStyle computeInherited(const std::shared_ptr<DOMNode>& element,
                                      const HTMLInheritedStyle& parent);

  // Returns true when the box carries any visual that requires a Rectangle/Fill/Stroke
  // chain on the outer Layer. Used to decide whether a double-host split is needed.
  static bool hasBackgroundVisuals(const HTMLBoxAttributes& box);

  // Returns true when an inner host Layer is needed (the outer carries the background and
  // the inner carries padding / layout). Mirrors the rule that padding cannot live on the
  // same Layer as the background rectangle without changing geometry.
  static bool requiresInnerHost(const HTMLBoxAttributes& box);

  // Splits class attribute on whitespace and merges the matching class rule declarations
  // into `out`. Used by both canvas-size detection and full style resolution.
  void mergeClassRules(const std::string& classAttribute,
                       std::unordered_map<std::string, std::string>& out);

  // Reads element id (or generates a unique one on collision) and assigns it to `layer`.
  void assignElementId(Layer* layer, const std::shared_ptr<DOMNode>& element);

  // Returns the property's resolved value, or `fallback`. Looks up inline style, class
  // rules, element defaults (in that priority), respecting CSS rules.
  std::string getStyleProperty(const std::shared_ptr<DOMNode>& node, const std::string& property,
                               const std::string& fallback = "");

  // Output building helpers -----------------------------------------------------------
  // Applies size / position attributes from box to layer (width, height, percentWidth,
  // percentHeight, left/right/top/bottom, includeInLayout).
  void applySizeAndPosition(Layer* layer, const HTMLBoxAttributes& box);

  // Applies layout attributes (layout, gap, padding, alignment, arrangement, flex).
  void applyLayoutAttributes(Layer* layer, const HTMLBoxAttributes& box);

  // Applies background visuals: bg-color, bg-gradient, border, border-radius, box-shadow,
  // backdrop-filter. Returns true if any visual was applied (the caller should consider
  // the double-layer split for padded contents).
  bool applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box);

  // Resolves a CSS gradient string ("linear-gradient(...)" / "radial-gradient(...)" /
  // "conic-gradient(...)") into a registered gradient node. Returns nullptr when the
  // value is empty, not a gradient, or fails to parse.
  ColorSource* parseGradientByValue(const std::string& value);

  // Emits a single text-decoration line (underline or line-through) onto `host`. When the
  // decoration colour differs from the text colour the rectangle and its solid fill are
  // wrapped in a Group so they don't bleed onto subsequent contents. `bottom` and
  // `centerY` are mutually exclusive — pass NaN for the one that should not be set.
  void emitTextDecorationLine(Layer* host, const Color& textColor, const Color& decorationColor,
                              bool decorationColorDiffers, float bottom, float centerY);

  // Creates the inner host Layer for the standard "outer background + inner padded
  // container" double-layer pattern. Caller decides when to invoke based on
  // `hasBackgroundVisuals(box) && requiresInnerHost(box)`.
  Layer* createInnerHost(Layer* outer, const HTMLBoxAttributes& box);

  // Applies layer-level attributes that don't depend on background: opacity, blend mode,
  // filter chain, overflow clip, data-* attributes.
  void applyLayerAttributes(Layer* layer, const std::shared_ptr<DOMNode>& element,
                            const HTMLBoxAttributes& box);

  // Parses a CSS color-like value into a PAGX Color. Returns transparent on empty/"none".
  Color parseColor(const std::string& value);

  // Parses a non-negative pixel length (no % allowed). Returns NaN on empty.
  float parsePxLength(const std::string& value);

  // Resolves a CSS line-height value into pixels. Supports unitless multipliers (CSS spec:
  // "the used value is this unitless <number> multiplied by the element's font size"), `px`,
  // `em`/`rem` (treated as 16px and current font size respectively), and percentages relative to
  // font size. Returns NaN when the value is empty or unparseable.
  float resolveLineHeightPx(const std::string& value, float fontSizePx);

  // Parses a `box-shadow` value into a list of resolved shadows. Inset shadows have
  // `inset == true`.
  struct ShadowSpec {
    float offsetX = 0;
    float offsetY = 0;
    float blur = 0;
    float spread = 0;  // ignored (warning) but parsed away
    Color color = {};
    bool inset = false;
  };
  std::vector<ShadowSpec> parseShadowList(const std::string& value);

  // Parses a `filter` chain (filter, drop-shadow, backdrop-filter shares blur parsing).
  struct FilterStep {
    enum class Kind { Blur, DropShadow, Unsupported };
    Kind kind = Kind::Unsupported;
    float blurX = 0;
    float blurY = 0;
    ShadowSpec shadow = {};
    std::string raw = {};
  };
  std::vector<FilterStep> parseFilterChain(const std::string& value);

  // Parses `linear-gradient(...)` into a LinearGradient node (registered with the
  // document) plus its color stops. Returns nullptr on parse failure.
  LinearGradient* parseLinearGradient(const std::string& value);
  RadialGradient* parseRadialGradient(const std::string& value);
  ConicGradient* parseConicGradient(const std::string& value);

  // Shared parsed-but-unfinalised stops representation produced by gradient parsers.
  using GradientStops = std::vector<std::pair<float, Color>>;

  // Parses the comma-separated trailing portion of a gradient call (`stop, stop, ...`)
  // into colour/offset pairs. `interpretAngularOffset` enables `<deg>` offsets used by
  // conic-gradient. Offsets that fail to parse are left as NaN for finalisation.
  GradientStops parseGradientStops(const std::vector<std::string>& parts, size_t startIndex,
                                   bool interpretAngularOffset);

  // Fills NaN offsets in `stops` with sensible defaults: first/last default to 0/1, and
  // intermediate gaps are spread evenly between known anchors. Returns false when no
  // stops are present.
  static bool finaliseGradientStops(GradientStops& stops);

  // Appends ColorStop nodes for every entry of `stops` to `targetStops`.
  template <typename T>
  void emitColorStops(T& targetStops, const GradientStops& stops);

  // Image resource registration.
  Image* registerImageResource(const std::string& imageSource);

  // Inline SVG capture -----------------------------------------------------------------
  // Serialises the given <svg> DOM node back into XML so that we can use it as a PAGX
  // import directive content.
  std::string serializeSvg(const std::shared_ptr<DOMNode>& svgNode);

  // ID handling ------------------------------------------------------------------------
  std::string consumeId(const std::shared_ptr<DOMNode>& element);
  std::string generateUniqueId(const std::string& prefix);

  // Diagnostics ------------------------------------------------------------------------
  void warn(const std::string& message);
  void hardError(const std::string& message);

  // Routes `message` to either `_document->errors` (when the document already exists) or
  // `_pendingDiagnostics` (before the document is constructed). Used by both warn() and
  // hardError() so the destination logic lives in exactly one place.
  void pushDiagnostic(std::string message);

  // Member fields ----------------------------------------------------------------------
  HTMLImporter::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;
  std::string _basePath = {};

  // CSS class selectors (key = class name without dot, value = declaration string).
  std::unordered_map<std::string, std::string> _cssClassRules = {};

  // CSS element selectors (key = tag name).
  std::unordered_map<std::string, std::string> _cssElementRules = {};

  // Cached resolved style per DOM node. Each entry merges inline + class + element rules.
  std::unordered_map<const DOMNode*, std::unordered_map<std::string, std::string>>
      _stylePropertiesCache = {};

  // IDs already seen in the HTML; used to avoid generating colliding IDs.
  std::unordered_set<std::string> _existingIds = {};

  // Image deduplication.
  std::unordered_map<std::string, Image*> _imageSourceToId = {};

  // Diagnostics queued before _document was constructed (so that errors raised during
  // canvas-size resolution can still be surfaced once the document exists).
  std::vector<std::string> _pendingDiagnostics = {};

  // Concrete (post-quote-strip, post-generic-map) family names seen anywhere in the
  // document's font-family stacks. Insertion-ordered for deterministic FontConfig
  // output; the set field deduplicates entries in O(1).
  std::vector<std::string> _fallbackFamilyNames = {};
  std::unordered_set<std::string> _fallbackFamilyNameSet = {};

  int _nextGeneratedId = 0;
  float _canvasWidth = 0;
  float _canvasHeight = 0;
  bool _hadHardError = false;

  // Records concrete family names from a font-family stack into the document-wide
  // dedup set. Empty inputs and duplicates are silently skipped.
  void recordFontFallbacks(const std::vector<std::string>& chain);

  // Flushes `_fallbackFamilyNames` into `_document->fontConfig()` as deferred user
  // fallback fonts. Called once at the tail of `parseDOM` so every font-family stack
  // discovered during the walk is available for per-glyph fallback at layout time.
  void flushFontFallbacksToDocument();
};

}  // namespace pagx
