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

#include "BitmapSequence.h"
#include <webp/encode.h>
#include "utils/AEHelper.h"
#include "utils/ImageData.h"

namespace exporter {

static bool IsKeyFrame(pag::Frame curFrame, pag::Frame lastKeyFrame, int diffSize, int fullSize,
                       int keyFrameRate) {
  pag::Frame frameDistance = curFrame - lastKeyFrame;
  if (curFrame == 0) {
    return true;
  }
  if (diffSize == 0) {
    return false;
  }
  if (diffSize == fullSize) {
    return true;
  }
  if (diffSize > (fullSize * 0.9) && frameDistance > 5) {
    return true;
  }
  if (diffSize > (fullSize * 0.75) && keyFrameRate > 20 && frameDistance > (keyFrameRate / 2)) {
    return true;
  }
  if (keyFrameRate > 0 && frameDistance > keyFrameRate) {
    return true;
  }

  return false;
}

void GetBitmapSequence(std::shared_ptr<PAGExportSession> session,
                       pag::BitmapComposition* composition, float compositionFactor) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  auto itemIter = session->itemHandleMap.find(composition->id);
  if (itemIter == session->itemHandleMap.end()) {
    session->pushWarning(AlertInfoType::CompositionHandleNotFound, std::to_string(composition->id));
    return;
  }
  AEGP_ItemH itemHandle = itemIter->second;
  float factor = compositionFactor;
  float frameRate = std::min(session->configParam.frameRate, composition->frameRate);
  auto duration =
      static_cast<pag::Frame>(ceil(composition->duration * frameRate / composition->frameRate));

  if (session->configParam.bitmapMaxResolution > 0) {
    int shorterSideLength =
        static_cast<int>(std::min(composition->width, composition->height) * compositionFactor);
    if (shorterSideLength > session->configParam.bitmapMaxResolution) {
      factor *= static_cast<float>(session->configParam.bitmapMaxResolution) /
                static_cast<float>(shorterSideLength);
    }
  }

  if (factor > 0.99) {
    factor = 1.0;
  }

  auto sequence = new pag::BitmapSequence();
  composition->sequences.push_back(sequence);
  sequence->composition = composition;
  sequence->frameRate = frameRate;
  sequence->width = static_cast<int>(ceil(composition->width * factor));
  sequence->height = static_cast<int>(ceil(composition->height * factor));

  int width = sequence->width;
  int height = sequence->height;
  int stride = width * 4;
  int compositionStride = composition->width * 4;
  bool needToScale = (width != composition->width || height != composition->height);
  std::vector<uint8_t> preData(stride * height + stride * 2);
  std::vector<uint8_t> curData(stride * height + stride * 2);
  std::vector<uint8_t> rgbaData(compositionStride * composition->height + compositionStride * 2);

  ImageRect lastKeyFrameDiffRect = {0, 0, width, height};
  pag::Frame lastKeyFrame = 0;
  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return;
  }
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  for (pag::Frame frame = 0; frame < duration && !session->stopExport; frame++) {
    A_long frameWidth = 0;
    A_long frameHeight = 0;
    A_u_long frameStride = 0;
    uint8_t* buffer = needToScale ? rgbaData.data() : curData.data();
    auto bitmapFrame = new pag::BitmapFrame();
    sequence->frames.push_back(bitmapFrame);

    SetRenderTime(renderOptions, frameRate, frame);
    GetRenderFrameSize(renderOptions, frameStride, frameWidth, frameHeight);
    GetRenderFrame(buffer, frameStride, compositionStride, frameWidth, frameHeight, renderOptions);
    if (frameWidth == composition->width && frameHeight == composition->height) {
      if (needToScale) {
        ScalePixels(curData.data(), stride, width, height, buffer, compositionStride, frameWidth,
                    frameHeight);
      }

      ImageRect diffRect = {0, 0, width, height};
      GetImageDiffRect(diffRect, preData.data(), curData.data(), width, height, stride);

      ImageRect encodeRect = {0, 0, width, height};
      bool isKeyFrame = IsKeyFrame(frame, lastKeyFrame, diffRect.width * diffRect.height,
                                   width * height, session->configParam.bitmapKeyFrameInterval);
      if (isKeyFrame) {
        diffRect.xPos = 0;
        diffRect.yPos = 0;
        diffRect.width = width;
        diffRect.height = height;
        lastKeyFrame = frame;
        ClipTransparentEdge(diffRect, curData.data(), width, height, stride);
        encodeRect = diffRect;
      } else if (diffRect.width > 0 && diffRect.height > 0) {
        ExpandRectRange(encodeRect, diffRect, lastKeyFrameDiffRect, width, height, 4);
      }

      if (diffRect.width > 0 && diffRect.height > 0) {
        auto data = curData.data() + encodeRect.yPos * stride + encodeRect.xPos * 4;
        auto bitmapData = EncodeImageData(data, encodeRect.width, encodeRect.height, stride,
                                          session->configParam.imageQuality);
        if (bitmapData == nullptr) {
          session->pushWarning(AlertInfoType::WebpEncodeError);
        }

        auto bitmapRect = new pag::BitmapRect();
        bitmapFrame->bitmaps.push_back(bitmapRect);
        bitmapRect->x = encodeRect.xPos;
        bitmapRect->y = encodeRect.yPos;
        bitmapRect->fileBytes = bitmapData;
      }
      bitmapFrame->isKeyframe = isKeyFrame;
      lastKeyFrameDiffRect = diffRect;
    } else {
      session->pushWarning(AlertInfoType::ExportRenderError);
    }

    session->progressModel.addFinishedSteps();
    std::swap(curData, preData);
  }

  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

}  // namespace exporter
