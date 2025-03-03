/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "ImageRgbaData.h"
#include "ImageRawData.h"

void ImageRgbaData::resize(int newWidth, int newHeight) {
  if (data != nullptr && width == newWidth && height == newHeight) {
    return;
  }
  width = newWidth;
  height = newHeight;
  data = std::shared_ptr<uint8_t>(new uint8_t[width * height * 4], std::default_delete<uint8_t[]>());
  auto p = data.get();
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      p[0] = 0;
      p[1] = 0;
      p[2] = 0;
      p[3] = 255;
      p += 4;
    }
  }
}

void ImageRgbaData::scaleTo(uint8_t* src, int srcStride, int srcWidth, int srcHeight) {
  double factorX = (double)width / srcWidth;
  double factorY = (double)height / srcHeight;
  double factor = factorX < factorY ? factorX : factorY;
  int scaledWidth = srcWidth * factor;
  int scaledHeight = srcHeight * factor;
  int stride = width * 4;
  int posX = (width - scaledWidth) / 2;
  int posY = (height - scaledHeight) / 2;
  ScaleCoreGraphics(data.get() + posY * stride + posX * 4, stride, src, srcStride,
                    scaledWidth, scaledHeight, srcWidth, srcHeight);
}