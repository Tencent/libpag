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

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
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
 * Maximum nesting depth tolerated during HTML traversal. Mirrors the SVG importer.
 */
static constexpr int MAX_HTML_RECURSION_DEPTH = 128;

/**
 * Default CSS font-size used for HTML text leaves before any user style is applied.
 */
static constexpr float HTML_DEFAULT_FONT_SIZE = 14.0f;

/**
 * Inherited CSS style properties that cascade down the element tree during HTML
 * traversal. Empty strings denote "not set" (preserved so that descendants can know
 * whether to inherit further or use their own defaults).
 */
struct HTMLInheritedStyle {
  std::string color = {};
  std::string fontFamily = {};
  std::string fontSize = {};
  std::string fontWeight = {};
  std::string fontStyle = {};
  std::string fontStyleName = {};  // computed combination, e.g. "Bold Italic"
  std::string letterSpacing = {};
  std::string lineHeight = {};
  std::string textAlign = {};
  std::string textDecoration = {};
  std::string textDecorationColor = {};
  std::string whiteSpace = {};
};

/**
 * Resolved box-model attributes for a single HTML element. Anything left as NaN / empty
 * is "not specified" and falls back to PAGX defaults (which mirror CSS defaults for the
 * accepted subset).
 */
struct HTMLBoxAttributes {
  // Sizing
  float widthPx = NAN;    // explicit px width
  float widthPct = NAN;   // explicit % width (0-100)
  float heightPx = NAN;   // explicit px height
  float heightPct = NAN;  // explicit % height (0-100)

  // Positioning (only when position: absolute)
  bool absolute = false;
  float leftPx = NAN;
  float rightPx = NAN;
  float topPx = NAN;
  float bottomPx = NAN;

  // Layout
  bool displayFlex = false;
  bool flexRow = true;  // default of CSS flex
  float gapPx = 0.0f;
  bool gapSet = false;
  Padding padding = {};
  bool paddingSet = false;
  std::string alignItems = {};
  std::string justifyContent = {};
  float flexGrow = 0.0f;
  bool flexGrowSet = false;

  // Effects
  Color backgroundColor = {0, 0, 0, 0, ColorSpace::SRGB};
  bool backgroundColorSet = false;
  std::string backgroundImage = {};

  float borderRadiusPx = 0.0f;
  bool borderRadiusSet = false;

  float borderWidthPx = 0.0f;
  Color borderColor = {0, 0, 0, 1, ColorSpace::SRGB};
  bool borderSet = false;

  std::string boxShadow = {};
  std::string filter = {};
  std::string backdropFilter = {};

  float opacity = 1.0f;
  bool opacitySet = false;

  std::string mixBlendMode = {};

  bool clipOverflow = false;
};

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
  void collectAllIds(const std::shared_ptr<DOMNode>& node);

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
  };
  void collectTextFragments(const std::shared_ptr<DOMNode>& element,
                            const HTMLInheritedStyle& inherited, std::vector<TextFragment>& out);

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

  // <img> conversion.
  Layer* convertImage(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

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
  bool applyBackgroundVisuals(Layer* layer, const HTMLBoxAttributes& box, bool addRectangle);

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

  int _nextGeneratedId = 0;
  float _canvasWidth = 0;
  float _canvasHeight = 0;
  bool _hadHardError = false;
};

namespace detail {

// Lower-case ASCII in place.
inline std::string ToLower(std::string s) {
  for (auto& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// Trim ASCII whitespace from both ends.
inline std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Split a CSS value list on top-level commas (commas inside parentheses are not split).
std::vector<std::string> SplitTopLevelCommas(const std::string& s);

// Split a CSS value on whitespace runs while respecting parenthesised groups (so
// "0 2px 6px rgba(0,0,0,0.2)" yields four tokens).
std::vector<std::string> SplitTopLevelWhitespace(const std::string& s);

// Parse a CSS style string into a property map. Later properties override earlier ones.
// Comments and parenthesised values are respected.
void ParseStyleString(const std::string& styleStr,
                      std::unordered_map<std::string, std::string>& out);

// CSS named color table (CSS Color 3 + rebeccapurple).
const std::unordered_map<std::string, uint32_t>& NamedColors();

// Default style sheet by element tag.
const std::unordered_map<std::string, std::string>& ElementDefaults();

inline bool IsContainerTag(const std::string& tag) {
  return tag == "div" || tag == "section" || tag == "header" || tag == "footer" || tag == "main" ||
         tag == "aside" || tag == "nav" || tag == "article" || tag == "body";
}

inline bool IsTextLeafTag(const std::string& tag) {
  return tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" ||
         tag == "h6" || tag == "span" || tag == "a";
}

inline bool IsInlineRunTag(const std::string& tag) {
  return tag == "span" || tag == "a" || tag == "br";
}

// Lower-case the tag name of every element node in place.
void LowercaseTagsInPlace(const std::shared_ptr<DOMNode>& node);

// Escape XML text/attribute values for round-tripping inline SVG content.
std::string EscapeXml(const std::string& text, bool isAttribute);

// Try to parse a CSS angle in `deg`, `rad`, `turn`, or unitless (deg).
float ParseAngle(const std::string& raw);

// Returns the converted PAGX gradient angle (degrees, 0deg = +X axis) for a CSS angle
// (degrees, 0deg = top, clockwise).
inline float CssToPagxAngle(float cssDeg) {
  return cssDeg - 90.0f;
}

// Convert CSS keyword direction ("to bottom right", etc.) to a CSS angle in degrees.
float CssDirectionToAngle(const std::string& kw);

// Pull the bracketed args of a function-like CSS value:
// "linear-gradient(a, b, c)" -> "a, b, c".
std::string ExtractParenArgs(const std::string& value);

// Strip trailing slashes so file path resolution doesn't pick up wrong directories.
std::string DirectoryOf(const std::string& filePath);

bool LooksAbsolutePath(const std::string& src);

// Returns the value of property `key` from `props`, or an empty string when absent.
inline const std::string& LookupProperty(const std::unordered_map<std::string, std::string>& props,
                                         const std::string& key) {
  static const std::string emptyValue;
  auto it = props.find(key);
  return it == props.end() ? emptyValue : it->second;
}

// Convenience: lower-cased and trimmed lookup, used a lot when interpreting CSS keyword
// values such as `display`, `position`, `align-items`.
inline std::string LookupLowerTrimmed(const std::unordered_map<std::string, std::string>& props,
                                      const std::string& key) {
  return ToLower(Trim(LookupProperty(props, key)));
}

// Convert a CSS hex value (already parsed into a uint32_t) into a Color. When `hasAlpha`
// is true the alpha channel is the most significant byte, otherwise alpha defaults to 1.
Color HexToColor(uint32_t hex, bool hasAlpha);

// Builds a Padding shorthand from 1-4 numbers (CSS top/right/bottom/left expansion).
Padding BuildPaddingShorthand(const std::vector<float>& nums);

// Parses a CSS dimension string into either an explicit pixel value or a percent value.
// Returns true on success and writes one of `outPx` / `outPct` (the other stays NaN).
bool ParseSizingDimension(const std::string& raw, float& outPx, float& outPct);

// Collapse HTML whitespace in a single fragment: convert tabs/CR to spaces, collapse
// adjacent ASCII whitespace, and trim leading/trailing whitespace at the fragment level.
std::string CollapseHTMLWhitespace(const std::string& raw);

}  // namespace detail

}  // namespace pagx
