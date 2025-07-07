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

#include "EffectCompositingOption.h"

namespace pag {
void ReadEffectCompositingMasks(DecodeStream* stream, void* target) {
  auto effect = reinterpret_cast<Effect*>(target);
  auto length = stream->readEncodedUint32();
  for (uint32_t i = 0; i < length; i++) {
    if (stream->context->hasException()) {
      break;
    }
    auto mask = ReadMaskID(stream);
    effect->maskReferences.push_back(mask);
  }
}

bool WriteEffectCompositingMasks(EncodeStream* stream, void* target) {
  auto effect = reinterpret_cast<Effect*>(target);
  auto length = static_cast<uint32_t>(effect->maskReferences.size());
  if (length > 0) {
    stream->writeEncodedUint32(length);
    for (uint32_t i = 0; i < length; i++) {
      auto mask = effect->maskReferences[i];
      WriteMaskID(stream, mask);
    }
  }
  return length > 0;
}

void EffectCompositingOptionTag(BlockConfig* tagConfig, Effect* effect) {
  AddAttribute(tagConfig, &effect->effectOpacity, AttributeType::SimpleProperty, Opaque);
  AddCustomAttribute(tagConfig, effect, ReadEffectCompositingMasks, WriteEffectCompositingMasks);
}
}  // namespace pag
