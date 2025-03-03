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
#ifndef STREAMVALUE_H
#define STREAMVALUE_H
#include <functional>
#include "src/exports/PAGDataTypes.h"

template<typename T>
using ValueParser = std::function<T(pagexporter::Context*, AEGP_StreamVal2)>;

#define DEFINE_ENUM_PARSER(name) static ValueParser<pag::Enum> name

class Parsers {
 public:
  static ValueParser<pag::Frame> Time;
  static ValueParser<float> Float;
  static ValueParser<float> FloatM255;
  static ValueParser<float> Percent;
  static ValueParser<float> PercentD255;
  static ValueParser<bool> Boolean;
  static ValueParser<pag::ID> ID;
  static ValueParser<pag::Layer*> LayerID;
  static ValueParser<pag::MaskData*> MaskID;
  static ValueParser<uint8_t> Uint8;
  static ValueParser<uint16_t> Uint16;
  static ValueParser<int32_t> Int32;
  static ValueParser<pag::Opacity> Opacity0_100;
  static ValueParser<pag::Opacity> Opacity0_255;
  static ValueParser<pag::Opacity> Opacity0_1;
  static ValueParser<pag::Point> Point;
  static ValueParser<pag::Point3D> Point3D;
  static ValueParser<pag::Point> Scale;
  static ValueParser<pag::Point3D> Scale3D;
  static ValueParser<pag::Color> Color;
  static ValueParser<pag::PathHandle> Path;
  static ValueParser<pag::TextDocumentHandle> TextDocument;
  static ValueParser<pag::GradientColorHandle> GradientColor;
  static ValueParser<pag::GradientColorHandle> GradientOverlayColor;
  static ValueParser<bool> ShapeDirection;
  static ValueParser<pag::Enum> ShapeBlendMode;
  static ValueParser<pag::Enum> StyleBlendMode;
  static ValueParser<pag::Enum> GradientOverlayType;
  static ValueParser<pag::GradientColorHandle> OuterGlowGradientColor;

  DEFINE_ENUM_PARSER(StrokePosition);
  DEFINE_ENUM_PARSER(ColorClipMode);
  DEFINE_ENUM_PARSER(PolyStarType);
  DEFINE_ENUM_PARSER(CompositeOrder);
  DEFINE_ENUM_PARSER(FillRule);
  DEFINE_ENUM_PARSER(TrimType);
  DEFINE_ENUM_PARSER(LineCap);
  DEFINE_ENUM_PARSER(RepeaterOrder);
  DEFINE_ENUM_PARSER(LineJoin);
  DEFINE_ENUM_PARSER(GradientFillType);
  DEFINE_ENUM_PARSER(MergePathsMode);
  DEFINE_ENUM_PARSER(AnchorPointGrouping);
  DEFINE_ENUM_PARSER(ScaleMode);
  DEFINE_ENUM_PARSER(BlurDimensionsDirection);
  DEFINE_ENUM_PARSER(DisplacementMapSource);
  DEFINE_ENUM_PARSER(DisplacementBehavior);
  DEFINE_ENUM_PARSER(RadialBlurMode);
  DEFINE_ENUM_PARSER(RadialBlurAntialias);
  DEFINE_ENUM_PARSER(TextAnimatorTrackingType);
  DEFINE_ENUM_PARSER(TextRangeSelectorUnits);
  DEFINE_ENUM_PARSER(TextRangeSelectorBasedOn);
  DEFINE_ENUM_PARSER(TextRangeSelectorMode);
  DEFINE_ENUM_PARSER(TextRangeSelectorShape);
  DEFINE_ENUM_PARSER(ChannelControlType);
  DEFINE_ENUM_PARSER(IrisShapeType);
  DEFINE_ENUM_PARSER(GlowColorType);
  DEFINE_ENUM_PARSER(GlowTechniqueType);
};

#undef DEFINE_ENUM_PARSER

template<typename T>
T ExportValue(pagexporter::Context* context, AEGP_StreamRefH stream, ValueParser<T> parser) {
  static A_Time time = {0, 100};
  AEGP_StreamType streamType;
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_GetStreamType(stream, &streamType));
  AEGP_StreamValue2 streamValue = {};
  if (streamType != AEGP_StreamType_NO_DATA) {
    RECORD_ERROR(context->suites.StreamSuite4()->AEGP_GetNewStreamValue(context->pluginID, stream,
                                                                        AEGP_LTimeMode_CompTime, &time,
                                                                        TRUE, &streamValue));
  }
  context->keyframeIndex = 0;
  auto value = parser(context, streamValue.val);
  if (streamType != AEGP_StreamType_NO_DATA) {
    RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStreamValue(&streamValue));
  }
  return value;
}

template<typename T>
inline T ExportValue(pagexporter::Context* context, AEGP_StreamRefH groupStream, int index, ValueParser<T> parser) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(context->suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, groupStream, index,
                                                                                  &stream));
  auto result = ExportValue(context, stream, parser);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

template<typename T>
inline T ExportValue(pagexporter::Context* context, AEGP_StreamRefH groupStream,
                     const A_char* key, ValueParser<T> parser) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(context->suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, groupStream,
                                                                                      key, &stream));
  auto result = ExportValue(context, stream, parser);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

#endif //STREAMVALUE_H
