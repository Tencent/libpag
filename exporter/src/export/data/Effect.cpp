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

#include "Effect.h"
#include <unordered_map>
#include "export/PAGExport.h"
#include "export/stream/StreamProperty.h"
#include "utils/AEHelper.h"
#include "utils/PAGExportSession.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

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

static const std::unordered_map<std::string, AEEffectType> EffectTypeMap = {
    {"ADBE Image Fill Rule", AEEffectType::ImageFillRule},
    {"ADBE Image Fill Rule2", AEEffectType::ImageFillRuleV2},
    {"ADBE Text Background", AEEffectType::TextBackground},
    {"ADBE Tile", AEEffectType::MotionTile},
    {"ADBE Pro Levels2", AEEffectType::LevelsIndividual},
    {"ADBE Corner Pin", AEEffectType::CornerPin},
    {"ADBE Bulge", AEEffectType::Bulge},
    {"ADBE Fast Blur", AEEffectType::FastBlur},
    {"ADBE Gaussian Blur 2", AEEffectType::GaussBlur},
    {"ADBE Glo2", AEEffectType::Glow},
    {"ADBE Displacement Map", AEEffectType::DisplacementMap},
    {"ADBE Radial Blur", AEEffectType::RadialBlur},
    {"ADBE Mosaic", AEEffectType::Mosaic},
    {"ADBE Brightness & Contrast 2", AEEffectType::BrightnessContrast},
    {"ADBE HUE SATURATION", AEEffectType::HueSaturation},
};

static pag::Effect* GetHueSaturationEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::HueSaturationEffect();

  auto channelControl =
      GetValue(streamHandle, "ADBE HUE SATURATION-0002", AEStreamParser::ChannelControlTypeParser);
  auto hue = GetValue(streamHandle, "ADBE HUE SATURATION-0004", AEStreamParser::FloatParser);
  auto saturation = GetValue(streamHandle, "ADBE HUE SATURATION-0005", AEStreamParser::FloatParser);
  auto lightness = GetValue(streamHandle, "ADBE HUE SATURATION-0006", AEStreamParser::FloatParser);
  effect->channelControl = channelControl;
  effect->hue[static_cast<int>(effect->channelControl)] = hue;
  effect->saturation[static_cast<int>(effect->channelControl)] = saturation;
  effect->lightness[static_cast<int>(effect->channelControl)] = lightness;
  effect->colorize =
      GetValue(streamHandle, "ADBE HUE SATURATION-0007", AEStreamParser::BooleanParser);
  effect->colorizeHue =
      GetProperty(streamHandle, "ADBE HUE SATURATION-0008", AEStreamParser::FloatParser);
  effect->colorizeSaturation =
      GetProperty(streamHandle, "ADBE HUE SATURATION-0009", AEStreamParser::FloatParser);
  effect->colorizeLightness =
      GetProperty(streamHandle, "ADBE HUE SATURATION-0010", AEStreamParser::FloatParser);

  return effect;
}

static pag::Effect* GetBrightnessContrastEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::BrightnessContrastEffect();

  effect->brightness =
      GetProperty(streamHandle, "ADBE Brightness & Contrast 2-0001", AEStreamParser::FloatParser);
  effect->contrast =
      GetProperty(streamHandle, "ADBE Brightness & Contrast 2-0002", AEStreamParser::FloatParser);
  effect->useOldVersion =
      GetProperty(streamHandle, "ADBE Brightness & Contrast 2-0003", AEStreamParser::BooleanParser);
  return effect;
}

static pag::Effect* GetMosaicEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::MosaicEffect();

  effect->horizontalBlocks =
      GetProperty(streamHandle, "ADBE Mosaic-0001", AEStreamParser::Uint16Parser);
  effect->verticalBlocks =
      GetProperty(streamHandle, "ADBE Mosaic-0002", AEStreamParser::Uint16Parser);
  effect->sharpColors =
      GetProperty(streamHandle, "ADBE Mosaic-0003", AEStreamParser::BooleanParser);
  return effect;
}

