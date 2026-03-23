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

#include "StringParser.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include "base/utils/Log.h"

namespace pagx {

//==============================================================================
// Helper macro for enum string conversions
//==============================================================================

#define DEFINE_ENUM_CONVERSION(EnumType, DefaultValue, ...)                                     \
  static const std::unordered_map<EnumType, std::string> EnumType##ToStringMap = {__VA_ARGS__}; \
  static const std::unordered_map<std::string, EnumType> StringTo##EnumType##Map = [] {         \
    std::unordered_map<std::string, EnumType> map = {};                                         \
    for (const auto& pair : EnumType##ToStringMap) {                                            \
      map[pair.second] = pair.first;                                                            \
    }                                                                                           \
    return map;                                                                                 \
  }();                                                                                          \
  std::string EnumType##ToString(EnumType value) {                                              \
    auto it = EnumType##ToStringMap.find(value);                                                \
    return it != EnumType##ToStringMap.end() ? it->second : "";                                 \
  }                                                                                             \
  EnumType EnumType##FromString(const std::string& str) {                                       \
    auto it = StringTo##EnumType##Map.find(str);                                                \
    if (it != StringTo##EnumType##Map.end()) {                                                  \
      return it->second;                                                                        \
    }                                                                                           \
    LOGE("Invalid %s value: \"%s\"", #EnumType, str.c_str());                                   \
    return (DefaultValue);                                                                      \
  }                                                                                             \
  bool IsValid##EnumType##String(const std::string& str) {                                      \
    return StringTo##EnumType##Map.count(str) > 0;                                              \
  }

//==============================================================================
// TypeName functions
//==============================================================================

const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::PathData:
      return "PathData";
    case NodeType::Image:
      return "Image";
    case NodeType::Composition:
      return "Composition";
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
    case NodeType::Layer:
      return "Layer";
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
    case NodeType::Rectangle:
      return "Rectangle";
    case NodeType::Ellipse:
      return "Ellipse";
    case NodeType::Polystar:
      return "Polystar";
    case NodeType::Path:
      return "Path";
    case NodeType::Text:
      return "Text";
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
    case NodeType::TextBox:
      return "TextBox";
    case NodeType::Group:
      return "Group";
    case NodeType::Repeater:
      return "Repeater";
    case NodeType::RangeSelector:
      return "RangeSelector";
    default:
      return "Unknown";
  }
}

//==============================================================================
// Enum string conversions
//==============================================================================

DEFINE_ENUM_CONVERSION(BlendMode, BlendMode::Normal, {BlendMode::Normal, "normal"},
                       {BlendMode::Multiply, "multiply"}, {BlendMode::Screen, "screen"},
                       {BlendMode::Overlay, "overlay"}, {BlendMode::Darken, "darken"},
                       {BlendMode::Lighten, "lighten"}, {BlendMode::ColorDodge, "colorDodge"},
                       {BlendMode::ColorBurn, "colorBurn"}, {BlendMode::HardLight, "hardLight"},
                       {BlendMode::SoftLight, "softLight"}, {BlendMode::Difference, "difference"},
                       {BlendMode::Exclusion, "exclusion"}, {BlendMode::Hue, "hue"},
                       {BlendMode::Saturation, "saturation"}, {BlendMode::Color, "color"},
                       {BlendMode::Luminosity, "luminosity"},
                       {BlendMode::PlusLighter, "plusLighter"},
                       {BlendMode::PlusDarker, "plusDarker"})

DEFINE_ENUM_CONVERSION(LineCap, LineCap::Butt, {LineCap::Butt, "butt"}, {LineCap::Round, "round"},
                       {LineCap::Square, "square"})

DEFINE_ENUM_CONVERSION(LineJoin, LineJoin::Miter, {LineJoin::Miter, "miter"},
                       {LineJoin::Round, "round"}, {LineJoin::Bevel, "bevel"})

DEFINE_ENUM_CONVERSION(FillRule, FillRule::Winding, {FillRule::Winding, "winding"},
                       {FillRule::EvenOdd, "evenOdd"})

DEFINE_ENUM_CONVERSION(StrokeAlign, StrokeAlign::Center, {StrokeAlign::Center, "center"},
                       {StrokeAlign::Inside, "inside"}, {StrokeAlign::Outside, "outside"})

DEFINE_ENUM_CONVERSION(LayerPlacement, LayerPlacement::Foreground,
                       {LayerPlacement::Background, "background"},
                       {LayerPlacement::Foreground, "foreground"})

DEFINE_ENUM_CONVERSION(TileMode, TileMode::Clamp, {TileMode::Clamp, "clamp"},
                       {TileMode::Repeat, "repeat"}, {TileMode::Mirror, "mirror"},
                       {TileMode::Decal, "decal"})

