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

#include "VideoSequence.h"
#include <QApplication>
#include "codec/mp4/MP4BoxHelper.h"
#include "export/encode/PAGEncodeThread.h"
#include "export/encode/VideoEncoder.h"
#include "utils/AEHelper.h"
#include "utils/ImageData.h"
#include "utils/LayerHelper.h"
#include "utils/UniqueID.h"

namespace exporter {

static void GetVisibleRanges(std::vector<pag::TimeRange>& ranges, pag::ID id,
                             pag::Composition* composition, pag::Frame startFrame,
                             pag::Frame endFrame) {
  if (composition->id == id) {
    pag::TimeRange range = {};
    range.start = std::max(startFrame, static_cast<pag::Frame>(0));
    range.end = std::min(composition->duration, endFrame);
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
      auto newStartFrame = std::max(startFrame, layer->startTime);
      newStartFrame -= preComposeLayer->compositionStartTime;

      auto newEndFrame =
          std::min(endFrame, std::min(composition->duration, (layer->startTime + layer->duration)));
      newEndFrame -= preComposeLayer->compositionStartTime;

      GetVisibleRanges(ranges, id, preComposeLayer->composition, newStartFrame, newEndFrame);
    }
  }
}

static bool IsFrameVisible(std::vector<pag::TimeRange>& ranges, pag::Frame frame,
                           float frameRateFactor) {
  return std::any_of(ranges.begin(), ranges.end(),
                     [frame, frameRateFactor](const pag::TimeRange& range) {
                       auto frame1 = static_cast<pag::Frame>(frame * frameRateFactor);
                       auto frame2 = static_cast<pag::Frame>(ceil(frame * frameRateFactor));
                       return (frame1 >= range.start && frame1 <= range.end) ||
                              (frame2 >= range.start && frame2 <= range.end);
                     });
}

static bool IsFrameExport(const pag::TimeRange& range, pag::Frame frame, float frameRateFactor) {
  auto frame1 = static_cast<pag::Frame>(frame * frameRateFactor);
  auto frame2 = static_cast<pag::Frame>(ceil(frame * frameRateFactor));
  return (frame1 >= range.start && frame1 <= range.end) ||
         (frame2 >= range.start && frame2 <= range.end);
}

static void ClipVideoComposition(std::shared_ptr<PAGExportSession> session,
                                 pag::VideoComposition* composition, int& left, int& top,
                                 int& right, int& bottom) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  auto compWidth = composition->width;
  auto compHeight = composition->height;
  auto frameRate = composition->frameRate;
  auto duration = composition->duration;

  auto itemIter = session->itemHandleMap.find(composition->id);
  if (itemIter == session->itemHandleMap.end()) {
    session->pushWarning(AlertInfoType::CompositionHandleNotFound, std::to_string(composition->id));
    return;
  }
  AEGP_ItemH itemH = itemIter->second;
  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemH, &renderOptions);
  if (renderOptions == nullptr) {
    session->pushWarning(AlertInfoType::ExportRenderError);
    return;
  }
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);

  std::vector<pag::TimeRange> visibleRanges = {};
  auto mainComposition = session->compositions[session->compositions.size() - 1];
  GetVisibleRanges(visibleRanges, composition->id, mainComposition, 0, mainComposition->duration);

  left = compWidth;
  top = compHeight;
  right = -1;
  bottom = -1;
  uint8_t* data = nullptr;
  for (pag::Frame frame = 0; frame < duration && !session->stopExport; frame++) {
    SetRenderTime(renderOptions, frameRate, frame);

    A_long width = 0;
    A_long height = 0;
    A_u_long srcStride = 0;
    A_u_long dstStride = 0;
    SetRenderTime(renderOptions, frameRate, frame);
    GetRenderFrameSize(renderOptions, srcStride, width, height);
    dstStride = 4 * width;
    if (data == nullptr) {
      data = new uint8_t[dstStride * height];
    }
    GetRenderFrame(data, srcStride, dstStride, width, height, renderOptions);
    if (compWidth == width && compHeight == height) {
      bool isVisible = IsFrameVisible(visibleRanges, frame,
                                      static_cast<float>(mainComposition->frameRate / frameRate));
      if (isVisible) {
        GetOpaqueRect(left, top, right, bottom, data, width, height, dstStride);
        if (right - left >= width && bottom - top >= height) {
          break;
        }
      }
    } else {
      session->pushWarning(AlertInfoType::ExportRenderError);
    }

    session->progressModel.addTotalSteps(1);
    session->progressModel.addFinishedSteps();
  }

  if (right - left <= 0 || bottom - top <= 0) {
    left = 0;
    top = 0;
    right = std::min(16, compWidth);
    bottom = std::min(16, compHeight);
    session->pushWarning(AlertInfoType::VideoSequenceNoContent);
  } else if ((right - left) * (bottom - top) >= composition->width * composition->height * 0.9) {
    left = 0;
    top = 0;
    right = compWidth;
    bottom = compHeight;
  }

  if (data != nullptr) {
    delete[] data;
  }
  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