static pag::Effect* GetRadialBlurEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::RadialBlurEffect();

  effect->amount = GetProperty(streamHandle, "ADBE Radial Blur-0001", AEStreamParser::FloatParser);
  effect->center = GetProperty(streamHandle, "ADBE Radial Blur-0002", AEStreamParser::PointParser);
  effect->mode =
      GetProperty(streamHandle, "ADBE Radial Blur-0003", AEStreamParser::RadialBlurModeParser);
  effect->antialias =
      GetProperty(streamHandle, "ADBE Radial Blur-0004", AEStreamParser::RadialBlurAntialiasParser);
  return effect;
}

static pag::Effect* GetDisplacementMapEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::DisplacementMapEffect();

  effect->displacementMapLayer = new pag::Layer();
  effect->displacementMapLayer->id =
      GetProperty(streamHandle, "ADBE Displacement Map-0001", AEStreamParser::LayerIDParser)->value;
  if (effect->displacementMapLayer->id <= 0) {
    delete effect->displacementMapLayer;
    effect->displacementMapLayer = nullptr;
  }
  effect->useForHorizontalDisplacement = GetProperty(streamHandle, "ADBE Displacement Map-0002",
                                                     AEStreamParser::DisplacementMapSourceParser);
  effect->maxHorizontalDisplacement =
      GetProperty(streamHandle, "ADBE Displacement Map-0003", AEStreamParser::FloatParser);
  effect->useForVerticalDisplacement = GetProperty(streamHandle, "ADBE Displacement Map-0004",
                                                   AEStreamParser::DisplacementMapSourceParser);
  effect->maxVerticalDisplacement =
      GetProperty(streamHandle, "ADBE Displacement Map-0005", AEStreamParser::FloatParser);
  effect->displacementMapBehavior = GetProperty(streamHandle, "ADBE Displacement Map-0006",
                                                AEStreamParser::DisplacementMapBehaviorParser);
  effect->edgeBehavior =
      GetProperty(streamHandle, "ADBE Displacement Map-0007", AEStreamParser::BooleanParser);
  effect->expandOutput =
      GetProperty(streamHandle, "ADBE Displacement Map-0008", AEStreamParser::BooleanParser);
  return effect;
}

static pag::Effect* GetGlowEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::GlowEffect();

  effect->glowThreshold =
      GetProperty(streamHandle, "ADBE Glo2-0002", AEStreamParser::PercentD255Parser);
  effect->glowRadius = GetProperty(streamHandle, "ADBE Glo2-0003", AEStreamParser::FloatParser);
  effect->glowIntensity = GetProperty(streamHandle, "ADBE Glo2-0004", AEStreamParser::FloatParser);
  return effect;
}

static pag::Effect* GetGaussBlurEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::FastBlurEffect();

  effect->blurriness =
      GetProperty(streamHandle, "ADBE Gaussian Blur 2-0001", AEStreamParser::FloatParser);
  effect->blurDimensions = GetProperty(streamHandle, "ADBE Gaussian Blur 2-0002",
                                       AEStreamParser::BlurDimensionsDirectionParser);
  effect->repeatEdgePixels =
      GetProperty(streamHandle, "ADBE Gaussian Blur 2-0003", AEStreamParser::BooleanParser);
  return effect;
}

static pag::Effect* GetFastBlurEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::FastBlurEffect();

  effect->blurriness =
      GetProperty(streamHandle, "ADBE Fast Blur-0001", AEStreamParser::FloatParser);
  effect->blurDimensions = GetProperty(streamHandle, "ADBE Fast Blur-0002",
                                       AEStreamParser::BlurDimensionsDirectionParser);
  effect->repeatEdgePixels =
      GetProperty(streamHandle, "ADBE Fast Blur-0003", AEStreamParser::BooleanParser);

  return effect;
}

