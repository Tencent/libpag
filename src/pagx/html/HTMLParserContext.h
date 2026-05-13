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
  std::string whiteSpace = {};
};

/**
 * Resolved box-model attributes for a single HTML element. Anything left as NaN / empty
 * is "not specified" and falls back to PAGX defaults (which mirror CSS defaults for the
 * accepted subset).
 */
struct HTMLBoxAttributes {
  // Sizing
  float widthPx = NAN;     // explicit px width
  float widthPct = NAN;    // explicit % width (0-100)
  float heightPx = NAN;    // explicit px height
  float heightPct = NAN;   // explicit % height (0-100)

  // Positioning (only when position: absolute)
  bool absolute = false;
  float leftPx = NAN;
  float rightPx = NAN;
  float topPx = NAN;
  float bottomPx = NAN;

  // Layout
  bool displayFlex = false;
  bool flexRow = true;   // default of CSS flex
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
                            const HTMLInheritedStyle& inherited,
                            std::vector<TextFragment>& out);

  // <img> conversion.
  Layer* convertImage(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

  // Inline <svg> conversion.
  Layer* convertInlineSvg(const std::shared_ptr<DOMNode>& element, const HTMLBoxAttributes& box);

  // Style / property helpers ----------------------------------------------------------
  const std::unordered_map<std::string, std::string>& getResolvedStyle(
      const std::shared_ptr<DOMNode>& node);

  HTMLBoxAttributes resolveBox(const std::shared_ptr<DOMNode>& element,
                               const HTMLInheritedStyle& inherited);

  HTMLInheritedStyle computeInherited(const std::shared_ptr<DOMNode>& element,
                                      const HTMLInheritedStyle& parent);

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

  // Parses CSS color-like value into a PAGX Color. Returns transparent on empty/"none".
  Color parseColor(const std::string& value);

  // Parses a CSS length. Returns 0 on empty/invalid. Supports px, %, and unitless.
  // outIsPercent is set to true when the value uses %.
  float parseLength(const std::string& value, float containerSize, bool* outIsPercent = nullptr);

  // Parses a non-negative pixel length (no % allowed). Returns NaN on empty.
  float parsePxLength(const std::string& value);

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

  int _nextGeneratedId = 0;
  float _canvasWidth = 0;
  float _canvasHeight = 0;
  bool _hadHardError = false;
};

}  // namespace pagx
