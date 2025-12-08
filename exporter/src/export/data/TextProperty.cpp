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

#include "TextProperty.h"
#include "base/keyframes/SpatialPointKeyframe.h"
#include "export/stream/StreamProperty.h"
#include "rendering/graphics/Text.h"
#include "rendering/renderers/TextAnimatorRenderer.h"
#include "rendering/renderers/TextRenderer.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

enum class TextPropertyType { Unknown, Document, PathOptions, MoreOptions, Animators };

enum class TextAnimatorsType { Unknown, Animator };

enum class TextAnimatorType { Unknown, Selectors, AnimatorProperties };

enum class TextSelectorType { Unknown, RangeSelector, WigglySelector, ExpressibleSelector };

enum class TextAnimatorPropertiesType {
  Unknown,
  TrackingType,
  TrackingAmount,
  FillColor,
  StrokeColor,
  Position,
  Scale,
  Rotation,
  Opacity
};

static const std::unordered_map<std::string, TextPropertyType> TextPropertyTypeMap = {
    {"ADBE Text Document", TextPropertyType::Document},
    {"ADBE Text Path Options", TextPropertyType::PathOptions},
    {"ADBE Text More Options", TextPropertyType::MoreOptions},
    {"ADBE Text Animators", TextPropertyType::Animators},
};

static const std::unordered_map<std::string, TextAnimatorsType> TextAnimatorsTypeMap = {
    {"ADBE Text Animator", TextAnimatorsType::Animator},
};

static const std::unordered_map<std::string, TextAnimatorType> TextAnimatorTypeMap = {
    {"ADBE Text Selectors", TextAnimatorType::Selectors},
    {"ADBE Text Animator Properties", TextAnimatorType::AnimatorProperties},
};

static const std::unordered_map<std::string, TextSelectorType> TextSelectorTypeMap = {
    {"ADBE Text Selector", TextSelectorType::RangeSelector},
    {"ADBE Text Wiggly Selector", TextSelectorType::WigglySelector},
    {"ADBE Text Expressible Selector", TextSelectorType::ExpressibleSelector}};

static const std::unordered_map<std::string, TextAnimatorPropertiesType>
    TextAnimatorPropertiesTypeMap = {
        {"ADBE Text Track Type", TextAnimatorPropertiesType::TrackingType},
        {"ADBE Text Tracking Amount", TextAnimatorPropertiesType::TrackingAmount},
        {"ADBE Text Position 3D", TextAnimatorPropertiesType::Position},
        {"ADBE Text Scale 3D", TextAnimatorPropertiesType::Scale},
        {"ADBE Text Rotation", TextAnimatorPropertiesType::Rotation},
        {"ADBE Text Opacity", TextAnimatorPropertiesType::Opacity},
        {"ADBE Text Fill Color", TextAnimatorPropertiesType::FillColor},
        {"ADBE Text Stroke Color", TextAnimatorPropertiesType::StrokeColor}};

static TextPropertyType GetTextPropertyType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = TextPropertyTypeMap.find(matchName);
  if (result == TextPropertyTypeMap.end()) {
    return TextPropertyType::Unknown;
  }
  return result->second;
}

static TextAnimatorsType GetTextAnimatorsType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = TextAnimatorsTypeMap.find(matchName);
  if (result == TextAnimatorsTypeMap.end()) {
    return TextAnimatorsType::Unknown;
  }
  return result->second;
}

static TextAnimatorType GetTextAnimatorType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = TextAnimatorTypeMap.find(matchName);
  if (result == TextAnimatorTypeMap.end()) {
    return TextAnimatorType::Unknown;
  }
  return result->second;
}

static TextSelectorType GetTextSelectorType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = TextSelectorTypeMap.find(matchName);
  if (result == TextSelectorTypeMap.end()) {
    return TextSelectorType::Unknown;
  }
  return result->second;
}

static TextAnimatorPropertiesType GetTextAnimatorPropertiesType(
    const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = TextAnimatorPropertiesTypeMap.find(matchName);
  if (result == TextAnimatorPropertiesTypeMap.end()) {
    return TextAnimatorPropertiesType::Unknown;
  }
  return result->second;
}

