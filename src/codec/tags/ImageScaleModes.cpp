/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "ImageScaleModes.h"

namespace pag {
void ReadImageScaleModes(DecodeStream* stream) {
  auto context = static_cast<CodecContext*>(stream->context);
  auto count = stream->readEncodedUint32();
  if (count > 0) {
    context->imageScaleModes = new std::vector<PAGScaleMode>(count);
    for (uint32_t i = 0; i < count; i++) {
      context->imageScaleModes->at(i) = static_cast<PAGScaleMode>(stream->readEncodedUint32());
    }
  }
}

TagCode WriteImageScaleModes(EncodeStream* stream, const File* file) {
  if (file->imageScaleModes == nullptr) {
    stream->writeEncodedUint32(0);
  } else {
    stream->writeEncodedUint32(static_cast<uint32_t>(file->imageScaleModes->size()));
    for (auto mode : *file->imageScaleModes) {
      stream->writeEncodedUint32(static_cast<uint32_t>(mode));
    }
  }
  return TagCode::ImageScaleModes;
}
}  // namespace pag
