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
#include <cctype>
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
    case NodeType::RangeSelector:
      return "RangeSelector";
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

DEFINE_ENUM_CONVERSION(FilterMode,
    {FilterMode::Nearest, "nearest"},
    {FilterMode::Linear, "linear"})

DEFINE_ENUM_CONVERSION(MipmapMode,
    {MipmapMode::None, "none"},
    {MipmapMode::Nearest, "nearest"},
    {MipmapMode::Linear, "linear"})

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

//==============================================================================
// Matrix encoding/decoding
//==============================================================================

std::string MatrixToString(const Matrix& matrix) {
  std::ostringstream oss = {};
  oss << matrix.a << "," << matrix.b << "," << matrix.c << "," << matrix.d << "," << matrix.tx
      << "," << matrix.ty;
  return oss.str();
}

Matrix MatrixFromString(const std::string& str) {
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

//==============================================================================
// PathData SVG encoding/decoding
//==============================================================================

std::string PathDataToSVGString(const PathData& pathData) {
  std::ostringstream oss;
  oss.precision(6);

  size_t pointIndex = 0;
  const auto& verbs = pathData.verbs();
  const auto& points = pathData.points();

  for (auto verb : verbs) {
    const float* pts = points.data() + pointIndex;
    switch (verb) {
      case PathVerb::Move:
        oss << "M" << pts[0] << " " << pts[1];
        break;
      case PathVerb::Line:
        oss << "L" << pts[0] << " " << pts[1];
        break;
      case PathVerb::Quad:
        oss << "Q" << pts[0] << " " << pts[1] << " " << pts[2] << " " << pts[3];
        break;
      case PathVerb::Cubic:
        oss << "C" << pts[0] << " " << pts[1] << " " << pts[2] << " " << pts[3] << " " << pts[4]
            << " " << pts[5];
        break;
      case PathVerb::Close:
        oss << "Z";
        break;
    }
    pointIndex += PathData::PointsPerVerb(verb) * 2;
  }

  return oss.str();
}

// SVG path parser helper functions
static void SkipWhitespaceAndCommas(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
    ++ptr;
  }
}

static bool ParseNumber(const char*& ptr, const char* end, float& result) {
  SkipWhitespaceAndCommas(ptr, end);
  if (ptr >= end) {
    return false;
  }

  const char* start = ptr;
  if (*ptr == '-' || *ptr == '+') {
    ++ptr;
  }
  bool hasDigits = false;
  while (ptr < end && std::isdigit(*ptr)) {
    ++ptr;
    hasDigits = true;
  }
  if (ptr < end && *ptr == '.') {
    ++ptr;
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
      hasDigits = true;
    }
  }
  if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
    ++ptr;
    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
      ++ptr;
    }
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
    }
  }

  if (!hasDigits) {
    ptr = start;
    return false;
  }

  std::string numStr(start, ptr);
  result = std::stof(numStr);
  return true;
}

static bool ParseFlag(const char*& ptr, const char* end, bool& result) {
  SkipWhitespaceAndCommas(ptr, end);
  if (ptr >= end) {
    return false;
  }
  if (*ptr == '0') {
    result = false;
    ++ptr;
    return true;
  }
  if (*ptr == '1') {
    result = true;
    ++ptr;
    return true;
  }
  return false;
}

