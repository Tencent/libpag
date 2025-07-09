/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "TextRenderer.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/FontManager.h"
#include "rendering/graphics/Shape.h"
#include "rendering/utils/PathUtil.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/PathMeasure.h"

namespace pag {

#define LINE_GAP_FACTOR 1.2f
#define EMPTY_LINE_WIDTH 1.0f

struct GlyphInfo {
  int glyphIndex = 0;
  std::string name;
  float advance = 0;
  tgfx::Point position = tgfx::Point::Zero();
  tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
};

struct TextLayout {
  float fontTop = 0;
  float fontBottom = 0;
  float lineGap = 0;
  float tracking = 0;
  float firstBaseLine = 0;
  float baselineShift = 0;
  ParagraphJustification justification = ParagraphJustification::LeftJustify;
  tgfx::Rect boxRect = tgfx::Rect::MakeEmpty();
  float glyphScale = 1.0f;
  tgfx::Matrix coordinateMatrix = tgfx::Matrix::I();
};

std::vector<GlyphInfo> CreateGlyphInfos(const std::vector<GlyphHandle>& glyphList) {
  std::vector<GlyphInfo> glyphInfos = {};
  int index = 0;
  for (auto& glyph : glyphList) {
    GlyphInfo info = {};
    info.glyphIndex = index++;
    info.name = glyph->getName();
    info.advance = glyph->getAdvance();
    info.bounds = glyph->getBounds();
    // 当文本为竖版时，需要先转换为横排进行布局计算
    if (glyph->isVertical()) {
      tgfx::Matrix matrix = tgfx::Matrix::I();
      matrix.setRotate(-90);
      matrix.mapRect(&info.bounds);
    }
    glyphInfos.push_back(info);
  }
  return glyphInfos;
}

TextLayout CreateTextLayout(const TextDocument* textDocument,
                            const std::vector<GlyphHandle>& glyphList) {
  TextLayout layout = {};
  auto isVertical = textDocument->direction == TextDirection::Vertical;
  if (!textDocument->boxText) {
    layout.boxRect = tgfx::Rect::MakeEmpty();
    if (isVertical) {
      layout.coordinateMatrix.setRotate(90);
    }
  } else {
    if (isVertical) {
      auto boxRight = textDocument->boxTextPos.x + textDocument->boxTextSize.x;
      layout.boxRect = tgfx::Rect::MakeWH(textDocument->boxTextSize.y, textDocument->boxTextSize.x);
      layout.firstBaseLine = boxRight - textDocument->firstBaseLine;
      layout.coordinateMatrix.setRotate(90);
      layout.coordinateMatrix.postTranslate(boxRight, textDocument->boxTextPos.y);
    } else {
      layout.boxRect = tgfx::Rect::MakeWH(textDocument->boxTextSize.x, textDocument->boxTextSize.y);
      layout.firstBaseLine = textDocument->firstBaseLine - textDocument->boxTextPos.y;
      layout.coordinateMatrix.setTranslate(textDocument->boxTextPos.x, textDocument->boxTextPos.y);
    }
  }
  layout.lineGap = textDocument->leading == 0 ? roundf(textDocument->fontSize * LINE_GAP_FACTOR)
                                              : textDocument->leading;
  layout.tracking = roundf(textDocument->tracking * textDocument->fontSize * 0.001f);
  layout.baselineShift = textDocument->baselineShift;
  layout.justification = textDocument->justification;

  float minAscent = 0, maxDescent = 0;
  for (auto& glyph : glyphList) {
    minAscent = std::min(minAscent, glyph->getAscent());
    maxDescent = std::max(maxDescent, glyph->getDescent());
  }
  // 以 ascent 和 descent 比例计算出行高的上下位置。
  auto lineHeight = textDocument->fontSize * LINE_GAP_FACTOR;
  layout.fontBottom = (maxDescent / (maxDescent - minAscent)) * lineHeight;
  layout.fontTop = layout.fontBottom - lineHeight;
  return layout;
}

static size_t CalculateNextLineIndex(const std::vector<GlyphInfo>& glyphList, size_t index,
                                     float scaleFactor, float maxWidth, float tracking,
                                     float* lineWidth) {
  auto glyphCount = glyphList.size();
  *lineWidth = 0.0f;
  bool hasPrevious = false;
  while (index < glyphCount) {
    auto& glyph = glyphList[index];
    index++;
    if (glyph.name[0] == '\n') {
      break;
    }
    *lineWidth += glyph.advance * scaleFactor;

    if (hasPrevious) {
      *lineWidth += tracking;
    }
    hasPrevious = true;

    auto nextGlyphWidth = index < glyphCount ? glyphList[index].advance * scaleFactor : 0;
    if (*lineWidth + tracking + nextGlyphWidth > maxWidth) {
      break;
    }
  }
  return index;
}

static float CalculateGlyphScale(const TextLayout* layout, const std::vector<GlyphInfo>& glyphInfos,
                                 float oldFontSize, float lineTop, float lineBottom,
                                 float visibleTop, float visibleBottom, float* totalTextLines) {
  auto maxWidth = layout->boxRect.width();
  auto fontSize = oldFontSize;
  while (fontSize > 5) {
    bool canFit = true;
    (*totalTextLines) = 0;
    auto scaleFactor = fontSize / oldFontSize;
    float baseLine = visibleTop - lineTop * scaleFactor;
    float currentTracking = layout->tracking * scaleFactor;
    float currentLineHeight = layout->lineGap * scaleFactor;
    auto glyphCount = glyphInfos.size();
    size_t index = 0;
    while (index < glyphCount) {
      auto lineWidth = 0.0f;
      index = CalculateNextLineIndex(glyphInfos, index, scaleFactor, maxWidth, currentTracking,
                                     &lineWidth);
      // baseline有可能超过boxRect的bottom，导致最后一行文字显示不出来
      if (baseLine + lineBottom * scaleFactor > visibleBottom ||
          baseLine > layout->boxRect.bottom) {
        canFit = false;
        break;
      }
      baseLine += currentLineHeight;
      (*totalTextLines)++;
    }
    if (canFit) {
      break;
    }
    fontSize -= 1;
  }
  return fontSize / oldFontSize;
}

static void AdjustToFitBox(TextLayout* layout, std::vector<GlyphInfo>* glyphInfos, float fontSize) {
  auto boxYOffset = layout->firstBaseLine - layout->boxRect.top;
  auto boxTextLines = floorf((layout->boxRect.height() - boxYOffset) / layout->lineGap) + 1;
  auto visibleTop = layout->firstBaseLine + layout->fontTop;
  auto visibleBottom =
      layout->firstBaseLine + layout->lineGap * (boxTextLines - 1) + layout->fontBottom;
  float totalTextLines = 0;
  auto scaleFactor =
      CalculateGlyphScale(layout, *glyphInfos, fontSize, layout->fontTop, layout->fontBottom,
                          visibleTop, visibleBottom, &totalTextLines);
  if (scaleFactor != 1) {
    layout->glyphScale = scaleFactor;
    layout->firstBaseLine = visibleTop + -layout->fontTop * scaleFactor;
    if (totalTextLines == 1) {
      layout->firstBaseLine += (1 - scaleFactor) * 0.5f * layout->lineGap;
    }
    layout->fontTop *= scaleFactor;
    layout->fontBottom *= scaleFactor;
    layout->tracking *= scaleFactor;
    layout->lineGap *= scaleFactor;
    for (auto& glyph : *glyphInfos) {
      glyph.advance *= scaleFactor;
      glyph.bounds.scale(scaleFactor, scaleFactor);
    }
  } else {
    if (totalTextLines < boxTextLines) {
      layout->firstBaseLine += (boxTextLines - totalTextLines) * layout->lineGap * 0.5f;
    }
  }
}

static float CalculateDrawX(ParagraphJustification justification, float lineWidth, bool isLastLine,
                            const tgfx::Rect& boxRect) {
  float drawX;

  if (boxRect.isEmpty()) {
    switch (justification) {
      case ParagraphJustification::CenterJustify:
        drawX = -lineWidth * 0.5f;
        break;
      case ParagraphJustification::RightJustify:
        drawX = -lineWidth;
        break;
      default:
        drawX = 0.0f;
        break;
    }
    return drawX;
  }

  switch (justification) {
    case ParagraphJustification::CenterJustify:
      drawX = boxRect.centerX() - lineWidth * 0.5f;
      break;
    case ParagraphJustification::RightJustify:
      drawX = boxRect.right - lineWidth;
      break;
    case ParagraphJustification::FullJustifyLastLineCenter:
      if (isLastLine) {
        drawX = boxRect.centerX() - lineWidth * 0.5f;
      } else {
        drawX = boxRect.left;
      }
      break;
    case ParagraphJustification::FullJustifyLastLineRight:
      if (isLastLine) {
        drawX = boxRect.right - lineWidth;
      } else {
        drawX = boxRect.left;
      }
      break;
    default:
      drawX = boxRect.left;
      break;
  }
  return drawX;
}

static float CalculateLetterSpacing(ParagraphJustification justification, float tracking,
                                    float lineWidth, size_t lineGlyphCount, bool isLastLine,
                                    const tgfx::Rect& boxRect) {
  if (boxRect.isEmpty()) {
    return tracking;
  }
  auto lastGlyphIndex = static_cast<float>(lineGlyphCount - 1);
  float justifyTracking =
      (boxRect.width() - lineWidth + lastGlyphIndex * tracking) / lastGlyphIndex;
  float letterSpacing;
  switch (justification) {
    case ParagraphJustification::FullJustifyLastLineLeft:
    case ParagraphJustification::FullJustifyLastLineCenter:
    case ParagraphJustification::FullJustifyLastLineRight:
      if (isLastLine) {
        letterSpacing = tracking;
      } else {
        letterSpacing = justifyTracking;
      }
      break;
    case ParagraphJustification::FullJustifyLastLineFull:
      letterSpacing = justifyTracking;
      break;
    default:
      letterSpacing = tracking;
      break;
  }
  return letterSpacing;
}

static float FindMiniAscent(const std::vector<GlyphInfo*>& line) {
  float minAscent = 0;
  for (auto& info : line) {
    minAscent = std::min(minAscent, info->bounds.top);
  }
  return minAscent;
}

static std::vector<std::vector<GlyphInfo*>> ApplyLayoutToGlyphInfos(
    const TextLayout& layout, std::vector<GlyphInfo>* glyphInfos, tgfx::Rect* bounds,
    float* firstLineMiniAscent) {
  float maxWidth, maxY;
  if (layout.boxRect.isEmpty()) {
    maxWidth = std::numeric_limits<float>::infinity();
    maxY = std::numeric_limits<float>::infinity();
  } else {
    maxWidth = layout.boxRect.width();
    maxY = layout.boxRect.bottom;
  }

  std::vector<std::vector<GlyphInfo*>> lineList = {};
  auto glyphCount = glyphInfos->size();
  size_t index = 0;
  int lineIndex = 0;
  auto baseLine = layout.firstBaseLine;
  while (index < glyphCount) {
    if (baseLine > maxY) {
      break;
    }
    auto lineGlyphStart = index;
    auto lineWidth = 0.0f;
    auto nextLineIndex =
        CalculateNextLineIndex(*glyphInfos, index, 1.0f, maxWidth, layout.tracking, &lineWidth);
    auto hasLineBreaker = (*glyphInfos)[nextLineIndex - 1].name[0] == '\n';  // 换行符
    auto lineGlyphEnd = nextLineIndex - (hasLineBreaker ? 1 : 0);
    index = nextLineIndex;
    bool isLastLine = (index == glyphCount);
    auto drawX = CalculateDrawX(layout.justification, lineWidth, isLastLine || hasLineBreaker,
                                layout.boxRect);
    auto drawY = baseLine - layout.baselineShift;
    float letterSpacing = CalculateLetterSpacing(layout.justification, layout.tracking, lineWidth,
                                                 lineGlyphEnd - lineGlyphStart,
                                                 isLastLine || hasLineBreaker, layout.boxRect);
    std::vector<GlyphInfo*> glyphLine = {};
    for (size_t i = lineGlyphStart; i < lineGlyphEnd; i++) {
      auto& glyph = (*glyphInfos)[i];
      glyph.position.x = drawX;
      glyph.position.y = drawY;
      glyph.bounds.offset(drawX, drawY);
      glyphLine.push_back(&glyph);
      bounds->join(glyph.bounds);
      drawX += glyph.advance + letterSpacing;
    }
    if (!glyphLine.empty()) {
      lineList.push_back(glyphLine);
    } else {
      auto emptyLineBounds = tgfx::Rect::MakeLTRB(
          drawX, drawY + layout.fontTop, drawX + EMPTY_LINE_WIDTH, drawY + layout.fontBottom);
      bounds->join(emptyLineBounds);
    }

    if (firstLineMiniAscent && lineIndex == 0) {
      *firstLineMiniAscent = glyphLine.empty() ? layout.fontTop : FindMiniAscent(glyphLine);
    }

    baseLine += layout.lineGap;

    lineIndex++;
  }
  return lineList;
}

static std::vector<std::vector<GlyphHandle>> ApplyMatrixToGlyphs(
    const TextLayout& layout, const std::vector<std::vector<GlyphInfo*>>& glyphInfoLines,
    std::vector<GlyphHandle>* glyphList) {
  std::vector<std::vector<GlyphHandle>> glyphLines = {};
  for (auto& line : glyphInfoLines) {
    std::vector<GlyphHandle> glyphLine = {};
    for (auto& info : line) {
      auto& glyph = (*glyphList)[info->glyphIndex];
      auto matrix = tgfx::Matrix::I();
      auto pos = info->position;
      layout.coordinateMatrix.mapPoints(&pos, 1);
      matrix.postTranslate(pos.x, pos.y);
      glyph->setScale(layout.glyphScale);
      glyph->setMatrix(matrix);
      glyphLine.push_back(glyph);
    }
    if (!glyphLine.empty()) {
      glyphLines.push_back(glyphLine);
    }
  }
  return glyphLines;
}

static tgfx::Path RenderBackgroundPath(const std::vector<std::vector<GlyphHandle>>& glyphLines,
                                       float margin, float lineTop, float lineBottom,
                                       bool isVertical) {
  tgfx::Path backgroundPath = {};
  std::vector<tgfx::Rect> lineRectList = {};
  for (auto& line : glyphLines) {
    tgfx::Rect lineRect = tgfx::Rect::MakeEmpty();
    for (auto& glyph : line) {
      tgfx::Rect textBounds = {};
      if (isVertical) {
        textBounds = tgfx::Rect::MakeLTRB(-lineBottom, 0, -lineTop, glyph->getAdvance());
      } else {
        textBounds = tgfx::Rect::MakeLTRB(0, lineTop, glyph->getAdvance(), lineBottom);
      }
      glyph->getTotalMatrix().mapRect(&textBounds);
      lineRect.join(textBounds);
    }
    if (!lineRect.isEmpty()) {
      lineRect.outset(margin, margin);
      lineRectList.push_back(lineRect);
    }
  }
  auto lineCount = static_cast<int>(lineRectList.size());
  float topInset = 0, bottomInset = 0, leftInset = 0, rightInset = 0;
  for (int i = 0; i < lineCount; i++) {
    auto& lineRect = lineRectList[i];
    if (i < lineCount - 1) {
      auto nextLineRect = lineRectList[i + 1];
      if (isVertical) {
        if (lineRect.left > nextLineRect.right) {
          leftInset = (lineRect.left - nextLineRect.right) * 0.5f;
        } else {
          leftInset = 0;
        }
      } else {
        if (lineRect.bottom < nextLineRect.top) {
          bottomInset = (nextLineRect.top - lineRect.bottom) * 0.5f;
        } else {
          bottomInset = 0;
        }
      }
    }
    lineRect.left -= leftInset;
    lineRect.right += rightInset;
    lineRect.top -= topInset;
    lineRect.bottom += bottomInset;
    topInset = bottomInset;  // 下一行的topInset是上一行的bottomInset.
    rightInset = leftInset;
    tgfx::Path tempPath = {};
    // 必须round一下避开浮点计算误差，规避 Path 合并的 Bug，否则得到中间断开的不连续矩形。
    lineRect.round();
    tempPath.addRect(lineRect);
    backgroundPath.addPath(tempPath);
  }
  return backgroundPath;
}

std::shared_ptr<Graphic> RenderTextBackground(ID assetID,
                                              const std::vector<std::vector<GlyphHandle>>& lines,
                                              const TextDocument* textDocument) {
  float strokeWidth = textDocument->strokeWidth;
  auto margin = textDocument->fontSize * 0.4f;
  if (margin < strokeWidth) {
    margin = strokeWidth;
  }
  auto isVertical = textDocument->direction == TextDirection::Vertical;
  float lineTop = 0, lineBottom = 0;
  for (auto& line : lines) {
    for (auto& glyph : line) {
      if (isVertical) {
        lineTop = std::min(lineTop, -glyph->getBounds().right);
        lineBottom = std::max(lineBottom, -glyph->getBounds().left);
      } else {
        lineTop = std::min(lineTop, glyph->getBounds().top);
        lineBottom = std::max(lineBottom, glyph->getBounds().bottom);
      }
    }
  }

  auto backgroundPath = RenderBackgroundPath(lines, margin, lineTop, lineBottom, isVertical);
  auto bounds = backgroundPath.getBounds();
  float maxRadius = std::min(bounds.width(), bounds.height()) / 3.0f;
  if (maxRadius >= 25.0f) {
    maxRadius = 25.0f;
  }
  auto effect = tgfx::PathEffect::MakeCorner(maxRadius);
  if (effect) {
    effect->filterPath(&backgroundPath);
  }
  auto graphic = Shape::MakeFrom(assetID, backgroundPath, ToTGFX(textDocument->backgroundColor));
  auto modifier =
      Modifier::MakeBlend(ToAlpha(textDocument->backgroundAlpha), tgfx::BlendMode::SrcOver);
  return Graphic::MakeCompose(graphic, modifier);
}

static std::vector<GlyphHandle> BuildGlyphs(const TextDocument* textDocument) {
  TextPaint textPaint = {};
  if (textDocument->applyFill && textDocument->applyStroke) {
    textPaint.style = TextStyle::StrokeAndFill;
  } else if (textDocument->applyStroke) {
    textPaint.style = TextStyle::Stroke;
  } else {
    textPaint.style = TextStyle::Fill;
  }
  textPaint.fillColor = textDocument->fillColor;
  textPaint.strokeColor = textDocument->strokeColor;
  textPaint.strokeWidth = textDocument->strokeWidth;
  textPaint.strokeOverFill = textDocument->strokeOverFill;
  tgfx::Font textFont = {};
  textFont.setFauxBold(textDocument->fauxBold);
  textFont.setFauxItalic(textDocument->fauxItalic);
  textFont.setSize(textDocument->fontSize);
  textFont.setTypeface(
      FontManager::GetTypefaceWithoutFallback(textDocument->fontFamily, textDocument->fontStyle));
  return Glyph::BuildFromText(textDocument->text, textFont, textPaint,
                              textDocument->direction == TextDirection::Vertical);
}

std::pair<std::vector<std::vector<GlyphHandle>>, tgfx::Rect> GetLines(
    const TextDocument* textDocument, const TextPathOptions* pathOptions) {
  auto glyphList = BuildGlyphs(textDocument);
  // 无论文字朝向，都先按从(0,0)点开始的横向矩形排版。
  // 提取出跟文字朝向无关的 GlyphInfo 列表与 TextLayout,
  // 复用同一套排版规则。如果最终是纵向排版，再把坐标转成纵向坐标应用到 glyphList 上。
  auto glyphInfos = CreateGlyphInfos(glyphList);
  auto textLayout = CreateTextLayout(textDocument, glyphList);
  if (textDocument->boxText) {
    AdjustToFitBox(&textLayout, &glyphInfos, textDocument->fontSize);
  }
  // 文字路径是根据路径重新计算Y轴的位置，这里 firstBaseLine 为零表示，
  // 首行文字的 baseline 从路径上沿法线方向往外排列
  if (pathOptions != nullptr && textDocument->direction != TextDirection::Vertical) {
    textLayout.firstBaseLine = 0;
  }
  tgfx::Rect textBounds = tgfx::Rect::MakeEmpty();
  float firstLineMiniAscent = 0;
  auto glyphInfoLines =
      ApplyLayoutToGlyphInfos(textLayout, &glyphInfos, &textBounds, &firstLineMiniAscent);
  if (pathOptions != nullptr && textDocument->direction != TextDirection::Vertical) {
    // 由于框文本的路径规则是文字顶部从路径开始排列，这里需要计算出首行文字的 ascent(绝对值最大)，
    // 把所有文字沿法线移动 ascent，使得框文字的首行顶部在路径上
    textLayout.coordinateMatrix = textLayout.boxRect.isEmpty()
                                      ? tgfx::Matrix::I()
                                      : tgfx::Matrix::MakeTrans(0, -firstLineMiniAscent);
  }
  auto lines = ApplyMatrixToGlyphs(textLayout, glyphInfoLines, &glyphList);
  textLayout.coordinateMatrix.mapRect(&textBounds);
  return {lines, textBounds};
}

void CalculateTextAscentAndDescent(const TextDocument* textDocument, float* pMinAscent,
                                   float* pMaxDescent) {
  auto glyphList = BuildGlyphs(textDocument);
  float minAscent = 0;
  float maxDescent = 0;
  for (auto& glyph : glyphList) {
    minAscent = std::min(minAscent, glyph->getAscent());
    maxDescent = std::max(maxDescent, glyph->getDescent());
  }

  *pMinAscent = minAscent;
  *pMaxDescent = maxDescent;
}
}  // namespace pag
