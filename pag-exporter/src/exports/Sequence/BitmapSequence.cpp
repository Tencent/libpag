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
#include "BitmapSequence.h"
#include <webp/encode.h>
#include "pag/types.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/ui/qt/Progress/ProgressWindow.h"
#include "src/utils/AEUtils.h"
#include "src/utils/ImageData/ImageRawData.h"

#define CHECK_RET(statements)                                                      \
  do {                                                                             \
    if ((statements) != A_Err_NONE) {                                              \
      context->pushWarning(pagexporter::AlertInfoType::ExportBitmapSequenceError); \
      return;                                                                      \
    }                                                                              \
  } while (0)

// 判决是否为关键帧
static bool DecideIsKeyFrame(int frame, int lastKeyFramePos, int diffSize, int fullSize,
                             int keyFrameRate) {  // 判决是否关键帧
  bool bKeyFrame = false;
  const int diffFrameNum = frame - lastKeyFramePos;
  if (frame == 0) {  // 第一帧必须为关键帧
    bKeyFrame = true;
  } else if (diffSize == fullSize) {  // 如果Diff窗口等于全部窗口，则设为关键帧
    bKeyFrame = true;
  } else if (diffSize > fullSize * 90 / 100 &&
             diffFrameNum > 5) {  // 如果Diff窗口过大，可设为关键帧
    bKeyFrame = true;
  } else if (diffSize > fullSize * 75 / 100 && keyFrameRate > 20 &&
             diffFrameNum > keyFrameRate / 2) {  // 如果Diff窗口过大，可设为关键帧
    bKeyFrame = true;
  } else if (diffSize == 0) {  // 如果是静止帧，则不设为关键帧
    bKeyFrame = false;
  } else if (
      keyFrameRate > 0 &&
      diffFrameNum >=
          keyFrameRate) {  // 每个关键帧间距插入一个关键帧（特例：keyFrameRate为0表示仅第一帧为关键帧）
    bKeyFrame = true;
  }
  return bKeyFrame;
}

static pag::ByteData* EncodeBitmap(uint8_t* rgbaBytes, int xPos2, int yPos2, int rWidth2,
                                   int rHeight2, int rowBytes, int quality) {

  pag::ByteData* bitmapBytes = nullptr;
  uint8_t* output = nullptr;
  auto rgba = rgbaBytes + yPos2 * rowBytes + xPos2 * 4;
  auto size = WebPEncodeRGBA(rgba, rWidth2, rHeight2, rowBytes, quality, &output);
  if (size > 0 && output != nullptr) {
    auto bytes = new uint8_t[size];
    memcpy(bytes, output, size);
    bitmapBytes = pag::ByteData::MakeAdopted(bytes, size).release();
  }
  WebPFree(output);

  return bitmapBytes;
}

