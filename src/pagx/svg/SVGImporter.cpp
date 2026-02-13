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

#include "pagx/SVGImporter.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include "StringParser.h"
#include "SVGPathParser.h"
#include "MathUtil.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/SolidColor.h"
#include "SVGParserInternal.h"
#include "xml/XMLDOM.h"

namespace pagx {

static constexpr float DEFAULT_FONT_SIZE = 16.0f;

std::shared_ptr<PAGXDocument> SVGImporter::Parse(const std::string& filePath,
                                             const Options& options) {
  SVGParserContext parser(options);
  auto doc = parser.parseFile(filePath);
  if (doc) {
    // Convert relative paths to absolute paths
    std::string basePath = {};
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      basePath = filePath.substr(0, lastSlash + 1);
    }
    if (!basePath.empty()) {
      for (auto& node : doc->nodes) {
        if (node->nodeType() == NodeType::Image) {
          auto* image = static_cast<Image*>(node.get());
          if (!image->filePath.empty() && image->filePath[0] != '/' &&
              image->filePath.find("://") == std::string::npos &&
              image->filePath.find("data:") != 0) {
            image->filePath = basePath + image->filePath;
          }
        }
      }
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> SVGImporter::Parse(const uint8_t* data, size_t length,
                                             const Options& options) {
  SVGParserContext parser(options);
  return parser.parse(data, length);
}

std::shared_ptr<PAGXDocument> SVGImporter::ParseString(const std::string& svgContent,
                                                   const Options& options) {
  return Parse(reinterpret_cast<const uint8_t*>(svgContent.data()), svgContent.size(), options);
}

// ============== SVGParserContext ==============

SVGParserContext::SVGParserContext(const SVGImporter::Options& options) : _options(options) {
}

std::shared_ptr<PAGXDocument> SVGParserContext::parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }

  auto dom = DOM::Make(data, length);
  if (!dom) {
    return nullptr;
  }

  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> SVGParserContext::parseFile(const std::string& filePath) {
  auto dom = DOM::MakeFromFile(filePath);
  if (!dom) {
    return nullptr;
  }

  return parseDOM(dom);
}

// Parse a CSS style string into a property map. Later properties override earlier ones.
// Handles parenthesized values (e.g., color(display-p3 ...)) by tracking parenthesis depth.
static void ParseStyleString(const std::string& styleStr,
                             std::unordered_map<std::string, std::string>& out) {
  size_t pos = 0;
  while (pos < styleStr.size()) {
    while (pos < styleStr.size() && std::isspace(styleStr[pos])) {
      ++pos;
    }
    // Skip CSS comments.
    if (pos + 1 < styleStr.size() && styleStr[pos] == '/' && styleStr[pos + 1] == '*') {
      auto commentEnd = styleStr.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? styleStr.size() : commentEnd + 2;
      continue;
    }
    size_t colonPos = styleStr.find(':', pos);
    if (colonPos == std::string::npos) {
      break;
    }
    std::string propName = styleStr.substr(pos, colonPos - pos);
    size_t propStart = propName.find_first_not_of(" \t");
    size_t propEnd = propName.find_last_not_of(" \t");
    if (propStart != std::string::npos && propEnd != std::string::npos) {
      propName = propName.substr(propStart, propEnd - propStart + 1);
    }
    size_t searchStart = colonPos + 1;
    size_t semicolonPos = std::string::npos;
    int parenDepth = 0;
    for (size_t i = searchStart; i < styleStr.size(); i++) {
      if (styleStr[i] == '(') {
        parenDepth++;
      } else if (styleStr[i] == ')') {
        parenDepth--;
      } else if (styleStr[i] == ';' && parenDepth == 0) {
        semicolonPos = i;
        break;
      }
    }
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }
    std::string propValue = styleStr.substr(colonPos + 1, semicolonPos - colonPos - 1);
    size_t valStart = propValue.find_first_not_of(" \t");
    size_t valEnd = propValue.find_last_not_of(" \t");
    if (valStart != std::string::npos && valEnd != std::string::npos) {
      propValue = propValue.substr(valStart, valEnd - valStart + 1);
    }
    if (!propName.empty() && !propValue.empty()) {
      out[propName] = propValue;
    }
    pos = semicolonPos + 1;
  }
}

std::unordered_map<std::string, std::string> SVGParserContext::getStyleProperties(
    const std::shared_ptr<DOMNode>& node) {
  std::unordered_map<std::string, std::string> result = {};

  // CSS class rules (lowest priority): parse all matching classes.
  auto* classPtr = node->findAttribute("class");
  if (classPtr && !classPtr->empty()) {
    const std::string& classAttr = *classPtr;
    size_t pos = 0;
    while (pos < classAttr.size()) {
      while (pos < classAttr.size() && std::isspace(classAttr[pos])) {
        pos++;
      }
      if (pos >= classAttr.size()) {
        break;
      }
      size_t endPos = pos;
      while (endPos < classAttr.size() && !std::isspace(classAttr[endPos])) {
        endPos++;
      }
      std::string className = classAttr.substr(pos, endPos - pos);
      pos = endPos;
      auto classIt = _cssClassRules.find(className);
      if (classIt != _cssClassRules.end()) {
        ParseStyleString(classIt->second, result);
      }
    }
  }

  // Style attribute (highest priority): overwrites class rules.
  auto* stylePtr = node->findAttribute("style");
  if (stylePtr && !stylePtr->empty()) {
    ParseStyleString(*stylePtr, result);
  }

  return result;
}

std::string SVGParserContext::getAttribute(const std::shared_ptr<DOMNode>& node,
                                        const std::string& name,
                                        const std::string& defaultValue) {
  // CSS priority: style attribute > presentation attribute > CSS class rules
  // The cached style properties map merges both style attribute and class rules,
  // with style attribute values overriding class rule values.
  const auto& styleProps = getStyleProperties(node);
  auto styleIt = styleProps.find(name);
  if (styleIt != styleProps.end()) {
    return styleIt->second;
  }

  // Fallback: check for direct attribute (presentation attribute).
  auto* directValue = node->findAttribute(name);
  if (directValue) {
    return *directValue;
  }

  return defaultValue;
}

std::string SVGParserContext::getHrefAttribute(const std::shared_ptr<DOMNode>& node) {
  auto href = getAttribute(node, "href");
  if (!href.empty()) {
    return href;
  }
  return getAttribute(node, "xlink:href");
}

std::shared_ptr<PAGXDocument> SVGParserContext::parseDOM(const std::shared_ptr<DOM>& dom) {
  auto root = dom->getRootNode();
  if (!root || root->name != "svg") {
    return nullptr;
  }

  // Parse viewBox and dimensions.
  // When viewBox is present, use viewBox dimensions for the PAGX document size,
  // because PAGX doesn't support viewBox and all SVG coordinates are in viewBox space.
  // The explicit width/height with unit conversions (e.g., "1080pt" -> 1440px) are ignored
  // to avoid coordinate mismatch.
  auto viewBox = parseViewBox(getAttribute(root, "viewBox"));
  float width = 0;
  float height = 0;

  if (viewBox.size() >= 4) {
    _viewBoxWidth = viewBox[2];
    _viewBoxHeight = viewBox[3];
    width = _viewBoxWidth;
    height = _viewBoxHeight;
  } else {
    width = parseLength(getAttribute(root, "width"), 0);
    height = parseLength(getAttribute(root, "height"), 0);
    _viewBoxWidth = width;
    _viewBoxHeight = height;
  }

  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  _document = PAGXDocument::Make(width, height);

  // Collect all IDs from the SVG to avoid conflicts when generating new IDs.
  collectAllIds(root);

  // First pass: collect defs.
  auto child = root->getFirstChild();
  while (child) {
    if (child->name == "defs") {
      parseDefs(child);
    }
    child = child->getNextSibling();
  }

  // Parse <style> elements (can be in defs or directly under svg).
  child = root->getFirstChild();
  while (child) {
    if (child->name == "style") {
      parseStyleElement(child);
    } else if (child->name == "defs") {
      // Also check for <style> inside <defs>.
      auto defsChild = child->getFirstChild();
      while (defsChild) {
        if (defsChild->name == "style") {
          parseStyleElement(defsChild);
        }
        defsChild = defsChild->getNextSibling();
      }
    }
    child = child->getNextSibling();
  }

  // Second pass: count references to gradients/patterns.
  // This determines which ColorSources should be extracted to resources.
  countColorSourceReferences(root);

  // Handle viewBox offset if present (viewBox origin is not 0,0).
  bool needsViewBoxTransform = false;
  Matrix viewBoxMatrix = Matrix::Identity();
  if (viewBox.size() >= 4) {
    float viewBoxX = viewBox[0];
    float viewBoxY = viewBox[1];

    if (viewBoxX != 0 || viewBoxY != 0) {
      // Only translate for non-zero viewBox origin.
      viewBoxMatrix = Matrix::Translate(-viewBoxX, -viewBoxY);
      needsViewBoxTransform = true;
    }
  }

  // Compute initial inherited style from the root <svg> element.
  InheritedStyle rootStyle = {};
  rootStyle = computeInheritedStyle(root, rootStyle);

  // Collect converted layers.
  std::vector<Layer*> convertedLayers;
  child = root->getFirstChild();
  while (child) {
    if (child->name != "defs") {
      auto layer = convertToLayer(child, rootStyle);
      if (layer) {
        convertedLayers.push_back(layer);
      }
    }
    child = child->getNextSibling();
  }

  // Add collected mask layers (invisible, used as mask references) before other layers.
  if (!_maskLayers.empty()) {
    std::vector<Layer*> allLayers;
    allLayers.reserve(_maskLayers.size() + convertedLayers.size());
    allLayers.insert(allLayers.end(), _maskLayers.begin(), _maskLayers.end());
    allLayers.insert(allLayers.end(), convertedLayers.begin(), convertedLayers.end());
    convertedLayers = std::move(allLayers);
    _maskLayers.clear();
  }

  // Merge adjacent layers with the same geometry (optimize Fill + Stroke into one Layer).
  mergeAdjacentLayers(convertedLayers);

  // If viewBox transform is needed, wrap in a root layer with the transform.
  // Otherwise, add layers directly to document (no root wrapper).
  if (needsViewBoxTransform) {
    auto rootLayer = _document->makeNode<Layer>();
    rootLayer->matrix = viewBoxMatrix;
    for (auto& layer : convertedLayers) {
      rootLayer->children.push_back(layer);
    }
    _document->layers.push_back(rootLayer);
  } else {
    for (auto& layer : convertedLayers) {
      _document->layers.push_back(layer);
    }
  }

  return _document;
}

InheritedStyle SVGParserContext::computeInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                                    const InheritedStyle& parentStyle) {
  InheritedStyle style = parentStyle;

  static const std::pair<const char*, std::string InheritedStyle::*> StyleProperties[] = {
      {"fill", &InheritedStyle::fill},
      {"stroke", &InheritedStyle::stroke},
      {"fill-opacity", &InheritedStyle::fillOpacity},
      {"stroke-opacity", &InheritedStyle::strokeOpacity},
      {"fill-rule", &InheritedStyle::fillRule},
      {"stroke-dasharray", &InheritedStyle::strokeDasharray},
      {"stroke-dashoffset", &InheritedStyle::strokeDashoffset},
      {"stroke-width", &InheritedStyle::strokeWidth},
      {"stroke-linecap", &InheritedStyle::strokeLinecap},
      {"stroke-linejoin", &InheritedStyle::strokeLinejoin},
      {"stroke-miterlimit", &InheritedStyle::strokeMiterlimit},
      {"font-family", &InheritedStyle::fontFamily},
      {"font-size", &InheritedStyle::fontSize},
      {"font-weight", &InheritedStyle::fontWeight},
      {"font-style", &InheritedStyle::fontStyle},
      {"letter-spacing", &InheritedStyle::letterSpacing},
      {"text-anchor", &InheritedStyle::textAnchor},
  };

  for (const auto& [propName, memberPtr] : StyleProperties) {
    auto value = getAttribute(element, propName);
    if (!value.empty()) {
      style.*memberPtr = value;
    }
  }

  return style;
}

