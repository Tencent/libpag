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
#include "ImageRawData.h"
#include <assert.h>
#ifdef APPLE
#include <CoreGraphics/CoreGraphics.h>
#endif

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

// 裁剪：去掉边缘透明区域
// 未优化版, 仅用作理解示意
void ClipTransparentEdge(ImageRect& rect, const uint8_t* pData, int width, int height, int stride) {
  int minx = width - 1;
  int miny = height - 1;
  int maxx = 0;
  int maxy = 0;

  for (int y = 0; y < height; y++) {
    const uint8_t* data = pData + y * stride + 0;

    for (int x = 0; x < width; x++) {
      if (data[x * 4 + 3] != 0) {
        if (minx > x) {
          minx = x;
        }
        if (maxx < x) {
          maxx = x;
        }
        if (miny > y) {
          miny = y;
        }
        if (maxy < y) {
          maxy = y;
        }
      }
    }
  }

  if (minx <= maxx) {
    rect.xPos = minx;
    rect.yPos = miny;
    rect.width = maxx - minx + 1;
    rect.height = maxy - miny + 1;
  } else {
    rect.xPos = 0;
    rect.yPos = 0;
    rect.width = 1;
    rect.height = 1;
  }
}

// 未优化版, 仅用作理解示意
void GetImageDiffRect(ImageRect& rect, const uint8_t* pCur, const uint8_t* pRef, int width,
                      int height, int stride) {
  int minx = width - 1;
  int miny = height - 1;
  int maxx = 0;
  int maxy = 0;

  for (int y = 0; y < height; y++) {
    const uint32_t* pCurU32 = (uint32_t*)(pCur + y * stride);
    const uint32_t* pRefU32 = (uint32_t*)(pRef + y * stride);

    for (int x = 0; x < width; x++) {
      if (pCurU32[x] != pRefU32[x]) {
        if (minx > x) {
          minx = x;
        }
        if (maxx < x) {
          maxx = x;
        }
        if (miny > y) {
          miny = y;
        }
        if (maxy < y) {
          maxy = y;
        }
      }
    }
  }

  if (minx <= maxx) {
    rect.xPos = minx;
    rect.yPos = miny;
    rect.width = maxx - minx + 1;
    rect.height = maxy - miny + 1;
  } else {
    rect.xPos = 0;
    rect.yPos = 0;
    rect.width = 0;
    rect.height = 0;
  }
}

// 判断两帧是否相同
bool ImageIsStatic(const uint8_t* pCur, const uint8_t* pRef, int width, int height, int stride) {
  for (int y = 0; y < height; y++) {
    const uint32_t* pCurU32 = (uint32_t*)(pCur + y * stride);
    const uint32_t* pRefU32 = (uint32_t*)(pRef + y * stride);

    for (int x = 0; x < width; x++) {
      if (pCurU32[x] != pRefU32[x]) {
        return false;
      }
    }
  }

  return true;
}

#ifdef WIN32
// 缩放，双线性插值，未优化
void ScaleRGBABiLinear(uint8_t* dstRGBA, int dstStride, uint8_t* srcRGBA, int srcStride,
                       int dstWidth, int dstHeight, int srcWidth, int srcHeight) {

  double xFactor = (double)srcWidth / dstWidth;
  double yFactor = (double)srcHeight / dstHeight;

  for (int j = 0; j < dstHeight; j++) {
    auto dst = dstRGBA + j * dstStride;

    auto sj = (int)(j * yFactor);
    auto src0 = srcRGBA + sj * srcStride;
    auto src1 = srcRGBA + (sj + 1) * srcStride;
    if (sj >= srcHeight - 1) {
      src1 = src0;
    }

    // y权重
    double y1 = j * yFactor - sj;
    double y0 = 1 - y1;

    for (int i = 0; i < dstWidth; i++) {
      auto si = (int)(i * xFactor);

      // x权重
      double x1 = i * xFactor - si;
      double x0 = 1 - x1;

      si *= 4;  // RGBA=4

      dst[0] = (uint8_t)lround(src0[si + 0] * x0 * y0 + src0[si + 4] * x1 * y0 +
                               src1[si + 0] * x0 * y1 + src1[si + 4] * x1 * y1);
      dst[1] = (uint8_t)lround(src0[si + 1] * x0 * y0 + src0[si + 5] * x1 * y0 +
                               src1[si + 1] * x0 * y1 + src1[si + 5] * x1 * y1);
      dst[2] = (uint8_t)lround(src0[si + 2] * x0 * y0 + src0[si + 6] * x1 * y0 +
                               src1[si + 2] * x0 * y1 + src1[si + 6] * x1 * y1);
      dst[3] = (uint8_t)lround(src0[si + 3] * x0 * y0 + src0[si + 7] * x1 * y0 +
                               src1[si + 3] * x0 * y1 + src1[si + 7] * x1 * y1);

      dst += 4;
    }
  }
}
#endif