void ExportBitmapSequence(pagexporter::Context* context, const AEGP_ItemH& itemHandle,
                          pag::BitmapComposition* composition, float factor, float frameRate,
                          float compositionFactor) {

  frameRate = std::min(frameRate, composition->frameRate);  // 没必要大于composition的帧率
  auto duration =
      static_cast<pag::Frame>(ceil(composition->duration * frameRate / composition->frameRate));

  factor *= compositionFactor;

  if (context->bitmapMaxResolution > 0) {
    int minLine =
        static_cast<int>(std::min(composition->width, composition->height) * compositionFactor);
    if (minLine > context->bitmapMaxResolution) {
      factor *= ((double)context->bitmapMaxResolution / minLine);
    }
  }

  auto sequence = new pag::BitmapSequence();
  composition->sequences.push_back(sequence);

  sequence->composition = composition;
  sequence->frameRate = frameRate;
  sequence->width = static_cast<int>(ceil(composition->width * factor));
  sequence->height = static_cast<int>(ceil(composition->height * factor));
  if (factor > 0.99) {  // 在1.0附近或者大于1.0时，直接按原分辨率处理
    factor = 1.0;
    sequence->width = composition->width;
    sequence->height = composition->height;
  }
  int seqWidth = sequence->width;
  int seqHeight = sequence->height;

  auto& suites = context->suites;

  AEGP_RenderOptionsH renderOptions = nullptr;
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle,
                                                           &renderOptions));

  // 设置输出格式为8比特位图，避免出现16比特/32比特位深的情况
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8));

  int lastKeyFramePos = 0;
  int stride = sequence->width * 4;
  auto preData = new uint8_t[stride * sequence->height + stride * 2];
  auto curData = new uint8_t[stride * sequence->height + stride * 2];

  A_u_long curStride = static_cast<A_u_long>(composition->width * 4);
  uint8_t* curRGBABytes = nullptr;
  bool bScale = (sequence->width != composition->width || sequence->height != composition->height);

  ImageRect lastRect = {0, 0, seqWidth, seqHeight};

  for (int frame = 0; frame < duration && !context->bEarlyExit; frame++) {
    if (!bScale) {
      curRGBABytes = curData;
    } else if (curRGBABytes == nullptr) {
      curRGBABytes = new uint8_t[curStride * composition->height + curStride * 2];
    }

    SetRenderTime(context, renderOptions, frameRate, frame);  // 设置render时间

    A_long comWidth = 0;
    A_long comHeight = 0;
    GetRenderFrame(curRGBABytes, curStride, comWidth, comHeight, suites, renderOptions);

    auto bitmapFrame = new pag::BitmapFrame();

    if (comWidth == composition->width && comHeight == composition->height) {  // render成功

      if (bScale) {
        ScaleCoreGraphics(curData, stride, curRGBABytes, curStride, seqWidth, seqHeight, comWidth,
                          comHeight);  // 缩放
      }

      // 计算Diff差值
      ImageRect orgRect = {0, 0, seqWidth, seqHeight};
      if (frame > 0) {
        GetImageDiffRect(orgRect, curData, preData, seqWidth, seqHeight, stride);
      }

      // 判决是否为关键帧
      bool isKeyFrame = DecideIsKeyFrame(frame, lastKeyFramePos, orgRect.width * orgRect.height,
                                         seqWidth * seqHeight, context->bitmapKeyFrameInterval);

      ImageRect encodeRect = orgRect;

      // 如果判决为关键帧，则修改为全窗口大小，并记录关键帧的序号
      if (isKeyFrame) {
        orgRect.xPos = 0;
        orgRect.yPos = 0;
        orgRect.width = seqWidth;
        orgRect.height = seqHeight;

        lastKeyFramePos = frame;

        ClipTransparentEdge(orgRect, curData, seqWidth, seqHeight, stride);

        encodeRect = orgRect;
      } else if (orgRect.width > 0 && orgRect.height > 0) {
        ExpandRectRange(encodeRect, orgRect, lastRect, seqWidth, seqHeight, 4);
      }
      bitmapFrame->isKeyframe = isKeyFrame;

      lastRect = orgRect;

      if (orgRect.width > 0 && orgRect.height > 0) {
        pag::ByteData* bitmapBytes =
            EncodeBitmap(curData, encodeRect.xPos, encodeRect.yPos, encodeRect.width,
                         encodeRect.height, stride, context->imageQuality);

        if (bitmapBytes == nullptr) {
          context->pushWarning(pagexporter::AlertInfoType::WebpEncodeError);
        }

        auto bitmapRect = new pag::BitmapRect();
        bitmapRect->x = encodeRect.xPos;
        bitmapRect->y = encodeRect.yPos;
        bitmapRect->fileBytes = bitmapBytes;
        bitmapFrame->bitmaps.push_back(bitmapRect);
      } else {
        printf("static frame? diff rect=%dx%d\n", orgRect.width, orgRect.height);
      }

    } else {
      context->pushWarning(pagexporter::AlertInfoType::ExportRenderError);
      printf("error! %dx%d\n", comWidth, comHeight);
    }

    sequence->frames.push_back(bitmapFrame);

    if (context->progressWindow != nullptr) {
      context->progressWindow->increaseEncodedFrames();  // 更新进度
    }

    std::swap(curData, preData);
  }

  delete[] curData;
  delete[] preData;
  if (bScale) {
    delete[] curRGBABytes;
  }

  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions));
}
