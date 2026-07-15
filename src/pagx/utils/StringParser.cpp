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
#include <algorithm>
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
    case NodeType::NoiseStyle:
      return "NoiseStyle";
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
    case NodeType::NoiseFilter:
      return "NoiseFilter";
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

DEFINE_ENUM_CONVERSION(ScaleMode, ScaleMode::LetterBox, {ScaleMode::None, "none"},
                       {ScaleMode::Stretch, "stretch"}, {ScaleMode::LetterBox, "letterBox"},
                       {ScaleMode::Zoom, "zoom"})

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

DEFINE_ENUM_CONVERSION(TextBaseline, TextBaseline::LineBox, {TextBaseline::LineBox, "lineBox"},
                       {TextBaseline::Alphabetic, "alphabetic"})

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

DEFINE_ENUM_CONVERSION(NoiseMode, NoiseMode::Mono, {NoiseMode::Mono, "mono"},
                       {NoiseMode::Duo, "duo"}, {NoiseMode::Multi, "multi"})

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
  return FloatToString(matrix.a) + "," + FloatToString(matrix.b) + "," + FloatToString(matrix.c) +
         "," + FloatToString(matrix.d) + "," + FloatToString(matrix.tx) + "," +
         FloatToString(matrix.ty);
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

namespace {

// Trim ASCII whitespace from both ends of `s`.
std::string TrimAsciiWS(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool StartsWithIgnoreCase(const std::string& s, const char* prefix, size_t prefixLen) {
  if (s.size() < prefixLen) return false;
  for (size_t i = 0; i < prefixLen; ++i) {
    char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    char b = static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
    if (a != b) return false;
  }
  return true;
}

// Reads a leading float from `*ptr` and advances `*ptr` past it. Returns false when no
// number is consumed.
bool ConsumeNumber(const char*& ptr, const char* end, float& out) {
  while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
    ++ptr;
  }
  if (ptr >= end) return false;
  char* numEnd = nullptr;
  float v = std::strtof(ptr, &numEnd);
  if (numEnd == ptr) return false;
  ptr = numEnd;
  out = v;
  return true;
}

// Reads any contiguous run of unit characters (letters / `%` / `_`) at `*ptr` and returns it
// lower-cased. Skips leading whitespace before recording, which is harmless because hsl()
// callers tolerate `120 deg` as well as `120deg`.
std::string ConsumeUnit(const char*& ptr, const char* end) {
  while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
    ++ptr;
  }
  std::string unit;
  while (ptr < end &&
         (std::isalpha(static_cast<unsigned char>(*ptr)) || *ptr == '%' || *ptr == '_')) {
    unit.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*ptr))));
    ++ptr;
  }
  return unit;
}

// Converts a CSS hue token (number plus optional unit) into degrees, normalised into
// [0, 360). `unit` is already lower-cased; an empty unit means degrees per the CSS spec.
float NormaliseHueDegrees(float value, const std::string& unit) {
  float deg = value;
  if (unit.empty() || unit == "deg") {
    deg = value;
  } else if (unit == "rad") {
    deg = value * 180.0f / 3.14159265358979323846f;
  } else if (unit == "grad") {
    deg = value * 0.9f;
  } else if (unit == "turn") {
    deg = value * 360.0f;
  }
  deg = std::fmod(deg, 360.0f);
  if (deg < 0.0f) deg += 360.0f;
  return deg;
}

float HueToRgbChannel(float p, float q, float t) {
  if (t < 0.0f) t += 1.0f;
  if (t > 1.0f) t -= 1.0f;
  if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
  if (t < 1.0f / 2.0f) return q;
  if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
  return p;
}

}  // namespace

