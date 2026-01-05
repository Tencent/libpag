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

#include "TextSelectorRenderer.h"
#include "base/utils/BezierEasing.h"
#include "base/utils/MathUtil.h"

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

static float OverlayFactorByMode(float oldFactor, float factor, TextSelectorMode mode) {
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
  easeHigh = selector->easeHigh->getValueAt(frame);      // 缓和度高
  easeLow = selector->easeLow->getValueAt(frame);        // 缓和度低
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

// 范盛金公式求解一元三次方程 a * x^3 + b * x^2 + c * x + d = 0 (a != 0)，获取实数根
// 返回值列表为空，表示方程没有实数解
static std::vector<double> CalRealSolutionsOfCubicEquation(double a, double b, double c, double d) {
  if (a == 0) {
    return {};
  }

  auto A = b * b - 3 * a * c;
  auto B = b * c - 9 * a * d;
  auto C = c * c - 3 * b * d;
  auto delta = B * B - 4 * A * C;
  std::vector<double> solutions;
  if (A == 0 && B == 0) {
    solutions.emplace_back(-b / (3 * a));
  } else if (delta == 0 && A != 0) {
    auto k = B / A;
    solutions.emplace_back(-b / a + k);
    solutions.emplace_back(-0.5 * k);
  } else if (delta > 0) {
    auto y1 = A * b + 1.5 * a * (-B + sqrt(delta));
    auto y2 = A * b + 1.5 * a * (-B - sqrt(delta));
    solutions.emplace_back((-b - cbrt(y1) - cbrt(y2)) / (3 * a));
  } else if (delta < 0 && A > 0) {
    auto t = (A * b - 1.5 * a * B) / (A * sqrt(A));
    if (-1 < t && t < 1) {
      auto theta = acos(t);
      auto sqrtA = sqrt(A);
      auto cosA = cos(theta / 3);
      auto sinA = sin(theta / 3);
      solutions.emplace_back((-b - 2 * sqrtA * cosA) / (3 * a));
      solutions.emplace_back((-b + sqrtA * (cosA + sqrt(3) * sinA)) / (3 * a));
      solutions.emplace_back((-b + sqrtA * (cosA - sqrt(3) * sinA)) / (3 * a));
    }
  }
  return solutions;
}

static float CalculateRangeFactorTriangle(float textStart, float textEnd, float rangeStart,
                                          float rangeEnd, float easeHigh, float easeLow) {
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
  double x = textCenter;

  if (x < rangeStart || x > rangeEnd) {
    return 0;
  }

  // 四阶贝塞尔曲线的四个控制点
  double x1 = x <= rangeCenter ? rangeStart : rangeEnd, y1 = 0;
  double step = rangeCenter - x1;
  double x2 = easeLow >= 0 ? x1 + easeLow * step : x1;
  double y2 = easeLow >= 0 ? 0 : -easeLow;
  double x3 = easeHigh >= 0 ? rangeCenter - easeHigh * step : rangeCenter;
  double y3 = easeHigh >= 0 ? 1 : 1 + easeHigh;
  double x4 = rangeCenter, y4 = 1;

  // 这里使用方程法而不使用 BezierEasing 方法拟合，是由于使用拟合法需要额外存储分段数据，
  // easeHigh 和easeLow 不变情况下，速度上计算没有多大差异，
  // 使用 easeHigh 和 easeLow 关键帧动画时候会有额外的生成分段数据开销和缓存开销，
  // 因此这里采用方程法由 x 计算 t，由 t 计算 y
  // 由 P = (1-t)^3 * P1 + 3 * (1 - t)^2 * t * P2 + 3 * (1 - t) * t^2 * P3 + t^3 * P4,
  // 推出一元三次方程式 a * t^3 + b * t^2 + c * t + d = 0,
  // 其中 a = -x1 + 3 * x2 - 3 * x3 + x4, b = 3 * x1 - 6 * x2 + 3 * x3,
  // c = -3 * x1 + 3 * x2, d = x1 - x, 求解 t
  auto a = -x1 + 3 * x2 - 3 * x3 + x4;
  auto b = 3 * (x1 - 2 * x2 + x3);
  auto c = 3 * (-x1 + x2);
  auto d = x1 - x;

  double t = 0;
  for (auto solution : CalRealSolutionsOfCubicEquation(a, b, c, d)) {
    // 由于浮点计算有精确度问题，当 x = 0.5, t 会存在略大于1，因此需要做近似计算
    if ((solution >= 0 && solution <= 1) || DoubleNearlyEqual(solution, 1, 1e-6)) {
      t = solution;
      break;
    }
  }

  return static_cast<float>(pow(1 - t, 3) * y1 + 3 * pow(1 - t, 2) * t * y2 +
                            3 * (1 - t) * pow(t, 2) * y3 + pow(t, 3) * y4);
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
  static BezierEasing bezier = BezierEasing(Point::Make(0.5f, 0.0), Point::Make(0.5f, 1.0));
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
      factor =
          CalculateRangeFactorTriangle(textStart, textEnd, rangeStart, rangeEnd, easeHigh, easeLow);
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