static FrameType GetFrameType(bool currentFrameIsStatic, bool lastFrameIsStatic,
                              bool currentFrameIsVisible, bool lastFrameIsVisible) {
  if (!currentFrameIsStatic && lastFrameIsStatic) {
    return FrameType::FRAME_TYPE_I;
  }
  if (currentFrameIsVisible != lastFrameIsVisible) {
    return FrameType::FRAME_TYPE_I;
  }
  if (!currentFrameIsVisible) {
    return FrameType::FRAME_TYPE_P;
  }
  return FrameType::FRAME_TYPE_AUTO;
}

static void GetVideoSequence(std::shared_ptr<PAGExportSession> session,
                             pag::VideoComposition* composition, float compositionFactor, int left,
                             int top, int right, int bottom) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  composition->sequences.clear();

  auto needToScale = true;
  auto factor = compositionFactor;
  auto frameRate = std::min(session->configParam.frameRate, composition->frameRate);
  auto duration =
      static_cast<pag::Frame>(ceil(composition->duration * frameRate / composition->frameRate));

  if (session->configParam.bitmapMaxResolution > 0) {
    auto shorterSideLength = static_cast<float>(std::min(composition->width, composition->height));
    shorterSideLength *= compositionFactor;
    if (shorterSideLength > session->configParam.bitmapMaxResolution) {
      factor *= static_cast<float>(session->configParam.bitmapMaxResolution) / shorterSideLength;
    }
  }

  if (factor > 0.99) {
    factor = 1.0;
    needToScale = false;
  }

  auto seqWidth = static_cast<int>(ceil(static_cast<float>(right - left) * factor));
  auto seqHeight = static_cast<int>(ceil(static_cast<float>(bottom - top) * factor));
  auto seqStride = SIZE_ALIGN(seqWidth) * 4;
  auto sizeChanged = seqWidth != composition->width || seqHeight != composition->height;
  auto preData = pag::ByteData::Make(seqStride * SIZE_ALIGN(seqHeight) + seqStride * 2);
  auto curData = pag::ByteData::Make(seqStride * SIZE_ALIGN(seqHeight) + seqStride * 2);

  A_u_long renderStride = SIZE_ALIGN(composition->width) * 4;
  A_u_long renderOffset = top * renderStride + left * 4;

  auto rgbaData =
      pag::ByteData::Make(renderStride * SIZE_ALIGN(composition->height) + renderStride * 2);

  std::vector<pag::TimeRange> visibleRanges = {};
  auto mainComposition = session->compositions[session->compositions.size() - 1];
  GetVisibleRanges(visibleRanges, composition->id, mainComposition, 0, mainComposition->duration);
  pag::TimeRange maxVisibleRange = {-1, -1};
  if (!visibleRanges.empty()) {
    maxVisibleRange = visibleRanges[0];
  }
  for (const auto& range : visibleRanges) {
    if (range.start < maxVisibleRange.start) {
      maxVisibleRange.start = range.start;
    }
    if (range.end > maxVisibleRange.end) {
      maxVisibleRange.end = range.end;
    }
  }

  auto itemIter = session->itemHandleMap.find(composition->id);
  if (itemIter == session->itemHandleMap.end()) {
    session->pushWarning(AlertInfoType::CompositionHandleNotFound, std::to_string(composition->id));
    return;
  }
  AEGP_ItemH itemH = itemIter->second;
  AEGP_RenderOptionsH renderOptions = nullptr;
  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemH, &renderOptions);
  if (renderOptions == nullptr) {
    session->pushWarning(AlertInfoType::ExportRenderError);
    return;
  }
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  while (!session->stopExport) {
    auto sequence = new pag::VideoSequence();
    pag::Frame firstExportFrame = -1;
    pag::Frame exportFrameNum = 0;

    auto hasAlpha = session->videoHasAlpha;
    sequence->composition = composition;
    sequence->frameRate = frameRate;
    sequence->width = seqWidth;
    sequence->height = seqHeight;

    auto pagEncoder = std::make_unique<PAGEncoder>(session->hardwareEncode);
    pagEncoder->init(seqWidth, seqHeight, frameRate, hasAlpha,
                     session->configParam.bitmapKeyFrameInterval,
                     session->configParam.sequenceQuality);
    std::unique_ptr<PAGEncodeThread> pagEncodeThread = std::make_unique<PAGEncodeThread>();
    pagEncodeThread->setPAGEncoder(std::move(pagEncoder));
    pagEncodeThread->setEncodeFrameCallback(
        [&](std::unique_ptr<pag::ByteData> data, FrameType frameType, int64_t index) {
          auto videoFrame = new pag::VideoFrame();
          videoFrame->isKeyframe = (frameType == FrameType::FRAME_TYPE_I);
          videoFrame->frame = index;
          videoFrame->fileBytes = data.release();
          sequence->frames.push_back(videoFrame);
          session->progressModel.addFinishedSteps();
        });
    pagEncodeThread->setEncodeHeaderCallback([&](std::vector<pag::ByteData*> headers) {
      for (auto header : headers) {
        sequence->headers.push_back(header);
      }
    });

    pagEncodeThread->encodeHeaders();

    auto lastFrameIsVisible = false;
    auto lastFrameIsStatic = true;
    pag::TimeRange staticTimeRange = {-1, -1};
    for (pag::Frame frame = 0; frame < duration && !session->stopExport; frame++) {
      if (!IsFrameExport(maxVisibleRange, frame, mainComposition->frameRate / frameRate)) {
        if (firstExportFrame != -1) {
          session->progressModel.addFinishedSteps();
        }
        continue;
      }
      if (firstExportFrame == -1) {
        firstExportFrame = frame;
        session->videoCompositionStartTime[composition->uniqueID] = frame;
      }
      uint8_t* renderRgbaBytes = sizeChanged ? rgbaData->data() : curData->data();
      SetRenderTime(renderOptions, frameRate, frame);

      A_long compWidth = 0;
      A_long compHeight = 0;
      A_u_long compBytesLength = 0;
      GetRenderFrameSize(renderOptions, compBytesLength, compWidth, compHeight);
      GetRenderFrame(renderRgbaBytes, compBytesLength, renderStride, compWidth, compHeight,
                     renderOptions);
      if (compWidth == composition->width && compHeight == composition->height) {
        bool currentFrameIsVisible =
            IsFrameVisible(visibleRanges, frame, mainComposition->frameRate / frameRate);
        if (!currentFrameIsVisible) {
          std::memset(curData->data(), 128, curData->length());
        } else {
          if (sizeChanged) {
            if (needToScale) {
              ScalePixels(curData->data(), seqStride, seqWidth, seqHeight,
                          renderRgbaBytes + renderOffset, renderStride, right - left, bottom - top);
            } else {
              CopyRGBA(curData->data(), seqStride, renderRgbaBytes + renderOffset, renderStride,
                       seqWidth, seqHeight);
            }
          }

          if (!session->videoAlphaDetected) {
            if (hasAlpha != ImageHasAlpha(curData->data(), seqStride, seqWidth, seqHeight)) {
              hasAlpha = !hasAlpha;
              session->videoAlphaDetected = true;
              break;
            }
          }

          OddPaddingRGBA(curData->data(), seqStride, seqWidth, seqHeight);
        }

        auto currentFrameIsStatic = frame > 0 ? ImageIsStatic(curData->data(), preData->data(),
                                                              seqWidth, seqHeight, seqStride)
                                              : false;
        auto frameType = GetFrameType(currentFrameIsStatic, lastFrameIsStatic,
                                      currentFrameIsVisible, lastFrameIsVisible);
        pagEncodeThread->encodeFrame(curData, frameType, seqStride);
        exportFrameNum++;

        if (!currentFrameIsStatic) {
          if (lastFrameIsStatic && firstExportFrame != frame) {
            staticTimeRange.end = frame - 1;
            sequence->staticTimeRanges.push_back(staticTimeRange);
          }
          staticTimeRange.start = frame;
        } else if (frame == duration - 1) {
          staticTimeRange.end = frame;
          sequence->staticTimeRanges.push_back(staticTimeRange);
        }
        lastFrameIsStatic = currentFrameIsStatic;
        lastFrameIsVisible = currentFrameIsVisible;
      } else {
        session->pushWarning(AlertInfoType::ExportRenderError);
      }

      // Compensate progress for skipped frames before the first exported frame
      if (firstExportFrame == frame) {
        session->progressModel.addFinishedSteps(frame);
      }
      std::swap(curData, preData);
    }

    if (session->videoHasAlpha != hasAlpha) {
      session->videoHasAlpha = hasAlpha;
      delete sequence;
      continue;
    }

    pagEncodeThread->close();

    while (!session->stopExport && sequence->frames.size() < static_cast<size_t>(exportFrameNum) &&
           pagEncodeThread->isValid()) {
      QApplication::processEvents();
    }
    pagEncodeThread->getAlphaStartXY(&sequence->alphaStartX, &sequence->alphaStartY);

    pag::MP4BoxHelper::WriteMP4Header(sequence);
    composition->sequences.push_back(sequence);
    break;
  }

  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);
}

