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

#include "LayerStyle.h"
#include "export/stream/StreamProperty.h"
#include "utils/PAGExportSession.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

enum class AELayerStyleType {
  Unknown,
  BlendOptionsGroup,
  DropShadow,
  InnerShadow,
  OuterGlow,
  InnerGlow,
  BevelEmboss,
  ChromeFX,
  SolidFill,
  GradientOverlay,
  Stroke
};

static const std::unordered_map<std::string, AELayerStyleType> layerStyleTypeMap = {
    {"ADBE Blend Options Group", AELayerStyleType::BlendOptionsGroup},
    {"dropShadow/enabled", AELayerStyleType::DropShadow},
    {"innerShadow/enabled", AELayerStyleType::InnerShadow},
    {"outerGlow/enabled", AELayerStyleType::OuterGlow},
    {"innerGlow/enabled", AELayerStyleType::InnerGlow},
    {"bevelEmboss/enabled", AELayerStyleType::BevelEmboss},
    {"chromeFX/enabled", AELayerStyleType::ChromeFX},
    {"solidFill/enabled", AELayerStyleType::SolidFill},
    {"gradientFill/enabled", AELayerStyleType::GradientOverlay},
    {"frameFX/enabled", AELayerStyleType::Stroke},
};

static const std::unordered_map<AELayerStyleType, std::string> layerStyleNameMap = {
    {AELayerStyleType::Unknown, "未知类型(unknown)"},
    {AELayerStyleType::BlendOptionsGroup, "混合选项(Blend Options Group)"},
    {AELayerStyleType::DropShadow, "投影(Drop Shadow)"},
    {AELayerStyleType::InnerShadow, "内阴影(Inner Shadow)"},
    {AELayerStyleType::OuterGlow, "外发光(Outer Glow)"},
    {AELayerStyleType::InnerGlow, "内发光(Inner Glow)"},
    {AELayerStyleType::BevelEmboss, "斜面和浮雕(Bevel Emboss)"},
    {AELayerStyleType::ChromeFX, "光泽(Satin)"},
    {AELayerStyleType::SolidFill, "颜色叠加(Color Overlay)"},
    {AELayerStyleType::GradientOverlay, "渐变叠加(Gradient Overlay)"},
    {AELayerStyleType::Stroke, "描边(Stroke)"},
};

std::string GetLayerStyleName(AELayerStyleType type) {
  auto result = layerStyleNameMap.find(type);
  if (result == layerStyleNameMap.end()) {
    return "unknown";
  }
  return result->second;
}

static pag::LayerStyle* GetDropShadowStyle(const AEGP_StreamRefH& streamHandle) {
  auto style = new pag::DropShadowStyle();

  style->blendMode =
      GetProperty(streamHandle, "dropShadow/mode2", AEStreamParser::StyleBlendModeParser);
  style->color = GetProperty(streamHandle, "dropShadow/color", AEStreamParser::ColorParser);
  style->opacity =
      GetProperty(streamHandle, "dropShadow/opacity", AEStreamParser::Opacity0_100Parser);
  style->angle =
      GetProperty(streamHandle, "dropShadow/localLightingAngle", AEStreamParser::FloatParser);
  style->distance = GetProperty(streamHandle, "dropShadow/distance", AEStreamParser::FloatParser);
  style->size = GetProperty(streamHandle, "dropShadow/blur", AEStreamParser::FloatParser);
  style->spread = GetProperty(streamHandle, "dropShadow/chokeMatte", AEStreamParser::PercentParser);
  return style;
}

static pag::LayerStyle* GetGradientOverlayStyle(const AEGP_StreamRefH& streamHandle) {
  auto style = new pag::GradientOverlayStyle();

  style->blendMode =
      GetProperty(streamHandle, "gradientFill/mode2", AEStreamParser::StyleBlendModeParser);
  style->opacity =
      GetProperty(streamHandle, "gradientFill/opacity", AEStreamParser::Opacity0_100Parser);
  style->colors = GetProperty(streamHandle, "gradientFill/gradient",
                              AEStreamParser::GradientOverlayColorParser);
  style->gradientSmoothness =
      GetProperty(streamHandle, "gradientFill/gradientSmoothness", AEStreamParser::FloatParser);
  style->angle = GetProperty(streamHandle, "gradientFill/angle", AEStreamParser::FloatParser);
  style->style =
      GetProperty(streamHandle, "gradientFill/type", AEStreamParser::GradientOverlayTypeParser);
  style->reverse = GetProperty(streamHandle, "gradientFill/reverse", AEStreamParser::BooleanParser);
  style->alignWithLayer =
      GetProperty(streamHandle, "gradientFill/align", AEStreamParser::BooleanParser);
  style->scale = GetProperty(streamHandle, "gradientFill/scale", AEStreamParser::FloatParser);
  style->offset = GetProperty(streamHandle, "gradientFill/offset", AEStreamParser::PointParser);
  return style;
}

