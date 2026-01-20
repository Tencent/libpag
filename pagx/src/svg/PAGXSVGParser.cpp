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

  // Second pass: convert elements to a root layer.
  auto rootLayer = std::make_unique<LayerNode>();
  rootLayer->name = "root";

  child = root->getFirstChild();
  while (child) {
    if (child->name != "defs") {
      auto layer = convertToLayer(child);
      if (layer) {
        rootLayer->children.push_back(std::move(layer));
      }
    }
    child = child->getNextSibling();
  }

  _document->layers.push_back(std::move(rootLayer));
  return _document;
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

std::unique_ptr<LayerNode> SVGParserImpl::convertToLayer(const std::shared_ptr<DOMNode>& element) {
  const auto& tag = element->name;

  if (tag == "defs" || tag == "linearGradient" || tag == "radialGradient" || tag == "pattern" ||
      tag == "mask" || tag == "clipPath" || tag == "filter") {
    return nullptr;
  }

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

  // Convert contents.
  if (tag == "g" || tag == "svg") {
    // Group: convert children as child layers.
    auto child = element->getFirstChild();
    while (child) {
      auto childLayer = convertToLayer(child);
      if (childLayer) {
        layer->children.push_back(std::move(childLayer));
      }
      child = child->getNextSibling();
    }
  } else {
    // Shape element: convert to vector contents.
    convertChildren(element, layer->contents);
  }

  return layer;
}

void SVGParserImpl::convertChildren(const std::shared_ptr<DOMNode>& element,
                                    std::vector<std::unique_ptr<VectorElementNode>>& contents) {
  auto shapeElement = convertElement(element);
  if (shapeElement) {
    contents.push_back(std::move(shapeElement));
  }

  addFillStroke(element, contents);
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

std::unique_ptr<GroupNode> SVGParserImpl::convertG(const std::shared_ptr<DOMNode>& element) {
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
    addFillStroke(child, group->elements);
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

std::unique_ptr<GroupNode> SVGParserImpl::convertText(const std::shared_ptr<DOMNode>& element) {
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

  addFillStroke(element, group->elements);
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
  gradient->startPoint.x = parseLength(getAttribute(element, "x1", "0%"), 1.0f);
  gradient->startPoint.y = parseLength(getAttribute(element, "y1", "0%"), 1.0f);
  gradient->endPoint.x = parseLength(getAttribute(element, "x2", "100%"), 1.0f);
  gradient->endPoint.y = parseLength(getAttribute(element, "y2", "0%"), 1.0f);

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
  gradient->center.x = parseLength(getAttribute(element, "cx", "50%"), 1.0f);
  gradient->center.y = parseLength(getAttribute(element, "cy", "50%"), 1.0f);
  gradient->radius = parseLength(getAttribute(element, "r", "50%"), 1.0f);

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

void SVGParserImpl::addFillStroke(const std::shared_ptr<DOMNode>& element,
                                  std::vector<std::unique_ptr<VectorElementNode>>& contents) {
  std::string fill = getAttribute(element, "fill");
  if (!fill.empty() && fill != "none") {
    auto fillNode = std::make_unique<FillNode>();

    if (fill.find("url(") == 0) {
      std::string refId = resolveUrl(fill);
      fillNode->color = "#" + refId;

      // Try to inline the gradient.
      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->name == "linearGradient") {
          fillNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->name == "radialGradient") {
          fillNode->colorSource = convertRadialGradient(it->second);
        }
      }
    } else {
      Color color = parseColor(fill);
      std::string fillOpacity = getAttribute(element, "fill-opacity");
      if (!fillOpacity.empty()) {
        color.alpha = std::stof(fillOpacity);
      }
      fillNode->color = color.toHexString(color.alpha < 1);
    }

    std::string fillRule = getAttribute(element, "fill-rule");
    if (fillRule == "evenodd") {
      fillNode->fillRule = FillRule::EvenOdd;
    }

    contents.push_back(std::move(fillNode));
  } else if (fill.empty()) {
    // SVG default is black fill.
    auto fillNode = std::make_unique<FillNode>();
    fillNode->color = "#000000";
    contents.push_back(std::move(fillNode));
  }

  std::string stroke = getAttribute(element, "stroke");
  if (!stroke.empty() && stroke != "none") {
    auto strokeNode = std::make_unique<StrokeNode>();

    if (stroke.find("url(") == 0) {
      std::string refId = resolveUrl(stroke);
      strokeNode->color = "#" + refId;

      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->name == "linearGradient") {
          strokeNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->name == "radialGradient") {
          strokeNode->colorSource = convertRadialGradient(it->second);
        }
      }
    } else {
      Color color = parseColor(stroke);
      std::string strokeOpacity = getAttribute(element, "stroke-opacity");
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
      std::istringstream iss(dashArray);
      float val = 0;
      char sep = 0;
      while (iss >> val) {
        strokeNode->dashes.push_back(val);
        iss >> sep;
      }
    }

    std::string dashOffset = getAttribute(element, "stroke-dashoffset");
    if (!dashOffset.empty()) {
      strokeNode->dashOffset = parseLength(dashOffset, _viewBoxWidth);
    }

    contents.push_back(std::move(strokeNode));
  }
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

  // Named colors (subset).
  static const std::unordered_map<std::string, uint32_t> namedColors = {
      {"black", 0x000000},      {"white", 0xFFFFFF},   {"red", 0xFF0000},
      {"green", 0x008000},      {"blue", 0x0000FF},    {"yellow", 0xFFFF00},
      {"cyan", 0x00FFFF},       {"magenta", 0xFF00FF}, {"gray", 0x808080},
      {"grey", 0x808080},       {"silver", 0xC0C0C0},  {"maroon", 0x800000},
      {"olive", 0x808000},      {"lime", 0x00FF00},    {"aqua", 0x00FFFF},
      {"teal", 0x008080},       {"navy", 0x000080},    {"fuchsia", 0xFF00FF},
      {"purple", 0x800080},     {"orange", 0xFFA500},  {"transparent", 0x000000},
  };

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
    char c = 0;
    iss >> c;  // Skip separator.
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

}  // namespace pagx
