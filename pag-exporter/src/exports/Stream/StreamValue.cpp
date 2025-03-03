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
#include <AE_GeneralPlug.h>
#include <AE_EffectSuites.h>
#include "StreamValue.h"

static pag::Frame TimeParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::Frame>(round(streamValue.one_d * context->frameRate));
}

static float FloatParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<float>(streamValue.one_d);
}

static float FloatM255Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<float>(streamValue.one_d) * 255.0f;
}

static float PercentParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<float>(streamValue.one_d) / 100.0f;
}

static float PercentD255Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<float>(streamValue.one_d) / 255.0f;
}

static bool BooleanParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<bool>(streamValue.one_d);
}

static bool ShapeDirectionParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<bool>(streamValue.one_d == 3);
}

static pag::ID IDParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::ID>(streamValue.mask_id);
}

static pag::Layer* LayerIDParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  auto id = static_cast<pag::ID>(streamValue.layer_id);
  if (id > 0) {
    auto layer = new pag::Layer();
    layer->id = id;
    return layer;
  }
  return nullptr;
}

static pag::MaskData* MaskIDParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  auto id = static_cast<pag::ID>(streamValue.mask_id);
  if (id > 0) {
    auto mask = new pag::MaskData();
    mask->id = id;
    return mask;
  }
  return nullptr;
}

static pag::Opacity Uint8Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::Opacity>(round(streamValue.one_d));
}

static uint16_t Uint16Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<uint16_t>(round(streamValue.one_d));
}

static int32_t Int32Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<int32_t>(round(streamValue.one_d));
}

static pag::Opacity Opacity0_100Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::Opacity>(round(streamValue.one_d * 2.55));
}

static pag::Opacity Opacity0_255Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::Opacity>(round(streamValue.one_d));
}

static pag::Opacity Opacity0_1Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return static_cast<pag::Opacity>(round(streamValue.one_d * 255));
}

static pag::Point PointParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportPoint(streamValue.two_d);
}

static pag::Point3D Point3DParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportPoint3D(streamValue.three_d);
}

static pag::Point ScaleParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  pag::Point pagPoint = {};
  pagPoint.x = static_cast<float>(streamValue.two_d.x * 0.01f);
  pagPoint.y = static_cast<float>(streamValue.two_d.y * 0.01f);
  return pagPoint;
}

static pag::Point3D Scale3DParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  pag::Point3D pagPoint = {};
  pagPoint.x = static_cast<float>(streamValue.three_d.x * 0.01f);
  pagPoint.y = static_cast<float>(streamValue.three_d.y * 0.01f);
  pagPoint.z = static_cast<float>(streamValue.three_d.z * 0.01f);
  return pagPoint;
}

static pag::Color ColorParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportColor(streamValue.color);
}

static pag::Enum ShapeBlendModeParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportShapeBlendMode(static_cast<int>(streamValue.one_d));
}

static pag::Enum StyleBlendModeParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportStyleBlendMode(static_cast<int>(streamValue.one_d));
}

static pag::Enum GradientOverlayTypeParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return ExportGradientOverlayType(static_cast<int>(streamValue.one_d));
}

static void AddVertices(pag::PathData* path, const AEGP_MaskVertex& vertex, const AEGP_MaskVertex& lastVertex) {
  if (lastVertex.tan_out_x != 0 || lastVertex.tan_out_y != 0 || vertex.tan_in_x != 0 || vertex.tan_in_y != 0) {
    path->verbs.push_back(pag::PathDataVerb::CurveTo);
    path->points.emplace_back(pag::Point::Make(static_cast<float>(lastVertex.x + lastVertex.tan_out_x),
                                               static_cast<float>(lastVertex.y + lastVertex.tan_out_y)));
    path->points.emplace_back(pag::Point::Make(static_cast<float>(vertex.tan_in_x + vertex.x),
                                               static_cast<float>(vertex.tan_in_y + vertex.y)));
  } else {
    path->verbs.push_back(pag::PathDataVerb::LineTo);
  }
  path->points.emplace_back(pag::Point::Make(static_cast<float>(vertex.x), static_cast<float>(vertex.y)));
}

static pag::PathHandle PathParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  auto& suites = context->suites;
  auto pathStream = streamValue.mask;
  auto path = new pag::PathData();
  A_long numSegments;
  suites.MaskOutlineSuite3()->AEGP_GetMaskOutlineNumSegments(pathStream, &numSegments);
  if (numSegments == 0) {
    return pag::PathHandle(path);
  }
  AEGP_MaskVertex lastVertex = {};
  for (int i = 0; i <= numSegments; i++) {
    AEGP_MaskVertex vertex = {};
    suites.MaskOutlineSuite3()->AEGP_GetMaskOutlineVertexInfo(pathStream, i, &vertex);
    if (i == 0) {
      path->moveTo(static_cast<float>(vertex.x), static_cast<float>(vertex.y));
    } else {
      AddVertices(path, vertex, lastVertex);
    }
    lastVertex = vertex;
  }
  A_Boolean isOpen = 0;
  suites.MaskOutlineSuite3()->AEGP_IsMaskOutlineOpen(pathStream, &isOpen);
  if (isOpen == 0) {
    path->verbs.push_back(pag::PathDataVerb::Close);
  }
  return pag::PathHandle(path);
}

