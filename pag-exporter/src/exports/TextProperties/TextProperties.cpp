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
#include "TextProperties.h"
#include <unordered_map>
#include "src/exports/Stream/StreamProperty.h"
#include "pag/file.h"
#include "base/keyframes/SpatialPointKeyframe.h"
#include "rendering/renderers/TextAnimatorRenderer.h"
#include "rendering/renderers/TextRenderer.h"

template<typename T>
static T GetTypeByName(pagexporter::Context* context, AEGP_StreamRefH stream, const std::unordered_map<std::string, T>& map,
                       T defaultType) {
  auto& suites = context->suites;
  static char matchName[200];
  suites.DynamicStreamSuite4()->AEGP_GetMatchName(stream, matchName);
  auto result = map.find(matchName);
  if (result == map.end()) {
    printf("Type::Unknown: matchName=%s\n", matchName);
    return defaultType;
  }
  return result->second;
}

enum class TextPropertyType {
  Unknown,
  Document,
  PathOptions,
  MoreOptions,
  Animators
};
static const std::unordered_map<std::string, TextPropertyType> TextPropertyTypeMap = {
    {"ADBE Text Document",     TextPropertyType::Document},
    {"ADBE Text Path Options", TextPropertyType::PathOptions},
    {"ADBE Text More Options", TextPropertyType::MoreOptions},
    {"ADBE Text Animators",    TextPropertyType::Animators},
};

enum class TextAnimatorsType {
  Unknown,
  Animator
};
static const std::unordered_map<std::string, TextAnimatorsType> TextAnimatorsTypeMap = {
    {"ADBE Text Animator", TextAnimatorsType::Animator},
};

enum class TextAnimatorType {
  Unknown,
  Selectors,
  AnimatorProperties
};
static const std::unordered_map<std::string, TextAnimatorType> TextAnimatorTypeMap = {
    {"ADBE Text Selectors",           TextAnimatorType::Selectors},
    {"ADBE Text Animator Properties", TextAnimatorType::AnimatorProperties},
};


enum class TextSelectorType {
  Unknown,
  RangeSelector,
  WigglySelector,
  ExpressibleSelector
};
static const std::unordered_map<std::string, TextSelectorType> TextSelectorTypeMap = {
    {"ADBE Text Selector",             TextSelectorType::RangeSelector},
    {"ADBE Text Wiggly Selector",      TextSelectorType::WigglySelector},
    {"ADBE Text Expressible Selector", TextSelectorType::ExpressibleSelector}
};

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
static const std::unordered_map<std::string, TextAnimatorPropertiesType> TextAnimatorPropertiesTypeMap = {
    {"ADBE Text Track Type",      TextAnimatorPropertiesType::TrackingType},
    {"ADBE Text Tracking Amount", TextAnimatorPropertiesType::TrackingAmount},
    {"ADBE Text Position 3D",     TextAnimatorPropertiesType::Position},
    {"ADBE Text Scale 3D",        TextAnimatorPropertiesType::Scale},
    {"ADBE Text Rotation",        TextAnimatorPropertiesType::Rotation},
    {"ADBE Text Opacity",         TextAnimatorPropertiesType::Opacity},
    {"ADBE Text Fill Color",      TextAnimatorPropertiesType::FillColor},
    {"ADBE Text Stroke Color",    TextAnimatorPropertiesType::StrokeColor}
};

static TextPropertyType GetTextPropertyType(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto& suites = context->suites;
  static char matchName[200];
  suites.DynamicStreamSuite4()->AEGP_GetMatchName(stream, matchName);
  auto result = TextPropertyTypeMap.find(matchName);
  if (result == TextPropertyTypeMap.end()) {
    printf("TextPropertyType::Unknown: matchName=%s", matchName);
    return TextPropertyType::Unknown;
  }
  return result->second;
}

static pag::TextPathOptions* ExportTextPathOptions(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto pathOptions = new pag::TextPathOptions();
  pathOptions->path = ExportValue(context, stream, "ADBE Text Path", Parsers::MaskID);
  if (pathOptions->path == nullptr) {
    delete pathOptions;
    return nullptr;
  }
  pathOptions->reversedPath = ExportProperty(context, stream, "ADBE Text Reverse Path", Parsers::Boolean);
  pathOptions->perpendicularToPath = ExportProperty(context, stream, "ADBE Text Perpendicular To Path",
                                                    Parsers::Boolean);
  pathOptions->forceAlignment = ExportProperty(context, stream, "ADBE Text Force Align Path", Parsers::Boolean);
  pathOptions->firstMargin = ExportProperty(context, stream, "ADBE Text First Margin", Parsers::Float);
  pathOptions->lastMargin = ExportProperty(context, stream, "ADBE Text Last Margin", Parsers::Float);
  return pathOptions;
}

