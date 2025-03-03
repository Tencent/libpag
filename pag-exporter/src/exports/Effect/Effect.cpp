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
#include "Effect.h"
#include <unordered_map>
#include "src/exports/Stream/StreamProperty.h"
#include "src/utils/AEUtils.h"

// 因为AE里的EffectType和PAG的EffectType不能一一对应（GaussBlur），所以这里再临时定义一套
enum class AEEffectType {
  Unknown,
  ImageFillRule,
  ImageFillRuleV2,
  TextBackground,
  MotionTile,
  LevelsIndividual,
  CornerPin,
  Bulge,
  FastBlur,
  GaussBlur,
  Glow,
  DisplacementMap,
  RadialBlur,
  Mosaic,
  BrightnessContrast,
  HueSaturation,
};

static const std::unordered_map<std::string, AEEffectType> effectTypeMap = {
    {"ADBE Image Fill Rule",  AEEffectType::ImageFillRule},
    {"ADBE Image Fill Rule2", AEEffectType::ImageFillRuleV2},
    {"ADBE Text Background",  AEEffectType::TextBackground},
    {"ADBE Tile",             AEEffectType::MotionTile},
    {"ADBE Pro Levels2",      AEEffectType::LevelsIndividual},
    {"ADBE Corner Pin",       AEEffectType::CornerPin},
    {"ADBE Bulge",            AEEffectType::Bulge},
    {"ADBE Fast Blur",        AEEffectType::FastBlur},
    {"ADBE Gaussian Blur 2",  AEEffectType::GaussBlur},
    {"ADBE Glo2",             AEEffectType::Glow},
    {"ADBE Displacement Map", AEEffectType::DisplacementMap},
    {"ADBE Radial Blur",      AEEffectType::RadialBlur},
    {"ADBE Mosaic",           AEEffectType::Mosaic},
    {"ADBE Brightness & Contrast 2", AEEffectType::BrightnessContrast},
    {"ADBE HUE SATURATION",   AEEffectType::HueSaturation},
};

static AEEffectType GetEffectType(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto& suites = context->suites;
  static char matchName[200];
  suites.DynamicStreamSuite4()->AEGP_GetMatchName(effectStream, matchName);
  auto result = effectTypeMap.find(matchName);
  if (result == effectTypeMap.end()) {
    return AEEffectType::Unknown;
  }
  return result->second;
}

static void ExportCompositingOption(pagexporter::Context* context, AEGP_StreamRefH effectStream, pag::Effect* effect) {
  auto& suites = context->suites;
  A_long numParams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(effectStream, &numParams));
  AEGP_StreamRefH builtInParams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, effectStream,
                                                                         numParams - 1, &builtInParams));
  static char matchName[100];
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetMatchName(builtInParams, matchName));
  if (strcmp(matchName, "ADBE Effect Built In Params") != 0) {
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(builtInParams));
    return;
  }
  AEGP_StreamRefH masks;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, builtInParams, 0, &masks));
  A_long numMasks;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(masks, &numMasks));
  for (int i = 0; i < numMasks; i++) {
    AEGP_StreamRefH maskReference;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, masks, i, &maskReference));
    auto mask = ExportValue(context, maskReference, nullptr, Parsers::MaskID);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(maskReference));
    effect->maskReferences.push_back(mask);
  }
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(masks));
  effect->effectOpacity = ExportProperty(context, builtInParams, 1, Parsers::Opacity0_100);
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(builtInParams));
  if (!effect->effectOpacity->animatable() &&
      effect->effectOpacity->value == pag::Opaque) {
    delete effect->effectOpacity;
    effect->effectOpacity = nullptr;
  }
}

