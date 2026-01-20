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

#include "PAGXAttributes.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace pagx {

float PAGXAttributes::ParseFloat(const std::shared_ptr<DOMNode>& node, const std::string& name,
                                 float defaultValue) {
  auto [found, value] = node->findAttribute(name);
  if (!found || value.empty()) {
    return defaultValue;
  }
  return std::strtof(value.c_str(), nullptr);
}

bool PAGXAttributes::ParseBool(const std::shared_ptr<DOMNode>& node, const std::string& name,
                               bool defaultValue) {
  auto [found, value] = node->findAttribute(name);
  if (!found || value.empty()) {
    return defaultValue;
  }
  if (value == "true" || value == "1") {
    return true;
  }
  if (value == "false" || value == "0") {
    return false;
  }
  return defaultValue;
}

std::string PAGXAttributes::ParseString(const std::shared_ptr<DOMNode>& node,
                                        const std::string& name, const std::string& defaultValue) {
  auto [found, value] = node->findAttribute(name);
  if (!found) {
    return defaultValue;
  }
  return value;
}

static int HexCharToInt(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}

static Color ParseHexColor(const std::string& hex) {
  size_t start = (hex[0] == '#') ? 1 : 0;
  std::string h = hex.substr(start);
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;

  if (h.length() == 3) {
    // #RGB -> #RRGGBB
    r = static_cast<float>(HexCharToInt(h[0]) * 17) / 255.0f;
    g = static_cast<float>(HexCharToInt(h[1]) * 17) / 255.0f;
    b = static_cast<float>(HexCharToInt(h[2]) * 17) / 255.0f;
  } else if (h.length() == 6) {
    // #RRGGBB
    r = static_cast<float>(HexCharToInt(h[0]) * 16 + HexCharToInt(h[1])) / 255.0f;
    g = static_cast<float>(HexCharToInt(h[2]) * 16 + HexCharToInt(h[3])) / 255.0f;
    b = static_cast<float>(HexCharToInt(h[4]) * 16 + HexCharToInt(h[5])) / 255.0f;
  } else if (h.length() == 8) {
    // #RRGGBBAA
    r = static_cast<float>(HexCharToInt(h[0]) * 16 + HexCharToInt(h[1])) / 255.0f;
    g = static_cast<float>(HexCharToInt(h[2]) * 16 + HexCharToInt(h[3])) / 255.0f;
    b = static_cast<float>(HexCharToInt(h[4]) * 16 + HexCharToInt(h[5])) / 255.0f;
    a = static_cast<float>(HexCharToInt(h[6]) * 16 + HexCharToInt(h[7])) / 255.0f;
  }
  return {r, g, b, a};
}

static Color ParseRGBColor(const std::string& value) {
  // rgb(255,0,0) or rgba(255,0,0,0.5)
  bool hasAlpha = value.find("rgba") != std::string::npos;
  size_t start = hasAlpha ? 5 : 4;
  size_t end = value.find(')');
  std::string content = value.substr(start, end - start);

  std::vector<float> values;
  std::istringstream stream(content);
  std::string token;
  while (std::getline(stream, token, ',')) {
    values.push_back(std::strtof(token.c_str(), nullptr));
  }

  float r = values.size() > 0 ? values[0] / 255.0f : 0.0f;
  float g = values.size() > 1 ? values[1] / 255.0f : 0.0f;
  float b = values.size() > 2 ? values[2] / 255.0f : 0.0f;
  float a = values.size() > 3 ? values[3] : 1.0f;
  return {r, g, b, a};
}

Color PAGXAttributes::ParseColor(const std::string& value) {
  if (value.empty()) {
    return Color::Black();
  }
  if (value[0] == '#') {
    return ParseHexColor(value);
  }
  if (value.find("rgb") == 0) {
    return ParseRGBColor(value);
  }
  // TODO: Add HSL and color() support per spec
  // Default fallback
  return Color::Black();
}

Point PAGXAttributes::ParsePoint(const std::string& value, Point defaultValue) {
  if (value.empty()) {
    return defaultValue;
  }
  size_t commaPos = value.find(',');
  if (commaPos == std::string::npos) {
    return defaultValue;
  }
  float x = std::strtof(value.substr(0, commaPos).c_str(), nullptr);
  float y = std::strtof(value.substr(commaPos + 1).c_str(), nullptr);
  return {x, y};
}

