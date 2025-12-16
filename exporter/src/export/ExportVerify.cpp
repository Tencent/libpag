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

#include "ExportVerify.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include "utils/AEHelper.h"
#include "utils/LayerHelper.h"
#include "utils/ScopedHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

static bool IsStaticVideoSequence(pag::Composition* composition) {
  if (composition->type() == pag::CompositionType::Video) {
    auto sequences = static_cast<pag::VideoComposition*>(composition)->sequences;
    if (!sequences.empty()) {
      auto staticTimeRanges = sequences[0]->staticTimeRanges;
      if (staticTimeRanges.size() == 1 &&
          (staticTimeRanges[0].end - staticTimeRanges[0].start + 1 == sequences[0]->duration())) {
        return true;
      }
    }
  }
  return false;
}

static size_t GetMaxCountInSameTimeRange(
    std::vector<std::pair<pag::TimeRange, size_t>>& timeRangeList,
    pag::Frame* startTime = nullptr) {
  if (timeRangeList.empty()) {
    return 0;
  }

  std::vector<std::pair<pag::Frame, size_t>> startTimeList = {};
  std::vector<std::pair<pag::Frame, size_t>> endTimeList = {};

  for (auto timeRange : timeRangeList) {
    startTimeList.emplace_back(timeRange.first.start, timeRange.second);
    endTimeList.emplace_back(timeRange.first.end, timeRange.second);
  }

  auto timeCompareFunction = [](const std::pair<pag::Frame, size_t> a,
                                const std::pair<pag::Frame, size_t> b) -> bool {
    return a.first < b.first;
  };

  sort(startTimeList.begin(), startTimeList.end(), timeCompareFunction);
  sort(endTimeList.begin(), endTimeList.end(), timeCompareFunction);

  size_t startTimeIndex = 0;
  size_t endTimeIndex = 0;
  size_t maxCount = 0;
  size_t count = 0;
  pag::Frame maxCountFrame = 0;
  while (startTimeIndex < startTimeList.size()) {
    if (startTimeList[startTimeIndex].first < endTimeList[endTimeIndex].first) {
      count += startTimeList[startTimeIndex].second;
      if (maxCount < count) {
        maxCount = count;
        maxCountFrame = startTimeList[startTimeIndex].first;
      }
      startTimeIndex++;
    } else {
      count -= endTimeList[endTimeIndex].second;
      endTimeIndex++;
    }
  }

  if (startTime != nullptr) {
    *startTime = maxCountFrame;
  }
  return maxCount;
}

static void CheckBitmapAndVideoNum(std::shared_ptr<PAGExportSession> session,
                                   std::vector<pag::Composition*>& compositions,
                                   std::vector<pag::ImageBytes*>& images) {
  constexpr size_t maxImageNum = 30;
  constexpr size_t maxBmpCompositionNum = 3;

  size_t staticVideoSequenceNum = 0;
  size_t sequenceCompositionNum = 0;
  for (auto composition : compositions) {
    if (composition->type() != pag::CompositionType::Vector) {
      if (IsStaticVideoSequence(composition)) {
        staticVideoSequenceNum++;
      } else {
        sequenceCompositionNum++;
      }
    }
  }

  auto imageNum = images.size() + staticVideoSequenceNum;
  if (imageNum > maxImageNum) {
    session->pushWarning(AlertInfoType::ImageNum, std::to_string(imageNum));
    return;
  }

  if (sequenceCompositionNum > maxBmpCompositionNum) {
    session->pushWarning(AlertInfoType::BmpCompositionNum, std::to_string(sequenceCompositionNum));
  }
}

static void GetLayerTimeRangeAndEffectCount(std::shared_ptr<PAGExportSession>, pag::Layer* layer,
                                            pag::Frame compositionStartTime, void* ctx) {
  size_t count = layer->layerStyles.size() + layer->effects.size();
  if (count == 0) {
    return;
  }

  auto timeRangeList = static_cast<std::vector<std::pair<pag::TimeRange, size_t>>*>(ctx);
  pag::TimeRange timeRange = {layer->startTime + compositionStartTime,
                              layer->startTime + layer->duration + compositionStartTime};
  timeRangeList->emplace_back(timeRange, count);
}