static bool CheckImageFillRuleValid(pagexporter::Context* context, pag::Layer* layer) {
  if (layer->type() != pag::LayerType::Image) { // 只有对ImageLayer有效
    context->pushWarning(pagexporter::AlertInfoType::ImageFillRuleOnlyImageLayer);
    return false;
  }
  if (context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::ImageFillRule)) {
    context->pushWarning(pagexporter::AlertInfoType::TagLevelImageFillRule);
    return false;
  }
  if (static_cast<pag::ImageLayer*>(layer)->imageFillRule != nullptr) {
    context->pushWarning(pagexporter::AlertInfoType::ImageFillRuleOnlyOne);
    return false; // 多个效果只用第一个，后面的跳过
  }
  return true;
}

static void ExportImageFillRule(pagexporter::Context* context, AEGP_StreamRefH effectStream, pag::Layer* layer) {
  if (!CheckImageFillRuleValid(context, layer)) {
    return;
  }
  auto imageFillRule = new pag::ImageFillRule();
  auto scaleMode = ExportValue(context, effectStream, "ADBE Image Fill Rule-0001", Parsers::ScaleMode);
  imageFillRule->scaleMode = static_cast<int>(scaleMode);
  imageFillRule->timeRemap = ExportProperty(context, effectStream, "ADBE Image Fill Rule-0002", Parsers::Time);
  if (imageFillRule->timeRemap->animatable()) { // 只有关键帧才需要处理
    auto timeRemap = imageFillRule->timeRemap;
    for (auto& keyFrame: static_cast<pag::AnimatableProperty<pag::Frame>*>(timeRemap)->keyframes) {
      keyFrame->interpolationType = pag::KeyframeInterpolationType::Linear;
    }
  }
  static_cast<pag::ImageLayer*>(layer)->imageFillRule = imageFillRule;
}

static void ExportImageFillRuleV2(pagexporter::Context* context, AEGP_StreamRefH effectStream,
                                  pag::Layer* layer) {
  if (!CheckImageFillRuleValid(context, layer)) {
    return;
  }
  auto imageFillRule = new pag::ImageFillRule();
  auto scaleMode = ExportValue(context, effectStream, "ADBE Image Fill Rule2-0001", Parsers::ScaleMode);
  imageFillRule->scaleMode = static_cast<int>(scaleMode);
  imageFillRule->timeRemap = ExportProperty(context, effectStream, "ADBE Image Fill Rule2-0002", Parsers::Time);

  // 如果TagLevel小于ImageFillRuleV2, 则降级到V1导出
  if (context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::ImageFillRuleV2)) {
    bool isAllLinear = true;
    auto timeRemap = imageFillRule->timeRemap;
    for (auto& keyFrame: static_cast<pag::AnimatableProperty<pag::Frame>*>(timeRemap)->keyframes) {
      if (keyFrame->interpolationType != pag::KeyframeInterpolationType::Linear) {
        isAllLinear = false;
        keyFrame->interpolationType = pag::KeyframeInterpolationType::Linear;
      }
    }
    context->pushWarning(pagexporter::AlertInfoType::TagLevelImageFillRuleV2);
  }
  static_cast<pag::ImageLayer*>(layer)->imageFillRule = imageFillRule;
}

static void ExportTextBackground(pagexporter::Context* context, AEGP_StreamRefH effectStream, pag::Layer* layer) {
  if (layer->type() != pag::LayerType::Text) {
    context->pushWarning(pagexporter::AlertInfoType::TextBackgroundOnlyTextLayer);
    return;
  }
  auto backgroundColor = ExportValue(context, effectStream, "ADBE Text Background-0001", Parsers::Color);
  auto backgroundAlpha = ExportValue(context, effectStream, "ADBE Text Background-0002", Parsers::Opacity0_100);
  auto sourceText = (static_cast<pag::TextLayer*>(layer))->sourceText;
  if (sourceText != nullptr) {
    if (sourceText->animatable()) {
      for (auto keyframe : reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)->keyframes) {
        keyframe->startValue->backgroundColor = backgroundColor;
        keyframe->startValue->backgroundAlpha = backgroundAlpha;
        keyframe->endValue->backgroundColor = backgroundColor;
        keyframe->endValue->backgroundAlpha = backgroundAlpha;
      }
    } else {
      auto textDocument = sourceText->getValueAt(0);
      textDocument->backgroundColor = backgroundColor;
      textDocument->backgroundAlpha = backgroundAlpha;
    }
  }
}