struct PreComposeReferenceContext {
  pag::Composition* original = nullptr;
  pag::Composition* replacement = nullptr;
};

static void ProcessLayerReference(std::shared_ptr<PAGExportSession>, pag::Layer* layer, void* ctx) {
  if (ctx == nullptr || layer->type() != pag::LayerType::PreCompose) {
    return;
  }
  auto context = static_cast<PreComposeReferenceContext*>(ctx);
  auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
  if (preComposeLayer->composition == context->original &&
      preComposeLayer->containingComposition != context->replacement) {
    preComposeLayer->composition = context->replacement;
  }
}

static void AdjustMainCompositionParam(pag::VectorComposition* newComposition,
                                       pag::VideoComposition* composition,
                                       std::vector<pag::Composition*>& compositions) {
  if (compositions.size() != 1) {
    return;
  }
  auto sequence = composition->sequences[0];
  auto factorWidth = static_cast<double>(sequence->width) / composition->width;
  auto factorHeight = static_cast<double>(sequence->height) / composition->height;
  auto factorSize = std::max(factorWidth, factorHeight);
  newComposition->width = static_cast<int>(floor(newComposition->width * factorSize));
  newComposition->height = static_cast<int>(floor(newComposition->height * factorSize));

  auto& transform = newComposition->layers[0]->transform;
  transform->anchorPoint->value.x *= factorSize;
  transform->anchorPoint->value.y *= factorSize;
  transform->position->value.x *= factorSize;
  transform->position->value.y *= factorSize;

  composition->width = sequence->width;
  composition->height = sequence->height;
  composition->frameRate = sequence->frameRate;
  composition->duration = static_cast<pag::Frame>(sequence->frames.size());
  newComposition->frameRate = sequence->frameRate;
  newComposition->duration = static_cast<pag::Frame>(sequence->frames.size());
}

