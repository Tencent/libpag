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

#include "GlowEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> GlowEffectTag(GlowEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::GlowEffect);
  AddAttribute(tagConfig, &effect->glowThreshold, AttributeType::SimpleProperty,
               static_cast<Percent>(1.0f));
  AddAttribute(tagConfig, &effect->glowRadius, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &effect->glowIntensity, AttributeType::SimpleProperty, 0.0f);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