void SVGParserContext::parseDefs(const std::shared_ptr<DOMNode>& defsNode) {
  auto child = defsNode->getFirstChild();
  while (child) {
    std::string id = getAttribute(child, "id");
    if (!id.empty()) {
      _defs[id] = child;
    }
    // Also handle nested defs.
    if (child->name == "defs") {
      parseDefs(child);
    }
    child = child->getNextSibling();
  }
}

void SVGParserContext::parseStyleElement(const std::shared_ptr<DOMNode>& styleNode) {
  // Get the text content of the <style> element.
  auto textNode = styleNode->getFirstChild();
  if (!textNode || textNode->type != DOMNodeType::Text) {
    return;
  }

  // The text content is stored in the node's name for text nodes.
  std::string cssContent = textNode->name;
  if (cssContent.empty()) {
    return;
  }

  // Parse CSS rules.
  // Format: .class-name { property1: value1; property2: value2; }
  size_t pos = 0;
  while (pos < cssContent.size()) {
    // Skip whitespace.
    while (pos < cssContent.size() && std::isspace(cssContent[pos])) {
      pos++;
    }
    if (pos >= cssContent.size()) {
      break;
    }

    // Skip CSS comments.
    if (pos + 1 < cssContent.size() && cssContent[pos] == '/' && cssContent[pos + 1] == '*') {
      auto commentEnd = cssContent.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? cssContent.size() : commentEnd + 2;
      continue;
    }

    // Look for class selector starting with dot.
    if (cssContent[pos] != '.') {
      // Skip non-class selectors (find next rule).
      size_t bracePos = cssContent.find('{', pos);
      if (bracePos == std::string::npos) {
        break;
      }
      size_t closeBracePos = cssContent.find('}', bracePos);
      if (closeBracePos == std::string::npos) {
        break;
      }
      pos = closeBracePos + 1;
      continue;
    }

    // Found a class selector.
    pos++;  // Skip the dot.

    // Extract class name (until whitespace or {).
    size_t classNameEnd = pos;
    while (classNameEnd < cssContent.size() && !std::isspace(cssContent[classNameEnd]) &&
           cssContent[classNameEnd] != '{') {
      classNameEnd++;
    }
    std::string className = cssContent.substr(pos, classNameEnd - pos);

    // Find the opening brace.
    size_t bracePos = cssContent.find('{', classNameEnd);
    if (bracePos == std::string::npos) {
      break;
    }

    // Find the closing brace.
    size_t closeBracePos = cssContent.find('}', bracePos);
    if (closeBracePos == std::string::npos) {
      break;
    }

    // Extract the style content between braces.
    std::string styleContent = cssContent.substr(bracePos + 1, closeBracePos - bracePos - 1);

    // Trim whitespace and newlines from style content.
    size_t contentStart = styleContent.find_first_not_of(" \t\n\r");
    size_t contentEnd = styleContent.find_last_not_of(" \t\n\r");
    if (contentStart != std::string::npos && contentEnd != std::string::npos) {
      styleContent = styleContent.substr(contentStart, contentEnd - contentStart + 1);
    }

    // Store the class rule.
    if (!className.empty() && !styleContent.empty()) {
      _cssClassRules[className] = styleContent;
    }

    pos = closeBracePos + 1;
  }
}

Layer* SVGParserContext::convertToLayer(const std::shared_ptr<DOMNode>& element,
                                                     const InheritedStyle& parentStyle, int depth) {
  if (depth >= MAX_SVG_RECURSION_DEPTH) {
    return nullptr;
  }
  const auto& tag = element->name;

  if (tag == "defs" || tag == "linearGradient" || tag == "radialGradient" || tag == "pattern" ||
      tag == "mask" || tag == "clipPath" || tag == "filter" || tag == "style" || tag == "title" ||
      tag == "desc" || tag == "metadata") {
    return nullptr;
  }

  // Skip invisible elements (display:none or visibility:hidden).
  // Mask layers are handled separately via convertMaskElement.
  std::string display = getAttribute(element, "display");
  if (display == "none") {
    return nullptr;
  }
  std::string visibility = getAttribute(element, "visibility");
  if (visibility == "hidden") {
    return nullptr;
  }

  // Compute inherited style for this element.
  InheritedStyle inheritedStyle = computeInheritedStyle(element, parentStyle);

  auto layer = _document->makeNode<Layer>();

  // Parse common layer attributes.
  layer->id = getAttribute(element, "id");

  // Parse data-* custom attributes.
  parseCustomData(element, layer);

  std::string transform = getAttribute(element, "transform");
  if (!transform.empty()) {
    layer->matrix = parseTransform(transform);
  }

  std::string opacity = getAttribute(element, "opacity");
  if (!opacity.empty()) {
    layer->alpha = strtof(opacity.c_str(), nullptr);
  }

  // Handle mask attribute.
  std::string maskAttr = getAttribute(element, "mask");
  if (!maskAttr.empty() && maskAttr != "none") {
    std::string maskId = resolveUrl(maskAttr);
    auto maskIt = _defs.find(maskId);
    if (maskIt != _defs.end()) {
      // Convert mask element to a mask layer.
      auto maskLayer = convertMaskElement(maskIt->second, inheritedStyle);
      if (maskLayer) {
        layer->mask = maskLayer;
        // SVG masks use luminance by default.
        layer->maskType = MaskType::Luminance;
        // Add mask layer as invisible layer to the document.
        _maskLayers.push_back(maskLayer);
      }
    }
  }

  // Handle clip-path attribute (similar to mask but uses contour/alpha).
  std::string clipPathAttr = getAttribute(element, "clip-path");
  if (!clipPathAttr.empty() && clipPathAttr != "none") {
    std::string clipId = resolveUrl(clipPathAttr);
    auto clipIt = _defs.find(clipId);
    if (clipIt != _defs.end()) {
      // Check if the clipPath is a single full-canvas rect, which has no visual effect.
      bool isFullCanvasClip = false;
      auto clipElement = clipIt->second;
      auto firstChild = clipElement->getFirstChild();
      if (firstChild && !firstChild->getNextSibling() && firstChild->name == "rect") {
        std::string rectTransform = getAttribute(firstChild, "transform");
        if (rectTransform.empty()) {
          float cx = parseLength(getAttribute(firstChild, "x"), _viewBoxWidth);
          float cy = parseLength(getAttribute(firstChild, "y"), _viewBoxHeight);
          float cw = parseLength(getAttribute(firstChild, "width"), _viewBoxWidth);
          float ch = parseLength(getAttribute(firstChild, "height"), _viewBoxHeight);
          if (cx <= 0 && cy <= 0 && cw >= _viewBoxWidth && ch >= _viewBoxHeight) {
            isFullCanvasClip = true;
          }
        }
      }
      if (!isFullCanvasClip) {
        // Convert clipPath element to a mask layer.
        auto clipLayer = convertMaskElement(clipElement, inheritedStyle);
        if (clipLayer) {
          layer->mask = clipLayer;
          // SVG clip-path uses alpha (shape outline) for clipping.
          layer->maskType = MaskType::Alpha;
          // Add clip layer as invisible layer to the document.
          _maskLayers.push_back(clipLayer);
        }
      }
    }
  }

  // Handle filter attribute.
  std::string filterAttr = getAttribute(element, "filter");
  ShadowOnlyType shadowOnlyType = ShadowOnlyType::None;
  if (!filterAttr.empty() && filterAttr != "none") {
    std::string filterId = resolveUrl(filterAttr);
    auto filterIt = _defs.find(filterId);
    if (filterIt != _defs.end()) {
      bool filterConverted = convertFilterElement(filterIt->second, layer->filters, layer->styles,
                                                  &shadowOnlyType);
      if (!filterConverted) {
        // Filter could not be fully converted to PAGX format.
        if (shadowOnlyType != ShadowOnlyType::None) {
          // For shadowOnly filters (e.g., inner shadow), the element exists solely to produce
          // the filter effect. If the filter cannot be converted, skip the entire element
          // to avoid rendering incorrect results (e.g., black fill instead of shadow effect).
          return nullptr;
        }
        // For non-shadowOnly filters, continue rendering the element without the filter effect.
        layer->filters.clear();
        layer->styles.clear();
      }
    }
  }

  // Convert contents.
  if (tag == "g" || tag == "svg") {
    // Group: convert children as child layers.
    auto child = element->getFirstChild();
    while (child) {
      auto childLayer = convertToLayer(child, inheritedStyle, depth + 1);
      if (childLayer) {
        layer->children.push_back(childLayer);
      }
      child = child->getNextSibling();
    }
    // Skip empty <g> elements that have no renderable content and no visual effects.
    if (tag == "g" && layer->children.empty() && layer->contents.empty() &&
        layer->mask == nullptr && layer->filters.empty() && layer->styles.empty()) {
      return nullptr;
    }
  } else {
    // Shape element: convert to vector contents.
    // Pass the shadow-only type to determine how to handle fill.
    convertChildren(element, layer->contents, inheritedStyle, shadowOnlyType);
  }

  return layer;
}

void SVGParserContext::convertChildren(const std::shared_ptr<DOMNode>& element,
                                    std::vector<Element*>& contents,
                                    const InheritedStyle& inheritedStyle,
                                    ShadowOnlyType shadowOnlyType) {
  const auto& tag = element->name;

  // Handle text element specially - it returns a Group with Text.
  if (tag == "text") {
    auto textGroup = convertText(element, inheritedStyle);
    if (textGroup) {
      contents.push_back(textGroup);
    }
    return;
  }

  // Check if this is a use element referencing an image.
  // In that case, we don't add fill/stroke because the image already has its own fill.
  bool skipFillStroke = false;
  if (tag == "use") {
    std::string refId = resolveUrl(getHrefAttribute(element));
    auto it = _defs.find(refId);
    if (it != _defs.end() && it->second->name == "image") {
      skipFillStroke = true;
    }
  }

  auto shapeElement = convertElement(element);
  if (shapeElement) {
    contents.push_back(shapeElement);
  }

  if (!skipFillStroke) {
    if (shadowOnlyType == ShadowOnlyType::DropShadow) {
      // For shadowOnly DropShadow filters, we need to add a fill to provide a source for the shadow.
      // The DropShadowFilter generates shadow based on the layer's content alpha channel.
      // We use black (alpha=1) as the fill color since the actual shadow color is determined
      // by the DropShadowFilter's color property, not the fill color.
      auto fillNode = _document->makeNode<Fill>();
      auto solidColor = _document->makeNode<SolidColor>();
      solidColor->color = {0, 0, 0, 1, ColorSpace::SRGB};
      fillNode->color = solidColor;
      contents.push_back(fillNode);
    } else if (shadowOnlyType == ShadowOnlyType::InnerShadow) {
      // For shadowOnly InnerShadow filters, no fill is needed.
      // The InnerShadowFilter draws shadow inside the layer's alpha boundary,
      // using only the shape geometry as the clipping region.
    } else {
      addFillStroke(element, contents, inheritedStyle);
    }
  }
}

Element* SVGParserContext::convertElement(
    const std::shared_ptr<DOMNode>& element) {
  const auto& tag = element->name;

  if (tag == "rect") {
    return convertRect(element);
  } else if (tag == "circle") {
    return convertCircle(element);
  } else if (tag == "ellipse") {
    return convertEllipse(element);
  } else if (tag == "line") {
    return convertLine(element);
  } else if (tag == "polyline") {
    return convertPolyline(element);
  } else if (tag == "polygon") {
    return convertPolygon(element);
  } else if (tag == "path") {
    return convertPath(element);
  } else if (tag == "use") {
    return convertUse(element);
  }

  return nullptr;
}

