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

#include "pag/file.h"

namespace pag {

class TextSelectorRenderer {
 public:
  static float CalculateFactorFromSelectors(
      const std::vector<TextSelectorRenderer*>& selectorRenderers, size_t index,
      bool* pBiasFlag = nullptr);

  TextSelectorRenderer(size_t textCount, Frame frame) : textCount(textCount), frame(frame) {
  }
  virtual ~TextSelectorRenderer() = default;

  // 叠加选择器
  float overlayFactor(float oldFactor, float factor, bool isFirstSelector);

 protected:
  size_t textCount = 0;
  Frame frame = 0;
  Enum mode = TextSelectorMode::Intersect;  // 模式
  std::vector<int> randomIndexs;

  // 生成随机序号
  void calculateRandomIndexs(uint16_t seed);
  // 计算某个字符的范围因子
  virtual float calculateFactorByIndex(size_t index, bool* pBiasFlag) = 0;
};

class WigglySelectorRenderer : public TextSelectorRenderer {
 public:
  WigglySelectorRenderer(const TextWigglySelector* selector, size_t textCount, Frame frame);
  ~WigglySelectorRenderer() override = default;

 private:
  // 计算某个字符的范围因子
  float calculateFactorByIndex(size_t index, bool* pBiasFlag) override;

  // 摆动选择器参数：模式(在父类里)、最大量、最小量、摆动/秒、关联、时间相位、空间相位
  float maxAmount = 1.0f;  // 最大量
  float minAmount = 1.0f;  // 最小量
  float wigglesPerSecond = 2.0f;
  float correlation = 0.5f;  // 关联
  float temporalPhase = 0;   // 时间相位
  float spatialPhase = 0;    // 空间相位
  uint16_t randomSeed = 0;   // 随机植入
};

class RangeSelectorRenderer : public TextSelectorRenderer {
 public:
  RangeSelectorRenderer(const TextRangeSelector* selector, size_t textCount, Frame frame);
  ~RangeSelectorRenderer() override = default;

 private:
  // 计算某个字符的范围因子
  float calculateFactorByIndex(size_t index, bool* pBiasFlag) override;
  void calculateBiasFlag(bool* pBiasFlag);

  float rangeStart = 0.0f;
  float rangeEnd = 1.0f;  // AE默认范围是(0%-100%)

  float easeHigh = 0.0f;  // 缓和度高(0%-100%)
  float easeLow = 0.0f;   // 缓和度低(0%-100%)

  // 范围选择器的高级选项：模式(在父类里)、数量、形状、随机排序
  float amount = 1.0f;                          // 数量
  Enum shape = TextRangeSelectorShape::Square;  // 形状
  bool randomizeOrder = false;                  // 随机排序
  uint16_t randomSeed = 0;                      // 随机植入
};

}  // namespace pag
