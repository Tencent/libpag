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
#include <fstream>
#include "PAGXStringUtils.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/SolidColor.h"
#include "SVGParserInternal.h"
#include "xml/XMLDOM.h"

namespace pagx {

std::shared_ptr<PAGXDocument> SVGImporter::Parse(const std::string& filePath,
                                             const Options& options) {
  SVGParserImpl parser(options);
  auto doc = parser.parseFile(filePath);
  if (doc) {
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      doc->basePath = filePath.substr(0, lastSlash + 1);
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> SVGImporter::Parse(const uint8_t* data, size_t length,
                                             const Options& options) {
  SVGParserImpl parser(options);
  return parser.parse(data, length);
}

std::shared_ptr<PAGXDocument> SVGImporter::ParseString(const std::string& svgContent,
                                                   const Options& options) {
  return Parse(reinterpret_cast<const uint8_t*>(svgContent.data()), svgContent.size(), options);
}

// ============== SVGParserImpl ==============

SVGParserImpl::SVGParserImpl(const SVGImporter::Options& options) : _options(options) {
}

std::shared_ptr<PAGXDocument> SVGParserImpl::parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }

  auto dom = DOM::Make(data, length);
  if (!dom) {
    return nullptr;
  }

  return parseDOM(dom);
}

std::shared_ptr<PAGXDocument> SVGParserImpl::parseFile(const std::string& filePath) {
  auto dom = DOM::MakeFromFile(filePath);
  if (!dom) {
    return nullptr;
  }

  return parseDOM(dom);
}

std::string SVGParserImpl::getAttribute(const std::shared_ptr<DOMNode>& node,
                                        const std::string& name,
                                        const std::string& defaultValue) const {
  // CSS priority: style attribute > presentation attribute > CSS class rules
  // Check style attribute first for CSS property.
  // Style attribute format: "property1: value1; property2: value2; ..."
  // CSS cascade rule: later properties override earlier ones.
  auto [hasStyle, styleStr] = node->findAttribute("style");
  if (hasStyle && !styleStr.empty()) {
    std::string propName = name;
    std::string lastValue;
    size_t pos = 0;
    while (pos < styleStr.size()) {
      // Skip whitespace.
      while (pos < styleStr.size() && std::isspace(styleStr[pos])) {
        ++pos;
      }
      // Find property name end (colon).
      size_t colonPos = styleStr.find(':', pos);
      if (colonPos == std::string::npos) {
        break;
      }
      // Extract property name and trim whitespace.
      std::string currentProp = styleStr.substr(pos, colonPos - pos);
      size_t propStart = currentProp.find_first_not_of(" \t");
      size_t propEnd = currentProp.find_last_not_of(" \t");
      if (propStart != std::string::npos && propEnd != std::string::npos) {
        currentProp = currentProp.substr(propStart, propEnd - propStart + 1);
      }
      // Find value end (semicolon or end of string).
      // Special case: CSS color() function may contain parentheses,
      // so find the next semicolon that's not inside parentheses.
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
      // Check if this is the property we're looking for.
      if (currentProp == propName) {
        // Extract and trim the value.
        std::string propValue = styleStr.substr(colonPos + 1, semicolonPos - colonPos - 1);
        size_t valStart = propValue.find_first_not_of(" \t");
        size_t valEnd = propValue.find_last_not_of(" \t");
        if (valStart != std::string::npos && valEnd != std::string::npos) {
          lastValue = propValue.substr(valStart, valEnd - valStart + 1);
        } else {
          lastValue = propValue;
        }
        // Continue searching for later occurrences (CSS cascade).
      }
      // Move to next property.
      pos = semicolonPos + 1;
    }
    if (!lastValue.empty()) {
      return lastValue;
    }
  }

  // Fallback: check for direct attribute (presentation attribute).
  auto [found, value] = node->findAttribute(name);
  if (found) {
    return value;
  }

  // Check CSS class rules (lowest priority).
  // class attribute can contain multiple class names separated by whitespace.
  auto [hasClass, classAttr] = node->findAttribute("class");
  if (hasClass && !classAttr.empty()) {
    // Parse class names.
    size_t pos = 0;
    while (pos < classAttr.size()) {
      // Skip whitespace.
      while (pos < classAttr.size() && std::isspace(classAttr[pos])) {
        pos++;
      }
      if (pos >= classAttr.size()) {
        break;
      }
      // Find end of class name.
      size_t endPos = pos;
      while (endPos < classAttr.size() && !std::isspace(classAttr[endPos])) {
        endPos++;
      }
      std::string className = classAttr.substr(pos, endPos - pos);
      pos = endPos;

      // Look up the class in CSS rules.
      auto it = _cssClassRules.find(className);
      if (it != _cssClassRules.end()) {
        // Parse the style string to find the property we need.
        const std::string& classStyle = it->second;
        size_t stylePos = 0;
        while (stylePos < classStyle.size()) {
          // Skip whitespace.
          while (stylePos < classStyle.size() && std::isspace(classStyle[stylePos])) {
            stylePos++;
          }
          // Find property name end (colon).
          size_t colonPos = classStyle.find(':', stylePos);
          if (colonPos == std::string::npos) {
            break;
          }
          // Extract property name and trim whitespace.
          std::string currentProp = classStyle.substr(stylePos, colonPos - stylePos);
          size_t propStart = currentProp.find_first_not_of(" \t");
          size_t propEnd = currentProp.find_last_not_of(" \t");
          if (propStart != std::string::npos && propEnd != std::string::npos) {
            currentProp = currentProp.substr(propStart, propEnd - propStart + 1);
          }
          // Find value end (semicolon or end of string).
          size_t semicolonPos = classStyle.find(';', colonPos);
          if (semicolonPos == std::string::npos) {
            semicolonPos = classStyle.size();
          }
          // Check if this is the property we're looking for.
          if (currentProp == name) {
            // Extract and trim the value.
            std::string propValue = classStyle.substr(colonPos + 1, semicolonPos - colonPos - 1);
            size_t valStart = propValue.find_first_not_of(" \t");
            size_t valEnd = propValue.find_last_not_of(" \t");
            if (valStart != std::string::npos && valEnd != std::string::npos) {
              return propValue.substr(valStart, valEnd - valStart + 1);
            }
            return propValue;
          }
          // Move to next property.
          stylePos = semicolonPos + 1;
        }
      }
    }
  }

  return defaultValue;
}

std::shared_ptr<PAGXDocument> SVGParserImpl::parseDOM(const std::shared_ptr<DOM>& dom) {
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
  std::vector<std::unique_ptr<Layer>> convertedLayers;
  child = root->getFirstChild();
  while (child) {
    if (child->name != "defs") {
      auto layer = convertToLayer(child, rootStyle);
      if (layer) {
        convertedLayers.push_back(std::move(layer));
      }
    }
    child = child->getNextSibling();
  }

  // Add collected mask layers (invisible, used as mask references).
  for (auto& maskLayer : _maskLayers) {
    convertedLayers.insert(convertedLayers.begin(), std::move(maskLayer));
  }
  _maskLayers.clear();

  // Merge adjacent layers with the same geometry (optimize Fill + Stroke into one Layer).
  mergeAdjacentLayers(convertedLayers);

  // If viewBox transform is needed, wrap in a root layer with the transform.
  // Otherwise, add layers directly to document (no root wrapper).
  if (needsViewBoxTransform) {
    auto rootLayer = std::make_unique<Layer>();
    rootLayer->matrix = viewBoxMatrix;
    for (auto& layer : convertedLayers) {
      rootLayer->children.push_back(std::move(layer));
    }
    _document->layers.push_back(std::move(rootLayer));
  } else {
    for (auto& layer : convertedLayers) {
      _document->layers.push_back(std::move(layer));
    }
  }

  return _document;
}

InheritedStyle SVGParserImpl::computeInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                                    const InheritedStyle& parentStyle) {
  InheritedStyle style = parentStyle;

  std::string fill = getAttribute(element, "fill");
  if (!fill.empty()) {
    style.fill = fill;
  }

  std::string stroke = getAttribute(element, "stroke");
  if (!stroke.empty()) {
    style.stroke = stroke;
  }

  std::string fillOpacity = getAttribute(element, "fill-opacity");
  if (!fillOpacity.empty()) {
    style.fillOpacity = fillOpacity;
  }

  std::string strokeOpacity = getAttribute(element, "stroke-opacity");
  if (!strokeOpacity.empty()) {
    style.strokeOpacity = strokeOpacity;
  }

  std::string fillRule = getAttribute(element, "fill-rule");
  if (!fillRule.empty()) {
    style.fillRule = fillRule;
  }

  std::string strokeDasharray = getAttribute(element, "stroke-dasharray");
  if (!strokeDasharray.empty()) {
    style.strokeDasharray = strokeDasharray;
  }

  std::string strokeDashoffset = getAttribute(element, "stroke-dashoffset");
  if (!strokeDashoffset.empty()) {
    style.strokeDashoffset = strokeDashoffset;
  }

  std::string strokeWidth = getAttribute(element, "stroke-width");
  if (!strokeWidth.empty()) {
    style.strokeWidth = strokeWidth;
  }

  std::string strokeLinecap = getAttribute(element, "stroke-linecap");
  if (!strokeLinecap.empty()) {
    style.strokeLinecap = strokeLinecap;
  }

  std::string strokeLinejoin = getAttribute(element, "stroke-linejoin");
  if (!strokeLinejoin.empty()) {
    style.strokeLinejoin = strokeLinejoin;
  }

  std::string strokeMiterlimit = getAttribute(element, "stroke-miterlimit");
  if (!strokeMiterlimit.empty()) {
    style.strokeMiterlimit = strokeMiterlimit;
  }

  // Text properties.
  std::string fontFamily = getAttribute(element, "font-family");
  if (!fontFamily.empty()) {
    style.fontFamily = fontFamily;
  }

  std::string fontSize = getAttribute(element, "font-size");
  if (!fontSize.empty()) {
    style.fontSize = fontSize;
  }

  std::string fontWeight = getAttribute(element, "font-weight");
  if (!fontWeight.empty()) {
    style.fontWeight = fontWeight;
  }

  std::string fontStyle = getAttribute(element, "font-style");
  if (!fontStyle.empty()) {
    style.fontStyle = fontStyle;
  }

  std::string letterSpacing = getAttribute(element, "letter-spacing");
  if (!letterSpacing.empty()) {
    style.letterSpacing = letterSpacing;
  }

  std::string textAnchor = getAttribute(element, "text-anchor");
  if (!textAnchor.empty()) {
    style.textAnchor = textAnchor;
  }

  return style;
}

