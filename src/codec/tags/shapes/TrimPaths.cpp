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

#include "TrimPaths.h"

namespace pag {
std::unique_ptr<BlockConfig> TrimPathsTag(TrimPathsElement* shape) {
  auto tagConfig = new BlockConfig(TagCode::TrimPaths);
  AddAttribute(tagConfig, &shape->start, AttributeType::SimpleProperty, 0.0f);
  // The default value of the end property should be 1.0f, but for backward compatibility we do not
  // change it.
  AddAttribute(tagConfig, &shape->end, AttributeType::SimpleProperty, 100.0f);
  AddAttribute(tagConfig, &shape->offset, AttributeType::SimpleProperty, 0.0f);
  AddAttribute(tagConfig, &shape->trimType, AttributeType::Value,
               static_cast<uint8_t>(TrimPathsType::Simultaneously));
  return std::unique_ptr<BlockConfig>(tagConfig);
}
}  // namespace pag
