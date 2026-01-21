/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/PAGXTypes.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace pagx {

//==============================================================================
// Color implementation
//==============================================================================

Color Color::FromHex(uint32_t hex, bool hasAlpha) {
  Color color = {};
  if (hasAlpha) {
    color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
  } else {
    color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
    color.alpha = 1.0f;
  }
  return color;
}

Color Color::FromRGBA(float r, float g, float b, float a) {
  return {r, g, b, a};
}

static int ParseHexDigit(char c) {
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

Color Color::Parse(const std::string& str) {
  if (str.empty()) {
    return {};
  }
  if (str[0] == '#') {
    auto hex = str.substr(1);
    if (hex.size() == 3) {
      int r = ParseHexDigit(hex[0]);
      int g = ParseHexDigit(hex[1]);
      int b = ParseHexDigit(hex[2]);
      return Color::FromRGBA(static_cast<float>(r * 17) / 255.0f,
                             static_cast<float>(g * 17) / 255.0f,
                             static_cast<float>(b * 17) / 255.0f, 1.0f);
    }
    if (hex.size() == 6) {
      int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
      int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
      int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
      return Color::FromRGBA(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                             static_cast<float>(b) / 255.0f, 1.0f);
    }
    if (hex.size() == 8) {
      int r = ParseHexDigit(hex[0]) * 16 + ParseHexDigit(hex[1]);
      int g = ParseHexDigit(hex[2]) * 16 + ParseHexDigit(hex[3]);
      int b = ParseHexDigit(hex[4]) * 16 + ParseHexDigit(hex[5]);
      int a = ParseHexDigit(hex[6]) * 16 + ParseHexDigit(hex[7]);
      return Color::FromRGBA(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                             static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f);
    }
  }
  if (str.substr(0, 4) == "rgb(" || str.substr(0, 5) == "rgba(") {
    auto start = str.find('(');
    auto end = str.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto values = str.substr(start + 1, end - start - 1);
      std::istringstream iss(values);
      std::string token = {};
      std::vector<float> components = {};
      while (std::getline(iss, token, ',')) {
        auto trimmed = token;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
        components.push_back(std::stof(trimmed));
      }
      if (components.size() >= 3) {
        float r = components[0] / 255.0f;
        float g = components[1] / 255.0f;
        float b = components[2] / 255.0f;
        float a = components.size() >= 4 ? components[3] : 1.0f;
        return Color::FromRGBA(r, g, b, a);
      }
    }
  }
  return {};
}

std::string Color::toHexString(bool includeAlpha) const {
  auto toHex = [](float v) {
    int i = static_cast<int>(std::round(v * 255.0f));
    i = std::max(0, std::min(255, i));
    char buf[3] = {};
    snprintf(buf, sizeof(buf), "%02X", i);
    return std::string(buf);
  };
  std::string result = "#" + toHex(red) + toHex(green) + toHex(blue);
  if (includeAlpha && alpha < 1.0f) {
    result += toHex(alpha);
  }
  return result;
}

//==============================================================================
// Matrix implementation
//==============================================================================

Matrix Matrix::Translate(float x, float y) {
  Matrix m = {};
  m.tx = x;
  m.ty = y;
  return m;
}

Matrix Matrix::Scale(float sx, float sy) {
  Matrix m = {};
  m.a = sx;
  m.d = sy;
  return m;
}

Matrix Matrix::Rotate(float degrees) {
  Matrix m = {};
  float radians = degrees * 3.14159265358979323846f / 180.0f;
  float cosVal = std::cos(radians);
  float sinVal = std::sin(radians);
  m.a = cosVal;
  m.b = sinVal;
  m.c = -sinVal;
  m.d = cosVal;
  return m;
}

Matrix Matrix::Parse(const std::string& str) {
  Matrix m = {};
  std::istringstream iss(str);
  std::string token = {};
  std::vector<float> values = {};
  while (std::getline(iss, token, ',')) {
    auto trimmed = token;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (!trimmed.empty()) {
      values.push_back(std::stof(trimmed));
    }
  }
  if (values.size() >= 6) {
    m.a = values[0];
    m.b = values[1];
    m.c = values[2];
    m.d = values[3];
    m.tx = values[4];
    m.ty = values[5];
  }
  return m;
}

std::string Matrix::toString() const {
  std::ostringstream oss = {};
  oss << a << "," << b << "," << c << "," << d << "," << tx << "," << ty;
  return oss.str();
}

Matrix Matrix::operator*(const Matrix& other) const {
  Matrix result = {};
  result.a = a * other.a + c * other.b;
  result.b = b * other.a + d * other.b;
  result.c = a * other.c + c * other.d;
  result.d = b * other.c + d * other.d;
  result.tx = a * other.tx + c * other.ty + tx;
  result.ty = b * other.tx + d * other.ty + ty;
  return result;
}

Point Matrix::mapPoint(const Point& point) const {
  return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
}

//==============================================================================
// Enum string conversions
//==============================================================================

#define DEFINE_ENUM_CONVERSION(EnumType, ...)                                                    \
  static const std::unordered_map<EnumType, std::string> EnumType##ToStringMap = {__VA_ARGS__};  \
  static const std::unordered_map<std::string, EnumType> StringTo##EnumType##Map = [] {          \
    std::unordered_map<std::string, EnumType> map = {};                                          \
    for (const auto& pair : EnumType##ToStringMap) {                                             \
      map[pair.second] = pair.first;                                                             \
    }                                                                                            \
    return map;                                                                                  \
  }();                                                                                           \
  std::string EnumType##ToString(EnumType value) {                                               \
    auto it = EnumType##ToStringMap.find(value);                                                 \
    return it != EnumType##ToStringMap.end() ? it->second : "";                                  \
  }                                                                                              \
  EnumType EnumType##FromString(const std::string& str) {                                        \
    auto it = StringTo##EnumType##Map.find(str);                                                 \
    return it != StringTo##EnumType##Map.end() ? it->second : EnumType##ToStringMap.begin()->first; \
  }