static void CheckEffect(std::shared_ptr<PAGExportSession> session, pag::Composition* composition) {
  constexpr size_t maxEffectCountInSameTimeRange = 3;

  std::vector<std::pair<pag::TimeRange, size_t>> timeRangeCounts = {};
  TraversalLayers(session, composition, pag::LayerType::Unknown, 0,
                  &GetLayerTimeRangeAndEffectCount, &timeRangeCounts);
  pag::Frame startTime = 0;
  if (GetMaxCountInSameTimeRange(timeRangeCounts, &startTime) > maxEffectCountInSameTimeRange) {
    session->pushWarning(AlertInfoType::EffectAndStylePickNum, std::to_string(startTime));
  }
}

static bool IsLayerHasVideoTrack(pag::Layer* layer) {
  return std::any_of(layer->markers.begin(), layer->markers.end(), [](pag::Marker* marker) {
    auto pos1 = marker->comment.find("{\"videoTrack\" : 1}");
    auto pos2 = marker->comment.find("{\"videoTrack\":1}");
    return pos1 != std::string::npos || pos2 != std::string::npos;
  });
}

static void GetLayerVideoTrackTimeRangeAndCount(std::shared_ptr<PAGExportSession>,
                                                pag::Layer* layer, pag::Frame compositionStartTime,
                                                void* ctx) {
  if (!IsLayerHasVideoTrack(layer)) {
    return;
  }

  auto timeRangeList = static_cast<std::vector<std::pair<pag::TimeRange, size_t>>*>(ctx);
  pag::TimeRange timeRange = {layer->startTime + compositionStartTime,
                              layer->startTime + layer->duration + compositionStartTime};
  timeRangeList->emplace_back(timeRange, 1);
}

static void GetNameOfLayerWithVideoTrack(std::shared_ptr<PAGExportSession>, pag::Layer* layer,
                                         pag::Frame compositionStartTime, void* ctx) {
  if (!IsLayerHasVideoTrack(layer)) {
    return;
  }

  if ((layer->startTime + layer->duration) < compositionStartTime ||
      layer->startTime > compositionStartTime) {
    return;
  }
  auto layerNames = static_cast<std::string*>(ctx);
  std::string name = "\"" + layer->name + "\"";
  if (layerNames->empty()) {
    *layerNames += name;
  } else {
    *layerNames = *layerNames + ", " + name;
  }
}

static void CheckVideoTrack(std::shared_ptr<PAGExportSession> session,
                            pag::Composition* composition) {
  constexpr size_t maxVideoTrackNum = 2;

  std::vector<std::pair<pag::TimeRange, size_t>> timeRangeList = {};
  TraversalLayers(session, composition, pag::LayerType::Image, 0,
                  &GetLayerVideoTrackTimeRangeAndCount, &timeRangeList);
  pag::Frame startTime = 0;
  if (GetMaxCountInSameTimeRange(timeRangeList, &startTime) > maxVideoTrackNum) {
    std::string layerNames;
    TraversalLayers(session, composition, pag::LayerType::Image, startTime,
                    &GetNameOfLayerWithVideoTrack, &layerNames);
    session->pushWarning(AlertInfoType::VideoTrackPeakNum, std::to_string(startTime));
  }
}

static void CheckContinuousSequenceComposition(std::shared_ptr<PAGExportSession> session,
                                               pag::Composition* composition) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  bool lastIsSequence = false;
  auto lastBlendMode = pag::BlendMode::Normal;
  auto lastCompostionId = pag::ZeroID;
  ScopedAssign<pag::ID> compID(session->compID, composition->id);

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;
      if (lastCompostionId != subCompostion->id) {
        lastCompostionId = subCompostion->id;
        if (subCompostion->type() != pag::CompositionType::Vector) {
          if (lastIsSequence && layer->blendMode == lastBlendMode) {
            session->pushWarning(AlertInfoType::ContinuousSequence);
          }
          lastIsSequence = true;
          lastBlendMode = layer->blendMode;
        } else {
          CheckContinuousSequenceComposition(session, subCompostion);
          lastIsSequence = false;
        }
      }
    } else {
      lastIsSequence = false;
    }
  }
}