static pag::Effect* GetBulgeEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::BulgeEffect();

  effect->horizontalRadius =
      GetProperty(streamHandle, "ADBE Bulge-0001", AEStreamParser::FloatParser);
  effect->verticalRadius =
      GetProperty(streamHandle, "ADBE Bulge-0002", AEStreamParser::FloatParser);
  effect->bulgeCenter = GetProperty(streamHandle, "ADBE Bulge-0003", AEStreamParser::PointParser);
  effect->bulgeHeight = GetProperty(streamHandle, "ADBE Bulge-0004", AEStreamParser::FloatParser);
  effect->taperRadius = GetProperty(streamHandle, "ADBE Bulge-0005", AEStreamParser::FloatParser);
  effect->pinning = GetProperty(streamHandle, "ADBE Bulge-0007", AEStreamParser::BooleanParser);
  return effect;
}

static pag::Effect* GetCornerPinEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::CornerPinEffect();

  effect->upperLeft =
      GetProperty(streamHandle, "ADBE Corner Pin-0001", AEStreamParser::PointParser);
  effect->upperRight =
      GetProperty(streamHandle, "ADBE Corner Pin-0002", AEStreamParser::PointParser);
  effect->lowerLeft =
      GetProperty(streamHandle, "ADBE Corner Pin-0003", AEStreamParser::PointParser);
  effect->lowerRight =
      GetProperty(streamHandle, "ADBE Corner Pin-0004", AEStreamParser::PointParser);
  return effect;
}

static pag::Effect* GetLevelsIndividualEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::LevelsIndividualEffect();

  effect->inputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0004", AEStreamParser::FloatM255Parser);
  effect->inputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0005", AEStreamParser::FloatM255Parser);
  effect->gamma = GetProperty(streamHandle, "ADBE Pro Levels2-0006", AEStreamParser::FloatParser);
  effect->outputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0007", AEStreamParser::FloatM255Parser);
  effect->outputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0008", AEStreamParser::FloatM255Parser);

  effect->redInputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0011", AEStreamParser::FloatM255Parser);
  effect->redInputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0012", AEStreamParser::FloatM255Parser);
  effect->redGamma =
      GetProperty(streamHandle, "ADBE Pro Levels2-0013", AEStreamParser::FloatParser);
  effect->redOutputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0014", AEStreamParser::FloatM255Parser);
  effect->redOutputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0015", AEStreamParser::FloatM255Parser);

  effect->greenInputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0018", AEStreamParser::FloatM255Parser);
  effect->greenInputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0019", AEStreamParser::FloatM255Parser);
  effect->greenGamma =
      GetProperty(streamHandle, "ADBE Pro Levels2-0020", AEStreamParser::FloatParser);
  effect->greenOutputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0021", AEStreamParser::FloatM255Parser);
  effect->greenOutputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0022", AEStreamParser::FloatM255Parser);

  effect->blueInputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0025", AEStreamParser::FloatM255Parser);
  effect->blueInputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0026", AEStreamParser::FloatM255Parser);
  effect->blueGamma =
      GetProperty(streamHandle, "ADBE Pro Levels2-0027", AEStreamParser::FloatParser);
  effect->blueOutputBlack =
      GetProperty(streamHandle, "ADBE Pro Levels2-0028", AEStreamParser::FloatM255Parser);
  effect->blueOutputWhite =
      GetProperty(streamHandle, "ADBE Pro Levels2-0029", AEStreamParser::FloatM255Parser);

  return effect;
}

static pag::Effect* GetMotionTileEffect(const AEGP_StreamRefH& streamHandle) {
  auto effect = new pag::MotionTileEffect();
  effect->tileCenter = GetProperty(streamHandle, "ADBE Tile-0001", AEStreamParser::PointParser);
  effect->tileWidth = GetProperty(streamHandle, "ADBE Tile-0002", AEStreamParser::FloatParser);
  effect->tileHeight = GetProperty(streamHandle, "ADBE Tile-0003", AEStreamParser::FloatParser);
  effect->outputWidth = GetProperty(streamHandle, "ADBE Tile-0004", AEStreamParser::FloatParser);
  effect->outputHeight = GetProperty(streamHandle, "ADBE Tile-0005", AEStreamParser::FloatParser);
  effect->mirrorEdges = GetProperty(streamHandle, "ADBE Tile-0006", AEStreamParser::BooleanParser);
  effect->phase = GetProperty(streamHandle, "ADBE Tile-0007", AEStreamParser::FloatParser);
  effect->horizontalPhaseShift =
      GetProperty(streamHandle, "ADBE Tile-0008", AEStreamParser::BooleanParser);
  return effect;
}