Element* SVGParserContext::convertRect(
    const std::shared_ptr<DOMNode>& element) {
  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
  float width = parseLength(getAttribute(element, "width"), _viewBoxWidth);
  float height = parseLength(getAttribute(element, "height"), _viewBoxHeight);
  float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
  float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);

  if (rx == 0) {
    rx = ry;
  }
  if (ry == 0) {
    ry = rx;
  }

  auto rect = _document->makeNode<Rectangle>();
  rect->center.x = x + width / 2;
  rect->center.y = y + height / 2;
  rect->size.width = width;
  rect->size.height = height;
  rect->roundness = std::max(rx, ry);

  return rect;
}

Element* SVGParserContext::convertCircle(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float r = parseLength(getAttribute(element, "r"), _viewBoxWidth);

  auto ellipse = _document->makeNode<Ellipse>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = r * 2;
  ellipse->size.height = r * 2;

  return ellipse;
}

Element* SVGParserContext::convertEllipse(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
  float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);

  auto ellipse = _document->makeNode<Ellipse>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = rx * 2;
  ellipse->size.height = ry * 2;

  return ellipse;
}

Element* SVGParserContext::convertLine(
    const std::shared_ptr<DOMNode>& element) {
  float x1 = parseLength(getAttribute(element, "x1"), _viewBoxWidth);
  float y1 = parseLength(getAttribute(element, "y1"), _viewBoxHeight);
  float x2 = parseLength(getAttribute(element, "x2"), _viewBoxWidth);
  float y2 = parseLength(getAttribute(element, "y2"), _viewBoxHeight);

  auto path = _document->makeNode<Path>();
  auto pathData = _document->makeNode<PathData>();
  pathData->moveTo(x1, y1);
  pathData->lineTo(x2, y2);
  if (!pathData->isEmpty()) {
    registerPathDataResource(pathData);
    path->data = pathData;
  }

  return path;
}

Element* SVGParserContext::convertPolyline(
    const std::shared_ptr<DOMNode>& element) {
  auto path = _document->makeNode<Path>();
  auto pathDataNode = _document->makeNode<PathData>();
  *pathDataNode = parsePoints(getAttribute(element, "points"), false);
  if (!pathDataNode->isEmpty()) {
    registerPathDataResource(pathDataNode);
    path->data = pathDataNode;
  }
  return path;
}

Element* SVGParserContext::convertPolygon(
    const std::shared_ptr<DOMNode>& element) {
  auto path = _document->makeNode<Path>();
  auto pathDataNode = _document->makeNode<PathData>();
  *pathDataNode = parsePoints(getAttribute(element, "points"), true);
  if (!pathDataNode->isEmpty()) {
    registerPathDataResource(pathDataNode);
    path->data = pathDataNode;
  }
  return path;
}

Element* SVGParserContext::convertPath(
    const std::shared_ptr<DOMNode>& element) {
  auto path = _document->makeNode<Path>();
  std::string d = getAttribute(element, "d");
  if (!d.empty()) {
    auto pathDataNode = _document->makeNode<PathData>();
    *pathDataNode = PathDataFromSVGString(d);
    if (!pathDataNode->isEmpty()) {
      registerPathDataResource(pathDataNode);
      path->data = pathDataNode;
    }
  }
  return path;
}

Group* SVGParserContext::convertText(const std::shared_ptr<DOMNode>& element,
                                                  const InheritedStyle& inheritedStyle) {
  auto group = _document->makeNode<Group>();

  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);

  // Resolve font size early so that em-based dx/dy can be computed correctly.
  std::string fontSizeStr = getAttribute(element, "font-size");
  if (fontSizeStr.empty()) {
    fontSizeStr = inheritedStyle.fontSize;
  }
  float currentFontSize =
      fontSizeStr.empty() ? DEFAULT_FONT_SIZE : parseLength(fontSizeStr, _viewBoxHeight);

  // Parse dx/dy offsets. SVG dx/dy provide relative offsets to the text position.
  x += parseLengthEm(getAttribute(element, "dx"), _viewBoxWidth, currentFontSize);
  y += parseLengthEm(getAttribute(element, "dy"), _viewBoxHeight, currentFontSize);

  // Parse text-anchor attribute for horizontal alignment (use inherited if not set).
  // SVG values: start (default), middle, end.
  std::string anchor = getAttribute(element, "text-anchor");
  if (anchor.empty()) {
    anchor = inheritedStyle.textAnchor;
  }

  // Get text content from child text nodes and tspan elements.
  std::string textContent;
  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      textContent += child->name;
    } else if (child->name == "tspan") {
      // Handle tspan element: parse dx/dy offsets for positioning.
      x += parseLengthEm(getAttribute(child, "dx"), _viewBoxWidth, currentFontSize);
      y += parseLengthEm(getAttribute(child, "dy"), _viewBoxHeight, currentFontSize);
      // Extract text content from tspan children.
      auto tspanChild = child->getFirstChild();
      while (tspanChild) {
        if (tspanChild->type == DOMNodeType::Text) {
          textContent += tspanChild->name;
        }
        tspanChild = tspanChild->getNextSibling();
      }
    }
    child = child->getNextSibling();
  }

  // SVG whitespace normalization: replace newlines and tabs with spaces,
  // collapse consecutive spaces, and trim leading/trailing spaces.
  for (auto& ch : textContent) {
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
  }
  std::string normalized;
  normalized.reserve(textContent.size());
  bool previousWasSpace = true;
  for (char ch : textContent) {
    if (ch == ' ') {
      if (!previousWasSpace) {
        normalized += ch;
      }
      previousWasSpace = true;
    } else {
      normalized += ch;
      previousWasSpace = false;
    }
  }
  if (!normalized.empty() && normalized.back() == ' ') {
    normalized.pop_back();
  }
  textContent = std::move(normalized);

  if (!textContent.empty()) {
    auto text = _document->makeNode<Text>();
    text->position = {x, y};
    text->text = textContent;

    // Font family: element attribute > inherited style.
    std::string fontFamily = getAttribute(element, "font-family");
    if (fontFamily.empty()) {
      fontFamily = inheritedStyle.fontFamily;
    }
    if (!fontFamily.empty()) {
      text->fontFamily = fontFamily;
    }

    // Font size already resolved above.
    text->fontSize = currentFontSize;

    // Font weight: element attribute > inherited style.
    // SVG font-weight maps to fontStyle in PAGX (e.g., "Bold", "Light").
    std::string fontWeight = getAttribute(element, "font-weight");
    if (fontWeight.empty()) {
      fontWeight = inheritedStyle.fontWeight;
    }
    // Font style: element attribute > inherited style.
    std::string fontStyleAttr = getAttribute(element, "font-style");
    if (fontStyleAttr.empty()) {
      fontStyleAttr = inheritedStyle.fontStyle;
    }
    // Combine font-weight and font-style into fontStyle field.
    // This is a simplification; in practice, font selection is more complex.
    if (!fontWeight.empty() || !fontStyleAttr.empty()) {
      bool isBold = (fontWeight == "bold" || fontWeight == "700" || fontWeight == "800" ||
                     fontWeight == "900" || fontWeight == "bolder");
      bool isItalic = (fontStyleAttr == "italic" || fontStyleAttr == "oblique");
      if (isBold && isItalic) {
        text->fontStyle = "Bold Italic";
      } else if (isBold) {
        text->fontStyle = "Bold";
      } else if (isItalic) {
        text->fontStyle = "Italic";
      }
    }

    // Letter spacing: element attribute > inherited style.
    std::string letterSpacing = getAttribute(element, "letter-spacing");
    if (letterSpacing.empty()) {
      letterSpacing = inheritedStyle.letterSpacing;
    }
    if (!letterSpacing.empty()) {
      text->letterSpacing = parseLength(letterSpacing, _viewBoxWidth);
    }

    group->elements.push_back(text);

    // Add TextLayout modifier if text-anchor requires alignment.
    // SVG text-anchor maps to PAGX TextLayout.textAlign:
    //   start  -> Left (default, no TextLayout needed)
    //   middle -> Center
    //   end    -> End
    if (!anchor.empty() && anchor != "start") {
      auto textLayout = _document->makeNode<TextLayout>();
      textLayout->width = 0;   // auto-width (Point Text mode)
      textLayout->height = 0;  // auto-height
      if (anchor == "middle") {
        textLayout->textAlign = TextAlign::Center;
      } else if (anchor == "end") {
        textLayout->textAlign = TextAlign::End;
      }
      group->elements.push_back(textLayout);
    }
  }

  addFillStroke(element, group->elements, inheritedStyle);
  return group;
}

Element* SVGParserContext::convertUse(
    const std::shared_ptr<DOMNode>& element) {
  std::string refId = resolveUrl(getHrefAttribute(element));
  if (refId.empty() || _useStack.count(refId) > 0) {
    return nullptr;
  }
  auto it = _defs.find(refId);
  if (it == _defs.end()) {
    return nullptr;
  }

  // Parse position offset from use element.
  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);

  // Check if referenced element is an image.
  if (it->second->name == "image") {
    std::string imageHref = getHrefAttribute(it->second);
    if (imageHref.empty()) {
      return nullptr;
    }

    // Get image dimensions from the referenced image element.
    // Note: The transform on the use element is handled by convertToLayer,
    // which sets it on the Layer's matrix. So here we just use the original
    // image dimensions without applying the transform again.
    float imageWidth = parseLength(getAttribute(it->second, "width"), _viewBoxWidth);
    float imageHeight = parseLength(getAttribute(it->second, "height"), _viewBoxHeight);

    // Register the image resource.
    auto imageNode = registerImageResource(imageHref);

    // Create a rectangle to display the image at original size.
    // The transform will be applied by the parent Layer's matrix.
    auto rect = _document->makeNode<Rectangle>();
    rect->center.x = x + imageWidth / 2;
    rect->center.y = y + imageHeight / 2;
    rect->size.width = imageWidth;
    rect->size.height = imageHeight;

    // Create an ImagePattern fill for the rectangle.
    auto pattern = _document->makeNode<ImagePattern>();
    pattern->image = imageNode;
    // Position the pattern at the rectangle's origin.
    pattern->matrix = Matrix::Translate(x, y);

    // Create a fill with the image pattern.
    auto fill = _document->makeNode<Fill>();
    fill->color = pattern;

    // Create a group containing the rectangle and fill.
    auto group = _document->makeNode<Group>();
    group->elements.push_back(rect);
    group->elements.push_back(fill);

    return group;
  }

  if (_options.expandUseReferences) {
    _useStack.insert(refId);
    auto node = convertElement(it->second);
    _useStack.erase(refId);
    if (node) {
      if (x != 0 || y != 0) {
        // Wrap in a group with translation.
        auto group = _document->makeNode<Group>();
        group->position = {x, y};
        group->elements.push_back(node);
        return group;
      }
    }
    return node;
  }

  // For non-expanded use references, just create an empty group for now.
  auto group = _document->makeNode<Group>();
  // group->name (removed) = "_useRef:" + refId;
  return group;
}

LinearGradient* SVGParserContext::convertLinearGradient(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto gradient = _document->makeNode<LinearGradient>();

  gradient->id = getAttribute(element, "id");

  // Check gradientUnits - determines how gradient coordinates are interpreted.
  // Default is objectBoundingBox, meaning values are 0-1 ratios of the shape bounds.
  std::string gradientUnits = getAttribute(element, "gradientUnits", "objectBoundingBox");
  bool useObjectBoundingBox = (gradientUnits == "objectBoundingBox");

  // Parse gradient coordinates.
  float x1 = parseLength(getAttribute(element, "x1", "0%"), 1.0f);
  float y1 = parseLength(getAttribute(element, "y1", "0%"), 1.0f);
  float x2 = parseLength(getAttribute(element, "x2", "100%"), 1.0f);
  float y2 = parseLength(getAttribute(element, "y2", "0%"), 1.0f);

  // Parse gradientTransform.
  std::string gradientTransform = getAttribute(element, "gradientTransform");
  Matrix transformMatrix = gradientTransform.empty() ? Matrix::Identity()
                                                     : parseTransform(gradientTransform);

  Point start = useObjectBoundingBox
                    ? Point{shapeBounds.x + x1 * shapeBounds.width,
                            shapeBounds.y + y1 * shapeBounds.height}
                    : Point{x1, y1};
  Point end = useObjectBoundingBox
                  ? Point{shapeBounds.x + x2 * shapeBounds.width,
                          shapeBounds.y + y2 * shapeBounds.height}
                  : Point{x2, y2};
  gradient->startPoint = transformMatrix.mapPoint(start);
  gradient->endPoint = transformMatrix.mapPoint(end);

  // Parse stops.
  parseGradientStops(element, gradient->colorStops);

  return gradient;
}

