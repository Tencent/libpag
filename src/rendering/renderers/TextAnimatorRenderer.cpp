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

#include "TextAnimatorRenderer.h"
#include "base/utils/TGFXCast.h"

namespace pag {

// 判断是否包含动画
static bool HasAnimator(const std::vector<TextAnimator*>* animators) {
  if (animators != nullptr) {
    for (auto animator : *animators) {
      auto typographyProperties = animator->typographyProperties;
      if (typographyProperties != nullptr) {
        if (typographyProperties->trackingAmount != nullptr ||
            typographyProperties->position != nullptr || typographyProperties->scale != nullptr ||
            typographyProperties->rotation != nullptr || typographyProperties->opacity != nullptr) {
          return true;
        }
      }
    }
  }
  return false;
}

// 根据对齐方式计算（字间距）偏移量
static float CalculateOffsetByJustification(ParagraphJustification justification,
                                            float trackingAnimatorLen) {
  float offset = 0.0f;
  switch (justification) {
    case ParagraphJustification::RightJustify:
      offset = -trackingAnimatorLen;
      break;
    case ParagraphJustification::LeftJustify:
      offset = 0.0f;
      break;
    default:
      offset = -trackingAnimatorLen * 0.5f;
      break;
  }
  return offset;
}

// 统计字符数目
static int CalculateCharactersCount(const std::vector<std::vector<GlyphHandle>>& glyphList) {
  // 这里统计用于字间距的总字符数。
  // 对照AE里字间距范围选择器的统计规则：普通字符和空格计入统计、换行符不计入。
  // 此规则与glyphRunLines里的个数相同：glyphRunLines里也是换行符不进入列表
  // 为什么不用text的字符数，或者run->glyphIndex？因为它们是包括换行的，因此不符合
  int count = 0;
  for (auto& line : glyphList) {
    count += static_cast<int>(line.size());
  }
  return count;
}

bool TextAnimatorRenderer::ApplyToGlyphs(std::vector<std::vector<GlyphHandle>>& glyphList,
                                         const std::vector<TextAnimator*>* animators,
                                         ParagraphJustification justification, Frame layerFrame) {
  if (!HasAnimator(animators)) {
    return false;  // 提前判断如果没有文本动画，就不必要进行下面一堆操作了。
  }
  int count = CalculateCharactersCount(glyphList);
  if (count == 0) {
    return false;  // 如果字符数为0，则提前退出
  }
  for (auto animator : *animators) {
    TextAnimatorRenderer animatorRenderer(animator, justification, count, layerFrame);
    animatorRenderer.apply(glyphList);
  }
  return true;
}

// 根据序号获取文本动画位置（供AE导出插件在计算firstBaseLine时调用）
tgfx::Point TextAnimatorRenderer::GetPositionFromAnimators(
    const std::vector<TextAnimator*>* animators, const TextDocument* textDocument, Frame layerFrame,
    size_t index, bool* pBiasFlag) {
  tgfx::Point ret = {0.0f, 0.0f};
  *pBiasFlag = false;
  if (animators != nullptr) {
    for (auto animator : *animators) {
      TextAnimatorRenderer animatorRenderer(animator, textDocument->justification,
                                            textDocument->text.size(), layerFrame);
      bool biasFlag = false;
      auto point = animatorRenderer.getPositionByIndex(index, &biasFlag);
      *pBiasFlag |= biasFlag;

      ret.x += point.x;
      ret.y += point.y;
    }
  }
  return ret;
}

TextAnimatorRenderer::TextAnimatorRenderer(const TextAnimator* animator,
                                           ParagraphJustification justification, size_t textCount,
                                           Frame frame)
    : justification(justification) {
  // 读取动画属性信息
  auto typographyProperties = animator->typographyProperties;
  if (typographyProperties != nullptr) {
    readTackingInfo(animator, frame);  // 字间距

    if (typographyProperties->position != nullptr) {
      position = ToTGFX(typographyProperties->position->getValueAt(frame));  // 位置
    }
    if (typographyProperties->scale != nullptr) {
      scale = ToTGFX(typographyProperties->scale->getValueAt(frame));  // 缩放
    }
    if (typographyProperties->rotation != nullptr) {
      rotation = typographyProperties->rotation->getValueAt(frame);  // 旋转
    }
    if (typographyProperties->opacity != nullptr) {
      alpha = ToAlpha(typographyProperties->opacity->getValueAt(frame));  // 不透明度
    }
  }

  // 读取范围选择器信息
  for (auto selector : animator->selectors) {
    auto type = static_cast<TextSelectorType>(selector->type());
    if (type == TextSelectorType::Range) {
      auto selectorRenderer =
          new RangeSelectorRenderer(static_cast<TextRangeSelector*>(selector), textCount, frame);
      selectorRenderers.push_back(selectorRenderer);
    } else if (type == TextSelectorType::Wiggly) {
      auto selectorRenderer =
          new WigglySelectorRenderer(static_cast<TextWigglySelector*>(selector), textCount, frame);
      selectorRenderers.push_back(selectorRenderer);
    }
  }
}

TextAnimatorRenderer::~TextAnimatorRenderer() {
  for (auto selectorRenderer : selectorRenderers) {
    delete selectorRenderer;
  }
}

// 读取字间距信息
void TextAnimatorRenderer::readTackingInfo(const TextAnimator* animator, Frame frame) {
  if (animator->typographyProperties->trackingAmount != nullptr) {
    auto trackingType = TextAnimatorTrackingType::BeforeAndAfter;
    if (animator->typographyProperties->trackingType != nullptr) {
      trackingType = animator->typographyProperties->trackingType->getValueAt(frame);
    }
    auto trackingAmount = animator->typographyProperties->trackingAmount->getValueAt(frame);
    switch (trackingType) {
      case TextAnimatorTrackingType::Before:
        trackingBefore = trackingAmount;
        trackingAfter = 0.0f;
        break;
      case TextAnimatorTrackingType::After:
        trackingBefore = 0.0f;
        trackingAfter = trackingAmount;
        break;
      default:  // TextAnimatorTrackingType::AfterAndBefore
        trackingBefore = trackingAmount * 0.5f;
        trackingAfter = trackingAmount * 0.5f;
        break;
    }
  }
}

// 应用动画
void TextAnimatorRenderer::apply(std::vector<std::vector<GlyphHandle>>& glyphList) {
  int index = 0;
  for (auto& line : glyphList) {
    int lineIndex = index;
    int nextLineIndex = lineIndex + static_cast<int>(line.size());
    auto trackingAnimatorLen = calculateTrackingLen(lineIndex, nextLineIndex);
    auto offset = CalculateOffsetByJustification(justification, trackingAnimatorLen);
    for (auto& glyph : line) {
      auto matrix = glyph->getMatrix();
      auto factor = calculateFactorByIndex(index, nullptr);
      // 字间距
      if (index > lineIndex) {  // 行首不加字间距的before部分
        offset += trackingBefore * factor;
      }
      if (!glyph->isVertical()) {
        matrix.postTranslate(offset, 0);
      } else {
        matrix.postTranslate(0, offset);
      }
      offset += trackingAfter * factor;

      // 位置、缩放、旋转
      matrix.postTranslate(position.x * factor, position.y * factor);
      matrix.preScale((scale.x - 1.0f) * factor + 1.0f, (scale.y - 1.0f) * factor + 1.0f);
      matrix.preRotate(rotation * factor);

      // 透明度
      if (factor < 0.0f) {
        factor = 0.0f;  // 透明度的范围不能超过[0，1]，所以限制factor不能为负。
      }
      auto oldAlpha = glyph->getAlpha();
      auto alphaFactor = (alpha - 1.0f) * factor + 1.0f;
      glyph->setAlpha(oldAlpha * alphaFactor);

      glyph->setMatrix(matrix);
      index++;
    }
  }
}

// 计算一行的字间距长度
float TextAnimatorRenderer::calculateTrackingLen(size_t textStart, size_t textEnd) {
  float animatorTrackingLen = 0.0f;
  for (size_t i = textStart; i < textEnd; i++) {
    auto factor = calculateFactorByIndex(i, nullptr);
    if (i > textStart) {  // 不计行首字母前面的间距
      animatorTrackingLen += trackingBefore * factor;
    }
    if (i < textEnd - 1) {  // 不计行尾字母后面的间距
      animatorTrackingLen += trackingAfter * factor;
    }
  }
  return animatorTrackingLen;
}

// 计算某个字符的范围因子
float TextAnimatorRenderer::calculateFactorByIndex(size_t index, bool* pBiasFlag) {
  return TextSelectorRenderer::CalculateFactorFromSelectors(selectorRenderers, index, pBiasFlag);
}

// 根据序号获取位置（供AE导出插件在计算firstBaseLine时调用）
tgfx::Point TextAnimatorRenderer::getPositionByIndex(size_t index, bool* pBiasFlag) {
  auto biasFlag = false;
  auto factor = calculateFactorByIndex(index, &biasFlag);
  if (biasFlag && (position.x != 0.0f || position.y != 0.0f)) {
    *pBiasFlag = true;
  }
  return {position.x * factor, position.y * factor};
}
}  // namespace pag
