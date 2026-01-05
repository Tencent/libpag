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

#include "CompositionReference.h"

namespace pag {
void ReadCompositionReference(DecodeStream* stream, PreComposeLayer* layer) {
  auto id = stream->readEncodedUint32();
  if (id > 0) {
    layer->composition = new Composition();
    layer->composition->id = id;
  }
  layer->compositionStartTime = ReadTime(stream);
}

TagCode WriteCompositionReference(EncodeStream* stream, PreComposeLayer* layer) {
  auto id = layer->composition != nullptr ? layer->composition->id : static_cast<ID>(0);
  stream->writeEncodedUint32(id);
  WriteTime(stream, layer->compositionStartTime);
  return TagCode::CompositionReference;
}
}  // namespace pag
