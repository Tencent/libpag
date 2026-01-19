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

#include "Repeater.h"

namespace pag {
std::unique_ptr<BlockConfig> RepeaterTag(RepeaterElement* shape) {
  if (shape->transform == nullptr) {
    shape->transform = new RepeaterTransform();
  }
  auto transform = shape->transform;
  auto tagConfig = new BlockConfig(TagCode::Repeater);
  AddAttribute(tagConfig, &shape->composite, AttributeType::Value,
               static_cast<uint8_t>(RepeaterOrder::Below));
  AddAttribute(tagConfig, &shape->copies, AttributeType::SimpleProperty, 3.0f);
  AddAttribute(tagConfig, &shape->offset, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &transform->anchorPoint, AttributeType::SpatialProperty, Point::Zero());
  AddAttribute(tagConfig, &transform->position, AttributeType::SpatialProperty,
               Point::Make(100, 100));
  AddAttribute(tagConfig, &transform->scale, AttributeType::MultiDimensionProperty,
               Point::Make(1, 1));
  AddAttribute(tagConfig, &transform->rotation, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &transform->startOpacity, AttributeType::SimpleProperty, Opaque);
  AddAttribute(tagConfig, &transform->endOpacity, AttributeType::SimpleProperty, Opaque);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