void SVGParserImpl::parseDefs(const std::shared_ptr<DOMNode>& defsNode) {
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

void SVGParserImpl::parseStyleElement(const std::shared_ptr<DOMNode>& styleNode) {
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

std::unique_ptr<Layer> SVGParserImpl::convertToLayer(const std::shared_ptr<DOMNode>& element,
                                                     const InheritedStyle& parentStyle) {
  const auto& tag = element->name;

  if (tag == "defs" || tag == "linearGradient" || tag == "radialGradient" || tag == "pattern" ||
      tag == "mask" || tag == "clipPath" || tag == "filter") {
    return nullptr;
  }

  // Compute inherited style for this element.
  InheritedStyle inheritedStyle = computeInheritedStyle(element, parentStyle);

  auto layer = std::make_unique<Layer>();

  // Parse common layer attributes.
  layer->id = getAttribute(element, "id");

  // Parse data-* custom attributes.
  parseCustomData(element, layer.get());

  std::string transform = getAttribute(element, "transform");
  if (!transform.empty()) {
    layer->matrix = parseTransform(transform);
  }

  std::string opacity = getAttribute(element, "opacity");
  if (!opacity.empty()) {
    layer->alpha = std::stof(opacity);
  }

  std::string display = getAttribute(element, "display");
  if (display == "none") {
    layer->visible = false;
  }

  std::string visibility = getAttribute(element, "visibility");
  if (visibility == "hidden") {
    layer->visible = false;
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
        layer->mask = "#" + maskLayer->id;
        // SVG masks use luminance by default.
        layer->maskType = MaskType::Luminance;
        // Add mask layer as invisible layer to the document.
        _maskLayers.push_back(std::move(maskLayer));
      }
    }
  }

  // Handle clip-path attribute (similar to mask but uses contour/alpha).
  std::string clipPathAttr = getAttribute(element, "clip-path");
  if (!clipPathAttr.empty() && clipPathAttr != "none") {
    std::string clipId = resolveUrl(clipPathAttr);
    auto clipIt = _defs.find(clipId);
    if (clipIt != _defs.end()) {
      // Convert clipPath element to a mask layer.
      auto clipLayer = convertMaskElement(clipIt->second, inheritedStyle);
      if (clipLayer) {
        layer->mask = "#" + clipLayer->id;
        // SVG clip-path uses alpha (shape outline) for clipping.
        layer->maskType = MaskType::Alpha;
        // Add clip layer as invisible layer to the document.
        _maskLayers.push_back(std::move(clipLayer));
      }
    }
  }

  // Handle filter attribute.
  std::string filterAttr = getAttribute(element, "filter");
  bool hasShadowOnlyFilter = false;
  if (!filterAttr.empty() && filterAttr != "none") {
    std::string filterId = resolveUrl(filterAttr);
    auto filterIt = _defs.find(filterId);
    if (filterIt != _defs.end()) {
      bool filterConverted = convertFilterElement(filterIt->second, layer->filters, layer->styles,
                                                  &hasShadowOnlyFilter);
      if (!filterConverted) {
        // Filter could not be fully converted to PAGX format.
        if (hasShadowOnlyFilter) {
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
      auto childLayer = convertToLayer(child, inheritedStyle);
      if (childLayer) {
        layer->children.push_back(std::move(childLayer));
      }
      child = child->getNextSibling();
    }
  } else {
    // Shape element: convert to vector contents.
    // If this element has a shadow-only filter, skip adding fill because the fill is only
    // used as shadow source, not to be rendered directly.
    convertChildren(element, layer->contents, inheritedStyle, hasShadowOnlyFilter);
  }

  return layer;
}

void SVGParserImpl::convertChildren(const std::shared_ptr<DOMNode>& element,
                                    std::vector<std::unique_ptr<Element>>& contents,
                                    const InheritedStyle& inheritedStyle,
                                    bool shadowOnlyFilter) {
  const auto& tag = element->name;

  // Handle text element specially - it returns a Group with TextSpan.
  if (tag == "text") {
    auto textGroup = convertText(element, inheritedStyle);
    if (textGroup) {
      contents.push_back(std::move(textGroup));
    }
    return;
  }

  // Check if this is a use element referencing an image.
  // In that case, we don't add fill/stroke because the image already has its own fill.
  bool skipFillStroke = false;
  if (tag == "use") {
    std::string href = getAttribute(element, "xlink:href");
    if (href.empty()) {
      href = getAttribute(element, "href");
    }
    std::string refId = resolveUrl(href);
    auto it = _defs.find(refId);
    if (it != _defs.end() && it->second->name == "image") {
      skipFillStroke = true;
    }
  }

  auto shapeElement = convertElement(element);
  if (shapeElement) {
    contents.push_back(std::move(shapeElement));
  }

  if (!skipFillStroke) {
    if (shadowOnlyFilter) {
      // For shadowOnly filters, we need to add a fill to provide a source for the shadow.
      // The DropShadowFilter generates shadow based on the layer's content alpha channel.
      // We use black (alpha=1) as the fill color since the actual shadow color is determined
      // by the DropShadowFilter's color property, not the fill color.
      auto fillNode = std::make_unique<Fill>();
      auto solidColor = std::make_unique<SolidColor>();
      solidColor->color = {0, 0, 0, 1, ColorSpace::SRGB};
      fillNode->color = std::move(solidColor);
      contents.push_back(std::move(fillNode));
    } else {
      addFillStroke(element, contents, inheritedStyle);
    }
  }
}

std::unique_ptr<Element> SVGParserImpl::convertElement(
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

std::unique_ptr<Group> SVGParserImpl::convertG(const std::shared_ptr<DOMNode>& element,
                                               const InheritedStyle& parentStyle) {
  // Compute inherited style for this group element.
  InheritedStyle inheritedStyle = computeInheritedStyle(element, parentStyle);

  auto group = std::make_unique<Group>();

  // group->name (removed) = getAttribute(element, "id");

  std::string transform = getAttribute(element, "transform");
  if (!transform.empty()) {
    // For Group, we need to decompose the matrix into position/rotation/scale.
    // For simplicity, just store as position offset for translation.
    Matrix m = parseTransform(transform);
    group->position = {m.tx, m.ty};
    // Note: Full matrix decomposition would be more complex.
  }

  std::string opacity = getAttribute(element, "opacity");
  if (!opacity.empty()) {
    group->alpha = std::stof(opacity);
  }

  auto child = element->getFirstChild();
  while (child) {
    auto childElement = convertElement(child);
    if (childElement) {
      group->elements.push_back(std::move(childElement));
    }
    addFillStroke(child, group->elements, inheritedStyle);
    child = child->getNextSibling();
  }

  return group;
}

std::unique_ptr<Element> SVGParserImpl::convertRect(
    const std::shared_ptr<DOMNode>& element) {
  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
  float width = parseLength(getAttribute(element, "width"), _viewBoxWidth);
  float height = parseLength(getAttribute(element, "height"), _viewBoxHeight);
  float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
  float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);

  if (ry == 0) {
    ry = rx;
  }

  auto rect = std::make_unique<Rectangle>();
  rect->center.x = x + width / 2;
  rect->center.y = y + height / 2;
  rect->size.width = width;
  rect->size.height = height;
  rect->roundness = std::max(rx, ry);

  return rect;
}

std::unique_ptr<Element> SVGParserImpl::convertCircle(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float r = parseLength(getAttribute(element, "r"), _viewBoxWidth);

  auto ellipse = std::make_unique<Ellipse>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = r * 2;
  ellipse->size.height = r * 2;

  return ellipse;
}

std::unique_ptr<Element> SVGParserImpl::convertEllipse(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
  float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);

  auto ellipse = std::make_unique<Ellipse>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = rx * 2;
  ellipse->size.height = ry * 2;

  return ellipse;
}

std::unique_ptr<Element> SVGParserImpl::convertLine(
    const std::shared_ptr<DOMNode>& element) {
  float x1 = parseLength(getAttribute(element, "x1"), _viewBoxWidth);
  float y1 = parseLength(getAttribute(element, "y1"), _viewBoxHeight);
  float x2 = parseLength(getAttribute(element, "x2"), _viewBoxWidth);
  float y2 = parseLength(getAttribute(element, "y2"), _viewBoxHeight);

  auto path = std::make_unique<Path>();
  path->data.moveTo(x1, y1);
  path->data.lineTo(x2, y2);

  return path;
}

std::unique_ptr<Element> SVGParserImpl::convertPolyline(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<Path>();
  path->data = parsePoints(getAttribute(element, "points"), false);
  return path;
}

std::unique_ptr<Element> SVGParserImpl::convertPolygon(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<Path>();
  path->data = parsePoints(getAttribute(element, "points"), true);
  return path;
}

std::unique_ptr<Element> SVGParserImpl::convertPath(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<Path>();
  std::string d = getAttribute(element, "d");
  if (!d.empty()) {
    path->data = PathDataFromSVGString(d);
  }
  return path;
}

std::unique_ptr<Group> SVGParserImpl::convertText(const std::shared_ptr<DOMNode>& element,
                                                  const InheritedStyle& inheritedStyle) {
  auto group = std::make_unique<Group>();

  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);

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
      // Handle tspan element: extract text content from its children.
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

  if (!textContent.empty()) {
    auto textSpan = std::make_unique<TextSpan>();
    textSpan->position = {x, y};
    textSpan->text = textContent;

    // Font family: element attribute > inherited style.
    std::string fontFamily = getAttribute(element, "font-family");
    if (fontFamily.empty()) {
      fontFamily = inheritedStyle.fontFamily;
    }
    if (!fontFamily.empty()) {
      textSpan->font = fontFamily;
    }

    // Font size: element attribute > inherited style.
    std::string fontSize = getAttribute(element, "font-size");
    if (fontSize.empty()) {
      fontSize = inheritedStyle.fontSize;
    }
    if (!fontSize.empty()) {
      textSpan->fontSize = parseLength(fontSize, _viewBoxHeight);
    }

    // Font weight: element attribute > inherited style.
    std::string fontWeight = getAttribute(element, "font-weight");
    if (fontWeight.empty()) {
      fontWeight = inheritedStyle.fontWeight;
    }
    if (!fontWeight.empty()) {
      // SVG font-weight: normal=400, bold=700, or numeric 100-900.
      if (fontWeight == "normal") {
        textSpan->fontWeight = 400;
      } else if (fontWeight == "bold") {
        textSpan->fontWeight = 700;
      } else if (fontWeight == "lighter") {
        textSpan->fontWeight = 300;
      } else if (fontWeight == "bolder") {
        textSpan->fontWeight = 800;
      } else {
        textSpan->fontWeight = static_cast<int>(strtof(fontWeight.c_str(), nullptr));
      }
    }

    // Font style: element attribute > inherited style.
    std::string fontStyle = getAttribute(element, "font-style");
    if (fontStyle.empty()) {
      fontStyle = inheritedStyle.fontStyle;
    }
    if (!fontStyle.empty()) {
      textSpan->fontStyle = fontStyle;
    }

    // Letter spacing: element attribute > inherited style.
    std::string letterSpacing = getAttribute(element, "letter-spacing");
    if (letterSpacing.empty()) {
      letterSpacing = inheritedStyle.letterSpacing;
    }
    if (!letterSpacing.empty()) {
      textSpan->tracking = parseLength(letterSpacing, _viewBoxWidth);
    }

    group->elements.push_back(std::move(textSpan));

    // Add TextLayout modifier if text-anchor requires alignment.
    // SVG text-anchor maps to PAGX TextLayout.textAlign:
    //   start  -> Left (default, no TextLayout needed)
    //   middle -> Center
    //   end    -> End
    if (!anchor.empty() && anchor != "start") {
      auto textLayout = std::make_unique<TextLayout>();
      textLayout->width = 0;   // auto-width (Point Text mode)
      textLayout->height = 0;  // auto-height
      if (anchor == "middle") {
        textLayout->textAlign = TextAlign::Center;
      } else if (anchor == "end") {
        textLayout->textAlign = TextAlign::End;
      }
      group->elements.push_back(std::move(textLayout));
    }
  }

  addFillStroke(element, group->elements, inheritedStyle);
  return group;
}

