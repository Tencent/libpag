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

#include "ImageData.h"
#include <webp/encode.h>
#include "tgfx/core/ImageCodec.h"

namespace exporter {

void ConvertARGBToRGBA(const uint8_t* argb, int width, int height, int srcStride, uint8_t* rgba,
                       int dstStride) {
  for (int y = 0; y < height; y++) {
    auto src = argb + srcStride * y;
    auto dst = rgba + dstStride * y;
    for (int x = 0; x < width; x++) {
      dst[0] = src[1];
      dst[1] = src[2];
      dst[2] = src[3];
      dst[3] = src[0];
      src += 4;
      dst += 4;
    }
  }
}

void ClipTransparentEdge(ImageRect& rect, const uint8_t* srcData, int width, int height,
                         int stride) {
  int minX = width - 1;
  int minY = height - 1;
  int maxX = 0;
  int maxY = 0;

  for (int y = 0; y < height; y++) {
    const uint8_t* data = srcData + y * stride + 0;

    for (int x = 0; x < width; x++) {
      if (data[x * 4 + 3] != 0) {
        if (minX > x) {
          minX = x;
        }
        if (maxX < x) {
          maxX = x;
        }
        if (minY > y) {
          minY = y;
        }
        if (maxY < y) {
          maxY = y;
        }
      }
    }
  }

  if (minX <= maxX) {
    rect.xPos = minX;
    rect.yPos = minY;
    rect.width = maxX - minX + 1;
    rect.height = maxY - minY + 1;
  } else {
    rect.xPos = 0;
    rect.yPos = 0;
    rect.width = 1;
    rect.height = 1;
  }
}

void ScalePixels(uint8_t* dstRGBA, int dstStride, int dstWidth, int dstHeight, uint8_t* srcRGBA,
                 int srcStride, int srcWidth, int srcHeight) {
  std::shared_ptr<tgfx::Data> data = tgfx::Data::MakeWithoutCopy(srcRGBA, srcStride * srcHeight);
  auto imageInfo = tgfx::ImageInfo::Make(srcWidth, srcHeight, tgfx::ColorType::RGBA_8888,
                                         tgfx::AlphaType::Premultiplied, srcStride);
  auto codec = tgfx::ImageCodec::MakeFrom(imageInfo, data);
  auto scaleImageInfo = tgfx::ImageInfo::Make(dstWidth, dstHeight, tgfx::ColorType::RGBA_8888,
                                              tgfx::AlphaType::Premultiplied, dstStride);
  codec->readPixels(scaleImageInfo, dstRGBA);
}

void GetImageDiffRect(ImageRect& rect, const uint8_t* preImage, const uint8_t* curImage, int width,
                      int height, int stride) {
  if (preImage == nullptr || curImage == nullptr) {
    return;
  }
  int minX = width - 1;
  int minY = height - 1;
  int maxX = 0;
  int maxY = 0;

  for (int y = 0; y < height; y++) {
    const auto preData =
        reinterpret_cast<const uint32_t*>(static_cast<const void*>(preImage + y * stride));
    const auto curData =
        reinterpret_cast<const uint32_t*>(static_cast<const void*>(curImage + y * stride));

    for (int x = 0; x < width; x++) {
      if (preData[x] == curData[x]) {
        continue;
      }
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }

  if (minX < maxX) {
    rect.xPos = minX;
    rect.yPos = minY;
    rect.width = maxX - minX + 1;
    rect.height = maxY - minY + 1;
  } else {
    rect.xPos = 0;
    rect.yPos = 0;
    rect.width = 0;
    rect.height = 0;
  }
}

static bool InRange(int val, int pos, int width) {
  return val >= pos && val <= pos + width;
}

void ExpandRectRange(ImageRect& dstRect, const ImageRect& srcRect, const ImageRect& lastRect,
                     int width, int height, int expand) {

  int leftExpand = 0;
  int topExpand = 0;
  int rightExpand = 0;
  int bottomExpand = 0;

  if (InRange(srcRect.xPos, lastRect.xPos, lastRect.width)) {
    leftExpand = std::min(expand, srcRect.xPos);
  }
  if (InRange(srcRect.xPos + srcRect.width, lastRect.xPos, lastRect.width)) {
    rightExpand = std::min(expand, width - (srcRect.xPos + srcRect.width));
  }
  if (InRange(srcRect.yPos, lastRect.yPos, lastRect.height)) {
    topExpand = std::min(expand, srcRect.yPos);
  }
  if (InRange(srcRect.yPos + srcRect.height, lastRect.yPos, lastRect.height)) {
    bottomExpand = std::min(expand, height - (srcRect.yPos + srcRect.height));
  }

  dstRect.xPos = srcRect.xPos - leftExpand;
  dstRect.yPos = srcRect.yPos - topExpand;
  dstRect.width = srcRect.width + leftExpand + rightExpand;
  dstRect.height = srcRect.height + topExpand + bottomExpand;
}

pag::ByteData* EncodeImageData(uint8_t* rgbaBytes, int width, int height, int rowBytes,
                               int quality) {

  pag::ByteData* bitmapBytes = nullptr;
  uint8_t* output = nullptr;
  auto size = WebPEncodeRGBA(rgbaBytes, width, height, rowBytes, quality, &output);
  if (size > 0 && output != nullptr) {
    auto bytes = new uint8_t[size];
    memcpy(bytes, output, size);
    bitmapBytes = pag::ByteData::MakeAdopted(bytes, size).release();
  }
  WebPFree(output);

  return bitmapBytes;
}

void GetOpaqueRect(int& left, int& top, int& right, int& bottom, const uint8_t* data, int width,
                   int height, int stride) {
  if (right - left >= width && bottom - top >= height) {
    return;
  }

  for (int y = 0; y < height; y++) {
    const uint8_t* p = data + y * stride + 3;

    for (int x = 0; x < width; x++) {
      if (p[x * 4] != 0) {
        if (left > x) {
          left = x;
        }
        if (right <= x) {
          right = x + 1;
        }
        if (top > y) {
          top = y;
        }
        if (bottom <= y) {
          bottom = y + 1;
        }
      }
    }
  }
}

static void CopyPlane(uint8_t* dst, int dstStride, const uint8_t* src, int srcStride, int width,
                      int height) {
  auto pDst = dst;
  auto pSrc = src;
  for (int j = 0; j < height; j++) {
    memcpy(pDst, pSrc, width);
    pDst += dstStride;
    pSrc += srcStride;
  }
}

void CopyRGBA(uint8_t* dst, int dstStride, const uint8_t* src, int srcStride, int width,
              int height) {
  CopyPlane(dst, dstStride, src, srcStride, width * 4, height);
}

bool ImageHasAlpha(uint8_t* data, int stride, int width, int height) {

  uint8_t sum = 0xFF;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      sum &= data[(i << 2) + 3];
    }

    if (sum != 0xFF) {
      return true;
    }

    data += stride;
  }

  return false;
}

void OddPaddingRGBA(uint8_t* inData, int inDataStride, int width, int height) {
  int widthAdd = 0;

  if ((width & 0x01) && inDataStride >= (width + 1) * 4) {
    uint8_t* pData = &inData[width * 4];
    for (int j = 0; j < height; j++) {
      pData[0] = pData[0 - 4];
      pData[1] = pData[1 - 4];
      pData[2] = pData[2 - 4];
      pData[3] = pData[3 - 4];

      pData += inDataStride;
    }

    widthAdd = 1;
  }

  if (height & 0x01) {
    uint8_t* pData = &inData[height * inDataStride];
    memcpy(pData, pData - inDataStride, (width + widthAdd) * 4);
  }
}

bool ImageIsStatic(const uint8_t* pCur, const uint8_t* pRef, int width, int height, int stride) {
  for (int y = 0; y < height; y++) {
    const auto pCurU32 = static_cast<const uint32_t*>(static_cast<const void*>(pCur + y * stride));
    const auto pRefU32 = static_cast<const uint32_t*>(static_cast<const void*>(pRef + y * stride));

    for (int x = 0; x < width; x++) {
      if (pCurU32[x] != pRefU32[x]) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace exporter