// Convert arc to cubic Bezier curves
static void ArcToCubics(PathData& path, float x1, float y1, float rx, float ry, float angle,
                        bool largeArc, bool sweep, float x2, float y2) {
  if (rx == 0 || ry == 0) {
    path.lineTo(x2, y2);
    return;
  }

  rx = std::abs(rx);
  ry = std::abs(ry);

  float radians = angle * 3.14159265358979323846f / 180.0f;
  float cosAngle = std::cos(radians);
  float sinAngle = std::sin(radians);

  float dx2 = (x1 - x2) / 2.0f;
  float dy2 = (y1 - y2) / 2.0f;

  float x1p = cosAngle * dx2 + sinAngle * dy2;
  float y1p = -sinAngle * dx2 + cosAngle * dy2;

  float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
  if (lambda > 1.0f) {
    float sqrtLambda = std::sqrt(lambda);
    rx *= sqrtLambda;
    ry *= sqrtLambda;
  }

  float rx2 = rx * rx;
  float ry2 = ry * ry;
  float x1p2 = x1p * x1p;
  float y1p2 = y1p * y1p;

  float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
  float den = rx2 * y1p2 + ry2 * x1p2;
  float sq = (num < 0 || den == 0) ? 0.0f : std::sqrt(num / den);

  if (largeArc == sweep) {
    sq = -sq;
  }

  float cxp = sq * rx * y1p / ry;
  float cyp = -sq * ry * x1p / rx;

  float cx = cosAngle * cxp - sinAngle * cyp + (x1 + x2) / 2.0f;
  float cy = sinAngle * cxp + cosAngle * cyp + (y1 + y2) / 2.0f;

  auto vectorAngle = [](float ux, float uy, float vx, float vy) -> float {
    float n = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
    if (n == 0) {
      return 0;
    }
    float c = (ux * vx + uy * vy) / n;
    c = std::max(-1.0f, std::min(1.0f, c));
    float angle = std::acos(c);
    if (ux * vy - uy * vx < 0) {
      angle = -angle;
    }
    return angle;
  };

  float theta1 = vectorAngle(1.0f, 0.0f, (x1p - cxp) / rx, (y1p - cyp) / ry);
  float dtheta = vectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx,
                             (-y1p - cyp) / ry);

  if (!sweep && dtheta > 0) {
    dtheta -= 2.0f * 3.14159265358979323846f;
  } else if (sweep && dtheta < 0) {
    dtheta += 2.0f * 3.14159265358979323846f;
  }

  int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (3.14159265358979323846f / 2.0f)));
  float segmentAngle = dtheta / segments;

  float t = std::tan(segmentAngle / 2.0f);
  float alpha = std::sin(segmentAngle) * (std::sqrt(4.0f + 3.0f * t * t) - 1.0f) / 3.0f;

  float currentAngle = theta1;
  float currentX = x1;
  float currentY = y1;

  for (int i = 0; i < segments; ++i) {
    float nextAngle = currentAngle + segmentAngle;

    float cosStart = std::cos(currentAngle);
    float sinStart = std::sin(currentAngle);
    float cosEnd = std::cos(nextAngle);
    float sinEnd = std::sin(nextAngle);

    float ex = cx + rx * (cosAngle * cosEnd - sinAngle * sinEnd);
    float ey = cy + rx * (sinAngle * cosEnd + cosAngle * sinEnd);

    float dx1 = -rx * (cosAngle * sinStart + sinAngle * cosStart);
    float dy1 = -rx * (sinAngle * sinStart - cosAngle * cosStart);
    float dx2m = -rx * (cosAngle * sinEnd + sinAngle * cosEnd);
    float dy2m = -rx * (sinAngle * sinEnd - cosAngle * cosEnd);

    float c1x = currentX + alpha * dx1;
    float c1y = currentY + alpha * dy1;
    float c2x = ex - alpha * dx2m;
    float c2y = ey - alpha * dy2m;

    path.cubicTo(c1x, c1y, c2x, c2y, ex, ey);

    currentAngle = nextAngle;
    currentX = ex;
    currentY = ey;
  }
}

