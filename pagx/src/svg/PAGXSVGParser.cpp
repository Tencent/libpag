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
#include <cstring>
#include <fstream>
#include <sstream>
#include "SVGParserInternal.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXSVGParser::Parse(const std::string& filePath,
                                                   const Options& options) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return nullptr;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  auto doc = Parse(reinterpret_cast<const uint8_t*>(content.data()), content.size(), options);
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

// Simple XML parser for SVG
class SVGXMLTokenizer {
 public:
  SVGXMLTokenizer(const char* data, size_t length) : _data(data), _length(length), _pos(0) {
  }

  bool isEnd() const {
    return _pos >= _length;
  }

  void skipWhitespace() {
    while (_pos < _length && std::isspace(_data[_pos])) {
      ++_pos;
    }
  }

  bool match(char c) {
    if (_pos < _length && _data[_pos] == c) {
      ++_pos;
      return true;
    }
    return false;
  }

  bool matchString(const char* str) {
    size_t len = std::strlen(str);
    if (_pos + len <= _length && std::strncmp(_data + _pos, str, len) == 0) {
      _pos += len;
      return true;
    }
    return false;
  }

  std::string readUntil(char delimiter) {
    std::string result;
    while (_pos < _length && _data[_pos] != delimiter) {
      result += _data[_pos++];
    }
    return result;
  }

  std::string readUntilAny(const char* delimiters) {
    std::string result;
    while (_pos < _length) {
      bool isDelim = false;
      for (const char* d = delimiters; *d; ++d) {
        if (_data[_pos] == *d) {
          isDelim = true;
          break;
        }
      }
      if (isDelim) {
        break;
      }
      result += _data[_pos++];
    }
    return result;
  }

  std::string readName() {
    std::string result;
    while (_pos < _length &&
           (std::isalnum(_data[_pos]) || _data[_pos] == '_' || _data[_pos] == '-' ||
            _data[_pos] == ':')) {
      result += _data[_pos++];
    }
    return result;
  }

  std::string readQuotedString() {
    char quote = _data[_pos++];
    std::string result;
    while (_pos < _length && _data[_pos] != quote) {
      if (_data[_pos] == '&') {
        result += readEscapeSequence();
      } else {
        result += _data[_pos++];
      }
    }
    if (_pos < _length) {
      ++_pos;
    }
    return result;
  }

  std::string readEscapeSequence() {
    std::string seq;
    ++_pos;
    while (_pos < _length && _data[_pos] != ';') {
      seq += _data[_pos++];
    }
    if (_pos < _length) {
      ++_pos;
    }
    if (seq == "lt")
      return "<";
    if (seq == "gt")
      return ">";
    if (seq == "amp")
      return "&";
    if (seq == "quot")
      return "\"";
    if (seq == "apos")
      return "'";
    if (!seq.empty() && seq[0] == '#') {
      int code = 0;
      if (seq.length() > 1 && seq[1] == 'x') {
        code = std::stoi(seq.substr(2), nullptr, 16);
      } else {
        code = std::stoi(seq.substr(1));
      }
      if (code > 0 && code < 128) {
        return std::string(1, static_cast<char>(code));
      }
    }
    return "&" + seq + ";";
  }

  std::string readTextContent() {
    std::string result;
    while (_pos < _length && _data[_pos] != '<') {
      if (_data[_pos] == '&') {
        result += readEscapeSequence();
      } else {
        result += _data[_pos++];
      }
    }
    return result;
  }

  void skipComment() {
    while (_pos + 2 < _length) {
      if (_data[_pos] == '-' && _data[_pos + 1] == '-' && _data[_pos + 2] == '>') {
        _pos += 3;
        return;
      }
      ++_pos;
    }
  }

  void skipCDATA() {
    while (_pos + 2 < _length) {
      if (_data[_pos] == ']' && _data[_pos + 1] == ']' && _data[_pos + 2] == '>') {
        _pos += 3;
        return;
      }
      ++_pos;
    }
  }

  void skipProcessingInstruction() {
    while (_pos + 1 < _length) {
      if (_data[_pos] == '?' && _data[_pos + 1] == '>') {
        _pos += 2;
        return;
      }
      ++_pos;
    }
  }

 private:
  const char* _data = nullptr;
  size_t _length = 0;
  size_t _pos = 0;
};

static std::unique_ptr<SVGXMLNode> ParseSVGXMLElement(SVGXMLTokenizer& tokenizer);

