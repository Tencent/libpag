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

#include "VideoSequence.h"
#include <webp/encode.h>
#include "codec/mp4/MP4BoxHelper.h"
#include "pag/types.h"
#include "src/VideoEncoder/VideoEncoder.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/ui/qt/Progress/ProgressWindow.h"
#include "src/utils/AEUtils.h"
#include "src/utils/ImageData/ImageRawData.h"
#define EXPORT_MP4_HEADER true

#define CHECK_RET(statements)                                                     \
  do {                                                                            \
    if ((statements) != A_Err_NONE) {                                             \
      context->pushWarning(pagexporter::AlertInfoType::ExportVideoSequenceError); \
      return;                                                                     \
    }                                                                             \
  } while (0)

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif /* MIN */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif /* MAX */

static int EncodeVideoHeader(PAGEncoder* pagEncoder, std::vector<pag::ByteData*>& headers) {
  uint8_t* nal[16];
  int size[16];
  int count = pagEncoder->encodeHeaders(nal, size);

  if (count >= 2) {
    auto sps = new uint8_t[size[0]];
    auto pps = new uint8_t[size[1]];
    memcpy(sps, nal[0], size[0]);
    memcpy(pps, nal[1], size[1]);
    auto spsBytes = pag::ByteData::MakeAdopted(sps, size[0]).release();
    auto ppsBytes = pag::ByteData::MakeAdopted(pps, size[1]).release();

    headers.push_back(spsBytes);
    headers.push_back(ppsBytes);
  }

  return count;
}

static pag::ByteData* EncodeVideoFrame(PAGEncoder* pagEncoder, uint8_t* rgbaBytes, int rowBytes,
                                       FrameType* pFrameType, int64_t* pOutTimeStamp) {

  pag::ByteData* videoBytes = nullptr;
  uint8_t* output = nullptr;
  auto size = pagEncoder->encodeRGBA(rgbaBytes, rowBytes, &output, pFrameType, pOutTimeStamp);
  if (size > 0 && output != nullptr) {
    auto bytes = new uint8_t[size];
    memcpy(bytes, output, size);
    videoBytes = pag::ByteData::MakeAdopted(bytes, size).release();
  }

  return videoBytes;
}

static FrameType DecideFrameType(bool isStatic, bool lastIsStatic, bool isVisible,
                                 bool lastIsVisible) {
  if (!isStatic && lastIsStatic) {
    return FRAME_TYPE_I;
  } else if (isVisible != lastIsVisible) {
    return FRAME_TYPE_I;
  } else if (!isVisible) {
    return FRAME_TYPE_P;
  } else {
    return FRAME_TYPE_AUTO;
  }
}

static void GetVisibleRanges(std::vector<pag::TimeRange>& ranges, pag::ID id,
                             pag::Composition* composition, pag::Frame startFrame,
                             pag::Frame endFrame) {

  if (composition->id == id) {
    pag::TimeRange range;
    range.start = MAX(startFrame, 0);
    range.end = MIN(composition->duration, endFrame);

    if (range.end >= range.start) {
      ranges.push_back(range);
    }
    return;
  }

  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
      auto newStartFrame = MAX(startFrame, layer->startTime);
      auto newEndFrame =
          MIN(endFrame, MIN(composition->duration, (layer->startTime + layer->duration)));

      newStartFrame -= preComposeLayer->compositionStartTime;
      newEndFrame -= preComposeLayer->compositionStartTime;

      GetVisibleRanges(ranges, id, preComposeLayer->composition, newStartFrame, newEndFrame);
    }
  }
}

static bool IsVisible(std::vector<pag::TimeRange>& ranges, pag::Frame frame,
                      float frameRateFactor) {
  for (auto range : ranges) {
    auto frame1 = static_cast<pag::Frame>(frame * frameRateFactor);
    auto frame2 = static_cast<pag::Frame>(ceil(frame * frameRateFactor));
    if ((frame1 >= range.start && frame1 <= range.end) ||
        (frame2 >= range.start && frame2 <= range.end)) {
      return true;
    }
  }
  return false;
}