DEFINE_ENUM_CONVERSION(FilterMode, FilterMode::Nearest, {FilterMode::Nearest, "nearest"},
                       {FilterMode::Linear, "linear"})

DEFINE_ENUM_CONVERSION(MipmapMode, MipmapMode::None, {MipmapMode::None, "none"},
                       {MipmapMode::Nearest, "nearest"}, {MipmapMode::Linear, "linear"})

DEFINE_ENUM_CONVERSION(MaskType, MaskType::Alpha, {MaskType::Alpha, "alpha"},
                       {MaskType::Luminance, "luminance"}, {MaskType::Contour, "contour"})

DEFINE_ENUM_CONVERSION(PolystarType, PolystarType::Polygon, {PolystarType::Polygon, "polygon"},
                       {PolystarType::Star, "star"})

DEFINE_ENUM_CONVERSION(TrimType, TrimType::Separate, {TrimType::Separate, "separate"},
                       {TrimType::Continuous, "continuous"})

DEFINE_ENUM_CONVERSION(MergePathMode, MergePathMode::Append, {MergePathMode::Append, "append"},
                       {MergePathMode::Union, "union"}, {MergePathMode::Intersect, "intersect"},
                       {MergePathMode::Xor, "xor"}, {MergePathMode::Difference, "difference"})

DEFINE_ENUM_CONVERSION(TextAlign, TextAlign::Start, {TextAlign::Start, "start"},
                       {TextAlign::Center, "center"}, {TextAlign::End, "end"},
                       {TextAlign::Justify, "justify"})

DEFINE_ENUM_CONVERSION(ParagraphAlign, ParagraphAlign::Near, {ParagraphAlign::Near, "near"},
                       {ParagraphAlign::Middle, "middle"}, {ParagraphAlign::Far, "far"})

DEFINE_ENUM_CONVERSION(TextAnchor, TextAnchor::Start, {TextAnchor::Start, "start"},
                       {TextAnchor::Center, "center"}, {TextAnchor::End, "end"})

DEFINE_ENUM_CONVERSION(WritingMode, WritingMode::Horizontal,
                       {WritingMode::Horizontal, "horizontal"}, {WritingMode::Vertical, "vertical"})

DEFINE_ENUM_CONVERSION(Overflow, Overflow::Visible, {Overflow::Visible, "visible"},
                       {Overflow::Hidden, "hidden"})

DEFINE_ENUM_CONVERSION(SelectorUnit, SelectorUnit::Percentage, {SelectorUnit::Index, "index"},
                       {SelectorUnit::Percentage, "percentage"})

DEFINE_ENUM_CONVERSION(SelectorShape, SelectorShape::Square, {SelectorShape::Square, "square"},
                       {SelectorShape::RampUp, "rampUp"}, {SelectorShape::RampDown, "rampDown"},
                       {SelectorShape::Triangle, "triangle"}, {SelectorShape::Round, "round"},
                       {SelectorShape::Smooth, "smooth"})

DEFINE_ENUM_CONVERSION(SelectorMode, SelectorMode::Add, {SelectorMode::Add, "add"},
                       {SelectorMode::Subtract, "subtract"}, {SelectorMode::Intersect, "intersect"},
                       {SelectorMode::Min, "min"}, {SelectorMode::Max, "max"},
                       {SelectorMode::Difference, "difference"})

DEFINE_ENUM_CONVERSION(RepeaterOrder, RepeaterOrder::BelowOriginal,
                       {RepeaterOrder::BelowOriginal, "belowOriginal"},
                       {RepeaterOrder::AboveOriginal, "aboveOriginal"})

DEFINE_ENUM_CONVERSION(LayoutMode, LayoutMode::None, {LayoutMode::None, "none"},
                       {LayoutMode::Horizontal, "horizontal"}, {LayoutMode::Vertical, "vertical"})

DEFINE_ENUM_CONVERSION(Alignment, Alignment::Stretch, {Alignment::Start, "start"},
                       {Alignment::Center, "center"}, {Alignment::End, "end"},
                       {Alignment::Stretch, "stretch"})

DEFINE_ENUM_CONVERSION(Arrangement, Arrangement::Start, {Arrangement::Start, "start"},
                       {Arrangement::Center, "center"}, {Arrangement::End, "end"},
                       {Arrangement::SpaceBetween, "spaceBetween"},
                       {Arrangement::SpaceEvenly, "spaceEvenly"},
                       {Arrangement::SpaceAround, "spaceAround"})

std::string ColorSpaceToString(ColorSpace space) {
  switch (space) {
    case ColorSpace::SRGB:
      return "srgb";
    case ColorSpace::DisplayP3:
      return "p3";
    default:
      return "srgb";
  }
}

