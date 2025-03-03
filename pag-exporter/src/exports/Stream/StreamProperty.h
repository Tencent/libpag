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
#ifndef STREAMPROPERTY_H
#define STREAMPROPERTY_H
#include <AE_GeneralPlug.h>
#include <A.h>
#include "StreamValue.h"
#include "StreamFlags.h"

std::vector<float> ExportAverageSpeed(pagexporter::Context* context, AEGP_StreamRefH stream, int index, int dimensionality);
void CheckStreamExpression(pagexporter::Context* context, AEGP_StreamRefH stream);

template<typename T>
pag::Keyframe<T>* ExportKeyframe(pagexporter::Context* context, AEGP_StreamRefH stream, ValueParser<T> parser,
                                 int index, int dimensionality) {
  auto& suites = context->suites;
  auto keyframe = new pag::Keyframe<T>();

  A_Time time = {};
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeTime(stream, index - 1, AEGP_LTimeMode_CompTime, &time));
  keyframe->startTime = ExportTime(time, context);
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeTime(stream, index, AEGP_LTimeMode_CompTime, &time));
  keyframe->endTime = ExportTime(time, context);
  AEGP_StreamType streamType;
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_GetStreamType(stream, &streamType));
  bool hasSpatial = (streamType == AEGP_StreamType_ThreeD_SPATIAL || streamType == AEGP_StreamType_TwoD_SPATIAL);
  AEGP_StreamValue2 streamValue = {};
  if (streamType != AEGP_StreamType_NO_DATA) {
    RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(context->pluginID, stream, index - 1, &streamValue));
  }
  context->keyframeIndex = index - 1;
  keyframe->startValue = parser(context, streamValue.val);
  if (streamType != AEGP_StreamType_NO_DATA) {
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStreamValue(&streamValue));
    RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(context->pluginID, stream, index, &streamValue));
  }
  context->keyframeIndex = index;
  keyframe->endValue = parser(context, streamValue.val);
  if (streamType != AEGP_StreamType_NO_DATA) {
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStreamValue(&streamValue));
  }

  if (hasSpatial) {
    AEGP_StreamValue2 spatialOut = {};
    AEGP_StreamValue2 spatialIn = {};
    RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(context->pluginID, stream, index - 1,
                                                                             &spatialIn, &spatialOut));
    keyframe->spatialOut.x = static_cast<float>(spatialOut.val.three_d.x);
    keyframe->spatialOut.y = static_cast<float>(spatialOut.val.three_d.y);
    if (streamType == AEGP_StreamType_ThreeD_SPATIAL) {
      keyframe->spatialOut.z = static_cast<float>(spatialOut.val.three_d.z);
    } else {
      keyframe->spatialOut.z = 0.0f;
    }
    RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(context->pluginID, stream, index,
                                                                             &spatialIn, &spatialOut));
    keyframe->spatialIn.x = static_cast<float>(spatialIn.val.three_d.x);
    keyframe->spatialIn.y = static_cast<float>(spatialIn.val.three_d.y);
    if (streamType == AEGP_StreamType_ThreeD_SPATIAL) {
      keyframe->spatialIn.z = static_cast<float>(spatialIn.val.three_d.z);
    } else {
      keyframe->spatialIn.z = 0.0f;
    }
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStreamValue(&spatialIn));
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStreamValue(&spatialOut));
  }

  AEGP_KeyframeInterpolationType tempType;
  AEGP_KeyframeInterpolationType outType;
  AEGP_KeyframeInterpolationType inType;
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeInterpolation(stream, index - 1, &tempType, &outType));
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeInterpolation(stream, index, &inType, &tempType));

  AEGP_KeyframeFlags flags;
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeFlags(stream, index - 1, &flags));
  if (flags & AEGP_KeyframeFlag_ROVING) {
    outType = AEGP_KeyInterp_BEZIER;
  }

  if (outType == AEGP_KeyInterp_HOLD) {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Hold;
  } else if (outType == AEGP_KeyInterp_LINEAR && inType == AEGP_KeyInterp_LINEAR) {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Linear;
  } else {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Bezier;
    auto averageSpeedList = ExportAverageSpeed(context, stream, index, dimensionality);
    for (int i = 0; i < dimensionality; i++) {
      AEGP_KeyframeEase tempEase = {};
      AEGP_KeyframeEase easeOut = {};
      AEGP_KeyframeEase easeIn = {};
      RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeTemporalEase(stream, index - 1, i, &tempEase, &easeOut));
      RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetKeyframeTemporalEase(stream, index, i, &easeIn, &tempEase));
      auto averageSpeed = i < averageSpeedList.size() ? averageSpeedList[i] : averageSpeedList[0];
      if (averageSpeed > 0) {
        if (hasSpatial) {
          // Calculate the real influence value.
          easeOut.influenceF = std::min(averageSpeed / easeOut.speedF, easeOut.influenceF);
          easeIn.influenceF = std::min(averageSpeed / easeIn.speedF, easeIn.influenceF);
        } else if (streamType == AEGP_StreamType_MASK || streamType == AEGP_StreamType_NO_DATA) {
          easeOut.influenceF = std::min(1 / easeOut.speedF, easeOut.influenceF);
          easeIn.influenceF = std::min(1 / easeIn.speedF, easeIn.influenceF);
        }
      }
      pag::Point bezierOut = {};
      pag::Point bezierIn = {};
      bezierOut.x = static_cast<float>(easeOut.influenceF);
      bezierIn.x = 1 - static_cast<float>(easeIn.influenceF);
      if (fabs(averageSpeed) <= std::numeric_limits<float>::epsilon()) {
        bezierOut.y = bezierOut.x;
        bezierIn.y = bezierIn.x;
      } else {
        bezierOut.y = static_cast<float>(easeOut.speedF / averageSpeed * easeOut.influenceF);
        bezierIn.y = 1 - static_cast<float>(easeIn.speedF / averageSpeed * easeIn.influenceF);
      }
      keyframe->bezierOut.push_back(bezierOut);
      keyframe->bezierIn.push_back(bezierIn);
    }
  }
  return keyframe;
}