void ExportVideoSequence(pagexporter::Context* context, const AEGP_ItemH& itemHandle,
                         pag::VideoComposition* composition, float factor, float frameRate,
                         float compositionFactor, int left, int top, int right, int bottom) {

  frameRate = std::min(frameRate, composition->frameRate);  // 没必要大于composition的帧率
  auto duration =
      static_cast<pag::Frame>(ceil(composition->duration * frameRate / composition->frameRate));

  factor *= compositionFactor;

  if (context->bitmapMaxResolution > 0) {
    int minLine = std::min(composition->width, composition->height) * compositionFactor;
    if (minLine > context->bitmapMaxResolution) {
      factor *= ((double)context->bitmapMaxResolution / minLine);
    }
  }

  auto& suites = context->suites;
  AEGP_RenderOptionsH renderOptions = nullptr;
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle,
                                                           &renderOptions));
  // 设置输出格式为8比特位图，避免出现16比特/32比特位深的情况
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8));

  bool bScale = true;
  int seqWidth = static_cast<int>(ceil((right - left) * factor));
  int seqHeight = static_cast<int>(ceil((bottom - top) * factor));
  if (factor > 0.99) {  // 在1.0附近或者大于1.0时，直接按原分辨率处理
    factor = 1.0;
    seqWidth = right - left;
    seqHeight = bottom - top;
    bScale = false;
  }

  int stride = SIZE_ALIGN(seqWidth) * 4;
  auto curData = new uint8_t[stride * SIZE_ALIGN(seqHeight) + stride * 2];
  auto preData = new uint8_t[stride * SIZE_ALIGN(seqHeight) + stride * 2];

  A_u_long renderStride = SIZE_ALIGN(composition->width) * 4;
  auto renderOffset = top * renderStride + left * 4;
  uint8_t* renderRgbaBytes = nullptr;

  std::vector<pag::TimeRange> visibleRanges;
  auto mainComposition = context->compositions[context->compositions.size() - 1];
  GetVisibleRanges(visibleRanges, composition->id, mainComposition, 0,
                   mainComposition->duration);  // 计算可见区间

  do {
    auto sequence = new pag::VideoSequence();

    bool hasAlpha = context->hasAlpha;
    sequence->composition = composition;
    sequence->frameRate = frameRate;
    sequence->width = seqWidth;
    sequence->height = seqHeight;

    PAGEncoder* pagEncoder = new PAGEncoder(context->bHardware);
    pagEncoder->init(seqWidth, seqHeight, frameRate, hasAlpha, context->bitmapKeyFrameInterval,
                     context->sequenceQuality);

    pagEncoder->getAlphaStartXY(&sequence->alphaStartX, &sequence->alphaStartY);

    EncodeVideoHeader(pagEncoder, sequence->headers);

    bool lastIsVisible = false;
    bool lastIsStatic = true;
    pag::TimeRange staticTimeRange = {-1, -1};

    for (int frame = 0; frame < duration && !context->bEarlyExit; frame++) {
      if (seqWidth == composition->width && seqHeight == composition->height) {
        renderRgbaBytes = curData;
      } else if (renderRgbaBytes == nullptr) {
        renderRgbaBytes =
            new uint8_t[renderStride * SIZE_ALIGN(composition->height) + renderStride * 2];
      }

      SetRenderTime(context, renderOptions, frameRate, frame);  // 设置render时间

      A_long comWidth = 0;
      A_long comHeight = 0;
      GetRenderFrame(renderRgbaBytes, renderStride, comWidth, comHeight, suites, renderOptions);

      if (comWidth == composition->width && comHeight == composition->height) {  // render成功

        auto isVisible =
            IsVisible(visibleRanges, frame,
                      (double)mainComposition->frameRate /
                          frameRate);  // 判断该帧是否可见，不可见则复制为相同帧（静止）
        if (!isVisible) {
          memset(curData, 128, stride * SIZE_ALIGN(seqHeight));
        } else {
          if (seqWidth != composition->width || seqHeight != composition->height) {
            if (bScale) {  // 缩放
              ScaleCoreGraphics(curData, stride, renderRgbaBytes + renderOffset, renderStride,
                                seqWidth, seqHeight, right - left, bottom - top);
            } else {
              CopyRGBA(curData, stride, renderRgbaBytes + renderOffset, renderStride, seqWidth,
                       seqHeight);
            }
          }

          if (!context->alphaDetected) {
            auto flag = DetectAlpha(curData, stride, seqWidth, seqHeight);
            if (flag && !hasAlpha) {
              context->hasAlpha = true;
              break;
            }
          }

          OddPaddingRGBA(curData, stride, seqWidth, seqHeight);
        }

        bool isStatic = false;
        if (frame > 0) {
          isStatic = ImageIsStatic(curData, preData, seqWidth, seqHeight, stride);
        }

        FrameType frameType = DecideFrameType(isStatic, lastIsStatic, isVisible, lastIsVisible);
        int64_t outTimeStamp = 0;

        pag::ByteData* videoBytes =
            EncodeVideoFrame(pagEncoder, curData, stride, &frameType, &outTimeStamp);

        if (videoBytes != nullptr) {
          auto videoFrame = new pag::VideoFrame();

          videoFrame->isKeyframe = (frameType == FRAME_TYPE_I);
          videoFrame->frame = outTimeStamp;
          videoFrame->fileBytes = videoBytes;

          sequence->frames.push_back(videoFrame);
        }

        if (!isStatic) {
          if (lastIsStatic && frame > 0) {  // 如果是运动，则写入上一个静止区间
            staticTimeRange.end = frame - 1;
            sequence->staticTimeRanges.push_back(staticTimeRange);
          }
          staticTimeRange.start = frame;     // 可能为下一个静态区间的开始点
        } else if (frame == duration - 1) {  // 静止，并且最后一帧结束了
          staticTimeRange.end = frame;
          sequence->staticTimeRanges.push_back(staticTimeRange);
        }
        lastIsStatic = isStatic;
        lastIsVisible = isVisible;

      } else {
        context->pushWarning(pagexporter::AlertInfoType::ExportRenderError);
      }

      if (context->progressWindow != nullptr) {
        context->progressWindow->increaseEncodedFrames();  // 更新进度
      }

      std::swap(curData, preData);
    }

    context->alphaDetected = true;
    if (context->hasAlpha != hasAlpha) {
      delete sequence;
      delete pagEncoder;
      continue;  // 如果实际alpha状态与预设不符，则重新导出。
    } else {
      // Flush delayed frames
      do {
        FrameType frameType = FRAME_TYPE_AUTO;
        int64_t outTimeStamp = 0;
        pag::ByteData* videoBytes =
            EncodeVideoFrame(pagEncoder, nullptr, 0, &frameType, &outTimeStamp);

        if (videoBytes != nullptr) {
          auto videoFrame = new pag::VideoFrame();

          videoFrame->isKeyframe = (frameType == FRAME_TYPE_I);
          videoFrame->frame = outTimeStamp;
          videoFrame->fileBytes = videoBytes;

          sequence->frames.push_back(videoFrame);
        } else {
          break;
        }
      } while (!context->bEarlyExit);
      if (EXPORT_MP4_HEADER) {
        pag::MP4BoxHelper::WriteMP4Header(sequence);
      }
      composition->sequences.push_back(sequence);
      delete pagEncoder;
      break;  // 如果实际alpha状态与预设相符，则结束。
    }
  } while (!context->bEarlyExit);

  delete[] curData;
  delete[] preData;
  if (seqWidth != composition->width || seqHeight != composition->height) {
    delete[] renderRgbaBytes;
  }

  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions));
}