static void ExportAttachment(pagexporter::Context* context, const AEGP_EffectRefH& effectHandle,
                             pag::Layer* layer) {
  auto& suites = context->suites;
  A_long numParams;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetEffectNumParamStreams(effectHandle, &numParams));
  if (numParams < 2) {
    return;
  }
  AEGP_EffectFlags effectFlags;
  RECORD_ERROR(suites.EffectSuite4()->AEGP_GetEffectFlags(effectHandle, &effectFlags));
  if ((effectFlags & AEGP_EffectFlags_ACTIVE) == 0) {
    return;
  }
  AEGP_StreamRefH firstStream;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetNewEffectStreamByIndex(context->pluginID, effectHandle, 1, &firstStream));
  AEGP_StreamRefH effectStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(context->pluginID, firstStream, &effectStream));
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(firstStream));
  auto type = GetEffectType(context, effectStream);
  switch (type) {
    case AEEffectType::ImageFillRule:
      ExportImageFillRule(context, effectStream, layer);
      break;
    case AEEffectType::ImageFillRuleV2:
      ExportImageFillRuleV2(context, effectStream, layer);
      break;
    case AEEffectType::TextBackground:
      ExportTextBackground(context, effectStream, layer);
      break;
    default:
      break;
  }
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(effectStream));
}

void ExportAttachments(pagexporter::Context* context, const AEGP_LayerH& layerHandle, pag::Layer* layer) {
  auto& suites = context->suites;
  int numEffects = AEUtils::GetLayerNumEffects(layerHandle);
  for (int i = 0; i < numEffects; i++) {
    AEGP_EffectRefH effectHandle;
    RECORD_ERROR(suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(context->pluginID, layerHandle, i, &effectHandle));
    ExportAttachment(context, effectHandle, layer);
    RECORD_ERROR(suites.EffectSuite4()->AEGP_DisposeEffect(effectHandle));
  }
}

static pag::DisplacementMapEffect* ExportDisplacementMapEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::DisplacementMapEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->displacementMapLayer = ExportValue(context, effectStream, "ADBE Displacement Map-0001", Parsers::LayerID);
  effect->useForHorizontalDisplacement = ExportProperty(context, effectStream, "ADBE Displacement Map-0002",
                                                        Parsers::DisplacementMapSource);
  effect->maxHorizontalDisplacement = ExportProperty(context, effectStream, "ADBE Displacement Map-0003",
                                                     Parsers::Float);
  effect->useForVerticalDisplacement = ExportProperty(context, effectStream, "ADBE Displacement Map-0004",
                                                      Parsers::DisplacementMapSource);
  effect->maxVerticalDisplacement = ExportProperty(context, effectStream, "ADBE Displacement Map-0005", Parsers::Float);
  effect->displacementMapBehavior = ExportProperty(context, effectStream, "ADBE Displacement Map-0006",
                                                   Parsers::DisplacementBehavior);
  effect->edgeBehavior = ExportProperty(context, effectStream, "ADBE Displacement Map-0007", Parsers::Boolean);
  effect->expandOutput = ExportProperty(context, effectStream, "ADBE Displacement Map-0008", Parsers::Boolean);

  return effect;
}

static pag::MotionTileEffect* ExportMotionTileEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::MotionTileEffect();
  ExportCompositingOption(context, effectStream, effect);
  effect->tileCenter = ExportProperty(context, effectStream, "ADBE Tile-0001", Parsers::Point);
  effect->tileWidth = ExportProperty(context, effectStream, "ADBE Tile-0002", Parsers::Float);
  effect->tileHeight = ExportProperty(context, effectStream, "ADBE Tile-0003", Parsers::Float);
  effect->outputWidth = ExportProperty(context, effectStream, "ADBE Tile-0004", Parsers::Float);
  effect->outputHeight = ExportProperty(context, effectStream, "ADBE Tile-0005", Parsers::Float);
  effect->mirrorEdges = ExportProperty(context, effectStream, "ADBE Tile-0006", Parsers::Boolean);
  effect->phase = ExportProperty(context, effectStream, "ADBE Tile-0007", Parsers::Float);
  effect->horizontalPhaseShift = ExportProperty(context, effectStream, "ADBE Tile-0008", Parsers::Boolean);
  return effect;
}

