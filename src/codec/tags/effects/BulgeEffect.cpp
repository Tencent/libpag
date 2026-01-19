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

#include "BulgeEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> BulgeEffectTag(BulgeEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::BulgeEffect);
  AddAttribute(tagConfig, &effect->horizontalRadius, AttributeType::SimpleProperty, 50.0f);
  AddAttribute(tagConfig, &effect->verticalRadius, AttributeType::SimpleProperty, 50.0f);
  AddAttribute(tagConfig, &effect->bulgeCenter, AttributeType::SpatialProperty,
               Point::Make(640.0f, 360.0f));
  AddAttribute(tagConfig, &effect->bulgeHeight, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &effect->taperRadius, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->pinning, AttributeType::DiscreteProperty, false);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