static pag::TextRangeSelector* GetTextRangeSelector(const AEGP_StreamRefH& streamHandle) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  auto selector = new pag::TextRangeSelector();

  selector->start =
      GetProperty(streamHandle, "ADBE Text Percent Start", AEStreamParser::PercentParser);
  selector->end = GetProperty(streamHandle, "ADBE Text Percent End", AEStreamParser::PercentParser);
  selector->offset =
      GetProperty(streamHandle, "ADBE Text Percent Offset", AEStreamParser::PercentParser);

  AEGP_StreamRefH advancedStream = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, streamHandle, "ADBE Text Range Advanced", &advancedStream);
  selector->units = GetValue(advancedStream, "ADBE Text Range Units",
                             AEStreamParser::TextRangeSelectorUnitsParser);
  selector->basedOn =
      GetValue(advancedStream, "ADBE Text Range Type2", AEStreamParser::TextSelectorBasedOnParser);
  selector->mode = GetProperty(advancedStream, "ADBE Text Selector Mode",
                               AEStreamParser::TextSelectorModeParser);
  selector->amount =
      GetProperty(advancedStream, "ADBE Text Selector Max Amount", AEStreamParser::PercentParser);
  selector->shape = GetValue(advancedStream, "ADBE Text Range Shape",
                             AEStreamParser::TextRangeSelectorShapeParser);
  selector->smoothness =
      GetProperty(advancedStream, "ADBE Text Selector Smoothness", AEStreamParser::PercentParser);
  selector->easeHigh =
      GetProperty(advancedStream, "ADBE Text Levels Max Ease", AEStreamParser::PercentParser);
  selector->easeLow =
      GetProperty(advancedStream, "ADBE Text Levels Min Ease", AEStreamParser::PercentParser);
  selector->randomizeOrder =
      GetValue(advancedStream, "ADBE Text Randomize Order", AEStreamParser::BooleanParser);
  selector->randomSeed =
      GetProperty(advancedStream, "ADBE Text Random Seed", AEStreamParser::Uint16Parser);

  return selector;
}

static pag::TextWigglySelector* GetTextWigglySelector(const AEGP_StreamRefH& streamHandle) {
  auto selector = new pag::TextWigglySelector();

  selector->mode =
      GetProperty(streamHandle, "ADBE Text Selector Mode", AEStreamParser::TextSelectorModeParser);
  selector->maxAmount =
      GetProperty(streamHandle, "ADBE Text Wiggly Max Amount", AEStreamParser::PercentParser);
  selector->minAmount =
      GetProperty(streamHandle, "ADBE Text Wiggly Min Amount", AEStreamParser::PercentParser);
  selector->basedOn =
      GetValue(streamHandle, "ADBE Text Range Type2", AEStreamParser::TextSelectorBasedOnParser);
  selector->wigglesPerSecond =
      GetProperty(streamHandle, "ADBE Text Temporal Freq", AEStreamParser::FloatParser);
  selector->correlation =
      GetProperty(streamHandle, "ADBE Text Character Correlation", AEStreamParser::PercentParser);
  selector->temporalPhase =
      GetProperty(streamHandle, "ADBE Text Temporal Phase", AEStreamParser::FloatParser);
  selector->spatialPhase =
      GetProperty(streamHandle, "ADBE Text Spatial Phase", AEStreamParser::FloatParser);
  selector->lockDimensions =
      GetProperty(streamHandle, "ADBE Text Wiggly Lock Dim", AEStreamParser::BooleanParser);
  selector->randomSeed =
      GetProperty(streamHandle, "ADBE Text Wiggly Random Seed", AEStreamParser::Uint16Parser);

  return selector;
}

