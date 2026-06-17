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
#include "pagx/html/importer/HTMLSubsetTransformer.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

//==================================================================================================
// HTMLParserContext: top-level traversal
//==================================================================================================

void HTMLParserContext::RecordFontFallbacksThunk(void* userData,
                                                 const std::vector<std::string>& chain) {
  static_cast<HTMLParserContext*>(userData)->recordFontFallbacks(chain);
}

HTMLParserContext::HTMLParserContext(const HTMLImporter::Options& options) : _options(options) {
  _diagnostics = std::make_unique<HTMLDiagnosticSink>(_options.strict);
  _idAllocator = std::make_unique<HTMLIdAllocator>();
  _valueParser = std::make_unique<HTMLValueParser>(*_diagnostics, _canvasWidth, _canvasHeight);
  _imageResources = std::make_unique<HTMLImageResources>(*_idAllocator);
  _svgEmitter = std::make_unique<HTMLInlineSvgEmitter>();
  _styleCascade = std::make_unique<HTMLStyleCascade>(*_diagnostics, *_valueParser);
  _layerBuilder = std::make_unique<HTMLLayerBuilder>(*_diagnostics, *_valueParser);
  _textFragmentBuilder = std::make_unique<HTMLTextFragmentBuilder>(
      *_diagnostics, *_valueParser, *_layerBuilder, *_styleCascade, *_idAllocator);
  // Forward cascade-discovered font-family chains into the document-wide fallback pool.
  _styleCascade->setFontFallbackSink(&HTMLParserContext::RecordFontFallbacksThunk, this);
}

