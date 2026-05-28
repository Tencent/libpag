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

#include "pagx/html/importer/HTMLParserContext.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "pagx/HTMLSubsetTransformer.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

//==================================================================================================
// HTMLParserContext: top-level traversal
//==================================================================================================

HTMLParserContext::HTMLParserContext(const HTMLImporter::Options& options) : _options(options) {
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parseFile(const std::string& filePath) {
  auto dom = XMLDOM::MakeFromFile(filePath);
  if (!dom) {
    return nullptr;
  }
  _basePath = DirectoryOf(filePath);
  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }
  auto dom = XMLDOM::Make(data, length);
  if (!dom) {
    return nullptr;
  }
  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parseDOM(const std::shared_ptr<XMLDOM>& dom) {
  auto root = dom->getRootNode();
  if (!root) {
    return nullptr;
  }
  LowercaseTagsInPlace(root);
  if (root->name != "html") {
    return nullptr;
  }

  // Run the subset transformer as a pre-pass so the remainder of this routine only ever sees
  // subset-compliant HTML. The transformer mutates `root` in place; diagnostics are forwarded
  // into _pendingDiagnostics so they end up on PAGXDocument::errors once the document exists.
  if (_options.autoNormalize) {
    HTMLSubsetTransformer::Options topts = {};
    topts.strict = _options.strict;
    topts.preserveUnknownElements = _options.preserveUnknownElements;
    topts.canvasWidth = _options.targetWidth;
    topts.canvasHeight = _options.targetHeight;
    topts.inferFlexFromAbsolute = _options.inferFlexFromAbsolute;
    auto report = HTMLSubsetTransformer::Transform(root, topts);
    for (auto& diag : report.diagnostics) {
      std::string formatted = diag.message;
      if (!diag.code.empty()) {
        formatted += " [" + diag.code + "]";
      }
      _pendingDiagnostics.push_back(std::move(formatted));
    }
    if (!report.ok) {
      _hadHardError = true;
      return nullptr;
    }
  }

  // Find <head> and <body>.
  std::shared_ptr<DOMNode> head = nullptr;
  std::shared_ptr<DOMNode> body = nullptr;
  auto child = root->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      if (child->name == "head" && !head) {
        head = child;
      } else if (child->name == "body" && !body) {
        body = child;
      } else {
        warn("html: element '" + child->name + "' at root is not allowed; skipped");
      }
    }
    child = child->getNextSibling();
  }
  if (!body) {
    hardError("html: missing <body>");
    return nullptr;
  }

  // Collect <style> rules from <head> BEFORE canvas-size resolution: the body's width/height
  // may live in a class rule, and resolveCanvasSize() routes through getResolvedStyle() which
  // depends on _cssClassRules / _cssElementRules being populated.
  if (head) {
    collectStyles(head);
  }

  // Resolve the canvas size. Without one we cannot make a usable document, so this is a
  // hard error: any diagnostics queued up to this point are lost (the public Parse()
  // contract surfaces failure via a nullptr return).
  float canvasW = 0;
  float canvasH = 0;
  if (!resolveCanvasSize(body, canvasW, canvasH)) {
    hardError("html: failed to determine canvas size");
    return nullptr;
  }
  _canvasWidth = canvasW;
  _canvasHeight = canvasH;
  _document = PAGXDocument::Make(canvasW, canvasH);

  // Seed the document's font registry from the caller-supplied FontConfig before any
  // discovery happens, so registered typefaces and pre-configured fallbacks are in place
  // when `flushFontFallbacksToDocument()` later layers the CSS-discovered family names on
  // top as additional deferred fallbacks.
  if (_options.fontConfig != nullptr) {
    _document->fontConfig() = *_options.fontConfig;
  }

  // Now that the document exists, flush any pending diagnostics gathered before its creation.
  for (auto& msg : _pendingDiagnostics) {
    _document->errors.push_back(std::move(msg));
  }
  _pendingDiagnostics.clear();

  // Title -> data-title on the document (PAGX has no top-level title node; the
  // exporter writes data-* on the root <pagx>).
  if (head) {
    auto titleNode = head->getFirstChild("title");
    if (titleNode) {
      auto textNode = titleNode->getFirstChild();
      if (textNode && textNode->type == DOMNodeType::Text) {
        _document->customData["title"] = Trim(textNode->name);
      }
    }
  }

  collectAllIds(root);

  Layer* bodyLayer = convertBody(body, canvasW, canvasH);
  if (_hadHardError) {
    return nullptr;
  }
  if (bodyLayer) {
    _document->layers.push_back(bodyLayer);
  }
  flushFontFallbacksToDocument();
  return _document;
}