static void CheckBmpSuffix(std::shared_ptr<PAGExportSession> session,
                           pag::Composition* composition) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  ScopedAssign<pag::ID> compID(session->compID, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;
      if (subCompostion->type() != pag::CompositionType::Vector) {
        continue;
      }
      if (session->configParam.isTagCodeSupport(pag::TagCode::VideoSequence)) {
        if (IsEndWidthSuffix(layer->name, CompositionBmpSuffix)) {
          session->pushWarning(AlertInfoType::BmpLayerButVectorComp);
        }
      }
      CheckBmpSuffix(session, subCompostion);
    } else {
      if (IsEndWidthSuffix(layer->name, CompositionBmpSuffix)) {
        session->pushWarning(AlertInfoType::NoPrecompLayerWithBmpName);
      }
    }
  }
}

static void CheckLayerNum(std::shared_ptr<PAGExportSession> session,
                          std::vector<pag::Composition*>& compositions) {
  constexpr size_t maxLayerNum = 60;

  size_t layerNum = 0;
  for (auto composition : compositions) {
    if (composition->type() != pag::CompositionType::Vector) {
      layerNum++;
      continue;
    }
    for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
      if (layer->type() == pag::LayerType::PreCompose) {
        continue;
      }
      layerNum++;
    }
  }

  if (layerNum > maxLayerNum) {
    session->pushWarning(AlertInfoType::LayerNum);
  }
}

static void ZeroingTimeRemapOffset(pag::AnimatableProperty<pag::Frame>* timeRemap) {
  auto keyFrames = timeRemap->keyframes;
  pag::Frame offset = keyFrames[0]->startValue;
  for (auto keyFrame : keyFrames) {
    offset = std::min(offset, keyFrame->startValue);
  }
  for (auto keyFrame : keyFrames) {
    keyFrame->startValue -= offset;
    keyFrame->endValue -= offset;
  }
}

static bool IsKeyFrameDurationLongEnough(pag::Keyframe<pag::Frame>* keyFrame) {
  return (keyFrame->endTime - keyFrame->startTime) > 2;
}

static void CheckTimeRemapPlaySpeed(std::shared_ptr<PAGExportSession> session,
                                    pag::AnimatableProperty<pag::Frame>* timeRemap) {
  constexpr double maxPlaySpeed = 8.0;
  for (auto keyFrame : timeRemap->keyframes) {
    auto speed = static_cast<double>(keyFrame->endValue - keyFrame->startValue) /
                 static_cast<double>(keyFrame->endTime - keyFrame->startTime);
    if (speed > maxPlaySpeed && IsKeyFrameDurationLongEnough(keyFrame)) {
      session->pushWarning(AlertInfoType::VideoSpeedTooFast);
    }
  }
}

static void CheckTimeRemapPlayBackwards(std::shared_ptr<PAGExportSession> session,
                                        pag::AnimatableProperty<pag::Frame>* timeRemap) {
  for (auto keyFrame : timeRemap->keyframes) {
    if (keyFrame->startValue > keyFrame->endValue &&
        (keyFrame->startTime + 1 != keyFrame->endTime) && IsKeyFrameDurationLongEnough(keyFrame)) {
      session->pushWarning(AlertInfoType::VideoPlayBackward);
    }
  }
}

static void CheckLayerImageFillRule(std::shared_ptr<PAGExportSession> session, pag::Layer* layer,
                                    void*) {
  auto imageFillRule = static_cast<pag::ImageLayer*>(layer)->imageFillRule;
  if (imageFillRule == nullptr) {
    return;
  }
  if (imageFillRule->timeRemap == nullptr || !imageFillRule->timeRemap->animatable()) {
    return;
  }
  auto timeRemap = static_cast<pag::AnimatableProperty<pag::Frame>*>(imageFillRule->timeRemap);
  ZeroingTimeRemapOffset(timeRemap);
  CheckTimeRemapPlaySpeed(session, timeRemap);
  CheckTimeRemapPlayBackwards(session, timeRemap);
}

static void CheckImageFillRule(std::shared_ptr<PAGExportSession> session,
                               pag::Composition* composition) {
  TraversalLayers(session, composition, pag::LayerType::Image, &CheckLayerImageFillRule,
                  composition);
}

static void CheckVideoCompositionInUIScenes(std::shared_ptr<PAGExportSession> session,
                                            std::vector<pag::Composition*>& compositions) {
  if (session->configParam.scenes != ExportScenes::UI) {
    return;
  }
  for (auto composition : compositions) {
    if (composition->type() == pag::CompositionType::Video) {
      ScopedAssign<pag::ID> compID(session->compID, composition->id);
      session->pushWarning(AlertInfoType::VideoSequenceInUiScene);
    }
  }
}

