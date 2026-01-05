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

#include "MotionTileEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> MotionTileEffectTag(MotionTileEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::MotionTileEffect);
  AddAttribute(tagConfig, &effect->tileCenter, AttributeType::SpatialProperty,
               Point::Make(640, 360));
  AddAttribute(tagConfig, &effect->tileWidth, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &effect->tileHeight, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &effect->outputWidth, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &effect->outputHeight, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &effect->mirrorEdges, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &effect->phase, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->horizontalPhaseShift, AttributeType::DiscreteProperty, false);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