RadialGradient* SVGParserContext::convertRadialGradient(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto gradient = _document->makeNode<RadialGradient>();

  gradient->id = getAttribute(element, "id");

  // Check gradientUnits - determines how gradient coordinates are interpreted.
  std::string gradientUnits = getAttribute(element, "gradientUnits", "objectBoundingBox");
  bool useObjectBoundingBox = (gradientUnits == "objectBoundingBox");

  // Parse gradient coordinates.
  float cx = parseLength(getAttribute(element, "cx", "50%"), 1.0f);
  float cy = parseLength(getAttribute(element, "cy", "50%"), 1.0f);
  float r = parseLength(getAttribute(element, "r", "50%"), 1.0f);

  // Parse gradientTransform.
  std::string gradientTransform = getAttribute(element, "gradientTransform");
  Matrix transformMatrix = gradientTransform.empty() ? Matrix::Identity()
                                                     : parseTransform(gradientTransform);

  if (useObjectBoundingBox) {
    // For objectBoundingBox, convert normalized coordinates to actual coordinates.
    gradient->center = {shapeBounds.x + cx * shapeBounds.width,
                        shapeBounds.y + cy * shapeBounds.height};
    // Radius is scaled by the average of width and height.
    gradient->radius = r * (shapeBounds.width + shapeBounds.height) / 2.0f;
  } else {
    // For userSpaceOnUse, coordinates are in user space.
    gradient->center = {cx, cy};
    gradient->radius = r;
  }

  if (!transformMatrix.isIdentity()) {
    gradient->matrix = transformMatrix;
  }

  // Parse stops.
  parseGradientStops(element, gradient->colorStops);

  return gradient;
}

void SVGParserContext::parseGradientStops(const std::shared_ptr<DOMNode>& element,
                                          std::vector<ColorStop>& colorStops) {
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "stop") {
      ColorStop stop;
      stop.offset = parseLength(getAttribute(child, "offset", "0"), 1.0f);
      stop.color = parseColor(getAttribute(child, "stop-color", "#000000"));
      float opacity = parseLength(getAttribute(child, "stop-opacity", "1"), 1.0f);
      stop.color.alpha = opacity;
      colorStops.push_back(stop);
    }
    child = child->getNextSibling();
  }
}

ImagePattern* SVGParserContext::convertPattern(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto pattern = _document->makeNode<ImagePattern>();

  pattern->id = getAttribute(element, "id");

  // Parse pattern dimensions from SVG attributes.
  float patternWidth = parseLength(getAttribute(element, "width"), 1.0f);
  float patternHeight = parseLength(getAttribute(element, "height"), 1.0f);

  // Check patternUnits - determines how pattern x/y/width/height are interpreted.
  // Default is objectBoundingBox, meaning values are relative to the shape bounds.
  std::string patternUnitsStr = getAttribute(element, "patternUnits", "objectBoundingBox");
  bool patternUnitsIsObjectBoundingBox = (patternUnitsStr == "objectBoundingBox");

  // Determine tile mode based on whether pattern covers the full shape.
  // When patternUnits is objectBoundingBox and width/height are 1.0 (100%),
  // the pattern covers the entire shape and should not repeat.
  if (patternUnitsIsObjectBoundingBox && patternWidth >= 1.0f && patternHeight >= 1.0f) {
    pattern->tileModeX = TileMode::Clamp;
    pattern->tileModeY = TileMode::Clamp;
  } else {
    // Pattern tile is smaller than shape - use repeat mode.
    pattern->tileModeX = TileMode::Repeat;
    pattern->tileModeY = TileMode::Repeat;
  }

  // Check patternContentUnits - determines how pattern content coordinates are interpreted.
  // Default is userSpaceOnUse, meaning content uses absolute coordinates.
  std::string contentUnitsStr = getAttribute(element, "patternContentUnits", "userSpaceOnUse");
  bool contentUnitsIsObjectBoundingBox = (contentUnitsStr == "objectBoundingBox");

  // Look for image reference inside the pattern.
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "use") {
      std::string imageId = resolveUrl(getHrefAttribute(child));

      // Find the referenced image in defs.
      auto imgIt = _defs.find(imageId);
      if (imgIt != _defs.end() && imgIt->second->name == "image") {
        std::string imageHref = getHrefAttribute(imgIt->second);

        // Register the image resource and use the reference pointer.
        auto imageNode = registerImageResource(imageHref);
        pattern->image = imageNode;

        // Parse transform on the use element.
        std::string useTransform = getAttribute(child, "transform");
        Matrix useMatrix = useTransform.empty() ? Matrix::Identity() : parseTransform(useTransform);

        // Construct the complete transformation matrix for the ImagePattern.
        // In SVG with patternContentUnits="objectBoundingBox", the transformation chain is:
        //   image pixels → useMatrix → pattern space (0-1) → shapeBounds → screen space
        //
        // tgfx ImagePattern matrix maps from screen coordinates to image coordinates.
        // The tgfx shader internally inverts the matrix we provide, so we should provide
        // the forward transformation (image pixels to screen pixels).
        //
        // Forward transform = T(shapeBounds.origin) * S(shapeBounds.size) * useMatrix
        //
        // This means:
        // 1. useMatrix transforms image pixels to pattern space (0-1 coordinates)
        // 2. Scale by shapeBounds.size transforms to user space dimensions
        // 3. Translate by shapeBounds.origin positions the pattern at the shape location

        Matrix forwardMatrix = Matrix::Identity();
        if (contentUnitsIsObjectBoundingBox) {
          // Pattern content is in objectBoundingBox coordinates (0-1).
          // Build the complete forward transform: image pixels → screen pixels
          forwardMatrix = Matrix::Translate(shapeBounds.x, shapeBounds.y) *
                          Matrix::Scale(shapeBounds.width, shapeBounds.height) *
                          useMatrix;
        } else {
          // Pattern content is in userSpaceOnUse coordinates.
          // useMatrix transforms image directly to user space, then translate to shape bounds.
          forwardMatrix = Matrix::Translate(shapeBounds.x, shapeBounds.y) * useMatrix;
        }

        pattern->matrix = forwardMatrix;
      }
    } else if (child->name == "image") {
      // Direct image element inside pattern.
      std::string imageHref = getHrefAttribute(child);

      // Register the image resource and use the reference pointer.
      auto imageNode = registerImageResource(imageHref);
      pattern->image = imageNode;

      if (contentUnitsIsObjectBoundingBox) {
        // Image dimensions are 0-1 ratios, scale by shape bounds.
        pattern->matrix = Matrix::Translate(shapeBounds.x, shapeBounds.y) *
                          Matrix::Scale(shapeBounds.width, shapeBounds.height);
      } else {
        // Image dimensions are absolute, translate to shape bounds origin.
        pattern->matrix = Matrix::Translate(shapeBounds.x, shapeBounds.y);
      }
    }
    child = child->getNextSibling();
  }

  return pattern;
}

void SVGParserContext::addFillStroke(const std::shared_ptr<DOMNode>& element,
                                  std::vector<Element*>& contents,
                                  const InheritedStyle& inheritedStyle) {
  // Lazily compute shape bounds only when needed (for gradient/pattern references).
  bool boundsComputed = false;
  Rect shapeBounds = {};

  // Determine effective fill value (element attribute overrides inherited).
  std::string fill = getAttribute(element, "fill");
  if (fill.empty()) {
    fill = inheritedStyle.fill;
  }

  // Only add fill if we have an effective fill value that is not "none".
  // If fill is empty and no inherited value, SVG default is black fill.
  // But if inherited value is "none", we skip fill entirely.
  if (fill != "none") {
    if (fill.empty()) {
      // No fill specified anywhere - use SVG default black.
      auto fillNode = _document->makeNode<Fill>();
      auto solidColor = _document->makeNode<SolidColor>();
      solidColor->color = {0, 0, 0, 1, ColorSpace::SRGB};
      fillNode->color = solidColor;
      contents.push_back(fillNode);
    } else if (fill.compare(0, 4, "url(") == 0) {
      auto fillNode = _document->makeNode<Fill>();
      std::string refId = resolveUrl(fill);
      // Use getColorSourceForRef which handles reference counting.
      if (!boundsComputed) {
        shapeBounds = getShapeBounds(element);
        boundsComputed = true;
      }
      fillNode->color = getColorSourceForRef(refId, shapeBounds);
      // Apply fill-opacity even for url() fills.
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (fillOpacity.empty()) {
        fillOpacity = inheritedStyle.fillOpacity;
      }
      if (!fillOpacity.empty()) {
        fillNode->alpha = strtof(fillOpacity.c_str(), nullptr);
      }
      contents.push_back(fillNode);
    } else {
      auto fillNode = _document->makeNode<Fill>();

      // Determine effective fill-opacity.
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (fillOpacity.empty()) {
        fillOpacity = inheritedStyle.fillOpacity;
      }
      if (!fillOpacity.empty()) {
        fillNode->alpha = strtof(fillOpacity.c_str(), nullptr);
      }

      // Convert color to SolidColor for PAGX compatibility.
      // SolidColor is always inlined (no id).
      Color parsedColor = parseColor(fill);
      auto solidColor = _document->makeNode<SolidColor>();
      solidColor->color = parsedColor;
      fillNode->color = solidColor;

      // Determine effective fill-rule.
      std::string fillRule = getAttribute(element, "fill-rule");
      if (fillRule.empty()) {
        fillRule = inheritedStyle.fillRule;
      }
      if (fillRule == "evenodd") {
        fillNode->fillRule = FillRule::EvenOdd;
      }

      contents.push_back(fillNode);
    }
  }

  // Determine effective stroke value (element attribute overrides inherited).
  std::string stroke = getAttribute(element, "stroke");
  if (stroke.empty()) {
    stroke = inheritedStyle.stroke;
  }

  if (!stroke.empty() && stroke != "none") {
    // Check stroke width first. A zero-width stroke is invisible and should be skipped.
    std::string strokeWidth = getAttribute(element, "stroke-width");
    if (strokeWidth.empty()) {
      strokeWidth = inheritedStyle.strokeWidth;
    }
    float parsedStrokeWidth = 1;
    if (!strokeWidth.empty()) {
      parsedStrokeWidth = parseLength(strokeWidth, _viewBoxWidth);
    }
    if (parsedStrokeWidth <= 0) {
      // Skip creating an invisible zero-width stroke.
      return;
    }

    auto strokeNode = _document->makeNode<Stroke>();

    if (stroke.compare(0, 4, "url(") == 0) {
      std::string refId = resolveUrl(stroke);
      // Use getColorSourceForRef which handles reference counting.
      if (!boundsComputed) {
        shapeBounds = getShapeBounds(element);
        boundsComputed = true;
      }
      strokeNode->color = getColorSourceForRef(refId, shapeBounds);
    } else {
      // Determine effective stroke-opacity.
      std::string strokeOpacity = getAttribute(element, "stroke-opacity");
      if (strokeOpacity.empty()) {
        strokeOpacity = inheritedStyle.strokeOpacity;
      }
      if (!strokeOpacity.empty()) {
        strokeNode->alpha = strtof(strokeOpacity.c_str(), nullptr);
      }

      // Convert color to SolidColor for PAGX compatibility.
      // SolidColor is always inlined (no id).
      Color parsedColor = parseColor(stroke);
      auto solidColor = _document->makeNode<SolidColor>();
      solidColor->color = parsedColor;
      strokeNode->color = solidColor;
    }

    strokeNode->width = parsedStrokeWidth;

    std::string strokeLinecap = getAttribute(element, "stroke-linecap");
    if (strokeLinecap.empty()) {
      strokeLinecap = inheritedStyle.strokeLinecap;
    }
    if (!strokeLinecap.empty()) {
      strokeNode->cap = LineCapFromString(strokeLinecap);
    }

    std::string strokeLinejoin = getAttribute(element, "stroke-linejoin");
    if (strokeLinejoin.empty()) {
      strokeLinejoin = inheritedStyle.strokeLinejoin;
    }
    if (!strokeLinejoin.empty()) {
      strokeNode->join = LineJoinFromString(strokeLinejoin);
    }

    std::string strokeMiterlimit = getAttribute(element, "stroke-miterlimit");
    if (strokeMiterlimit.empty()) {
      strokeMiterlimit = inheritedStyle.strokeMiterlimit;
    }
    if (!strokeMiterlimit.empty()) {
      strokeNode->miterLimit = strtof(strokeMiterlimit.c_str(), nullptr);
    }

    std::string dashArray = getAttribute(element, "stroke-dasharray");
    if (dashArray.empty()) {
      dashArray = inheritedStyle.strokeDasharray;
    }
    if (!dashArray.empty() && dashArray != "none") {
      // Parse dash array values, which may contain units (e.g., "2px,2px" or "2,2").
      // Use parseLength to handle both numeric values and values with units.
      std::string token;
      for (size_t i = 0; i <= dashArray.size(); i++) {
        char c = (i < dashArray.size()) ? dashArray[i] : ',';
        if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
          if (!token.empty()) {
            strokeNode->dashes.push_back(parseLength(token, _viewBoxWidth));
            token.clear();
          }
        } else {
          token += c;
        }
      }
    }

    std::string dashOffset = getAttribute(element, "stroke-dashoffset");
    if (dashOffset.empty()) {
      dashOffset = inheritedStyle.strokeDashoffset;
    }
    if (!dashOffset.empty()) {
      strokeNode->dashOffset = parseLength(dashOffset, _viewBoxWidth);
    }

    contents.push_back(strokeNode);
  }
}