void HTMLParserContext::recordFontFallbacks(const std::vector<std::string>& chain) {
  for (auto& name : chain) {
    if (name.empty()) continue;
    if (_fallbackFamilyNameSet.insert(name).second) {
      _fallbackFamilyNames.push_back(name);
    }
  }
}

void HTMLParserContext::flushFontFallbacksToDocument() {
  if (!_document) return;
  auto& cfg = _document->fontConfig();
  for (auto& name : _fallbackFamilyNames) {
    // Style is fixed to "Regular": LayoutContext's user-fallback path matches by family
    // name only (style is ignored for per-glyph fallback), so registering a single
    // deferred record per family is sufficient.
    cfg.addFallbackFont(/*path=*/std::string(), /*ttcIndex=*/0, name, "Regular");
  }
}

//==================================================================================================
// Diagnostics
//==================================================================================================

void HTMLParserContext::pushDiagnostic(std::string message) {
  if (_document) {
    _document->errors.push_back(std::move(message));
  } else {
    _pendingDiagnostics.push_back(std::move(message));
  }
}

void HTMLParserContext::warn(const std::string& message) {
  if (_options.strict) {
    hardError(message);
    return;
  }
  pushDiagnostic(message);
}

void HTMLParserContext::hardError(const std::string& message) {
  _hadHardError = true;
  pushDiagnostic(message);
}

//==================================================================================================
// Canvas size resolution
//==================================================================================================

bool HTMLParserContext::resolveCanvasSize(const std::shared_ptr<DOMNode>& body, float& outW,
                                          float& outH) {
  bool haveTarget = !std::isnan(_options.targetWidth) && !std::isnan(_options.targetHeight);
  // Reuse getResolvedStyle so the body's resolved property map is parsed once and cached
  // for later resolveBox() in convertBody. This also means class rules and element rules
  // (collected just before this call) participate in canvas-size resolution.
  const auto& props = getResolvedStyle(body);
  float bodyW = parsePxLength(LookupProperty(props, "width"));
  float bodyH = parsePxLength(LookupProperty(props, "height"));
  bool haveBody = !std::isnan(bodyW) && !std::isnan(bodyH) && bodyW > 0 && bodyH > 0;

  if (haveTarget && (!haveBody || !_options.preferBodySize)) {
    outW = _options.targetWidth;
    outH = _options.targetHeight;
    return true;
  }
  if (haveBody) {
    outW = bodyW;
    outH = bodyH;
    return true;
  }
  return false;
}

//==================================================================================================
// ID handling
//==================================================================================================

void HTMLParserContext::collectAllIds(const std::shared_ptr<DOMNode>& node, int depth) {
  if (!node) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    warn("html: maximum recursion depth reached during id collection; subtree skipped");
    return;
  }
  if (node->type == DOMNodeType::Element) {
    auto* idAttr = node->findAttribute("id");
    if (idAttr && !idAttr->empty()) {
      _existingIds.insert(*idAttr);
    }
  }
  auto child = node->getFirstChild();
  while (child) {
    collectAllIds(child, depth + 1);
    child = child->getNextSibling();
  }
}

std::string HTMLParserContext::generateUniqueId(const std::string& prefix) {
  std::string id;
  do {
    id = prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0);
  _existingIds.insert(id);
  return id;
}

