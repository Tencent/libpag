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

#include "ImageBytesV2.h"
#include "ImageBytes.h"
#include "codec/utils/WebpDecoder.h"

namespace pag {
ImageBytes* ReadImageBytesV2(DecodeStream* stream) {
  auto imageBytes = new ImageBytes();
  imageBytes->id = stream->readEncodedUint32();
  imageBytes->fileBytes = stream->readByteData().release();
  if (imageBytes->fileBytes == nullptr || imageBytes->fileBytes->length() == 0) {
    return imageBytes;
  }
  imageBytes->scaleFactor = stream->readFloat();
  int width;
  int height;
  if (WebPGetInfo(imageBytes->fileBytes->data(), imageBytes->fileBytes->length(), &width,
                  &height)) {
    imageBytes->width = static_cast<int>(round(width * 1.0 / imageBytes->scaleFactor));
    imageBytes->height = static_cast<int>(round(height * 1.0 / imageBytes->scaleFactor));
  } else {
    LOGE("Get webP size fail.");
  }
  return imageBytes;
}

TagCode WriteImageBytesV2(EncodeStream* stream, pag::ImageBytes* imageBytes) {
  WriteImageBytes(stream, imageBytes);
  stream->writeFloat(imageBytes->scaleFactor);
  return TagCode::ImageBytesV2;
}
}  // namespace pag