Rect SVGParserContext::getShapeBounds(const std::shared_ptr<DOMNode>& element) {
  const auto& tag = element->name;

  if (tag == "rect") {
    float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
    float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
    float width = parseLength(getAttribute(element, "width"), _viewBoxWidth);
    float height = parseLength(getAttribute(element, "height"), _viewBoxHeight);
    return Rect::MakeXYWH(x, y, width, height);
  }

  if (tag == "circle") {
    float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
    float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
    float r = parseLength(getAttribute(element, "r"), _viewBoxWidth);
    return Rect::MakeXYWH(cx - r, cy - r, r * 2, r * 2);
  }

  if (tag == "ellipse") {
    float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
    float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
    float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
    float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);
    return Rect::MakeXYWH(cx - rx, cy - ry, rx * 2, ry * 2);
  }

  if (tag == "line") {
    float x1 = parseLength(getAttribute(element, "x1"), _viewBoxWidth);
    float y1 = parseLength(getAttribute(element, "y1"), _viewBoxHeight);
    float x2 = parseLength(getAttribute(element, "x2"), _viewBoxWidth);
    float y2 = parseLength(getAttribute(element, "y2"), _viewBoxHeight);
    float minX = std::min(x1, x2);
    float minY = std::min(y1, y2);
    float maxX = std::max(x1, x2);
    float maxY = std::max(y1, y2);
    return Rect::MakeXYWH(minX, minY, maxX - minX, maxY - minY);
  }

  if (tag == "path") {
    std::string d = getAttribute(element, "d");
    if (!d.empty()) {
      auto pathData = PathDataFromSVGString(d);
      return pathData.getBounds();
    }
  }

  // For polyline and polygon, parse points and compute bounds.
  if (tag == "polyline" || tag == "polygon") {
    std::string pointsStr = getAttribute(element, "points");
    if (!pointsStr.empty()) {
      auto pathData = parsePoints(pointsStr, tag == "polygon");
      return pathData.getBounds();
    }
  }

  // For use element, get bounds of the referenced element and apply x/y offset.
  if (tag == "use") {
    std::string refId = resolveUrl(getHrefAttribute(element));
    if (refId.empty() || _useStack.count(refId) > 0) {
      return Rect::MakeXYWH(0, 0, 0, 0);
    }
    auto it = _defs.find(refId);
    if (it != _defs.end()) {
      _useStack.insert(refId);
      Rect refBounds = getShapeBounds(it->second);
      _useStack.erase(refId);
      float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
      float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
      return Rect::MakeXYWH(refBounds.x + x, refBounds.y + y, refBounds.width, refBounds.height);
    }
  }

  return Rect::MakeXYWH(0, 0, 0, 0);
}

static void SkipWhitespace(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
    ++ptr;
  }
}

static float ReadNumber(const char*& ptr, const char* end) {
  SkipWhitespace(ptr, end);
  const char* start = ptr;
  if (ptr < end && (*ptr == '-' || *ptr == '+')) {
    ++ptr;
  }
  while (ptr < end && (std::isdigit(*ptr) || *ptr == '.')) {
    ++ptr;
  }
  if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
    ++ptr;
    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
      ++ptr;
    }
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
    }
  }
  if (start == ptr) {
    return 0.0f;
  }
  return strtof(start, nullptr);
}

Matrix SVGParserContext::parseTransform(const std::string& value) {
  Matrix result = Matrix::Identity();
  if (value.empty()) {
    return result;
  }

  const char* ptr = value.c_str();
  const char* end = ptr + value.length();

  while (ptr < end) {
    SkipWhitespace(ptr, end);
    if (ptr >= end) {
      break;
    }

    const char* funcStart = ptr;
    while (ptr < end && std::isalpha(*ptr)) {
      ++ptr;
    }
    size_t funcLen = static_cast<size_t>(ptr - funcStart);

    SkipWhitespace(ptr, end);
    if (ptr >= end || *ptr != '(') {
      break;
    }
    ++ptr;

    Matrix m = Matrix::Identity();

    if (funcLen == 9 && memcmp(funcStart, "translate", 9) == 0) {
      float tx = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      float ty = 0;
      if (ptr < end && *ptr != ')') {
        ty = ReadNumber(ptr, end);
      }
      m = Matrix::Translate(tx, ty);
    } else if (funcLen == 5 && memcmp(funcStart, "scale", 5) == 0) {
      float sx = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      float sy = sx;
      if (ptr < end && *ptr != ')') {
        sy = ReadNumber(ptr, end);
      }
      m = Matrix::Scale(sx, sy);
    } else if (funcLen == 6 && memcmp(funcStart, "rotate", 6) == 0) {
      float angle = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      if (ptr < end && *ptr != ')') {
        float cx = ReadNumber(ptr, end);
        float cy = ReadNumber(ptr, end);
        m = Matrix::Translate(cx, cy) * Matrix::Rotate(angle) * Matrix::Translate(-cx, -cy);
      } else {
        m = Matrix::Rotate(angle);
      }
    } else if (funcLen == 5 && memcmp(funcStart, "skewX", 5) == 0) {
      float angle = ReadNumber(ptr, end);
      m.c = std::tan(DegreesToRadians(angle));
    } else if (funcLen == 5 && memcmp(funcStart, "skewY", 5) == 0) {
      float angle = ReadNumber(ptr, end);
      m.b = std::tan(DegreesToRadians(angle));
    } else if (funcLen == 6 && memcmp(funcStart, "matrix", 6) == 0) {
      m.a = ReadNumber(ptr, end);
      m.b = ReadNumber(ptr, end);
      m.c = ReadNumber(ptr, end);
      m.d = ReadNumber(ptr, end);
      m.tx = ReadNumber(ptr, end);
      m.ty = ReadNumber(ptr, end);
    }

    SkipWhitespace(ptr, end);
    if (ptr < end && *ptr == ')') {
      ++ptr;
    }

    result = result * m;
  }

  return result;
}

// Parse CSS Color Level 4 color() function: color(display-p3 r g b) or color(display-p3 r g b / a).
// Returns a Color with alpha=-1 on parse failure.
static Color ParseCSSColorFunction(const std::string& value) {
  auto start = value.find('(');
  auto end = value.find(')');
  if (start == std::string::npos || end == std::string::npos) {
    return {0, 0, 0, -1};
  }
  auto inner = value.substr(start + 1, end - start - 1);
  // Trim leading whitespace.
  inner.erase(0, inner.find_first_not_of(" \t"));

  // Detect color space identifier.
  ColorSpace colorSpace = ColorSpace::SRGB;
  if (inner.compare(0, 10, "display-p3") == 0) {
    colorSpace = ColorSpace::DisplayP3;
    inner = inner.substr(10);
  } else if (inner.compare(0, 7, "a98-rgb") == 0) {
    inner = inner.substr(7);
  } else if (inner.compare(0, 7, "rec2020") == 0) {
    inner = inner.substr(7);
  } else if (inner.compare(0, 4, "srgb") == 0) {
    inner = inner.substr(4);
  }

  // Trim whitespace after color space name.
  inner.erase(0, inner.find_first_not_of(" \t"));
  auto lastNonSpace = inner.find_last_not_of(" \t");
  if (lastNonSpace != std::string::npos) {
    inner.erase(lastNonSpace + 1);
  } else {
    inner.clear();
  }

  // Parse space-separated values and optional "/ alpha".
  std::vector<float> components;
  float alpha = 1.0f;
  const char* ptr = inner.c_str();
  const char* endPtr = ptr + inner.size();
  bool foundSlash = false;
  while (ptr < endPtr) {
    while (ptr < endPtr && std::isspace(*ptr)) {
      ++ptr;
    }
    if (ptr >= endPtr) {
      break;
    }
    if (*ptr == '/') {
      foundSlash = true;
      ++ptr;
      continue;
    }
    char* numEnd = nullptr;
    float val = strtof(ptr, &numEnd);
    if (numEnd == ptr) {
      break;
    }
    if (foundSlash) {
      alpha = val;
    } else {
      components.push_back(val);
    }
    ptr = numEnd;
  }
  if (components.size() >= 3) {
    return {components[0], components[1], components[2], alpha, colorSpace};
  }
  return {0, 0, 0, -1};
}