std::unique_ptr<Element> SVGParserImpl::convertUse(
    const std::shared_ptr<DOMNode>& element) {
  std::string href = getAttribute(element, "xlink:href");
  if (href.empty()) {
    href = getAttribute(element, "href");
  }

  std::string refId = resolveUrl(href);
  auto it = _defs.find(refId);
  if (it == _defs.end()) {
    return nullptr;
  }

  // Parse position offset from use element.
  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);

  // Check if referenced element is an image.
  if (it->second->name == "image") {
    std::string imageHref = getAttribute(it->second, "xlink:href");
    if (imageHref.empty()) {
      imageHref = getAttribute(it->second, "href");
    }
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
    std::string resourceId = registerImageResource(imageHref);

    // Create a rectangle to display the image at original size.
    // The transform will be applied by the parent Layer's matrix.
    auto rect = std::make_unique<Rectangle>();
    rect->center.x = x + imageWidth / 2;
    rect->center.y = y + imageHeight / 2;
    rect->size.width = imageWidth;
    rect->size.height = imageHeight;

    // Create an ImagePattern fill for the rectangle.
    auto pattern = std::make_unique<ImagePattern>();
    pattern->image = "#" + resourceId;
    // Position the pattern at the rectangle's origin.
    pattern->matrix = Matrix::Translate(x, y);

    // Create a fill with the image pattern.
    auto fill = std::make_unique<Fill>();
    fill->color = std::move(pattern);

    // Create a group containing the rectangle and fill.
    auto group = std::make_unique<Group>();
    group->elements.push_back(std::move(rect));
    group->elements.push_back(std::move(fill));

    return group;
  }

  if (_options.expandUseReferences) {
    auto node = convertElement(it->second);
    if (node) {
      if (x != 0 || y != 0) {
        // Wrap in a group with translation.
        auto group = std::make_unique<Group>();
        group->position = {x, y};
        group->elements.push_back(std::move(node));
        return group;
      }
    }
    return node;
  }

  // For non-expanded use references, just create an empty group for now.
  auto group = std::make_unique<Group>();
  // group->name (removed) = "_useRef:" + refId;
  return group;
}

