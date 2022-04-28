/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "EditableLayer.h"

namespace pag {
void ReadEditableLayer(DecodeStream* stream) {
  auto context = static_cast<CodecContext*>(stream->context);
  auto count = stream->readEncodedUint32();
  for (uint32_t i = 0; i < count; i++) {
    auto imageLayer = new ImageLayer();
    imageLayer->id = stream->readEncodedUint32();
    context->editableImageLayers.push_back(imageLayer);
  }
  count = stream->readEncodedUint32();
  for (uint32_t i = 0; i < count; i++) {
    auto textLayer = new TextLayer();
    textLayer->id = stream->readEncodedUint32();
    context->editableTextLayers.push_back(textLayer);
  }
}

TagCode WriteEditableLayer(EncodeStream* stream, const File* file) {
  stream->writeEncodedUint32(static_cast<uint32_t>(file->editableImageLayers.size()));
  for (auto& layer : file->editableImageLayers) {
    stream->writeEncodedUint32(layer->id);
  }
  stream->writeEncodedUint32(static_cast<uint32_t>(file->editableTextLayers.size()));
  for (auto& layer : file->editableTextLayers) {
    stream->writeEncodedUint32(layer->id);
  }
  return TagCode::EditableLayer;
}
}  // namespace pag