/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#pragma once

#include <pag/file.h>

namespace exporter {

typedef struct {
  int xPos;
  int yPos;
  int width;
  int height;
} ImageRect;

void ConvertARGBToRGBA(const uint8_t* argb, int width, int height, int srcStride, uint8_t* rgba,
                       int dstStride);

void ClipTransparentEdge(ImageRect& rect, const uint8_t* srcData, int width, int height,
                         int stride);

void ScalePixels(uint8_t* dstRGBA, int dstStride, int dstWidth, int dstHeight, uint8_t* srcRGBA,
                 int srcStride, int srcWidth, int srcHeight);

void GetImageDiffRect(ImageRect& rect, const uint8_t* preImage, const uint8_t* curImage, int width,
                      int height, int stride);

void ExpandRectRange(ImageRect& dstRect, const ImageRect& srcRect, const ImageRect& lastRect,
                     int width, int height, int expand);

pag::ByteData* EncodeImageData(uint8_t* rgbaBytes, int width, int height, int rowBytes,
                               int quality);

void GetOpaqueRect(int& left, int& top, int& right, int& bottom, const uint8_t* data, int width,
                   int height, int stride);

void CopyRGBA(uint8_t* dst, int dstStride, const uint8_t* src, int srcStride, int width,
              int height);

bool ImageHasAlpha(uint8_t* data, int stride, int width, int height);

void OddPaddingRGBA(uint8_t* inData, int inDataStride, int width, int height);

bool ImageIsStatic(const uint8_t* pCur, const uint8_t* pRef, int width, int height, int stride);

}  // namespace exporter
