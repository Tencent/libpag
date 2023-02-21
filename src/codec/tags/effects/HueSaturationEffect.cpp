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

#include "HueSaturationEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> HueSaturationEffectTag(HueSaturationEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::HueSaturationEffect);
  AddAttribute(tagConfig, &effect->channelControl, AttributeType::Value, ChannelControlType::Master);
  for (int i = 0; i < ChannelControlType::Count; i++) {
    AddAttribute(tagConfig, &effect->hue[i], AttributeType::Value, 0.0f);
    AddAttribute(tagConfig, &effect->saturation[i], AttributeType::Value, 0.0f);
    AddAttribute(tagConfig, &effect->lightness[i], AttributeType::Value, 0.0f);
  }
  AddAttribute(tagConfig, &effect->colorize, AttributeType::BitFlag, false);
  AddAttribute(tagConfig, &effect->colorizeHue, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->colorizeSaturation, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->colorizeLightness, AttributeType::SimpleProperty, 0.0f);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
