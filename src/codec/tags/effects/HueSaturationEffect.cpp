/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "HueSaturationEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> HueSaturationEffectTag(HueSaturationEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::HueSaturationEffect);
  AddAttribute(tagConfig, &effect->channelControl, AttributeType::Value,
               static_cast<uint8_t>(ChannelControlType::Master));
  for (int i = 0; i < static_cast<int>(ChannelControlType::Count); i++) {
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