std::string HTMLParserContext::consumeId(const std::shared_ptr<DOMNode>& element) {
  auto* idAttr = element->findAttribute("id");
  if (!idAttr || idAttr->empty()) return {};
  // The PAGX layer registry is populated lazily by `makeNode<>`, never with an explicit id from
  // this importer, so collisions can only come from the source DOM itself. `_existingIds` already
  // tracks every DOM-side id discovered by `collectAllIds`, so we trust the author and forward
  // the id verbatim.
  return *idAttr;
}

void HTMLParserContext::assignElementId(Layer* layer, const std::shared_ptr<DOMNode>& element) {
  std::string id = consumeId(element);
  if (!id.empty()) layer->id = id;
}

bool HTMLParserContext::hasBackgroundVisuals(const HTMLBoxAttributes& box) {
  return box.backgroundColorSet || !box.backgroundImage.empty() || box.borderRadiusSet ||
         box.borderSet || !box.boxShadow.empty() || !box.backdropFilter.empty();
}

bool HTMLParserContext::requiresInnerHost(const HTMLBoxAttributes& box) {
  return box.paddingSet || box.displayFlex || box.gapSet;
}

Layer* HTMLParserContext::createInnerHost(Layer* outer, const HTMLBoxAttributes& box) {
  auto inner = _document->makeNode<Layer>();
  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  applyLayoutAttributes(inner, box);
  outer->children.push_back(inner);
  return inner;
}

//==================================================================================================
// Body / element conversion
//==================================================================================================

Layer* HTMLParserContext::convertBody(const std::shared_ptr<DOMNode>& body, float canvasW,
                                      float canvasH) {
  HTMLInheritedStyle inherited = computeInherited(body, HTMLInheritedStyle{});
  HTMLBoxAttributes box = resolveBox(body);

  auto layer = _document->makeNode<Layer>();
  layer->width = canvasW;
  layer->height = canvasH;
  layer->percentWidth = NAN;
  layer->percentHeight = NAN;

  bool hasBgVisuals = hasBackgroundVisuals(box);
  if (hasBgVisuals) {
    applyBackgroundVisuals(layer, box);
  }
  applyLayerAttributes(layer, body, box);

  // Hoist any `box-shadow` off the clipped layer so `overflow: hidden` does not also clip the
  // shadow. `wrapper` is `layer` when no split happens.
  Layer* wrapper = maybeSplitBoxShadowFromClip(layer);

  Layer* contentHost = layer;
  bool needsInnerHost = hasBgVisuals && (box.paddingSet || box.displayFlex);
  if (needsInnerHost) {
    contentHost = createInnerHost(layer, box);
  } else {
    applyLayoutAttributes(layer, box);
  }

  auto child = body->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto* childLayer = convertElement(child, inherited, 1);
      if (childLayer) {
        contentHost->children.push_back(childLayer);
      }
    } else if (child->type == DOMNodeType::Text) {
      // Stray text directly under <body> — match convertContainer's behaviour so authors
      // who write `<body>hello</body>` get a visible result instead of silent loss.
      std::string trimmed = Trim(child->name);
      if (!trimmed.empty()) {
        warn("html: stray text inside <body>; emitted as anonymous text leaf");
        HTMLBoxAttributes leafBox = HTMLBoxAttributes{};
        auto anon = MakeStrayTextSpan(trimmed);
        Layer* leaf = convertTextLeaf(anon, leafBox, inherited);
        if (leaf) {
          contentHost->children.push_back(leaf);
        }
      }
    }
    child = child->getNextSibling();
  }
  assignElementId(wrapper, body);
  return wrapper;
}

