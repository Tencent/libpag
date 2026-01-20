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

#include "pagx/PAGXSVGParser.h"
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include "SVGParserInternal.h"
#include "xml/XMLDOM.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXSVGParser::Parse(const std::string& filePath,
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

std::shared_ptr<PAGXDocument> PAGXSVGParser::Parse(const uint8_t* data, size_t length,
                                                   const Options& options) {
  SVGParserImpl parser(options);
  return parser.parse(data, length);
}

std::shared_ptr<PAGXDocument> PAGXSVGParser::ParseString(const std::string& svgContent,
                                                         const Options& options) {
  return Parse(reinterpret_cast<const uint8_t*>(svgContent.data()), svgContent.size(), options);
}

// ============== SVGParserImpl ==============

SVGParserImpl::SVGParserImpl(const PAGXSVGParser::Options& options) : _options(options) {
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
  auto [found, value] = node->findAttribute(name);
  return found ? value : defaultValue;
}

std::shared_ptr<PAGXDocument> SVGParserImpl::parseDOM(const std::shared_ptr<DOM>& dom) {
  auto root = dom->getRootNode();
  if (!root || root->name != "svg") {
    return nullptr;
  }

  // Parse viewBox and dimensions.
  auto viewBox = parseViewBox(getAttribute(root, "viewBox"));
  float width = parseLength(getAttribute(root, "width"), 0);
  float height = parseLength(getAttribute(root, "height"), 0);

  if (viewBox.size() >= 4) {
    _viewBoxWidth = viewBox[2];
    _viewBoxHeight = viewBox[3];
    if (width == 0) {
      width = _viewBoxWidth;
    }
    if (height == 0) {
      height = _viewBoxHeight;
    }
  } else {
    _viewBoxWidth = width;
    _viewBoxHeight = height;
  }

  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  _document = PAGXDocument::Make(width, height);

  // First pass: collect defs.
  auto child = root->getFirstChild();
  while (child) {
    if (child->name == "defs") {
      parseDefs(child);
    }
    child = child->getNextSibling();
  }

  // Check if we need a viewBox transform.
  bool needsViewBoxTransform = false;
  Matrix viewBoxMatrix = Matrix::Identity();
  if (viewBox.size() >= 4) {
    float viewBoxX = viewBox[0];
    float viewBoxY = viewBox[1];
    float viewBoxW = viewBox[2];
    float viewBoxH = viewBox[3];

    if (viewBoxW > 0 && viewBoxH > 0 &&
        (viewBoxX != 0 || viewBoxY != 0 || viewBoxW != width || viewBoxH != height)) {
      // Calculate uniform scale (meet behavior: fit inside viewport).
      float scaleX = width / viewBoxW;
      float scaleY = height / viewBoxH;
      float scale = std::min(scaleX, scaleY);

      // Calculate translation to center content (xMidYMid).
      float translateX = (width - viewBoxW * scale) / 2.0f - viewBoxX * scale;
      float translateY = (height - viewBoxH * scale) / 2.0f - viewBoxY * scale;

      // Build the transform matrix: scale then translate.
      viewBoxMatrix = Matrix::Translate(translateX, translateY) * Matrix::Scale(scale, scale);
      needsViewBoxTransform = true;
    }
  }

  // Compute initial inherited style from the root <svg> element.
  InheritedStyle rootStyle = {};
  rootStyle = computeInheritedStyle(root, rootStyle);

  // Collect converted layers.
  std::vector<std::unique_ptr<LayerNode>> convertedLayers;
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
    auto rootLayer = std::make_unique<LayerNode>();
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

std::unique_ptr<LayerNode> SVGParserImpl::convertToLayer(const std::shared_ptr<DOMNode>& element,
                                                        const InheritedStyle& parentStyle) {
  const auto& tag = element->name;

  if (tag == "defs" || tag == "linearGradient" || tag == "radialGradient" || tag == "pattern" ||
      tag == "mask" || tag == "clipPath" || tag == "filter") {
    return nullptr;
  }

  // Compute inherited style for this element.
  InheritedStyle inheritedStyle = computeInheritedStyle(element, parentStyle);

  auto layer = std::make_unique<LayerNode>();

  // Parse common layer attributes.
  layer->id = getAttribute(element, "id");
  layer->name = getAttribute(element, "id");

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
        // Add mask layer as invisible layer to the document.
        _maskLayers.push_back(std::move(maskLayer));
      }
    }
  }

  // Handle filter attribute.
  std::string filterAttr = getAttribute(element, "filter");
  if (!filterAttr.empty() && filterAttr != "none") {
    std::string filterId = resolveUrl(filterAttr);
    auto filterIt = _defs.find(filterId);
    if (filterIt != _defs.end()) {
      convertFilterElement(filterIt->second, layer->filters);
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
    convertChildren(element, layer->contents, inheritedStyle);
  }

  return layer;
}

