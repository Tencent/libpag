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

#include "AttributeHelper.h"

namespace pag {
void AddCustomAttribute(BlockConfig* blockConfig, void* target,
                        const std::function<void(DecodeStream*, void*)> reader,
                        const std::function<bool(EncodeStream*, void*)> writer) {
  blockConfig->targets.push_back(target);
  blockConfig->configs.push_back(new CustomAttribute(reader, writer));
}

AttributeFlag ReadAttributeFlag(DecodeStream* stream, const AttributeBase* config) {
  AttributeFlag flag = {};
  auto attributeType = config->attributeType;
  if (attributeType == AttributeType::FixedValue) {
    flag.exist = true;
    return flag;
  }
  flag.exist = stream->readBitBoolean();
  if (!flag.exist || attributeType == AttributeType::Value ||
      attributeType == AttributeType::BitFlag || attributeType == AttributeType::Custom) {
    return flag;
  }
  flag.animatable = stream->readBitBoolean();
  if (!flag.animatable || attributeType != AttributeType::SpatialProperty) {
    return flag;
  }
  flag.hasSpatial = stream->readBitBoolean();
  return flag;
}

void WriteAttributeFlag(EncodeStream* stream, const AttributeFlag& flag,
                        const AttributeBase* config) {
  auto attributeType = config->attributeType;
  if (attributeType == AttributeType::FixedValue) {
    return;
  }
  stream->writeBitBoolean(flag.exist);
  if (!flag.exist || attributeType == AttributeType::Value ||
      attributeType == AttributeType::BitFlag || attributeType == AttributeType::Custom) {
    return;
  }
  stream->writeBitBoolean(flag.animatable);
  if (!flag.animatable || attributeType != AttributeType::SpatialProperty) {
    return;
  }
  stream->writeBitBoolean(flag.hasSpatial);
}
}  // namespace pag
