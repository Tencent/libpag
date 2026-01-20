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

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace pagx {

/**
 * A point with x and y coordinates.
 */
struct Point {
  float x = 0;
  float y = 0;

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  bool operator!=(const Point& other) const {
    return !(*this == other);
  }
};

/**
 * A rectangle defined by position and size.
 */
struct Rect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;

  /**
   * Returns a Rect from left, top, right, bottom coordinates.
   */
  static Rect MakeLTRB(float l, float t, float r, float b) {
    return {l, t, r - l, b - t};
  }

  /**
   * Returns a Rect from position and size.
   */
  static Rect MakeXYWH(float x, float y, float w, float h) {
    return {x, y, w, h};
  }

  float left() const {
    return x;
  }

  float top() const {
    return y;
  }

  float right() const {
    return x + width;
  }

  float bottom() const {
    return y + height;
  }

  bool isEmpty() const {
    return width <= 0 || height <= 0;
  }

  void setEmpty() {
    x = y = width = height = 0;
  }

  bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
  }

  bool operator!=(const Rect& other) const {
    return !(*this == other);
  }
};

/**
 * An RGBA color with floating-point components in [0, 1].
 */
struct Color {
  float red = 0;
  float green = 0;
  float blue = 0;
  float alpha = 1;

  /**
   * Returns a Color from a hex value (0xRRGGBB or 0xRRGGBBAA).
   */
  static Color FromHex(uint32_t hex, bool hasAlpha = false);

  /**
   * Returns a Color from RGBA components in [0, 1].
   */
  static Color FromRGBA(float r, float g, float b, float a = 1);

  /**
   * Parses a color string. Supports:
   * - Hex: "#RGB", "#RRGGBB", "#RRGGBBAA"
   * - RGB: "rgb(r,g,b)", "rgba(r,g,b,a)"
   * Returns black if parsing fails.
   */
  static Color Parse(const std::string& str);

  /**
   * Returns the color as a hex string "#RRGGBB" or "#RRGGBBAA".
   */
  std::string toHexString(bool includeAlpha = false) const;

  bool operator==(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
  }

  bool operator!=(const Color& other) const {
    return !(*this == other);
  }
};

/**
 * A 2D affine transformation matrix.
 * Matrix form:
 *   | a  c  tx |
 *   | b  d  ty |
 *   | 0  0  1  |
 */
struct Matrix {
  float a = 1;   // scaleX
  float b = 0;   // skewY
  float c = 0;   // skewX
  float d = 1;   // scaleY
  float tx = 0;  // transX
  float ty = 0;  // transY

  /**
   * Returns the identity matrix.
   */
  static Matrix Identity() {
    return {};
  }

  /**
   * Returns a translation matrix.
   */
  static Matrix Translate(float x, float y);

  /**
   * Returns a scale matrix.
   */
  static Matrix Scale(float sx, float sy);

  /**
   * Returns a rotation matrix (angle in degrees).
   */
  static Matrix Rotate(float degrees);

  /**
   * Parses a matrix string "a,b,c,d,tx,ty".
   */
  static Matrix Parse(const std::string& str);

  /**
   * Returns the matrix as a string "a,b,c,d,tx,ty".
   */
  std::string toString() const;

  /**
   * Returns true if this is the identity matrix.
   */
  bool isIdentity() const {
    return a == 1 && b == 0 && c == 0 && d == 1 && tx == 0 && ty == 0;
  }

  /**
   * Concatenates this matrix with another.
   */
  Matrix operator*(const Matrix& other) const;

  /**
   * Transforms a point by this matrix.
   */
  Point mapPoint(const Point& point) const;

  bool operator==(const Matrix& other) const {
    return a == other.a && b == other.b && c == other.c && d == other.d && tx == other.tx &&
           ty == other.ty;
  }

  bool operator!=(const Matrix& other) const {
    return !(*this == other);
  }
};

//==============================================================================
// Enumerations
//==============================================================================

/**
 * Blend modes for compositing.
 */
enum class BlendMode {
  Normal,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  ColorDodge,
  ColorBurn,
  HardLight,
  SoftLight,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,
  Add
};

/**
 * Line cap styles for strokes.
 */
enum class LineCap {
  Butt,
  Round,
  Square
};

/**
 * Line join styles for strokes.
 */
enum class LineJoin {
  Miter,
  Round,
  Bevel
};

/**
 * Fill rules for paths.
 */
enum class FillRule {
  Winding,
  EvenOdd
};