std::unique_ptr<LinearGradient> SVGParserImpl::convertLinearGradient(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto gradient = std::make_unique<LinearGradient>();

  gradient->id = getAttribute(element, "id");

  // Check gradientUnits - determines how gradient coordinates are interpreted.
  // Default is objectBoundingBox, meaning values are 0-1 ratios of the shape bounds.
  std::string gradientUnits = getAttribute(element, "gradientUnits", "objectBoundingBox");
  bool useOBB = (gradientUnits == "objectBoundingBox");

  // Parse gradient coordinates.
  float x1 = parseLength(getAttribute(element, "x1", "0%"), 1.0f);
  float y1 = parseLength(getAttribute(element, "y1", "0%"), 1.0f);
  float x2 = parseLength(getAttribute(element, "x2", "100%"), 1.0f);
  float y2 = parseLength(getAttribute(element, "y2", "0%"), 1.0f);

  // Parse gradientTransform.
  std::string gradientTransform = getAttribute(element, "gradientTransform");
  Matrix transformMatrix = gradientTransform.empty() ? Matrix::Identity()
                                                     : parseTransform(gradientTransform);

  if (useOBB) {
    // For objectBoundingBox, coordinates are normalized 0-1.
    // Convert to actual coordinates based on shape bounds.
    Point start = {shapeBounds.x + x1 * shapeBounds.width, shapeBounds.y + y1 * shapeBounds.height};
    Point end = {shapeBounds.x + x2 * shapeBounds.width, shapeBounds.y + y2 * shapeBounds.height};
    // Apply gradient transform after converting to actual coordinates.
    start = transformMatrix.mapPoint(start);
    end = transformMatrix.mapPoint(end);
    gradient->startPoint = start;
    gradient->endPoint = end;
  } else {
    // For userSpaceOnUse, coordinates are in user space.
    // Apply gradient transform to user space points.
    Point start = {x1, y1};
    Point end = {x2, y2};
    start = transformMatrix.mapPoint(start);
    end = transformMatrix.mapPoint(end);
    gradient->startPoint = start;
    gradient->endPoint = end;
  }

  // Parse stops.
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "stop") {
      ColorStop stop;
      stop.offset = parseLength(getAttribute(child, "offset", "0"), 1.0f);
      stop.color = parseColor(getAttribute(child, "stop-color", "#000000"));
      float opacity = parseLength(getAttribute(child, "stop-opacity", "1"), 1.0f);
      stop.color.alpha = opacity;
      gradient->colorStops.push_back(stop);
    }
    child = child->getNextSibling();
  }

  return gradient;
}

std::unique_ptr<RadialGradient> SVGParserImpl::convertRadialGradient(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto gradient = std::make_unique<RadialGradient>();

  gradient->id = getAttribute(element, "id");

  // Check gradientUnits - determines how gradient coordinates are interpreted.
  std::string gradientUnits = getAttribute(element, "gradientUnits", "objectBoundingBox");
  bool useOBB = (gradientUnits == "objectBoundingBox");

  // Parse gradient coordinates.
  float cx = parseLength(getAttribute(element, "cx", "50%"), 1.0f);
  float cy = parseLength(getAttribute(element, "cy", "50%"), 1.0f);
  float r = parseLength(getAttribute(element, "r", "50%"), 1.0f);

  // Parse gradientTransform.
  std::string gradientTransform = getAttribute(element, "gradientTransform");
  Matrix transformMatrix = gradientTransform.empty() ? Matrix::Identity()
                                                     : parseTransform(gradientTransform);

  if (useOBB) {
    // For objectBoundingBox, convert normalized coordinates to actual coordinates.
    Point center = {shapeBounds.x + cx * shapeBounds.width,
                    shapeBounds.y + cy * shapeBounds.height};
    // Radius is scaled by the average of width and height.
    float actualRadius = r * (shapeBounds.width + shapeBounds.height) / 2.0f;

    // Apply gradientTransform after converting to actual coordinates.
    center = transformMatrix.mapPoint(center);
    // For radius, account for scaling in the transform.
    float scaleX = std::sqrt(transformMatrix.a * transformMatrix.a +
                             transformMatrix.b * transformMatrix.b);
    float scaleY = std::sqrt(transformMatrix.c * transformMatrix.c +
                             transformMatrix.d * transformMatrix.d);
    actualRadius *= (scaleX + scaleY) / 2.0f;

    gradient->center = center;
    gradient->radius = actualRadius;

    // Store the matrix for non-uniform scaling (rotation, skew, etc.).
    if (!transformMatrix.isIdentity()) {
      gradient->matrix = transformMatrix;
    }
  } else {
    // For userSpaceOnUse, coordinates are in user space.
    if (!gradientTransform.empty()) {
      Point center = {cx, cy};
      center = transformMatrix.mapPoint(center);
      gradient->center = center;

      float scaleX = std::sqrt(transformMatrix.a * transformMatrix.a +
                               transformMatrix.b * transformMatrix.b);
      float scaleY = std::sqrt(transformMatrix.c * transformMatrix.c +
                               transformMatrix.d * transformMatrix.d);
      gradient->radius = r * (scaleX + scaleY) / 2.0f;

      if (!transformMatrix.isIdentity()) {
        gradient->matrix = transformMatrix;
      }
    } else {
      gradient->center.x = cx;
      gradient->center.y = cy;
      gradient->radius = r;
    }
  }

  // Parse stops.
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "stop") {
      ColorStop stop;
      stop.offset = parseLength(getAttribute(child, "offset", "0"), 1.0f);
      stop.color = parseColor(getAttribute(child, "stop-color", "#000000"));
      float opacity = parseLength(getAttribute(child, "stop-opacity", "1"), 1.0f);
      stop.color.alpha = opacity;
      gradient->colorStops.push_back(stop);
    }
    child = child->getNextSibling();
  }

  return gradient;
}