Layer* HTMLParserContext::convertElement(const std::shared_ptr<DOMNode>& element,
                                         const HTMLInheritedStyle& inherited, int depth) {
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    warn("html: maximum recursion depth reached; element skipped");
    return nullptr;
  }
  if (element->type != DOMNodeType::Element) {
    return nullptr;
  }
  const std::string& tag = element->name;

  if (tag == "br") {
    auto layer = _document->makeNode<Layer>();
    auto text = _document->makeNode<Text>();
    text->text = "\n";
    text->fontFamily = inherited.primaryFontFamily.empty() ? HTML_DEFAULT_FONT_FAMILY
                                                           : inherited.primaryFontFamily;
    text->fontSize = HTML_DEFAULT_FONT_SIZE;
    layer->contents.push_back(text);
    return layer;
  }
  if (tag == "svg") {
    HTMLBoxAttributes box = resolveBox(element);
    return wrapWithMargin(convertInlineSvg(element, box, inherited), box);
  }
  if (tag == "img") {
    HTMLBoxAttributes box = resolveBox(element);
    return wrapWithMargin(convertImage(element, box), box);
  }

  HTMLInheritedStyle childInherited = computeInherited(element, inherited);
  HTMLBoxAttributes box = resolveBox(element);

  if (IsContainerTag(tag)) {
    return wrapWithMargin(convertContainer(element, box, childInherited, depth), box);
  }
  if (IsTextLeafTag(tag)) {
    // HTML allows mixed content: a <span> / <p> may contain inline-block children
    // (<div>, <svg>, <img>, ...). Strict text-leaf handling would drop them. When we
    // detect any non-inline-run element child, fall back to container handling so
    // both the text fragments and the block children survive as sibling layers.
    bool hasBlockChild = false;
    for (auto c = element->getFirstChild(); c; c = c->getNextSibling()) {
      if (c->type != DOMNodeType::Element) continue;
      if (IsInlineLeafChildName(c->name)) continue;
      hasBlockChild = true;
      break;
    }
    if (hasBlockChild) {
      return wrapWithMargin(convertContainer(element, box, childInherited, depth), box);
    }
    return wrapWithMargin(convertTextLeaf(element, box, childInherited), box);
  }
  if (_options.preserveUnknownElements) {
    warn("html: unknown element '" + tag + "' preserved as placeholder");
    auto layer = _document->makeNode<Layer>();
    layer->customData["html-unknown"] = tag;
    return layer;
  }
  warn("html: element '" + tag + "' not supported; skipped");
  return nullptr;
}

Layer* HTMLParserContext::convertContainer(const std::shared_ptr<DOMNode>& element,
                                           const HTMLBoxAttributes& box,
                                           const HTMLInheritedStyle& inherited, int depth) {
  bool hasBgVisuals = hasBackgroundVisuals(box);
  bool needsInner = hasBgVisuals && requiresInnerHost(box);

  auto layer = _document->makeNode<Layer>();
  applySizeAndPosition(layer, box);
  applyLayerAttributes(layer, element, box);

  if (hasBgVisuals) {
    applyBackgroundVisuals(layer, box);
  }

  applyBoxTransform(layer, box, element);

  // Must run after applyBackgroundVisuals(): the fold reuses the rounded Rectangle just
  // emitted into `layer->contents` as the image's fill geometry.
  if (foldRoundedImageWrapper(element, box, layer)) {
    return layer;
  }

  // Hoist any non-inset `box-shadow` off the clipping layer so `overflow: hidden` does not
  // cut the shadow. `wrapper` is `layer` when no split happens; both children added below
  // and the layout-host machinery continue to operate on `layer` (now potentially the
  // inner of the split).
  Layer* wrapper = maybeSplitBoxShadowFromClip(layer);

  Layer* contentHost = layer;
  if (needsInner) {
    contentHost = createInnerHost(layer, box);
  } else {
    applyLayoutAttributes(layer, box);
  }

  // CSS border-box: layout-flow content sits inside the border edge, so reserve the
  // border width as additional padding on the layout host. Without this, children
  // would start flush with the layer's outer edge and overlap the inside-aligned
  // border stroke.
  if (box.borderSet && box.borderWidthPx > 0 && contentHost->layout != LayoutMode::None) {
    contentHost->padding.top += box.borderWidthPx;
    contentHost->padding.right += box.borderWidthPx;
    contentHost->padding.bottom += box.borderWidthPx;
    contentHost->padding.left += box.borderWidthPx;
  }

  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Element) {
      auto* childLayer = convertElement(child, inherited, depth + 1);
      if (childLayer) {
        // CSS border-box: position:absolute descendants are positioned relative to the
        // parent's padding box, which sits inside the border. Shift their explicit
        // left/right/top/bottom offsets by the border width so they don't overlap the
        // border stroke.
        if (box.borderSet && box.borderWidthPx > 0 && !childLayer->includeInLayout) {
          if (!std::isnan(childLayer->left)) childLayer->left += box.borderWidthPx;
          if (!std::isnan(childLayer->right)) childLayer->right += box.borderWidthPx;
          if (!std::isnan(childLayer->top)) childLayer->top += box.borderWidthPx;
          if (!std::isnan(childLayer->bottom)) childLayer->bottom += box.borderWidthPx;
        }
        contentHost->children.push_back(childLayer);
      }
    } else if (child->type == DOMNodeType::Text) {
      std::string trimmed = Trim(child->name);
      if (!trimmed.empty()) {
        warn("html: stray text inside non-text container; emitted as anonymous text leaf");
        HTMLBoxAttributes leafBox = HTMLBoxAttributes{};
        auto anon = MakeStrayTextSpan(trimmed);
        Layer* leaf = convertTextLeaf(anon, leafBox, inherited);
        if (leaf) {
          contentHost->children.push_back(leaf);
        }
      }
    }
    child = child->getNextSibling();
  }
  assignElementId(wrapper, element);
  return wrapper;
}