/**
 * Stroke alignment relative to path.
 */
enum class StrokeAlign {
  Center,
  Inside,
  Outside
};

/**
 * Placement of fill/stroke relative to child layers.
 */
enum class Placement {
  Background,
  Foreground
};

/**
 * Tile modes for patterns and gradients.
 */
enum class TileMode {
  Clamp,
  Repeat,
  Mirror,
  Decal
};

/**
 * Sampling modes for images.
 */
enum class SamplingMode {
  Nearest,
  Linear,
  Mipmap
};

/**
 * Mask types for layer masking.
 */
enum class MaskType {
  Alpha,
  Luminance,
  Contour
};

/**
 * Polystar types.
 */
enum class PolystarType {
  Polygon,
  Star
};

/**
 * Trim path types.
 */
enum class TrimType {
  Separate,
  Continuous
};

/**
 * Path boolean operations.
 */
enum class PathOp {
  Append,
  Union,
  Intersect,
  Xor,
  Difference
};

/**
 * Text horizontal alignment.
 */
enum class TextAlign {
  Left,
  Center,
  Right,
  Justify
};

/**
 * Text vertical alignment.
 */
enum class VerticalAlign {
  Top,
  Center,
  Bottom
};

/**
 * Text overflow handling.
 */
enum class Overflow {
  Clip,
  Visible,
  Ellipsis
};

/**
 * Font style.
 */
enum class FontStyle {
  Normal,
  Italic
};

/**
 * Text path alignment.
 */
enum class TextPathAlign {
  Start,
  Center,
  End
};

/**
 * Range selector unit.
 */
enum class SelectorUnit {
  Index,
  Percentage
};

/**
 * Range selector shape.
 */
enum class SelectorShape {
  Square,
  RampUp,
  RampDown,
  Triangle,
  Round,
  Smooth
};

/**
 * Range selector combination mode.
 */
enum class SelectorMode {
  Add,
  Subtract,
  Intersect,
  Min,
  Max,
  Difference
};

/**
 * Repeater stacking order.
 */
enum class RepeaterOrder {
  BelowOriginal,
  AboveOriginal
};

//==============================================================================
// Enum string conversion utilities
//==============================================================================

std::string BlendModeToString(BlendMode mode);
BlendMode BlendModeFromString(const std::string& str);

std::string LineCapToString(LineCap cap);
LineCap LineCapFromString(const std::string& str);

std::string LineJoinToString(LineJoin join);
LineJoin LineJoinFromString(const std::string& str);

std::string FillRuleToString(FillRule rule);
FillRule FillRuleFromString(const std::string& str);

std::string StrokeAlignToString(StrokeAlign align);
StrokeAlign StrokeAlignFromString(const std::string& str);

std::string PlacementToString(Placement placement);
Placement PlacementFromString(const std::string& str);

std::string TileModeToString(TileMode mode);
TileMode TileModeFromString(const std::string& str);

std::string SamplingModeToString(SamplingMode mode);
SamplingMode SamplingModeFromString(const std::string& str);

std::string MaskTypeToString(MaskType type);
MaskType MaskTypeFromString(const std::string& str);

std::string PolystarTypeToString(PolystarType type);
PolystarType PolystarTypeFromString(const std::string& str);

std::string TrimTypeToString(TrimType type);
TrimType TrimTypeFromString(const std::string& str);

std::string PathOpToString(PathOp op);
PathOp PathOpFromString(const std::string& str);

std::string TextAlignToString(TextAlign align);
TextAlign TextAlignFromString(const std::string& str);

std::string VerticalAlignToString(VerticalAlign align);
VerticalAlign VerticalAlignFromString(const std::string& str);

std::string OverflowToString(Overflow overflow);
Overflow OverflowFromString(const std::string& str);

std::string FontStyleToString(FontStyle style);
FontStyle FontStyleFromString(const std::string& str);

std::string TextPathAlignToString(TextPathAlign align);
TextPathAlign TextPathAlignFromString(const std::string& str);

std::string SelectorUnitToString(SelectorUnit unit);
SelectorUnit SelectorUnitFromString(const std::string& str);

std::string SelectorShapeToString(SelectorShape shape);
SelectorShape SelectorShapeFromString(const std::string& str);

std::string SelectorModeToString(SelectorMode mode);
SelectorMode SelectorModeFromString(const std::string& str);

std::string RepeaterOrderToString(RepeaterOrder order);
RepeaterOrder RepeaterOrderFromString(const std::string& str);

}  // namespace pagx
