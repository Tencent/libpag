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

#include "OuterGlowStyle.h"

namespace pag {
std::unique_ptr<BlockConfig> OuterGlowStyleTag(OuterGlowStyle* style) {
  auto tagConfig = new BlockConfig(TagCode::OuterGlowStyle);
  AddAttribute(tagConfig, &style->blendMode, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &style->opacity, AttributeType::SimpleProperty, (Opacity)191);
  AddAttribute(tagConfig, &style->noise, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &style->colorType, AttributeType::SimpleProperty,
               static_cast<uint8_t>(GlowColorType::SingleColor));
  Color defaultColor = {0xFF, 0xFF, 0xBE};
  AddAttribute(tagConfig, &style->color, AttributeType::SimpleProperty, defaultColor);
  AddAttribute(tagConfig, &style->colors, AttributeType::SimpleProperty,
               GradientColorHandle(new GradientColor()));
  AddAttribute(tagConfig, &style->gradientSmoothness, AttributeType::SimpleProperty, 1.0f);
  AddAttribute(tagConfig, &style->technique, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(GlowTechniqueType::Softer));
  AddAttribute(tagConfig, &style->spread, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &style->size, AttributeType::SimpleProperty, 5.0f);
  AddAttribute(tagConfig, &style->range, AttributeType::SimpleProperty, 0.5f);
  AddAttribute(tagConfig, &style->jitter, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
