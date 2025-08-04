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

#include "Dashes.h"

namespace pag {
static const AttributeConfig<float> dashOffsetConfig = {AttributeType::SimpleProperty, 0.0f};
static const AttributeConfig<float> dashConfig = {AttributeType::SimpleProperty, 10.0f};

Property<float>* ReadDashes(DecodeStream* stream, std::vector<Property<float>*>& dashes) {
  stream->alignWithBytes();
  uint64_t dashLength = stream->readUBits(3) + 1;
  auto dashOffsetFlag = ReadAttributeFlag(stream, &dashOffsetConfig);
  std::vector<AttributeFlag> dashFlags;
  for (uint64_t i = 0; i < dashLength; i++) {
    if (stream->context->hasException()) {
      return nullptr;
    }
    auto flag = ReadAttributeFlag(stream, &dashConfig);
    dashFlags.push_back(flag);
  }
  Property<float>* dashOffset = nullptr;
  dashOffsetConfig.readAttribute(stream, dashOffsetFlag, &dashOffset);
  for (uint64_t i = 0; i < dashLength; i++) {
    if (stream->context->hasException()) {
      return dashOffset;
    }
    Property<float>* dash = nullptr;
    dashConfig.readAttribute(stream, dashFlags[i], &dash);
    dashes.push_back(dash);
  }
  return dashOffset;
}

void WriteDashes(EncodeStream* stream, std::vector<Property<float>*>& dashes,
                 Property<float>* dashOffset) {
  if (dashes.empty()) {
    return;
  }
  stream->alignWithBytes();
  EncodeStream contentBytes(stream->context);

  auto dashLength = static_cast<uint32_t>(dashes.size());
  if (dashLength > 6) {
    dashLength = 6;
  }
  stream->writeUBits(dashLength - 1, 3);
  dashOffsetConfig.writeAttribute(stream, &contentBytes, &dashOffset);
  for (uint32_t i = 0; i < dashLength; i++) {
    dashConfig.writeAttribute(stream, &contentBytes, &(dashes[i]));
  }
  stream->writeBytes(&contentBytes);
}
}  // namespace pag
