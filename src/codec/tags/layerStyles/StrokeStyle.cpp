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

#include "StrokeStyle.h"

namespace pag {
std::unique_ptr<BlockConfig> StrokeStyleTag(StrokeStyle* style) {
  auto tagConfig = new BlockConfig(TagCode::StrokeStyle);
  AddAttribute(tagConfig, &style->blendMode, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(BlendMode::Normal));
  AddAttribute(tagConfig, &style->color, AttributeType::SimpleProperty, Red);
  AddAttribute(tagConfig, &style->size, AttributeType::SimpleProperty, 3.0f);
  AddAttribute(tagConfig, &style->opacity, AttributeType::SimpleProperty, Opaque);
  AddAttribute(tagConfig, &style->position, AttributeType::DiscreteProperty,
               static_cast<uint8_t>(StrokePosition::Outside));
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
