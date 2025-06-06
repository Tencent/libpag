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
#include "VideoEncoder.h"


#ifdef _WIN32
#include <io.h>       /* _setmode() */
#include <fcntl.h>    /* _O_BINARY */
#endif

#include <stdint.h>
#include <stdio.h>
#include "VideoEncoderFfmpeg.h"
#include "VideoEncoderX264.h"
#include "VideoEncoderOffline.h"
#include "src/utils/color_space/color_space.h"

std::unique_ptr<VideoEncoder> VideoEncoder::MakeVideoEncoder(bool bHW) {
  // if (bHW && VideoEncoderFfmpeg::TestBeEnable()) {
  //   auto videoEncoder = new VideoEncoderFfmpeg();
  //   return std::unique_ptr<VideoEncoder>(videoEncoder);
  // } else {
    //auto videoEncoder = new VideoEncoderX264();
    auto videoEncoder = new VideoEncoderOffline();
    return std::unique_ptr<VideoEncoder>(videoEncoder);
  // }
}

static void FillYUVPadding(uint8_t* data[4], int stride[4], int width, int height,
                           int alphaStartX, int alphaStartY) {
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
    auto psrc = data[0] + (height - 1) * stride[0];
    auto pdst = data[0] + height * stride[0];
    auto qsrc = data[0] + alphaStartY * stride[0];
    auto qdst = data[0] + (alphaStartY - 1) * stride[0];
    for (int j = 0; j < yAdd; j++) {
      memcpy(pdst, psrc, width);
      memcpy(qdst, qsrc, width);
      pdst += stride[0];
      qdst -= stride[0];
    }
  }
}

PAGEncoder::PAGEncoder(bool bHW) {
  enc = VideoEncoder::MakeVideoEncoder(bHW);
}

bool PAGEncoder::init(int width, int height, double frameRate, bool hasAlpha, int maxKeyFrameInterval, int quality) {
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

  enc->open(internalWidth, internalHeight, frameRate, hasAlpha, maxKeyFrameInterval, quality);

  colorspace_init();
  c264_csp_init(0);

  return true;
}

int PAGEncoder::encodeHeaders(uint8_t* header[], int headerSize[]) {
  return enc->encodeHeaders(header, headerSize);
}

static void GetAlphaFromRGBA(uint8_t* dst[4], int dstStride[4],
                             uint8_t* src, int srcStride,
                             int width, int height,
                             int alphaStartX, int alphaStartY) {
  uint8_t* pdstY = dst[0] + alphaStartY * dstStride[0] + alphaStartX; // dst alpha
  uint8_t* psrc = src; // src RGBA

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      pdstY[i] = (psrc[i * 4 + 3] * 219 >> 8) + 16;
    }

    pdstY += dstStride[0];
    psrc += srcStride;
  }
}

int PAGEncoder::encodeRGBA(uint8_t* inData, int inDataStride,
                            uint8_t** pOutStream,
                            FrameType* pFrameType, int64_t* pOutTimeStamp) {
  uint8_t* data[4] = {nullptr};
  int stride[4] = {0};
  enc->getInputFrameBuf(data, stride);

  /* Encode frames */
  if (inData != NULL) {
    // input frame
    RGBAToYUV420(inData, inDataStride,
                 data[0], data[1], data[2],
                 stride[0], stride[1],
                 inputWidth, inputHeight, 0);

    if (hasAlpha) {
      GetAlphaFromRGBA(data, stride,
                       inData, inDataStride,
                       SIZE_ALIGN(inputWidth), SIZE_ALIGN(inputHeight),
                       alphaStartX, alphaStartY);

      FillYUVPadding(data, stride,
                     SIZE_ALIGN(inputWidth), SIZE_ALIGN(inputHeight),
                     alphaStartX, alphaStartY);
    }
  } else {
    data[0] = nullptr; // for flush delayed frames
  }

  auto size = enc->encodeFrame(data, stride, pOutStream, pFrameType, pOutTimeStamp);

  return size;
}

PAGEncoder::~PAGEncoder() {
}