static pag::LayerStyle* GetStrokeStyle(const AEGP_StreamRefH& streamHandle) {
  auto style = new pag::StrokeStyle();

  style->blendMode =
      GetProperty(streamHandle, "frameFX/mode2", AEStreamParser::StyleBlendModeParser);
  style->color = GetProperty(streamHandle, "frameFX/color", AEStreamParser::ColorParser);
  style->size = GetProperty(streamHandle, "frameFX/size", AEStreamParser::FloatParser);
  style->opacity = GetProperty(streamHandle, "frameFX/opacity", AEStreamParser::Opacity0_100Parser);
  style->position =
      GetProperty(streamHandle, "frameFX/style", AEStreamParser::StrokePositionParser);
  return style;
}

static pag::LayerStyle* GetOuterGlowStyle(const AEGP_StreamRefH& streamHandle) {
  auto style = new pag::OuterGlowStyle();

  style->blendMode =
      GetProperty(streamHandle, "outerGlow/mode2", AEStreamParser::StyleBlendModeParser);
  style->opacity =
      GetProperty(streamHandle, "outerGlow/opacity", AEStreamParser::Opacity0_100Parser);
  style->noise = GetProperty(streamHandle, "outerGlow/noise", AEStreamParser::PercentParser);
  style->colorType =
      GetProperty(streamHandle, "outerGlow/AEColorChoice", AEStreamParser::GlowColorTypeParser);
  style->color = GetProperty(streamHandle, "outerGlow/color", AEStreamParser::ColorParser);
  style->colors =
      GetProperty(streamHandle, "outerGlow/gradient", AEStreamParser::OuterGlowGradientColorParser);
  style->gradientSmoothness =
      GetProperty(streamHandle, "outerGlow/gradientSmoothness", AEStreamParser::PercentParser);
  style->technique =
      GetProperty(streamHandle, "outerGlow/glowTechnique", AEStreamParser::GlowTechniqueTypeParser);
  style->spread = GetProperty(streamHandle, "outerGlow/chokeMatte", AEStreamParser::PercentParser);
  style->size = GetProperty(streamHandle, "outerGlow/blur", AEStreamParser::FloatParser);
  style->range = GetProperty(streamHandle, "outerGlow/inputRange", AEStreamParser::PercentParser);
  style->jitter =
      GetProperty(streamHandle, "outerGlow/shadingNoise", AEStreamParser::PercentParser);
  return style;
}

static pag::LayerStyle* GetLayerStyleByType(const AEGP_StreamRefH& streamHandle,
                                            AELayerStyleType type) {
  pag::LayerStyle* layerStyle = nullptr;
  switch (type) {
    case AELayerStyleType::DropShadow:
      layerStyle = GetDropShadowStyle(streamHandle);
      break;
    case AELayerStyleType::GradientOverlay:
      layerStyle = GetGradientOverlayStyle(streamHandle);
      break;
    case AELayerStyleType::Stroke:
      layerStyle = GetStrokeStyle(streamHandle);
      break;
    case AELayerStyleType::OuterGlow:
      layerStyle = GetOuterGlowStyle(streamHandle);
      break;
    case AELayerStyleType::BlendOptionsGroup:
      break;
    default:
      PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::UnsupportedLayerStyle,
                                                            GetLayerStyleName(type));
      break;
  }
  return layerStyle;
}

static AELayerStyleType GetLayerStyleType(const AEGP_StreamRefH& streamHandle) {
  std::string matchName = GetStreamMatchName(streamHandle);
  auto result = layerStyleTypeMap.find(matchName);
  if (result == layerStyleTypeMap.end()) {
    return AELayerStyleType::Unknown;
  }
  return result->second;
}

std::vector<pag::LayerStyle*> GetLayerStyles(const AEGP_LayerH& layerHandle) {
  const auto Suites = GetSuites();
  const auto PluginID = GetPluginID();

  std::vector<pag::LayerStyle*> layerStyles = {};
  AEGP_StreamRefH layerStream = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(PluginID, layerHandle, &layerStream);
  if (layerStream == nullptr) {
    return layerStyles;
  }
  AEGP_StreamRefH rootStream = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(PluginID, layerStream,
                                                                 "ADBE Layer Styles", &rootStream);
  if (rootStream == nullptr) {
    Suites->StreamSuite4()->AEGP_DisposeStream(layerStream);
    return layerStyles;
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(layerStream);
  if (IsStreamHidden(rootStream) || !IsStreamActive(rootStream)) {
    Suites->StreamSuite4()->AEGP_DisposeStream(rootStream);
    return layerStyles;
  }

  A_long numStreams;
  Suites->DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStream, &numStreams);
  for (int index = 0; index < numStreams; index++) {
    AEGP_StreamRefH styleStreamHandle = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, rootStream, index,
                                                               &styleStreamHandle);
    if (styleStreamHandle == nullptr) {
      continue;
    }
    if (IsStreamHidden(styleStreamHandle) || !IsStreamActive(styleStreamHandle)) {
      Suites->StreamSuite4()->AEGP_DisposeStream(styleStreamHandle);
      continue;
    }

    auto type = GetLayerStyleType(styleStreamHandle);
    pag::LayerStyle* layerStyle = GetLayerStyleByType(styleStreamHandle, type);
    if (layerStyle != nullptr) {
      layerStyles.push_back(layerStyle);
    }
    Suites->StreamSuite4()->AEGP_DisposeStream(styleStreamHandle);
  }

  Suites->StreamSuite4()->AEGP_DisposeStream(rootStream);

  return layerStyles;
}

}  // namespace exporter