//==================================================================================================
// Text fragment collection / text leaf conversion
//==================================================================================================

HTMLParserContext::TextFragment HTMLParserContext::makeTextFragment(
    const HTMLInheritedStyle& inherited) {
  TextFragment frag;
  frag.fontFamily =
      inherited.primaryFontFamily.empty() ? HTML_DEFAULT_FONT_FAMILY : inherited.primaryFontFamily;
  frag.fontStyleName = inherited.fontStyleName;
  frag.fontSize = inherited.fontSizePx;
  frag.letterSpacing = inherited.letterSpacingPx;
  frag.color = inherited.resolvedTextColor;
  frag.textDecoration = inherited.textDecoration;
  frag.fillImage = inherited.textFillImage;
  // Resolve once per fragment so convertTextLeaf can derive TextBox.lineHeight without
  // re-parsing the cascade. Empty / `normal` cascades resolve to NaN, signalling "no
  // explicit contribution" — the line-box then collapses to the parent's font metrics.
  frag.lineHeight = resolveLineHeightPx(inherited.lineHeight, inherited.fontSizePx);
  return frag;
}

bool HTMLParserContext::fragmentsShareStyle(const TextFragment& a, const TextFragment& b) {
  // CSS unit conversions (em -> px etc.) introduce sub-pixel rounding that should not
  // prevent two same-styled inline runs from merging. Use a small absolute tolerance for
  // the float fields. The tolerance is well below any visually-meaningful step.
  constexpr float epsilon = 1e-3f;
  return a.fontFamily == b.fontFamily && a.fontStyleName == b.fontStyleName &&
         std::fabs(a.fontSize - b.fontSize) < epsilon &&
         std::fabs(a.letterSpacing - b.letterSpacing) < epsilon && a.color == b.color &&
         a.textDecoration == b.textDecoration && a.fillImage == b.fillImage;
}

bool HTMLParserContext::fragmentMatchesInherited(const TextFragment& a,
                                                 const HTMLInheritedStyle& inherited) {
  constexpr float epsilon = 1e-3f;
  const bool familyMatch = inherited.primaryFontFamily.empty()
                               ? a.fontFamily == HTML_DEFAULT_FONT_FAMILY
                               : a.fontFamily == inherited.primaryFontFamily;
  return familyMatch && a.fontStyleName == inherited.fontStyleName &&
         std::fabs(a.fontSize - inherited.fontSizePx) < epsilon &&
         std::fabs(a.letterSpacing - inherited.letterSpacingPx) < epsilon &&
         a.color == inherited.resolvedTextColor && a.textDecoration == inherited.textDecoration &&
         a.fillImage == inherited.textFillImage;
}

