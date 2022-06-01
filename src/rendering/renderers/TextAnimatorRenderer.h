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

#include "TextSelectorRenderer.h"
#include "pag/file.h"
#include "rendering/caches/TextBlock.h"
#include "rendering/graphics/Graphic.h"

namespace pag {

class TextAnimatorRenderer {
 public:
  // 应用动画到Glyphs, 如果含有动画内容返回 true
  static bool ApplyToGlyphs(std::vector<std::vector<GlyphHandle>>& glyphList,
                            const std::vector<TextAnimator*>* animators, Enum justification,
                            Frame layerFrame);
  // 根据序号获取文本动画位置（供AE导出插件在计算firstBaseLine时调用）
  static tgfx::Point GetPositionFromAnimators(const std::vector<TextAnimator*>* animators,
                                              const TextDocument* textDocument, Frame layerFrame,
                                              size_t index, bool* pBiasFlag);
  TextAnimatorRenderer(const TextAnimator* animator, Enum justification, size_t textCount,
                       Frame frame);
  ~TextAnimatorRenderer();

 private:
  // 应用文本动画
  void apply(std::vector<std::vector<GlyphHandle>>& glyphList);
  // 计算一行的字间距总长度
  float calculateTrackingLen(size_t textStart, size_t textEnd);
  // 根据字符序号计算该字符的范围因子
  float calculateFactorByIndex(size_t index, bool* pBiasFlag);
  // 读取字间距信息
  void readTackingInfo(const TextAnimator* animator, Frame frame);
  // 根据序号获取位置（供AE导出插件在计算firstBaseLine时调用）
  tgfx::Point getPositionByIndex(size_t index, bool* pBiasFlag);

  //
  // 动画属性：位置、缩放、旋转、不透明度、字间距
  //
  tgfx::Point position = tgfx::Point::Zero();   // 位置
  tgfx::Point scale = tgfx::Point::Make(1, 1);  // 缩放
  float rotation = 0.0f;                        // 旋转
  float alpha = 1.0f;                           // 不透明度，默认不透明

  float trackingBefore = 0.0f;  // 字间距-之前
  float trackingAfter = 0.0f;   // 字间距-之后

  Enum justification = ParagraphJustification::LeftJustify;

  std::vector<TextSelectorRenderer*> selectorRenderers;
};
}  // namespace pag
