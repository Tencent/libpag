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

#include "Text.h"
#include <unordered_map>
#include "base/utils/TGFXCast.h"
#include "pag/file.h"
#include "rendering/caches/RenderCache.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/PathEffect.h"

namespace pag {
static std::unique_ptr<tgfx::Paint> CreateFillPaint(const Glyph* glyph) {
  if (glyph->getStyle() != TextStyle::Fill && glyph->getStyle() != TextStyle::StrokeAndFill) {
    return nullptr;
  }
  auto fillPaint = new tgfx::Paint();
  fillPaint->setStyle(tgfx::PaintStyle::Fill);
  fillPaint->setColor(ToTGFX(glyph->getFillColor()));
  fillPaint->setAlpha(glyph->getAlpha());
  return std::unique_ptr<tgfx::Paint>(fillPaint);
}

static std::unique_ptr<tgfx::Paint> CreateStrokePaint(const Glyph* glyph) {
  if (glyph->getStyle() != TextStyle::Stroke && glyph->getStyle() != TextStyle::StrokeAndFill) {
    return nullptr;
  }
  auto strokePaint = new tgfx::Paint();
  strokePaint->setStyle(tgfx::PaintStyle::Stroke);
  strokePaint->setColor(ToTGFX(glyph->getStrokeColor()));
  strokePaint->setAlpha(glyph->getAlpha());
  strokePaint->setStrokeWidth(glyph->getStrokeWidth());
  return std::unique_ptr<tgfx::Paint>(strokePaint);
}

static std::unique_ptr<TextRun> MakeTextRun(const std::vector<Glyph*>& glyphs) {
  if (glyphs.empty()) {
    return nullptr;
  }
  auto textRun = new TextRun();
  auto firstGlyph = glyphs[0];
  // Creates text paints.
  textRun->paints[0] = CreateFillPaint(firstGlyph).release();
  textRun->paints[1] = CreateStrokePaint(firstGlyph).release();
  auto textStyle = firstGlyph->getStyle();
  if ((textStyle == TextStyle::StrokeAndFill && !firstGlyph->getStrokeOverFill()) ||
      textRun->paints[0] == nullptr) {
    std::swap(textRun->paints[0], textRun->paints[1]);
  }
  // Creates text blob.
  auto noTranslateMatrix = firstGlyph->getTotalMatrix();
  noTranslateMatrix.setTranslateX(0);
  noTranslateMatrix.setTranslateY(0);
  textRun->matrix = noTranslateMatrix;
  noTranslateMatrix.invert(&noTranslateMatrix);
  std::vector<tgfx::GlyphID> glyphIDs = {};
  std::vector<tgfx::Point> positions = {};
  for (auto& glyph : glyphs) {
    glyphIDs.push_back(glyph->getGlyphID());
    auto m = glyph->getTotalMatrix();
    m.postConcat(noTranslateMatrix);
    positions.push_back({m.getTranslateX(), m.getTranslateY()});
  }
  textRun->textFont = firstGlyph->getFont();
  textRun->glyphIDs = glyphIDs;
  textRun->positions = positions;
  return std::unique_ptr<TextRun>(textRun);
}

std::shared_ptr<Graphic> Text::MakeFrom(const std::vector<GlyphHandle>& glyphs,
                                        std::shared_ptr<TextBlock> textBlock,
                                        const tgfx::Rect* calculatedBounds) {
  if (glyphs.empty()) {
    return nullptr;
  }
  // 用 vector 存 key 的目的是让文字叠加顺序固定。
  // 不固定的话叠加区域的像素会不一样，肉眼看不出来，但是测试用例的结果不稳定。
  std::vector<tgfx::BytesKey> styleKeys = {};
  std::unordered_map<tgfx::BytesKey, std::vector<Glyph*>, tgfx::BytesHasher> styleMap = {};
  for (auto& glyph : glyphs) {
    if (!glyph->isVisible()) {
      continue;
    }
    tgfx::BytesKey styleKey = {};
    glyph->computeStyleKey(&styleKey);
    auto size = styleMap.size();
    styleMap[styleKey].push_back(glyph.get());
    if (styleMap.size() != size) {
      styleKeys.push_back(styleKey);
    }
  }
  bool hasAlpha = false;
  tgfx::Rect bounds = calculatedBounds ? *calculatedBounds : tgfx::Rect::MakeEmpty();
  std::vector<TextRun*> textRuns;
  float maxStrokeWidth = 0;
  for (auto& key : styleKeys) {
    auto& glyphList = styleMap[key];
    tgfx::Rect textBounds = tgfx::Rect::MakeEmpty();
    for (auto glyph : glyphList) {
      auto glyphBounds = glyph->getOriginBounds();
      glyph->getTotalMatrix().mapRect(&glyphBounds);
      textBounds.join(glyphBounds);
    }
    if (textBounds.isEmpty()) {
      continue;
    }
    if (calculatedBounds == nullptr) {
      bounds.join(textBounds);
    }
    auto strokeWidth = glyphList[0]->getStrokeWidth();
    if (strokeWidth > maxStrokeWidth) {
      maxStrokeWidth = strokeWidth;
    }
    if (glyphList[0]->getAlpha() != 1.0f) {
      hasAlpha = true;
    }
    auto textRun = MakeTextRun(glyphList).release();
    textRuns.push_back(textRun);
  }
  bounds.outset(maxStrokeWidth, maxStrokeWidth);
  if (textRuns.empty()) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(
      new Text(glyphs, std::move(textRuns), bounds, hasAlpha, std::move(textBlock)));
}

Text::Text(std::vector<GlyphHandle> glyphs, std::vector<TextRun*> textRuns,
           const tgfx::Rect& bounds, bool hasAlpha, std::shared_ptr<TextBlock> textBlock)
    : glyphs(std::move(glyphs)), textRuns(std::move(textRuns)), bounds(bounds), hasAlpha(hasAlpha),
      textBlock(std::move(textBlock)) {
}

Text::~Text() {
  for (auto& textRun : textRuns) {
    delete textRun;
  }
}

void Text::measureBounds(tgfx::Rect* rect) const {
  *rect = bounds;
}

static void ApplyPaintToPath(const tgfx::Paint& paint, tgfx::Path* path) {
  if (paint.getStyle() == tgfx::PaintStyle::Fill || path == nullptr) {
    return;
  }
  auto strokePath = *path;
  auto strokeEffect = tgfx::PathEffect::MakeStroke(*paint.getStroke());
  if (strokeEffect) {
    strokeEffect->applyTo(&strokePath);
  }
  *path = strokePath;
}

bool Text::hitTest(RenderCache*, float x, float y) {
  for (auto& textRun : textRuns) {
    auto local = tgfx::Point::Make(x, y);
    tgfx::Matrix invertMatrix = {};
    if (!textRun->matrix.invert(&invertMatrix)) {
      continue;
    }
    invertMatrix.mapPoints(&local, 1);
    tgfx::Path glyphPath = {};
    int index = 0;
    auto& textFont = textRun->textFont;
    for (auto& glyphID : textRun->glyphIDs) {
      textFont.getGlyphPath(glyphID, &glyphPath);
      auto pos = textRun->positions[index++];
      auto localX = local.x - pos.x;
      auto localY = local.y - pos.y;
      for (auto paint : textRun->paints) {
        if (paint == nullptr) {
          continue;
        }
        auto tempPath = glyphPath;
        ApplyPaintToPath(*paint, &tempPath);
        if (tempPath.contains(localX, localY)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool Text::getPath(tgfx::Path* path) const {
  if (hasAlpha || path == nullptr) {
    return false;
  }
  tgfx::Path textPath = {};
  for (auto& textRun : textRuns) {
    tgfx::Path glyphPath = {};
    int index = 0;
    auto& textFont = textRun->textFont;
    for (auto& glyphID : textRun->glyphIDs) {
      tgfx::Path tempPath = {};
      if (!textFont.getGlyphPath(glyphID, &tempPath)) {
        return false;
      }
      auto pos = textRun->positions[index];
      tempPath.transform(tgfx::Matrix::MakeTrans(pos.x, pos.y));
      glyphPath.addPath(tempPath);
      index++;
    }
    glyphPath.transform(textRun->matrix);
    tgfx::Path tempPath = glyphPath;
    auto firstPaint = textRun->paints[0];
    auto secondPaint = textRun->paints[1];
    ApplyPaintToPath(*firstPaint, &tempPath);
    textPath.addPath(tempPath);
    if (secondPaint != nullptr) {
      tempPath = glyphPath;
      ApplyPaintToPath(*secondPaint, &tempPath);
      textPath.addPath(tempPath);
    }
  }
  path->addPath(textPath);
  return true;
}

void Text::prepare(RenderCache*) const {
}

static std::vector<TextStyle> GetGlyphStyles(const GlyphHandle& glyph) {
  std::vector<TextStyle> styles = {};
  if (glyph->getStyle() == TextStyle::Fill) {
    styles.push_back(TextStyle::Fill);
  } else if (glyph->getStyle() == TextStyle::Stroke) {
    styles.push_back(TextStyle::Stroke);
  } else {
    if (glyph->getStrokeOverFill()) {
      styles.push_back(TextStyle::Fill);
      styles.push_back(TextStyle::Stroke);
    } else {
      styles.push_back(TextStyle::Stroke);
      styles.push_back(TextStyle::Fill);
    }
  }
  return styles;
}

void Text::draw(tgfx::Canvas* canvas, RenderCache* renderCache) const {
  auto textAtlas = renderCache->getTextAtlas(textBlock.get());
  if (textAtlas != nullptr) {
    draw(canvas, textAtlas);
  } else {
    drawTextRuns(canvas, 0);
    drawTextRuns(canvas, 1);
  }
}

struct Parameters {
  size_t imageIndex = 0;
  std::vector<tgfx::Matrix> matrices;
  std::vector<tgfx::Rect> rects;
  std::vector<tgfx::Color> colors;
};

static void Draw(tgfx::Canvas* canvas, const TextAtlas* atlas, const Parameters& parameters) {
  if (parameters.matrices.empty()) {
    return;
  }
  canvas->drawAtlas(
      atlas->getAtlasImage(parameters.imageIndex), &parameters.matrices[0], &parameters.rects[0],
      parameters.colors.empty() ? nullptr : &parameters.colors[0], parameters.matrices.size());
}

static bool RectStaysRectAndNoScale(const tgfx::Matrix& matrix) {
  float m00 = matrix.getScaleX();
  float m01 = matrix.getSkewX();
  float m10 = matrix.getSkewY();
  float m11 = matrix.getScaleY();
  if (m01 != 0 || m10 != 0) {
    return m00 == 0 && m11 == 0 && std::abs(m10) == 1.f && std::abs(m01) == 1.f;
  } else {
    return std::abs(m00) == 1.f && std::abs(m11) == 1.f;
  }
}

void Text::draw(tgfx::Canvas* canvas, const TextAtlas* textAtlas) const {
  Parameters parameters = {};
  auto viewMatrix = canvas->getMatrix();
  canvas->setMatrix(tgfx::Matrix::I());
  float atlasScaleInverted = 1.f / textAtlas->totalScale();
  for (auto& glyph : glyphs) {
    if (!glyph->isVisible()) {
      continue;
    }
    auto styles = GetGlyphStyles(glyph);
    AtlasLocator locator;
    for (auto style : styles) {
      tgfx::BytesKey bytesKey;
      glyph->computeAtlasKey(&bytesKey, style);
      if (!textAtlas->getLocator(bytesKey, &locator)) {
        continue;
      }
      if (parameters.imageIndex != locator.imageIndex) {
        Draw(canvas, textAtlas, parameters);
        parameters = {};
        parameters.imageIndex = locator.imageIndex;
      }
      auto matrix = tgfx::Matrix::I();
      matrix.postTranslate(locator.glyphBounds.x(), locator.glyphBounds.y());
      matrix.postScale(atlasScaleInverted, atlasScaleInverted);
      matrix.postConcat(glyph->getTotalMatrix());
      matrix.postConcat(viewMatrix);
      if (RectStaysRectAndNoScale(matrix)) {
        auto rect = tgfx::Rect::MakeWH(locator.location.width(), locator.location.height());
        matrix.mapRect(&rect);
        matrix.postTranslate(std::round(rect.left) - rect.left, std::round(rect.top) - rect.top);
      }
      parameters.matrices.emplace_back(matrix);
      parameters.rects.emplace_back(locator.location);
      if (glyph->getFont().getTypeface()->hasColor()) {
        auto alpha = canvas->getAlpha();
        canvas->setAlpha(alpha * glyph->getAlpha());
        Draw(canvas, textAtlas, parameters);
        parameters = {};
        canvas->setAlpha(alpha);
      } else {
        auto color =
            ToTGFX(style == TextStyle::Stroke ? glyph->getStrokeColor() : glyph->getFillColor());
        color.alpha *= glyph->getAlpha();
        parameters.colors.emplace_back(color);
      }
    }
  }
  Draw(canvas, textAtlas, parameters);
  canvas->setMatrix(viewMatrix);
}

void Text::drawTextRuns(tgfx::Canvas* canvas, int paintIndex) const {
  auto totalMatrix = canvas->getMatrix();
  for (auto& textRun : textRuns) {
    auto textPaint = textRun->paints[paintIndex];
    if (!textPaint) {
      continue;
    }
    canvas->setMatrix(totalMatrix);
    canvas->concat(textRun->matrix);
    auto ids = &textRun->glyphIDs[0];
    auto positions = &textRun->positions[0];
    canvas->drawGlyphs(ids, positions, textRun->glyphIDs.size(), textRun->textFont, *textPaint);
  }
  canvas->setMatrix(totalMatrix);
}
}  // namespace pag