void HTMLParserContext::appendTextFragment(std::vector<TextFragment>& out,
                                           const HTMLInheritedStyle& inherited, std::string text) {
  if (text.empty()) return;
  // Fast path: when the new run shares style with the back of `out`, just extend its text and
  // skip the fragment construction entirely. Saves four std::string copies per merged run on
  // typical rich-text inputs.
  if (!out.empty() && fragmentMatchesInherited(out.back(), inherited)) {
    out.back().text += text;
    // CSS line-box height is the max of all participants' line-heights, so a later run that
    // shares glyph styling but carries a taller `line-height` (e.g. a nested span used for
    // vertical centring inside a fixed-height badge) must still raise the merged fragment's
    // value. Without this, the merged line-height freezes at the first run's resolution and
    // later inner-span overrides would be silently dropped.
    float incomingLh = resolveLineHeightPx(inherited.lineHeight, inherited.fontSizePx);
    if (!std::isnan(incomingLh) && incomingLh > 0 &&
        (std::isnan(out.back().lineHeight) || incomingLh > out.back().lineHeight)) {
      out.back().lineHeight = incomingLh;
    }
    return;
  }
  TextFragment frag = makeTextFragment(inherited);
  frag.text = std::move(text);
  out.push_back(std::move(frag));
}

void HTMLParserContext::collectTextFragments(const std::shared_ptr<DOMNode>& element,
                                             const HTMLInheritedStyle& inherited,
                                             std::vector<TextFragment>& out, int depth) {
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    warn("html: maximum recursion depth reached inside text leaf; remaining inline runs skipped");
    return;
  }
  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      // Map every collapsible whitespace character in the text node — newline,
      // carriage return, tab — to a regular space before handing the run to
      // `appendTextFragment`. CSS treats these as equivalent to spaces inside
      // a `white-space: normal | nowrap` inline-formatting-context, so the
      // downstream `CollapseHTMLWhitespace` pass (whose front-trim only drops
      // ASCII spaces, intentionally, to preserve `<br>`-originated `\n`) can
      // collapse and trim them away just like the browser would. Without this
      // step, indented source HTML such as `<h1>\n        Title</h1>` leaks
      // its source newline into the first fragment ("\n Title"), shifting the
      // rendered baseline by one space and / or introducing a phantom line
      // break. `<br>` keeps its hard `\n` because that path appends the
      // newline directly (not via a text node), so its semantics are unaffected.
      std::string text = child->name;
      for (char& c : text) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
      }
      appendTextFragment(out, inherited, std::move(text));
    } else if (child->type == DOMNodeType::Element) {
      if (child->name == "br") {
        appendTextFragment(out, inherited, "\n");
      } else if (IsInlineRunTag(child->name)) {
        HTMLInheritedStyle childInherited = computeInherited(child, inherited);
        collectTextFragments(child, childInherited, out, depth + 1);
      } else {
        warn("html: '<" + child->name + ">' not supported inside text leaf; skipped");
      }
    }
    child = child->getNextSibling();
  }
}