static void RebuildVideoComposition(std::shared_ptr<PAGExportSession> session,
                                    pag::VideoComposition* composition,
                                    std::vector<pag::Composition*>& compositions, int left, int top,
                                    int right, int bottom) {
  auto newComposition = new pag::VectorComposition();
  auto newLayer = new pag::PreComposeLayer();

  newLayer->id = GetLayerUniqueID(session->compositions);
  newLayer->transform = new pag::Transform2D();
  newLayer->transform->position = new pag::Property<pag::Point>();
  newLayer->transform->anchorPoint = new pag::Property<pag::Point>();
  newLayer->transform->scale = new pag::Property<pag::Point>();
  newLayer->transform->rotation = new pag::Property<float>();
  newLayer->transform->opacity = new pag::Property<pag::Opacity>();

  newLayer->transform->anchorPoint->value.x = static_cast<float>(right - left) / 2.0f;
  newLayer->transform->anchorPoint->value.y = static_cast<float>(bottom - top) / 2.0f;
  newLayer->transform->position->value.x = static_cast<float>(left + right) / 2.0f;
  newLayer->transform->position->value.y = static_cast<float>(top + bottom) / 2.0f;
  newLayer->transform->scale->value.x = 1.0f;
  newLayer->transform->scale->value.y = 1.0f;
  newLayer->transform->rotation->value = 0.0f;
  newLayer->transform->opacity->value = pag::Opaque;
  newLayer->composition = composition;
  newLayer->containingComposition = newComposition;
  newLayer->startTime = 0;
  newLayer->duration = composition->duration;

  auto originalID = composition->id;
  AEGP_ItemH originalItemHandle = nullptr;
  auto itemIter = session->itemHandleMap.find(originalID);
  if (itemIter != session->itemHandleMap.end()) {
    originalItemHandle = itemIter->second;
  }

  newComposition->layers.push_back(newLayer);
  newComposition->width = composition->width;
  newComposition->height = composition->height;
  newComposition->duration = composition->duration;
  newComposition->frameRate = composition->frameRate;
  newComposition->id = originalID;
  newComposition->backgroundColor = composition->backgroundColor;

  newComposition->audioBytes = composition->audioBytes;
  newComposition->audioMarkers = composition->audioMarkers;
  newComposition->audioStartTime = composition->audioStartTime;
  composition->audioBytes = nullptr;
  composition->audioMarkers.clear();
  composition->audioStartTime = pag::ZeroFrame;
  auto iter = std::find(compositions.begin(), compositions.end(), composition);
  if (iter != compositions.end()) {
    compositions.insert(iter + 1, newComposition);
  }

  auto sessionIter =
      std::find(session->compositions.begin(), session->compositions.end(), composition);
  if (sessionIter != session->compositions.end()) {
    session->compositions.insert(sessionIter + 1, newComposition);
  }

  PreComposeReferenceContext context{composition, newComposition};
  TraversalLayers(session, compositions, pag::LayerType::PreCompose, &ProcessLayerReference,
                  &context);

  composition->id = GetCompositionUniqueID(session->compositions);
  if (originalItemHandle != nullptr) {
    session->itemHandleMap[newComposition->id] = originalItemHandle;
    session->itemHandleMap[composition->id] = originalItemHandle;
  }
  composition->width = right - left;
  composition->height = bottom - top;
  AdjustMainCompositionParam(newComposition, composition, compositions);
}

void GetVideoSequence(std::shared_ptr<PAGExportSession> session,
                      std::vector<pag::Composition*>& compositions,
                      pag::VideoComposition* composition, float factor) {
  int left = 0;
  int right = 0;
  int top = 0;
  int bottom = 0;
  ClipVideoComposition(session, composition, left, top, right, bottom);

  if ((right - left) * (bottom - top) >= composition->width * composition->height * 0.9) {
    GetVideoSequence(session, composition, factor, 0, 0, composition->width, composition->height);
  } else {
    GetVideoSequence(session, composition, factor, left, top, right, bottom);
    RebuildVideoComposition(session, composition, compositions, left, top, right, bottom);
  }
}
}  // namespace exporter
