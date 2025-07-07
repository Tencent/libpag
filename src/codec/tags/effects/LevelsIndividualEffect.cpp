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

#include "LevelsIndividualEffect.h"
#include "EffectCompositingOption.h"

namespace pag {
std::unique_ptr<BlockConfig> LevelsIndividualEffectTag(LevelsIndividualEffect* effect) {
  auto tagConfig = new BlockConfig(TagCode::LevelsIndividualEffect);
  // RGB
  AddAttribute(tagConfig, &effect->inputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->inputWhite, AttributeType::SimpleProperty, 255.0f);
  AddAttribute(tagConfig, &effect->gamma, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &effect->outputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->outputWhite, AttributeType::SimpleProperty, 255.0f);
  // Red
  AddAttribute(tagConfig, &effect->redInputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->redInputWhite, AttributeType::SimpleProperty, 255.0f);
  AddAttribute(tagConfig, &effect->redGamma, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &effect->redOutputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->redOutputWhite, AttributeType::SimpleProperty, 255.0f);
  // Green
  AddAttribute(tagConfig, &effect->greenInputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->greenInputWhite, AttributeType::SimpleProperty, 255.0f);
  AddAttribute(tagConfig, &effect->greenGamma, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &effect->greenOutputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->greenOutputWhite, AttributeType::SimpleProperty, 255.0f);
  // Blue
  AddAttribute(tagConfig, &effect->blueInputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->blueInputWhite, AttributeType::SimpleProperty, 255.0f);
  AddAttribute(tagConfig, &effect->blueGamma, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &effect->blueOutputBlack, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &effect->blueOutputWhite, AttributeType::SimpleProperty, 255.0f);
  EffectCompositingOptionTag(tagConfig, effect);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