std::unique_ptr<ImagePattern> SVGParserImpl::convertPattern(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto pattern = std::make_unique<ImagePattern>();

  pattern->id = getAttribute(element, "id");

  // SVG patterns use repeat by default.
  pattern->tileModeX = TileMode::Repeat;
  pattern->tileModeY = TileMode::Repeat;

  // Parse pattern dimensions from SVG attributes.
  float patternWidth = parseLength(getAttribute(element, "width"), 1.0f);
  float patternHeight = parseLength(getAttribute(element, "height"), 1.0f);

  // Check patternUnits - determines how pattern x/y/width/height are interpreted.
  // Default is objectBoundingBox, meaning values are relative to the shape bounds.
  std::string patternUnitsStr = getAttribute(element, "patternUnits", "objectBoundingBox");
  bool patternUnitsOBB = (patternUnitsStr == "objectBoundingBox");

  // Check patternContentUnits - determines how pattern content coordinates are interpreted.
  // Default is userSpaceOnUse, meaning content uses absolute coordinates.
  std::string contentUnitsStr = getAttribute(element, "patternContentUnits", "userSpaceOnUse");
  bool contentUnitsOBB = (contentUnitsStr == "objectBoundingBox");

  // Calculate the actual tile size in user space.
  // When patternUnits is objectBoundingBox, pattern dimensions are 0-1 ratios of shape bounds.
  float tileWidth = patternUnitsOBB ? patternWidth * shapeBounds.width : patternWidth;
  float tileHeight = patternUnitsOBB ? patternHeight * shapeBounds.height : patternHeight;

  // Look for image reference inside the pattern.
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "use") {
      std::string href = getAttribute(child, "xlink:href");
      if (href.empty()) {
        href = getAttribute(child, "href");
      }
      std::string imageId = resolveUrl(href);

      // Find the referenced image in defs.
      auto imgIt = _defs.find(imageId);
      if (imgIt != _defs.end() && imgIt->second->name == "image") {
        std::string imageHref = getAttribute(imgIt->second, "xlink:href");
        if (imageHref.empty()) {
          imageHref = getAttribute(imgIt->second, "href");
        }

        // Register the image resource and use the reference ID.
        std::string resourceId = registerImageResource(imageHref);
        pattern->image = "#" + resourceId;

        // Get image display dimensions from SVG (these are the dimensions in pattern content space).
        float imageWidth = parseLength(getAttribute(imgIt->second, "width"), 1.0f);
        float imageHeight = parseLength(getAttribute(imgIt->second, "height"), 1.0f);

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
        if (contentUnitsOBB) {
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
      std::string imageHref = getAttribute(child, "xlink:href");
      if (imageHref.empty()) {
        imageHref = getAttribute(child, "href");
      }

      // Register the image resource and use the reference ID.
      std::string resourceId = registerImageResource(imageHref);
      pattern->image = "#" + resourceId;

      float imageWidth = parseLength(getAttribute(child, "width"), 1.0f);
      float imageHeight = parseLength(getAttribute(child, "height"), 1.0f);

      if (contentUnitsOBB) {
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

void SVGParserImpl::addFillStroke(const std::shared_ptr<DOMNode>& element,
                                  std::vector<std::unique_ptr<Element>>& contents,
                                  const InheritedStyle& inheritedStyle) {
  // Get shape bounds for pattern calculations (computed once, used if needed).
  Rect shapeBounds = getShapeBounds(element);

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
      auto fillNode = std::make_unique<Fill>();
      auto solidColor = std::make_unique<SolidColor>();
      solidColor->color = {0, 0, 0, 1, ColorSpace::SRGB};
      fillNode->color = std::move(solidColor);
      contents.push_back(std::move(fillNode));
    } else if (fill.find("url(") == 0) {
      auto fillNode = std::make_unique<Fill>();
      std::string refId = resolveUrl(fill);
      // Use getColorSourceForRef which handles reference counting.
      fillNode->color = getColorSourceForRef(refId, shapeBounds);
      // Apply fill-opacity even for url() fills.
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (fillOpacity.empty()) {
        fillOpacity = inheritedStyle.fillOpacity;
      }
      if (!fillOpacity.empty()) {
        fillNode->alpha = std::stof(fillOpacity);
      }
      contents.push_back(std::move(fillNode));
    } else {
      auto fillNode = std::make_unique<Fill>();

      // Determine effective fill-opacity.
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (fillOpacity.empty()) {
        fillOpacity = inheritedStyle.fillOpacity;
      }
      if (!fillOpacity.empty()) {
        fillNode->alpha = std::stof(fillOpacity);
      }

      // Convert color to SolidColor for PAGX compatibility.
      // SolidColor is always inlined (no id).
      Color parsedColor = parseColor(fill);
      auto solidColor = std::make_unique<SolidColor>();
      solidColor->color = parsedColor;
      fillNode->color = std::move(solidColor);

      // Determine effective fill-rule.
      std::string fillRule = getAttribute(element, "fill-rule");
      if (fillRule.empty()) {
        fillRule = inheritedStyle.fillRule;
      }
      if (fillRule == "evenodd") {
        fillNode->fillRule = FillRule::EvenOdd;
      }

      contents.push_back(std::move(fillNode));
    }
  }

  // Determine effective stroke value (element attribute overrides inherited).
  std::string stroke = getAttribute(element, "stroke");
  if (stroke.empty()) {
    stroke = inheritedStyle.stroke;
  }

  if (!stroke.empty() && stroke != "none") {
    auto strokeNode = std::make_unique<Stroke>();

    if (stroke.find("url(") == 0) {
      std::string refId = resolveUrl(stroke);
      // Use getColorSourceForRef which handles reference counting.
      strokeNode->color = getColorSourceForRef(refId, shapeBounds);
    } else {
      // Determine effective stroke-opacity.
      std::string strokeOpacity = getAttribute(element, "stroke-opacity");
      if (strokeOpacity.empty()) {
        strokeOpacity = inheritedStyle.strokeOpacity;
      }
      if (!strokeOpacity.empty()) {
        strokeNode->alpha = std::stof(strokeOpacity);
      }

      // Convert color to SolidColor for PAGX compatibility.
      // SolidColor is always inlined (no id).
      Color parsedColor = parseColor(stroke);
      auto solidColor = std::make_unique<SolidColor>();
      solidColor->color = parsedColor;
      strokeNode->color = std::move(solidColor);
    }

    std::string strokeWidth = getAttribute(element, "stroke-width");
    if (strokeWidth.empty()) {
      strokeWidth = inheritedStyle.strokeWidth;
    }
    if (!strokeWidth.empty()) {
      strokeNode->width = parseLength(strokeWidth, _viewBoxWidth);
    }

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
      strokeNode->miterLimit = std::stof(strokeMiterlimit);
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

    contents.push_back(std::move(strokeNode));
  }
}

Rect SVGParserImpl::getShapeBounds(const std::shared_ptr<DOMNode>& element) {
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
    std::string href = getAttribute(element, "xlink:href");
    if (href.empty()) {
      href = getAttribute(element, "href");
    }
    std::string refId = resolveUrl(href);
    auto it = _defs.find(refId);
    if (it != _defs.end()) {
      Rect refBounds = getShapeBounds(it->second);
      float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
      float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
      return Rect::MakeXYWH(refBounds.x + x, refBounds.y + y, refBounds.width, refBounds.height);
    }
  }

  return Rect::MakeXYWH(0, 0, 0, 0);
}

Matrix SVGParserImpl::parseTransform(const std::string& value) {
  Matrix result = Matrix::Identity();
  if (value.empty()) {
    return result;
  }

  const char* ptr = value.c_str();
  const char* end = ptr + value.length();

  auto skipWS = [&]() {
    while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
      ++ptr;
    }
  };

  auto readNumber = [&]() -> float {
    skipWS();
    const char* start = ptr;
    if (*ptr == '-' || *ptr == '+') {
      ++ptr;
    }
    while (ptr < end && (std::isdigit(*ptr) || *ptr == '.')) {
      ++ptr;
    }
    if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
      ++ptr;
      if (*ptr == '-' || *ptr == '+') {
        ++ptr;
      }
      while (ptr < end && std::isdigit(*ptr)) {
        ++ptr;
      }
    }
    return std::stof(std::string(start, ptr));
  };

  while (ptr < end) {
    skipWS();
    if (ptr >= end) {
      break;
    }

    std::string func;
    while (ptr < end && std::isalpha(*ptr)) {
      func += *ptr++;
    }

    skipWS();
    if (*ptr != '(') {
      break;
    }
    ++ptr;

    Matrix m = Matrix::Identity();

    if (func == "translate") {
      float tx = readNumber();
      skipWS();
      float ty = 0;
      if (ptr < end && *ptr != ')') {
        ty = readNumber();
      }
      m = Matrix::Translate(tx, ty);
    } else if (func == "scale") {
      float sx = readNumber();
      skipWS();
      float sy = sx;
      if (ptr < end && *ptr != ')') {
        sy = readNumber();
      }
      m = Matrix::Scale(sx, sy);
    } else if (func == "rotate") {
      float angle = readNumber();
      skipWS();
      if (ptr < end && *ptr != ')') {
        float cx = readNumber();
        float cy = readNumber();
        m = Matrix::Translate(cx, cy) * Matrix::Rotate(angle) * Matrix::Translate(-cx, -cy);
      } else {
        m = Matrix::Rotate(angle);
      }
    } else if (func == "skewX") {
      float angle = readNumber();
      float radians = angle * 3.14159265358979323846f / 180.0f;
      m.c = std::tan(radians);
    } else if (func == "skewY") {
      float angle = readNumber();
      float radians = angle * 3.14159265358979323846f / 180.0f;
      m.b = std::tan(radians);
    } else if (func == "matrix") {
      m.a = readNumber();
      m.b = readNumber();
      m.c = readNumber();
      m.d = readNumber();
      m.tx = readNumber();
      m.ty = readNumber();
    }

    skipWS();
    if (*ptr == ')') {
      ++ptr;
    }

    result = result * m;
  }

  return result;
}

