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

#include "TextMoreOption.h"

namespace pag {
std::unique_ptr<BlockConfig> TextMoreOptionTag(TextMoreOptions* moreOption) {
  auto tagConfig = new BlockConfig(TagCode::TextPathOption);
  AddAttribute(tagConfig, &moreOption->anchorPointGrouping, AttributeType::Value,
               static_cast<uint8_t>(AnchorPointGrouping::Character));
  AddAttribute(tagConfig, &moreOption->groupingAlignment, AttributeType::MultiDimensionProperty,
               Point::Zero());
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
