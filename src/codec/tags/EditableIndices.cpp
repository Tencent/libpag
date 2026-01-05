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

#include "EditableIndices.h"

namespace pag {
void ReadEditableIndices(DecodeStream* stream) {
  auto context = static_cast<CodecContext*>(stream->context);
  auto count = stream->readEncodedUint32();
  context->editableImages = new std::vector<int>(count);
  for (uint32_t i = 0; i < count; i++) {
    context->editableImages->at(i) = stream->readEncodedInt32();
  }
  count = stream->readEncodedUint32();
  context->editableTexts = new std::vector<int>(count);
  for (uint32_t i = 0; i < count; i++) {
    context->editableTexts->at(i) = stream->readEncodedInt32();
  }
}

TagCode WriteEditableIndices(EncodeStream* stream, const File* file) {
  if (file->editableImages != nullptr &&
      file->editableImages->size() != static_cast<size_t>(file->numImages())) {
    stream->writeEncodedUint32(static_cast<uint32_t>(file->editableImages->size()));
    for (int index : *file->editableImages) {
      stream->writeEncodedInt32(index);
    }
  } else {
    stream->writeEncodedUint32(0);
  }

  if (file->editableTexts != nullptr &&
      file->editableTexts->size() != static_cast<size_t>(file->numTexts())) {
    stream->writeEncodedUint32(static_cast<uint32_t>(file->editableTexts->size()));
    for (int index : *file->editableTexts) {
      stream->writeEncodedInt32(index);
    }
  } else {
    stream->writeEncodedUint32(0);
  }
  return TagCode::EditableIndices;
}
}  // namespace pag