static void ParseSVGXMLAttributes(SVGXMLTokenizer& tokenizer, SVGXMLNode* node) {
  while (true) {
    tokenizer.skipWhitespace();
    if (tokenizer.isEnd()) {
      break;
    }

    std::string name = tokenizer.readName();
    if (name.empty()) {
      break;
    }

    tokenizer.skipWhitespace();
    if (!tokenizer.match('=')) {
      break;
    }

    tokenizer.skipWhitespace();
    std::string value = tokenizer.readQuotedString();
    node->attributes[name] = value;
  }
}

static std::unique_ptr<SVGXMLNode> ParseSVGXMLElement(SVGXMLTokenizer& tokenizer) {
  tokenizer.skipWhitespace();

  if (tokenizer.isEnd() || !tokenizer.match('<')) {
    return nullptr;
  }

  while (true) {
    if (tokenizer.matchString("!--")) {
      tokenizer.skipComment();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("![CDATA[")) {
      tokenizer.skipCDATA();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("?")) {
      tokenizer.skipProcessingInstruction();
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else if (tokenizer.matchString("!DOCTYPE")) {
      tokenizer.readUntil('>');
      tokenizer.match('>');
      tokenizer.skipWhitespace();
      if (!tokenizer.match('<')) {
        return nullptr;
      }
    } else {
      break;
    }
  }

  if (tokenizer.match('/')) {
    return nullptr;
  }

  std::string tagName = tokenizer.readName();
  if (tagName.empty()) {
    return nullptr;
  }

  auto node = std::make_unique<SVGXMLNode>();
  node->tagName = tagName;

  ParseSVGXMLAttributes(tokenizer, node.get());

  tokenizer.skipWhitespace();

  if (tokenizer.match('/')) {
    tokenizer.match('>');
    return node;
  }

  if (!tokenizer.match('>')) {
    return node;
  }

  while (true) {
    std::string text = tokenizer.readTextContent();
    if (!text.empty()) {
      // Trim whitespace
      size_t start = text.find_first_not_of(" \t\n\r");
      size_t end = text.find_last_not_of(" \t\n\r");
      if (start != std::string::npos && end != std::string::npos) {
        node->textContent += text.substr(start, end - start + 1);
      }
    }

    if (tokenizer.isEnd()) {
      break;
    }

    auto child = ParseSVGXMLElement(tokenizer);
    if (!child) {
      tokenizer.skipWhitespace();
      tokenizer.readUntil('>');
      tokenizer.match('>');
      break;
    }

    node->children.push_back(std::move(child));
  }

  return node;
}

std::unique_ptr<SVGXMLNode> SVGParserImpl::parseXML(const char* data, size_t length) {
  SVGXMLTokenizer tokenizer(data, length);
  return ParseSVGXMLElement(tokenizer);
}

std::shared_ptr<PAGXDocument> SVGParserImpl::parse(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    return nullptr;
  }

  auto xmlRoot = parseXML(reinterpret_cast<const char*>(data), length);
  if (!xmlRoot || xmlRoot->tagName != "svg") {
    return nullptr;
  }

  // Parse viewBox and dimensions
  auto viewBox = parseViewBox(xmlRoot->getAttribute("viewBox"));
  float width = parseLength(xmlRoot->getAttribute("width"), 0);
  float height = parseLength(xmlRoot->getAttribute("height"), 0);

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

  // First pass: collect defs
  for (auto& child : xmlRoot->children) {
    if (child->tagName == "defs") {
      parseDefs(child.get());
    }
  }

  // Second pass: convert elements to a root layer
  auto rootLayer = std::make_unique<LayerNode>();
  rootLayer->name = "root";

  for (auto& child : xmlRoot->children) {
    if (child->tagName == "defs") {
      continue;
    }
    auto layer = convertToLayer(child.get());
    if (layer) {
      rootLayer->children.push_back(std::move(layer));
    }
  }

  _document->layers.push_back(std::move(rootLayer));
  return _document;
}

void SVGParserImpl::parseDefs(SVGXMLNode* defsNode) {
  for (auto& child : defsNode->children) {
    std::string id = child->getAttribute("id");
    if (!id.empty()) {
      _defs[id] = child.get();
    }
    // Also handle nested defs
    if (child->tagName == "defs") {
      parseDefs(child.get());
    }
  }
}