static void InitEffect(const AEGP_StreamRefH& streamHandle, pag::Effect* effect) {
  if (effect == nullptr) {
    return;
  }

  const auto Suites = GetSuites();
  const auto PluginID = GetPluginID();
  static char matchName[AEGP_MAX_STREAM_MATCH_NAME_SIZE] = {0};

  A_long numParams = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamHandle, &numParams);
  AEGP_StreamRefH builtInParams = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, streamHandle, numParams - 1,
                                                             &builtInParams);
  Suites->DynamicStreamSuite4()->AEGP_GetMatchName(builtInParams, matchName);
  if (strcmp(matchName, "ADBE Effect Built In Params") != 0) {
    Suites->StreamSuite4()->AEGP_DisposeStream(builtInParams);
    return;
  }
  AEGP_StreamRefH masks = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, builtInParams, 0, &masks);
  A_long numMasks = 0;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(masks, &numMasks);
  for (int i = 0; i < numMasks; i++) {
    AEGP_StreamRefH maskReference = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, masks, i, &maskReference);
    auto maskID = GetValue(maskReference, AEStreamParser::MaskIDParser);
    auto mask = new pag::MaskData();
    mask->id = maskID;
    Suites->StreamSuite4()->AEGP_DisposeStream(maskReference);
    effect->maskReferences.push_back(mask);
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(masks);
  effect->effectOpacity = GetProperty(builtInParams, 1, AEStreamParser::Opacity0_100Parser);
  Suites->StreamSuite4()->AEGP_DisposeStream(builtInParams);
  if (!effect->effectOpacity->animatable() && effect->effectOpacity->value == pag::Opaque) {
    delete effect->effectOpacity;
    effect->effectOpacity = nullptr;
  }
}

static pag::Effect* GetEffectByType(const AEGP_StreamRefH& streamHandle, AEEffectType type) {
  pag::Effect* effect = nullptr;
  switch (type) {
    case AEEffectType::MotionTile:
      effect = GetMotionTileEffect(streamHandle);
      break;
    case AEEffectType::LevelsIndividual:
      effect = GetLevelsIndividualEffect(streamHandle);
      break;
    case AEEffectType::CornerPin:
      effect = GetCornerPinEffect(streamHandle);
      break;
    case AEEffectType::Bulge:
      effect = GetBulgeEffect(streamHandle);
      break;
    case AEEffectType::FastBlur:
      effect = GetFastBlurEffect(streamHandle);
      break;
    case AEEffectType::GaussBlur:
      effect = GetGaussBlurEffect(streamHandle);
      break;
    case AEEffectType::Glow:
      effect = GetGlowEffect(streamHandle);
      break;
    case AEEffectType::DisplacementMap:
      effect = GetDisplacementMapEffect(streamHandle);
      break;
    case AEEffectType::RadialBlur:
      effect = GetRadialBlurEffect(streamHandle);
      break;
    case AEEffectType::Mosaic:
      effect = GetMosaicEffect(streamHandle);
      break;
    case AEEffectType::BrightnessContrast:
      effect = GetBrightnessContrastEffect(streamHandle);
      break;
    case AEEffectType::HueSaturation:
      effect = GetHueSaturationEffect(streamHandle);
      break;
    case AEEffectType::Unknown:
      PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::UnsupportedEffects,
                                                            GetStreamMatchName(streamHandle));
      break;
    default:
      break;
  }
  InitEffect(streamHandle, effect);
  return effect;
}

static AEEffectType GetEffectType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = EffectTypeMap.find(matchName);
  if (result == EffectTypeMap.end()) {
    return AEEffectType::Unknown;
  }
  return result->second;
}

