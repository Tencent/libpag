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

#include "pagx/model/types/Types.h"
#include "pagx/model/types/Enums.h"
#include "pagx/model/NodeType.h"
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
  // CSS Color Level 4: color(colorspace r g b) or color(colorspace r g b / a)
  // Supported colorspaces: display-p3, a98-rgb, rec2020, srgb
  if (str.substr(0, 6) == "color(") {
    auto start = str.find('(');
    auto end = str.find(')');
    if (start != std::string::npos && end != std::string::npos) {
      auto inner = str.substr(start + 1, end - start - 1);
      // Trim whitespace
      inner.erase(0, inner.find_first_not_of(" \t"));
      inner.erase(inner.find_last_not_of(" \t") + 1);

      // Parse colorspace and values (space-separated)
      std::istringstream iss(inner);
      std::string colorspace = {};
      iss >> colorspace;

      std::vector<float> components = {};
      std::string token = {};
      float alpha = 1.0f;
      bool foundSlash = false;

      while (iss >> token) {
        if (token == "/") {
          foundSlash = true;
          continue;
        }
        float value = std::stof(token);
        if (foundSlash) {
          alpha = value;
        } else {
          components.push_back(value);
        }
      }

      if (components.size() >= 3) {
        float r = components[0];
        float g = components[1];
        float b = components[2];

        // Convert from wide gamut colorspace to sRGB (approximate clipping).
        // For display-p3, a98-rgb, rec2020: values are in 0-1 range.
        // We do a simplified conversion by clamping to sRGB gamut.
        // A proper implementation would use ICC profiles or matrix transforms.
        if (colorspace == "display-p3") {
          // Display P3 to sRGB approximate conversion matrix.
          float sR = 1.2249f * r - 0.2247f * g - 0.0002f * b;
          float sG = -0.0420f * r + 1.0419f * g + 0.0001f * b;
          float sB = -0.0197f * r - 0.0786f * g + 1.0983f * b;
          r = std::max(0.0f, std::min(1.0f, sR));
          g = std::max(0.0f, std::min(1.0f, sG));
          b = std::max(0.0f, std::min(1.0f, sB));
        } else if (colorspace == "a98-rgb") {
          // Adobe RGB to sRGB approximate conversion.
          float sR = 1.3982f * r - 0.3982f * g + 0.0f * b;
          float sG = 0.0f * r + 1.0f * g + 0.0f * b;
          float sB = 0.0f * r - 0.0429f * g + 1.0429f * b;
          r = std::max(0.0f, std::min(1.0f, sR));
          g = std::max(0.0f, std::min(1.0f, sG));
          b = std::max(0.0f, std::min(1.0f, sB));
        } else if (colorspace == "rec2020") {
          // Rec.2020 to sRGB approximate conversion.
          float sR = 1.6605f * r - 0.5877f * g - 0.0728f * b;
          float sG = -0.1246f * r + 1.1330f * g - 0.0084f * b;
          float sB = -0.0182f * r - 0.1006f * g + 1.1188f * b;
          r = std::max(0.0f, std::min(1.0f, sR));
          g = std::max(0.0f, std::min(1.0f, sG));
          b = std::max(0.0f, std::min(1.0f, sB));
        }
        // For "srgb" or unknown colorspaces, use values directly.

        return Color::FromRGBA(r, g, b, alpha);
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
    {BlendMode::PlusLighter, "plusLighter"},
    {BlendMode::PlusDarker, "plusDarker"})

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

#undef DEFINE_ENUM_CONVERSION

const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::SolidColor:
      return "SolidColor";
    case NodeType::LinearGradient:
      return "LinearGradient";
    case NodeType::RadialGradient:
      return "RadialGradient";
    case NodeType::ConicGradient:
      return "ConicGradient";
    case NodeType::DiamondGradient:
      return "DiamondGradient";
    case NodeType::ImagePattern:
      return "ImagePattern";
    case NodeType::ColorStop:
      return "ColorStop";
    case NodeType::Rectangle:
      return "Rectangle";
    case NodeType::Ellipse:
      return "Ellipse";
    case NodeType::Polystar:
      return "Polystar";
    case NodeType::Path:
      return "Path";
    case NodeType::TextSpan:
      return "TextSpan";
    case NodeType::Fill:
      return "Fill";
    case NodeType::Stroke:
      return "Stroke";
    case NodeType::TrimPath:
      return "TrimPath";
    case NodeType::RoundCorner:
      return "RoundCorner";
    case NodeType::MergePath:
      return "MergePath";
    case NodeType::TextModifier:
      return "TextModifier";
    case NodeType::TextPath:
      return "TextPath";
    case NodeType::TextLayout:
      return "TextLayout";
    case NodeType::RangeSelector:
      return "RangeSelector";
    case NodeType::Repeater:
      return "Repeater";
    case NodeType::Group:
      return "Group";
    case NodeType::DropShadowStyle:
      return "DropShadowStyle";
    case NodeType::InnerShadowStyle:
      return "InnerShadowStyle";
    case NodeType::BackgroundBlurStyle:
      return "BackgroundBlurStyle";
    case NodeType::BlurFilter:
      return "BlurFilter";
    case NodeType::DropShadowFilter:
      return "DropShadowFilter";
    case NodeType::InnerShadowFilter:
      return "InnerShadowFilter";
    case NodeType::BlendFilter:
      return "BlendFilter";
    case NodeType::ColorMatrixFilter:
      return "ColorMatrixFilter";
    case NodeType::Image:
      return "Image";
    case NodeType::PathData:
      return "PathData";
    case NodeType::Composition:
      return "Composition";
    case NodeType::Layer:
      return "Layer";
  }
  return "Unknown";
}

}  // namespace pagx