static bool CompareVideoComposition(std::shared_ptr<PAGExportSession> session,
                                    pag::VideoComposition* composition1,
                                    pag::VideoComposition* composition2) {
  if (composition1->width != composition2->width || composition1->height != composition2->height) {
    return false;
  }

  if (composition1->duration != composition2->duration ||
      composition1->frameRate != composition2->frameRate) {
    return false;
  }

  for (pag::Frame frame = 0; frame < composition1->duration && !session->stopExport; frame++) {
    auto itemIter1 = session->itemHandleMap.find(composition1->id);
    auto itemIter2 = session->itemHandleMap.find(composition2->id);
    if (itemIter1 == session->itemHandleMap.end() || itemIter2 == session->itemHandleMap.end()) {
      return false;
    }
    auto image1 = GetCompositionFrameImage(itemIter1->second, frame);
    auto image2 = GetCompositionFrameImage(itemIter2->second, frame);
    if (image1 != image2) {
      return false;
    }
  }

  return true;
}

static void CheckDuplicateVideoSequence(std::shared_ptr<PAGExportSession> session,
                                        std::vector<pag::Composition*>& compositions) {
  if (!session->exportStaticCompAsBmp) {
    return;
  }

  std::vector<pag::Composition*> videoCompositions = {};
  for (auto composition : compositions) {
    if (composition->type() == pag::CompositionType::Video) {
      videoCompositions.push_back(composition);
    }
  }

  if (videoCompositions.size() < 2) {
    return;
  }

  for (size_t i = 0; i < videoCompositions.size() - 1; i++) {
    for (size_t j = i + 1; j < videoCompositions.size(); j++) {
      ScopedAssign<pag::ID> compID(session->compID, videoCompositions[i]->id);
      if (CompareVideoComposition(session,
                                  static_cast<pag::VideoComposition*>(videoCompositions[i]),
                                  static_cast<pag::VideoComposition*>(videoCompositions[j]))) {
        session->pushWarning(AlertInfoType::SameSequence);
      }
    }
  }
}

static bool HasChineseCharacter(std::string str) {
  auto len = str.length();
  bool betweenDoubleQuotationMark = false;
  for (size_t index = 0; index < len; index++) {
    if (str[index] == '\"') {
      betweenDoubleQuotationMark = !betweenDoubleQuotationMark;
    }
    if (str[index] & 0x80) {
      if (!betweenDoubleQuotationMark) {
        return true;
      }
      index++;
    }
  }

  return false;
}

static void CheckLayerMarkerCharacter(std::shared_ptr<PAGExportSession> session,
                                      pag::Layer* layer) {
  for (auto marker : layer->markers) {
    auto& comment = marker->comment;

    /* Normal Json */
    QJsonParseError parseError;
    QJsonDocument::fromJson(comment.c_str(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
      continue;
    }

    /* Normal Comment */
    auto posStart = comment.find_first_of('{');
    auto posEnd = comment.find_first_of('}');
    if (posStart == std::string::npos || posEnd == std::string::npos) {
      continue;
    }

    if (HasChineseCharacter(comment)) {
      session->pushWarning(AlertInfoType::MarkerJsonHasChinese, comment);
    } else {
      session->pushWarning(AlertInfoType::MarkerJsonGrammar, comment);
    }
  }
}

static void CheckLayerDisplacementMap(std::shared_ptr<PAGExportSession> session,
                                      pag::Layer* layer) {
  for (auto effect : layer->effects) {
    if (effect->type() == pag::EffectType::DisplacementMap) {
      auto displacementMapLayer =
          static_cast<pag::DisplacementMapEffect*>(effect)->displacementMapLayer;
      if (displacementMapLayer != nullptr && displacementMapLayer->id == layer->id) {
        session->pushWarning(AlertInfoType::DisplacementMapRefSelf);
      }
    }
  }
}

static void CheckLayerFlags(std::shared_ptr<PAGExportSession> session, pag::Layer* layer) {
  const auto& Suites = GetSuites();

  auto layerHandle = session->layerHandleMap[layer->id];
  if (layerHandle == nullptr) {
    return;
  }
  AEGP_LayerFlags layerFlags;
  Suites->LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &layerFlags);
  if (layerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
    session->pushWarning(AlertInfoType::AdjustmentLayer);
  }
  if (layerFlags & AEGP_LayerFlag_TIME_REMAPPING) {
    session->pushWarning(AlertInfoType::LayerTimeRemapping);
  }
  if ((layerFlags & AEGP_LayerFlag_LAYER_IS_3D) &&
      !session->configParam.isTagCodeSupport(pag::TagCode::Transform3D)) {
    session->pushWarning(AlertInfoType::Layer3D);
  }
  if (layerFlags & AEGP_LayerFlag_MOTION_BLUR &&
      !session->configParam.isTagCodeSupport(pag::TagCode::LayerAttributesExtra)) {
    session->pushWarning(AlertInfoType::TagLevelMotionBlur);
  }
}