static pag::Effect* GetEffect(const AEGP_EffectRefH& effectHandle) {
  const auto PluginID = GetPluginID();
  const auto Suites = GetSuites();

  A_long numParams = 0;
  Suites->StreamSuite4()->AEGP_GetEffectNumParamStreams(effectHandle, &numParams);
  if (numParams < 2) {
    return nullptr;
  }
  AEGP_EffectFlags effectFlags;
  Suites->EffectSuite4()->AEGP_GetEffectFlags(effectHandle, &effectFlags);
  if ((effectFlags & AEGP_EffectFlags_ACTIVE) == 0) {
    return nullptr;
  }
  AEGP_StreamRefH firstStreamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewEffectStreamByIndex(PluginID, effectHandle, 1,
                                                         &firstStreamHandle);
  if (firstStreamHandle == nullptr) {
    return nullptr;
  }
  AEGP_StreamRefH effectStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(PluginID, firstStreamHandle,
                                                            &effectStreamHandle);
  if (effectStreamHandle == nullptr) {
    Suites->StreamSuite4()->AEGP_DisposeStream(firstStreamHandle);
    return nullptr;
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(firstStreamHandle);
  auto type = GetEffectType(effectStreamHandle);
  pag::Effect* effect = GetEffectByType(effectStreamHandle, type);
  Suites->StreamSuite4()->AEGP_DisposeStream(effectStreamHandle);
  return effect;
}

std::vector<pag::Effect*> GetEffects(const AEGP_LayerH& layerHandle) {
  const auto PluginID = GetPluginID();
  const auto Suites = GetSuites();
  std::vector<pag::Effect*> effects = {};

  int numEffects = GetLayerEffectNum(layerHandle);
  for (int i = 0; i < numEffects; i++) {
    AEGP_EffectRefH effectHandle = nullptr;
    Suites->EffectSuite4()->AEGP_GetLayerEffectByIndex(PluginID, layerHandle, i, &effectHandle);
    if (effectHandle == nullptr) {
      continue;
    }
    auto effect = GetEffect(effectHandle);
    if (effect != nullptr) {
      effects.push_back(effect);
    }
    Suites->EffectSuite4()->AEGP_DisposeEffect(effectHandle);
  }
  return effects;
}

static void GetTextBackground(const AEGP_StreamRefH& streamHandle,
                              pag::Property<pag::TextDocumentHandle>* textDocument) {
  auto backgroundColor =
      GetValue(streamHandle, "ADBE Text Background-0001", AEStreamParser::ColorParser);
  auto backgroundAlpha =
      GetValue(streamHandle, "ADBE Text Background-0002", AEStreamParser::Opacity0_100Parser);
  if (textDocument->animatable()) {
    for (auto& keyframe :
         reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(textDocument)
             ->keyframes) {
      keyframe->startValue->backgroundColor = backgroundColor;
      keyframe->startValue->backgroundAlpha = backgroundAlpha;
      keyframe->endValue->backgroundColor = backgroundColor;
      keyframe->endValue->backgroundAlpha = backgroundAlpha;
    }
  } else {
    auto document = textDocument->getValueAt(0);
    document->backgroundColor = backgroundColor;
    document->backgroundAlpha = backgroundAlpha;
  }
}

static pag::ImageFillRule* GetImageFillRule(const AEGP_StreamRefH& streamHandle, float frameRate,
                                            uint16_t tagLevel, AEEffectType type) {
  if (tagLevel < static_cast<uint16_t>(pag::TagCode::ImageFillRule)) {
    PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::TagLevelImageFillRule);
    return nullptr;
  }

  auto imageFillRule = new pag::ImageFillRule();
  QVariantMap map = {};
  map["frameRate"] = frameRate;
  if (type == AEEffectType::ImageFillRule) {
    imageFillRule->scaleMode =
        GetValue(streamHandle, "ADBE Image Fill Rule1-0001", AEStreamParser::ScaleModeParser);
    imageFillRule->timeRemap =
        GetProperty(streamHandle, "ADBE Image Fill Rule1-0002", AEStreamParser::TimeParser, map);
  } else {
    imageFillRule->scaleMode =
        GetValue(streamHandle, "ADBE Image Fill Rule2-0001", AEStreamParser::ScaleModeParser);
    imageFillRule->timeRemap =
        GetProperty(streamHandle, "ADBE Image Fill Rule2-0002", AEStreamParser::TimeParser, map);

    if (tagLevel < static_cast<uint16_t>(pag::TagCode::ImageFillRuleV2)) {
      PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::TagLevelImageFillRuleV2);
    }
  }

  if (!(type == AEEffectType::ImageFillRuleV2 &&
        tagLevel >= static_cast<uint16_t>(pag::TagCode::ImageFillRuleV2)) &&
      imageFillRule->timeRemap->animatable()) {
    auto timeRemap = imageFillRule->timeRemap;
    for (auto& keyFrame : static_cast<pag::AnimatableProperty<pag::Frame>*>(timeRemap)->keyframes) {
      keyFrame->interpolationType = pag::KeyframeInterpolationType::Linear;
    }
  }

  return imageFillRule;
}

