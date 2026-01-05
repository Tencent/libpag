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

#include "TextPathOption.h"

namespace pag {
std::unique_ptr<BlockConfig> TextPathOptionTag(TextPathOptions* pathOption) {
  auto tagConfig = new BlockConfig(TagCode::TextPathOption);
  AddAttribute(tagConfig, &pathOption->path, AttributeType::Value, static_cast<MaskData*>(nullptr));
  AddAttribute(tagConfig, &pathOption->reversedPath, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &pathOption->perpendicularToPath, AttributeType::DiscreteProperty, true);
  AddAttribute(tagConfig, &pathOption->forceAlignment, AttributeType::DiscreteProperty, false);
  AddAttribute(tagConfig, &pathOption->firstMargin, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &pathOption->lastMargin, AttributeType::SimpleProperty, 0.0f);
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