static pag::TextRangeSelector* ExportTextRangeSelector(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto selector = new pag::TextRangeSelector();
  selector->start = ExportProperty(context, stream, "ADBE Text Percent Start", Parsers::Percent);
  selector->end = ExportProperty(context, stream, "ADBE Text Percent End", Parsers::Percent);
  selector->offset = ExportProperty(context, stream, "ADBE Text Percent Offset", Parsers::Percent);

  auto& suites = context->suites;
  AEGP_StreamRefH advancedStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, stream,
                                                                             "ADBE Text Range Advanced",
                                                                             &advancedStream));

  selector->units = ExportValue(context, advancedStream, "ADBE Text Range Units", Parsers::TextRangeSelectorUnits);
  selector->basedOn = ExportValue(context, advancedStream, "ADBE Text Range Type2", Parsers::TextRangeSelectorBasedOn);
  selector->mode = ExportProperty(context, advancedStream, "ADBE Text Selector Mode", Parsers::TextRangeSelectorMode);
  selector->amount = ExportProperty(context, advancedStream, "ADBE Text Selector Max Amount", Parsers::Percent);
  selector->shape = ExportValue(context, advancedStream, "ADBE Text Range Shape", Parsers::TextRangeSelectorShape);
  selector->smoothness = ExportProperty(context, advancedStream, "ADBE Text Selector Smoothness", Parsers::Percent);
  selector->easeHigh = ExportProperty(context, advancedStream, "ADBE Text Levels Max Ease", Parsers::Percent);
  selector->easeLow = ExportProperty(context, advancedStream, "ADBE Text Levels Min Ease", Parsers::Percent);
  selector->randomizeOrder = ExportValue(context, advancedStream, "ADBE Text Randomize Order", Parsers::Boolean);
  selector->randomSeed = ExportProperty(context, advancedStream, "ADBE Text Random Seed", Parsers::Uint16);

  return selector;
}

static pag::TextWigglySelector* ExportTextWigglySelector(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto selector = new pag::TextWigglySelector();

  selector->mode = ExportProperty(context, stream, "ADBE Text Selector Mode", Parsers::TextRangeSelectorMode);
  selector->maxAmount = ExportProperty(context, stream, "ADBE Text Wiggly Max Amount", Parsers::Percent);
  selector->minAmount = ExportProperty(context, stream, "ADBE Text Wiggly Min Amount", Parsers::Percent);
  selector->basedOn = ExportValue(context, stream, "ADBE Text Range Type2", Parsers::TextRangeSelectorBasedOn);
  selector->wigglesPerSecond = ExportProperty(context, stream, "ADBE Text Temporal Freq", Parsers::Float);
  selector->correlation = ExportProperty(context, stream, "ADBE Text Character Correlation", Parsers::Percent);
  selector->temporalPhase = ExportProperty(context, stream, "ADBE Text Temporal Phase", Parsers::Float);
  selector->spatialPhase = ExportProperty(context, stream, "ADBE Text Spatial Phase", Parsers::Float);
  selector->lockDimensions = ExportProperty(context, stream, "ADBE Text Wiggly Lock Dim", Parsers::Boolean);
  selector->randomSeed = ExportProperty(context, stream, "ADBE Text Wiggly Random Seed", Parsers::Uint16);

  return selector;
}

static pag::TextAnimatorColorProperties* ExportTextAnimatorColorProperties(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto properties = new pag::TextAnimatorColorProperties();

  auto& suites = context->suites;
  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(stream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, stream, i, &childStreamH));
    auto type = GetTypeByName(context, childStreamH, TextAnimatorPropertiesTypeMap,
                              TextAnimatorPropertiesType::Unknown);
    switch (type) {
      case TextAnimatorPropertiesType::FillColor:
        properties->fillColor = ExportProperty(context, childStreamH, Parsers::Color);
        break;
      case TextAnimatorPropertiesType::StrokeColor:
        properties->strokeColor = ExportProperty(context, childStreamH, Parsers::Color);
        break;
      default:
        break;
    }
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
  }

  if (!properties->verify()) {
    delete properties;
    return nullptr;
  }

  return properties;
}

