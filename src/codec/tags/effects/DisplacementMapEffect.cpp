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

#include "DisplacementMapEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> DisplacementMapEffectTag(DisplacementMapEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::DisplacementMapEffect);
  AddAttribute(tagConfig, &effect->displacementMapLayer, AttributeType::Value,
               static_cast<Layer*>(nullptr));
  AddAttribute(tagConfig, &effect->useForHorizontalDisplacement, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(DisplacementMapSource::Red));
  AddAttribute(tagConfig, &effect->maxHorizontalDisplacement, AttributeType::SimpleProperty, 5.0f);
  AddAttribute(tagConfig, &effect->useForVerticalDisplacement, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(DisplacementMapSource::Green));
  AddAttribute(tagConfig, &effect->maxVerticalDisplacement, AttributeType::SimpleProperty, 5.0f);
  AddAttribute(tagConfig, &effect->displacementMapBehavior, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(DisplacementMapBehavior::CenterMap));
  AddAttribute(tagConfig, &effect->edgeBehavior, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &effect->expandOutput, AttributeType::DiscreteProperty, true);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