static void CheckTextDirection(pag::Property<pag::TextDocumentHandle>* textDocument,
                               uint16_t tagLevel) {
  if (textDocument == nullptr) {
    return;
  }
  bool hasVerticalText = false;
  if (textDocument->animatable()) {
    for (const auto& keyFrame :
         reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(textDocument)
             ->keyframes) {
      if (keyFrame->startValue->direction == pag::TextDirection::Vertical ||
          keyFrame->endValue->direction == pag::TextDirection::Vertical) {
        hasVerticalText = true;
        break;
      }
    }
    if (!hasVerticalText || tagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
      for (auto& keyframe :
           reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(textDocument)
               ->keyframes) {
        keyframe->startValue->direction = pag::TextDirection::Default;
        keyframe->endValue->direction = pag::TextDirection::Default;
      }
    }
  } else {
    auto document = textDocument->getValueAt(0);
    if (document->direction == pag::TextDirection::Vertical) {
      hasVerticalText = true;
    }
    if (!hasVerticalText || tagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
      document->direction = pag::TextDirection::Default;
    }
  }

  if (hasVerticalText && tagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
    PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::TagLevelVerticalText);
  }
}

static std::vector<pag::TextSelector*> GetTextSelectors(const AEGP_StreamRefH& streamHandle) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();
  std::vector<pag::TextSelector*> selectors = {};

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, streamHandle, index,
                                                               &childStreamHandle);
    if (childStreamHandle == nullptr) {
      continue;
    }
    if (!IsStreamHidden(childStreamHandle) && IsStreamActive(childStreamHandle)) {
      auto type = GetTextSelectorType(childStreamHandle);
      pag::TextSelector* selector = nullptr;
      switch (type) {
        case TextSelectorType::RangeSelector:
          selector = GetTextRangeSelector(childStreamHandle);
          break;
        case TextSelectorType::WigglySelector:
          selector = GetTextWigglySelector(childStreamHandle);
          break;
        default:
          break;
      }
      if (selector != nullptr) {
        selectors.push_back(selector);
      }
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }

  return selectors;
}

static pag::TextAnimatorColorProperties* GetTextAnimatorColorProperties(
    const AEGP_StreamRefH& streamHandle) {
  auto properties = new pag::TextAnimatorColorProperties();
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, streamHandle, index,
                                                               &childStreamHandle);
    if (childStreamHandle == nullptr) {
      continue;
    }
    if (!IsStreamHidden(childStreamHandle) || IsStreamActive(childStreamHandle)) {
      auto type = GetTextAnimatorPropertiesType(childStreamHandle);
      switch (type) {
        case TextAnimatorPropertiesType::FillColor:
          properties->fillColor = GetProperty(childStreamHandle, AEStreamParser::ColorParser);
          break;
        case TextAnimatorPropertiesType::StrokeColor:
          properties->strokeColor = GetProperty(childStreamHandle, AEStreamParser::ColorParser);
          break;
        default:
          break;
      }
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }
  if (!properties->verify()) {
    delete properties;
    return nullptr;
  }
  return properties;
}

static pag::Property<float>* GetTextAnimatorTrackingAmount(const AEGP_StreamRefH& streamHandle) {
  auto trackingAmount = GetProperty(streamHandle, AEStreamParser::FloatParser);
  if (!trackingAmount->animatable() && trackingAmount->value == 0.0f) {
    delete trackingAmount;
    return nullptr;
  }
  return trackingAmount;
}

static pag::Property<pag::Point>* GetTextAnimatorPosition(const AEGP_StreamRefH& streamHandle) {
  auto position = GetProperty(streamHandle, AEStreamParser::PointParser);
  if (!position->animatable() && position->value.x == 0.0f && position->value.y == 0.0f) {
    delete position;
    return nullptr;
  }

  if (position->animatable()) {
    auto& keyframes = static_cast<pag::AnimatableProperty<pag::Point>*>(position)->keyframes;
    for (auto& keyframe : keyframes) {
      auto newKeyframe = new pag::SpatialPointKeyframe();
      *static_cast<pag::Keyframe<pag::Point>*>(newKeyframe) = *keyframe;
      newKeyframe->initialize();
      delete keyframe;
      keyframe = newKeyframe;
    }
  }

  return position;
}

