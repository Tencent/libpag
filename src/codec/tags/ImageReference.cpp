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

#include "ImageReference.h"

namespace pag {
void ReadImageReference(DecodeStream* stream, ImageLayer* layer) {
  auto id = stream->readEncodedUint32();
  auto context = static_cast<CodecContext*>(stream->context);
  layer->imageBytes = context->getImageBytes(id);
}

TagCode WriteImageReference(EncodeStream* stream, ImageLayer* layer) {
  stream->writeEncodedUint32(layer->imageBytes->id);
  return TagCode::ImageReference;
}
}  // namespace pag