DEFINE_ENUM_CONVERSION(BlendMode,
    {BlendMode::Normal, "normal"},
    {BlendMode::Multiply, "multiply"},
    {BlendMode::Screen, "screen"},
    {BlendMode::Overlay, "overlay"},
    {BlendMode::Darken, "darken"},
    {BlendMode::Lighten, "lighten"},
    {BlendMode::ColorDodge, "colorDodge"},
    {BlendMode::ColorBurn, "colorBurn"},
    {BlendMode::HardLight, "hardLight"},
    {BlendMode::SoftLight, "softLight"},
    {BlendMode::Difference, "difference"},
    {BlendMode::Exclusion, "exclusion"},
    {BlendMode::Hue, "hue"},
    {BlendMode::Saturation, "saturation"},
    {BlendMode::Color, "color"},
    {BlendMode::Luminosity, "luminosity"},
    {BlendMode::PlusLighter, "plusLighter"})

DEFINE_ENUM_CONVERSION(LineCap,
    {LineCap::Butt, "butt"},
    {LineCap::Round, "round"},
    {LineCap::Square, "square"})

DEFINE_ENUM_CONVERSION(LineJoin,
    {LineJoin::Miter, "miter"},
    {LineJoin::Round, "round"},
    {LineJoin::Bevel, "bevel"})

DEFINE_ENUM_CONVERSION(FillRule,
    {FillRule::Winding, "winding"},
    {FillRule::EvenOdd, "evenOdd"})

DEFINE_ENUM_CONVERSION(StrokeAlign,
    {StrokeAlign::Center, "center"},
    {StrokeAlign::Inside, "inside"},
    {StrokeAlign::Outside, "outside"})

DEFINE_ENUM_CONVERSION(Placement,
    {Placement::Background, "background"},
    {Placement::Foreground, "foreground"})

DEFINE_ENUM_CONVERSION(TileMode,
    {TileMode::Clamp, "clamp"},
    {TileMode::Repeat, "repeat"},
    {TileMode::Mirror, "mirror"},
    {TileMode::Decal, "decal"})

DEFINE_ENUM_CONVERSION(SamplingMode,
    {SamplingMode::Nearest, "nearest"},
    {SamplingMode::Linear, "linear"},
    {SamplingMode::Mipmap, "mipmap"})

DEFINE_ENUM_CONVERSION(MaskType,
    {MaskType::Alpha, "alpha"},
    {MaskType::Luminance, "luminance"},
    {MaskType::Contour, "contour"})

DEFINE_ENUM_CONVERSION(PolystarType,
    {PolystarType::Polygon, "polygon"},
    {PolystarType::Star, "star"})

DEFINE_ENUM_CONVERSION(TrimType,
    {TrimType::Separate, "separate"},
    {TrimType::Continuous, "continuous"})

DEFINE_ENUM_CONVERSION(MergePathMode,
    {MergePathMode::Append, "append"},
    {MergePathMode::Union, "union"},
    {MergePathMode::Intersect, "intersect"},
    {MergePathMode::Xor, "xor"},
    {MergePathMode::Difference, "difference"})

DEFINE_ENUM_CONVERSION(TextAlign,
    {TextAlign::Left, "left"},
    {TextAlign::Center, "center"},
    {TextAlign::Right, "right"},
    {TextAlign::Justify, "justify"})

DEFINE_ENUM_CONVERSION(VerticalAlign,
    {VerticalAlign::Top, "top"},
    {VerticalAlign::Center, "center"},
    {VerticalAlign::Bottom, "bottom"})

DEFINE_ENUM_CONVERSION(Overflow,
    {Overflow::Clip, "clip"},
    {Overflow::Visible, "visible"},
    {Overflow::Ellipsis, "ellipsis"})

DEFINE_ENUM_CONVERSION(FontStyle,
    {FontStyle::Normal, "normal"},
    {FontStyle::Italic, "italic"})

DEFINE_ENUM_CONVERSION(TextPathAlign,
    {TextPathAlign::Start, "start"},
    {TextPathAlign::Center, "center"},
    {TextPathAlign::End, "end"})

DEFINE_ENUM_CONVERSION(SelectorUnit,
    {SelectorUnit::Index, "index"},
    {SelectorUnit::Percentage, "percentage"})

DEFINE_ENUM_CONVERSION(SelectorShape,
    {SelectorShape::Square, "square"},
    {SelectorShape::RampUp, "rampUp"},
    {SelectorShape::RampDown, "rampDown"},
    {SelectorShape::Triangle, "triangle"},
    {SelectorShape::Round, "round"},
    {SelectorShape::Smooth, "smooth"})

DEFINE_ENUM_CONVERSION(SelectorMode,
    {SelectorMode::Add, "add"},
    {SelectorMode::Subtract, "subtract"},
    {SelectorMode::Intersect, "intersect"},
    {SelectorMode::Min, "min"},
    {SelectorMode::Max, "max"},
    {SelectorMode::Difference, "difference"})

DEFINE_ENUM_CONVERSION(RepeaterOrder,
    {RepeaterOrder::BelowOriginal, "belowOriginal"},
    {RepeaterOrder::AboveOriginal, "aboveOriginal"})

DEFINE_ENUM_CONVERSION(TextAnchor,
    {TextAnchor::Start, "start"},
    {TextAnchor::Middle, "middle"},
    {TextAnchor::End, "end"})

#undef DEFINE_ENUM_CONVERSION

}  // namespace pagx
