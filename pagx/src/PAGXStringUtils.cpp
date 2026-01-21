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

#include "PAGXStringUtils.h"
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace pagx {

//==============================================================================
// Helper macro for enum string conversions
//==============================================================================

#define DEFINE_ENUM_CONVERSION(EnumType, ...)                                                      \
  static const std::unordered_map<EnumType, std::string> EnumType##ToStringMap = {__VA_ARGS__};    \
  static const std::unordered_map<std::string, EnumType> StringTo##EnumType##Map = [] {            \
    std::unordered_map<std::string, EnumType> map = {};                                            \
    for (const auto& pair : EnumType##ToStringMap) {                                               \
      map[pair.second] = pair.first;                                                               \
    }                                                                                              \
    return map;                                                                                    \
  }();                                                                                             \
  std::string EnumType##ToString(EnumType value) {                                                 \
    auto it = EnumType##ToStringMap.find(value);                                                   \
    return it != EnumType##ToStringMap.end() ? it->second : "";                                    \
  }                                                                                                \
  EnumType EnumType##FromString(const std::string& str) {                                          \
    auto it = StringTo##EnumType##Map.find(str);                                                   \
    return it != StringTo##EnumType##Map.end() ? it->second                                        \
                                               : EnumType##ToStringMap.begin()->first;             \
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
    case NodeType::Group:
      return "Group";
    case NodeType::Repeater:
      return "Repeater";
    default:
      return "Unknown";
  }
}

const char* ColorSourceTypeName(ColorSourceType type) {
  switch (type) {
    case ColorSourceType::SolidColor:
      return "SolidColor";
    case ColorSourceType::LinearGradient:
      return "LinearGradient";
    case ColorSourceType::RadialGradient:
      return "RadialGradient";
    case ColorSourceType::ConicGradient:
      return "ConicGradient";
    case ColorSourceType::DiamondGradient:
      return "DiamondGradient";
    case ColorSourceType::ImagePattern:
      return "ImagePattern";
    default:
      return "Unknown";
  }
}

const char* LayerStyleTypeName(LayerStyleType type) {
  switch (type) {
    case LayerStyleType::DropShadowStyle:
      return "DropShadowStyle";
    case LayerStyleType::InnerShadowStyle:
      return "InnerShadowStyle";
    case LayerStyleType::BackgroundBlurStyle:
      return "BackgroundBlurStyle";
    default:
      return "Unknown";
  }
}

const char* LayerFilterTypeName(LayerFilterType type) {
  switch (type) {
    case LayerFilterType::BlurFilter:
      return "BlurFilter";
    case LayerFilterType::DropShadowFilter:
      return "DropShadowFilter";
    case LayerFilterType::InnerShadowFilter:
      return "InnerShadowFilter";
    case LayerFilterType::BlendFilter:
      return "BlendFilter";
    case LayerFilterType::ColorMatrixFilter:
      return "ColorMatrixFilter";
    default:
      return "Unknown";
  }
}

//==============================================================================
// Enum string conversions
//==============================================================================

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

DEFINE_ENUM_CONVERSION(LayerPlacement,
    {LayerPlacement::Background, "background"},
    {LayerPlacement::Foreground, "foreground"})

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
    {TextAlign::Start, "start"},
    {TextAlign::Center, "center"},
    {TextAlign::End, "end"},
    {TextAlign::Justify, "justify"})

DEFINE_ENUM_CONVERSION(VerticalAlign,
    {VerticalAlign::Top, "top"},
    {VerticalAlign::Center, "center"},
    {VerticalAlign::Bottom, "bottom"})

DEFINE_ENUM_CONVERSION(TextDirection,
    {TextDirection::Horizontal, "horizontal"},
    {TextDirection::Vertical, "vertical"})

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
  if (str == "p3" || str == "displayP3" || str == "DisplayP3") {
    return ColorSpace::DisplayP3;
  }
  return ColorSpace::SRGB;
}

std::string ColorToHexString(const Color& color, bool withAlpha) {
  if (color.colorSpace == ColorSpace::DisplayP3) {
    std::ostringstream oss = {};
    oss << "p3(" << color.red << ", " << color.green << ", " << color.blue;
    if (withAlpha && color.alpha < 1.0f) {
      oss << ", " << color.alpha;
    }
    oss << ")";
    return oss.str();
  }
  auto toHex = [](float v) -> std::string {
    int i = static_cast<int>(std::round(v * 255.0f));
    i = std::max(0, std::min(255, i));
    char buf[3] = {};
    snprintf(buf, sizeof(buf), "%02X", i);
    return std::string(buf);
  };
  std::string result = "#" + toHex(color.red) + toHex(color.green) + toHex(color.blue);
  if (withAlpha && color.alpha < 1.0f) {
    result += toHex(color.alpha);
  }
  return result;
}

#undef DEFINE_ENUM_CONVERSION

}  // namespace pagx
