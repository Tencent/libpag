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

#include "ImageTables.h"
#include "ImageBytes.h"

namespace pag {
void ReadImageTables(DecodeStream* stream, std::vector<ImageBytes*>* images) {
  auto count = stream->readEncodedUint32();
  for (uint32_t i = 0; i < count; i++) {
    if (stream->context->hasException()) {
      break;
    }
    auto imageBytes = ReadImageBytes(stream);
    images->push_back(imageBytes);
  }
}

TagCode WriteImageTables(EncodeStream* stream, const std::vector<pag::ImageBytes*>* images) {
  uint32_t imageCount = 0;
  for (auto& imageBytes : *images) {
    if (imageBytes->fileBytes == nullptr || imageBytes->fileBytes->length() == 0) {
      continue;
    }
    imageCount++;
  }
  stream->writeEncodedUint32(imageCount);
  for (auto& imageBytes : *images) {
    if (imageBytes->fileBytes == nullptr || imageBytes->fileBytes->length() == 0) {
      continue;
    }
    WriteImageBytes(stream, imageBytes);
  }
  return TagCode::ImageTables;
}
}  // namespace pag