std::unique_ptr<LayerNode> SVGParserImpl::convertToLayer(SVGXMLNode* element) {
  const auto& tag = element->tagName;

  if (tag == "defs" || tag == "linearGradient" || tag == "radialGradient" || tag == "pattern" ||
      tag == "mask" || tag == "clipPath" || tag == "filter") {
    return nullptr;
  }

  auto layer = std::make_unique<LayerNode>();

  // Parse common layer attributes
  layer->id = element->getAttribute("id");
  layer->name = element->getAttribute("id");

  std::string transform = element->getAttribute("transform");
  if (!transform.empty()) {
    layer->matrix = parseTransform(transform);
  }

  std::string opacity = element->getAttribute("opacity");
  if (!opacity.empty()) {
    layer->alpha = std::stof(opacity);
  }

  std::string display = element->getAttribute("display");
  if (display == "none") {
    layer->visible = false;
  }

  std::string visibility = element->getAttribute("visibility");
  if (visibility == "hidden") {
    layer->visible = false;
  }

  // Convert contents
  if (tag == "g" || tag == "svg") {
    // Group: convert children as child layers
    for (auto& child : element->children) {
      auto childLayer = convertToLayer(child.get());
      if (childLayer) {
        layer->children.push_back(std::move(childLayer));
      }
    }
  } else {
    // Shape element: convert to vector contents
    convertChildren(element, layer->contents);
  }

  return layer;
}

