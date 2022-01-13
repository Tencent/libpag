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

#include "TextSelectorRenderer.h"
#include "base/utils/BezierEasing.h"

namespace pag {

float TextSelectorRenderer::CalculateFactorFromSelectors(
    const std::vector<TextSelectorRenderer*>& selectorRenderers, size_t index, bool* pBiasFlag) {
  float totalFactor = 1.0f;
  bool isFirstSelector = true;
  for (auto selectorRenderer : selectorRenderers) {
    bool biasFlag = false;
    auto factor = selectorRenderer->calculateFactorByIndex(index, &biasFlag);
    if (pBiasFlag != nullptr) {
      *pBiasFlag |= biasFlag;
    }
    totalFactor = selectorRenderer->overlayFactor(totalFactor, factor, isFirstSelector);
    isFirstSelector = false;
  }
  return totalFactor;
}

static float OverlayFactorByMode(float oldFactor, float factor, Enum mode) {
  float newFactor;
  switch (mode) {
    case TextSelectorMode::Subtract:
      if (factor >= 0.0f) {
        newFactor = oldFactor * (1.0f - factor);
      } else {
        newFactor = oldFactor * (-1.0f - factor);
      }
      break;
    case TextSelectorMode::Intersect:
      newFactor = oldFactor * factor;
      break;
    case TextSelectorMode::Min:
      newFactor = std::min(oldFactor, factor);
      break;
    case TextSelectorMode::Max:
      newFactor = std::max(oldFactor, factor);
      break;
    case TextSelectorMode::Difference:
      newFactor = fabs(oldFactor - factor);
      break;
    default:  // TextSelectorMode::Add:
      newFactor = oldFactor + factor;
      break;
  }
  return newFactor;
}

// 叠加选择器
float TextSelectorRenderer::overlayFactor(float oldFactor, float factor, bool isFirstSelector) {
  auto newFactor = 0.0f;
  if (isFirstSelector && mode != TextSelectorMode::Subtract) {
    // 首次且叠加模式不为Subtract时，直接取factor
    // 首次且叠加模式为Subtract时，合并入OverlayFactorByMode计算。（首次时oldFactor外部默认为1.0f）
    newFactor = factor;
  } else {
    newFactor = OverlayFactorByMode(oldFactor, factor, mode);
  }
  if (newFactor < -1.0f) {
    newFactor = -1.0f;
  } else if (newFactor > 1.0f) {
    newFactor = 1.0f;
  }
  return newFactor;
}

//
// 因无法获取AE的随机策略，所以随机的具体值也和AE不一样，但因需要获取准确的FirtBaseline，
// 而Position动画会影响插件里FirtBaseline的获取，所以我们需要得到第一个字符的准确位置，
// 因此，我们需要得到文本动画里第一个字符的随机值。
// 下表即是指示第一个字符的随机值，表中具体取值经AE测试得出。
// 经测试，AE里的随机值仅跟字符数目相关，所以下表（和函数）是通过字符数目获取第一个字符的随机值。
// 具体含义：字符数textCount处于范围[start,end]时，第一字符的随机值为index。
// 其中，因start可以通过上一个end+1获取，所以表里忽略start，仅有end和index。
//
static const struct {
  int end;
  int index;
} randomRanges[] = {
    {2, 0},       // [1,2] = 0
    {4, 2},       // [3,4] = 2
    {5, 4},       // [5,5] = 4
    {20, 5},      // [6,20] = 5
    {69, 20},     // [21,69] = 20
    {127, 69},    // [70,127] = 69
    {206, 127},   // [128,206] = 127
    {10000, 205}  // [207, ...] = 205 后面结束点未测试
};

static int GetRandomIndex(int textCount) {
  for (size_t i = 0; i < sizeof(randomRanges) / sizeof(randomRanges[0]); i++) {
    if (textCount <= randomRanges[i].end) {
      return randomRanges[i].index;
    }
  }
  return 0;
}

void TextSelectorRenderer::calculateRandomIndexs(uint16_t seed) {
  srand(seed);  // 重置随机种子
  std::vector<std::pair<int, int>> randList;
  for (size_t i = 0; i < textCount; i++) {
    randList.push_back(std::make_pair(rand(), i));  // 生成随机数
  }

  // 排序
  std::sort(std::begin(randList), std::end(randList),
            [](const std::pair<int, int>& a, std::pair<int, int>& b) { return a.first < b.first; });

  // 随机数排序后的序号作为真正的顺序
  for (size_t i = 0; i < textCount; i++) {
    randomIndexs.push_back(randList[i].second);
  }

  if (seed == 0 && textCount > 1) {
    // 查表获取位置0的随机序号m
    // 同时，为了避免序号冲突，需要再把0原本的序号赋值给原本取值为m的位置k
    auto m = GetRandomIndex(static_cast<int>(textCount));
    size_t k = 0;
    do {
      if (randomIndexs[k] == m) {
        break;
      }
    } while (++k < textCount);
    std::swap(randomIndexs[0], randomIndexs[k]);
  }
}

// 读取摆动选择器
WigglySelectorRenderer::WigglySelectorRenderer(const TextWigglySelector* selector, size_t textCount,
                                               Frame frame)
    : TextSelectorRenderer(textCount, frame) {
  // 获取属性：模式、最大量、最小量
  mode = selector->mode->getValueAt(frame);                          // 模式
  maxAmount = selector->maxAmount->getValueAt(frame);                // 数量
  minAmount = selector->minAmount->getValueAt(frame);                // 数量
  wigglesPerSecond = selector->wigglesPerSecond->getValueAt(frame);  // 摇摆/秒
  correlation = selector->correlation->getValueAt(frame);            // 关联
  temporalPhase = selector->temporalPhase->getValueAt(frame);        // 时间相位
  spatialPhase = selector->spatialPhase->getValueAt(frame);          // 空间相位
  randomSeed = selector->randomSeed->getValueAt(frame);              // 随机植入
}

// 计算摆动选择器中某个字符的范围因子
float WigglySelectorRenderer::calculateFactorByIndex(size_t index, bool* pBiasFlag) {
  if (textCount == 0) {
    return 0.0f;
  }

  // 这里的公式离复原AE效果还有一定的距离。后续可优化。
  // 经验值也需要优化.
  auto temporalSeed = wigglesPerSecond / 2.0 * (frame + temporalPhase / 30.f) / 24.0f;
  auto spatialSeed = (13.73f * (1.0f - correlation) * index + spatialPhase / 80.0f) / 21.13f;
  auto seed = (spatialSeed + temporalSeed + randomSeed / 3.13f) * 2 * M_PI;
  auto factor = cos(seed) * cos(seed / 7 + M_PI / 5);
  if (factor < -1.0f) {
    factor = -1.0f;
  } else if (factor > 1.0f) {
    factor = 1.0f;
  }

  // 考虑"最大量"/"最小量"的影响
  factor = (factor + 1.0f) / 2 * (maxAmount - minAmount) + minAmount;
  if (pBiasFlag != nullptr) {
    *pBiasFlag = true;  // 摆动选择器计算有误差
  }
  return factor;
}

// 读取范围选择器
RangeSelectorRenderer::RangeSelectorRenderer(const TextRangeSelector* selector, size_t textCount,
                                             Frame frame)
    : TextSelectorRenderer(textCount, frame) {
  // 获取基础属性：开始、结束、偏移
  rangeStart = selector->start->getValueAt(frame);    // 开始
  rangeEnd = selector->end->getValueAt(frame);        // 结束
  auto offset = selector->offset->getValueAt(frame);  // 偏移
  rangeStart += offset;
  rangeEnd += offset;
  if (rangeStart > rangeEnd) {  // 如果开始比结束大，则调换，方便后续处理
    std::swap(rangeStart, rangeEnd);
  }

  // 获取高级属性：模式、数量、形状、随机排序
  mode = selector->mode->getValueAt(frame);              // 模式
  amount = selector->amount->getValueAt(frame);          // 数量
  shape = selector->shape;                               // 形状
  randomizeOrder = selector->randomizeOrder;             // 随机排序
  randomSeed = selector->randomSeed->getValueAt(frame);  // 随机植入

  if (randomizeOrder) {
    // 随机排序开启时，生成随机序号
    calculateRandomIndexs(randomSeed);
  }
}

static float CalculateFactorSquare(float textStart, float textEnd, float rangeStart,
                                   float rangeEnd) {
  //
  // 正方形
  //            ___________
  //           |           |
  //           |           |
  //           |           |
  // __________|           |__________
  //
  if (textStart >= rangeEnd || textEnd <= rangeStart) {
    return 0.0f;
  }
  auto textRange = textEnd - textStart;
  if (textStart < rangeStart) {
    textStart = rangeStart;
  }
  if (textEnd > rangeEnd) {
    textEnd = rangeEnd;
  }
  auto factor = (textEnd - textStart) / textRange;
  return factor;
}

static float CalculateRangeFactorRampUp(float textStart, float textEnd, float rangeStart,
                                        float rangeEnd) {
  //
  // 上斜坡
  //                  __________
  //                 /
  //                /
  //               /
  //              /
  //             /
  // ___________/
  //
  auto textCenter = (textStart + textEnd) * 0.5f;
  auto factor = (textCenter - rangeStart) / (rangeEnd - rangeStart);
  return factor;
}

static float CalculateRangeFactorRampDown(float textStart, float textEnd, float rangeStart,
                                          float rangeEnd) {
  //
  // 下斜坡
  // ___________
  //            \
  //             \
  //              \
  //               \
  //                \
  //                 \_________
  //
  auto textCenter = (textStart + textEnd) * 0.5f;
  auto factor = (rangeEnd - textCenter) / (rangeEnd - rangeStart);
  return factor;
}

static float CalculateRangeFactorTriangle(float textStart, float textEnd, float rangeStart,
                                          float rangeEnd) {
  //
  // 三角形
  //                /\
  //               /  \
  //              /    \
  //             /      \
  //            /        \
  // __________/          \__________
  //
  auto textCenter = (textStart + textEnd) * 0.5f;
  auto rangeCenter = (rangeStart + rangeEnd) * 0.5f;
  float factor;
  if (textCenter < rangeCenter) {
    factor = (textCenter - rangeStart) / (rangeCenter - rangeStart);
  } else {
    factor = (rangeEnd - textCenter) / (rangeEnd - rangeCenter);
  }
  return factor;
}

static float CalculateRangeFactorRound(float textStart, float textEnd, float rangeStart,
                                       float rangeEnd) {
  //
  // 圆形：
  // 利用勾股定理确定高度：B*B = C*C - A*A  （其中 C 为圆的半径）
  //
  //                             ***
  //                       *             *
  //                  *                 /|    *
  //              *                    / |        *
  //           *                      /  |           *
  //         *                     C /   | B            *
  //       *                        /    |                *
  //      *                        /     |                 *
  //      *_______________________/______|_________________*
  //                                  A
  //
  auto textCenter = (textStart + textEnd) * 0.5f;
  if (textCenter >= rangeEnd || textCenter <= rangeStart) {
    return 0.0f;
  }
  auto rangeCenter = (rangeStart + rangeEnd) * 0.5f;
  auto radius = (rangeEnd - rangeStart) * 0.5f;                 // C = radius
  auto xWidth = rangeCenter - textCenter;                       // A = xWidth
  auto yHeight = std::sqrt(radius * radius - xWidth * xWidth);  // B = yHeight
  auto factor = yHeight / radius;
  return factor;
}

static float CalculateRangeFactorSmooth(float textStart, float textEnd, float rangeStart,
                                        float rangeEnd) {
  //
  // 平滑
  // 三次贝塞尔曲线
  //                   _ - - - _
  //                 -           -
  //               -               -
  //             -                   -
  //           _                       _
  //  _ _ _ _ -                           - _ _ _ _
  //
  auto textCenter = (textStart + textEnd) * 0.5f;
  if (textCenter >= rangeEnd || textCenter <= rangeStart) {
    return 0.0f;
  }
  auto rangeCenter = (rangeStart + rangeEnd) * 0.5f;
  float x = 0.0f;
  if (textCenter < rangeCenter) {
    x = (textCenter - rangeStart) / (rangeCenter - rangeStart);
  } else {
    x = (rangeEnd - textCenter) / (rangeEnd - rangeCenter);
  }

  // 根据AE实际数据, 拟合出贝塞尔曲线两个控制点
  // P1、P2的取值分别为(0.5, 0.0)、(0.5, 1.0)
  static BezierEasing bezier = BezierEasing(Point::Make(0.5, 0.0), Point::Make(0.5, 1.0));
  auto factor = bezier.getInterpolation(x);
  return factor;
}

void RangeSelectorRenderer::calculateBiasFlag(bool* pBiasFlag) {
  if (pBiasFlag != nullptr) {
    if (shape == TextRangeSelectorShape::Round || shape == TextRangeSelectorShape::Smooth) {
      *pBiasFlag = true;  // 圆形和平滑计算有误差
    } else if (randomizeOrder && randomSeed != 0 && textCount > 1) {
      *pBiasFlag = true;  // 随机数不为0时有误差
    } else {
      *pBiasFlag = false;
    }
  }
}

// 计算某个字符的范围因子
float RangeSelectorRenderer::calculateFactorByIndex(size_t index, bool* pBiasFlag) {
  if (textCount == 0) {
    return 0.0f;
  }
  if (randomizeOrder) {
    index = randomIndexs[index];  // 从随机过后的列表中获取新的序号。
  }
  auto textStart = static_cast<float>(index) / textCount;
  auto textEnd = static_cast<float>(index + 1) / textCount;
  auto factor = 0.0f;
  switch (shape) {
    case TextRangeSelectorShape::RampUp:  // 上斜坡
      factor = CalculateRangeFactorRampUp(textStart, textEnd, rangeStart, rangeEnd);
      break;
    case TextRangeSelectorShape::RampDown:  // 下斜坡
      factor = CalculateRangeFactorRampDown(textStart, textEnd, rangeStart, rangeEnd);
      break;
    case TextRangeSelectorShape::Triangle:  // 三角形
      factor = CalculateRangeFactorTriangle(textStart, textEnd, rangeStart, rangeEnd);
      break;
    case TextRangeSelectorShape::Round:  // 圆形
      factor = CalculateRangeFactorRound(textStart, textEnd, rangeStart, rangeEnd);
      break;
    case TextRangeSelectorShape::Smooth:  // 平滑
      factor = CalculateRangeFactorSmooth(textStart, textEnd, rangeStart, rangeEnd);
      break;
    default:  // TextRangeSelectorShape::Square  // 正方形
      factor = CalculateFactorSquare(textStart, textEnd, rangeStart, rangeEnd);
      break;
  }
  if (factor < 0.0f) {
    factor = 0.0f;
  } else if (factor > 1.0f) {
    factor = 1.0f;
  }
  factor *= amount;  // 乘以高级选项里的"数量"系数
  calculateBiasFlag(pBiasFlag);
  return factor;
}
}  // namespace pag