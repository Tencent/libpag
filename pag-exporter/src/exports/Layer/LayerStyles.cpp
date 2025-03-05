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
#include "LayerStyles.h"
#include <unordered_map>
#include "src/exports/Stream/StreamProperty.h"
#include "src/utils/EnumClassHash.h"

// 因为AE里的LayerStyleType比PAG的LayerStyleType更多，所以这里再临时定义一套
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
    {"dropShadow/enabled",       AELayerStyleType::DropShadow},
    {"innerShadow/enabled",      AELayerStyleType::InnerShadow},
    {"outerGlow/enabled",        AELayerStyleType::OuterGlow},
    {"innerGlow/enabled",        AELayerStyleType::InnerGlow},
    {"bevelEmboss/enabled",      AELayerStyleType::BevelEmboss},
    {"chromeFX/enabled",         AELayerStyleType::ChromeFX},
    {"solidFill/enabled",        AELayerStyleType::SolidFill},
    {"gradientFill/enabled",     AELayerStyleType::GradientOverlay},
    {"frameFX/enabled",          AELayerStyleType::Stroke},
};

static const std::unordered_map<AELayerStyleType, std::string, pag::EnumClassHash> layerStyleNameMap = {
    {AELayerStyleType::Unknown,           "未知类型(unknown)"},
    {AELayerStyleType::BlendOptionsGroup, "混合选项(Blend Options Group)"},
    {AELayerStyleType::DropShadow,        "投影(Drop Shadow)"},
    {AELayerStyleType::InnerShadow,       "内阴影(Inner Shadow)"},
    {AELayerStyleType::OuterGlow,         "外发光(Outer Glow)"},
    {AELayerStyleType::InnerGlow,         "内发光(Inner Glow)"},
    {AELayerStyleType::BevelEmboss,       "斜面和浮雕(Bevel Emboss)"},
    {AELayerStyleType::ChromeFX,          "光泽(Satin)"},
    {AELayerStyleType::SolidFill,         "颜色叠加(Color Overlay)"},
    {AELayerStyleType::GradientOverlay,   "渐变叠加(Gradient Overlay)"},
    {AELayerStyleType::Stroke,            "描边(Stroke)"},
};

static AELayerStyleType GetLayerStyleType(pagexporter::Context* context, AEGP_StreamRefH styleStream) {
  auto& suites = context->suites;
  static char matchName[200];
  suites.DynamicStreamSuite4()->AEGP_GetMatchName(styleStream, matchName);
  auto result = layerStyleTypeMap.find(matchName);
  if (result == layerStyleTypeMap.end()) {
    return AELayerStyleType::Unknown;
  }
  return result->second;
}

static std::string GetLayerStyleName(AELayerStyleType type) {
  auto result = layerStyleNameMap.find(type);
  if (result != layerStyleNameMap.end()) {
    return result->second;
  }
  return "unknown";
}

static pag::DropShadowStyle* ExportDropShadowStyle(pagexporter::Context* context, AEGP_StreamRefH styleStream) {
  auto style = new pag::DropShadowStyle();
  style->blendMode = ExportProperty(context, styleStream, "dropShadow/mode2", Parsers::StyleBlendMode);
  style->color = ExportProperty(context, styleStream, "dropShadow/color", Parsers::Color);
  style->opacity = ExportProperty(context, styleStream, "dropShadow/opacity", Parsers::Opacity0_100);
  style->angle = ExportProperty(context, styleStream, "dropShadow/localLightingAngle", Parsers::Float);
  style->distance = ExportProperty(context, styleStream, "dropShadow/distance", Parsers::Float);
  style->size = ExportProperty(context, styleStream, "dropShadow/blur", Parsers::Float);
  style->spread = ExportProperty(context, styleStream, "dropShadow/chokeMatte", Parsers::Percent);
  return style;
}

static pag::GradientOverlayStyle* ExportGradientOverlayStyle(pagexporter::Context* context, AEGP_StreamRefH styleStream) {
  auto style = new pag::GradientOverlayStyle();
  style->blendMode = ExportProperty(context, styleStream, "gradientFill/mode2", Parsers::StyleBlendMode);
  style->opacity = ExportProperty(context, styleStream, "gradientFill/opacity", Parsers::Opacity0_100);
  style->colors = ExportProperty(context, styleStream, "gradientFill/gradient", Parsers::GradientOverlayColor);
  style->gradientSmoothness = ExportProperty(context, styleStream, "gradientFill/gradientSmoothness", Parsers::Float);
  style->angle = ExportProperty(context, styleStream, "gradientFill/angle", Parsers::Float);
  style->style = ExportProperty(context, styleStream, "gradientFill/type", Parsers::GradientOverlayType);
  style->reverse = ExportProperty(context, styleStream, "gradientFill/reverse", Parsers::Boolean);
  style->alignWithLayer = ExportProperty(context, styleStream, "gradientFill/align", Parsers::Boolean);
  style->scale = ExportProperty(context, styleStream, "gradientFill/scale", Parsers::Float);
  style->offset = ExportProperty(context, styleStream, "gradientFill/offset", Parsers::Point);
  return style;
}

