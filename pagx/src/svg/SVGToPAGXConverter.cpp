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

#include "SVGToPAGXConverter.h"
#include <cmath>
#include <iomanip>
#include "tgfx/svg/SVGPathParser.h"
#include "tgfx/svg/node/SVGImage.h"
#include "tgfx/svg/node/SVGLinearGradient.h"
#include "tgfx/svg/node/SVGPattern.h"
#include "tgfx/svg/node/SVGRadialGradient.h"
#include "tgfx/svg/node/SVGStop.h"
#include "tgfx/svg/node/SVGUse.h"

namespace pagx {

// Helper to get value from SVGProperty
template <typename T>
static std::optional<T> GetPropertyValue(const SVGProperty<T, true>& prop) {
  return prop.get();
}

template <typename T>
static std::optional<T> GetPropertyValue(const SVGProperty<T, false>& prop) {
  return prop.get();
}

static std::string CleanFontFamily(const std::string& family) {
  std::string result = family;
  if (result.size() >= 2) {
    if ((result.front() == '"' && result.back() == '"') ||
        (result.front() == '\'' && result.back() == '\'')) {
      result = result.substr(1, result.size() - 2);
    }
  }
  return result;
}

SVGToPAGXConverter::SVGToPAGXConverter(const std::shared_ptr<SVGDOM>& svgDOM) : _svgDOM(svgDOM) {
}

std::string SVGToPAGXConverter::convert() {
  if (!_svgDOM || !_svgDOM->getRoot()) {
    return "";
  }

  auto root = _svgDOM->getRoot();
  auto containerSize = _svgDOM->getContainerSize();
  _width = containerSize.width;
  _height = containerSize.height;

  if (_width <= 0 || _height <= 0) {
    auto viewBox = root->getViewBox();
    if (viewBox.has_value()) {
      _width = viewBox->width();
      _height = viewBox->height();
    }
  }

  if (_width <= 0) {
    _width = 100;
  }
  if (_height <= 0) {
    _height = 100;
  }

  collectGradients();
  collectPatterns();
  collectMasks();
  collectUsedMasks(root.get());
  countColorSourceUsages();

  writeHeader();
  writeResources();
  writeMaskLayers(1);
  writeLayers();

  _output << "</pagx>\n";
  return _output.str();
}

void SVGToPAGXConverter::collectUsedMasks(const SVGNode* node) {
  if (!node) {
    return;
  }

  auto maskId = getMaskId(node);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  // Recursively check children if node is a container
  if (node->hasChildren()) {
    auto container = static_cast<const SVGContainer*>(node);
    for (const auto& child : container->getChildren()) {
      collectUsedMasks(child.get());
    }
  }
}

void SVGToPAGXConverter::writeHeader() {
  _output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  _output << "<pagx version=\"1.0\" width=\"" << static_cast<int>(_width) << "\" height=\""
          << static_cast<int>(_height) << "\">\n";
}

void SVGToPAGXConverter::collectGradients() {
  // Collect gradients from nodeIDMapper directly instead of traversing the whole tree
  for (const auto& [id, svgNode] : _svgDOM->nodeIDMapper()) {
    auto tag = svgNode->tag();
    if (tag == SVGTag::LinearGradient || tag == SVGTag::RadialGradient) {
      _gradients[id] = svgNode.get();
    }
  }
}

void SVGToPAGXConverter::collectPatterns() {
  // Collect patterns from nodeIDMapper
  for (const auto& [id, svgNode] : _svgDOM->nodeIDMapper()) {
    if (svgNode->tag() != SVGTag::Pattern) {
      continue;
    }

    auto pattern = static_cast<const SVGPattern*>(svgNode.get());
    auto container = static_cast<const SVGContainer*>(pattern);

    // Check if pattern uses objectBoundingBox units
    bool isObjectBoundingBox =
        pattern->getPatternUnits().type() == SVGObjectBoundingBoxUnits::Type::ObjectBoundingBox;

    // Pattern may contain <use> elements that reference <image> elements
    for (const auto& child : container->getChildren()) {
      if (child->tag() != SVGTag::Use) {
        continue;
      }

      auto use = static_cast<const SVGUse*>(child.get());
      auto href = use->getHref();
      auto imageId = href.iri();
      if (imageId.empty()) {
        continue;
      }

      // Find the referenced image
      auto imageIt = _svgDOM->nodeIDMapper().find(imageId);
      if (imageIt == _svgDOM->nodeIDMapper().end() || imageIt->second->tag() != SVGTag::Image) {
        continue;
      }

      auto image = static_cast<const SVGImage*>(imageIt->second.get());
      auto imageHref = image->getHref();
      auto imageData = imageHref.iri();

      // Store image if not already stored
      if (_images.find(imageId) == _images.end()) {
        _images[imageId] = imageData;
      }

      // Get image dimensions
      float imageWidth = image->getWidth().value();
      float imageHeight = image->getHeight().value();

      // Get pattern dimensions
      auto patternWidth = pattern->getWidth();
      auto patternHeight = pattern->getHeight();

      PatternInfo patternInfo = {};
      patternInfo.patternId = id;
      patternInfo.imageId = imageId;
      patternInfo.imageData = imageData;
      patternInfo.imageWidth = imageWidth;
      patternInfo.imageHeight = imageHeight;
      patternInfo.isObjectBoundingBox = isObjectBoundingBox;

      // Store pattern dimensions
      if (patternWidth.has_value() && patternHeight.has_value()) {
        patternInfo.patternWidth = patternWidth->value();
        patternInfo.patternHeight = patternHeight->value();
      }

      _patterns[id] = patternInfo;
    }
  }
}

void SVGToPAGXConverter::collectMasks() {
  // Collect masks from nodeIDMapper
  for (const auto& [id, svgNode] : _svgDOM->nodeIDMapper()) {
    if (svgNode->tag() != SVGTag::Mask) {
      continue;
    }

    auto mask = static_cast<const SVGMask*>(svgNode.get());
    MaskInfo maskInfo = {};
    maskInfo.maskId = id;
    maskInfo.maskNode = mask;
    maskInfo.isLuminance = mask->getMaskType().type() == SVGMaskType::Type::Luminance;

    _masks[id] = maskInfo;
  }
}

void SVGToPAGXConverter::countColorSourceUsages() {
  auto root = _svgDOM->getRoot();
  for (const auto& child : root->getChildren()) {
    countColorSourceUsagesFromNode(child.get());
  }
}

void SVGToPAGXConverter::countColorSourceUsagesFromNode(const SVGNode* node) {
  if (!node) {
    return;
  }

  auto tag = node->tag();

  // Skip non-renderable elements
  if (tag == SVGTag::Defs || tag == SVGTag::LinearGradient || tag == SVGTag::RadialGradient ||
      tag == SVGTag::Stop || tag == SVGTag::ClipPath || tag == SVGTag::Mask ||
      tag == SVGTag::Filter || tag == SVGTag::Pattern) {
    return;
  }

  // Check fill attribute for color source references
  auto fillProp = node->getFill();
  auto fillOpt = GetPropertyValue(fillProp);
  if (fillOpt.has_value() && fillOpt->type() == SVGPaint::Type::IRI) {
    auto iri = fillOpt->iri().iri();
    auto& usage = _colorSourceUsages[iri];
    usage.count++;
    if (_gradients.find(iri) != _gradients.end()) {
      usage.type = "gradient";
    } else if (_patterns.find(iri) != _patterns.end()) {
      usage.type = "pattern";
    }
  }

  // Check stroke attribute for color source references
  auto strokeProp = node->getStroke();
  auto strokeOpt = GetPropertyValue(strokeProp);
  if (strokeOpt.has_value() && strokeOpt->type() == SVGPaint::Type::IRI) {
    auto iri = strokeOpt->iri().iri();
    auto& usage = _colorSourceUsages[iri];
    usage.count++;
    if (_gradients.find(iri) != _gradients.end()) {
      usage.type = "gradient";
    } else if (_patterns.find(iri) != _patterns.end()) {
      usage.type = "pattern";
    }
  }

  // Recurse into containers
  if (tag == SVGTag::G || tag == SVGTag::Svg) {
    auto container = static_cast<const SVGContainer*>(node);
    for (const auto& child : container->getChildren()) {
      countColorSourceUsagesFromNode(child.get());
    }
  }
}

void SVGToPAGXConverter::writeResources() {
  // Collect resources that need to be in Resources section (referenced more than once)
  std::vector<std::string> sharedGradients = {};
  std::vector<std::string> sharedPatterns = {};

  for (const auto& [id, usage] : _colorSourceUsages) {
    if (usage.count > 1) {
      if (usage.type == "gradient") {
        sharedGradients.push_back(id);
      } else if (usage.type == "pattern") {
        // For patterns with objectBoundingBox, always inline since they depend on shape size
        auto patternIt = _patterns.find(id);
        if (patternIt != _patterns.end() && !patternIt->second.isObjectBoundingBox) {
          sharedPatterns.push_back(id);
        }
      }
    }
  }

  // Check if we have any shared resources or images
  bool hasSharedResources = !sharedGradients.empty() || !sharedPatterns.empty() || !_images.empty();
  if (!hasSharedResources) {
    return;
  }

  _output << "\n";
  indent(1);
  _output << "<Resources>\n";

  // Output shared gradients
  for (const auto& id : sharedGradients) {
    auto it = _gradients.find(id);
    if (it == _gradients.end()) {
      continue;
    }
    auto node = it->second;
    auto tag = node->tag();
    if (tag == SVGTag::LinearGradient) {
      auto gradient = static_cast<const SVGLinearGradient*>(node);
      indent(2);
      _output << "<LinearGradient id=\"" << id << "\"";
      _output << " startX=\"" << lengthToFloat(gradient->getX1(), _width) << "\"";
      _output << " startY=\"" << lengthToFloat(gradient->getY1(), _height) << "\"";
      _output << " endX=\"" << lengthToFloat(gradient->getX2(), _width) << "\"";
      _output << " endY=\"" << lengthToFloat(gradient->getY2(), _height) << "\"";
      _output << ">\n";

      auto container = static_cast<const SVGContainer*>(node);
      for (const auto& child : container->getChildren()) {
        if (child->tag() == SVGTag::Stop) {
          auto stop = static_cast<const SVGStop*>(child.get());
          indent(3);
          auto offset = stop->getOffset();
          float offsetValue = offset.value();
          if (offset.unit() == SVGLength::Unit::Percentage) {
            offsetValue = offsetValue / 100.0f;
          }
          _output << "<ColorStop offset=\"" << offsetValue << "\"";
          auto stopColorProp = stop->getStopColor();
          auto stopColorOpt = GetPropertyValue(stopColorProp);
          if (stopColorOpt.has_value()) {
            auto color = stopColorOpt->color();
            auto opacityProp = stop->getStopOpacity();
            auto opacityOpt = GetPropertyValue(opacityProp);
            float alpha = opacityOpt.has_value() ? opacityOpt.value() : 1.0f;
            Color c = color;
            c.alpha = alpha;
            _output << " color=\"" << colorToHex(c) << "\"";
          }
          _output << "/>\n";
        }
      }

      indent(2);
      _output << "</LinearGradient>\n";
    } else if (tag == SVGTag::RadialGradient) {
      auto gradient = static_cast<const SVGRadialGradient*>(node);
      indent(2);
      _output << "<RadialGradient id=\"" << id << "\"";
      _output << " centerX=\"" << lengthToFloat(gradient->getCx(), _width) << "\"";
      _output << " centerY=\"" << lengthToFloat(gradient->getCy(), _height) << "\"";
      auto r = gradient->getR();
      _output << " radius=\"" << lengthToFloat(r, std::max(_width, _height)) << "\"";
      _output << ">\n";

      auto container = static_cast<const SVGContainer*>(node);
      for (const auto& child : container->getChildren()) {
        if (child->tag() == SVGTag::Stop) {
          auto stop = static_cast<const SVGStop*>(child.get());
          indent(3);
          auto offset = stop->getOffset();
          float offsetValue = offset.value();
          if (offset.unit() == SVGLength::Unit::Percentage) {
            offsetValue = offsetValue / 100.0f;
          }
          _output << "<ColorStop offset=\"" << offsetValue << "\"";
          auto stopColorProp = stop->getStopColor();
          auto stopColorOpt = GetPropertyValue(stopColorProp);
          if (stopColorOpt.has_value()) {
            auto color = stopColorOpt->color();
            auto opacityProp = stop->getStopOpacity();
            auto opacityOpt = GetPropertyValue(opacityProp);
            float alpha = opacityOpt.has_value() ? opacityOpt.value() : 1.0f;
            Color c = color;
            c.alpha = alpha;
            _output << " color=\"" << colorToHex(c) << "\"";
          }
          _output << "/>\n";
        }
      }

      indent(2);
      _output << "</RadialGradient>\n";
    }
  }

  // Output Image resources (always needed for patterns)
  for (const auto& [imageId, imageData] : _images) {
    indent(2);
    _output << "<Image id=\"" << imageId << "\" source=\"" << imageData << "\"/>\n";
  }

  // Output shared ImagePattern resources (non-objectBoundingBox patterns referenced multiple times)
  for (const auto& patternId : sharedPatterns) {
    auto it = _patterns.find(patternId);
    if (it == _patterns.end()) {
      continue;
    }
    const auto& patternInfo = it->second;
    indent(2);
    _output << "<ImagePattern id=\"" << patternId << "\" image=\"#" << patternInfo.imageId
            << "\" tileModeX=\"repeat\" tileModeY=\"repeat\"/>\n";
  }

  indent(1);
  _output << "</Resources>\n";
}

void SVGToPAGXConverter::writeLayers() {
  auto root = _svgDOM->getRoot();
  _output << "\n";

  convertChildren(root->getChildren(), 1);
}

void SVGToPAGXConverter::convertNode(const SVGNode* node, int depth, bool /*needScopeIsolation*/) {
  if (!node) {
    return;
  }

  auto tag = node->tag();

  if (tag == SVGTag::Defs || tag == SVGTag::LinearGradient || tag == SVGTag::RadialGradient ||
      tag == SVGTag::Stop || tag == SVGTag::ClipPath || tag == SVGTag::Mask ||
      tag == SVGTag::Filter) {
    return;
  }

  auto visibilityProp = node->getVisibility();
  auto visibilityOpt = GetPropertyValue(visibilityProp);
  if (visibilityOpt.has_value() && visibilityOpt.value().type() == SVGVisibility::Type::Hidden) {
    return;
  }

  switch (tag) {
    case SVGTag::G:
    case SVGTag::Svg:
      convertContainer(static_cast<const SVGContainer*>(node), depth);
      break;
    case SVGTag::Rect:
      convertRect(static_cast<const SVGRect*>(node), depth);
      break;
    case SVGTag::Circle:
      convertCircle(static_cast<const SVGCircle*>(node), depth);
      break;
    case SVGTag::Ellipse:
      convertEllipse(static_cast<const SVGEllipse*>(node), depth);
      break;
    case SVGTag::Path:
      convertPath(static_cast<const SVGPath*>(node), depth);
      break;
    case SVGTag::Polygon:
    case SVGTag::Polyline:
      convertPoly(static_cast<const SVGPoly*>(node), depth);
      break;
    case SVGTag::Line:
      convertLine(static_cast<const SVGLine*>(node), depth);
      break;
    case SVGTag::Text:
      convertText(static_cast<const SVGText*>(node), depth);
      break;
    default:
      break;
  }
}

void SVGToPAGXConverter::convertContainer(const SVGContainer* container, int depth) {
  if (!container || !container->hasChildren()) {
    return;
  }

  int renderableCount = countRenderableChildren(container);

  // If container has only one renderable child, don't create a wrapper Layer for the container
  // Just output the child directly
  if (renderableCount == 1) {
    convertChildren(container->getChildren(), depth);
    return;
  }

  // Container with multiple children needs a wrapper Layer
  indent(depth);
  _output << "<Layer";

  auto transformable = static_cast<const SVGTransformableNode*>(container);
  auto transform = transformable->getTransform();
  if (!transform.isIdentity()) {
    _output << " matrix=\"" << matrixToString(transform) << "\"";
  }

  auto opacityProp = container->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  if (opacityOpt.has_value() && opacityOpt.value() < 1.0f) {
    _output << " alpha=\"" << opacityOpt.value() << "\"";
  }

  _output << ">\n";

  convertChildren(container->getChildren(), depth + 1);

  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertRect(const SVGRect* rect, int depth) {
  if (!rect) {
    return;
  }

  float x = std::stof(lengthToFloat(rect->getX(), _width));
  float y = std::stof(lengthToFloat(rect->getY(), _height));
  float width = std::stof(lengthToFloat(rect->getWidth(), _width));
  float height = std::stof(lengthToFloat(rect->getHeight(), _height));

  if (width <= 0 || height <= 0) {
    return;
  }

  float centerX = x + width / 2.0f;
  float centerY = y + height / 2.0f;

  float rx = 0;
  float ry = 0;
  auto rxOpt = rect->getRx();
  auto ryOpt = rect->getRy();
  if (rxOpt.has_value()) {
    rx = std::stof(lengthToFloat(*rxOpt, _width));
  }
  if (ryOpt.has_value()) {
    ry = std::stof(lengthToFloat(*ryOpt, _height));
  }
  if (rx > 0 && ry <= 0) {
    ry = rx;
  }
  if (ry > 0 && rx <= 0) {
    rx = ry;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(rect);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = rect->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  // Check for mask
  std::string maskId = getMaskId(rect);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  // Each shape element always gets its own Layer
  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  // Group is needed when there's a transform or opacity to apply
  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Rectangle centerX=\"" << centerX << "\" centerY=\"" << centerY << "\" width=\""
          << width << "\" height=\"" << height << "\"";
  if (rx > 0 || ry > 0) {
    float roundness = std::min(rx, ry);
    _output << " roundness=\"" << roundness << "\"";
  }
  _output << "/>\n";

  writeFillStyle(rect, contentDepth, x, y, width, height);
  writeStrokeStyle(rect, contentDepth, x, y, width, height);

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertCircle(const SVGCircle* circle, int depth) {
  if (!circle) {
    return;
  }

  float cx = std::stof(lengthToFloat(circle->getCx(), _width));
  float cy = std::stof(lengthToFloat(circle->getCy(), _height));
  float r = std::stof(lengthToFloat(circle->getR(), std::max(_width, _height)));

  if (r <= 0) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(circle);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = circle->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  std::string maskId = getMaskId(circle);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Ellipse centerX=\"" << cx << "\" centerY=\"" << cy << "\" width=\"" << r * 2
          << "\" height=\"" << r * 2 << "\"/>\n";

  writeFillStyle(circle, contentDepth, cx - r, cy - r, r * 2, r * 2);
  writeStrokeStyle(circle, contentDepth, cx - r, cy - r, r * 2, r * 2);

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertEllipse(const SVGEllipse* ellipse, int depth) {
  if (!ellipse) {
    return;
  }

  float cx = std::stof(lengthToFloat(ellipse->getCx(), _width));
  float cy = std::stof(lengthToFloat(ellipse->getCy(), _height));

  float rx = 0;
  float ry = 0;
  auto rxOpt = ellipse->getRx();
  auto ryOpt = ellipse->getRy();
  if (rxOpt.has_value()) {
    rx = std::stof(lengthToFloat(*rxOpt, _width));
  }
  if (ryOpt.has_value()) {
    ry = std::stof(lengthToFloat(*ryOpt, _height));
  }

  if (rx <= 0 || ry <= 0) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(ellipse);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = ellipse->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  std::string maskId = getMaskId(ellipse);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Ellipse centerX=\"" << cx << "\" centerY=\"" << cy << "\" width=\"" << rx * 2
          << "\" height=\"" << ry * 2 << "\"/>\n";

  writeFillStyle(ellipse, contentDepth, cx - rx, cy - ry, rx * 2, ry * 2);
  writeStrokeStyle(ellipse, contentDepth, cx - rx, cy - ry, rx * 2, ry * 2);

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertPath(const SVGPath* path, int depth) {
  if (!path) {
    return;
  }

  auto shapePath = path->getShapePath();
  if (shapePath.isEmpty()) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(path);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = path->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  std::string maskId = getMaskId(path);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Path d=\"" << SVGPathParser::ToSVGString(shapePath) << "\"/>\n";

  auto pathBounds = shapePath.getBounds();
  writeFillStyle(path, contentDepth, pathBounds.left, pathBounds.top, pathBounds.width(),
                 pathBounds.height());
  writeStrokeStyle(path, contentDepth, pathBounds.left, pathBounds.top, pathBounds.width(),
                   pathBounds.height());

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertLine(const SVGLine* line, int depth) {
  if (!line) {
    return;
  }

  auto x1 = line->getX1().value();
  auto y1 = line->getY1().value();
  auto x2 = line->getX2().value();
  auto y2 = line->getY2().value();

  Path linePath;
  linePath.moveTo(x1, y1);
  linePath.lineTo(x2, y2);

  if (linePath.isEmpty()) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(line);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = line->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  std::string maskId = getMaskId(line);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Path d=\"" << SVGPathParser::ToSVGString(linePath) << "\"/>\n";

  auto lineBounds = linePath.getBounds();
  writeFillStyle(line, contentDepth, lineBounds.left, lineBounds.top, lineBounds.width(),
                 lineBounds.height());
  writeStrokeStyle(line, contentDepth, lineBounds.left, lineBounds.top, lineBounds.width(),
                   lineBounds.height());

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertPoly(const SVGPoly* poly, int depth) {
  if (!poly) {
    return;
  }

  const auto& points = poly->getPoints();
  if (points.empty()) {
    return;
  }

  Path polyPath;
  polyPath.moveTo(points[0]);
  for (size_t i = 1; i < points.size(); ++i) {
    polyPath.lineTo(points[i]);
  }
  if (poly->tag() == SVGTag::Polygon) {
    polyPath.close();
  }

  if (polyPath.isEmpty()) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(poly);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = poly->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  std::string maskId = getMaskId(poly);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Path d=\"" << SVGPathParser::ToSVGString(polyPath) << "\"/>\n";

  auto polyBounds = polyPath.getBounds();
  writeFillStyle(poly, contentDepth, polyBounds.left, polyBounds.top, polyBounds.width(),
                 polyBounds.height());
  writeStrokeStyle(poly, contentDepth, polyBounds.left, polyBounds.top, polyBounds.width(),
                   polyBounds.height());

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::convertText(const SVGText* text, int depth) {
  if (!text) {
    return;
  }

  const auto& textChildren = text->getTextChildren();
  if (textChildren.empty()) {
    return;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(text);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = text->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  // Get text-level x, y position
  auto xList = text->getX();
  auto yList = text->getY();
  float baseX = xList.empty() ? 0.0f : std::stof(lengthToFloat(xList[0], _width));
  float baseY = yList.empty() ? 0.0f : std::stof(lengthToFloat(yList[0], _height));

  // Get font properties from text element
  auto fontFamilyProp = text->getFontFamily();
  auto fontFamilyOpt = GetPropertyValue(fontFamilyProp);
  std::string fontFamily =
      fontFamilyOpt.has_value() ? CleanFontFamily(fontFamilyOpt->family()) : "";

  auto fontSizeProp = text->getFontSize();
  auto fontSizeOpt = GetPropertyValue(fontSizeProp);
  float fontSize = fontSizeOpt.has_value() ? std::stof(lengthToFloat(fontSizeOpt->size(), _height))
                                           : 12.0f;

  // Get text-anchor property
  auto textAnchorProp = text->getTextAnchor();
  auto textAnchorOpt = GetPropertyValue(textAnchorProp);
  std::string textAnchor = "start";
  if (textAnchorOpt.has_value()) {
    switch (textAnchorOpt->type()) {
      case SVGTextAnchor::Type::Middle:
        textAnchor = "middle";
        break;
      case SVGTextAnchor::Type::End:
        textAnchor = "end";
        break;
      default:
        textAnchor = "start";
        break;
    }
  }

  std::string maskId = getMaskId(text);
  if (!maskId.empty()) {
    _usedMasks.insert(maskId);
  }

  indent(depth);
  _output << "<Layer";
  if (hasTransform) {
    _output << " matrix=\"" << matrixToString(transform) << "\"";
  }
  if (hasOpacity) {
    _output << " alpha=\"" << opacityOpt.value() << "\"";
  }
  if (!maskId.empty()) {
    _output << " mask=\"#mask_" << maskId << "\"";
    auto it = _masks.find(maskId);
    if (it != _masks.end() && it->second.isLuminance) {
      _output << " maskType=\"luminance\"";
    }
  }
  _output << ">\n";

  indent(depth + 1);
  _output << "<contents>\n";

  // Process each text child (TextLiteral or TSpan)
  for (const auto& child : textChildren) {
    auto tag = child->tag();
    if (tag == SVGTag::TextLiteral) {
      auto literal = static_cast<const SVGTextLiteral*>(child.get());
      const std::string& textContent = literal->getText();
      if (textContent.empty()) {
        continue;
      }

      indent(depth + 2);
      _output << "<Group>\n";

      indent(depth + 3);
      _output << "<TextSpan x=\"" << baseX << "\" y=\"" << baseY << "\"";
      if (!fontFamily.empty()) {
        _output << " font=\"" << fontFamily << "\"";
      }
      _output << " fontSize=\"" << fontSize << "\"";
      if (textAnchor != "start") {
        _output << " textAnchor=\"" << textAnchor << "\"";
      }
      _output << "><![CDATA[" << textContent << "]]></TextSpan>\n";

      writeFillStyle(text, depth + 3, 0, 0, 0, 0);
      writeStrokeStyle(text, depth + 3, 0, 0, 0, 0);

      indent(depth + 2);
      _output << "</Group>\n";
    } else if (tag == SVGTag::TSpan) {
      auto tspan = static_cast<const SVGTextContainer*>(child.get());
      const auto& tspanChildren = tspan->getTextChildren();

      // Get tspan-specific position (if specified)
      auto tspanX = tspan->getX();
      auto tspanY = tspan->getY();
      float spanX = tspanX.empty() ? baseX : std::stof(lengthToFloat(tspanX[0], _width));
      float spanY = tspanY.empty() ? baseY : std::stof(lengthToFloat(tspanY[0], _height));

      // Get tspan-specific font properties (inherit from text if not specified)
      auto tspanFontFamilyProp = tspan->getFontFamily();
      auto tspanFontFamilyOpt = GetPropertyValue(tspanFontFamilyProp);
      std::string spanFontFamily =
          tspanFontFamilyOpt.has_value() ? CleanFontFamily(tspanFontFamilyOpt->family()) : fontFamily;

      auto tspanFontSizeProp = tspan->getFontSize();
      auto tspanFontSizeOpt = GetPropertyValue(tspanFontSizeProp);
      float spanFontSize = tspanFontSizeOpt.has_value()
                               ? std::stof(lengthToFloat(tspanFontSizeOpt->size(), _height))
                               : fontSize;

      // Get tspan-specific text-anchor (inherit from text if not specified)
      auto tspanTextAnchorProp = tspan->getTextAnchor();
      auto tspanTextAnchorOpt = GetPropertyValue(tspanTextAnchorProp);
      std::string spanTextAnchor = textAnchor;
      if (tspanTextAnchorOpt.has_value()) {
        switch (tspanTextAnchorOpt->type()) {
          case SVGTextAnchor::Type::Middle:
            spanTextAnchor = "middle";
            break;
          case SVGTextAnchor::Type::End:
            spanTextAnchor = "end";
            break;
          default:
            spanTextAnchor = "start";
            break;
        }
      }

      for (const auto& tspanChild : tspanChildren) {
        if (tspanChild->tag() != SVGTag::TextLiteral) {
          continue;
        }
        auto literal = static_cast<const SVGTextLiteral*>(tspanChild.get());
        const std::string& textContent = literal->getText();
        if (textContent.empty()) {
          continue;
        }

        indent(depth + 2);
        _output << "<Group>\n";

        indent(depth + 3);
        _output << "<TextSpan x=\"" << spanX << "\" y=\"" << spanY << "\"";
        if (!spanFontFamily.empty()) {
          _output << " font=\"" << spanFontFamily << "\"";
        }
        _output << " fontSize=\"" << spanFontSize << "\"";
        if (spanTextAnchor != "start") {
          _output << " textAnchor=\"" << spanTextAnchor << "\"";
        }
        _output << "><![CDATA[" << textContent << "]]></TextSpan>\n";

        // Use tspan's fill/stroke if specified, otherwise inherit from text
        writeFillStyle(tspan, depth + 3, 0, 0, 0, 0);
        writeStrokeStyle(tspan, depth + 3, 0, 0, 0, 0);

        indent(depth + 2);
        _output << "</Group>\n";
      }
    }
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

void SVGToPAGXConverter::writeFillStyle(const SVGNode* node, int depth, float shapeX, float shapeY,
                                        float shapeWidth, float shapeHeight) {
  auto fillProp = node->getFill();
  auto fillOpt = GetPropertyValue(fillProp);

  // Only output Fill if fill attribute is explicitly set
  if (!fillOpt.has_value()) {
    return;
  }

  auto fillValue = fillOpt.value();
  if (fillValue.type() == SVGPaint::Type::None) {
    return;
  }

  auto fillRuleProp = node->getFillRule();
  auto fillRuleOpt = GetPropertyValue(fillRuleProp);
  bool hasEvenOddFillRule =
      fillRuleOpt.has_value() && fillRuleOpt.value().type() == SVGFillRule::Type::EvenOdd;

  if (fillValue.type() == SVGPaint::Type::IRI) {
    auto iri = fillValue.iri().iri();
    if (iri.empty()) {
      return;
    }

    // Check if this color source should be inlined
    auto usageIt = _colorSourceUsages.find(iri);
    bool shouldInline =
        (usageIt == _colorSourceUsages.end()) || (usageIt->second.count == 1) ||
        (_patterns.find(iri) != _patterns.end() && _patterns.at(iri).isObjectBoundingBox);

    if (shouldInline) {
      // Check if it's a gradient
      auto gradientIt = _gradients.find(iri);
      if (gradientIt != _gradients.end()) {
        indent(depth);
        _output << "<Fill";
        if (hasEvenOddFillRule) {
          _output << " fillRule=\"evenOdd\"";
        }
        _output << ">\n";
        writeInlineGradient(iri, depth + 1);
        indent(depth);
        _output << "</Fill>\n";
        return;
      }

      // Check if it's a pattern
      auto patternIt = _patterns.find(iri);
      if (patternIt != _patterns.end()) {
        indent(depth);
        _output << "<Fill";
        if (hasEvenOddFillRule) {
          _output << " fillRule=\"evenOdd\"";
        }
        _output << ">\n";
        writeInlineImagePattern(patternIt->second, depth + 1, shapeX, shapeY, shapeWidth,
                                shapeHeight);
        indent(depth);
        _output << "</Fill>\n";
        return;
      }
    }

    // Reference to shared resource
    indent(depth);
    _output << "<Fill color=\"#" << iri << "\"";
    if (hasEvenOddFillRule) {
      _output << " fillRule=\"evenOdd\"";
    }
    _output << "/>\n";
    return;
  }

  // Solid color
  Color fillColor = Color::Black();
  if (fillValue.type() == SVGPaint::Type::Color) {
    fillColor = fillValue.color().color();
    auto fillOpacityProp = node->getFillOpacity();
    auto fillOpacityOpt = GetPropertyValue(fillOpacityProp);
    if (fillOpacityOpt.has_value()) {
      fillColor.alpha = fillOpacityOpt.value();
    }
  }

  indent(depth);
  _output << "<Fill color=\"" << colorToHex(fillColor) << "\"";
  if (hasEvenOddFillRule) {
    _output << " fillRule=\"evenOdd\"";
  }
  _output << "/>\n";
}

void SVGToPAGXConverter::writeStrokeStyle(const SVGNode* node, int depth, float shapeX,
                                          float shapeY, float shapeWidth, float shapeHeight) {
  auto strokeProp = node->getStroke();
  auto strokeOpt = GetPropertyValue(strokeProp);
  if (!strokeOpt.has_value()) {
    return;
  }

  auto strokeValue = strokeOpt.value();
  if (strokeValue.type() == SVGPaint::Type::None) {
    return;
  }

  // Collect stroke attributes
  auto strokeWidthProp = node->getStrokeWidth();
  auto strokeWidthOpt = GetPropertyValue(strokeWidthProp);
  float width = 1.0f;
  if (strokeWidthOpt.has_value()) {
    width = std::stof(lengthToFloat(strokeWidthOpt.value(), _width));
  }

  std::string capAttr = {};
  auto lineCapProp = node->getStrokeLineCap();
  auto lineCapOpt = GetPropertyValue(lineCapProp);
  if (lineCapOpt.has_value()) {
    auto cap = lineCapOpt.value();
    if (cap == SVGLineCap::Round) {
      capAttr = " cap=\"round\"";
    } else if (cap == SVGLineCap::Square) {
      capAttr = " cap=\"square\"";
    }
  }

  std::string joinAttr = {};
  auto lineJoinProp = node->getStrokeLineJoin();
  auto lineJoinOpt = GetPropertyValue(lineJoinProp);
  if (lineJoinOpt.has_value()) {
    auto joinType = lineJoinOpt.value().type();
    if (joinType == SVGLineJoin::Type::Round) {
      joinAttr = " join=\"round\"";
    } else if (joinType == SVGLineJoin::Type::Bevel) {
      joinAttr = " join=\"bevel\"";
    }
  }

  std::string miterAttr = {};
  auto miterLimitProp = node->getStrokeMiterLimit();
  auto miterLimitOpt = GetPropertyValue(miterLimitProp);
  if (miterLimitOpt.has_value() && miterLimitOpt.value() != 4.0f) {
    std::ostringstream ss;
    ss << " miterLimit=\"" << miterLimitOpt.value() << "\"";
    miterAttr = ss.str();
  }

  std::string dashesAttr = {};
  auto dashArrayProp = node->getStrokeDashArray();
  auto dashArrayOpt = GetPropertyValue(dashArrayProp);
  if (dashArrayOpt.has_value()) {
    auto& dashes = dashArrayOpt.value().dashArray();
    if (!dashes.empty()) {
      std::ostringstream ss;
      ss << " dashes=\"";
      for (size_t i = 0; i < dashes.size(); ++i) {
        if (i > 0) {
          ss << ",";
        }
        ss << lengthToFloat(dashes[i], _width);
      }
      ss << "\"";
      dashesAttr = ss.str();
    }
  }

  std::string dashOffsetAttr = {};
  auto dashOffsetProp = node->getStrokeDashOffset();
  auto dashOffsetOpt = GetPropertyValue(dashOffsetProp);
  if (dashOffsetOpt.has_value()) {
    float offset = std::stof(lengthToFloat(dashOffsetOpt.value(), _width));
    if (offset != 0.0f) {
      std::ostringstream ss;
      ss << " dashOffset=\"" << offset << "\"";
      dashOffsetAttr = ss.str();
    }
  }

  // Handle IRI reference (gradient or pattern)
  if (strokeValue.type() == SVGPaint::Type::IRI) {
    auto iri = strokeValue.iri().iri();
    if (iri.empty()) {
      return;
    }

    // Check if this color source should be inlined
    auto usageIt = _colorSourceUsages.find(iri);
    bool shouldInline =
        (usageIt == _colorSourceUsages.end()) || (usageIt->second.count == 1) ||
        (_patterns.find(iri) != _patterns.end() && _patterns.at(iri).isObjectBoundingBox);

    if (shouldInline) {
      // Check if it's a gradient
      auto gradientIt = _gradients.find(iri);
      if (gradientIt != _gradients.end()) {
        indent(depth);
        _output << "<Stroke width=\"" << width << "\"" << capAttr << joinAttr << miterAttr
                << dashesAttr << dashOffsetAttr << ">\n";
        writeInlineGradient(iri, depth + 1);
        indent(depth);
        _output << "</Stroke>\n";
        return;
      }

      // Check if it's a pattern
      auto patternIt = _patterns.find(iri);
      if (patternIt != _patterns.end()) {
        indent(depth);
        _output << "<Stroke width=\"" << width << "\"" << capAttr << joinAttr << miterAttr
                << dashesAttr << dashOffsetAttr << ">\n";
        writeInlineImagePattern(patternIt->second, depth + 1, shapeX, shapeY, shapeWidth,
                                shapeHeight);
        indent(depth);
        _output << "</Stroke>\n";
        return;
      }
    }

    // Reference to shared resource
    indent(depth);
    _output << "<Stroke color=\"#" << iri << "\" width=\"" << width << "\"" << capAttr << joinAttr
            << miterAttr << dashesAttr << dashOffsetAttr << "/>\n";
    return;
  }

  // Solid color
  indent(depth);
  _output << "<Stroke";
  if (strokeValue.type() == SVGPaint::Type::Color) {
    auto color = strokeValue.color().color();
    auto strokeOpacityProp = node->getStrokeOpacity();
    auto strokeOpacityOpt = GetPropertyValue(strokeOpacityProp);
    if (strokeOpacityOpt.has_value()) {
      color.alpha = strokeOpacityOpt.value();
    }
    _output << " color=\"" << colorToHex(color) << "\"";
  }
  _output << " width=\"" << width << "\"" << capAttr << joinAttr << miterAttr << dashesAttr
          << dashOffsetAttr << "/>\n";
}

void SVGToPAGXConverter::writeInlineGradient(const std::string& gradientId, int depth) {
  auto it = _gradients.find(gradientId);
  if (it == _gradients.end()) {
    return;
  }

  auto node = it->second;
  auto tag = node->tag();

  if (tag == SVGTag::LinearGradient) {
    auto gradient = static_cast<const SVGLinearGradient*>(node);
    indent(depth);
    _output << "<LinearGradient";
    _output << " startX=\"" << lengthToFloat(gradient->getX1(), _width) << "\"";
    _output << " startY=\"" << lengthToFloat(gradient->getY1(), _height) << "\"";
    _output << " endX=\"" << lengthToFloat(gradient->getX2(), _width) << "\"";
    _output << " endY=\"" << lengthToFloat(gradient->getY2(), _height) << "\"";
    _output << ">\n";

    auto container = static_cast<const SVGContainer*>(node);
    for (const auto& child : container->getChildren()) {
      if (child->tag() == SVGTag::Stop) {
        auto stop = static_cast<const SVGStop*>(child.get());
        indent(depth + 1);
        auto offset = stop->getOffset();
        float offsetValue = offset.value();
        if (offset.unit() == SVGLength::Unit::Percentage) {
          offsetValue = offsetValue / 100.0f;
        }
        _output << "<ColorStop offset=\"" << offsetValue << "\"";
        auto stopColorProp = stop->getStopColor();
        auto stopColorOpt = GetPropertyValue(stopColorProp);
        if (stopColorOpt.has_value()) {
          auto color = stopColorOpt->color();
          auto opacityProp = stop->getStopOpacity();
          auto opacityOpt = GetPropertyValue(opacityProp);
          float alpha = opacityOpt.has_value() ? opacityOpt.value() : 1.0f;
          Color c = color;
          c.alpha = alpha;
          _output << " color=\"" << colorToHex(c) << "\"";
        }
        _output << "/>\n";
      }
    }

    indent(depth);
    _output << "</LinearGradient>\n";
  } else if (tag == SVGTag::RadialGradient) {
    auto gradient = static_cast<const SVGRadialGradient*>(node);
    indent(depth);
    _output << "<RadialGradient";
    _output << " centerX=\"" << lengthToFloat(gradient->getCx(), _width) << "\"";
    _output << " centerY=\"" << lengthToFloat(gradient->getCy(), _height) << "\"";
    auto r = gradient->getR();
    _output << " radius=\"" << lengthToFloat(r, std::max(_width, _height)) << "\"";
    _output << ">\n";

    auto container = static_cast<const SVGContainer*>(node);
    for (const auto& child : container->getChildren()) {
      if (child->tag() == SVGTag::Stop) {
        auto stop = static_cast<const SVGStop*>(child.get());
        indent(depth + 1);
        auto offset = stop->getOffset();
        float offsetValue = offset.value();
        if (offset.unit() == SVGLength::Unit::Percentage) {
          offsetValue = offsetValue / 100.0f;
        }
        _output << "<ColorStop offset=\"" << offsetValue << "\"";
        auto stopColorProp = stop->getStopColor();
        auto stopColorOpt = GetPropertyValue(stopColorProp);
        if (stopColorOpt.has_value()) {
          auto color = stopColorOpt->color();
          auto opacityProp = stop->getStopOpacity();
          auto opacityOpt = GetPropertyValue(opacityProp);
          float alpha = opacityOpt.has_value() ? opacityOpt.value() : 1.0f;
          Color c = color;
          c.alpha = alpha;
          _output << " color=\"" << colorToHex(c) << "\"";
        }
        _output << "/>\n";
      }
    }

    indent(depth);
    _output << "</RadialGradient>\n";
  }
}

void SVGToPAGXConverter::writeInlineImagePattern(const PatternInfo& patternInfo, int depth,
                                                 float shapeX, float shapeY, float shapeWidth,
                                                 float shapeHeight) {
  indent(depth);
  _output << "<ImagePattern image=\"#" << patternInfo.imageId
          << "\" tileModeX=\"repeat\" tileModeY=\"repeat\"";

  // For objectBoundingBox patterns, compute the matrix based on shape size
  // The pattern tile size = shape_size × pattern_ratio
  // In shader matrix: scale(N) makes the pattern N times larger visually
  // So to scale image from imageSize to tileSize, use matrix scale = tileSize / imageSize
  // Also need to translate to align pattern with shape's top-left corner
  if (patternInfo.isObjectBoundingBox && patternInfo.patternWidth > 0 &&
      patternInfo.patternHeight > 0 && shapeWidth > 0 && shapeHeight > 0) {
    float tileWidth = shapeWidth * patternInfo.patternWidth;
    float tileHeight = shapeHeight * patternInfo.patternHeight;

    if (patternInfo.imageWidth > 0 && patternInfo.imageHeight > 0 && tileWidth > 0 &&
        tileHeight > 0) {
      float scaleX = tileWidth / patternInfo.imageWidth;
      float scaleY = tileHeight / patternInfo.imageHeight;
      // Translate to align pattern with shape's top-left corner
      float tx = shapeX;
      float ty = shapeY;
      _output << " matrix=\"" << scaleX << ",0,0," << scaleY << "," << tx << "," << ty << "\"";
    }
  }

  _output << "/>\n";
}

std::string SVGToPAGXConverter::colorToHex(const Color& color) const {
  auto toHex = [](float value) -> int { return static_cast<int>(std::round(value * 255)); };

  std::ostringstream ss;
  ss << "#" << std::uppercase << std::hex << std::setfill('0');
  ss << std::setw(2) << toHex(color.red);
  ss << std::setw(2) << toHex(color.green);
  ss << std::setw(2) << toHex(color.blue);

  if (color.alpha < 1.0f) {
    ss << std::setw(2) << toHex(color.alpha);
  }

  return ss.str();
}

std::string SVGToPAGXConverter::matrixToString(const Matrix& matrix) const {
  std::ostringstream ss;
  ss << matrix.getScaleX() << "," << matrix.getSkewY() << "," << matrix.getSkewX() << ","
     << matrix.getScaleY() << "," << matrix.getTranslateX() << "," << matrix.getTranslateY();
  return ss.str();
}

std::string SVGToPAGXConverter::lengthToFloat(const SVGLength& length, float containerSize) const {
  float value = length.value();
  auto unit = length.unit();

  switch (unit) {
    case SVGLength::Unit::Percentage:
      value = value * containerSize / 100.0f;
      break;
    case SVGLength::Unit::PX:
    case SVGLength::Unit::Number:
    case SVGLength::Unit::Unknown:
    default:
      break;
  }

  std::ostringstream ss;
  ss << value;
  return ss.str();
}

void SVGToPAGXConverter::indent(int depth) {
  for (int i = 0; i < depth; ++i) {
    _output << "  ";
  }
}

int SVGToPAGXConverter::countRenderableChildren(const SVGContainer* container) const {
  int count = 0;
  for (const auto& child : container->getChildren()) {
    auto tag = child->tag();
    if (tag == SVGTag::Defs || tag == SVGTag::LinearGradient || tag == SVGTag::RadialGradient ||
        tag == SVGTag::Stop || tag == SVGTag::ClipPath || tag == SVGTag::Mask ||
        tag == SVGTag::Filter || tag == SVGTag::G || tag == SVGTag::Svg) {
      continue;
    }
    auto visibilityProp = child->getVisibility();
    auto visibilityOpt = GetPropertyValue(visibilityProp);
    if (visibilityOpt.has_value() && visibilityOpt.value().type() == SVGVisibility::Type::Hidden) {
      continue;
    }
    count++;
  }
  return count;
}

bool SVGToPAGXConverter::hasOnlyFill(const SVGNode* node) const {
  auto fillProp = node->getFill();
  auto fillOpt = GetPropertyValue(fillProp);
  bool hasFill = fillOpt.has_value() && fillOpt->type() != SVGPaint::Type::None;

  auto strokeProp = node->getStroke();
  auto strokeOpt = GetPropertyValue(strokeProp);
  bool hasStroke = strokeOpt.has_value() && strokeOpt->type() != SVGPaint::Type::None;

  return hasFill && !hasStroke;
}

bool SVGToPAGXConverter::hasOnlyStroke(const SVGNode* node) const {
  auto fillProp = node->getFill();
  auto fillOpt = GetPropertyValue(fillProp);
  bool hasFill = fillOpt.has_value() && fillOpt->type() != SVGPaint::Type::None;

  auto strokeProp = node->getStroke();
  auto strokeOpt = GetPropertyValue(strokeProp);
  bool hasStroke = strokeOpt.has_value() && strokeOpt->type() != SVGPaint::Type::None;

  return !hasFill && hasStroke;
}

bool SVGToPAGXConverter::areRectsEqual(const SVGRect* a, const SVGRect* b) const {
  if (!a || !b) {
    return false;
  }

  auto ax = lengthToFloat(a->getX(), _width);
  auto ay = lengthToFloat(a->getY(), _height);
  auto aw = lengthToFloat(a->getWidth(), _width);
  auto ah = lengthToFloat(a->getHeight(), _height);

  auto bx = lengthToFloat(b->getX(), _width);
  auto by = lengthToFloat(b->getY(), _height);
  auto bw = lengthToFloat(b->getWidth(), _width);
  auto bh = lengthToFloat(b->getHeight(), _height);

  if (ax != bx || ay != by || aw != bw || ah != bh) {
    return false;
  }

  float arx = 0, ary = 0, brx = 0, bry = 0;
  if (a->getRx().has_value()) {
    arx = std::stof(lengthToFloat(*a->getRx(), _width));
  }
  if (a->getRy().has_value()) {
    ary = std::stof(lengthToFloat(*a->getRy(), _height));
  }
  if (b->getRx().has_value()) {
    brx = std::stof(lengthToFloat(*b->getRx(), _width));
  }
  if (b->getRy().has_value()) {
    bry = std::stof(lengthToFloat(*b->getRy(), _height));
  }

  return arx == brx && ary == bry;
}

void SVGToPAGXConverter::convertChildren(const std::vector<std::shared_ptr<SVGNode>>& children,
                                         int depth) {
  size_t i = 0;
  while (i < children.size()) {
    auto& child = children[i];
    auto tag = child->tag();

    // Skip non-renderable elements
    if (tag == SVGTag::Defs || tag == SVGTag::LinearGradient || tag == SVGTag::RadialGradient ||
        tag == SVGTag::Stop || tag == SVGTag::ClipPath || tag == SVGTag::Mask ||
        tag == SVGTag::Filter || tag == SVGTag::Pattern) {
      i++;
      continue;
    }

    auto visibilityProp = child->getVisibility();
    auto visibilityOpt = GetPropertyValue(visibilityProp);
    if (visibilityOpt.has_value() && visibilityOpt.value().type() == SVGVisibility::Type::Hidden) {
      i++;
      continue;
    }

    // Check for consecutive identical rects (fill-only followed by stroke-only)
    if (tag == SVGTag::Rect && hasOnlyFill(child.get()) && i + 1 < children.size()) {
      auto& next = children[i + 1];
      if (next->tag() == SVGTag::Rect && hasOnlyStroke(next.get())) {
        auto fillRect = static_cast<const SVGRect*>(child.get());
        auto strokeRect = static_cast<const SVGRect*>(next.get());
        if (areRectsEqual(fillRect, strokeRect)) {
          convertRectWithStroke(fillRect, strokeRect, depth);
          i += 2;
          continue;
        }
      }
    }

    // Normal processing
    if (tag == SVGTag::G || tag == SVGTag::Svg) {
      convertContainer(static_cast<const SVGContainer*>(child.get()), depth);
    } else {
      convertNode(child.get(), depth, false);
    }
    i++;
  }
}

void SVGToPAGXConverter::convertRectWithStroke(const SVGRect* fillRect, const SVGRect* strokeRect,
                                               int depth) {
  float x = std::stof(lengthToFloat(fillRect->getX(), _width));
  float y = std::stof(lengthToFloat(fillRect->getY(), _height));
  float width = std::stof(lengthToFloat(fillRect->getWidth(), _width));
  float height = std::stof(lengthToFloat(fillRect->getHeight(), _height));

  if (width <= 0 || height <= 0) {
    return;
  }

  float centerX = x + width / 2.0f;
  float centerY = y + height / 2.0f;

  float rx = 0;
  float ry = 0;
  auto rxOpt = fillRect->getRx();
  auto ryOpt = fillRect->getRy();
  if (rxOpt.has_value()) {
    rx = std::stof(lengthToFloat(*rxOpt, _width));
  }
  if (ryOpt.has_value()) {
    ry = std::stof(lengthToFloat(*ryOpt, _height));
  }
  if (rx > 0 && ry <= 0) {
    ry = rx;
  }
  if (ry > 0 && rx <= 0) {
    rx = ry;
  }

  auto transformable = static_cast<const SVGTransformableNode*>(fillRect);
  auto transform = transformable->getTransform();
  bool hasTransform = !transform.isIdentity();

  auto opacityProp = fillRect->getOpacity();
  auto opacityOpt = GetPropertyValue(opacityProp);
  bool hasOpacity = opacityOpt.has_value() && opacityOpt.value() < 1.0f;

  indent(depth);
  _output << "<Layer>\n";
  indent(depth + 1);
  _output << "<contents>\n";

  int contentDepth = depth + 2;
  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "<Group";
    if (hasTransform) {
      _output << " matrix=\"" << matrixToString(transform) << "\"";
    }
    if (hasOpacity) {
      _output << " alpha=\"" << opacityOpt.value() << "\"";
    }
    _output << ">\n";
    contentDepth = depth + 3;
  }

  indent(contentDepth);
  _output << "<Rectangle centerX=\"" << centerX << "\" centerY=\"" << centerY << "\" width=\""
          << width << "\" height=\"" << height << "\"";
  if (rx > 0 || ry > 0) {
    float roundness = std::min(rx, ry);
    _output << " roundness=\"" << roundness << "\"";
  }
  _output << "/>\n";

  writeFillStyle(fillRect, contentDepth, x, y, width, height);
  writeStrokeStyle(strokeRect, contentDepth, x, y, width, height);

  if (hasTransform || hasOpacity) {
    indent(depth + 2);
    _output << "</Group>\n";
  }

  indent(depth + 1);
  _output << "</contents>\n";
  indent(depth);
  _output << "</Layer>\n";
}

std::string SVGToPAGXConverter::getMaskId(const SVGNode* node) const {
  if (!node) {
    return "";
  }

  auto maskProp = node->getMask();
  auto maskOpt = GetPropertyValue(maskProp);
  if (!maskOpt.has_value() || maskOpt->type() != SVGFuncIRI::Type::IRI) {
    return "";
  }

  auto maskIri = maskOpt->iri().iri();
  if (maskIri.empty()) {
    return "";
  }

  // Check if this mask exists in our collected masks
  if (_masks.find(maskIri) == _masks.end()) {
    return "";
  }

  return maskIri;
}

void SVGToPAGXConverter::writeMaskLayers(int depth) {
  // Write mask layers that are referenced by content layers
  for (const auto& [maskId, maskInfo] : _masks) {
    // Only output masks that will be used
    if (_usedMasks.find(maskId) == _usedMasks.end()) {
      continue;
    }

    auto maskNode = static_cast<const SVGMask*>(maskInfo.maskNode);
    auto container = static_cast<const SVGContainer*>(maskNode);
    if (!container->hasChildren()) {
      continue;
    }

    indent(depth);
    _output << "<Layer id=\"mask_" << maskId << "\" visible=\"false\">\n";
    indent(depth + 1);
    _output << "<contents>\n";

    // Convert mask children - we need to output shapes directly without wrapping in Layer
    for (const auto& child : container->getChildren()) {
      auto tag = child->tag();
      if (tag == SVGTag::Rect) {
        auto rect = static_cast<const SVGRect*>(child.get());
        float x = std::stof(lengthToFloat(rect->getX(), _width));
        float y = std::stof(lengthToFloat(rect->getY(), _height));
        float width = std::stof(lengthToFloat(rect->getWidth(), _width));
        float height = std::stof(lengthToFloat(rect->getHeight(), _height));
        float centerX = x + width / 2.0f;
        float centerY = y + height / 2.0f;

        indent(depth + 2);
        _output << "<Rectangle centerX=\"" << centerX << "\" centerY=\"" << centerY << "\" width=\""
                << width << "\" height=\"" << height << "\"/>\n";
        writeFillStyle(rect, depth + 2, x, y, width, height);
      } else if (tag == SVGTag::Circle) {
        auto circle = static_cast<const SVGCircle*>(child.get());
        float cx = std::stof(lengthToFloat(circle->getCx(), _width));
        float cy = std::stof(lengthToFloat(circle->getCy(), _height));
        float r = std::stof(lengthToFloat(circle->getR(), std::max(_width, _height)));

        indent(depth + 2);
        _output << "<Ellipse centerX=\"" << cx << "\" centerY=\"" << cy << "\" width=\"" << r * 2
                << "\" height=\"" << r * 2 << "\"/>\n";
        writeFillStyle(circle, depth + 2, cx - r, cy - r, r * 2, r * 2);
      } else if (tag == SVGTag::Ellipse) {
        auto ellipse = static_cast<const SVGEllipse*>(child.get());
        float cx = std::stof(lengthToFloat(ellipse->getCx(), _width));
        float cy = std::stof(lengthToFloat(ellipse->getCy(), _height));
        auto rxOpt = ellipse->getRx();
        auto ryOpt = ellipse->getRy();
        float rx = rxOpt.has_value() ? std::stof(lengthToFloat(*rxOpt, _width)) : 0;
        float ry = ryOpt.has_value() ? std::stof(lengthToFloat(*ryOpt, _height)) : 0;

        indent(depth + 2);
        _output << "<Ellipse centerX=\"" << cx << "\" centerY=\"" << cy << "\" width=\"" << rx * 2
                << "\" height=\"" << ry * 2 << "\"/>\n";
        writeFillStyle(ellipse, depth + 2, cx - rx, cy - ry, rx * 2, ry * 2);
      } else if (tag == SVGTag::Path) {
        auto path = static_cast<const SVGPath*>(child.get());
        auto shapePath = path->getShapePath();
        if (!shapePath.isEmpty()) {
          indent(depth + 2);
          _output << "<Path d=\"" << SVGPathParser::ToSVGString(shapePath) << "\"/>\n";
          auto bounds = shapePath.getBounds();
          writeFillStyle(path, depth + 2, bounds.left, bounds.top, bounds.width(), bounds.height());
        }
      }
    }

    indent(depth + 1);
    _output << "</contents>\n";
    indent(depth);
    _output << "</Layer>\n";
  }
}

}  // namespace pagx
