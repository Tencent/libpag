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

#include "VideoEncoder.h"
#include "OfflineVideoEncoder.h"
#include "utils/ColorSpace.h"

namespace exporter {

static void GetAlphaFromRGBA(uint8_t* dstData[4], int dstStride[4], uint8_t* srcData, int srcStride,
                             int width, int height, int alphaStartX, int alphaStartY) {
  uint8_t* alphaData = dstData[0] + alphaStartY * dstStride[0] + alphaStartX;
  uint8_t* data = srcData;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      alphaData[i] = (data[i * 4 + 3] * 219 >> 8) + 16;
    }

    alphaData += dstStride[0];
    data += srcStride;
  }
}

static void FillYUVPadding(uint8_t* data[4], int stride[4], int width, int height, int alphaStartX,
                           int alphaStartY) {
  if (alphaStartX > width) {
    int xAdd = (alphaStartX - width) / 2;
    auto p = data[0] + width;
    auto q = data[0] + alphaStartX;
    for (int j = 0; j < height; j++) {
      memset(p, p[-1], xAdd);
      memset(q - xAdd, q[0], xAdd);
      p += stride[0];
      q += stride[0];
    }
  } else if (alphaStartY > height) {
    int yAdd = (alphaStartY - height) / 2;
    auto pSrc = data[0] + (height - 1) * stride[0];
    auto pDst = data[0] + height * stride[0];
    auto qSrc = data[0] + alphaStartY * stride[0];
    auto qDst = data[0] + (alphaStartY - 1) * stride[0];
    for (int j = 0; j < yAdd; j++) {
      memcpy(pDst, pSrc, width);
      memcpy(qDst, qSrc, width);
      pDst += stride[0];
      qDst -= stride[0];
    }
  }
}

std::unique_ptr<VideoEncoder> VideoEncoder::MakeVideoEncoder(bool) {
  return std::make_unique<OfflineVideoEncoder>();
}

PAGEncoder::PAGEncoder(bool hardwareEncode) {
  encoder = VideoEncoder::MakeVideoEncoder(hardwareEncode);
}

bool PAGEncoder::init(int width, int height, double frameRate, bool hasAlpha,
                      int maxKeyFrameInterval, int quality) {
  inputWidth = width;
  inputHeight = height;
  internalWidth = SIZE_ALIGN(width);
  internalHeight = SIZE_ALIGN(height);
  this->hasAlpha = hasAlpha;
  if (hasAlpha) {
    if (internalWidth < internalHeight) {
      alphaStartX = internalWidth + paddingX;
      alphaStartY = 0;
      internalWidth = internalWidth * 2 + paddingX;
    } else {
      alphaStartX = 0;
      alphaStartY = internalHeight + paddingY;
      internalHeight = internalHeight * 2 + paddingY;
    }
  }

  encoder->open(internalWidth, internalHeight, frameRate, hasAlpha, maxKeyFrameInterval, quality);
  colorspace_init();
  c264_csp_init(0);

  return true;
}

void PAGEncoder::getAlphaStartXY(int32_t* pAlphaStartX, int32_t* pAlphaStartY) {
  *pAlphaStartX = alphaStartX;
  *pAlphaStartY = alphaStartY;
}

void PAGEncoder::encodeRGBA(uint8_t* data, int dataStride, FrameType frameType) {
  if (data == nullptr) {
    return;
  }
  uint8_t* tmpData[4] = {nullptr};
  int tmpStride[4] = {0};
  encoder->getInputFrameBuf(tmpData, tmpStride);
  RGBAToYUV420(data, dataStride, tmpData[0], tmpData[1], tmpData[2], tmpStride[0], tmpStride[1],
               inputWidth, inputHeight, 0);

  if (hasAlpha) {
    GetAlphaFromRGBA(tmpData, tmpStride, data, dataStride, SIZE_ALIGN(inputWidth),
                     SIZE_ALIGN(inputHeight), alphaStartX, alphaStartY);
    FillYUVPadding(tmpData, tmpStride, SIZE_ALIGN(inputWidth), SIZE_ALIGN(inputHeight), alphaStartX,
                   alphaStartY);
  }

  encoder->encodeFrame(tmpData, tmpStride, frameType);
}

int PAGEncoder::encodeHeaders(uint8_t* header[], int headerSize[]) {
  return encoder->encodeHeaders(header, headerSize);
}

int PAGEncoder::getEncodedData(uint8_t** outData, FrameType* outFrameType, int64_t* outFrameIndex) {
  return encoder->getEncodedFrame(false, outData, outFrameType, outFrameIndex);
}

void PAGEncoder::close() {
  encoder->close();
}

}  // namespace exporter
