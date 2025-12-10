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

#pragma once

#include <pag/file.h>
#include <QVariantMap>
#include <functional>
#include "src/utils/AEHelper.h"

namespace exporter {

template <typename T>
using StreamParser = std::function<T(const AEGP_StreamVal2&, const QVariantMap&)>;

class AEStreamParser {
 public:
  static StreamParser<pag::Frame> TimeParser;
  static StreamParser<float> FloatParser;
  static StreamParser<float> FloatM255Parser;
  static StreamParser<float> PercentParser;
  static StreamParser<float> PercentD255Parser;
  static StreamParser<bool> BooleanParser;
  static StreamParser<pag::ID> LayerIDParser;
  static StreamParser<pag::ID> MaskIDParser;
  static StreamParser<uint8_t> Uint8Parser;
  static StreamParser<uint16_t> Uint16Parser;
  static StreamParser<int32_t> Int32Parser;
  static StreamParser<pag::Opacity> Opacity0_100Parser;
  static StreamParser<pag::Opacity> Opacity0_255Parser;
  static StreamParser<pag::Opacity> Opacity0_1Parser;
  static StreamParser<pag::Point> PointParser;
  static StreamParser<pag::Point3D> Point3DParser;
  static StreamParser<pag::Point> ScaleParser;
  static StreamParser<pag::Point3D> Scale3DParser;
  static StreamParser<pag::Color> ColorParser;
  static StreamParser<pag::PathHandle> PathParser;
  static StreamParser<pag::TextDocumentHandle> TextDocumentParser;
  static StreamParser<pag::GradientColorHandle> GradientColorParser;
  static StreamParser<pag::GradientColorHandle> GradientOverlayColorParser;
  static StreamParser<bool> ShapeDirectionReversedParser;
  static StreamParser<pag::BlendMode> ShapeBlendModeParser;
  static StreamParser<pag::BlendMode> StyleBlendModeParser;
  static StreamParser<pag::GradientFillType> GradientOverlayTypeParser;
  static StreamParser<pag::GradientColorHandle> OuterGlowGradientColorParser;
  static StreamParser<pag::StrokePosition> StrokePositionParser;
  static StreamParser<pag::PolyStarType> PolyStarTypeParser;
  static StreamParser<pag::CompositeOrder> CompositeOrderParser;
  static StreamParser<pag::FillRule> FillRuleParser;
  static StreamParser<pag::TrimPathsType> TrimPathsTypeParser;
  static StreamParser<pag::LineCap> LineCapParser;
  static StreamParser<pag::RepeaterOrder> RepeaterOrderParser;
  static StreamParser<pag::LineJoin> LineJoinParser;
  static StreamParser<pag::GradientFillType> GradientFillTypeParser;
  static StreamParser<pag::MergePathsMode> MergePathsModeParser;
  static StreamParser<pag::AnchorPointGrouping> AnchorPointGroupingParser;
  static StreamParser<pag::PAGScaleMode> ScaleModeParser;
  static StreamParser<pag::BlurDimensionsDirection> BlurDimensionsDirectionParser;
  static StreamParser<pag::DisplacementMapSource> DisplacementMapSourceParser;
  static StreamParser<pag::DisplacementMapBehavior> DisplacementMapBehaviorParser;
  static StreamParser<pag::RadialBlurMode> RadialBlurModeParser;
  static StreamParser<pag::RadialBlurAntialias> RadialBlurAntialiasParser;
  static StreamParser<pag::TextAnimatorTrackingType> TextAnimatorTrackingTypeParser;
  static StreamParser<pag::TextRangeSelectorUnits> TextRangeSelectorUnitsParser;
  static StreamParser<pag::TextRangeSelectorShape> TextRangeSelectorShapeParser;
  static StreamParser<pag::TextSelectorBasedOn> TextSelectorBasedOnParser;
  static StreamParser<pag::TextSelectorMode> TextSelectorModeParser;
  static StreamParser<pag::ChannelControlType> ChannelControlTypeParser;
  static StreamParser<pag::IrisShapeType> IrisShapeTypeParser;
  static StreamParser<pag::GlowColorType> GlowColorTypeParser;
  static StreamParser<pag::GlowTechniqueType> GlowTechniqueTypeParser;
};

template <typename T>
T GetValue(AEGP_StreamRefH streamHandle, StreamParser<T> parser, const QVariantMap& map = {}) {
  if (streamHandle == nullptr) {
    return T{};
  }
  static A_Time time = {0, 100};
  AEGP_StreamType streamType = AEGP_StreamType_NO_DATA;
  A_Err err = GetSuites()->StreamSuite4()->AEGP_GetStreamType(streamHandle, &streamType);
  if (err != A_Err_NONE) {
    return T{};
  }
  AEGP_StreamValue2 streamValue = {};
  bool needDispose = false;
  if (streamType != AEGP_StreamType_NO_DATA) {
    err = GetSuites()->StreamSuite4()->AEGP_GetNewStreamValue(
        GetPluginID(), streamHandle, AEGP_LTimeMode_CompTime, &time, TRUE, &streamValue);
    needDispose = (err == A_Err_NONE);
  }
  auto value = parser(streamValue.val, map);
  if (needDispose) {
    GetSuites()->StreamSuite4()->AEGP_DisposeStreamValue(&streamValue);
  }

  return value;
}

template <typename T>
T GetValue(AEGP_StreamRefH groupStreamHandle, int index, StreamParser<T> parser,
           const QVariantMap& map = {}) {
  if (groupStreamHandle == nullptr) {
    return T{};
  }
  AEGP_StreamRefH streamHandle = nullptr;
  GetSuites()->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(GetPluginID(), groupStreamHandle,
                                                                  index, &streamHandle);
  auto result = GetValue(streamHandle, parser, map);
  GetSuites()->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

template <typename T>
T GetValue(AEGP_StreamRefH groupStreamHandle, const A_char* key, StreamParser<T> parser,
           const QVariantMap& map = {}) {
  if (groupStreamHandle == nullptr) {
    return T{};
  }
  AEGP_StreamRefH streamHandle = nullptr;
  GetSuites()->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(
      GetPluginID(), groupStreamHandle, key, &streamHandle);
  auto result = GetValue(streamHandle, parser, map);
  GetSuites()->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

}  // namespace exporter