static pag::TextDocumentHandle TextDocumentParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return context->currentTextDocument();
}

static pag::GradientColorHandle GradientColorParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return context->currentGradientColors({"ADBE Vector Graphic - G-Fill", "ADBE Vector Graphic - G-Stroke"}
                                        , context->gradientIndex);
}

static pag::GradientColorHandle GradientOverlayColorParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return context->currentGradientColors({"gradientFill/gradient"});
}

static pag::GradientColorHandle OuterGlowGradientColorParser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) {
  return context->currentGradientColors({"outerGlow/gradient"});
}

ValueParser<pag::Frame> Parsers::Time = TimeParser;
ValueParser<float> Parsers::Float = FloatParser;
ValueParser<float> Parsers::FloatM255 = FloatM255Parser;
ValueParser<float> Parsers::Percent = PercentParser;
ValueParser<float> Parsers::PercentD255 = PercentD255Parser;
ValueParser<bool> Parsers::Boolean = BooleanParser;
ValueParser<pag::ID> Parsers::ID = IDParser;
ValueParser<pag::Layer*> Parsers::LayerID = LayerIDParser;
ValueParser<pag::MaskData*> Parsers::MaskID = MaskIDParser;
ValueParser<uint8_t> Parsers::Uint8 = Uint8Parser;
ValueParser<uint16_t> Parsers::Uint16 = Uint16Parser;
ValueParser<int32_t> Parsers::Int32 = Int32Parser;
ValueParser<pag::Opacity> Parsers::Opacity0_100 = Opacity0_100Parser;
ValueParser<pag::Opacity> Parsers::Opacity0_255 = Opacity0_255Parser;
ValueParser<pag::Opacity> Parsers::Opacity0_1 = Opacity0_1Parser;
ValueParser<pag::Point> Parsers::Point = PointParser;
ValueParser<pag::Point3D> Parsers::Point3D = Point3DParser;
ValueParser<pag::Point> Parsers::Scale = ScaleParser;
ValueParser<pag::Point3D> Parsers::Scale3D = Scale3DParser;
ValueParser<pag::Color> Parsers::Color = ColorParser;
ValueParser<pag::PathHandle> Parsers::Path = PathParser;
ValueParser<pag::TextDocumentHandle> Parsers::TextDocument = TextDocumentParser;
ValueParser<pag::GradientColorHandle> Parsers::GradientColor = GradientColorParser;
ValueParser<pag::GradientColorHandle> Parsers::GradientOverlayColor = GradientOverlayColorParser;
ValueParser<bool> Parsers::ShapeDirection = ShapeDirectionParser;
ValueParser<pag::Enum> Parsers::ShapeBlendMode = ShapeBlendModeParser;
ValueParser<pag::Enum> Parsers::StyleBlendMode = StyleBlendModeParser;
ValueParser<pag::Enum> Parsers::GradientOverlayType = GradientOverlayTypeParser;
ValueParser<pag::GradientColorHandle> Parsers::OuterGlowGradientColor = OuterGlowGradientColorParser;


#define IMPLEMENT_ENUM_PARSER(name) \
static pag::Enum name##Parser(pagexporter::Context* context, AEGP_StreamVal2 streamValue) { \
    return Export##name(static_cast<int>(streamValue.one_d)); \
} \
ValueParser<pag::Enum> Parsers::name = name##Parser

IMPLEMENT_ENUM_PARSER(PolyStarType);

IMPLEMENT_ENUM_PARSER(CompositeOrder);

IMPLEMENT_ENUM_PARSER(FillRule);

IMPLEMENT_ENUM_PARSER(TrimType);

IMPLEMENT_ENUM_PARSER(LineCap);

IMPLEMENT_ENUM_PARSER(RepeaterOrder);

IMPLEMENT_ENUM_PARSER(LineJoin);

IMPLEMENT_ENUM_PARSER(GradientFillType);

IMPLEMENT_ENUM_PARSER(MergePathsMode);

IMPLEMENT_ENUM_PARSER(AnchorPointGrouping);

IMPLEMENT_ENUM_PARSER(ScaleMode);

IMPLEMENT_ENUM_PARSER(BlurDimensionsDirection);

IMPLEMENT_ENUM_PARSER(DisplacementMapSource);

IMPLEMENT_ENUM_PARSER(DisplacementBehavior);

IMPLEMENT_ENUM_PARSER(RadialBlurMode);

IMPLEMENT_ENUM_PARSER(RadialBlurAntialias);

IMPLEMENT_ENUM_PARSER(TextAnimatorTrackingType);

IMPLEMENT_ENUM_PARSER(TextRangeSelectorUnits);

IMPLEMENT_ENUM_PARSER(TextRangeSelectorBasedOn);

IMPLEMENT_ENUM_PARSER(TextRangeSelectorMode);

IMPLEMENT_ENUM_PARSER(TextRangeSelectorShape);

IMPLEMENT_ENUM_PARSER(ChannelControlType);

IMPLEMENT_ENUM_PARSER(IrisShapeType);

IMPLEMENT_ENUM_PARSER(StrokePosition);

IMPLEMENT_ENUM_PARSER(GlowColorType);

IMPLEMENT_ENUM_PARSER(GlowTechniqueType);
#undef IMPLEMENT_ENUM_PARSER
