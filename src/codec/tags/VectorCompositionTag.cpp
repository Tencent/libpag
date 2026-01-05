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

#include "VectorCompositionTag.h"
#include "AudioBytes.h"
#include "CompositionAttributes.h"
#include "CompositionTag.h"
#include "LayerTag.h"

namespace pag {
void ReadTagsOfVectorComposition(DecodeStream* stream, TagCode code,
                                 VectorComposition* composition) {
  switch (code) {
    case TagCode::LayerBlock: {
      auto layer = ReadLayer(stream);
      composition->layers.push_back(layer);
    } break;
    default:
      ReadTagsOfComposition(stream, code, composition);
      break;
  }
}

VectorComposition* ReadVectorComposition(DecodeStream* stream) {
  auto composition = new VectorComposition();
  composition->id = stream->readEncodedUint32();
  ReadTags(stream, composition, ReadTagsOfVectorComposition);
  if (stream->context->hasException()) {
    delete composition;
    return nullptr;
  }
  Codec::InstallReferences(composition->layers);
  return composition;
}

TagCode WriteVectorComposition(EncodeStream* stream, VectorComposition* composition) {
  stream->writeEncodedUint32(composition->id);
  WriteTagsOfComposition(stream, composition);
  for (auto& layer : composition->layers) {
    WriteTag(stream, layer, WriteLayer);
  }
  WriteEndTag(stream);
  return TagCode::VectorCompositionBlock;
}
}  // namespace pag