Color SVGParserImpl::parseColor(const std::string& value) {
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
      std::string expanded = std::string() + r + r + g + g + b + b;
      hex = std::stoul(expanded, nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    } else if (value.length() == 7) {
      hex = std::stoul(value.substr(1), nullptr, 16);
      color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
      color.alpha = 1.0f;
      return color;
    } else if (value.length() == 9) {
      hex = std::stoul(value.substr(1), nullptr, 16);
      color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
      color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
      color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
      color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
      return color;
    }
  }

  if (value.find("rgb") == 0) {
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
  if (value.find("color(") == 0) {
    auto start = value.find('(');
    auto end = value.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto inner = value.substr(start + 1, end - start - 1);
      // Trim leading whitespace
      inner.erase(0, inner.find_first_not_of(" \t"));

      // Detect color space identifier
      ColorSpace colorSpace = ColorSpace::SRGB;
      if (inner.find("display-p3") == 0) {
        colorSpace = ColorSpace::DisplayP3;
        inner = inner.substr(10);  // Skip "display-p3"
      } else if (inner.find("a98-rgb") == 0) {
        // Adobe RGB 1998 - convert to sRGB approximation
        colorSpace = ColorSpace::SRGB;
        inner = inner.substr(7);  // Skip "a98-rgb"
      } else if (inner.find("rec2020") == 0) {
        // Rec.2020 - convert to sRGB approximation
        colorSpace = ColorSpace::SRGB;
        inner = inner.substr(7);  // Skip "rec2020"
      } else if (inner.find("srgb") == 0) {
        colorSpace = ColorSpace::SRGB;
        inner = inner.substr(4);  // Skip "srgb"
      }

      // Trim whitespace after color space name
      inner.erase(0, inner.find_first_not_of(" \t"));
      inner.erase(inner.find_last_not_of(" \t") + 1);

      // Parse space-separated values and optional "/ alpha"
      std::vector<float> components;
      float alpha = 1.0f;
      const char* ptr = inner.c_str();
      const char* endPtr = ptr + inner.size();
      bool foundSlash = false;
      while (ptr < endPtr) {
        // Skip whitespace.
        while (ptr < endPtr && std::isspace(*ptr)) {
          ++ptr;
        }
        if (ptr >= endPtr) {
          break;
        }
        // Check for slash separator.
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
        Color color = {};
        color.red = components[0];
        color.green = components[1];
        color.blue = components[2];
        color.alpha = alpha;
        color.colorSpace = colorSpace;
        return color;
      }
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

std::string SVGParserImpl::colorToHex(const std::string& value) {
  if (value.empty() || value == "none") {
    return value;
  }
  // Already a hex color, return as-is.
  if (value[0] == '#') {
    return value;
  }
  // Already a PAGX p3() color, return as-is.
  if (value.substr(0, 3) == "p3(") {
    return value;
  }
  // Already a PAGX srgb() color, return as-is.
  if (value.substr(0, 5) == "srgb(") {
    return value;
  }
  // url() references should be returned as-is.
  if (value.find("url(") == 0) {
    return value;
  }
  // CSS Color Level 4: color(display-p3 r g b) -> p3(r, g, b)
  if (value.find("color(display-p3") == 0) {
    auto start = value.find("display-p3");
    auto end = value.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto inner = value.substr(start + 10, end - start - 10);  // skip "display-p3"
      // Trim whitespace
      inner.erase(0, inner.find_first_not_of(" \t"));
      inner.erase(inner.find_last_not_of(" \t") + 1);
      // Parse space-separated values and optional "/ alpha"
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
        char buf[64] = {};
        if (alpha < 1.0f) {
          snprintf(buf, sizeof(buf), "p3(%.4g, %.4g, %.4g, %.4g)", components[0], components[1],
                   components[2], alpha);
        } else {
          snprintf(buf, sizeof(buf), "p3(%.4g, %.4g, %.4g)", components[0], components[1],
                   components[2]);
        }
        return std::string(buf);
      }
    }
  }
  // Parse the color (handles named colors, rgb, rgba, etc.) and convert to hex.
  Color color = parseColor(value);
  // Convert to hex string.
  auto toHex = [](float v) {
    int i = static_cast<int>(std::round(v * 255.0f));
    i = std::max(0, std::min(255, i));
    char buf[3] = {};
    snprintf(buf, sizeof(buf), "%02X", i);
    return std::string(buf);
  };
  std::string result = "#" + toHex(color.red) + toHex(color.green) + toHex(color.blue);
  if (color.alpha < 1.0f) {
    result += toHex(color.alpha);
  }
  return result;
}