std::shared_ptr<PAGXDocument> HTMLParserContext::parseFile(const std::string& filePath) {
  auto dom = XMLDOM::MakeFromFile(filePath);
  if (!dom) {
    return nullptr;
  }
  _imageResources->setBasePath(DirectoryOf(filePath));
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
  // through `_diagnostics` so they end up on PAGXDocument::errors once the document exists.
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
      _diagnostics->warn(formatted);
    }
    if (!report.ok) {
      _diagnostics->flagHardError();
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
    _styleCascade->collectStyles(head);
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

  // Now that the document exists, flush any diagnostics buffered before its creation.
  _diagnostics->bindDocument(_document.get());
  _valueParser->bindDocument(_document.get());
  _imageResources->bindDocument(_document.get());
  _layerBuilder->bindDocument(_document.get());
  _textFragmentBuilder->bindDocument(_document.get());

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

  _idAllocator->collectAll(root, *_diagnostics);

  Layer* bodyLayer = convertBody(body, canvasW, canvasH);
  if (_diagnostics->hadHardError()) {
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
// Diagnostics — short forwarders to `_diagnostics`, kept for the very common warn/hardError
// call sites where the unique_ptr-deref boilerplate would dominate the surrounding code.
//==================================================================================================

void HTMLParserContext::warn(const std::string& message) {
  _diagnostics->warn(message);
}

void HTMLParserContext::hardError(const std::string& message) {
  _diagnostics->hardError(message);
}

//==================================================================================================
// Canvas size resolution
//==================================================================================================

bool HTMLParserContext::resolveCanvasSize(const std::shared_ptr<DOMNode>& body, float& outW,
                                          float& outH) {
  bool haveTarget = !std::isnan(_options.targetWidth) && !std::isnan(_options.targetHeight);
  // Reuse getResolvedStyle so the body's resolved property map is parsed once and cached
  // for later computeBoxAttributes() in convertBody. This also means class rules and element rules
  // (collected just before this call) participate in canvas-size resolution.
  const auto& props = _styleCascade->getResolvedStyle(body);
  float bodyW = _valueParser->parseAbsoluteLengthPx(LookupProperty(props, "width"));
  float bodyH = _valueParser->parseAbsoluteLengthPx(LookupProperty(props, "height"));
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
// Body / element conversion
//==================================================================================================

Layer* HTMLParserContext::convertBody(const std::shared_ptr<DOMNode>& body, float canvasW,
                                      float canvasH) {
  HTMLInheritedStyle inherited = _styleCascade->resolveInheritedStyle(body, HTMLInheritedStyle{});
  HTMLBoxAttributes box = _styleCascade->computeBoxAttributes(body);

  auto layer = _document->makeNode<Layer>();
  layer->width = canvasW;
  layer->height = canvasH;
  layer->percentWidth = NAN;
  layer->percentHeight = NAN;

  bool hasBgVisuals = HTMLLayerBuilder::hasBackgroundVisuals(box);
  if (hasBgVisuals) {
    _layerBuilder->applyBackgroundVisuals(layer, box);
  }
  _layerBuilder->applyLayerAttributes(layer, body, box);

  // Hoist any `box-shadow` off the clipped layer so `overflow: hidden` does not also clip the
  // shadow. `wrapper` is `layer` when no split happens.
  Layer* wrapper = _layerBuilder->maybeSplitBoxShadowFromClip(layer);

  Layer* contentHost = layer;
  bool needsInnerHost = hasBgVisuals && (box.paddingSet || box.displayFlex);
  if (needsInnerHost) {
    contentHost = _layerBuilder->createInnerHost(layer, box);
  } else {
    _layerBuilder->applyLayoutAttributes(layer, box);
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
  _idAllocator->assign(wrapper, body);
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
    HTMLBoxAttributes box = _styleCascade->computeBoxAttributes(element);
    return _layerBuilder->wrapForMargin(convertInlineSvg(element, box, inherited), box);
  }
  if (tag == "img") {
    HTMLBoxAttributes box = _styleCascade->computeBoxAttributes(element);
    return _layerBuilder->wrapForMargin(convertImage(element, box), box);
  }

  HTMLInheritedStyle childInherited = _styleCascade->resolveInheritedStyle(element, inherited);
  HTMLBoxAttributes box = _styleCascade->computeBoxAttributes(element);

  if (IsContainerTag(tag)) {
    return _layerBuilder->wrapForMargin(convertContainer(element, box, childInherited, depth), box);
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
      return _layerBuilder->wrapForMargin(convertContainer(element, box, childInherited, depth),
                                          box);
    }
    return _layerBuilder->wrapForMargin(convertTextLeaf(element, box, childInherited), box);
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
  bool hasBgVisuals = HTMLLayerBuilder::hasBackgroundVisuals(box);
  bool needsInner = hasBgVisuals && HTMLLayerBuilder::requiresInnerHost(box);

  auto layer = _document->makeNode<Layer>();
  _layerBuilder->applySizeAndPosition(layer, box);
  _layerBuilder->applyLayerAttributes(layer, element, box);

  if (hasBgVisuals) {
    _layerBuilder->applyBackgroundVisuals(layer, box);
  }

  _layerBuilder->applyBoxTransform(layer, box, element);

  // Must run after applyBackgroundVisuals(): the fold reuses the rounded Rectangle just
  // emitted into `layer->contents` as the image's fill geometry.
  if (foldRoundedImageWrapper(element, box, layer)) {
    return layer;
  }

  // Hoist any non-inset `box-shadow` off the clipping layer so `overflow: hidden` does not
  // cut the shadow. `wrapper` is `layer` when no split happens; both children added below
  // and the layout-host machinery continue to operate on `layer` (now potentially the
  // inner of the split).
  Layer* wrapper = _layerBuilder->maybeSplitBoxShadowFromClip(layer);

  Layer* contentHost = layer;
  if (needsInner) {
    contentHost = _layerBuilder->createInnerHost(layer, box);
  } else {
    _layerBuilder->applyLayoutAttributes(layer, box);
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
  _idAllocator->assign(wrapper, element);
  return wrapper;
}

//==================================================================================================
// Text leaf conversion — thin forwarder to `_textFragmentBuilder`.
//==================================================================================================

Layer* HTMLParserContext::convertTextLeaf(const std::shared_ptr<DOMNode>& element,
                                          const HTMLBoxAttributes& box,
                                          const HTMLInheritedStyle& inherited) {
  return _textFragmentBuilder->convertTextLeaf(element, box, inherited);
}

}  // namespace pagx