static pag::LevelsIndividualEffect* ExportLevelsIndividualEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::LevelsIndividualEffect();
  ExportCompositingOption(context, effectStream, effect);

  // RGB
  effect->inputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0004", Parsers::FloatM255);
  effect->inputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0005", Parsers::FloatM255);
  effect->gamma = ExportProperty(context, effectStream, "ADBE Pro Levels2-0006", Parsers::Float);
  effect->outputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0007", Parsers::FloatM255);
  effect->outputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0008", Parsers::FloatM255);

  // red
  effect->redInputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0011", Parsers::FloatM255);
  effect->redInputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0012", Parsers::FloatM255);
  effect->redGamma = ExportProperty(context, effectStream, "ADBE Pro Levels2-0013", Parsers::Float);
  effect->redOutputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0014", Parsers::FloatM255);
  effect->redOutputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0015", Parsers::FloatM255);

  // green
  effect->greenInputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0018", Parsers::FloatM255);
  effect->greenInputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0019", Parsers::FloatM255);
  effect->greenGamma = ExportProperty(context, effectStream, "ADBE Pro Levels2-0020", Parsers::Float);
  effect->greenOutputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0021", Parsers::FloatM255);
  effect->greenOutputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0022", Parsers::FloatM255);

  // blue
  effect->blueInputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0025", Parsers::FloatM255);
  effect->blueInputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0026", Parsers::FloatM255);
  effect->blueGamma = ExportProperty(context, effectStream, "ADBE Pro Levels2-0027", Parsers::Float);
  effect->blueOutputBlack = ExportProperty(context, effectStream, "ADBE Pro Levels2-0028", Parsers::FloatM255);
  effect->blueOutputWhite = ExportProperty(context, effectStream, "ADBE Pro Levels2-0029", Parsers::FloatM255);

  return effect;
}

static pag::CornerPinEffect* ExportCornerPinEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::CornerPinEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->upperLeft = ExportProperty(context, effectStream, "ADBE Corner Pin-0001", Parsers::Point);
  effect->upperRight = ExportProperty(context, effectStream, "ADBE Corner Pin-0002", Parsers::Point);
  effect->lowerLeft = ExportProperty(context, effectStream, "ADBE Corner Pin-0003", Parsers::Point);
  effect->lowerRight = ExportProperty(context, effectStream, "ADBE Corner Pin-0004", Parsers::Point);
  return effect;
}

static pag::BulgeEffect* ExportBulgeEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::BulgeEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->horizontalRadius = ExportProperty(context, effectStream, "ADBE Bulge-0001", Parsers::Float);
  effect->verticalRadius = ExportProperty(context, effectStream, "ADBE Bulge-0002", Parsers::Float);
  effect->bulgeCenter = ExportProperty(context, effectStream, "ADBE Bulge-0003", Parsers::Point);
  effect->bulgeHeight = ExportProperty(context, effectStream, "ADBE Bulge-0004", Parsers::Float);
  effect->taperRadius = ExportProperty(context, effectStream, "ADBE Bulge-0005", Parsers::Float);
  //auto antiAlias = ExportProperty(context, effectStream, "ADBE Bulge-0006", Parsers::Enum);
  effect->pinning = ExportProperty(context, effectStream, "ADBE Bulge-0007", Parsers::Boolean);

  return effect;
}

static pag::FastBlurEffect* ExportFastBlurEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::FastBlurEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->blurriness = ExportProperty(context, effectStream, "ADBE Fast Blur-0001", Parsers::Float);
  effect->blurDimensions = ExportProperty(context, effectStream, "ADBE Fast Blur-0002",
                                          Parsers::BlurDimensionsDirection);
  effect->repeatEdgePixels = ExportProperty(context, effectStream, "ADBE Fast Blur-0003", Parsers::Boolean);

  return effect;
}