Color SVGParserContext::parseColor(const std::string& value) {
  if (value.empty() || value == "none") {
    return {0, 0, 0, 0, ColorSpace::SRGB};
  }

  if (value[0] == '#') {
    uint32_t hex = 0;
    Color color = {};
    color.colorSpace = ColorSpace::SRGB;
    if (value.length() == 4) {
      // #RGB -> #RRGGBB
      char r = value[1];
      char g = value[2];
      char b = value[3];
      char expanded[7] = {r, r, g, g, b, b, '\0'};
      hex = strtoul(expanded, nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    } else if (value.length() == 7) {
      hex = strtoul(value.c_str() + 1, nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    } else if (value.length() == 9) {
      hex = strtoul(value.c_str() + 1, nullptr, 16);
      color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
      return color;
    }
  }

  if (value.compare(0, 3, "rgb") == 0) {
    size_t start = value.find('(');
    size_t end = value.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      std::string inner = value.substr(start + 1, end - start - 1);
      auto components = ParseFloatList(inner);
      float r = 0, g = 0, b = 0, a = 1.0f;
      if (components.size() >= 3) {
        r = components[0];
        g = components[1];
        b = components[2];
        if (components.size() >= 4) {
          a = components[3];
        }
      }
      return {r / 255.0f, g / 255.0f, b / 255.0f, a, ColorSpace::SRGB};
    }
  }

  // CSS Color Level 4: color(display-p3 r g b) or color(display-p3 r g b / a)
  if (value.compare(0, 6, "color(") == 0) {
    auto color = ParseCSSColorFunction(value);
    if (color.alpha >= 0) {
      return color;
    }
  }

  // SVG/CSS named colors (CSS Color 3 + CSS Color 4 rebeccapurple).
  // clang-format off
  static const std::unordered_map<std::string, uint32_t> namedColors = {
      {"aliceblue", 0xF0F8FF},
      {"antiquewhite", 0xFAEBD7},
      {"aqua", 0x00FFFF},
      {"aquamarine", 0x7FFFD4},
      {"azure", 0xF0FFFF},
      {"beige", 0xF5F5DC},
      {"bisque", 0xFFE4C4},
      {"black", 0x000000},
      {"blanchedalmond", 0xFFEBCD},
      {"blue", 0x0000FF},
      {"blueviolet", 0x8A2BE2},
      {"brown", 0xA52A2A},
      {"burlywood", 0xDEB887},
      {"cadetblue", 0x5F9EA0},
      {"chartreuse", 0x7FFF00},
      {"chocolate", 0xD2691E},
      {"coral", 0xFF7F50},
      {"cornflowerblue", 0x6495ED},
      {"cornsilk", 0xFFF8DC},
      {"crimson", 0xDC143C},
      {"cyan", 0x00FFFF},
      {"darkblue", 0x00008B},
      {"darkcyan", 0x008B8B},
      {"darkgoldenrod", 0xB8860B},
      {"darkgray", 0xA9A9A9},
      {"darkgreen", 0x006400},
      {"darkgrey", 0xA9A9A9},
      {"darkkhaki", 0xBDB76B},
      {"darkmagenta", 0x8B008B},
      {"darkolivegreen", 0x556B2F},
      {"darkorange", 0xFF8C00},
      {"darkorchid", 0x9932CC},
      {"darkred", 0x8B0000},
      {"darksalmon", 0xE9967A},
      {"darkseagreen", 0x8FBC8F},
      {"darkslateblue", 0x483D8B},
      {"darkslategray", 0x2F4F4F},
      {"darkslategrey", 0x2F4F4F},
      {"darkturquoise", 0x00CED1},
      {"darkviolet", 0x9400D3},
      {"deeppink", 0xFF1493},
      {"deepskyblue", 0x00BFFF},
      {"dimgray", 0x696969},
      {"dimgrey", 0x696969},
      {"dodgerblue", 0x1E90FF},
      {"firebrick", 0xB22222},
      {"floralwhite", 0xFFFAF0},
      {"forestgreen", 0x228B22},
      {"fuchsia", 0xFF00FF},
      {"gainsboro", 0xDCDCDC},
      {"ghostwhite", 0xF8F8FF},
      {"gold", 0xFFD700},
      {"goldenrod", 0xDAA520},
      {"gray", 0x808080},
      {"green", 0x008000},
      {"greenyellow", 0xADFF2F},
      {"grey", 0x808080},
      {"honeydew", 0xF0FFF0},
      {"hotpink", 0xFF69B4},
      {"indianred", 0xCD5C5C},
      {"indigo", 0x4B0082},
      {"ivory", 0xFFFFF0},
      {"khaki", 0xF0E68C},
      {"lavender", 0xE6E6FA},
      {"lavenderblush", 0xFFF0F5},
      {"lawngreen", 0x7CFC00},
      {"lemonchiffon", 0xFFFACD},
      {"lightblue", 0xADD8E6},
      {"lightcoral", 0xF08080},
      {"lightcyan", 0xE0FFFF},
      {"lightgoldenrodyellow", 0xFAFAD2},
      {"lightgray", 0xD3D3D3},
      {"lightgreen", 0x90EE90},
      {"lightgrey", 0xD3D3D3},
      {"lightpink", 0xFFB6C1},
      {"lightsalmon", 0xFFA07A},
      {"lightseagreen", 0x20B2AA},
      {"lightskyblue", 0x87CEFA},
      {"lightslategray", 0x778899},
      {"lightslategrey", 0x778899},
      {"lightsteelblue", 0xB0C4DE},
      {"lightyellow", 0xFFFFE0},
      {"lime", 0x00FF00},
      {"limegreen", 0x32CD32},
      {"linen", 0xFAF0E6},
      {"magenta", 0xFF00FF},
      {"maroon", 0x800000},
      {"mediumaquamarine", 0x66CDAA},
      {"mediumblue", 0x0000CD},
      {"mediumorchid", 0xBA55D3},
      {"mediumpurple", 0x9370DB},
      {"mediumseagreen", 0x3CB371},
      {"mediumslateblue", 0x7B68EE},
      {"mediumspringgreen", 0x00FA9A},
      {"mediumturquoise", 0x48D1CC},
      {"mediumvioletred", 0xC71585},
      {"midnightblue", 0x191970},
      {"mintcream", 0xF5FFFA},
      {"mistyrose", 0xFFE4E1},
      {"moccasin", 0xFFE4B5},
      {"navajowhite", 0xFFDEAD},
      {"navy", 0x000080},
      {"oldlace", 0xFDF5E6},
      {"olive", 0x808000},
      {"olivedrab", 0x6B8E23},
      {"orange", 0xFFA500},
      {"orangered", 0xFF4500},
      {"orchid", 0xDA70D6},
      {"palegoldenrod", 0xEEE8AA},
      {"palegreen", 0x98FB98},
      {"paleturquoise", 0xAFEEEE},
      {"palevioletred", 0xDB7093},
      {"papayawhip", 0xFFEFD5},
      {"peachpuff", 0xFFDAB9},
      {"peru", 0xCD853F},
      {"pink", 0xFFC0CB},
      {"plum", 0xDDA0DD},
      {"powderblue", 0xB0E0E6},
      {"purple", 0x800080},
      {"red", 0xFF0000},
      {"rosybrown", 0xBC8F8F},
      {"royalblue", 0x4169E1},
      {"saddlebrown", 0x8B4513},
      {"salmon", 0xFA8072},
      {"sandybrown", 0xF4A460},
      {"seagreen", 0x2E8B57},
      {"seashell", 0xFFF5EE},
      {"sienna", 0xA0522D},
      {"silver", 0xC0C0C0},
      {"skyblue", 0x87CEEB},
      {"slateblue", 0x6A5ACD},
      {"slategray", 0x708090},
      {"slategrey", 0x708090},
      {"snow", 0xFFFAFA},
      {"springgreen", 0x00FF7F},
      {"steelblue", 0x4682B4},
      {"tan", 0xD2B48C},
      {"teal", 0x008080},
      {"thistle", 0xD8BFD8},
      {"tomato", 0xFF6347},
      {"transparent", 0x000000},
      {"turquoise", 0x40E0D0},
      {"violet", 0xEE82EE},
      {"wheat", 0xF5DEB3},
      {"white", 0xFFFFFF},
      {"whitesmoke", 0xF5F5F5},
      {"yellow", 0xFFFF00},
      {"yellowgreen", 0x9ACD32},
      // CSS Color 4 addition
      {"rebeccapurple", 0x663399},
  };
  // clang-format on

  auto it = namedColors.find(value);
  if (it != namedColors.end()) {
    uint32_t hex = it->second;
    Color color = {};
    color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
    color.alpha = (value == "transparent") ? 0.0f : 1.0f;
    color.colorSpace = ColorSpace::SRGB;
    return color;
  }

  return {0, 0, 0, 1, ColorSpace::SRGB};
}

std::string SVGParserContext::colorToHex(const std::string& value) {
  if (value.empty() || value == "none") {
    return value;
  }
  // Already a hex color, return as-is.
  if (value[0] == '#') {
    return value;
  }
  // Already a PAGX p3() color, return as-is.
  if (value.compare(0, 3, "p3(") == 0) {
    return value;
  }
  // Already a PAGX srgb() color, return as-is.
  if (value.compare(0, 5, "srgb(") == 0) {
    return value;
  }
  // url() references should be returned as-is.
  if (value.compare(0, 4, "url(") == 0) {
    return value;
  }
  // CSS Color Level 4: color(display-p3 r g b) -> p3(r, g, b)
  if (value.compare(0, 6, "color(") == 0) {
    auto color = ParseCSSColorFunction(value);
    if (color.alpha >= 0 && color.colorSpace == ColorSpace::DisplayP3) {
      char buf[64] = {};
      if (color.alpha < 1.0f) {
        snprintf(buf, sizeof(buf), "p3(%.4g, %.4g, %.4g, %.4g)", color.red, color.green, color.blue,
                 color.alpha);
      } else {
        snprintf(buf, sizeof(buf), "p3(%.4g, %.4g, %.4g)", color.red, color.green, color.blue);
      }
      return std::string(buf);
    }
  }
  // Parse the color (handles named colors, rgb, rgba, etc.) and convert to hex.
  Color color = parseColor(value);
  // Convert color to hex string using existing utility.
  std::string result = ColorToHexString(color, true);
  return result;
}

float SVGParserContext::parseLength(const std::string& value, float containerSize) {
  if (value.empty()) {
    return 0;
  }

  char* endPtr = nullptr;
  float num = strtof(value.c_str(), &endPtr);
  if (endPtr == value.c_str()) {
    return 0;
  }

  std::string unit(endPtr);
  if (unit == "%") {
    return num / 100.0f * containerSize;
  }
  if (unit == "px" || unit.empty()) {
    return num;
  }
  if (unit == "pt") {
    return num * 1.333333f;
  }
  if (unit == "em" || unit == "rem") {
    return num * DEFAULT_FONT_SIZE;
  }
  if (unit == "in") {
    return num * 96.0f;
  }
  if (unit == "cm") {
    return num * 37.795275591f;
  }
  if (unit == "mm") {
    return num * 3.7795275591f;
  }

  return num;
}

float SVGParserContext::parseLengthEm(const std::string& value, float containerSize,
                                      float fontSize) {
  if (value.empty()) {
    return 0;
  }
  char* endPtr = nullptr;
  float num = strtof(value.c_str(), &endPtr);
  if (endPtr == value.c_str()) {
    return 0;
  }
  std::string unit(endPtr);
  if (unit == "em" || unit == "rem") {
    return num * fontSize;
  }
  return parseLength(value, containerSize);
}

std::vector<float> SVGParserContext::parseViewBox(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  return ParseFloatList(value);
}

PathData SVGParserContext::parsePoints(const std::string& value, bool closed) {
  PathData path;
  if (value.empty()) {
    return path;
  }

  // Parse space/comma-separated float values.
  std::vector<float> points;
  points.reserve(value.size() / 4);
  const char* ptr = value.c_str();
  const char* end = ptr + value.size();
  while (ptr < end) {
    // Skip whitespace and commas.
    while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
      ++ptr;
    }
    if (ptr >= end) {
      break;
    }
    char* numEnd = nullptr;
    float num = strtof(ptr, &numEnd);
    if (numEnd == ptr) {
      break;
    }
    points.push_back(num);
    ptr = numEnd;
  }

  if (points.size() >= 2) {
    path.moveTo(points[0], points[1]);
    for (size_t i = 2; i + 1 < points.size(); i += 2) {
      path.lineTo(points[i], points[i + 1]);
    }
    if (closed) {
      path.close();
    }
  }

  return path;
}

std::string SVGParserContext::resolveUrl(const std::string& url) {
  if (url.empty()) {
    return "";
  }
  // Handle url(#id) format.
  if (url.compare(0, 4, "url(") == 0) {
    size_t start = url.find('#');
    size_t end = url.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      return url.substr(start + 1, end - start - 1);
    }
  }
  // Handle #id format.
  if (url[0] == '#') {
    return url.substr(1);
  }
  return url;
}

Image* SVGParserContext::registerImageResource(const std::string& imageSource) {
  if (imageSource.empty()) {
    return nullptr;
  }

  // Check if this image source has already been registered.
  auto it = _imageSourceToId.find(imageSource);
  if (it != _imageSourceToId.end()) {
    return it->second;
  }

  // Generate a unique image ID that doesn't conflict with existing SVG IDs.
  std::string imageId = generateUniqueId("image");

  // Create and add the image resource to the document.
  auto imageNode = _document->makeNode<Image>();
  imageNode->id = imageId;
  imageNode->filePath = imageSource;

  // Cache the mapping.
  _imageSourceToId[imageSource] = imageNode;

  return imageNode;
}

