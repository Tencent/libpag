/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "tgfx/core/Canvas.h"
#include "tgfx/core/RenderFlags.h"
#include "tgfx/core/SamplingOptions.h"

namespace pag {
class RenderCache;
struct CanvasState;

tgfx::SamplingOptions GetSamplingOptions(const tgfx::Matrix& matrix, tgfx::Image* image);

class Canvas {
 public:
  Canvas(tgfx::Surface* surface, RenderCache* cache);

  Canvas(tgfx::Canvas* canvas, RenderCache* cache);

  RenderCache* getCache() const {
    return cache;
  }

  tgfx::Context* getContext() const;

  tgfx::Surface* getSurface() const;

  uint32_t renderFlags() const;

  void save();

  void restore();

  float getAlpha() const;

  void setAlpha(float value);

  tgfx::BlendMode getBlendMode() const;

  void setBlendMode(tgfx::BlendMode value);

  void translate(float dx, float dy) {
    canvas->translate(dx, dy);
  }

  void scale(float sx, float sy) {
    canvas->scale(sx, sy);
  }

  void rotate(float degrees) {
    canvas->rotate(degrees);
  }

  void rotate(float degress, float px, float py) {
    canvas->rotate(degress, px, py);
  }

  void skew(float sx, float sy) {
    canvas->skew(sx, sy);
  }

  void concat(const tgfx::Matrix& matrix) {
    canvas->concat(matrix);
  }

  tgfx::Matrix getMatrix() const {
    return canvas->getMatrix();
  }

  void setMatrix(const tgfx::Matrix& matrix) {
    canvas->setMatrix(matrix);
  }

  void resetMatrix() {
    canvas->resetMatrix();
  }

  tgfx::Path getTotalClip() const {
    return canvas->getTotalClip();
  }

  void clipRect(const tgfx::Rect& rect) {
    canvas->clipRect(rect);
  }

  void clipPath(const tgfx::Path& path) {
    canvas->clipPath(path);
  }

  void clear() {
    canvas->clear();
  }

  void drawLine(float x0, float y0, float x1, float y1, const tgfx::Paint& paint) {
    canvas->drawLine(x0, y0, x1, y1, createPaint(paint));
  }

  void drawLine(const tgfx::Point& p0, const tgfx::Point& p1, const tgfx::Paint& paint) {
    canvas->drawLine(p0, p1, createPaint(paint));
  }

  void drawRect(const tgfx::Rect& rect, const tgfx::Paint& paint) {
    canvas->drawRect(rect, createPaint(paint));
  }

  void drawOval(const tgfx::Rect& oval, const tgfx::Paint& paint) {
    canvas->drawOval(oval, createPaint(paint));
  }

  void drawCircle(float centerX, float centerY, float radius, const tgfx::Paint& paint) {
    canvas->drawCircle(centerX, centerY, radius, createPaint(paint));
  }

  void drawCircle(const tgfx::Point& center, float radius, const tgfx::Paint& paint) {
    canvas->drawCircle(center.x, center.y, radius, createPaint(paint));
  }

  void drawRoundRect(const tgfx::Rect& rect, float radiusX, float radiusY,
                     const tgfx::Paint& paint) {
    canvas->drawRoundRect(rect, radiusX, radiusY, createPaint(paint));
  }

  void drawRRect(const tgfx::RRect& rRect, const tgfx::Paint& paint) {
    canvas->drawRRect(rRect, paint);
  }

  /**
   * Draws a path using the current clip, matrix, and specified paint.
   */
  void drawPath(const tgfx::Path& path, const tgfx::Paint& paint) {
    canvas->drawPath(path, createPaint(paint));
  }

  void drawImage(std::shared_ptr<tgfx::Image> image, const tgfx::Paint* paint = nullptr) {
    auto realPaint = createPaint(paint);
    auto samplingOptions = GetSamplingOptions(canvas->getMatrix(), image.get());
    canvas->drawImage(std::move(image), samplingOptions, &realPaint);
  }

  void drawImage(std::shared_ptr<tgfx::Image> image, tgfx::SamplingOptions sampling,
                 const tgfx::Paint* paint = nullptr) {
    auto realPaint = createPaint(paint);
    canvas->drawImage(std::move(image), sampling, &realPaint);
  }

  void drawImage(std::shared_ptr<tgfx::Image> image, float x, float y,
                 const tgfx::Paint* paint = nullptr) {
    auto realPaint = createPaint(paint);
    auto samplingOptions = GetSamplingOptions(canvas->getMatrix(), image.get());
    canvas->drawImage(std::move(image), x, y, samplingOptions, &realPaint);
  }

  void drawSimpleText(const std::string& text, float x, float y, const tgfx::Font& font,
                      const tgfx::Paint& paint) {
    canvas->drawSimpleText(text, x, y, font, createPaint(paint));
  }

  void drawGlyphs(const tgfx::GlyphID glyphs[], const tgfx::Point positions[], size_t glyphCount,
                  const tgfx::Font& font, const tgfx::Paint& paint) {
    canvas->drawGlyphs(glyphs, positions, glyphCount, font, createPaint(paint));
  }

  void drawAtlas(std::shared_ptr<tgfx::Image> atlas, const tgfx::Matrix matrix[],
                 const tgfx::Rect tex[], const tgfx::Color colors[], size_t count,
                 tgfx::SamplingOptions sampling = {}, const tgfx::Paint* paint = nullptr) {
    auto realPaint = createPaint(paint);
    canvas->drawAtlas(std::move(atlas), matrix, tex, colors, count, sampling, &realPaint);
  }

  void drawPicture(std::shared_ptr<tgfx::Picture> picture, const tgfx::Paint* paint = nullptr) {
    auto realPaint = createPaint(paint);
    canvas->drawPicture(std::move(picture), nullptr, &realPaint);
  }

 private:
  tgfx::Canvas* canvas = nullptr;
  RenderCache* cache = nullptr;
  std::shared_ptr<CanvasState> state = nullptr;
  std::vector<std::shared_ptr<CanvasState>> savedStateList = {};

  const tgfx::Paint createPaint(const tgfx::Paint& paint) const {
    return createPaint(&paint);
  }

  const tgfx::Paint createPaint(const tgfx::Paint* paint) const;
};
}  // namespace pag
