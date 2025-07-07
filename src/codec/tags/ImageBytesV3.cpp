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

#include "ImageBytesV3.h"
#include "ImageBytesV2.h"

namespace pag {
ImageBytes* ReadImageBytesV3(DecodeStream* stream) {
  auto imageBytes = new ImageBytes();
  imageBytes->id = stream->readEncodedUint32();
  imageBytes->fileBytes = stream->readByteData().release();
  imageBytes->scaleFactor = stream->readFloat();
  imageBytes->width = stream->readEncodedInt32();
  imageBytes->height = stream->readEncodedInt32();
  imageBytes->anchorX = stream->readEncodedInt32();
  imageBytes->anchorY = stream->readEncodedInt32();
  return imageBytes;
}

TagCode WriteImageBytesV3(EncodeStream* stream, pag::ImageBytes* imageBytes) {
  WriteImageBytesV2(stream, imageBytes);
  stream->writeEncodedInt32(imageBytes->width);
  stream->writeEncodedInt32(imageBytes->height);
  stream->writeEncodedInt32(imageBytes->anchorX);
  stream->writeEncodedInt32(imageBytes->anchorY);
  return TagCode::ImageBytesV3;
}
}  // namespace pag