static pag::Property<float>* ExportTextAnimatorTrackingAmount(pagexporter::Context* context,
                                                              AEGP_StreamRefH childStreamH) {
  auto trackingAmount = ExportProperty(context, childStreamH, Parsers::Float);
  if (!trackingAmount->animatable() && trackingAmount->value == 0.0f) {
    delete trackingAmount; // 如果没有有效值，则释放
    return nullptr;
  }
  return trackingAmount;
}

static pag::Property<pag::Point>* ExportTextAnimatorPosition(pagexporter::Context* context,
                                                             AEGP_StreamRefH childStreamH) {
  auto position = ExportProperty(context, childStreamH, Parsers::Point);

  if (!position->animatable() && position->value.x == 0.0f && position->value.y == 0.0f) {
    delete position; // 如果没有有效值，则释放
    return nullptr;
  }

  if (position->animatable()) {
    auto& keyframes = static_cast<pag::AnimatableProperty<pag::Point>*>(position)->keyframes;
    for (int i = 0; i < keyframes.size(); i++) {
      auto newKeyframe = new pag::SpatialPointKeyframe();
      *static_cast<pag::Keyframe<pag::Point>*>(newKeyframe) = *keyframes[i];
      newKeyframe->initialize();
      delete keyframes[i];
      keyframes[i] = newKeyframe;
    }
  }

  return position;
}

static pag::Property<pag::Point>* ExportTextAnimatorScale(pagexporter::Context* context,
                                                          AEGP_StreamRefH childStreamH) {
  auto scale = ExportProperty(context, childStreamH, Parsers::Scale);
  if (!scale->animatable() && scale->value.x == 1.0f && scale->value.x == 1.0f) {
    delete scale; // 如果没有有效值，则释放
    return nullptr;
  }
  return scale;
}

static pag::Property<float>* ExportTextAnimatorRotation(pagexporter::Context* context,
                                                        AEGP_StreamRefH childStreamH) {
  auto rotation = ExportProperty(context, childStreamH, Parsers::Float);
  if (!rotation->animatable() && rotation->value == 0.0f) {
    delete rotation; // 如果没有有效值，则释放
    return nullptr;
  }
  return rotation;
}

static pag::Property<pag::Opacity>* ExportTextAnimatorOpacity(pagexporter::Context* context,
                                                              AEGP_StreamRefH childStreamH) {
  auto opacity = ExportProperty(context, childStreamH, Parsers::Opacity0_100);
  if (!opacity->animatable() && opacity->value == pag::Opaque) {
    delete opacity; // 如果没有有效值，则释放
    return nullptr;
  }
  return opacity;
}

static pag::TextAnimatorTypographyProperties*
ExportTextAnimatorTypographyProperties(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto properties = new pag::TextAnimatorTypographyProperties();

  auto& suites = context->suites;
  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(stream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, stream, i, &childStreamH));
    auto type = GetTypeByName(context, childStreamH, TextAnimatorPropertiesTypeMap,
                              TextAnimatorPropertiesType::Unknown);
    switch (type) {
      case TextAnimatorPropertiesType::TrackingType:
        properties->trackingType = ExportProperty(context, childStreamH, Parsers::TextAnimatorTrackingType);
        break;
      case TextAnimatorPropertiesType::TrackingAmount:
        properties->trackingAmount = ExportTextAnimatorTrackingAmount(context, childStreamH);
        break;
      case TextAnimatorPropertiesType::Position:
        properties->position = ExportTextAnimatorPosition(context, childStreamH);
        break;
      case TextAnimatorPropertiesType::Scale:
        properties->scale = ExportTextAnimatorScale(context, childStreamH);
        break;
      case TextAnimatorPropertiesType::Rotation:
        properties->rotation = ExportTextAnimatorRotation(context, childStreamH);
        break;
      case TextAnimatorPropertiesType::Opacity:
        properties->opacity = ExportTextAnimatorOpacity(context, childStreamH);
        break;
      default:
        break;
    }
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
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

static void ExportTextSelectors(pagexporter::Context* context, AEGP_StreamRefH stream, pag::TextAnimator* animator) {
  auto& suites = context->suites;

  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(stream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, stream, i, &childStreamH));

    if (!IsStreamHidden(suites, childStreamH) && IsStreamActive(suites, childStreamH)) {
      auto type = GetTypeByName(context, childStreamH, TextSelectorTypeMap, TextSelectorType::Unknown);
      switch (type) {
        case TextSelectorType::RangeSelector: {
          auto selector = ExportTextRangeSelector(context, childStreamH);
          if (selector != nullptr) {
            animator->selectors.push_back(selector);
          }
          break;
        }
        case TextSelectorType::WigglySelector: {
          auto selector = ExportTextWigglySelector(context, childStreamH);
          if (selector != nullptr) {
            animator->selectors.push_back(selector);
          }
          break;
        }
        case TextSelectorType::ExpressibleSelector:
          break;
        default:
          break;
      }
    }
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
  }
}