void ClipVideoComposition(pagexporter::Context* context, const AEGP_ItemH& itemHandle,
                          pag::VideoComposition* composition, int& left, int& top, int& right,
                          int& bottom) {
  auto& suites = context->suites;
  int width = composition->width;
  int height = composition->height;
  auto frameRate = composition->frameRate;
  auto duration = composition->duration;

  AEGP_RenderOptionsH renderOptions = nullptr;
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_NewFromItem(context->pluginID, itemHandle,
                                                           &renderOptions));

  // 设置输出格式为8比特位图，避免出现16比特/32比特位深的情况
  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8));

  std::vector<pag::TimeRange> visibleRanges;
  auto mainComposition = context->compositions[context->compositions.size() - 1];
  GetVisibleRanges(visibleRanges, composition->id, mainComposition, 0,
                   mainComposition->duration);  // 计算可见区间

  uint8_t* data = nullptr;
  A_u_long stride = 0;

  left = width;
  top = height;
  right = -1;
  bottom = -1;

  int frame = 0;
  for (frame = 0; frame < duration && !context->bEarlyExit; frame++) {
    SetRenderTime(context, renderOptions, frameRate, frame);  // 设置render时间

    A_long comWidth = 0;
    A_long comHeight = 0;
    GetRenderFrame(data, stride, comWidth, comHeight, suites, renderOptions);

    if (comWidth == width && comHeight == height) {  // render成功
      auto isVisible =
          IsVisible(visibleRanges, frame,
                    (double)mainComposition->frameRate / frameRate);  // 判断该帧是否可见
      if (isVisible) {
        GetOpaqueRect(left, top, right, bottom, data, comWidth, comHeight, stride);

        if (right - left >= comWidth && bottom - top >= comHeight) {
          break;
        }
      }
    } else {
      context->pushWarning(pagexporter::AlertInfoType::ExportRenderError);
    }

    if (context->progressWindow != nullptr) {
      const float progressWeight = 0.25f;  // 相对正式编码来说速度较快，只算四分之一
      context->progressWindow->addEncodedFramesExtra(progressWeight);  // 更新进度
    }
  }

  if (right - left <= 0 || bottom - top <= 0) {
    context->pushWarning(pagexporter::AlertInfoType::VideoSequenceNoContent);

    left = 0;
    top = 0;
    right = std::min(16, width);
    bottom = std::min(16, height);
  } else if ((right - left) * (bottom - top) >= composition->width * composition->height * 0.9) {
    left = 0;
    top = 0;
    right = composition->width;
    bottom = composition->height;
  }

  delete[] data;

  CHECK_RET(suites.RenderOptionsSuite3()->AEGP_Dispose(renderOptions));
}
