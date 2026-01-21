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

#include <string>
#include <unordered_map>
#include "pagx/model/Fill.h"
#include "pagx/model/ImagePattern.h"
#include "pagx/model/Layer.h"
#include "pagx/model/MergePath.h"
#include "pagx/model/Polystar.h"
#include "pagx/model/RangeSelector.h"
#include "pagx/model/Repeater.h"
#include "pagx/model/Stroke.h"
#include "pagx/model/TextLayout.h"
#include "pagx/model/TextPath.h"
#include "pagx/model/TrimPath.h"
#include "pagx/model/types/BlendMode.h"
#include "pagx/model/types/Placement.h"
#include "pagx/model/types/TileMode.h"

namespace pagx {

//==============================================================================
// Enum string conversions
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

}  // namespace pagx
