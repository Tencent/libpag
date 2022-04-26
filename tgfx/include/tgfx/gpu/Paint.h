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

#include "tgfx/core/Color.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/gpu/Shader.h"

namespace tgfx {
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
   * Retrieves alpha and RGB, unpremultiplied, as four floating point values.
   */
  Color getColor() const {
    return color;
  }

  /**
   * Sets alpha and RGB used when stroking and filling. The color is four floating point values,
   * unpremultiplied.
   */
  void setColor(Color newColor) {
    color = newColor;
  }

  /**
   * Retrieves alpha from the color used when stroking and filling.
   */
  float getAlpha() const {
    return color.alpha;
  }

  /**
   * Replaces alpha, leaving RGB unchanged.
   */
  void setAlpha(float newAlpha) {
    color.alpha = newAlpha;
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
  LineCap getLineCap() const {
    return stroke.cap;
  }

  /**
   * Sets the geometry drawn at the beginning and end of strokes.
   */
  void setLineCap(LineCap cap) {
    stroke.cap = cap;
  }

  /**
   * Returns the geometry drawn at the corners of strokes.
   */
  LineJoin getLineJoin() const {
    return stroke.join;
  }

  /**
   * Sets the geometry drawn at the corners of strokes.
   */
  void setLineJoin(LineJoin join) {
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

  /**
   * Returns the stroke options.
   */
  const Stroke* getStroke() const {
    return &stroke;
  }

  /**
   * Sets the stroke options.
   */
  void setStroke(const Stroke& newStroke) {
    stroke = newStroke;
  }

  /**
   * Returns optional colors used when filling a path if previously set, such as a gradient.
   */
  std::shared_ptr<Shader> getShader() const {
    return shader;
  }

  /**
   * Sets optional colors used when filling a path, such as a gradient. If nullptr, color is used
   * instead.
   */
  void setShader(std::shared_ptr<Shader> newShader) {
    shader = newShader;
  }

 private:
  PaintStyle style = PaintStyle::Fill;
  Color color = Color::Black();
  Stroke stroke = Stroke(0);
  std::shared_ptr<Shader> shader = nullptr;
};
}  // namespace tgfx