// 缩放，CoreGraphics系统差值
void ScaleCoreGraphics(uint8_t* dstRGBA, int dstStride, uint8_t* srcRGBA, int srcStride,
                       int dstWidth, int dstHeight, int srcWidth, int srcHeight) {

#ifdef WIN32
  ScaleRGBABiLinear(dstRGBA, dstStride, srcRGBA, srcStride, dstWidth, dstHeight, srcWidth,
                    srcHeight);
#else
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

  CGContextRef dstCtx =
      CGBitmapContextCreate(dstRGBA, (size_t)dstWidth, (size_t)dstHeight, 8, (size_t)dstStride,
                            colorSpace, kCGImageAlphaPremultipliedLast);

  CGContextRef srcCtx =
      CGBitmapContextCreate(srcRGBA, (size_t)srcWidth, (size_t)srcHeight, 8, (size_t)srcStride,
                            colorSpace, kCGImageAlphaPremultipliedLast);
  CGImageRef imageRef = CGBitmapContextCreateImage(srcCtx);

  auto dstRect = CGRectMake(0, 0, (size_t)dstWidth, (size_t)dstHeight);
  CGContextSetInterpolationQuality(dstCtx, kCGInterpolationLow);
  CGContextClearRect(dstCtx, dstRect);
  CGContextDrawImage(dstCtx, dstRect, imageRef);

  CGImageRelease(imageRef);
  CGContextRelease(srcCtx);
  CGContextRelease(dstCtx);
  CGColorSpaceRelease(colorSpace);
#endif
}

static bool InRange(int val, int pos, int width) {
  return val >= pos && val <= pos + width;
}

void ExpandRectRange(ImageRect& dstRect, ImageRect& srcRect, ImageRect& lastRect, A_long width,
                     A_long height, int expand) {

  int lftExpand = 0;
  int topExpand = 0;
  int rgtExpand = 0;
  int botExpand = 0;

  if (InRange(srcRect.xPos, lastRect.xPos, lastRect.width)) {
    lftExpand = std::min(expand, srcRect.xPos);
  }
  if (InRange(srcRect.xPos + srcRect.width, lastRect.xPos, lastRect.width)) {
    rgtExpand = std::min(expand, width - (srcRect.xPos + srcRect.width));
  }
  if (InRange(srcRect.yPos, lastRect.yPos, lastRect.height)) {
    topExpand = std::min(expand, srcRect.yPos);
  }
  if (InRange(srcRect.yPos + srcRect.height, lastRect.yPos, lastRect.height)) {
    botExpand = std::min(expand, height - (srcRect.yPos + srcRect.height));
  }

  dstRect.xPos = srcRect.xPos - lftExpand;
  dstRect.yPos = srcRect.yPos - topExpand;
  dstRect.width = srcRect.width + lftExpand + rgtExpand;
  dstRect.height = srcRect.height + topExpand + botExpand;
}

void ConvertAlphaRGB(uint8_t* dst, int dstStride, uint8_t* src, int srcStride, int width,
                     int height) {
  for (int j = 0; j < height; j++) {
    uint8_t* pdst0 = dst + j * dstStride;         // rgb
    uint8_t* pdst1 = pdst0 + height * dstStride;  // alpha
    uint8_t* psrc = src + j * srcStride;

    for (int i = 0; i < width; i++) {
      pdst0[0] = psrc[2];
      pdst0[1] = psrc[1];
      pdst0[2] = psrc[0];

      pdst1[0] = psrc[3];
      pdst1[1] = psrc[3];
      pdst1[2] = psrc[3];

      pdst0 += 3;
      pdst1 += 3;
      psrc += 4;
    }
  }
}

bool DetectAlpha(uint8_t* data, int stride, int width, int height) {

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

// 获取不透明区域
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

void CopyYUV420(uint8_t* dst[4], int dstStride[4], uint8_t* src[4], int srcStride[4], int width,
                int height) {
  CopyPlane(dst[0], dstStride[0], src[0], srcStride[0], width >> 0, height >> 0);
  CopyPlane(dst[1], dstStride[1], src[1], srcStride[1], width >> 1, height >> 1);
  CopyPlane(dst[2], dstStride[2], src[2], srcStride[2], width >> 1, height >> 1);
}
