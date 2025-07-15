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

#include "Images.h"
#include "codec/utils/WebpDecoder.h"

namespace pag {
static bool FindImage(const std::vector<pag::ImageBytes*>* images) {
  bool found = false;
  for (auto& imageBytes : *images) {
    if (imageBytes->fileBytes == nullptr || imageBytes->fileBytes->length() == 0) {
      continue;
    }
    if ((imageBytes->width != 0 && imageBytes->height != 0) || imageBytes->scaleFactor != 1.0f) {
      found = true;
      break;
    }
  }
  return found;
}

void WriteImages(EncodeStream* stream, const std::vector<pag::ImageBytes*>* images) {
  if (!FindImage(images)) {
    WriteTag(stream, images, WriteImageTables);
    return;
  }
  for (auto& imageBytes : *images) {
    if (imageBytes->fileBytes == nullptr || imageBytes->fileBytes->length() == 0) {
      continue;
    }

    int scaledWidth = 0;
    int scaledHeight = 0;
    if (!WebPGetInfo(imageBytes->fileBytes->data(), imageBytes->fileBytes->length(), &scaledWidth,
                     &scaledHeight)) {
      LOGE("Get webP size fail.");
      continue;
    }

    int realWidth = static_cast<int>(round(imageBytes->width * imageBytes->scaleFactor));
    int realHeight = static_cast<int>(round(imageBytes->height * imageBytes->scaleFactor));

    if (scaledWidth == realWidth && scaledHeight == realHeight) {
      if (imageBytes->scaleFactor == 1.0f) {
        WriteTag(stream, imageBytes, WriteImageBytes);
      } else {
        WriteTag(stream, imageBytes, WriteImageBytesV2);
      }
    } else {
      WriteTag(stream, imageBytes, WriteImageBytesV3);
    }
  }
}
}  // namespace pag