static pag::FastBlurEffect* ExportGaussBlurEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::FastBlurEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->blurriness = ExportProperty(context, effectStream, "ADBE Gaussian Blur 2-0001", Parsers::Float);
  effect->blurDimensions = ExportProperty(context, effectStream, "ADBE Gaussian Blur 2-0002",
                                          Parsers::BlurDimensionsDirection);
  effect->repeatEdgePixels = ExportProperty(context, effectStream, "ADBE Gaussian Blur 2-0003", Parsers::Boolean);

  return effect;
}

static pag::GlowEffect* ExportGlowEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::GlowEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->glowThreshold = ExportProperty(context, effectStream, "ADBE Glo2-0002", Parsers::PercentD255);
  effect->glowRadius = ExportProperty(context, effectStream, "ADBE Glo2-0003", Parsers::Float);
  effect->glowIntensity = ExportProperty(context, effectStream, "ADBE Glo2-0004", Parsers::Float);

  return effect;
}

static pag::RadialBlurEffect* ExportRadialBlurEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::RadialBlurEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->amount = ExportProperty(context, effectStream, "ADBE Radial Blur-0001", Parsers::Float);
  effect->center = ExportProperty(context, effectStream, "ADBE Radial Blur-0002", Parsers::Point);
  effect->mode = ExportProperty(context, effectStream, "ADBE Radial Blur-0003", Parsers::RadialBlurMode);
  effect->antialias = ExportProperty(context, effectStream, "ADBE Radial Blur-0004", Parsers::RadialBlurAntialias);

  return effect;
}

static pag::MosaicEffect* ExportMosaicEffect(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::MosaicEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->horizontalBlocks = ExportProperty(context, effectStream, "ADBE Mosaic-0001", Parsers::Uint16);
  effect->verticalBlocks = ExportProperty(context, effectStream, "ADBE Mosaic-0002", Parsers::Uint16);
  effect->sharpColors = ExportProperty(context, effectStream, "ADBE Mosaic-0003", Parsers::Boolean);

  return effect;
}

static pag::BrightnessContrastEffect* ExportBrightnessContrast(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::BrightnessContrastEffect();
  ExportCompositingOption(context, effectStream, effect);

  effect->brightness = ExportProperty(context, effectStream, "ADBE Brightness & Contrast 2-0001", Parsers::Float);
  effect->contrast = ExportProperty(context, effectStream, "ADBE Brightness & Contrast 2-0002", Parsers::Float);
  effect->useOldVersion = ExportProperty(context, effectStream, "ADBE Brightness & Contrast 2-0003", Parsers::Boolean);

  return effect;
}

static pag::HueSaturationEffect* ExportHueSaturation(pagexporter::Context* context, AEGP_StreamRefH effectStream) {
  auto effect = new pag::HueSaturationEffect();
  ExportCompositingOption(context, effectStream, effect);

  //auto a = ExportProperty(context, effectStream, "ADBE HUE SATURATION-0001", Parsers::Uint8);
  auto channelControl = ExportValue(context, effectStream, "ADBE HUE SATURATION-0002", Parsers::ChannelControlType);
  //auto channelRange = ExportProperty(context, effectStream, "ADBE HUE SATURATION-0003", Parsers::Float);
  auto hue = ExportValue(context, effectStream, "ADBE HUE SATURATION-0004", Parsers::Float);
  auto saturation = ExportValue(context, effectStream, "ADBE HUE SATURATION-0005", Parsers::Float);
  auto lightness = ExportValue(context, effectStream, "ADBE HUE SATURATION-0006", Parsers::Float);
  effect->channelControl = channelControl;
  effect->hue[channelControl] = hue;
  effect->saturation[channelControl] = saturation;
  effect->lightness[channelControl] = lightness;
  effect->colorize = ExportValue(context, effectStream, "ADBE HUE SATURATION-0007", Parsers::Boolean);
  effect->colorizeHue = ExportProperty(context, effectStream, "ADBE HUE SATURATION-0008", Parsers::Float);
  effect->colorizeSaturation = ExportProperty(context, effectStream, "ADBE HUE SATURATION-0009", Parsers::Float);
  effect->colorizeLightness = ExportProperty(context, effectStream, "ADBE HUE SATURATION-0010", Parsers::Float);

  return effect;
}