float SVGParserImpl::parseLength(const std::string& value, float containerSize) {
  if (value.empty()) {
    return 0;
  }

  size_t idx = 0;
  float num = std::stof(value, &idx);

  std::string unit = value.substr(idx);
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
    return num * 16.0f;  // Assume 16px base font.
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

std::vector<float> SVGParserImpl::parseViewBox(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  return ParseSpaceSeparatedFloats(value);
}

PathData SVGParserImpl::parsePoints(const std::string& value, bool closed) {
  PathData path;
  if (value.empty()) {
    return path;
  }

  // Parse space/comma-separated float values.
  std::vector<float> points;
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

std::string SVGParserImpl::resolveUrl(const std::string& url) {
  if (url.empty()) {
    return "";
  }
  // Handle url(#id) format.
  if (url.find("url(") == 0) {
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

std::string SVGParserImpl::registerImageResource(const std::string& imageSource) {
  if (imageSource.empty()) {
    return "";
  }

  // Check if this image source has already been registered.
  auto it = _imageSourceToId.find(imageSource);
  if (it != _imageSourceToId.end()) {
    return it->second;
  }

  // Generate a unique image ID that doesn't conflict with existing SVG IDs.
  std::string imageId = generateUniqueId("image");

  // Create and add the image resource to the document.
  auto imageNode = std::make_unique<Image>();
  imageNode->id = imageId;
  imageNode->source = imageSource;
  _document->resources.push_back(std::move(imageNode));

  // Cache the mapping.
  _imageSourceToId[imageSource] = imageId;

  return imageId;
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
      return PathDataToSVGString(pathA->data) == PathDataToSVGString(pathB->data);
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
  if (!layer->matrix.isIdentity() || layer->alpha != 1.0f) {
    return false;
  }

  const auto* first = layer->contents[0].get();
  const auto* second = layer->contents[1].get();

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

void SVGParserImpl::mergeAdjacentLayers(std::vector<std::unique_ptr<Layer>>& layers) {
  if (layers.size() < 2) {
    return;
  }

  std::vector<std::unique_ptr<Layer>> merged;
  size_t i = 0;

  while (i < layers.size()) {
    const Element* geomA = nullptr;
    const Element* painterA = nullptr;

    if (isSimpleShapeLayer(layers[i].get(), geomA, painterA)) {
      // Check if the next layer has the same geometry.
      if (i + 1 < layers.size()) {
        const Element* geomB = nullptr;
        const Element* painterB = nullptr;

        if (isSimpleShapeLayer(layers[i + 1].get(), geomB, painterB) &&
            isSameGeometry(geomA, geomB)) {
          // Merge: one has Fill, the other has Stroke.
          bool aHasFill = (painterA->nodeType() == NodeType::Fill);
          bool bHasFill = (painterB->nodeType() == NodeType::Fill);

          if (aHasFill != bHasFill) {
            // Create merged layer.
            auto mergedLayer = std::make_unique<Layer>();

            // Move geometry from first layer.
            mergedLayer->contents.push_back(std::move(layers[i]->contents[0]));

            // Add Fill first, then Stroke (standard order).
            if (aHasFill) {
              mergedLayer->contents.push_back(std::move(layers[i]->contents[1]));
              mergedLayer->contents.push_back(std::move(layers[i + 1]->contents[1]));
            } else {
              mergedLayer->contents.push_back(std::move(layers[i + 1]->contents[1]));
              mergedLayer->contents.push_back(std::move(layers[i]->contents[1]));
            }

            merged.push_back(std::move(mergedLayer));
            i += 2;  // Skip both layers.
            continue;
          }
        }
      }
    }

    // No merge, keep the layer as is.
    merged.push_back(std::move(layers[i]));
    i++;
  }

  layers = std::move(merged);
}

std::unique_ptr<Layer> SVGParserImpl::convertMaskElement(
    const std::shared_ptr<DOMNode>& maskElement, const InheritedStyle& parentStyle) {
  auto maskLayer = std::make_unique<Layer>();
  maskLayer->id = getAttribute(maskElement, "id");
  maskLayer->name = maskLayer->id;
  maskLayer->visible = false;

  // Compute inherited style from the mask element itself (it may have fill="white" etc.).
  InheritedStyle maskStyle = computeInheritedStyle(maskElement, parentStyle);

  // Parse mask contents.
  auto child = maskElement->getFirstChild();
  while (child) {
    if (child->name == "rect" || child->name == "circle" || child->name == "ellipse" ||
        child->name == "path" || child->name == "polygon" || child->name == "polyline") {
      InheritedStyle inheritedStyle = computeInheritedStyle(child, maskStyle);
      std::string transformStr = getAttribute(child, "transform");
      if (!transformStr.empty()) {
        // If child has transform, wrap it in a sub-layer with the matrix.
        auto subLayer = std::make_unique<Layer>();
        subLayer->matrix = parseTransform(transformStr);
        convertChildren(child, subLayer->contents, inheritedStyle);
        maskLayer->children.push_back(std::move(subLayer));
      } else {
        convertChildren(child, maskLayer->contents, inheritedStyle);
      }
    } else if (child->name == "g") {
      // Handle group inside mask.
      InheritedStyle inheritedStyle = computeInheritedStyle(child, maskStyle);
      std::string groupTransform = getAttribute(child, "transform");
      auto groupChild = child->getFirstChild();
      while (groupChild) {
        std::string childTransform = getAttribute(groupChild, "transform");
        // Combine group transform and child transform if needed.
        if (!groupTransform.empty() || !childTransform.empty()) {
          auto subLayer = std::make_unique<Layer>();
          Matrix combinedMatrix = Matrix::Identity();
          if (!groupTransform.empty()) {
            combinedMatrix = parseTransform(groupTransform);
          }
          if (!childTransform.empty()) {
            combinedMatrix = combinedMatrix * parseTransform(childTransform);
          }
          subLayer->matrix = combinedMatrix;
          convertChildren(groupChild, subLayer->contents, inheritedStyle);
          maskLayer->children.push_back(std::move(subLayer));
        } else {
          convertChildren(groupChild, maskLayer->contents, inheritedStyle);
        }
        groupChild = groupChild->getNextSibling();
      }
    }
    child = child->getNextSibling();
  }

  return maskLayer;
}

bool SVGParserImpl::convertFilterElement(
    const std::shared_ptr<DOMNode>& filterElement,
    std::vector<std::unique_ptr<LayerFilter>>& filters,
    std::vector<std::unique_ptr<LayerStyle>>& styles,
    bool* outShadowOnly) {
  size_t initialFilterCount = filters.size();
  size_t initialStyleCount = styles.size();

  // Collect all filter primitives for analysis.
  std::vector<std::shared_ptr<DOMNode>> primitives;
  bool hasMerge = false;
  auto child = filterElement->getFirstChild();
  while (child) {
    if (!child->name.empty() && child->name.substr(0, 2) == "fe") {
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
  if (outShadowOnly != nullptr) {
    *outShadowOnly = shadowOnly;
  }

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
        // Extract offset from feOffset.
        float offsetX = 0, offsetY = 0;
        std::string dx = getAttribute(primitives[i + 1], "dx", "0");
        std::string dy = getAttribute(primitives[i + 1], "dy", "0");
        offsetX = strtof(dx.c_str(), nullptr);
        offsetY = strtof(dy.c_str(), nullptr);

        // Extract blur from feGaussianBlur.
        std::string stdDeviation = getAttribute(primitives[i + 2], "stdDeviation", "0");
        auto blurValues = ParseSpaceSeparatedFloats(stdDeviation);
        float blurX = blurValues.empty() ? 0 : blurValues[0];
        float blurY = blurValues.size() > 1 ? blurValues[1] : blurX;

        // Extract color from the second feColorMatrix.
        // Format: "0 0 0 0 R 0 0 0 0 G 0 0 0 0 B 0 0 0 A 0" where R,G,B are 0-1 and A is alpha.
        Color shadowColor = {0, 0, 0, 1.0f};
        std::string colorMatrix = getAttribute(primitives[i + 4], "values");
        if (!colorMatrix.empty()) {
          auto values = ParseSpaceSeparatedFloats(colorMatrix);
          // R is at index 4, G at index 9, B at index 14, A at index 18.
          if (values.size() >= 19) {
            shadowColor.red = values[4];
            shadowColor.green = values[9];
            shadowColor.blue = values[14];
            shadowColor.alpha = values[18];
          }
        }

        auto dropShadow = std::make_unique<DropShadowFilter>();
        dropShadow->offsetX = offsetX;
        dropShadow->offsetY = offsetY;
        dropShadow->blurrinessX = blurX;
        dropShadow->blurrinessY = blurY;
        dropShadow->color = shadowColor;
        dropShadow->shadowOnly = shadowOnly;
        filters.push_back(std::move(dropShadow));

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
        // Extract offset from feOffset.
        float offsetX = 0, offsetY = 0;
        std::string dx = getAttribute(primitives[i + 1], "dx", "0");
        std::string dy = getAttribute(primitives[i + 1], "dy", "0");
        offsetX = strtof(dx.c_str(), nullptr);
        offsetY = strtof(dy.c_str(), nullptr);

        // Extract blur from feGaussianBlur.
        std::string stdDeviation = getAttribute(primitives[i + 2], "stdDeviation", "0");
        auto blurValues = ParseSpaceSeparatedFloats(stdDeviation);
        float blurX = blurValues.empty() ? 0 : blurValues[0];
        float blurY = blurValues.size() > 1 ? blurValues[1] : blurX;

        // Extract color from feColorMatrix.
        Color shadowColor = {0, 0, 0, 1.0f};
        std::string colorMatrix = getAttribute(primitives[i + 3], "values");
        if (!colorMatrix.empty()) {
          auto values = ParseSpaceSeparatedFloats(colorMatrix);
          if (values.size() >= 19) {
            shadowColor.red = values[4];
            shadowColor.green = values[9];
            shadowColor.blue = values[14];
            shadowColor.alpha = values[18];
          }
        }

        auto dropShadow = std::make_unique<DropShadowFilter>();
        dropShadow->offsetX = offsetX;
        dropShadow->offsetY = offsetY;
        dropShadow->blurrinessX = blurX;
        dropShadow->blurrinessY = blurY;
        dropShadow->color = shadowColor;
        dropShadow->shadowOnly = shadowOnly;
        filters.push_back(std::move(dropShadow));

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
        // Extract offset from feOffset.
        float offsetX = 0, offsetY = 0;
        std::string dx = getAttribute(primitives[i + 1], "dx", "0");
        std::string dy = getAttribute(primitives[i + 1], "dy", "0");
        offsetX = strtof(dx.c_str(), nullptr);
        offsetY = strtof(dy.c_str(), nullptr);

        // Extract color from feColorMatrix.
        // Format: "0 0 0 0 R 0 0 0 0 G 0 0 0 0 B 0 0 0 A 0" where R,G,B are 0-1 and A is alpha.
        Color shadowColor = {0, 0, 0, 1.0f};
        std::string colorMatrix = getAttribute(primitives[i + 2], "values");
        if (!colorMatrix.empty()) {
          auto values = ParseSpaceSeparatedFloats(colorMatrix);
          // R is at index 4, G at index 9, B at index 14, A at index 18.
          if (values.size() >= 19) {
            shadowColor.red = values[4];
            shadowColor.green = values[9];
            shadowColor.blue = values[14];
            shadowColor.alpha = values[18];
          }
        }

        auto dropShadow = std::make_unique<DropShadowFilter>();
        dropShadow->offsetX = offsetX;
        dropShadow->offsetY = offsetY;
        dropShadow->blurrinessX = 0;
        dropShadow->blurrinessY = 0;
        dropShadow->color = shadowColor;
        dropShadow->shadowOnly = shadowOnly;
        filters.push_back(std::move(dropShadow));

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
    // We skip inner shadow patterns as they are not supported by PAGX conversion.
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
          // Skip inner shadow pattern - not supported by PAGX conversion.
          // Inner shadow is a complex effect that cannot be represented as DropShadowFilter.
          i++;
          continue;
        }

        // Extract blur from feGaussianBlur.
        std::string stdDeviation = getAttribute(node, "stdDeviation", "0");
        auto blurValues = ParseSpaceSeparatedFloats(stdDeviation);
        float blurX = blurValues.empty() ? 0 : blurValues[0];
        float blurY = blurValues.size() > 1 ? blurValues[1] : blurX;

        // Extract offset from feOffset.
        float offsetX = 0, offsetY = 0;
        std::string dx = getAttribute(primitives[i + 1], "dx", "0");
        std::string dy = getAttribute(primitives[i + 1], "dy", "0");
        offsetX = strtof(dx.c_str(), nullptr);
        offsetY = strtof(dy.c_str(), nullptr);

        // Look for feColorMatrix after feOffset for shadow color.
        Color shadowColor = {0, 0, 0, 1.0f};
        size_t colorMatrixIdx = i + 2;
        if (colorMatrixIdx < primitives.size() && primitives[colorMatrixIdx]->name == "feColorMatrix") {
          std::string colorMatrix = getAttribute(primitives[colorMatrixIdx], "values");
          if (!colorMatrix.empty()) {
            auto values = ParseSpaceSeparatedFloats(colorMatrix);
            if (values.size() >= 19) {
              shadowColor.red = values[4];
              shadowColor.green = values[9];
              shadowColor.blue = values[14];
              shadowColor.alpha = values[18];
            }
          }
        }

        auto dropShadow = std::make_unique<DropShadowFilter>();
        dropShadow->offsetX = offsetX;
        dropShadow->offsetY = offsetY;
        dropShadow->blurrinessX = blurX;
        dropShadow->blurrinessY = blurY;
        dropShadow->color = shadowColor;
        dropShadow->shadowOnly = shadowOnly;
        filters.push_back(std::move(dropShadow));

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
        // Extract offset from feOffset.
        float offsetX = 0, offsetY = 0;
        std::string dx = getAttribute(node, "dx", "0");
        std::string dy = getAttribute(node, "dy", "0");
        offsetX = strtof(dx.c_str(), nullptr);
        offsetY = strtof(dy.c_str(), nullptr);

        // Extract blur from feGaussianBlur.
        std::string stdDeviation = getAttribute(primitives[i + 1], "stdDeviation", "0");
        auto blurValues = ParseSpaceSeparatedFloats(stdDeviation);
        float blurX = blurValues.empty() ? 0 : blurValues[0];
        float blurY = blurValues.size() > 1 ? blurValues[1] : blurX;

        // Look for feColorMatrix after feGaussianBlur for shadow color.
        Color shadowColor = {0, 0, 0, 1.0f};
        size_t colorMatrixIdx = i + 2;
        if (colorMatrixIdx < primitives.size() && primitives[colorMatrixIdx]->name == "feColorMatrix") {
          std::string colorMatrix = getAttribute(primitives[colorMatrixIdx], "values");
          if (!colorMatrix.empty()) {
            auto values = ParseSpaceSeparatedFloats(colorMatrix);
            if (values.size() >= 19) {
              shadowColor.red = values[4];
              shadowColor.green = values[9];
              shadowColor.blue = values[14];
              shadowColor.alpha = values[18];
            }
          }
        }

        auto dropShadow = std::make_unique<DropShadowFilter>();
        dropShadow->offsetX = offsetX;
        dropShadow->offsetY = offsetY;
        dropShadow->blurrinessX = blurX;
        dropShadow->blurrinessY = blurY;
        dropShadow->color = shadowColor;
        dropShadow->shadowOnly = shadowOnly;
        filters.push_back(std::move(dropShadow));

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
        std::string stdDeviation = getAttribute(node, "stdDeviation", "0");
        auto devValues = ParseSpaceSeparatedFloats(stdDeviation);
        float devX = devValues.empty() ? 0 : devValues[0];
        float devY = devValues.size() > 1 ? devValues[1] : devX;
        auto blurFilter = std::make_unique<BlurFilter>();
        blurFilter->blurrinessX = devX;
        blurFilter->blurrinessY = devY;
        filters.push_back(std::move(blurFilter));
      }
    }

    i++;
  }

  // Return true if any filter or style was added.
  return filters.size() > initialFilterCount || styles.size() > initialStyleCount;
}

void SVGParserImpl::collectAllIds(const std::shared_ptr<DOMNode>& node) {
  if (!node) {
    return;
  }

  // Collect id from this node.
  auto [found, id] = node->findAttribute("id");
  if (found && !id.empty()) {
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

std::string SVGParserImpl::generateUniqueId(const std::string& prefix) {
  std::string id;
  do {
    id = "_" + prefix + std::to_string(_nextGeneratedId++);
  } while (_existingIds.count(id) > 0);
  _existingIds.insert(id);
  return id;
}

void SVGParserImpl::parseCustomData(const std::shared_ptr<DOMNode>& element, Layer* layer) {
  if (!element || !layer) {
    return;
  }

  // Iterate through all attributes and find data-* ones.
  for (const auto& attr : element->attributes) {
    if (attr.name.length() > 5 && attr.name.substr(0, 5) == "data-") {
      // Remove "data-" prefix and store in customData.
      std::string key = attr.name.substr(5);
      layer->customData[key] = attr.value;
    }
  }
}

void SVGParserImpl::countColorSourceReferences(const std::shared_ptr<DOMNode>& root) {
  // Traverse all elements and count references to gradients/patterns.
  auto child = root->getFirstChild();
  while (child) {
    if (child->name != "defs") {
      countColorSourceReferencesInElement(child);
    }
    child = child->getNextSibling();
  }
}

void SVGParserImpl::countColorSourceReferencesInElement(const std::shared_ptr<DOMNode>& element) {
  if (!element) {
    return;
  }

  // Check fill attribute for url() references.
  std::string fill = getAttribute(element, "fill");
  if (!fill.empty() && fill.find("url(") == 0) {
    std::string refId = resolveUrl(fill);
    auto it = _defs.find(refId);
    if (it != _defs.end()) {
      // Only count gradients and patterns, not masks/clipPaths/filters.
      const auto& defName = it->second->name;
      if (defName == "linearGradient" || defName == "radialGradient" || defName == "pattern") {
        _colorSourceRefCount[refId]++;
      }
    }
  }

  // Check stroke attribute for url() references.
  std::string stroke = getAttribute(element, "stroke");
  if (!stroke.empty() && stroke.find("url(") == 0) {
    std::string refId = resolveUrl(stroke);
    auto it = _defs.find(refId);
    if (it != _defs.end()) {
      const auto& defName = it->second->name;
      if (defName == "linearGradient" || defName == "radialGradient" || defName == "pattern") {
        _colorSourceRefCount[refId]++;
      }
    }
  }

  // Recurse into children.
  auto child = element->getFirstChild();
  while (child) {
    countColorSourceReferencesInElement(child);
    child = child->getNextSibling();
  }
}

std::string SVGParserImpl::generateColorSourceId() {
  std::string id;
  do {
    id = "color" + std::to_string(_nextColorSourceId++);
  } while (_existingIds.count(id) > 0);
  _existingIds.insert(id);
  return id;
}

std::unique_ptr<ColorSource> SVGParserImpl::getColorSourceForRef(const std::string& refId,
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

  // If refCount > 1, we need to create/reuse a resource with an ID.
  if (refCount > 1) {
    // Check if we already created a resource for this ref.
    auto idIt = _colorSourceIdMap.find(refId);
    if (idIt != _colorSourceIdMap.end()) {
      // Return a new ColorSource instance that references the existing resource.
      // The caller should check if colorSource->id is non-empty to know it's a reference.
      // We need to create a "stub" ColorSource with just the id set.
      // Actually, the PAGXExporter uses colorSource->id to determine if it's a reference.
      // So we create a new instance with just the id filled in.
      
      // For proper behavior, we create a copy of the cached ColorSource with the same id.
      auto cacheIt = _colorSourceCache.find(refId);
      if (cacheIt != _colorSourceCache.end()) {
        // Clone the cached ColorSource
        ColorSource* cached = cacheIt->second;
        if (defName == "linearGradient") {
          auto grad = std::make_unique<LinearGradient>();
          auto* src = static_cast<LinearGradient*>(cached);
          grad->id = src->id;
          grad->startPoint = src->startPoint;
          grad->endPoint = src->endPoint;
          grad->matrix = src->matrix;
          grad->colorStops = src->colorStops;
          return grad;
        } else if (defName == "radialGradient") {
          auto grad = std::make_unique<RadialGradient>();
          auto* src = static_cast<RadialGradient*>(cached);
          grad->id = src->id;
          grad->center = src->center;
          grad->radius = src->radius;
          grad->matrix = src->matrix;
          grad->colorStops = src->colorStops;
          return grad;
        } else if (defName == "pattern") {
          auto pattern = std::make_unique<ImagePattern>();
          auto* src = static_cast<ImagePattern*>(cached);
          pattern->id = src->id;
          pattern->image = src->image;
          pattern->tileModeX = src->tileModeX;
          pattern->tileModeY = src->tileModeY;
          pattern->filterMode = src->filterMode;
          pattern->mipmapMode = src->mipmapMode;
          pattern->matrix = src->matrix;
          return pattern;
        }
      }
      return nullptr;
    }

    // First time seeing this multi-referenced def, create the resource.
    std::string colorSourceId = generateColorSourceId();
    _colorSourceIdMap[refId] = colorSourceId;

    std::unique_ptr<ColorSource> colorSource;
    if (defName == "linearGradient") {
      colorSource = convertLinearGradient(defNode, shapeBounds);
    } else if (defName == "radialGradient") {
      colorSource = convertRadialGradient(defNode, shapeBounds);
    } else if (defName == "pattern") {
      colorSource = convertPattern(defNode, shapeBounds);
    }

    if (colorSource) {
      colorSource->id = colorSourceId;
      // Cache the raw pointer before moving to resources.
      _colorSourceCache[refId] = colorSource.get();
      _document->resources.push_back(std::move(colorSource));
      
      // Now return a clone with the same id (as a reference).
      return getColorSourceForRef(refId, shapeBounds);
    }
    return nullptr;
  }

  // refCount <= 1: inline the ColorSource (no id).
  std::unique_ptr<ColorSource> colorSource = nullptr;
  if (defName == "linearGradient") {
    colorSource = convertLinearGradient(defNode, shapeBounds);
  } else if (defName == "radialGradient") {
    colorSource = convertRadialGradient(defNode, shapeBounds);
  } else if (defName == "pattern") {
    colorSource = convertPattern(defNode, shapeBounds);
  }
  if (colorSource) {
    // Clear the id for inline ColorSource to avoid being treated as a reference.
    colorSource->id.clear();
  }
  return colorSource;
}

}  // namespace pagx
