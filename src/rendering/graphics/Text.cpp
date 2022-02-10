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
#include "core/PathEffect.h"
#include "gpu/Canvas.h"
#include "pag/file.h"

namespace pag {
static std::unique_ptr<Paint> CreateFillPaint(const Glyph* glyph) {
  if (glyph->getStyle() != TextStyle::Fill && glyph->getStyle() != TextStyle::StrokeAndFill) {
    return nullptr;
  }
  auto fillPaint = new Paint();
  fillPaint->setStyle(PaintStyle::Fill);
  fillPaint->setColor(glyph->getFillColor());
  fillPaint->setAlpha(glyph->getAlpha());
  return std::unique_ptr<Paint>(fillPaint);
}

static std::unique_ptr<Paint> CreateStrokePaint(const Glyph* glyph) {
  if (glyph->getStyle() != TextStyle::Stroke && glyph->getStyle() != TextStyle::StrokeAndFill) {
    return nullptr;
  }
  auto strokePaint = new Paint();
  strokePaint->setStyle(PaintStyle::Stroke);
  strokePaint->setColor(glyph->getStrokeColor());
  strokePaint->setAlpha(glyph->getAlpha());
  strokePaint->setStrokeWidth(glyph->getStrokeWidth());
  return std::unique_ptr<Paint>(strokePaint);
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
  std::vector<GlyphID> glyphIDs = {};
  std::vector<Point> positions = {};
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
                                        const Rect* calculatedBounds) {
  if (glyphs.empty()) {
    return nullptr;
  }
  // 用 vector 存 key 的目的是让文字叠加顺序固定。
  // 不固定的话叠加区域的像素会不一样，肉眼看不出来，但是测试用例的结果不稳定。
  std::vector<BytesKey> styleKeys = {};
  std::unordered_map<BytesKey, std::vector<Glyph*>, BytesHasher> styleMap = {};
  for (auto& glyph : glyphs) {
    if (!glyph->isVisible()) {
      continue;
    }
    BytesKey styleKey = {};
    glyph->computeStyleKey(&styleKey);
    auto size = styleMap.size();
    styleMap[styleKey].push_back(glyph.get());
    if (styleMap.size() != size) {
      styleKeys.push_back(styleKey);
    }
  }
  bool hasAlpha = false;
  Rect bounds = calculatedBounds ? *calculatedBounds : Rect::MakeEmpty();
  std::vector<TextRun*> textRuns;
  float maxStrokeWidth = 0;
  for (auto& key : styleKeys) {
    auto& glyphList = styleMap[key];
    Rect textBounds = Rect::MakeEmpty();
    for (auto glyph : glyphList) {
      auto glyphBounds = glyph->getBounds();
      glyph->getMatrix().mapRect(&glyphBounds);
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
    if (glyphList[0]->getAlpha() != Opaque) {
      hasAlpha = true;
    }
    auto textRun = MakeTextRun(glyphList).release();
    textRuns.push_back(textRun);
  }
  bounds.outset(maxStrokeWidth, maxStrokeWidth);
  if (textRuns.empty()) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(new Text(textRuns, bounds, hasAlpha));
}

Text::Text(const std::vector<TextRun*>& textRuns, const Rect& bounds, bool hasAlpha)
    : textRuns(textRuns), bounds(bounds), hasAlpha(hasAlpha) {
}

Text::~Text() {
  for (auto& textRun : textRuns) {
    delete textRun;
  }
}

void Text::measureBounds(Rect* rect) const {
  *rect = bounds;
}

static void ApplyPaintToPath(const Paint& paint, Path* path) {
  if (paint.getStyle() == PaintStyle::Fill || path == nullptr) {
    return;
  }
  auto strokePath = *path;
  auto strokeEffect = PathEffect::MakeStroke(*paint.getStroke());
  if (strokeEffect) {
    strokeEffect->applyTo(&strokePath);
  }
  *path = strokePath;
}

bool Text::hitTest(RenderCache*, float x, float y) {
  for (auto& textRun : textRuns) {
    auto local = Point::Make(x, y);
    Matrix invertMatrix = {};
    if (!textRun->matrix.invert(&invertMatrix)) {
      continue;
    }
    invertMatrix.mapPoints(&local, 1);
    Path glyphPath = {};
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

bool Text::getPath(Path* path) const {
  if (hasAlpha || path == nullptr) {
    return false;
  }
  Path textPath = {};
  for (auto& textRun : textRuns) {
    Path glyphPath = {};
    int index = 0;
    auto& textFont = textRun->textFont;
    for (auto& glyphID : textRun->glyphIDs) {
      Path tempPath = {};
      if (!textFont.getGlyphPath(glyphID, &tempPath)) {
        return false;
      }
      auto pos = textRun->positions[index];
      tempPath.transform(Matrix::MakeTrans(pos.x, pos.y));
      glyphPath.addPath(tempPath);
      index++;
    }
    glyphPath.transform(textRun->matrix);
    Path tempPath = glyphPath;
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

void Text::draw(Canvas* canvas, RenderCache*) const {
  drawTextRuns(static_cast<Canvas*>(canvas), 0);
  drawTextRuns(static_cast<Canvas*>(canvas), 1);
}

void Text::drawTextRuns(Canvas* canvas, int paintIndex) const {
  auto totalMatrix = canvas->getMatrix();
  for (auto& textRun : textRuns) {
    auto textPaint = textRun->paints[paintIndex];
    if (!textPaint) {
      continue;
    }
    canvas->setMatrix(totalMatrix);
    canvas->concat(textRun->matrix);
    auto glyphs = &textRun->glyphIDs[0];
    auto positions = &textRun->positions[0];
    canvas->drawGlyphs(glyphs, positions, textRun->glyphIDs.size(), textRun->textFont, *textPaint);
  }
  canvas->setMatrix(totalMatrix);
}
}  // namespace pag
