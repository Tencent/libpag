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

#include "MaskTag.h"

namespace pag {
static std::unique_ptr<BlockConfig> MaskTagInternal(MaskData* mask, TagCode tagCode) {
  auto tagConfig = new BlockConfig(tagCode);
  AddAttribute(tagConfig, &mask->id, AttributeType::FixedValue, ZeroID);
  AddAttribute(tagConfig, &mask->inverted, AttributeType::BitFlag, false);
  AddAttribute(tagConfig, &mask->maskMode, AttributeType::Value,
               static_cast<uint8_t>(MaskMode::Add));
  AddAttribute(tagConfig, &mask->maskPath, AttributeType::SimpleProperty,
               PathHandle(new PathData()));
  if (tagCode == TagCode::MaskBlockV2) {
    AddAttribute(tagConfig, &mask->maskFeather, AttributeType::SpatialProperty, Point::Zero());
  }
  AddAttribute(tagConfig, &mask->maskOpacity, AttributeType::SimpleProperty, Opaque);
  AddAttribute(tagConfig, &mask->maskExpansion, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}

std::unique_ptr<BlockConfig> MaskTag(MaskData* mask) {
  return MaskTagInternal(mask, TagCode::MaskBlock);
}

std::unique_ptr<BlockConfig> MaskTagV2(MaskData* mask) {
  return MaskTagInternal(mask, TagCode::MaskBlockV2);
}
}  // namespace pag
