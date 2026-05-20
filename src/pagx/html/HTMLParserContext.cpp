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

#include "pagx/html/HTMLParserContext.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "pagx/HTMLSubsetTransformer.h"
#include "pagx/html/HTMLDetail.h"
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

  // Determine canvas size before constructing the document so that we can publish a
  // diagnostic entry into PAGXDocument::errors when sizing fails. The document is
  // constructed in either case so that callers can read diagnostics.
  float canvasW = 0;
  float canvasH = 0;
  bool canvasResolved = resolveCanvasSize(body, canvasW, canvasH);
  _canvasWidth = canvasW;
  _canvasHeight = canvasH;
  _document = PAGXDocument::Make(canvasResolved ? canvasW : 0.0f, canvasResolved ? canvasH : 0.0f);

  // Now that the document exists, flush any pending diagnostics gathered before its creation.
  for (auto& msg : _pendingDiagnostics) {
    _document->errors.push_back(std::move(msg));
  }
  _pendingDiagnostics.clear();

  if (!canvasResolved) {
    hardError("html: failed to determine canvas size");
    return nullptr;
  }

  // Collect <style> rules from <head>.
  if (head) {
    collectStyles(head);
  }

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
  return _document;
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
  std::unordered_map<std::string, std::string> props;
  auto* styleAttr = body->findAttribute("style");
  if (styleAttr) {
    ParseStyleString(*styleAttr, props);
  }
  auto* classAttr = body->findAttribute("class");
  if (classAttr) {
    mergeClassRules(*classAttr, props);
  }
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
  auto child = node->firstChild;
  while (child) {
    collectAllIds(child, depth + 1);
    child = child->nextSibling;
  }
}

std::string HTMLParserContext::generateUniqueId(const std::string& prefix) {
  std::string id;
  do {
    id = prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0 || (_document && _document->findNode(id) != nullptr));
  _existingIds.insert(id);
  return id;
}

std::string HTMLParserContext::consumeId(const std::shared_ptr<DOMNode>& element) {
  auto* idAttr = element->findAttribute("id");
  if (!idAttr || idAttr->empty()) return {};
  std::string id = *idAttr;
  // The id must be unique inside the PAGX document. Since IDs are author-controlled here,
  // we trust the source unless it would collide with an already-bound node.
  if (_document && _document->findNode(id) != nullptr) {
    warn("html: duplicate id '" + id + "' on element; renaming");
    id = generateUniqueId(id + "_");
  }
  return id;
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
    }
    child = child->getNextSibling();
  }
  assignElementId(layer, body);
  return layer;
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
    text->fontFamily =
        inherited.fontFamily.empty() ? HTML_DEFAULT_FONT_FAMILY : inherited.fontFamily;
    text->fontSize = HTML_DEFAULT_FONT_SIZE;
    layer->contents.push_back(text);
    return layer;
  }
  if (tag == "svg") {
    HTMLBoxAttributes box = resolveBox(element);
    return convertInlineSvg(element, box);
  }
  if (tag == "img") {
    HTMLBoxAttributes box = resolveBox(element);
    return convertImage(element, box);
  }

  HTMLInheritedStyle childInherited = computeInherited(element, inherited);
  HTMLBoxAttributes box = resolveBox(element);

  if (IsContainerTag(tag)) {
    return convertContainer(element, box, childInherited, depth);
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
      return convertContainer(element, box, childInherited, depth);
    }
    return convertTextLeaf(element, box, childInherited);
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

  // Must run after applyBackgroundVisuals(): the fold reuses the rounded Rectangle just
  // emitted into `layer->contents` as the image's fill geometry.
  if (foldRoundedImageWrapper(element, box, layer)) {
    return layer;
  }

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
  assignElementId(layer, element);
  return layer;
}

//==================================================================================================
// Text fragment collection / text leaf conversion
//==================================================================================================

HTMLParserContext::TextFragment HTMLParserContext::makeTextFragment(
    const HTMLInheritedStyle& inherited) {
  TextFragment frag;
  frag.fontFamily = inherited.fontFamily.empty() ? HTML_DEFAULT_FONT_FAMILY : inherited.fontFamily;
  frag.fontStyleName = inherited.fontStyleName;
  frag.fontSize = inherited.fontSizePx;
  frag.letterSpacing = inherited.letterSpacingPx;
  frag.color = inherited.resolvedTextColor;
  frag.textDecoration = inherited.textDecoration;
  frag.fillImage = inherited.textFillImage;
  return frag;
}

bool HTMLParserContext::fragmentsShareStyle(const TextFragment& a, const TextFragment& b) {
  return a.fontFamily == b.fontFamily && a.fontStyleName == b.fontStyleName &&
         a.fontSize == b.fontSize && a.letterSpacing == b.letterSpacing && a.color == b.color &&
         a.textDecoration == b.textDecoration && a.fillImage == b.fillImage;
}

void HTMLParserContext::appendTextFragment(std::vector<TextFragment>& out,
                                           const HTMLInheritedStyle& inherited, std::string text) {
  if (text.empty()) return;
  TextFragment frag = makeTextFragment(inherited);
  if (!out.empty() && fragmentsShareStyle(out.back(), frag)) {
    out.back().text += text;
    return;
  }
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
      appendTextFragment(out, inherited, child->name);
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
  size_t writeIndex = 0;
  for (size_t i = 0; i < fragments.size(); ++i) {
    fragments[i].text = CollapseHTMLWhitespace(fragments[i].text);
    if (fragments[i].text.empty()) continue;
    if (writeIndex != i) {
      fragments[writeIndex] = std::move(fragments[i]);
    }
    ++writeIndex;
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
  bool needsTextBox = hasMultipleFragments || !inherited.textAlign.empty() ||
                      !inherited.lineHeight.empty() || box.clipOverflow || hasNoWrap;

  auto outer = _document->makeNode<Layer>();
  applySizeAndPosition(outer, box);
  applyLayerAttributes(outer, element, box);

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
    if (hasNoWrap) {
      textBox->wordWrap = false;
    }
    if (box.clipOverflow) {
      textBox->overflow = Overflow::Hidden;
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

  assignElementId(outer, element);
  return outer;
}

}  // namespace pagx