static std::string TextSelectorBasedOnToString(pag::TextSelectorBasedOn basedOn) {
  switch (basedOn) {
    case pag::TextSelectorBasedOn::Characters:
      return "字符（Characters）";
    case pag::TextSelectorBasedOn::CharactersExcludingSpaces:
      return "不包含空格的字符（CharactersExcludingSpaces）";
    case pag::TextSelectorBasedOn::Words:
      return "词（Words）";
    case pag::TextSelectorBasedOn::Lines:
      return "行（Lines）";
    default:
      return "未知（Unknown）";
  }
}

static void CheckTextLayerAnimator(std::shared_ptr<PAGExportSession> session,
                                   pag::TextLayer* layer) {
  for (auto animator : layer->animators) {
    for (auto selector : animator->selectors) {
      if (!selector->verify()) {
        session->pushWarning(AlertInfoType::ExportRangeSlectorError);
      }
      if (selector->type() == pag::TextSelectorType::Range) {
        auto rangeSelector = static_cast<pag::TextRangeSelector*>(selector);
        if (rangeSelector->units != pag::TextRangeSelectorUnits::Percentage) {
          session->pushWarning(AlertInfoType::RangeSelectorUnitsIndex);
        }
        if (rangeSelector->basedOn != pag::TextSelectorBasedOn::Characters) {
          session->pushWarning(AlertInfoType::RangeSelectorBasedOn,
                               TextSelectorBasedOnToString(rangeSelector->basedOn));
        }
        if (rangeSelector->shape == pag::TextRangeSelectorShape::Square &&
            (rangeSelector->smoothness->animatable() || rangeSelector->smoothness->value != 1.0f)) {
          session->pushWarning(AlertInfoType::RangeSelectorSmoothness);
        }
      } else if (selector->type() == pag::TextSelectorType::Wiggly) {
        auto wigglySelector = static_cast<pag::TextWigglySelector*>(selector);
        if (wigglySelector->basedOn != pag::TextSelectorBasedOn::Characters) {
          session->pushWarning(AlertInfoType::WigglySelectorBasedOn,
                               TextSelectorBasedOnToString(wigglySelector->basedOn));
        }
      }
    }
  }
}

static void CheckTextLaterTransform(std::shared_ptr<PAGExportSession> session,
                                    pag::TextLayer* layer) {
  constexpr float largeScale = 5.0f;
  constexpr float smallFontSize = 5.0f;

  if (layer->transform == nullptr) {
    return;
  }
  if (layer->transform->scale == nullptr || layer->transform->scale->animatable()) {
    return;
  }
  auto& scale = layer->transform->scale->value;
  auto minScale = std::min(scale.x, scale.y);
  if (minScale < largeScale) {
    return;
  }
  if (layer->sourceText == nullptr || layer->sourceText->animatable()) {
    return;
  }

  auto fontSize = layer->getTextDocument()->fontSize;
  if (fontSize <= smallFontSize) {
    session->pushWarning(AlertInfoType::FontSmallAndScaleLarge,
                         std::to_string(static_cast<int>(minScale)));
  }
}

static bool HasVerticalText(pag::Property<pag::TextDocumentHandle>* sourceText) {
  if (sourceText == nullptr) {
    return false;
  }

  if (sourceText->animatable()) {
    for (auto keyframe :
         reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)
             ->keyframes) {
      if (keyframe->startValue->direction == pag::TextDirection::Vertical ||
          keyframe->endValue->direction == pag::TextDirection::Vertical) {
        return true;
      }
    }
  } else {
    auto textDocument = sourceText->getValueAt(0);
    if (textDocument->direction == pag::TextDirection::Vertical) {
      return true;
    }
  }

  return false;
}