template<typename T>
pag::Property<T>* ExportAnimatableProperty(pagexporter::Context* context, AEGP_StreamRefH stream,
                                           ValueParser<T> parser, uint8_t dimensionality) {
  auto& suites = context->suites;
  A_long keyframeNum = 0;
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(stream, &keyframeNum));
  std::vector<pag::Keyframe<T>*> keyframes;
  context->keyframeNum = keyframeNum;
  for (int i = 1; i < keyframeNum; i++) {
    auto keyframe = ExportKeyframe(context, stream, parser, i, dimensionality);
    keyframes.push_back(keyframe);
  }
  context->keyframeNum = 0;
  context->keyframeIndex = 0;
  auto property = new pag::AnimatableProperty<T>(keyframes);
  return property;
}

template<typename T>
pag::Property<T>* ExportProperty(pagexporter::Context* context, AEGP_StreamRefH stream,
                                 ValueParser<T> parser, uint8_t dimensionality = 1) {
  auto& suites = context->suites;
  if (stream == nullptr) {
    return nullptr;
  }
  CheckStreamExpression(context, stream);
  pag::Property<T>* property;
  A_long numKeys = 0;
  RECORD_ERROR(suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(stream, &numKeys));
  if (numKeys <= 1) {
    property = new pag::Property<T>();
    property->value = ExportValue(context, stream, parser);
  } else {
    return ExportAnimatableProperty(context, stream, parser, dimensionality);
  }
  return property;
}

template<typename T>
inline pag::Property<T>* ExportProperty(pagexporter::Context* context, AEGP_StreamRefH groupStream, int index,
                                        ValueParser<T> parser, uint8_t dimensionality = 1) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(context->suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(context->pluginID, groupStream, index,
                                                                                  &stream));
  auto result = ExportProperty(context, stream, parser, dimensionality);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

template<typename T>
inline pag::Property<T>* ExportProperty(pagexporter::Context* context, AEGP_StreamRefH groupStream, const A_char* key,
                                        ValueParser<T> parser, uint8_t dimensionality = 1) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(context->suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(context->pluginID, groupStream,
                                                                                      key, &stream));
  auto result = ExportProperty(context, stream, parser, dimensionality);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

template<typename T>
inline pag::Property<T>* ExportProperty(pagexporter::Context* context, AEGP_LayerH layerHandle, AEGP_LayerStream layerStream,
                                        ValueParser<T> parser, uint8_t dimensionality = 1) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(
      context->suites.StreamSuite4()->AEGP_GetNewLayerStream(context->pluginID, layerHandle, layerStream, &stream));
  auto result = ExportProperty(context, stream, parser, dimensionality);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

template<typename T>
inline pag::Property<T>* ExportProperty(pagexporter::Context* context, const AEGP_MaskRefH& maskHandle, AEGP_MaskStream maskStream,
                                        ValueParser<T> parser, uint8_t dimensionality = 1) {
  AEGP_StreamRefH stream;
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_GetNewMaskStream(context->pluginID, maskHandle, maskStream,
                                                                     &stream));
  auto result = ExportProperty(context, stream, parser, dimensionality);
  RECORD_ERROR(context->suites.StreamSuite4()->AEGP_DisposeStream(stream));
  return result;
}

#endif //STREAMPROPERTY_H