static pag::Effect* ExportEffectByType(pagexporter::Context* context, AEGP_StreamRefH& effectStream,
                                       AEEffectType type) {
  pag::Effect* effect = nullptr;
  switch (type) {
    case AEEffectType::MotionTile:
      effect = ExportMotionTileEffect(context, effectStream);
      break;
    case AEEffectType::LevelsIndividual:
      effect = ExportLevelsIndividualEffect(context, effectStream);
      break;
    case AEEffectType::CornerPin:
      effect = ExportCornerPinEffect(context, effectStream);
      break;
    case AEEffectType::Bulge:
      effect = ExportBulgeEffect(context, effectStream);
      break;
    case AEEffectType::FastBlur:
      effect = ExportFastBlurEffect(context, effectStream);
      break;
    case AEEffectType::GaussBlur:
      effect = ExportGaussBlurEffect(context, effectStream);
      break;
    case AEEffectType::Glow:
      effect = ExportGlowEffect(context, effectStream);
      break;
    case AEEffectType::DisplacementMap:
      effect = ExportDisplacementMapEffect(context, effectStream);
      break;
    case AEEffectType::RadialBlur:
      effect = ExportRadialBlurEffect(context, effectStream);
      break;
    case AEEffectType::Mosaic:
      effect = ExportMosaicEffect(context, effectStream);
      break;
    case AEEffectType::BrightnessContrast:
      effect = ExportBrightnessContrast(context, effectStream);
      break;
    case AEEffectType::HueSaturation:
      effect = ExportHueSaturation(context, effectStream);
      break;
    case AEEffectType::Unknown: {
      auto& suites = context->suites;
      char matchName[200];
      suites.DynamicStreamSuite4()->AEGP_GetMatchName(effectStream, matchName);
      context->pushWarning(pagexporter::AlertInfoType::UnsupportedEffects, matchName);
      break;
    }
    default:
      break;
  }
  return effect;
}

static pag::Effect* ExportEffect(pagexporter::Context* context, const AEGP_EffectRefH& effectH) {
  auto& suites = context->suites;
  A_long numParams;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetEffectNumParamStreams(effectH, &numParams));
  if (numParams < 2) {
    return nullptr;
  }
  AEGP_EffectFlags effectFlags;
  RECORD_ERROR(suites.EffectSuite4()->AEGP_GetEffectFlags(effectH, &effectFlags));
  if ((effectFlags & AEGP_EffectFlags_ACTIVE) == 0) {
    return nullptr;
  }
  AEGP_StreamRefH firstStream;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetNewEffectStreamByIndex(context->pluginID, effectH, 1, &firstStream));
  AEGP_StreamRefH effectStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(context->pluginID, firstStream, &effectStream));
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(firstStream));
  auto type = GetEffectType(context, effectStream);
  pag::Effect* effect = ExportEffectByType(context, effectStream, type);
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(effectStream));
  return effect;
}

std::vector<pag::Effect*> ExportEffects(pagexporter::Context* context, const AEGP_LayerH& layerH) {
  std::vector<pag::Effect*> effects;
  auto& suites = context->suites;
  int numEffects = AEUtils::GetLayerNumEffects(layerH);
  for (int i = 0; i < numEffects; i++) {
    AEGP_EffectRefH effectH;
    RECORD_ERROR(suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(context->pluginID, layerH, i, &effectH));
    auto effect = ExportEffect(context, effectH);
    if (effect != nullptr) {
      effects.push_back(effect);
    }
    RECORD_ERROR(suites.EffectSuite4()->AEGP_DisposeEffect(effectH));
  }
  return effects;
}