static pag::Property<pag::Point>* GetTextAnimatorScale(const AEGP_StreamRefH& streamHandle) {
  auto scale = GetProperty(streamHandle, AEStreamParser::ScaleParser);
  if (!scale->animatable() && scale->value.x == 1.0f && scale->value.x == 1.0f) {
    delete scale;
    return nullptr;
  }
  return scale;
}

static pag::Property<float>* GetTextAnimatorRotation(const AEGP_StreamRefH& streamHandle) {
  auto rotation = GetProperty(streamHandle, AEStreamParser::FloatParser);
  if (!rotation->animatable() && rotation->value == 0.0f) {
    delete rotation;
    return nullptr;
  }
  return rotation;
}

static pag::Property<pag::Opacity>* GetTextAnimatorOpacity(const AEGP_StreamRefH& streamHandle) {
  auto opacity = GetProperty(streamHandle, AEStreamParser::Opacity0_100Parser);
  if (!opacity->animatable() && opacity->value == pag::Opaque) {
    delete opacity;
    return nullptr;
  }
  return opacity;
}

static pag::TextAnimatorTypographyProperties* GetTextAnimatorTypographyProperties(
    const AEGP_StreamRefH& streamHandle) {
  auto properties = new pag::TextAnimatorTypographyProperties();
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, streamHandle, index,
                                                               &childStreamHandle);
    if (childStreamHandle == nullptr) {
      continue;
    }
    auto type = GetTextAnimatorPropertiesType(childStreamHandle);
    switch (type) {
      case TextAnimatorPropertiesType::TrackingType:
        properties->trackingType =
            GetProperty(childStreamHandle, AEStreamParser::TextAnimatorTrackingTypeParser);
        break;
      case TextAnimatorPropertiesType::TrackingAmount:
        properties->trackingAmount = GetTextAnimatorTrackingAmount(childStreamHandle);
        break;
      case TextAnimatorPropertiesType::Position:
        properties->position = GetTextAnimatorPosition(childStreamHandle);
        break;
      case TextAnimatorPropertiesType::Scale:
        properties->scale = GetTextAnimatorScale(childStreamHandle);
        break;
      case TextAnimatorPropertiesType::Rotation:
        properties->rotation = GetTextAnimatorRotation(childStreamHandle);
        break;
      case TextAnimatorPropertiesType::Opacity:
        properties->opacity = GetTextAnimatorOpacity(childStreamHandle);
        break;
      default:
        break;
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }

  if (properties->trackingAmount == nullptr && properties->trackingType != nullptr) {
    delete properties->trackingType;
    properties->trackingType = nullptr;
  }

  if (!properties->verify()) {
    delete properties;
    return nullptr;
  }
  return properties;
}

static pag::TextAnimator* GetTextAnimator(const AEGP_StreamRefH& streamHandle) {
  auto animator = new pag::TextAnimator();
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numStreams);
  for (long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, streamHandle, index,
                                                               &childStreamHandle);
    if (childStreamHandle == nullptr) {
      continue;
    }
    if (!IsStreamHidden(childStreamHandle) && IsStreamActive(childStreamHandle)) {
      auto type = GetTextAnimatorType(childStreamHandle);
      if (type == TextAnimatorType::Selectors) {
        auto selectors = GetTextSelectors(childStreamHandle);
        animator->selectors.insert(animator->selectors.end(), selectors.begin(), selectors.end());
      } else if (type == TextAnimatorType::AnimatorProperties) {
        animator->colorProperties = GetTextAnimatorColorProperties(childStreamHandle);
        animator->typographyProperties = GetTextAnimatorTypographyProperties(childStreamHandle);
      }
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }
  if (!animator->verify()) {
    delete animator;
    return nullptr;
  }
  return animator;
}

static std::vector<pag::TextAnimator*> GetTextAnimators(const AEGP_StreamRefH& streamHandle) {
  const auto& Suites = GetSuites();
  std::vector<pag::TextAnimator*> animators = {};

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH childStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(GetPluginID(), streamHandle, index,
                                                               &childStreamHandle);
    if (childStreamHandle == nullptr) {
      continue;
    }
    if (!IsStreamHidden(childStreamHandle) && IsStreamActive(childStreamHandle)) {
      auto type = GetTextAnimatorsType(childStreamHandle);
      if (type == TextAnimatorsType::Animator) {
        auto animator = GetTextAnimator(childStreamHandle);
        if (animator != nullptr) {
          animators.push_back(animator);
        }
      }
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(childStreamHandle);
  }

  return animators;
}

