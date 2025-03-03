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
#ifndef IMAGERAWDATA_H
#define IMAGERAWDATA_H
#include "src/exports/PAGDataTypes.h"
typedef struct {
  int xPos;
  int yPos;
  int width;
  int height;
} ImageRect;

void ConvertARGBToRGBA(const uint8_t* argb, int width, int height, int srcStride, uint8_t* rgba, int dstStride);

// 裁剪：去掉边缘透明区域
void ClipTransparentEdge(ImageRect& rect, const uint8_t* pData, int width, int height, int stride);

// 获取两帧图像内容不同的矩形框
void GetImageDiffRect(ImageRect& rect, const uint8_t* pCur, const uint8_t* pRef, int width, int height, int stride);

// 判断两帧是否相同
bool ImageIsStatic(const uint8_t* pCur, const uint8_t* pRef, int width, int height, int stride);

// 缩放，CoreGraphics系统差值
void ScaleCoreGraphics(uint8_t* dstRGBA, int dstStride, uint8_t* srcRGBA, int srcStride, int dstWidth, int dstHeight,
                       int srcWidth, int srcHeight);

void ExpandRectRange(ImageRect& dstRect, ImageRect& srcRect, ImageRect& lastRect,
                     A_long width, A_long height, int expand);

// 将RGBA数据转化为带透明通道的视频编码输入数据
void ConvertAlphaRGB(uint8_t* dst, int dstStride, uint8_t* src, int srcStride, int width, int height);

// 检测是否含有alpha数据
bool DetectAlpha(uint8_t* data, int stride, int width, int height);

// 奇数分辨率填充成偶数数据
void OddPaddingRGBA(uint8_t* inData, int inDataStride, int width, int height);

// 获取不透明区域
void GetOpaqueRect(int& left, int& top, int& right, int& bottom,
                   const uint8_t* data, int width, int height, int stride);

// 拷贝RGBA
void CopyRGBA(uint8_t* dst, int dstStride,
              const uint8_t* src, int srcStride,
              int width, int height);

// 拷贝YUV
void CopyYUV420(uint8_t* dst[4], int dstStride[4],
                uint8_t* src[4], int srcStride[4],
                int width, int height);

#endif //IMAGERAWDATA_H
