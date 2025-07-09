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

#include "TextPathRender.h"
#include "base/utils/MathUtil.h"
#include "pag/types.h"
#include "rendering/utils/PathUtil.h"
#include "tgfx/core/PathMeasure.h"

namespace pag {
struct TextPathLayout {
  ParagraphJustification justification = ParagraphJustification::LeftJustify;
  bool forceAlignment = false;
  bool perpendicularToPath = true;
  float firstMargin = 0;
  float lastMargin = 0;
  float layoutWidth = 0;
  float pathLength = 0;
  PathData pathData;
};

static TextPathLayout CreateTextPathLayout(const TextDocument* textDocument,
                                           const TextPathOptions* pathOptions, Frame frame) {
  auto firstMargin = pathOptions->firstMargin->getValueAt(frame);
  auto lastMargin = pathOptions->lastMargin->getValueAt(frame);
  auto inverted = pathOptions->reversedPath->getValueAt(frame);
  auto forceAlignment = pathOptions->forceAlignment->getValueAt(frame);
  auto perpendicularToPath = pathOptions->perpendicularToPath->getValueAt(frame);

  TextPathLayout textPathLayout = {};
  if (textDocument) {
    // 横排框文本
    if (textDocument->boxText && textDocument->direction != TextDirection::Vertical) {
      textPathLayout.layoutWidth = textDocument->boxTextSize.x;
    }
    textPathLayout.justification = textDocument->justification;
  }

  textPathLayout.forceAlignment = forceAlignment;
  textPathLayout.firstMargin = firstMargin;
  textPathLayout.lastMargin = lastMargin;
  textPathLayout.perpendicularToPath = perpendicularToPath;

  auto pathData = *pathOptions->path->maskPath->getValueAt(frame);
  if (inverted) {
    pathData.reverse();
  }
  textPathLayout.pathData = pathData;

  auto meas = tgfx::PathMeasure::MakeFrom(ToPath(pathData));
  auto pathLength = meas->getLength();
  textPathLayout.pathLength = pathLength;

  return textPathLayout;
}

static float MapToPathPosition(float position, const TextPathLayout& pathLayout) {
  if (pathLayout.forceAlignment ||
      pathLayout.justification == ParagraphJustification::LeftJustify) {
    return position + pathLayout.firstMargin;
  } else if (pathLayout.justification == ParagraphJustification::RightJustify) {
    return position - pathLayout.layoutWidth + pathLayout.pathLength + pathLayout.lastMargin;
  } else if (pathLayout.justification == ParagraphJustification::CenterJustify ||
             pathLayout.justification == ParagraphJustification::FullJustifyLastLineLeft ||
             pathLayout.justification == ParagraphJustification::FullJustifyLastLineRight ||
             pathLayout.justification == ParagraphJustification::FullJustifyLastLineCenter ||
             pathLayout.justification == ParagraphJustification::FullJustifyLastLineFull) {
    return position + (pathLayout.pathLength - pathLayout.layoutWidth) / 2 +
           pathLayout.firstMargin + pathLayout.lastMargin;
  }
  return 0;
}

static void AppendPathData(PathData& pathData, const PathData& source) {
  if (source.verbs.empty()) {
    return;
  }

  auto pointsBegin = source.points.begin();
  auto verbsBegin = source.verbs.begin();

  // 仅当首个路径指令是 move 开头，忽略指令
  if (*verbsBegin == PathDataVerb::MoveTo && !source.points.empty()) {
    verbsBegin++;
    pointsBegin++;
  }

  if (verbsBegin != source.verbs.end()) {
    pathData.verbs.insert(pathData.verbs.end(), verbsBegin, source.verbs.end());
  }

  if (pointsBegin != source.points.end()) {
    pathData.points.insert(pathData.points.end(), pointsBegin, source.points.end());
  }
}

static std::unique_ptr<tgfx::PathMeasure> CreatePath(const PathData& pathData, float first,
                                                     float end) {
  auto pathMeasure = tgfx::PathMeasure::MakeFrom(ToPath(pathData));
  auto pathLength = pathMeasure->getLength();

  if (pathMeasure->isClosed() || (first >= 0 && end <= pathLength)) {
    return pathMeasure;
  }

  auto newPathData = pathData;

  if (first < 0) {
    tgfx::Point pos{};
    tgfx::Point tan{};
    newPathData.clear();

    auto remainLength = abs(first);
    // 获取起点的法线角度
    if (pathMeasure->getPosTan(0, &pos, &tan)) {
      auto x = pos.x - remainLength * tan.x;
      auto y = pos.y - remainLength * tan.y;

      newPathData.moveTo(x, y);
      newPathData.lineTo(pos.x, pos.y);
      AppendPathData(newPathData, pathData);
    }
  }

  if (end > pathLength) {
    tgfx::Point pos{};
    tgfx::Point tan{};

    auto remainLength = end - pathLength;
    // 获取最后一点的法线角度
    if (pathMeasure->getPosTan(pathLength, &pos, &tan)) {
      auto x = pos.x + remainLength * tan.x;
      auto y = pos.y + remainLength * tan.y;
      newPathData.lineTo(x, y);
    }
  }
  return tgfx::PathMeasure::MakeFrom(ToPath(newPathData));
}

static std::pair<float, float> FindMinMaxPathPosition(
    const TextPathLayout& pathLayout, const std::vector<std::vector<GlyphHandle>>& glyphLines) {
  float first = 0;
  float end = pathLayout.pathLength;

  for (const auto& line : glyphLines) {
    for (const auto& glyph : line) {
      auto advance = glyph->getAdvance();
      auto posX = glyph->getMatrix().getTranslateX();
      first = std::min(first, MapToPathPosition(posX, pathLayout));
      end = std::max(end, MapToPathPosition(posX + advance, pathLayout));
    }
  }
  return {first, end};
}

static float CalculateForceAlignmentLetterSpacing(const TextPathLayout& layout,
                                                  const std::vector<GlyphHandle>& line) {
  auto pathLength = std::abs(layout.pathLength + layout.lastMargin - layout.firstMargin);
  float totalGlyphAdvance = 0;
  for (auto& info : line) {
    totalGlyphAdvance += info->getAdvance();
  }
  // 这里count指的是spacing数量即文字间隔数量
  auto count = static_cast<float>(line.size() - 1);
  float spacing = 0;
  if (count > 0) {
    if (layout.firstMargin <= layout.pathLength + layout.lastMargin) {
      // 这里分散对齐的规则是降所有字的Advance加起来,记为totalAdvance,
      // 然后按照路径长度segmentLength减去totalAdvance,取平均值计算出每个字之间的间隔长度
      spacing = (pathLength - totalGlyphAdvance) / count;
    } else {
      // 这里分散对齐的规则是每两字加上间隔的宽度相等设为TL，得到 count * TL - totalAdvance = segmentLength
      //因此得到 index * TL - sum(advance)，segmentLength 加上 totalAdvance 的和取平均值，然后取负值
      spacing = -(totalGlyphAdvance + pathLength) / count;
    }
  }
  return spacing;
}

void TextPathRender::applyForceAlignmentToGlyphs(const std::vector<std::vector<GlyphHandle>>& lines,
                                                 Frame layerFrame) {
  if (pathOptions == nullptr || textDocument == nullptr) {
    return;
  }

  auto textPathLayout = CreateTextPathLayout(textDocument, pathOptions, layerFrame);
  if (!textPathLayout.forceAlignment) {
    return;
  }

  std::vector<std::vector<GlyphHandle>> glyphLines;
  for (const auto& line : lines) {
    float spacing = CalculateForceAlignmentLetterSpacing(textPathLayout, line);
    float posX = 0;
    for (const auto& glyph : line) {
      auto matrix = glyph->getMatrix();
      // 分散对齐会无视排版的段落属性，如左对齐等，需要重新排版，因此这里重新生成 x 轴坐标
      matrix.setTranslateX(posX);
      glyph->setMatrix(matrix);
      posX += glyph->getAdvance() + spacing;
    }
  }
}

void TextPathRender::applyToGlyphs(const std::vector<std::vector<GlyphHandle>>& glyphLines,
                                   Frame layerFrame) {
  auto textPathLayout = CreateTextPathLayout(textDocument, pathOptions, layerFrame);
  // 由于 PathMeasure 的取值范围只能 [0, pathLength] 区间，这里AE上路径需要做两端路径补全，
  // 因此需要结合所有影响路径坐标的因素，找到最小值和最大值，做两端的路径延长
  auto pathBound = FindMinMaxPathPosition(textPathLayout, glyphLines);
  float first = pathBound.first;
  float end = pathBound.second;
  auto pathMeasure = CreatePath(textPathLayout.pathData, first, end);

  if (pathMeasure == nullptr) {
    return;
  }

  bool isPathClosed = pathMeasure->isClosed();
  // 更新路径长度
  auto newPathLength = pathMeasure->getLength();
  float hOffset = 0;
  // 非封闭曲线需要做延长，如果first小于0，表示路径做了 [first, 0] 的路径补全，
  // 坐标映射到原来新的路径上，需要将坐标加上abs(first)，还原的坐标原点
  hOffset += (!isPathClosed && first < 0) ? std::abs(first) : 0;

  float vOffset = 0;

  for (const auto& line : glyphLines) {
    for (const auto& glyph : line) {
      auto matrix = glyph->getMatrix();
      // 文字排版坐标叠加动画位移
      auto position = tgfx::Point::Make(matrix.getTranslateX(), matrix.getTranslateY());
      auto halfWidth = glyph->getAdvance() / 2;
      auto x = MapToPathPosition(position.x + halfWidth, textPathLayout) + hOffset;
      auto y = position.y + vOffset;

      // 闭合路径数值超过[0, pathLength]会被截断,这里需要通过取余做映射到路径两端
      if (x < 0 || x > newPathLength) {
        x = fmod(x + newPathLength, newPathLength);
      }

      tgfx::Point pos{};
      tgfx::Point tan{};
      // pos 表示路径上映射坐标，tan 表示 tan.y 表示正弦 sin，tan.x 表示余弦 cos,
      // 因此 tan.y / tan.x 表示正切,通过计算出来的角度记为 A ,A表示文字顺时针旋转 A 为法线方向
      if (!pathMeasure->getPosTan(x, &pos, &tan)) {
        pos.set(x, y);
        tan.set(1, 0);
      }

      matrix.setTranslateX(0);
      matrix.setTranslateY(0);
      // 垂直于路径开关
      if (textPathLayout.perpendicularToPath) {
        matrix.preRotate(RadiansToDegrees(atan2(tan.y, tan.x)));
        matrix.postTranslate(pos.x - tan.y * y - halfWidth, pos.y + tan.x * y);
      } else {  //  垂直路径关闭时候旋转角度为零，得到 tan.x = 1, tan.y = 0，因此等到以下矩阵
        matrix.postTranslate(pos.x - halfWidth, pos.y + y);
      }
      glyph->setMatrix(matrix);
    }
  }
}

TextPathRender::TextPathRender(const TextDocument* textDocument, const TextPathOptions* pathOptions)
    : textDocument(textDocument), pathOptions(pathOptions) {
}

std::shared_ptr<TextPathRender> TextPathRender::MakeFrom(const TextDocument* textDocument,
                                                         const TextPathOptions* pathOptions) {
  if (pathOptions == nullptr || textDocument == nullptr ||
      textDocument->direction == TextDirection::Vertical) {
    return nullptr;
  }
  return std::shared_ptr<TextPathRender>(new TextPathRender(textDocument, pathOptions));
}

}  // namespace pag
