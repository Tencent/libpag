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

#include "PolyStar.h"

namespace pag {
std::unique_ptr<BlockConfig> PolyStarTag(PolyStarElement* shape) {
  auto tagConfig = new BlockConfig(TagCode::PolyStar);
  AddAttribute(tagConfig, &shape->reversed, AttributeType::BitFlag, false);
  AddAttribute(tagConfig, &shape->polyType, AttributeType::Value,
               static_cast<uint8_t>(PolyStarType::Star));
  AddAttribute(tagConfig, &shape->points, AttributeType::SimpleProperty, 5.0f);
  AddAttribute(tagConfig, &shape->position, AttributeType::SpatialProperty, Point::Zero());
  AddAttribute(tagConfig, &shape->rotation, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &shape->innerRadius, AttributeType::SimpleProperty, 50.0f);
  AddAttribute(tagConfig, &shape->outerRadius, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &shape->innerRoundness, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &shape->outerRoundness, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
