///////////////////////////////////////////////////////////////////////////////////////////////
//
// Tencent is pleased to support the open source community by making libpag available.
//
// Copyright (C) 2026 Tencent. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the
// License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions
// and limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include "pagx/svg/SVGParserContext.h"

namespace pagx {

/**
 * Converts an SVG <filter> element into PAGX layer filters and styles. The parser walks the
 * primitive list once; each pattern helper returns the number of primitives it consumed so the
 * main loop never has to duplicate cursor arithmetic.
 */
class SVGFilterParser {
 public:
  explicit SVGFilterParser(SVGParserContext* context);

  bool parse(const std::shared_ptr<DOMNode>& filterElement, std::vector<LayerFilter*>& filters,
             std::vector<LayerStyle*>& styles, ShadowOnlyType* outShadowOnlyType);

 private:
  size_t parseNoise(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                    std::vector<LayerFilter*>& filters);
  size_t parseColorMatrixShadow(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                                size_t index, bool shadowOnly, std::vector<LayerFilter*>& filters);
  size_t parseBlurFirstShadow(const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t index,
                              bool shadowOnly, std::vector<LayerFilter*>& filters);
  size_t parseOffsetShadow(const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t index,
                           bool shadowOnly, std::vector<LayerFilter*>& filters,
                           std::vector<LayerStyle*>& styles);
  size_t parseForegroundBlur(const std::vector<std::shared_ptr<DOMNode>>& primitives, size_t index,
                             bool blurMergesWithSource, std::vector<LayerFilter*>& filters);

  bool isInnerShadowComposite(const std::shared_ptr<DOMNode>& node) const;
  bool isMorphologyInput(const std::shared_ptr<DOMNode>& node, float* spread) const;
  bool isBlendOfSourceGraphic(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                              size_t blendIndex) const;
  bool isOffsetSourceInput(const std::vector<std::shared_ptr<DOMNode>>& primitives,
                           size_t offsetIndex) const;

  InnerShadowFilter* createInnerShadow(float offsetX, float offsetY, float blurX, float blurY,
                                       const Color& color, bool shadowOnly);

  SVGParserContext* context_ = nullptr;
};

}  // namespace pagx