Matrix PAGXAttributes::ParseMatrix(const std::string& value) {
  if (value.empty()) {
    return Matrix::I();
  }
  std::vector<float> values;
  std::istringstream stream(value);
  std::string token;
  while (std::getline(stream, token, ',')) {
    values.push_back(std::strtof(token.c_str(), nullptr));
  }
  if (values.size() >= 6) {
    // a,b,c,d,tx,ty
    return Matrix::MakeAll(values[0], values[2], values[4], values[1], values[3], values[5]);
  }
  return Matrix::I();
}

BlendMode PAGXAttributes::ParseBlendMode(const std::string& value) {
  if (value == "multiply") {
    return BlendMode::Multiply;
  }
  if (value == "screen") {
    return BlendMode::Screen;
  }
  if (value == "overlay") {
    return BlendMode::Overlay;
  }
  if (value == "darken") {
    return BlendMode::Darken;
  }
  if (value == "lighten") {
    return BlendMode::Lighten;
  }
  if (value == "colorDodge") {
    return BlendMode::ColorDodge;
  }
  if (value == "colorBurn") {
    return BlendMode::ColorBurn;
  }
  if (value == "hardLight") {
    return BlendMode::HardLight;
  }
  if (value == "softLight") {
    return BlendMode::SoftLight;
  }
  if (value == "difference") {
    return BlendMode::Difference;
  }
  if (value == "exclusion") {
    return BlendMode::Exclusion;
  }
  if (value == "hue") {
    return BlendMode::Hue;
  }
  if (value == "saturation") {
    return BlendMode::Saturation;
  }
  if (value == "color") {
    return BlendMode::Color;
  }
  if (value == "luminosity") {
    return BlendMode::Luminosity;
  }
  if (value == "add") {
    return BlendMode::PlusLighter;
  }
  return BlendMode::SrcOver;
}

PathFillType PAGXAttributes::ParseFillRule(const std::string& value) {
  if (value == "evenOdd") {
    return PathFillType::EvenOdd;
  }
  return PathFillType::Winding;
}

LineCap PAGXAttributes::ParseLineCap(const std::string& value) {
  if (value == "round") {
    return LineCap::Round;
  }
  if (value == "square") {
    return LineCap::Square;
  }
  return LineCap::Butt;
}

LineJoin PAGXAttributes::ParseLineJoin(const std::string& value) {
  if (value == "round") {
    return LineJoin::Round;
  }
  if (value == "bevel") {
    return LineJoin::Bevel;
  }
  return LineJoin::Miter;
}

TileMode PAGXAttributes::ParseTileMode(const std::string& value) {
  if (value == "repeat") {
    return TileMode::Repeat;
  }
  if (value == "mirror") {
    return TileMode::Mirror;
  }
  if (value == "decal") {
    return TileMode::Decal;
  }
  return TileMode::Clamp;
}

LayerMaskType PAGXAttributes::ParseMaskType(const std::string& value) {
  if (value == "luminance") {
    return LayerMaskType::Luminance;
  }
  if (value == "contour") {
    return LayerMaskType::Contour;
  }
  return LayerMaskType::Alpha;
}

PolystarType PAGXAttributes::ParsePolystarType(const std::string& value) {
  if (value == "polygon") {
    return PolystarType::Polygon;
  }
  return PolystarType::Star;
}

TrimPathType PAGXAttributes::ParseTrimPathType(const std::string& value) {
  if (value == "continuous") {
    return TrimPathType::Continuous;
  }
  return TrimPathType::Separate;
}

MergePathOp PAGXAttributes::ParseMergePathOp(const std::string& value) {
  if (value == "union") {
    return MergePathOp::Union;
  }
  if (value == "intersect") {
    return MergePathOp::Intersect;
  }
  if (value == "xor") {
    return MergePathOp::XOR;
  }
  if (value == "difference") {
    return MergePathOp::Difference;
  }
  return MergePathOp::Append;
}

StrokeAlign PAGXAttributes::ParseStrokeAlign(const std::string& value) {
  if (value == "inside") {
    return StrokeAlign::Inside;
  }
  if (value == "outside") {
    return StrokeAlign::Outside;
  }
  return StrokeAlign::Center;
}

std::vector<float> PAGXAttributes::ParseDashes(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  std::vector<float> dashes;
  std::istringstream stream(value);
  std::string token;
  while (std::getline(stream, token, ',')) {
    dashes.push_back(std::strtof(token.c_str(), nullptr));
  }
  return dashes;
}

std::string PAGXAttributes::GetTextContent(const std::shared_ptr<DOMNode>& node) {
  if (!node) {
    return "";
  }
  auto child = node->firstChild;
  while (child) {
    if (child->type == DOMNodeType::Text) {
      return child->name;
    }
    child = child->nextSibling;
  }
  return "";
}

}  // namespace pagx