static void GetAttachment(const AEGP_EffectRefH& effectHandle, float frameRate, pag::Layer* layer,
                          uint16_t tagLevel) {
  const auto PluginID = GetPluginID();
  const auto Suites = GetSuites();

  A_long numParams = 0;
  Suites->StreamSuite4()->AEGP_GetEffectNumParamStreams(effectHandle, &numParams);
  if (numParams < 2) {
    return;
  }

  AEGP_EffectFlags effectFlags;
  Suites->EffectSuite4()->AEGP_GetEffectFlags(effectHandle, &effectFlags);
  if ((effectFlags & AEGP_EffectFlags_ACTIVE) == 0) {
    return;
  }

  AEGP_StreamRefH firstStreamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewEffectStreamByIndex(PluginID, effectHandle, 1,
                                                         &firstStreamHandle);
  if (firstStreamHandle == nullptr) {
    return;
  }
  AEGP_StreamRefH effectStreamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(PluginID, firstStreamHandle,
                                                            &effectStreamHandle);
  if (effectStreamHandle == nullptr) {
    Suites->StreamSuite4()->AEGP_DisposeStream(firstStreamHandle);
    return;
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(firstStreamHandle);
  auto type = GetEffectType(effectStreamHandle);
  switch (type) {
    case AEEffectType::TextBackground:
      if (layer->type() == pag::LayerType::Text) {
        GetTextBackground(effectStreamHandle, static_cast<pag::TextLayer*>(layer)->sourceText);
      } else {
        PAGExportSessionManager::GetInstance()->recordWarning(
            AlertInfoType::TextBackgroundOnlyTextLayer);
      }
      break;
    case AEEffectType::ImageFillRule:
    case AEEffectType::ImageFillRuleV2:
      if (layer->type() == pag::LayerType::Image) {
        if (static_cast<pag::ImageLayer*>(layer)->imageFillRule == nullptr) {
          static_cast<pag::ImageLayer*>(layer)->imageFillRule =
              GetImageFillRule(effectStreamHandle, frameRate, tagLevel, type);
        } else {
          PAGExportSessionManager::GetInstance()->recordWarning(
              AlertInfoType::ImageFillRuleOnlyOne);
        }
      } else {
        PAGExportSessionManager::GetInstance()->recordWarning(
            AlertInfoType::ImageFillRuleOnlyImageLayer);
      }
      break;
    default:
      break;
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(effectStreamHandle);
}

void GetAttachments(const AEGP_LayerH& layerHandle, float frameRate, pag::Layer* layer,
                    uint16_t tagLevel) {
  const auto PluginID = GetPluginID();
  const auto Suites = GetSuites();

  int numEffects = GetLayerEffectNum(layerHandle);
  for (int index = 0; index < numEffects; index++) {
    AEGP_EffectRefH effectHandle = nullptr;
    Suites->EffectSuite4()->AEGP_GetLayerEffectByIndex(PluginID, layerHandle, index, &effectHandle);
    if (effectHandle == nullptr) {
      continue;
    }
    GetAttachment(effectHandle, frameRate, layer, tagLevel);
    Suites->EffectSuite4()->AEGP_DisposeEffect(effectHandle);
  }
}

}  // namespace exporter