Layer* HTMLParserContext::convertTextLeaf(const std::shared_ptr<DOMNode>& element,
                                          const HTMLBoxAttributes& box,
                                          const HTMLInheritedStyle& inherited) {
  std::vector<TextFragment> fragments;
  collectTextFragments(element, inherited, fragments);
  // Collapse whitespace as if the inline-formatting-context were a single CSS run, not as
  // independent fragments. Each fragment carries its own font size, so a trailing space in a
  // parent span followed by a child span (`灵魂内核层 <span>SOUL...</span>`) must keep that
  // space at the parent's font size — trimming the trailing space per-fragment would render
  // the two runs glued together. Strategy: only the first non-empty fragment trims its
  // leading whitespace; intermediate fragments keep both ends; cross-fragment boundary
  // whitespace is collapsed by trimming a fragment's leading whitespace when the previous
  // fragment ended with whitespace; trailing whitespace at the very end of the run is removed
  // in a post-pass on the last surviving fragment.
  size_t writeIndex = 0;
  bool prevEndsWithWhitespace = false;
  for (size_t i = 0; i < fragments.size(); ++i) {
    bool trimLeading = (writeIndex == 0) || prevEndsWithWhitespace;
    fragments[i].text =
        CollapseHTMLWhitespace(fragments[i].text, trimLeading, /*trimTrailing=*/false);
    if (fragments[i].text.empty()) continue;
    char back = fragments[i].text.back();
    prevEndsWithWhitespace = (back == ' ' || back == '\n');
    if (writeIndex != i) {
      fragments[writeIndex] = std::move(fragments[i]);
    }
    ++writeIndex;
  }
  while (writeIndex > 0) {
    auto& last = fragments[writeIndex - 1].text;
    while (!last.empty() && (last.back() == ' ' || last.back() == '\n')) {
      last.pop_back();
    }
    if (last.empty()) {
      --writeIndex;
    } else {
      break;
    }
  }
  fragments.resize(writeIndex);
  if (fragments.empty()) {
    return nullptr;
  }
  if (box.absolute) {
    warn("html: position: absolute on text leaf '<" + element->name +
         ">' is downgraded to absolute on the surrounding Layer");
  }

  bool hasBgVisuals = hasBackgroundVisuals(box);
  bool hasMultipleFragments = fragments.size() > 1;
  bool hasNoWrap = !inherited.whiteSpace.empty() && ToLower(Trim(inherited.whiteSpace)) == "nowrap";
  std::string wm = ToLower(Trim(inherited.writingMode));
  bool isVertical = wm == "vertical-rl" || wm == "vertical-lr";
  // A child inline run that carries an explicit `line-height` (e.g. the digit-badge idiom
  // `<div height:20><span line-height:20>02</span></div>`) raises the line-box height
  // beyond the run's font metrics. The bare `<Text>+<Fill>` no-TextBox path has nowhere to
  // record that, so the glyph would render at its intrinsic font height aligned to the
  // top of the box instead of being vertically padded to the line-box. Force a TextBox in
  // that case so the line-height fallback below has somewhere to land.
  bool fragmentHasExplicitLineHeight = false;
  for (const auto& f : fragments) {
    if (!std::isnan(f.lineHeight) && f.lineHeight > 0) {
      fragmentHasExplicitLineHeight = true;
      break;
    }
  }
  // CSS `transform` is reproduced via `Layer.matrix` on the outer Layer (handled below by
  // applyBoxTransform); it does NOT force a TextBox to be synthesised. The earlier
  // codepath set TextBox.skew/rotation/scale and required an explicit TextBox to host
  // them, but the unified Layer.matrix path makes the wrapping Layer transform every
  // descendant uniformly — including the bare `<Text>+<Fill>` pair the no-TextBox branch
  // emits — so the extra TextBox is no longer needed.
  bool needsTextBox = hasMultipleFragments || !inherited.textAlign.empty() ||
                      !inherited.lineHeight.empty() || box.clipOverflow || hasNoWrap ||
                      isVertical || fragmentHasExplicitLineHeight;

  auto outer = _document->makeNode<Layer>();
  applySizeAndPosition(outer, box);
  applyLayerAttributes(outer, element, box);
  applyBoxTransform(outer, box, element);

  Layer* textHost = outer;
  if (hasBgVisuals) {
    applyBackgroundVisuals(outer, box);
    auto inner = _document->makeNode<Layer>();
    inner->percentWidth = 100.0f;
    inner->percentHeight = 100.0f;
    if (box.paddingSet) inner->padding = box.padding;
    outer->children.push_back(inner);
    textHost = inner;
  } else if (box.paddingSet) {
    outer->padding = box.padding;
  }

  // Hoist any non-inset `box-shadow` off `outer` when it also clips. `wrapper` becomes the
  // root of this subtree returned to the caller; `outer` (and `textHost`) keep their
  // existing role as the clipping/text host inside the wrapper.
  Layer* wrapper = maybeSplitBoxShadowFromClip(outer);

  if (!needsTextBox) {
    const auto& f = fragments.front();
    textHost->contents.push_back(buildTextElement(f));
    textHost->contents.push_back(buildTextFill(f));
  } else {
    auto textBox = _document->makeNode<TextBox>();
    // Anchor the TextBox to the host layer's content width so that wordWrap can engage. Without
    // this the TextBox would adopt its single-line natural width and overflow the parent box.
    textBox->percentWidth = 100.0f;
    std::string ta = ToLower(Trim(inherited.textAlign));
    if (!ta.empty()) {
      if (ta == "left" || ta == "start") textBox->textAlign = TextAlign::Start;
      else if (ta == "center")
        textBox->textAlign = TextAlign::Center;
      else if (ta == "right" || ta == "end")
        textBox->textAlign = TextAlign::End;
      else if (ta == "justify")
        textBox->textAlign = TextAlign::Justify;
      else
        warn("html: unsupported text-align '" + ta + "'");
    }
    if (!inherited.lineHeight.empty()) {
      float lh = resolveLineHeightPx(inherited.lineHeight, fragments.front().fontSize);
      if (!std::isnan(lh) && lh > 0) textBox->lineHeight = lh;
    }
    // CSS line-box height = max of the parent's line-height and every inline child's
    // line-height. The block above only consulted the outer element's cascade, so it
    // would silently drop a child run's `line-height` whenever the outer was `normal` or
    // had no explicit value (for example the `<div height:20><span line-height:20>02</span></div>`
    // digit-badge pattern, where the outer span only carries background/size and the
    // inner span supplies the line-height that vertically centres the glyph). Walk the
    // fragments and lift their resolved values to TextBox.lineHeight when they exceed the
    // outer-driven value, mirroring the browser's max-of-participants rule.
    for (const auto& f : fragments) {
      if (!std::isnan(f.lineHeight) && f.lineHeight > textBox->lineHeight) {
        textBox->lineHeight = f.lineHeight;
      }
    }
    if (hasNoWrap) {
      textBox->wordWrap = false;
    }
    if (box.clipOverflow) {
      textBox->overflow = Overflow::Hidden;
    }
    if (isVertical) {
      textBox->writingMode = WritingMode::Vertical;
    }
    for (size_t i = 0; i < fragments.size(); i++) {
      const auto& f = fragments[i];
      if (i == 0) {
        textBox->elements.push_back(buildTextElement(f));
        textBox->elements.push_back(buildTextFill(f));
      } else {
        auto group = _document->makeNode<Group>();
        group->elements.push_back(buildTextElement(f));
        group->elements.push_back(buildTextFill(f));
        textBox->elements.push_back(group);
      }
    }
    textHost->contents.push_back(textBox);
  }

  std::string deco = ToLower(Trim(inherited.textDecoration));
  if (!deco.empty() && deco != "none") {
    Color textColor = fragments.front().color;
    Color decorationColor = textColor;
    bool decorationColorDiffers = false;
    if (!inherited.textDecorationColor.empty()) {
      Color parsed = parseColor(inherited.textDecorationColor);
      if (!(parsed == textColor)) {
        decorationColor = parsed;
        decorationColorDiffers = true;
      }
    }
    if (deco.find("underline") != std::string::npos) {
      emitTextDecorationLine(textHost, textColor, decorationColor, decorationColorDiffers,
                             /*bottom=*/0.0f, /*centerY=*/NAN);
    }
    if (deco.find("line-through") != std::string::npos) {
      emitTextDecorationLine(textHost, textColor, decorationColor, decorationColorDiffers,
                             /*bottom=*/NAN, /*centerY=*/0.0f);
    }
    if (deco.find("overline") != std::string::npos) {
      warn("html: text-decoration overline not supported");
    }
  }

  assignElementId(wrapper, element);
  return wrapper;
}

}  // namespace pagx