std::string SVGParserContext::registerPathDataResource(PathData* pathData) {
  std::string pathId = generateUniqueId("path");
  pathData->id = pathId;
  return pathId;
}

// Helper function to check if two VectorElement nodes are the same geometry.
static bool isSameGeometry(const Element* a, const Element* b) {
  if (!a || !b || a->nodeType() != b->nodeType()) {
    return false;
  }

  switch (a->nodeType()) {
    case NodeType::Rectangle: {
      auto rectA = static_cast<const Rectangle*>(a);
      auto rectB = static_cast<const Rectangle*>(b);
      return rectA->center.x == rectB->center.x && rectA->center.y == rectB->center.y &&
             rectA->size.width == rectB->size.width && rectA->size.height == rectB->size.height &&
             rectA->roundness == rectB->roundness;
    }
    case NodeType::Ellipse: {
      auto ellipseA = static_cast<const Ellipse*>(a);
      auto ellipseB = static_cast<const Ellipse*>(b);
      return ellipseA->center.x == ellipseB->center.x && ellipseA->center.y == ellipseB->center.y &&
             ellipseA->size.width == ellipseB->size.width &&
             ellipseA->size.height == ellipseB->size.height;
    }
    case NodeType::Path: {
      auto pathA = static_cast<const Path*>(a);
      auto pathB = static_cast<const Path*>(b);
      if (!pathA->data || !pathB->data) {
        return pathA->data == pathB->data;
      }
      return pathA->data->verbs() == pathB->data->verbs() &&
             pathA->data->points() == pathB->data->points();
    }
    default:
      return false;
  }
}

// Check if a layer is a simple shape layer (contains exactly one geometry and one Fill or Stroke).
static bool isSimpleShapeLayer(const Layer* layer, const Element*& outGeometry,
                               const Element*& outPainter) {
  if (!layer || layer->contents.size() != 2) {
    return false;
  }
  if (!layer->children.empty() || !layer->filters.empty() || !layer->styles.empty()) {
    return false;
  }
  if (layer->mask != nullptr) {
    return false;
  }
  if (!layer->matrix.isIdentity() || layer->alpha != 1.0f) {
    return false;
  }

  const auto* first = layer->contents[0];
  const auto* second = layer->contents[1];

  // Check if first is geometry and second is painter.
  bool firstIsGeometry = (first->nodeType() == NodeType::Rectangle ||
                          first->nodeType() == NodeType::Ellipse || first->nodeType() == NodeType::Path);
  bool secondIsPainter =
      (second->nodeType() == NodeType::Fill || second->nodeType() == NodeType::Stroke);

  if (firstIsGeometry && secondIsPainter) {
    outGeometry = first;
    outPainter = second;
    return true;
  }
  return false;
}

void SVGParserContext::mergeAdjacentLayers(std::vector<Layer*>& layers) {
  if (layers.size() < 2) {
    return;
  }

  std::vector<Layer*> merged;
  size_t i = 0;

  while (i < layers.size()) {
    const Element* geomA = nullptr;
    const Element* painterA = nullptr;

    if (isSimpleShapeLayer(layers[i], geomA, painterA)) {
      // Check if the next layer has the same geometry.
      if (i + 1 < layers.size()) {
        const Element* geomB = nullptr;
        const Element* painterB = nullptr;

        if (isSimpleShapeLayer(layers[i + 1], geomB, painterB) &&
            isSameGeometry(geomA, geomB)) {
          // Merge: one has Fill, the other has Stroke.
          bool aHasFill = (painterA->nodeType() == NodeType::Fill);
          bool bHasFill = (painterB->nodeType() == NodeType::Fill);

          if (aHasFill != bHasFill) {
            // Create merged layer.
            auto mergedLayer = _document->makeNode<Layer>();
            mergedLayer->id = layers[i]->id;

            // Move geometry from first layer.
            mergedLayer->contents.push_back(layers[i]->contents[0]);

            // Add Fill first, then Stroke (standard order).
            if (aHasFill) {
              mergedLayer->contents.push_back(layers[i]->contents[1]);
              mergedLayer->contents.push_back(layers[i + 1]->contents[1]);
            } else {
              mergedLayer->contents.push_back(layers[i + 1]->contents[1]);
              mergedLayer->contents.push_back(layers[i]->contents[1]);
            }

            merged.push_back(mergedLayer);
            i += 2;  // Skip both layers.
            continue;
          }
        }
      }
    }

    // No merge, keep the layer as is.
    merged.push_back(layers[i]);
    i++;
  }

  layers = std::move(merged);
}

void SVGParserContext::parseMaskChildren(const std::shared_ptr<DOMNode>& parent,
                                         Layer* maskLayer,
                                         const InheritedStyle& parentStyle,
                                         const Matrix& parentMatrix) {
  auto child = parent->getFirstChild();
  while (child) {
    if (child->name == "rect" || child->name == "circle" || child->name == "ellipse" ||
        child->name == "path" || child->name == "polygon" || child->name == "polyline" ||
        child->name == "use") {
      InheritedStyle inheritedStyle = computeInheritedStyle(child, parentStyle);
      std::string transformStr = getAttribute(child, "transform");
      Matrix combinedMatrix = parentMatrix;
      if (!transformStr.empty()) {
        combinedMatrix = combinedMatrix * parseTransform(transformStr);
      }
      bool hasTransform = !combinedMatrix.isIdentity();
      if (hasTransform) {
        auto subLayer = _document->makeNode<Layer>();
        subLayer->matrix = combinedMatrix;
        convertChildren(child, subLayer->contents, inheritedStyle);
        maskLayer->children.push_back(subLayer);
      } else {
        convertChildren(child, maskLayer->contents, inheritedStyle);
      }
    } else if (child->name == "g") {
      InheritedStyle inheritedStyle = computeInheritedStyle(child, parentStyle);
      std::string groupTransform = getAttribute(child, "transform");
      Matrix combinedMatrix = parentMatrix;
      if (!groupTransform.empty()) {
        combinedMatrix = combinedMatrix * parseTransform(groupTransform);
      }
      parseMaskChildren(child, maskLayer, inheritedStyle, combinedMatrix);
    }
    child = child->getNextSibling();
  }
}

Layer* SVGParserContext::convertMaskElement(
    const std::shared_ptr<DOMNode>& maskElement, const InheritedStyle& parentStyle) {
  auto maskLayer = _document->makeNode<Layer>();
  maskLayer->id = getAttribute(maskElement, "id");
  maskLayer->name = maskLayer->id;
  maskLayer->visible = false;

  // Compute inherited style from the mask element itself (it may have fill="white" etc.).
  InheritedStyle maskStyle = computeInheritedStyle(maskElement, parentStyle);

  // Parse mask contents recursively.
  parseMaskChildren(maskElement, maskLayer, maskStyle, Matrix::Identity());

  return maskLayer;
}

std::pair<float, float> SVGParserContext::parseFilterOffset(const std::shared_ptr<DOMNode>& node) {
  std::string dx = getAttribute(node, "dx", "0");
  std::string dy = getAttribute(node, "dy", "0");
  return {strtof(dx.c_str(), nullptr), strtof(dy.c_str(), nullptr)};
}

std::pair<float, float> SVGParserContext::parseFilterBlur(const std::shared_ptr<DOMNode>& node) {
  std::string stdDeviation = getAttribute(node, "stdDeviation", "0");
  auto values = ParseFloatList(stdDeviation);
  float blurX = values.empty() ? 0 : values[0];
  float blurY = values.size() > 1 ? values[1] : blurX;
  return {blurX, blurY};
}

Color SVGParserContext::parseFilterColorMatrix(const std::shared_ptr<DOMNode>& node) {
  Color color = {0, 0, 0, 1.0f};
  std::string colorMatrix = getAttribute(node, "values");
  if (!colorMatrix.empty()) {
    auto values = ParseFloatList(colorMatrix);
    if (values.size() >= 19) {
      color.red = values[4];
      color.green = values[9];
      color.blue = values[14];
      color.alpha = values[18];
    }
  }
  return color;
}

DropShadowFilter* SVGParserContext::createDropShadow(float offsetX, float offsetY, float blurX,
                                                     float blurY, const Color& color,
                                                     bool shadowOnly) {
  auto dropShadow = _document->makeNode<DropShadowFilter>();
  dropShadow->offsetX = offsetX;
  dropShadow->offsetY = offsetY;
  dropShadow->blurX = blurX;
  dropShadow->blurY = blurY;
  dropShadow->color = color;
  dropShadow->shadowOnly = shadowOnly;
  return dropShadow;
}