ColorSpace ColorSpaceFromString(const std::string& str) {
  if (str.empty() || str == "srgb" || str == "sRGB" || str == "SRGB") {
    return ColorSpace::SRGB;
  }
  if (str == "p3" || str == "displayP3" || str == "DisplayP3") {
    return ColorSpace::DisplayP3;
  }
  LOGE("Invalid ColorSpace value: \"%s\"", str.c_str());
  return ColorSpace::SRGB;
}

bool IsValidColorSpaceString(const std::string& str) {
  return str.empty() || str == "srgb" || str == "sRGB" || str == "SRGB" || str == "p3" ||
         str == "displayP3" || str == "DisplayP3";
}

static int FloatToHexByte(float v) {
  int i = static_cast<int>(std::round(v * 255.0f));
  return std::max(0, std::min(255, i));
}

std::string ColorToHexString(const Color& color, bool withAlpha) {
  if (color.colorSpace == ColorSpace::DisplayP3) {
    char buf[64] = {};
    if (withAlpha && color.alpha < 1.0f) {
      snprintf(buf, sizeof(buf), "p3(%g, %g, %g, %g)", color.red, color.green, color.blue,
               color.alpha);
    } else {
      snprintf(buf, sizeof(buf), "p3(%g, %g, %g)", color.red, color.green, color.blue);
    }
    return std::string(buf);
  }
  char buf[10] = {};
  int r = FloatToHexByte(color.red);
  int g = FloatToHexByte(color.green);
  int b = FloatToHexByte(color.blue);
  if (withAlpha && color.alpha < 1.0f) {
    int a = FloatToHexByte(color.alpha);
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
  } else {
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  }
  return std::string(buf);
}

//==============================================================================
// Matrix encoding/decoding
//==============================================================================

std::string MatrixToString(const Matrix& matrix) {
  char buf[256] = {};
  snprintf(buf, sizeof(buf), "%g,%g,%g,%g,%g,%g", matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx,
           matrix.ty);
  return std::string(buf);
}

Matrix MatrixFromString(const std::string& str) {
  Matrix m = {};
  auto values = ParseFloatList(str);
  if (values.size() >= 6) {
    m.a = values[0];
    m.b = values[1];
    m.c = values[2];
    m.d = values[3];
    m.tx = values[4];
    m.ty = values[5];
  } else if (!str.empty()) {
    LOGE("Invalid Matrix value: \"%s\" (expected 6 numbers)", str.c_str());
  }
  return m;
}

//==============================================================================
// String parsing utilities
//==============================================================================

std::vector<float> ParseFloatList(const std::string& str) {
  std::vector<float> result = {};
  result.reserve(8);
  const char* ptr = str.c_str();
  const char* end = ptr + str.size();
  while (ptr < end) {
    // Skip whitespace and commas
    while (ptr < end &&
           (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r' || *ptr == ',')) {
      ++ptr;
    }
    if (ptr >= end) {
      break;
    }
    char* endPtr = nullptr;
    float value = strtof(ptr, &endPtr);
    if (endPtr == ptr) {
      break;
    }
    result.push_back(value);
    ptr = endPtr;
  }
  return result;
}

std::string FloatToString(float value) {
  char buf[32] = {};
  snprintf(buf, sizeof(buf), "%g", value);
  return std::string(buf);
}

Padding PaddingFromString(const std::string& str) {
  auto values = ParseFloatList(str);
  Padding p = {};
  if (values.size() == 1) {
    p.top = values[0];
    p.right = values[0];
    p.bottom = values[0];
    p.left = values[0];
  } else if (values.size() == 2) {
    p.top = values[0];
    p.bottom = values[0];
    p.right = values[1];
    p.left = values[1];
  } else if (values.size() >= 4) {
    p.top = values[0];
    p.right = values[1];
    p.bottom = values[2];
    p.left = values[3];
  } else if (!str.empty()) {
    LOGE("Invalid Padding value: \"%s\" (expected 1, 2, or 4 numbers)", str.c_str());
  }
  return p;
}

std::string PaddingToString(const Padding& padding) {
  if (padding.top == padding.right && padding.right == padding.bottom &&
      padding.bottom == padding.left) {
    return FloatToString(padding.top);
  }
  if (padding.top == padding.bottom && padding.left == padding.right) {
    return FloatToString(padding.top) + "," + FloatToString(padding.right);
  }
  return FloatToString(padding.top) + "," + FloatToString(padding.right) + "," +
         FloatToString(padding.bottom) + "," + FloatToString(padding.left);
}

#undef DEFINE_ENUM_CONVERSION

}  // namespace pagx