static pag::TextAnimator* ExportTextAnimator(pagexporter::Context* context, AEGP_StreamRefH stream) {
  auto animator = new pag::TextAnimator();
  auto& suites = context->suites;

  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(stream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, stream, i, &childStreamH));

    if (!IsStreamHidden(suites, childStreamH) && IsStreamActive(suites, childStreamH)) {
      auto type = GetTypeByName(context, childStreamH, TextAnimatorTypeMap, TextAnimatorType::Unknown);
      switch (type) {
        case TextAnimatorType::Selectors:
          ExportTextSelectors(context, childStreamH, animator);
          break;
        case TextAnimatorType::AnimatorProperties:
          animator->colorProperties = ExportTextAnimatorColorProperties(context, childStreamH);
          animator->typographyProperties = ExportTextAnimatorTypographyProperties(context, childStreamH);
          break;
        default:
          break;
      }
    }

    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
  }
  if (!animator->verify()) {
    delete animator;
    return nullptr;
  } else {
    return animator;
  }
}

static void ExportTextAnimators(pagexporter::Context* context, AEGP_StreamRefH stream, pag::TextLayer* layer) {
  auto& suites = context->suites;

  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(stream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, stream, i, &childStreamH));

    if (!IsStreamHidden(suites, childStreamH) && IsStreamActive(suites, childStreamH)) {
      auto type = GetTypeByName(context, childStreamH, TextAnimatorsTypeMap, TextAnimatorsType::Unknown);
      switch (type) {
        case TextAnimatorsType::Animator: {
          auto animator = ExportTextAnimator(context, childStreamH);
          if (animator != nullptr) {
            layer->animators.push_back(animator);
          }
          break;
        }
        default:
          break;
      }
    }

    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
  }
}

// 检查横排/竖排方向，如果没有竖排，则整体设置为Default，以便libpag encode可以按低版本tag来进行编码
static void CheckTextDirection(pagexporter::Context* context, pag::Property<pag::TextDocumentHandle>* sourceText) {
  if (sourceText != nullptr) {
    bool hasVertical = false;
    if (sourceText->animatable()) {
      for (auto keyframe : reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)->keyframes) {
        if (keyframe->startValue->direction == pag::TextDirection::Vertical
            || keyframe->endValue->direction == pag::TextDirection::Vertical) {
          hasVertical = true;
          break;
        }
      }
      if (!hasVertical || context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
        for (auto keyframe : reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)->keyframes) {
          keyframe->startValue->direction = pag::TextDirection::Default;
          keyframe->endValue->direction = pag::TextDirection::Default;
        }
      }
    } else {
      auto textDocument = sourceText->getValueAt(0);
      if (textDocument->direction == pag::TextDirection::Vertical) {
        hasVertical = true;
      }
      if (!hasVertical || context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
        textDocument->direction = pag::TextDirection::Default;
      }
    }

    if (hasVertical && context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::TextSourceV3)) {
      context->pushWarning(pagexporter::AlertInfoType::TagLevelVerticalText);
    }
  }
}

static void AdjustFirstBaseline(pag::TextDocumentHandle textDocument, bool hasBias) {
  if (textDocument->boxTextSize.x <= 0.001f || textDocument->boxTextSize.y <= 0.001f) {
    textDocument->firstBaseLine = 0.0f;
    return; // 点文本退出
  }

  if (textDocument->direction == pag::TextDirection::Vertical) {
    auto rightLine = textDocument->boxTextPos.x + textDocument->boxTextSize.x;
    float fontHeight = rightLine - textDocument->firstBaseLine;
    if (hasBias
        || fontHeight < textDocument->fontSize / 3
        || fontHeight > textDocument->fontSize) {
      float newHeight = textDocument->fontSize * 0.4;
      textDocument->firstBaseLine = rightLine - newHeight;
    }
  } else {
    float fontHeight = textDocument->firstBaseLine - textDocument->boxTextPos.y;
    if (hasBias
        || fontHeight < textDocument->fontSize / 2
        || fontHeight > textDocument->fontSize) {
      float ascent = 0.0f;
      float dscent = 0.0f;
      pag::CalculateTextAscentAndDescent(textDocument.get(), &ascent, &dscent);
      auto factor = -ascent / (dscent - ascent);
      float newHeight = textDocument->fontSize * factor * 0.9;
      textDocument->firstBaseLine = textDocument->boxTextPos.y + newHeight;
    }
  }
}