static pag::StrokeStyle* ExportStrokeStyle(pagexporter::Context* context, AEGP_StreamRefH styleStream) {
  auto style = new pag::StrokeStyle();
  style->blendMode = ExportProperty(context, styleStream, "frameFX/mode2", Parsers::StyleBlendMode);
  style->color = ExportProperty(context, styleStream, "frameFX/color", Parsers::Color);
  style->size = ExportProperty(context, styleStream, "frameFX/size", Parsers::Float);
  style->opacity = ExportProperty(context, styleStream, "frameFX/opacity", Parsers::Opacity0_100);
  style->position = ExportProperty(context, styleStream, "frameFX/style", Parsers::StrokePosition);
  return style;
}

static pag::OuterGlowStyle* ExportOuterGlowStyle(pagexporter::Context* context, AEGP_StreamRefH styleStream) {
  auto style = new pag::OuterGlowStyle();
  style->blendMode = ExportProperty(context, styleStream, "outerGlow/mode2", Parsers::StyleBlendMode);
  style->opacity = ExportProperty(context, styleStream, "outerGlow/opacity", Parsers::Opacity0_100);
  style->noise = ExportProperty(context, styleStream, "outerGlow/noise", Parsers::Percent);
  style->colorType = ExportProperty(context, styleStream, "outerGlow/AEColorChoice", Parsers::GlowColorType);
  style->color = ExportProperty(context, styleStream, "outerGlow/color", Parsers::Color);
  style->colors = ExportProperty(context, styleStream, "outerGlow/gradient", Parsers::OuterGlowGradientColor);
  style->gradientSmoothness = ExportProperty(context, styleStream, "outerGlow/gradientSmoothness", Parsers::Percent);
  style->technique = ExportProperty(context, styleStream, "outerGlow/glowTechnique", Parsers::GlowTechniqueType);
  style->spread = ExportProperty(context, styleStream, "outerGlow/chokeMatte", Parsers::Percent);
  style->size = ExportProperty(context, styleStream, "outerGlow/blur", Parsers::Float);
  style->range = ExportProperty(context, styleStream, "outerGlow/inputRange", Parsers::Percent);
  style->jitter = ExportProperty(context, styleStream, "outerGlow/shadingNoise", Parsers::Percent);
  return style;
}

std::vector<pag::LayerStyle*> ExportLayerStyles(pagexporter::Context* context, const AEGP_LayerH& layerH) {
  std::vector<pag::LayerStyle*> layerStyles;
  auto& suites = context->suites;
  AEGP_StreamRefH layerStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(context->pluginID, layerH, &layerStream));
  AEGP_StreamRefH rootStream;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, layerStream,
                                                                             "ADBE Layer Styles", &rootStream));
  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(layerStream));
  if (IsStreamHidden(suites, rootStream) || !IsStreamActive(suites, rootStream)) {
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(rootStream));
    return layerStyles;
  }

  A_long numStreams;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(rootStream, &numStreams));
  for (int i = 0; i < numStreams; i++) {
    AEGP_StreamRefH styleStream;
    RECORD_ERROR(
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, rootStream, i, &styleStream));
    if (!IsStreamHidden(suites, styleStream) && IsStreamActive(suites, styleStream)) {
      auto type = GetLayerStyleType(context, styleStream);
      pag::LayerStyle* layerStyle = nullptr;
      switch (type) {
        case AELayerStyleType::DropShadow:
          layerStyle = ExportDropShadowStyle(context, styleStream);
          break;
        case AELayerStyleType::GradientOverlay:
          layerStyle = ExportGradientOverlayStyle(context, styleStream);
          break;
        case AELayerStyleType::Stroke:
          layerStyle = ExportStrokeStyle(context, styleStream);
          break;
        case AELayerStyleType::OuterGlow:
          layerStyle = ExportOuterGlowStyle(context, styleStream);
          break;
        case AELayerStyleType::BlendOptionsGroup:
          // 不触发警告
          break;
        default:
          context->pushWarning(pagexporter::AlertInfoType::UnsupportedLayerStyle, GetLayerStyleName(type));
          break;
      }
      if (layerStyle != nullptr) {
        layerStyles.push_back(layerStyle);
      }
    }
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(styleStream));
  }

  RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(rootStream));

  return layerStyles;
}