bool ParseCSSHSLColor(const std::string& value, Color& out) {
  std::string trimmed = TrimAsciiWS(value);
  if (trimmed.empty()) return false;
  bool isHsla = false;
  size_t prefixLen = 0;
  if (StartsWithIgnoreCase(trimmed, "hsla", 4)) {
    isHsla = true;
    prefixLen = 4;
  } else if (StartsWithIgnoreCase(trimmed, "hsl", 3)) {
    prefixLen = 3;
  } else {
    return false;
  }
  size_t open = trimmed.find('(', prefixLen);
  if (open == std::string::npos) return false;
  size_t close = trimmed.rfind(')');
  if (close == std::string::npos || close <= open) return false;
  std::string inner = trimmed.substr(open + 1, close - open - 1);
  // Detect comma syntax. CSS Color 4 still allows commas in hsl()/hsla(); the modern form is
  // space-separated with an optional `/ alpha` tail. We accept both, distinguishing by whether
  // any top-level comma appears in `inner`.
  bool hasComma = inner.find(',') != std::string::npos;

  float h = 0, s = 0, l = 0, a = 1.0f;
  bool hasAlpha = false;

  if (hasComma) {
    // Replace commas with spaces, then walk through tokens manually so we keep the unit suffix
    // attached to each numeric component.
    std::string spaced = inner;
    bool sawSlash = false;
    for (auto& c : spaced) {
      if (c == ',') c = ' ';
    }
    const char* ptr = spaced.c_str();
    const char* end = ptr + spaced.size();
    float hv = 0;
    if (!ConsumeNumber(ptr, end, hv)) return false;
    std::string hUnit = ConsumeUnit(ptr, end);
    h = NormaliseHueDegrees(hv, hUnit);
    float sv = 0;
    if (!ConsumeNumber(ptr, end, sv)) return false;
    std::string sUnit = ConsumeUnit(ptr, end);
    if (sUnit != "%") {
      // Saturation in the legacy comma syntax is always a percentage. Reject so a stray
      // unitless number does not get silently treated as 0..100.
      return false;
    }
    s = std::clamp(sv, 0.0f, 100.0f) / 100.0f;
    float lv = 0;
    if (!ConsumeNumber(ptr, end, lv)) return false;
    std::string lUnit = ConsumeUnit(ptr, end);
    if (lUnit != "%") return false;
    l = std::clamp(lv, 0.0f, 100.0f) / 100.0f;

    while (ptr < end &&
           (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r' || *ptr == '/')) {
      if (*ptr == '/') sawSlash = true;
      ++ptr;
    }
    if (ptr < end) {
      float av = 0;
      if (!ConsumeNumber(ptr, end, av)) return false;
      std::string aUnit = ConsumeUnit(ptr, end);
      if (aUnit == "%") av /= 100.0f;
      a = std::clamp(av, 0.0f, 1.0f);
      hasAlpha = true;
    }
    (void)sawSlash;
  } else {
    const char* ptr = inner.c_str();
    const char* end = ptr + inner.size();
    float hv = 0;
    if (!ConsumeNumber(ptr, end, hv)) return false;
    std::string hUnit = ConsumeUnit(ptr, end);
    h = NormaliseHueDegrees(hv, hUnit);
    float sv = 0;
    if (!ConsumeNumber(ptr, end, sv)) return false;
    std::string sUnit = ConsumeUnit(ptr, end);
    // Modern syntax: percent suffix is recommended but the unitless form is widely emitted
    // when authors normalise to numeric. Treat unitless 0..100 like a percent.
    if (!sUnit.empty() && sUnit != "%") return false;
    s = std::clamp(sv, 0.0f, 100.0f) / 100.0f;
    float lv = 0;
    if (!ConsumeNumber(ptr, end, lv)) return false;
    std::string lUnit = ConsumeUnit(ptr, end);
    if (!lUnit.empty() && lUnit != "%") return false;
    l = std::clamp(lv, 0.0f, 100.0f) / 100.0f;

    while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
      ++ptr;
    }
    if (ptr < end && *ptr == '/') {
      ++ptr;
      float av = 0;
      if (!ConsumeNumber(ptr, end, av)) return false;
      std::string aUnit = ConsumeUnit(ptr, end);
      if (aUnit == "%") av /= 100.0f;
      a = std::clamp(av, 0.0f, 1.0f);
      hasAlpha = true;
    }
  }
  (void)isHsla;
  (void)hasAlpha;

  // CSS Color 3 HSL -> RGB conversion. Operates on the sRGB color space; downstream callers
  // can re-tag if they need a different colorSpace, but every CSS hsl() value is sRGB by spec.
  float r = l;
  float g = l;
  float b = l;
  if (s > 0.0f) {
    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    float hk = h / 360.0f;
    r = HueToRgbChannel(p, q, hk + 1.0f / 3.0f);
    g = HueToRgbChannel(p, q, hk);
    b = HueToRgbChannel(p, q, hk - 1.0f / 3.0f);
  }
  out.red = std::clamp(r, 0.0f, 1.0f);
  out.green = std::clamp(g, 0.0f, 1.0f);
  out.blue = std::clamp(b, 0.0f, 1.0f);
  out.alpha = std::clamp(a, 0.0f, 1.0f);
  out.colorSpace = ColorSpace::SRGB;
  return true;
}

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
  if (std::isnan(value) || std::isinf(value)) {
    return "0";
  }
  if (value == 0.0f) {
    return "0";
  }
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%g", value);
  std::string s(buf);
  // %g may pick scientific notation for very large or very small magnitudes. Re-emit those
  // in fixed form so the result always matches the schema's decimal patterns. We deliberately
  // do NOT snap small magnitudes to zero here: even sub-pixel residuals (e.g. sin/cos of an
  // angle that almost lines up with a multiple of pi/2) still round-trip through XML losslessly
  // and changing them would alter rendered output for baseline-sensitive callers.
  if (s.find('e') != std::string::npos || s.find('E') != std::string::npos) {
    // Use 9 fractional digits: this matches FLT_DECIMAL_DIG, the minimum precision that
    // guarantees every distinct float value serializes to a distinct decimal string and
    // round-trips back to the same bit pattern. Fewer digits would collapse near-denormal
    // values (e.g. 1e-30f) onto 0 after the scientific-notation branch is rewritten. Trailing
    // zeros are stripped below so typical values stay short.
    snprintf(buf, sizeof(buf), "%.9f", value);
    s = buf;
    auto dot = s.find('.');
    if (dot != std::string::npos) {
      auto last = s.find_last_not_of('0');
      if (last == dot) {
        last--;  // strip trailing dot
      }
      s = s.substr(0, last + 1);
    }
  }
  return s;
}

std::string CoordToString(float value) {
  if (std::isnan(value) || std::isinf(value)) {
    return "0";
  }
  float rounded = std::round(value * 100.0f) / 100.0f;
  if (rounded == 0.0f) {
    rounded = 0.0f;
  }
  char buf[32] = {};
  snprintf(buf, sizeof(buf), "%g", rounded);
  return std::string(buf);
}

std::string CssFloatToString(float value) {
  if (std::isnan(value) || std::isinf(value)) {
    return "0";
  }
  char buf[32] = {};
  snprintf(buf, sizeof(buf), "%.4f", value);
  std::string s(buf);
  if (s.find('.') != std::string::npos) {
    size_t lastNonZero = s.find_last_not_of('0');
    s.erase(lastNonZero + 1);
    if (!s.empty() && s.back() == '.') {
      s.pop_back();
    }
  }
  if (s == "-0") {
    s = "0";
  }
  return s;
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

bool IsValidCustomDataKey(const std::string& key) {
  if (key.empty() || key.front() == '-' || key.back() == '-') {
    return false;
  }
  for (auto c : key) {
    if ((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-') {
      return false;
    }
  }
  return true;
}

#undef DEFINE_ENUM_CONVERSION

}  // namespace pagx