static pag::TextPathOptions* GetTextPathOptions(const AEGP_StreamRefH& streamHandle) {
  auto pathOptions = new pag::TextPathOptions();
  pag::ID maskID = GetValue(streamHandle, "ADBE Text Path", AEStreamParser::MaskIDParser);

  pathOptions->path = new pag::MaskData();
  pathOptions->path->id = maskID;
  if (pathOptions->path->id <= 0) {
    delete pathOptions;
    return nullptr;
  }
  pathOptions->reversedPath =
      GetProperty(streamHandle, "ADBE Text Reverse Path", AEStreamParser::BooleanParser);
  pathOptions->perpendicularToPath =
      GetProperty(streamHandle, "ADBE Text Perpendicular To Path", AEStreamParser::BooleanParser);
  pathOptions->forceAlignment =
      GetProperty(streamHandle, "ADBE Text Force Align Path", AEStreamParser::BooleanParser);
  pathOptions->firstMargin =
      GetProperty(streamHandle, "ADBE Text First Margin", AEStreamParser::FloatParser);
  pathOptions->lastMargin =
      GetProperty(streamHandle, "ADBE Text Last Margin", AEStreamParser::FloatParser);
  return pathOptions;
}

template <typename T>
static void ModifyPropertyKeyframe(pag::Property<T>* property) {
  if (property == nullptr || !property->animatable()) {
    return;
  }

  auto animatableProperty = static_cast<pag::AnimatableProperty<T>*>(property);
  auto& keyFrames = animatableProperty->keyframes;

  for (auto& keyFrame : keyFrames) {
    auto newKeyFrame = new pag::SingleEaseKeyframe<T>();
    *static_cast<pag::Keyframe<T>*>(newKeyFrame) = *keyFrame;
    newKeyFrame->initialize();
    keyFrame = newKeyFrame;
  }
}

static void ModififyAnimatorKeyFrames(std::vector<pag::TextAnimator*>* animators) {
  if (animators == nullptr) {
    return;
  }

  for (auto& animator : *animators) {
    if (animator == nullptr) {
      return;
    }

    for (auto& selector : animator->selectors) {
      if (selector == nullptr || selector->type() != pag::TextSelectorType::Range) {
        continue;
      }

      auto rangeSelector = static_cast<pag::TextRangeSelector*>(selector);
      ModifyPropertyKeyframe(rangeSelector->start);
      ModifyPropertyKeyframe(rangeSelector->end);
      ModifyPropertyKeyframe(rangeSelector->offset);
      ModifyPropertyKeyframe(rangeSelector->amount);
    }
  }
}

static void AdjustFirstBaseLine(pag::TextDocumentHandle textDocument, bool hasBias) {
  if (textDocument->boxTextSize.x <= 0.001f || textDocument->boxTextSize.y <= 0.001f) {
    textDocument->firstBaseLine = 0.0f;
    return;
  }

  if (textDocument->direction == pag::TextDirection::Vertical) {
    auto rightLine = textDocument->boxTextPos.x + textDocument->boxTextSize.x;
    float fontHeight = rightLine + textDocument->firstBaseLine;

    bool needToAdjust = hasBias || fontHeight < textDocument->fontSize / 3.0f ||
                        fontHeight > textDocument->fontSize;
    if (needToAdjust) {
      float newHeight = textDocument->fontSize * 0.4f;
      textDocument->fontSize = rightLine - newHeight;
    }
  } else {
    float fontHeight = textDocument->firstBaseLine - textDocument->boxTextPos.y;
    bool needToAdjust = hasBias || fontHeight < textDocument->fontSize * 0.2f ||
                        fontHeight > textDocument->fontSize;
    if (needToAdjust) {
      float ascend = 0.0f;
      float descent = 0.0f;
      pag::CalculateTextAscentAndDescent(textDocument.get(), &ascend, &descent);

      auto factor = -ascend / (descent - ascend);
      float newHeight = textDocument->fontSize * factor * 0.9f;
      textDocument->firstBaseLine = textDocument->boxTextPos.y + newHeight;
    }
  }
}