static void ModifyFirstBaselineByPosition(pag::TextDocumentHandle textDocument,
                                          std::vector<pag::TextAnimator*>* animators,
                                          pag::Frame frame) {
  bool hasBias = false;
  // 取第一个符号的文本动画位置
  auto position = pag::TextAnimatorRenderer::GetPositionFromAnimators(animators,
                                                                      textDocument.get(),
                                                                      frame, 0, &hasBias);
  bool isVertical = (textDocument->direction == pag::TextDirection::Vertical);
  if (isVertical) {
    textDocument->firstBaseLine -= position.x;
  } else {
    textDocument->firstBaseLine -= position.y;
  }

  AdjustFirstBaseline(textDocument, hasBias);
}

template<typename T>
static void ModifyPropertyKeyframe(pag::Property<T>* property) {
  if (property != nullptr && property->animatable()) {
    auto& keyframes = static_cast<pag::AnimatableProperty<T>*>(property)->keyframes;
    for (int i = 0; i < keyframes.size(); i++) {
      auto newKeyframe = new pag::SingleEaseKeyframe<T>();
      *static_cast<pag::Keyframe<T>*>(newKeyframe) = *keyframes[i];
      newKeyframe->initialize();
      delete keyframes[i];
      keyframes[i] = newKeyframe;
    }
  }
}

static void ModifyAnimatorsKeyframes(std::vector<pag::TextAnimator*>* animators) {
  for (auto animator : *animators) {
    for (auto selector : animator->selectors) {
      if (selector->type() == pag::TextSelectorType::Range) {
        auto rangeSelector = static_cast<pag::TextRangeSelector*>(selector);
        ModifyPropertyKeyframe(rangeSelector->start);
        ModifyPropertyKeyframe(rangeSelector->end);
        ModifyPropertyKeyframe(rangeSelector->offset);
        ModifyPropertyKeyframe(rangeSelector->amount);
      }
    }
  }
}

static void ModifyFirstBaselinesByPosition(pag::Property<pag::TextDocumentHandle>* sourceText,
                                           std::vector<pag::TextAnimator*>* animators) {
  if (animators == nullptr || animators->size() == 0) {
    return;
  }

  ModifyAnimatorsKeyframes(animators);

  if (!sourceText->animatable()) {
    ModifyFirstBaselineByPosition(sourceText->value, animators, 0);
  } else {
    for (auto keyframe : reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)->keyframes) {
      ModifyFirstBaselineByPosition(keyframe->startValue, animators, keyframe->startTime);
      ModifyFirstBaselineByPosition(keyframe->endValue, animators, keyframe->endTime);
    }
  }
}

void ExportTextProperties(pagexporter::Context* context, const AEGP_LayerH& layerHandle, pag::TextLayer* layer) {
  auto& suites = context->suites;
  AEGP_StreamRefH layerStream;
  RECORD_ERROR(
      suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(context->pluginID, layerHandle, &layerStream));
  AEGP_StreamRefH rootStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, layerStream,
                                                                             "ADBE Text Properties", &rootStream));
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(layerStream));
  if (IsStreamHidden(suites, rootStream) || !IsStreamActive(suites, rootStream)) {
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(rootStream));
    return;
  }

  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH childStreamH;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, rootStream, i, &childStreamH));

    if (!IsStreamHidden(suites, childStreamH) && IsStreamActive(suites, childStreamH)) {
      auto type = GetTextPropertyType(context, childStreamH);
      switch (type) {
        case TextPropertyType::Document:
          layer->sourceText = ExportProperty(context, childStreamH, Parsers::TextDocument);
          CheckTextDirection(context, layer->sourceText);
          break;
        case TextPropertyType::PathOptions:
          layer->pathOption = ExportTextPathOptions(context, childStreamH);
          break;
        case TextPropertyType::MoreOptions:
          break;
        case TextPropertyType::Animators:
          ExportTextAnimators(context, childStreamH, layer);
          break;
        default:
          break;
      }
    }

    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(childStreamH));
  }

  ModifyFirstBaselinesByPosition(layer->sourceText, &layer->animators);

  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(rootStream));
}

