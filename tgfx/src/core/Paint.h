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

#include "Stroke.h"
#include "pag/types.h"

namespace pag {
/**
 * Defines attributes for drawing gradient colors.
 */
struct GradientPaint {
  Enum gradientType;
  Point startPoint;
  Point endPoint;
  std::vector<Color> colors;
  std::vector<Opacity> alphas;
  std::vector<float> positions;
};

/**
 * Defines the layout of a RGBAAA format image, which is half RGB, half AAA.
 */
class RGBAAALayout {
 public:
  RGBAAALayout() = default;

  RGBAAALayout(int width, int height, int alphaStartX, int alphaStartY)
      : width(width), height(height), alphaStartX(alphaStartX), alphaStartY(alphaStartY) {
  }

  /**
   * The display width of the image.
   */
  int width = 0;
  /**
   * The display height of the image.
   */
  int height = 0;
  /**
   * The x position of where alpha area begins.
   */
  int alphaStartX = 0;
  /**
   * The y position of where alpha area begins.
   */
  int alphaStartY = 0;
};

/**
 * Defines enumerations for Paint.setStyle().
 */
enum class PaintStyle {
  /**
   * Set to fill geometry.
   */
  Fill,
  /**
   * Set to stroke geometry.
   */
  Stroke
};

/**
 * Paint controls options applied when drawing.
 */
class Paint {
 public:
  /**
   * Sets all Paint contents to their initial values. This is equivalent to replacing Paint with the
   * result of Paint().
   */
  void reset();

  /**
   * Returns whether the geometry is filled, stroked, or filled and stroked.
   */
  PaintStyle getStyle() const {
    return style;
  }

  /**
   * Sets whether the geometry is filled, stroked, or filled and stroked.
   */
  void setStyle(PaintStyle newStyle) {
    style = newStyle;
  }

  /**
   * Retrieves RGB from the color used when stroking and filling.
   */
  Color getColor() const {
    return color;
  }

  /**
   * Sets RGB from the color used when stroking and filling.
   */
  void setColor(Color newColor) {
    color = newColor;
  }

  /**
   * Retrieves alpha from the color used when stroking and filling.
   */
  Opacity getAlpha() const {
    return alpha;
  }

  /**
   * Replaces alpha, leaving RGB unchanged.
   */
  void setAlpha(Opacity newAlpha) {
    alpha = newAlpha;
  }

  /**
   * Returns the thickness of the pen used by Paint to outline the shape.
   * @return  zero for hairline, greater than zero for pen thickness
   */
  float getStrokeWidth() const {
    return stroke.width;
  }

  /**
   * Sets the thickness of the pen used by the paint to outline the shape. Has no effect if width is
   * less than zero.
   * @param width  zero thickness for hairline; greater than zero for pen thickness
   */
  void setStrokeWidth(float width) {
    stroke.width = width;
  }

  /**
   * Returns the geometry drawn at the beginning and end of strokes.
   */
  Enum getLineCap() const {
    return stroke.cap;
  }

  /**
   * Sets the geometry drawn at the beginning and end of strokes.
   */
  void setLineCap(Enum cap) {
    stroke.cap = cap;
  }

  /**
   * Returns the geometry drawn at the corners of strokes.
   */
  Enum getLineJoin() const {
    return stroke.join;
  }

  /**
   * Sets the geometry drawn at the corners of strokes.
   */
  void setLineJoin(Enum join) {
    stroke.join = join;
  }

  /**
   * Returns the limit at which a sharp corner is drawn beveled.
   */
  float getMiterLimit() const {
    return stroke.miterLimit;
  }

  /**
   * Sets the limit at which a sharp corner is drawn beveled.
   */
  void setMiterLimit(float limit) {
    stroke.miterLimit = limit;
  }

  const Stroke* getStroke() const {
    return &stroke;
  }

  void setStroke(const Stroke& newStroke) {
    stroke = newStroke;
  }

 private:
  PaintStyle style = PaintStyle::Fill;
  Color color = Black;
  Opacity alpha = Opaque;
  Stroke stroke = Stroke(0);
};
}  // namespace pag