void SVGParserImpl::convertChildren(SVGXMLNode* element,
                                    std::vector<std::unique_ptr<VectorElementNode>>& contents) {
  auto shapeElement = convertElement(element);
  if (shapeElement) {
    contents.push_back(std::move(shapeElement));
  }

  addFillStroke(element, contents);
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertElement(SVGXMLNode* element) {
  const auto& tag = element->tagName;

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

std::unique_ptr<GroupNode> SVGParserImpl::convertG(SVGXMLNode* element) {
  auto group = std::make_unique<GroupNode>();

  group->name = element->getAttribute("id");

  std::string transform = element->getAttribute("transform");
  if (!transform.empty()) {
    // For GroupNode, we need to decompose the matrix into position/rotation/scale
    // For simplicity, just store as position offset for translation
    Matrix m = parseTransform(transform);
    group->position = {m.tx, m.ty};
    // Note: Full matrix decomposition would be more complex
  }

  std::string opacity = element->getAttribute("opacity");
  if (!opacity.empty()) {
    group->alpha = std::stof(opacity);
  }

  for (auto& child : element->children) {
    auto childElement = convertElement(child.get());
    if (childElement) {
      group->elements.push_back(std::move(childElement));
    }
    addFillStroke(child.get(), group->elements);
  }

  return group;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertRect(SVGXMLNode* element) {
  float x = parseLength(element->getAttribute("x"), _viewBoxWidth);
  float y = parseLength(element->getAttribute("y"), _viewBoxHeight);
  float width = parseLength(element->getAttribute("width"), _viewBoxWidth);
  float height = parseLength(element->getAttribute("height"), _viewBoxHeight);
  float rx = parseLength(element->getAttribute("rx"), _viewBoxWidth);
  float ry = parseLength(element->getAttribute("ry"), _viewBoxHeight);

  if (ry == 0) {
    ry = rx;
  }

  auto rect = std::make_unique<RectangleNode>();
  rect->centerX = x + width / 2;
  rect->centerY = y + height / 2;
  rect->width = width;
  rect->height = height;
  rect->roundness = std::max(rx, ry);

  return rect;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertCircle(SVGXMLNode* element) {
  float cx = parseLength(element->getAttribute("cx"), _viewBoxWidth);
  float cy = parseLength(element->getAttribute("cy"), _viewBoxHeight);
  float r = parseLength(element->getAttribute("r"), _viewBoxWidth);

  auto ellipse = std::make_unique<EllipseNode>();
  ellipse->centerX = cx;
  ellipse->centerY = cy;
  ellipse->width = r * 2;
  ellipse->height = r * 2;

  return ellipse;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertEllipse(SVGXMLNode* element) {
  float cx = parseLength(element->getAttribute("cx"), _viewBoxWidth);
  float cy = parseLength(element->getAttribute("cy"), _viewBoxHeight);
  float rx = parseLength(element->getAttribute("rx"), _viewBoxWidth);
  float ry = parseLength(element->getAttribute("ry"), _viewBoxHeight);

  auto ellipse = std::make_unique<EllipseNode>();
  ellipse->centerX = cx;
  ellipse->centerY = cy;
  ellipse->width = rx * 2;
  ellipse->height = ry * 2;

  return ellipse;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertLine(SVGXMLNode* element) {
  float x1 = parseLength(element->getAttribute("x1"), _viewBoxWidth);
  float y1 = parseLength(element->getAttribute("y1"), _viewBoxHeight);
  float x2 = parseLength(element->getAttribute("x2"), _viewBoxWidth);
  float y2 = parseLength(element->getAttribute("y2"), _viewBoxHeight);

  auto path = std::make_unique<PathNode>();
  path->d.moveTo(x1, y1);
  path->d.lineTo(x2, y2);

  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPolyline(SVGXMLNode* element) {
  auto path = std::make_unique<PathNode>();
  path->d = parsePoints(element->getAttribute("points"), false);
  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPolygon(SVGXMLNode* element) {
  auto path = std::make_unique<PathNode>();
  path->d = parsePoints(element->getAttribute("points"), true);
  return path;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertPath(SVGXMLNode* element) {
  auto path = std::make_unique<PathNode>();
  std::string d = element->getAttribute("d");
  if (!d.empty()) {
    path->d = PathData::FromSVGString(d);
  }
  return path;
}

std::unique_ptr<GroupNode> SVGParserImpl::convertText(SVGXMLNode* element) {
  auto group = std::make_unique<GroupNode>();

  float x = parseLength(element->getAttribute("x"), _viewBoxWidth);
  float y = parseLength(element->getAttribute("y"), _viewBoxHeight);

  if (!element->textContent.empty()) {
    auto textSpan = std::make_unique<TextSpanNode>();
    textSpan->x = x;
    textSpan->y = y;
    textSpan->text = element->textContent;

    std::string fontFamily = element->getAttribute("font-family");
    if (!fontFamily.empty()) {
      textSpan->font = fontFamily;
    }

    std::string fontSize = element->getAttribute("font-size");
    if (!fontSize.empty()) {
      textSpan->fontSize = parseLength(fontSize, _viewBoxHeight);
    }

    group->elements.push_back(std::move(textSpan));
  }

  addFillStroke(element, group->elements);
  return group;
}

std::unique_ptr<VectorElementNode> SVGParserImpl::convertUse(SVGXMLNode* element) {
  std::string href = element->getAttribute("xlink:href");
  if (href.empty()) {
    href = element->getAttribute("href");
  }

  std::string refId = resolveUrl(href);
  auto it = _defs.find(refId);
  if (it == _defs.end()) {
    return nullptr;
  }

  if (_options.expandUseReferences) {
    auto node = convertElement(it->second);
    if (node) {
      float x = parseLength(element->getAttribute("x"), _viewBoxWidth);
      float y = parseLength(element->getAttribute("y"), _viewBoxHeight);
      if (x != 0 || y != 0) {
        // Wrap in a group with translation
        auto group = std::make_unique<GroupNode>();
        group->position = {x, y};
        group->elements.push_back(std::move(node));
        return group;
      }
    }
    return node;
  }

  // For non-expanded use references, just create an empty group for now
  auto group = std::make_unique<GroupNode>();
  group->name = "_useRef:" + refId;
  return group;
}

std::unique_ptr<LinearGradientNode> SVGParserImpl::convertLinearGradient(SVGXMLNode* element) {
  auto gradient = std::make_unique<LinearGradientNode>();

  gradient->id = element->getAttribute("id");
  gradient->startX = parseLength(element->getAttribute("x1", "0%"), 1.0f);
  gradient->startY = parseLength(element->getAttribute("y1", "0%"), 1.0f);
  gradient->endX = parseLength(element->getAttribute("x2", "100%"), 1.0f);
  gradient->endY = parseLength(element->getAttribute("y2", "0%"), 1.0f);

  // Parse stops
  for (auto& child : element->children) {
    if (child->tagName == "stop") {
      ColorStopNode stop;
      stop.offset = parseLength(child->getAttribute("offset", "0"), 1.0f);
      stop.color = parseColor(child->getAttribute("stop-color", "#000000"));
      float opacity = parseLength(child->getAttribute("stop-opacity", "1"), 1.0f);
      stop.color.alpha = opacity;
      gradient->colorStops.push_back(stop);
    }
  }

  return gradient;
}

std::unique_ptr<RadialGradientNode> SVGParserImpl::convertRadialGradient(SVGXMLNode* element) {
  auto gradient = std::make_unique<RadialGradientNode>();

  gradient->id = element->getAttribute("id");
  gradient->centerX = parseLength(element->getAttribute("cx", "50%"), 1.0f);
  gradient->centerY = parseLength(element->getAttribute("cy", "50%"), 1.0f);
  gradient->radius = parseLength(element->getAttribute("r", "50%"), 1.0f);

  // Parse stops
  for (auto& child : element->children) {
    if (child->tagName == "stop") {
      ColorStopNode stop;
      stop.offset = parseLength(child->getAttribute("offset", "0"), 1.0f);
      stop.color = parseColor(child->getAttribute("stop-color", "#000000"));
      float opacity = parseLength(child->getAttribute("stop-opacity", "1"), 1.0f);
      stop.color.alpha = opacity;
      gradient->colorStops.push_back(stop);
    }
  }

  return gradient;
}

void SVGParserImpl::addFillStroke(SVGXMLNode* element,
                                  std::vector<std::unique_ptr<VectorElementNode>>& contents) {
  std::string fill = element->getAttribute("fill");
  if (!fill.empty() && fill != "none") {
    auto fillNode = std::make_unique<FillNode>();

    if (fill.find("url(") == 0) {
      std::string refId = resolveUrl(fill);
      fillNode->color = "#" + refId;

      // Try to inline the gradient
      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->tagName == "linearGradient") {
          fillNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->tagName == "radialGradient") {
          fillNode->colorSource = convertRadialGradient(it->second);
        }
      }
    } else {
      Color color = parseColor(fill);
      std::string fillOpacity = element->getAttribute("fill-opacity");
      if (!fillOpacity.empty()) {
        color.alpha = std::stof(fillOpacity);
      }
      fillNode->color = color.toHexString(color.alpha < 1);
    }

    std::string fillRule = element->getAttribute("fill-rule");
    if (fillRule == "evenodd") {
      fillNode->fillRule = FillRule::EvenOdd;
    }

    contents.push_back(std::move(fillNode));
  } else if (fill.empty()) {
    // SVG default is black fill
    auto fillNode = std::make_unique<FillNode>();
    fillNode->color = "#000000";
    contents.push_back(std::move(fillNode));
  }

  std::string stroke = element->getAttribute("stroke");
  if (!stroke.empty() && stroke != "none") {
    auto strokeNode = std::make_unique<StrokeNode>();

    if (stroke.find("url(") == 0) {
      std::string refId = resolveUrl(stroke);
      strokeNode->color = "#" + refId;

      auto it = _defs.find(refId);
      if (it != _defs.end()) {
        if (it->second->tagName == "linearGradient") {
          strokeNode->colorSource = convertLinearGradient(it->second);
        } else if (it->second->tagName == "radialGradient") {
          strokeNode->colorSource = convertRadialGradient(it->second);
        }
      }
    } else {
      Color color = parseColor(stroke);
      std::string strokeOpacity = element->getAttribute("stroke-opacity");
      if (!strokeOpacity.empty()) {
        color.alpha = std::stof(strokeOpacity);
      }
      strokeNode->color = color.toHexString(color.alpha < 1);
    }

    std::string strokeWidth = element->getAttribute("stroke-width");
    if (!strokeWidth.empty()) {
      strokeNode->strokeWidth = parseLength(strokeWidth, _viewBoxWidth);
    }

    std::string strokeLinecap = element->getAttribute("stroke-linecap");
    if (!strokeLinecap.empty()) {
      strokeNode->cap = LineCapFromString(strokeLinecap);
    }

    std::string strokeLinejoin = element->getAttribute("stroke-linejoin");
    if (!strokeLinejoin.empty()) {
      strokeNode->join = LineJoinFromString(strokeLinejoin);
    }

    std::string strokeMiterlimit = element->getAttribute("stroke-miterlimit");
    if (!strokeMiterlimit.empty()) {
      strokeNode->miterLimit = std::stof(strokeMiterlimit);
    }

    std::string dashArray = element->getAttribute("stroke-dasharray");
    if (!dashArray.empty() && dashArray != "none") {
      std::istringstream iss(dashArray);
      float val = 0;
      char sep = 0;
      while (iss >> val) {
        strokeNode->dashes.push_back(val);
        iss >> sep;
      }
    }

    std::string dashOffset = element->getAttribute("stroke-dashoffset");
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

  // Named colors (subset)
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
    return num * 16.0f;  // Assume 16px base font
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
    iss >> c;  // Skip separator
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
  // Handle url(#id) format
  if (url.find("url(") == 0) {
    size_t start = url.find('#');
    size_t end = url.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      return url.substr(start + 1, end - start - 1);
    }
  }
  // Handle #id format
  if (url[0] == '#') {
    return url.substr(1);
  }
  return url;
}

}  // namespace pagx