PathData PathDataFromSVGString(const std::string& d) {
  PathData path;
  if (d.empty()) {
    return path;
  }

  const char* ptr = d.c_str();
  const char* end = ptr + d.length();

  float currentX = 0;
  float currentY = 0;
  float startX = 0;
  float startY = 0;
  float lastControlX = 0;
  float lastControlY = 0;
  char lastCommand = 0;

  while (ptr < end) {
    SkipWhitespaceAndCommas(ptr, end);
    if (ptr >= end) {
      break;
    }

    char command = *ptr;
    if (std::isalpha(command)) {
      ++ptr;
    } else {
      command = lastCommand;
    }

    bool isRelative = std::islower(command);
    char upperCommand = std::toupper(command);

    switch (upperCommand) {
      case 'M': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        path.moveTo(x, y);
        currentX = startX = x;
        currentY = startY = y;
        lastCommand = isRelative ? 'l' : 'L';
        break;
      }
      case 'L': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        path.lineTo(x, y);
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'H': {
        float x;
        if (!ParseNumber(ptr, end, x)) {
          break;
        }
        if (isRelative) {
          x += currentX;
        }
        path.lineTo(x, currentY);
        currentX = x;
        lastCommand = command;
        break;
      }
      case 'V': {
        float y;
        if (!ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          y += currentY;
        }
        path.lineTo(currentX, y);
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'C': {
        float x1, y1, x2, y2, x, y;
        if (!ParseNumber(ptr, end, x1) || !ParseNumber(ptr, end, y1) ||
            !ParseNumber(ptr, end, x2) || !ParseNumber(ptr, end, y2) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x1 += currentX;
          y1 += currentY;
          x2 += currentX;
          y2 += currentY;
          x += currentX;
          y += currentY;
        }
        path.cubicTo(x1, y1, x2, y2, x, y);
        lastControlX = x2;
        lastControlY = y2;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'S': {
        float x2, y2, x, y;
        if (!ParseNumber(ptr, end, x2) || !ParseNumber(ptr, end, y2) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x2 += currentX;
          y2 += currentY;
          x += currentX;
          y += currentY;
        }
        float x1 = currentX;
        float y1 = currentY;
        char lastUpper = std::toupper(lastCommand);
        if (lastUpper == 'C' || lastUpper == 'S') {
          x1 = 2 * currentX - lastControlX;
          y1 = 2 * currentY - lastControlY;
        }
        path.cubicTo(x1, y1, x2, y2, x, y);
        lastControlX = x2;
        lastControlY = y2;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'Q': {
        float x1, y1, x, y;
        if (!ParseNumber(ptr, end, x1) || !ParseNumber(ptr, end, y1) ||
            !ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x1 += currentX;
          y1 += currentY;
          x += currentX;
          y += currentY;
        }
        path.quadTo(x1, y1, x, y);
        lastControlX = x1;
        lastControlY = y1;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'T': {
        float x, y;
        if (!ParseNumber(ptr, end, x) || !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        float x1 = currentX;
        float y1 = currentY;
        char lastUpper = std::toupper(lastCommand);
        if (lastUpper == 'Q' || lastUpper == 'T') {
          x1 = 2 * currentX - lastControlX;
          y1 = 2 * currentY - lastControlY;
        }
        path.quadTo(x1, y1, x, y);
        lastControlX = x1;
        lastControlY = y1;
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'A': {
        float rx, ry, arcAngle, x, y;
        bool largeArc, sweep;
        if (!ParseNumber(ptr, end, rx) || !ParseNumber(ptr, end, ry) ||
            !ParseNumber(ptr, end, arcAngle) || !ParseFlag(ptr, end, largeArc) ||
            !ParseFlag(ptr, end, sweep) || !ParseNumber(ptr, end, x) ||
            !ParseNumber(ptr, end, y)) {
          break;
        }
        if (isRelative) {
          x += currentX;
          y += currentY;
        }
        ArcToCubics(path, currentX, currentY, rx, ry, arcAngle, largeArc, sweep, x, y);
        currentX = x;
        currentY = y;
        lastCommand = command;
        break;
      }
      case 'Z': {
        path.close();
        currentX = startX;
        currentY = startY;
        lastCommand = command;
        break;
      }
      default:
        ++ptr;
        break;
    }
  }

  return path;
}

#undef DEFINE_ENUM_CONVERSION

}  // namespace pagx
