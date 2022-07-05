///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "GradientOverlayStyle.h"

namespace pag {
std::unique_ptr<BlockConfig> GradientOverlayStyleTag(GradientOverlayStyle* style) {
  auto tagConfig = new BlockConfig(TagCode::GradientOverlayStyle);
  AddAttribute(tagConfig, &style->blendMode, AttributeType::DiscreteProperty, BlendMode::Normal);
  AddAttribute(tagConfig, &style->opacity, AttributeType::SimpleProperty, Opaque);
  AddAttribute(tagConfig, &style->colors, AttributeType::SimpleProperty,
               GradientColorHandle(new GradientColor()));
  AddAttribute(tagConfig, &style->gradientSmoothness, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &style->angle, AttributeType::SimpleProperty, 90.0f);
  AddAttribute(tagConfig, &style->style, AttributeType::DiscreteProperty, GradientFillType::Linear);
  AddAttribute(tagConfig, &style->reverse, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &style->alignWithLayer, AttributeType::DiscreteProperty, true);
  AddAttribute(tagConfig, &style->scale, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &style->offset, AttributeType::SpatialProperty, Point::Zero());
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