static void GetFirstBaseLineByPos(pag::TextDocumentHandle textDocument,
                                  std::vector<pag::TextAnimator*>* animators, pag::Frame frame) {
  bool hasBias = false;
  auto position = pag::TextAnimatorRenderer::GetPositionFromAnimators(animators, textDocument.get(),
                                                                      frame, 0, &hasBias);
  bool isVertical = (textDocument->direction == pag::TextDirection::Vertical);
  textDocument->firstBaseLine -= isVertical ? position.x : position.y;
  AdjustFirstBaseLine(textDocument, hasBias);
}

static void GetFirstBaseLinesByPos(pag::Property<pag::TextDocumentHandle>* sourceText,
                                   std::vector<pag::TextAnimator*>* animators) {
  if (animators == nullptr || animators->empty()) {
    return;
  }

  ModififyAnimatorKeyFrames(animators);
  if (!sourceText->animatable()) {
    GetFirstBaseLineByPos(sourceText->value, animators, 0);
    return;
  }

  auto animatableText =
      reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText);
  for (auto& keyFrame : animatableText->keyframes) {
    GetFirstBaseLineByPos(keyFrame->startValue, animators, keyFrame->startTime);
    GetFirstBaseLineByPos(keyFrame->endValue, animators, keyFrame->endTime);
  }
}

void GetTextProperties(std::shared_ptr<PAGExportSession> session, const AEGP_LayerH& layerHandle,
                       pag::TextLayer* layer) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH layerStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(PluginID, layerHandle,
                                                              &layerStreamHandle);
  if (layerStreamHandle == nullptr) {
    return;
  }
  AEGP_StreamRefH rootStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      PluginID, layerStreamHandle, "ADBE Text Properties", &rootStreamHandle);
  if (rootStreamHandle == nullptr) {
    Suites->StreamSuite4()->AEGP_DisposeStream(layerStreamHandle);
    return;
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(layerStreamHandle);
  if (IsStreamHidden(rootStreamHandle) || !IsStreamActive(rootStreamHandle)) {
    Suites->StreamSuite4()->AEGP_DisposeStream(rootStreamHandle);
    return;
  }

  A_long numStreams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStreamHandle, &numStreams);
  for (A_long index = 0; index < numStreams; index++) {
    AEGP_StreamRefH streamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, rootStreamHandle, index,
                                                               &streamHandle);
    if (streamHandle == nullptr) {
      continue;
    }
    if (!IsStreamHidden(rootStreamHandle) || IsStreamActive(rootStreamHandle)) {
      auto type = GetTextPropertyType(streamHandle);
      switch (type) {
        case TextPropertyType::Document: {
          QVariantMap map = {};
          map["runJavaScript"] = session->enableRunScript;
          map["compID"] = session->compID;
          map["layerIndex"] = session->layerIndex;
          map["keyFrame"] = 0;
          if (session->configParam.exportFontFile) {
            map["outPath"] = QString(session->outputPath.data());
          }
          layer->sourceText = GetProperty(streamHandle, AEStreamParser::TextDocumentParser, map);
          CheckTextDirection(layer->sourceText, session->configParam.exportTagLevel);
          break;
        }
        case TextPropertyType::PathOptions: {
          layer->pathOption = GetTextPathOptions(streamHandle);
          break;
        }
        case TextPropertyType::Animators: {
          auto animators = GetTextAnimators(streamHandle);
          layer->animators.insert(layer->animators.end(), animators.begin(), animators.end());
          break;
        }
        default: {
          break;
        }
      }
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  }
  GetFirstBaseLinesByPos(layer->sourceText, &layer->animators);

  Suites->StreamSuite4()->AEGP_DisposeStream(rootStreamHandle);
}

}  // namespace exporter