bool SVGParserContext::convertFilterElement(
    const std::shared_ptr<DOMNode>& filterElement,
    std::vector<LayerFilter*>& filters,
    std::vector<LayerStyle*>& styles,
    ShadowOnlyType* outShadowOnlyType) {
  size_t initialFilterCount = filters.size();
  size_t initialStyleCount = styles.size();

  // Collect all filter primitives for analysis.
  std::vector<std::shared_ptr<DOMNode>> primitives;
  bool hasMerge = false;
  auto child = filterElement->getFirstChild();
  while (child) {
    if (!child->name.empty() && child->name.compare(0, 2, "fe") == 0) {
      primitives.push_back(child);
      // Check if filter merges shadow with SourceGraphic.
      // If feMerge or feBlend references SourceGraphic, shadow is composited with original.
      if (child->name == "feMerge" || child->name == "feBlend") {
        hasMerge = true;
      }
    }
    child = child->getNextSibling();
  }
  // If filter only produces shadow without merging with original content,
  // we should set shadowOnly=true.
  bool shadowOnly = !hasMerge;

  // Detect Figma-style drop shadow pattern and extract shadow parameters.
  // Pattern: feColorMatrix(in=SourceAlpha) → feOffset → feGaussianBlur → feComposite → feColorMatrix
  // This pattern may repeat multiple times for multiple shadows.
  size_t i = 0;
  while (i < primitives.size()) {
    auto& node = primitives[i];

    // Skip feFlood elements - they are used for background setup but don't affect the shadow.
    if (node->name == "feFlood") {
      i++;
      continue;
    }

    // Check for drop shadow pattern starting with feColorMatrix in="SourceAlpha".
    if (node->name == "feColorMatrix" && getAttribute(node, "in") == "SourceAlpha") {
      // Look for the sequence: feColorMatrix → feOffset → feGaussianBlur → feComposite → feColorMatrix
      if (i + 4 < primitives.size() && primitives[i + 1]->name == "feOffset" &&
          primitives[i + 2]->name == "feGaussianBlur" &&
          primitives[i + 3]->name == "feComposite" && primitives[i + 4]->name == "feColorMatrix") {
        auto [offsetX, offsetY] = parseFilterOffset(primitives[i + 1]);
        auto [blurX, blurY] = parseFilterBlur(primitives[i + 2]);
        auto shadowColor = parseFilterColorMatrix(primitives[i + 4]);

        filters.push_back(createDropShadow(offsetX, offsetY, blurX, blurY, shadowColor, shadowOnly));

        // Skip the consumed primitives (5 elements) plus the feBlend that follows.
        i += 5;
        if (i < primitives.size() && primitives[i]->name == "feBlend") {
          i++;
        }
        continue;
      }

      // Check for Figma double drop shadow pattern (without feComposite):
      // feColorMatrix(in=SourceAlpha) → feOffset → feGaussianBlur → feColorMatrix → feBlend
      // This is common in Figma exports with multiple shadows.
      if (i + 3 < primitives.size() && primitives[i + 1]->name == "feOffset" &&
          primitives[i + 2]->name == "feGaussianBlur" && primitives[i + 3]->name == "feColorMatrix") {
        auto [offsetX, offsetY] = parseFilterOffset(primitives[i + 1]);
        auto [blurX, blurY] = parseFilterBlur(primitives[i + 2]);
        auto shadowColor = parseFilterColorMatrix(primitives[i + 3]);

        filters.push_back(createDropShadow(offsetX, offsetY, blurX, blurY, shadowColor, shadowOnly));

        // Skip the consumed primitives (4 elements) plus the feBlend that follows.
        i += 4;
        if (i < primitives.size() && primitives[i]->name == "feBlend") {
          i++;
        }
        continue;
      }

      // Check for Figma-style solid drop shadow pattern (no blur):
      // feColorMatrix(in=SourceAlpha) → feOffset → feColorMatrix → [feBlend]
      // This creates a solid shadow without blur effect.
      if (i + 2 < primitives.size() && primitives[i + 1]->name == "feOffset" &&
          primitives[i + 2]->name == "feColorMatrix") {
        auto [offsetX, offsetY] = parseFilterOffset(primitives[i + 1]);
        auto shadowColor = parseFilterColorMatrix(primitives[i + 2]);

        filters.push_back(createDropShadow(offsetX, offsetY, 0, 0, shadowColor, shadowOnly));

        // Skip the consumed primitives (3 elements) plus any feBlend that follows.
        i += 3;
        while (i < primitives.size() && primitives[i]->name == "feBlend") {
          i++;
        }
        continue;
      }
    }

    // Check for standard drop shadow pattern starting with feGaussianBlur in="SourceAlpha".
    // Pattern: feGaussianBlur(in=SourceAlpha) → feOffset → [feColorMatrix]
    // Note: Inner shadow pattern is similar but has feComposite(arithmetic) after feOffset.
    if (node->name == "feGaussianBlur" && getAttribute(node, "in") == "SourceAlpha") {
      // Look for feOffset following the blur.
      if (i + 1 < primitives.size() && primitives[i + 1]->name == "feOffset") {
        // Check if this is an inner shadow pattern (has feComposite with arithmetic operator).
        // Inner shadow pattern: feGaussianBlur → feOffset → feComposite(arithmetic) → feColorMatrix
        bool isInnerShadow = false;
        if (i + 2 < primitives.size() && primitives[i + 2]->name == "feComposite") {
          std::string compositeOp = getAttribute(primitives[i + 2], "operator");
          if (compositeOp == "arithmetic") {
            isInnerShadow = true;
          }
        }

        if (isInnerShadow) {
          // Convert inner shadow pattern to InnerShadowFilter.
          auto [blurX, blurY] = parseFilterBlur(node);
          auto [offsetX, offsetY] = parseFilterOffset(primitives[i + 1]);

          // Look for feColorMatrix after feComposite for shadow color.
          Color shadowColor = {0, 0, 0, 1.0f};
          size_t colorMatrixIdx = i + 3;
          if (colorMatrixIdx < primitives.size() &&
              primitives[colorMatrixIdx]->name == "feColorMatrix") {
            shadowColor = parseFilterColorMatrix(primitives[colorMatrixIdx]);
          }

          auto innerShadow = _document->makeNode<InnerShadowFilter>();
          innerShadow->offsetX = offsetX;
          innerShadow->offsetY = offsetY;
          innerShadow->blurX = blurX;
          innerShadow->blurY = blurY;
          innerShadow->color = shadowColor;
          innerShadow->shadowOnly = shadowOnly;
          filters.push_back(innerShadow);

          // Skip consumed primitives: feGaussianBlur, feOffset, feComposite, [feColorMatrix]
          i += 3;
          if (i < primitives.size() && primitives[i]->name == "feColorMatrix") {
            i++;
          }
          continue;
        }

        auto [blurX, blurY] = parseFilterBlur(node);
        auto [offsetX, offsetY] = parseFilterOffset(primitives[i + 1]);

        // Look for feColorMatrix after feOffset for shadow color.
        Color shadowColor = {0, 0, 0, 1.0f};
        size_t colorMatrixIdx = i + 2;
        if (colorMatrixIdx < primitives.size() && primitives[colorMatrixIdx]->name == "feColorMatrix") {
          shadowColor = parseFilterColorMatrix(primitives[colorMatrixIdx]);
        }

        filters.push_back(createDropShadow(offsetX, offsetY, blurX, blurY, shadowColor, shadowOnly));

        // Skip consumed primitives.
        i += 2;
        if (i < primitives.size() && primitives[i]->name == "feColorMatrix") {
          i++;
        }
        continue;
      }
    }

    // Check for another standard drop shadow pattern: feOffset(in=SourceAlpha) → feGaussianBlur → feColorMatrix
    // Used by Sketch and other tools.
    if (node->name == "feOffset" && getAttribute(node, "in") == "SourceAlpha") {
      // Look for feGaussianBlur following.
      if (i + 1 < primitives.size() && primitives[i + 1]->name == "feGaussianBlur") {
        auto [offsetX, offsetY] = parseFilterOffset(node);
        auto [blurX, blurY] = parseFilterBlur(primitives[i + 1]);

        // Look for feColorMatrix after feGaussianBlur for shadow color.
        Color shadowColor = {0, 0, 0, 1.0f};
        size_t colorMatrixIdx = i + 2;
        if (colorMatrixIdx < primitives.size() && primitives[colorMatrixIdx]->name == "feColorMatrix") {
          shadowColor = parseFilterColorMatrix(primitives[colorMatrixIdx]);
        }

        filters.push_back(createDropShadow(offsetX, offsetY, blurX, blurY, shadowColor, shadowOnly));

        // Skip consumed primitives.
        i += 2;
        if (i < primitives.size() && primitives[i]->name == "feColorMatrix") {
          i++;
        }
        continue;
      }
    }

    // Check for blur filter that should apply to the source graphic.
    // This includes:
    // 1. feGaussianBlur with in="SourceGraphic"
    // 2. feGaussianBlur as the first primitive with no "in" attribute
    // 3. feGaussianBlur following a feBlend that merges SourceGraphic (Figma foreground blur pattern)
    if (node->name == "feGaussianBlur") {
      std::string inAttr = getAttribute(node, "in");
      bool isSourceGraphic = (inAttr == "SourceGraphic") || (inAttr.empty() && i == 0);

      // Check for Figma foreground blur pattern: feFlood → feBlend(in=SourceGraphic) → feGaussianBlur.
      if (!isSourceGraphic && i >= 2) {
        auto& prevNode = primitives[i - 1];
        if (prevNode->name == "feBlend" && getAttribute(prevNode, "in") == "SourceGraphic") {
          // This blur follows a blend with SourceGraphic, treat it as foreground blur.
          isSourceGraphic = true;
        }
      }

      if (isSourceGraphic) {
        auto [devX, devY] = parseFilterBlur(node);
        auto blurFilter = _document->makeNode<BlurFilter>();
        blurFilter->blurX = devX;
        blurFilter->blurY = devY;
        filters.push_back(blurFilter);
      }
    }

    i++;
  }

  // Determine the shadow-only type based on newly added filters.
  if (outShadowOnlyType != nullptr) {
    if (shadowOnly && filters.size() > initialFilterCount) {
      // Check the type of the first new filter to determine shadow-only type.
      // All shadow-only filters in one element should be of the same type.
      for (size_t idx = initialFilterCount; idx < filters.size(); ++idx) {
        auto* filter = filters[idx];
        if (filter->nodeType() == NodeType::InnerShadowFilter) {
          *outShadowOnlyType = ShadowOnlyType::InnerShadow;
          break;
        } else if (filter->nodeType() == NodeType::DropShadowFilter) {
          *outShadowOnlyType = ShadowOnlyType::DropShadow;
          break;
        }
      }
    }
  }

  // Return true if any filter or style was added.
  return filters.size() > initialFilterCount || styles.size() > initialStyleCount;
}

void SVGParserContext::collectAllIds(const std::shared_ptr<DOMNode>& node) {
  if (!node) {
    return;
  }

  // Collect id from this node.
  auto* idPtr = node->findAttribute("id");
  if (idPtr && !idPtr->empty()) {
    const std::string& id = *idPtr;
    _existingIds.insert(id);
    // Also collect referenceable elements (mask, clipPath, filter, etc.) to _defs,
    // even if they are defined inline (not inside <defs>).
    const auto& name = node->name;
    if (name == "mask" || name == "clipPath" || name == "filter" || name == "linearGradient" ||
        name == "radialGradient" || name == "pattern") {
      _defs[id] = node;
    }
  }

  // Recursively collect from children.
  auto child = node->getFirstChild();
  while (child) {
    collectAllIds(child);
    child = child->getNextSibling();
  }
}

std::string SVGParserContext::generateUniqueId(const std::string& prefix) {
  std::string id;
  do {
    id = prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0 || (_document && _document->findNode(id) != nullptr));
  _existingIds.insert(id);
  return id;
}

void SVGParserContext::parseCustomData(const std::shared_ptr<DOMNode>& element, Layer* layer) {
  if (!element || !layer) {
    return;
  }

  // Iterate through all attributes and find data-* ones.
  for (const auto& attr : element->attributes) {
    if (attr.name.length() > 5 && attr.name.compare(0, 5, "data-") == 0) {
      // Remove "data-" prefix and store in customData.
      std::string key = attr.name.substr(5);
      layer->customData[key] = attr.value;
    }
  }
}

void SVGParserContext::countColorSourceReferences(const std::shared_ptr<DOMNode>& root) {
  // Traverse all elements and count references to gradients/patterns.
  auto child = root->getFirstChild();
  while (child) {
    if (child->name != "defs") {
      countColorSourceReferencesInElement(child);
    }
    child = child->getNextSibling();
  }
}

void SVGParserContext::countUrlReference(const std::string& attrValue) {
  if (!attrValue.empty() && attrValue.compare(0, 4, "url(") == 0) {
    std::string refId = resolveUrl(attrValue);
    auto it = _defs.find(refId);
    if (it != _defs.end()) {
      const auto& defName = it->second->name;
      if (defName == "linearGradient" || defName == "radialGradient" || defName == "pattern") {
        _colorSourceRefCount[refId]++;
      }
    }
  }
}

void SVGParserContext::countColorSourceReferencesInElement(const std::shared_ptr<DOMNode>& element) {
  if (!element) {
    return;
  }

  countUrlReference(getAttribute(element, "fill"));
  countUrlReference(getAttribute(element, "stroke"));

  // Recurse into children.
  auto child = element->getFirstChild();
  while (child) {
    countColorSourceReferencesInElement(child);
    child = child->getNextSibling();
  }
}

std::string SVGParserContext::generateColorSourceId() {
  return generateUniqueId("color");
}

ColorSource* SVGParserContext::getColorSourceForRef(const std::string& refId,
                                                                  const Rect& shapeBounds) {
  auto defIt = _defs.find(refId);
  if (defIt == _defs.end()) {
    return nullptr;
  }

  const auto& defNode = defIt->second;
  const auto& defName = defNode->name;

  // Check if this reference is used multiple times.
  int refCount = 0;
  auto countIt = _colorSourceRefCount.find(refId);
  if (countIt != _colorSourceRefCount.end()) {
    refCount = countIt->second;
  }

  // If refCount > 1, check if we already created a resource for this ref.
  if (refCount > 1) {
    auto cacheIt = _colorSourceCache.find(refId);
    if (cacheIt != _colorSourceCache.end()) {
      return cacheIt->second;
    }
  }

  // Convert the SVG def to a ColorSource.
  ColorSource* colorSource = nullptr;
  if (defName == "linearGradient") {
    colorSource = convertLinearGradient(defNode, shapeBounds);
  } else if (defName == "radialGradient") {
    colorSource = convertRadialGradient(defNode, shapeBounds);
  } else if (defName == "pattern") {
    colorSource = convertPattern(defNode, shapeBounds);
  }

  if (!colorSource) {
    return nullptr;
  }

  if (refCount > 1) {
    colorSource->id = generateColorSourceId();
    _colorSourceCache[refId] = colorSource;
  } else {
    colorSource->id.clear();
  }
  return colorSource;
}

}  // namespace pagx