static void CheckTextLayerPathOption(std::shared_ptr<PAGExportSession> session,
                                     pag::TextLayer* layer) {
  if (layer->pathOption == nullptr || layer->pathOption->path == nullptr) {
    return;
  }

  auto perpendicularToPath = layer->pathOption->perpendicularToPath;
  auto forceAlignment = layer->pathOption->forceAlignment;
  if (perpendicularToPath->animatable() || perpendicularToPath->value != true) {
    session->pushWarning(AlertInfoType::TextPathParamPerpendicularToPath);
  }
  if (forceAlignment->animatable() || forceAlignment->value != false) {
    session->pushWarning(AlertInfoType::TextPathParamForceAlignment);
  }

  if (HasVerticalText(layer->sourceText)) {
    session->pushWarning(AlertInfoType::TextPathVertial);
  }

  if (!layer->animators.empty()) {
    session->pushWarning(AlertInfoType::TextPathAnimator);
  }
}

static void CheckLayerProperty(std::shared_ptr<PAGExportSession> session, pag::Layer* layer,
                               void*) {
  CheckLayerMarkerCharacter(session, layer);
  CheckLayerDisplacementMap(session, layer);
  CheckLayerFlags(session, layer);
  if (layer->type() == pag::LayerType::Text) {
    CheckTextLayerAnimator(session, static_cast<pag::TextLayer*>(layer));
    CheckTextLaterTransform(session, static_cast<pag::TextLayer*>(layer));
    CheckTextLayerPathOption(session, static_cast<pag::TextLayer*>(layer));
  }
}

static void CheckLayerProperty(std::shared_ptr<PAGExportSession> session,
                               std::vector<pag::Composition*>& compositions) {
  TraversalLayers(session, compositions, pag::LayerType::Unknown, &CheckLayerProperty, nullptr);
}

static void CheckStaticVideoSequence(std::shared_ptr<PAGExportSession> session,
                                     std::vector<pag::Composition*>& compositions) {
  for (auto composition : compositions) {
    if (IsStaticVideoSequence(composition)) {
      ScopedAssign<pag::ID> compID(session->compID, composition->id);
      session->pushWarning(AlertInfoType::StaticVideoSequence);
    }
  }
}

void CheckBeforeExport(std::shared_ptr<PAGExportSession> session,
                       std::vector<pag::Composition*>& compositions,
                       std::vector<pag::ImageBytes*>& images) {
  CheckBitmapAndVideoNum(session, compositions, images);
  CheckEffect(session, compositions.back());
  CheckVideoTrack(session, compositions.back());
  CheckContinuousSequenceComposition(session, compositions.back());
  CheckBmpSuffix(session, compositions.back());
  CheckLayerNum(session, compositions);
  CheckImageFillRule(session, compositions.back());
  CheckVideoCompositionInUIScenes(session, compositions);
  CheckDuplicateVideoSequence(session, compositions);
  CheckLayerProperty(session, compositions);
}

void CheckAfterExport(std::shared_ptr<PAGExportSession> session,
                      std::vector<pag::Composition*>& compositions) {
  CheckStaticVideoSequence(session, compositions);
}

void CheckGraphicsMemory(std::shared_ptr<PAGExportSession> session,
                         std::shared_ptr<pag::File> pagFile) {
  constexpr int64_t maxGraphicsMemorySizeForUI = 30 * 1024 * 1024;
  constexpr int64_t maxGraphicsMemorySize = 80 * 1024 * 1024;

  auto graphicsMemorySize = CalculateGraphicsMemory(pagFile);
  if (session->configParam.scenes == ExportScenes::UI &&
      graphicsMemorySize > maxGraphicsMemorySizeForUI) {
    session->pushWarning(AlertInfoType::GraphicsMemoryUI,
                         std::to_string(graphicsMemorySize / 1024 / 1024) + "MB");
  } else if (graphicsMemorySize > maxGraphicsMemorySize) {
    session->pushWarning(AlertInfoType::GraphicsMemory,
                         std::to_string(graphicsMemorySize / 1024 / 1024) + "MB");
  }
}

}  // namespace exporter