void SVGParserImpl::convertChildren(const std::shared_ptr<DOMNode>& element,
                                    std::vector<std::unique_ptr<VectorElementNode>>& contents,
                                    const InheritedStyle& inheritedStyle) {
  const auto& tag = element->name;

  // Handle text element specially - it returns a GroupNode with TextSpan.
  if (tag == "text") {
    auto textGroup = convertText(element, inheritedStyle);
    if (textGroup) {
      contents.push_back(std::move(textGroup));
    }
    return;
  }

  auto shapeElement = convertElement(element);
  if (shapeElement) {
    contents.push_back(std::move(shapeElement));
  }

  addFillStroke(element, contents, inheritedStyle);
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertElement(
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

std::unique_ptr<GroupNode> SVGParserImpl::convertG(const std::shared_ptr<DOMNode>& element,
                                                   const InheritedStyle& parentStyle) {
  // Compute inherited style for this group element.
  InheritedStyle inheritedStyle = computeInheritedStyle(element, parentStyle);

  auto group = std::make_unique<GroupNode>();

  group->name = getAttribute(element, "id");

  std::string transform = getAttribute(element, "transform");
  if (!transform.empty()) {
    // For GroupNode, we need to decompose the matrix into position/rotation/scale.
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

std::unique_ptr<VectorElementNode> SVGParserImpl::convertRect(
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

  auto rect = std::make_unique<RectangleNode>();
  rect->center.x = x + width / 2;
  rect->center.y = y + height / 2;
  rect->size.width = width;
  rect->size.height = height;
  rect->roundness = std::max(rx, ry);

  return rect;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertCircle(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float r = parseLength(getAttribute(element, "r"), _viewBoxWidth);

  auto ellipse = std::make_unique<EllipseNode>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = r * 2;
  ellipse->size.height = r * 2;

  return ellipse;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertEllipse(
    const std::shared_ptr<DOMNode>& element) {
  float cx = parseLength(getAttribute(element, "cx"), _viewBoxWidth);
  float cy = parseLength(getAttribute(element, "cy"), _viewBoxHeight);
  float rx = parseLength(getAttribute(element, "rx"), _viewBoxWidth);
  float ry = parseLength(getAttribute(element, "ry"), _viewBoxHeight);

  auto ellipse = std::make_unique<EllipseNode>();
  ellipse->center.x = cx;
  ellipse->center.y = cy;
  ellipse->size.width = rx * 2;
  ellipse->size.height = ry * 2;

  return ellipse;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertLine(
    const std::shared_ptr<DOMNode>& element) {
  float x1 = parseLength(getAttribute(element, "x1"), _viewBoxWidth);
  float y1 = parseLength(getAttribute(element, "y1"), _viewBoxHeight);
  float x2 = parseLength(getAttribute(element, "x2"), _viewBoxWidth);
  float y2 = parseLength(getAttribute(element, "y2"), _viewBoxHeight);

  auto path = std::make_unique<PathNode>();
  path->data.moveTo(x1, y1);
  path->data.lineTo(x2, y2);

  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPolyline(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<PathNode>();
  path->data = parsePoints(getAttribute(element, "points"), false);
  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPolygon(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<PathNode>();
  path->data = parsePoints(getAttribute(element, "points"), true);
  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPath(
    const std::shared_ptr<DOMNode>& element) {
  auto path = std::make_unique<PathNode>();
  std::string d = getAttribute(element, "d");
  if (!d.empty()) {
    path->data = PathData::FromSVGString(d);
  }
  return path;
}

std::unique_ptr<GroupNode> SVGParserImpl::convertText(const std::shared_ptr<DOMNode>& element,
                                                      const InheritedStyle& inheritedStyle) {
  auto group = std::make_unique<GroupNode>();

  float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
  float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);

  // Get text content from child text nodes.
  std::string textContent;
  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      textContent += child->name;
    }
    child = child->getNextSibling();
  }

  if (!textContent.empty()) {
    auto textSpan = std::make_unique<TextSpanNode>();
    textSpan->x = x;
    textSpan->y = y;
    textSpan->text = textContent;

    std::string fontFamily = getAttribute(element, "font-family");
    if (!fontFamily.empty()) {
      textSpan->font = fontFamily;
    }

    std::string fontSize = getAttribute(element, "font-size");
    if (!fontSize.empty()) {
      textSpan->fontSize = parseLength(fontSize, _viewBoxHeight);
    }

    group->elements.push_back(std::move(textSpan));
  }

  addFillStroke(element, group->elements, inheritedStyle);
  return group;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertUse(
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

  if (_options.expandUseReferences) {
    auto node = convertElement(it->second);
    if (node) {
      float x = parseLength(getAttribute(element, "x"), _viewBoxWidth);
      float y = parseLength(getAttribute(element, "y"), _viewBoxHeight);
      if (x != 0 || y != 0) {
        // Wrap in a group with translation.
        auto group = std::make_unique<GroupNode>();
        group->position = {x, y};
        group->elements.push_back(std::move(node));
        return group;
      }
    }
    return node;
  }

  // For non-expanded use references, just create an empty group for now.
  auto group = std::make_unique<GroupNode>();
  group->name = "_useRef:" + refId;
  return group;
}

std::unique_ptr<LinearGradientNode> SVGParserImpl::convertLinearGradient(
    const std::shared_ptr<DOMNode>& element) {
  auto gradient = std::make_unique<LinearGradientNode>();

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
    // Apply gradient transform to normalized points.
    Point start = {x1, y1};
    Point end = {x2, y2};
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
      ColorStopNode stop;
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

std::unique_ptr<RadialGradientNode> SVGParserImpl::convertRadialGradient(
    const std::shared_ptr<DOMNode>& element) {
  auto gradient = std::make_unique<RadialGradientNode>();

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

  if (useOBB || !gradientTransform.empty()) {
    // Apply gradientTransform to center point.
    Point center = {cx, cy};
    center = transformMatrix.mapPoint(center);
    gradient->center = center;

    // For radius, we need to account for scaling in the transform.
    // Use the average of X and Y scale factors.
    float scaleX = std::sqrt(transformMatrix.a * transformMatrix.a +
                             transformMatrix.b * transformMatrix.b);
    float scaleY = std::sqrt(transformMatrix.c * transformMatrix.c +
                             transformMatrix.d * transformMatrix.d);
    gradient->radius = r * (scaleX + scaleY) / 2.0f;

    // Store the matrix for non-uniform scaling (rotation, skew, etc.).
    if (!transformMatrix.isIdentity()) {
      gradient->matrix = transformMatrix;
    }
  } else {
    gradient->center.x = cx;
    gradient->center.y = cy;
    gradient->radius = r;
  }

  // Parse stops.
  auto child = element->getFirstChild();
  while (child) {
    if (child->name == "stop") {
      ColorStopNode stop;
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

std::unique_ptr<ImagePatternNode> SVGParserImpl::convertPattern(
    const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds) {
  auto pattern = std::make_unique<ImagePatternNode>();

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

        // Calculate the image's actual size in user space (considering content units and transform).
        float imageSizeInUserSpaceX = 0;
        float imageSizeInUserSpaceY = 0;
        if (contentUnitsOBB) {
          // When patternContentUnits is objectBoundingBox, image dimensions are 0-1 ratios.
          // Apply use transform (e.g., scale(0.005)) to map image to content space,
          // then scale by shape bounds to get user space size.
          imageSizeInUserSpaceX = imageWidth * useMatrix.a * shapeBounds.width;
          imageSizeInUserSpaceY = imageHeight * useMatrix.d * shapeBounds.height;
        } else {
          // When patternContentUnits is userSpaceOnUse, image dimensions are in user space.
          imageSizeInUserSpaceX = imageWidth * useMatrix.a;
          imageSizeInUserSpaceY = imageHeight * useMatrix.d;
        }

        // The ImagePattern shader tiles the original image pixels.
        // We need to scale the image so it renders at the correct size within the tile.
        // Since tgfx ImagePattern uses the image's original pixel dimensions as the base,
        // the matrix should scale the image to match imageSizeInUserSpace.
        // Note: imageWidth here is the SVG display size, which equals original pixel size
        // when the image is embedded at 1:1 scale.
        float scaleX = imageSizeInUserSpaceX / imageWidth;
        float scaleY = imageSizeInUserSpaceY / imageHeight;

        // PAGX ImagePattern coordinates are relative to the geometry's local origin (0,0).
        // SVG pattern with objectBoundingBox is relative to the shape's bounding box.
        // We need to translate the pattern to align with the shape bounds.
        // Matrix multiplication order: translate first, then scale (right to left).
        pattern->matrix = Matrix::Translate(shapeBounds.x, shapeBounds.y) *
                          Matrix::Scale(scaleX, scaleY);
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
                                  std::vector<std::unique_ptr<VectorElementNode>>& contents,
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
      auto fillNode = std::make_unique<FillNode>();
      fillNode->color = "#000000";
      contents.push_back(std::move(fillNode));
    } else if (fill.find("url(") == 0) {
      auto fillNode = std::make_unique<FillNode>();
      std::string refId = resolveUrl(fill);

      // Try to inline the gradient or pattern.
      // Don't set fillNode->color when using colorSource.
      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->name == "linearGradient") {
          fillNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->name == "radialGradient") {
          fillNode->colorSource = convertRadialGradient(it->second);
        } else if (it->second->name == "pattern") {
          fillNode->colorSource = convertPattern(it->second, shapeBounds);
        }
      }
      contents.push_back(std::move(fillNode));
    } else {
      auto fillNode = std::make_unique<FillNode>();
      Color color = parseColor(fill);

      // Determine effective fill-opacity.
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (fillOpacity.empty()) {
        fillOpacity = inheritedStyle.fillOpacity;
      }
      if (!fillOpacity.empty()) {
        color.alpha = std::stof(fillOpacity);
      }
      fillNode->color = color.toHexString(color.alpha < 1);

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
    auto strokeNode = std::make_unique<StrokeNode>();

    if (stroke.find("url(") == 0) {
      std::string refId = resolveUrl(stroke);

      // Don't set strokeNode->color when using colorSource.
      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->name == "linearGradient") {
          strokeNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->name == "radialGradient") {
          strokeNode->colorSource = convertRadialGradient(it->second);
        } else if (it->second->name == "pattern") {
          strokeNode->colorSource = convertPattern(it->second, shapeBounds);
        }
      }
    } else {
      Color color = parseColor(stroke);

      // Determine effective stroke-opacity.
      std::string strokeOpacity = getAttribute(element, "stroke-opacity");
      if (strokeOpacity.empty()) {
        strokeOpacity = inheritedStyle.strokeOpacity;
      }
      if (!strokeOpacity.empty()) {
        color.alpha = std::stof(strokeOpacity);
      }
      strokeNode->color = color.toHexString(color.alpha < 1);
    }

    std::string strokeWidth = getAttribute(element, "stroke-width");
    if (!strokeWidth.empty()) {
      strokeNode->strokeWidth = parseLength(strokeWidth, _viewBoxWidth);
    }

    std::string strokeLinecap = getAttribute(element, "stroke-linecap");
    if (!strokeLinecap.empty()) {
      strokeNode->cap = LineCapFromString(strokeLinecap);
    }

    std::string strokeLinejoin = getAttribute(element, "stroke-linejoin");
    if (!strokeLinejoin.empty()) {
      strokeNode->join = LineJoinFromString(strokeLinejoin);
    }

    std::string strokeMiterlimit = getAttribute(element, "stroke-miterlimit");
    if (!strokeMiterlimit.empty()) {
      strokeNode->miterLimit = std::stof(strokeMiterlimit);
    }

    std::string dashArray = getAttribute(element, "stroke-dasharray");
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
      auto pathData = PathData::FromSVGString(d);
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
    return {0, 0, 0, 0};
  }

  if (value[0] == '#') {
    uint32_t hex = 0;
    if (value.length() == 4) {
      // #RGB -> #RRGGBB
      char r = value[1];
      char g = value[2];
      char b = value[3];
      std::string expanded = std::string() + r + r + g + g + b + b;
      hex = std::stoul(expanded, nullptr, 16);
      return Color::FromHex(hex);
    } else if (value.length() == 7) {
      hex = std::stoul(value.substr(1), nullptr, 16);
      return Color::FromHex(hex);
    } else if (value.length() == 9) {
      hex = std::stoul(value.substr(1), nullptr, 16);
      return Color::FromHex(hex, true);
    }
  }

  if (value.find("rgb") == 0) {
    size_t start = value.find('(');
    size_t end = value.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      std::string inner = value.substr(start + 1, end - start - 1);
      std::istringstream iss(inner);
      float r = 0, g = 0, b = 0, a = 1.0f;
      char comma = 0;
      iss >> r >> comma >> g >> comma >> b;
      if (value.find("rgba") == 0) {
        iss >> comma >> a;
      }
      return Color::FromRGBA(r / 255.0f, g / 255.0f, b / 255.0f, a);
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
    auto color = Color::FromHex(it->second);
    if (value == "transparent") {
      color.alpha = 0;
    }
    return color;
  }

  return {0, 0, 0, 1};
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
  std::vector<float> result;
  if (value.empty()) {
    return result;
  }

  std::istringstream iss(value);
  float num = 0;
  while (iss >> num) {
    result.push_back(num);
  }

  return result;
}

PathData SVGParserImpl::parsePoints(const std::string& value, bool closed) {
  PathData path;
  if (value.empty()) {
    return path;
  }

  std::vector<float> points;
  std::istringstream iss(value);
  float num = 0;
  while (iss >> num) {
    points.push_back(num);
    char c = 0;
    if (iss.peek() == ',' || iss.peek() == ' ') {
      iss >> c;
    }
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

  // Generate a new image ID.
  std::string imageId = "image" + std::to_string(_nextImageId++);

  // Create and add the image resource to the document.
  auto imageNode = std::make_unique<ImageNode>();
  imageNode->id = imageId;
  imageNode->source = imageSource;
  _document->resources.push_back(std::move(imageNode));

  // Cache the mapping.
  _imageSourceToId[imageSource] = imageId;

  return imageId;
}

// Helper function to check if two VectorElement nodes are the same geometry.
static bool isSameGeometry(const VectorElementNode* a, const VectorElementNode* b) {
  if (!a || !b || a->type() != b->type()) {
    return false;
  }

  switch (a->type()) {
    case NodeType::Rectangle: {
      auto rectA = static_cast<const RectangleNode*>(a);
      auto rectB = static_cast<const RectangleNode*>(b);
      return rectA->center.x == rectB->center.x && rectA->center.y == rectB->center.y &&
             rectA->size.width == rectB->size.width && rectA->size.height == rectB->size.height &&
             rectA->roundness == rectB->roundness;
    }
    case NodeType::Ellipse: {
      auto ellipseA = static_cast<const EllipseNode*>(a);
      auto ellipseB = static_cast<const EllipseNode*>(b);
      return ellipseA->center.x == ellipseB->center.x && ellipseA->center.y == ellipseB->center.y &&
             ellipseA->size.width == ellipseB->size.width &&
             ellipseA->size.height == ellipseB->size.height;
    }
    case NodeType::Path: {
      auto pathA = static_cast<const PathNode*>(a);
      auto pathB = static_cast<const PathNode*>(b);
      return pathA->data.toSVGString() == pathB->data.toSVGString();
    }
    default:
      return false;
  }
}

// Check if a layer is a simple shape layer (contains exactly one geometry and one Fill or Stroke).
static bool isSimpleShapeLayer(const LayerNode* layer, const VectorElementNode*& outGeometry,
                               const VectorElementNode*& outPainter) {
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
  bool firstIsGeometry = (first->type() == NodeType::Rectangle ||
                          first->type() == NodeType::Ellipse || first->type() == NodeType::Path);
  bool secondIsPainter =
      (second->type() == NodeType::Fill || second->type() == NodeType::Stroke);

  if (firstIsGeometry && secondIsPainter) {
    outGeometry = first;
    outPainter = second;
    return true;
  }
  return false;
}

void SVGParserImpl::mergeAdjacentLayers(std::vector<std::unique_ptr<LayerNode>>& layers) {
  if (layers.size() < 2) {
    return;
  }

  std::vector<std::unique_ptr<LayerNode>> merged;
  size_t i = 0;

  while (i < layers.size()) {
    const VectorElementNode* geomA = nullptr;
    const VectorElementNode* painterA = nullptr;

    if (isSimpleShapeLayer(layers[i].get(), geomA, painterA)) {
      // Check if the next layer has the same geometry.
      if (i + 1 < layers.size()) {
        const VectorElementNode* geomB = nullptr;
        const VectorElementNode* painterB = nullptr;

        if (isSimpleShapeLayer(layers[i + 1].get(), geomB, painterB) &&
            isSameGeometry(geomA, geomB)) {
          // Merge: one has Fill, the other has Stroke.
          bool aHasFill = (painterA->type() == NodeType::Fill);
          bool bHasFill = (painterB->type() == NodeType::Fill);

          if (aHasFill != bHasFill) {
            // Create merged layer.
            auto mergedLayer = std::make_unique<LayerNode>();

            // Keep geometry from first layer.
            auto geomClone = layers[i]->contents[0]->clone();
            mergedLayer->contents.push_back(
                std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(geomClone.release())));

            // Add Fill first, then Stroke (standard order).
            if (aHasFill) {
              auto fillClone = layers[i]->contents[1]->clone();
              mergedLayer->contents.push_back(
                  std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(fillClone.release())));
              auto strokeClone = layers[i + 1]->contents[1]->clone();
              mergedLayer->contents.push_back(
                  std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(strokeClone.release())));
            } else {
              auto fillClone = layers[i + 1]->contents[1]->clone();
              mergedLayer->contents.push_back(
                  std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(fillClone.release())));
              auto strokeClone = layers[i]->contents[1]->clone();
              mergedLayer->contents.push_back(
                  std::unique_ptr<VectorElementNode>(static_cast<VectorElementNode*>(strokeClone.release())));
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

std::unique_ptr<LayerNode> SVGParserImpl::convertMaskElement(
    const std::shared_ptr<DOMNode>& maskElement, const InheritedStyle& parentStyle) {
  auto maskLayer = std::make_unique<LayerNode>();
  maskLayer->id = getAttribute(maskElement, "id");
  maskLayer->name = maskLayer->id;
  maskLayer->visible = false;

  // Parse mask contents.
  auto child = maskElement->getFirstChild();
  while (child) {
    if (child->name == "rect" || child->name == "circle" || child->name == "ellipse" ||
        child->name == "path" || child->name == "polygon" || child->name == "polyline") {
      InheritedStyle inheritedStyle = computeInheritedStyle(child, parentStyle);
      convertChildren(child, maskLayer->contents, inheritedStyle);
    } else if (child->name == "g") {
      // Handle group inside mask.
      InheritedStyle inheritedStyle = computeInheritedStyle(child, parentStyle);
      auto groupChild = child->getFirstChild();
      while (groupChild) {
        convertChildren(groupChild, maskLayer->contents, inheritedStyle);
        groupChild = groupChild->getNextSibling();
      }
    }
    child = child->getNextSibling();
  }

  return maskLayer;
}

void SVGParserImpl::convertFilterElement(
    const std::shared_ptr<DOMNode>& filterElement,
    std::vector<std::unique_ptr<LayerFilterNode>>& filters) {
  // Parse filter children to find effect elements.
  auto child = filterElement->getFirstChild();
  while (child) {
    if (child->name == "feGaussianBlur") {
      auto blurFilter = std::make_unique<BlurFilterNode>();
      std::string stdDeviation = getAttribute(child, "stdDeviation", "0");
      // stdDeviation can be one value (both X and Y) or two values (X Y).
      std::istringstream iss(stdDeviation);
      float devX = 0, devY = 0;
      iss >> devX;
      if (!(iss >> devY)) {
        devY = devX;
      }
      blurFilter->blurrinessX = devX;
      blurFilter->blurrinessY = devY;
      filters.push_back(std::move(blurFilter));
    }
    child = child->getNextSibling();
  }
}

}  // namespace pagx
