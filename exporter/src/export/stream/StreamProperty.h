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

#include <AE_GeneralPlug.h>
#include <pag/file.h>
#include "StreamValue.h"
#include "utils/AEDataTypeConverter.h"
#include "utils/AEHelper.h"
#include "utils/PAGExportSession.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

std::vector<float> GetAverageSpeed(AEGP_StreamRefH streamHandle, int index, int dimensionality);

void CheckStreamExpression(AEGP_StreamRefH stream);

template <typename T>
pag::Keyframe<T>* GetKeyframe(AEGP_StreamRefH streamHandle, StreamParser<T> parser, int index,
                              const QVariantMap& map, int dimensionality) {
  const auto& PluginID = GetPluginID();
  const auto& Suites = GetSuites();
  float frameRate = PAGExportSessionManager::GetInstance()->getCurrentCompositionFrameRate();

  auto keyframe = new pag::Keyframe<T>();
  A_Time time = {};
  Suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamHandle, index - 1, AEGP_LTimeMode_CompTime,
                                                 &time);
  keyframe->startTime = AEDurationToFrame(time, frameRate);
  Suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamHandle, index, AEGP_LTimeMode_CompTime,
                                                 &time);
  keyframe->endTime = AEDurationToFrame(time, frameRate);
  AEGP_StreamType streamType;
  Suites->StreamSuite4()->AEGP_GetStreamType(streamHandle, &streamType);
  bool hasSpatial =
      (streamType == AEGP_StreamType_ThreeD_SPATIAL || streamType == AEGP_StreamType_TwoD_SPATIAL);
  AEGP_StreamValue2 streamValue = {};
  if (streamType != AEGP_StreamType_NO_DATA) {
    Suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(PluginID, streamHandle, index - 1,
                                                       &streamValue);
  }
  QVariantMap newMap = map;
  newMap["keyFrameIndex"] = index - 1;
  keyframe->startValue = parser(streamValue.val, newMap);
  if (streamType != AEGP_StreamType_NO_DATA) {
    Suites->StreamSuite4()->AEGP_DisposeStreamValue(&streamValue);
    Suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(PluginID, streamHandle, index, &streamValue);
  }
  newMap["keyFrameIndex"] = index;
  keyframe->endValue = parser(streamValue.val, newMap);
  if (streamType != AEGP_StreamType_NO_DATA) {
    Suites->StreamSuite4()->AEGP_DisposeStreamValue(&streamValue);
  }

  if (hasSpatial) {
    AEGP_StreamValue2 spatialOut = {};
    AEGP_StreamValue2 spatialIn = {};
    Suites->KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(PluginID, streamHandle, index - 1,
                                                                 &spatialIn, &spatialOut);
    keyframe->spatialOut.x = static_cast<float>(spatialOut.val.three_d.x);
    keyframe->spatialOut.y = static_cast<float>(spatialOut.val.three_d.y);
    if (streamType == AEGP_StreamType_ThreeD_SPATIAL) {
      keyframe->spatialOut.z = static_cast<float>(spatialOut.val.three_d.z);
    } else {
      keyframe->spatialOut.z = 0.0f;
    }
    Suites->KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(PluginID, streamHandle, index,
                                                                 &spatialIn, &spatialOut);
    keyframe->spatialIn.x = static_cast<float>(spatialIn.val.three_d.x);
    keyframe->spatialIn.y = static_cast<float>(spatialIn.val.three_d.y);
    if (streamType == AEGP_StreamType_ThreeD_SPATIAL) {
      keyframe->spatialIn.z = static_cast<float>(spatialIn.val.three_d.z);
    } else {
      keyframe->spatialIn.z = 0.0f;
    }
    Suites->StreamSuite4()->AEGP_DisposeStreamValue(&spatialIn);
    Suites->StreamSuite4()->AEGP_DisposeStreamValue(&spatialOut);
  }

  AEGP_KeyframeInterpolationType tempType;
  AEGP_KeyframeInterpolationType outType;
  AEGP_KeyframeInterpolationType inType;
  Suites->KeyframeSuite4()->AEGP_GetKeyframeInterpolation(streamHandle, index - 1, &tempType,
                                                          &outType);
  Suites->KeyframeSuite4()->AEGP_GetKeyframeInterpolation(streamHandle, index, &inType, &tempType);

  AEGP_KeyframeFlags flags;
  Suites->KeyframeSuite4()->AEGP_GetKeyframeFlags(streamHandle, index - 1, &flags);
  if (flags & AEGP_KeyframeFlag_ROVING) {
    outType = AEGP_KeyInterp_BEZIER;
  }

  if (outType == AEGP_KeyInterp_HOLD) {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Hold;
  } else if (outType == AEGP_KeyInterp_LINEAR && inType == AEGP_KeyInterp_LINEAR) {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Linear;
  } else {
    keyframe->interpolationType = pag::KeyframeInterpolationType::Bezier;
    auto averageSpeedList = GetAverageSpeed(streamHandle, index, dimensionality);
    for (int i = 0; i < dimensionality; i++) {
      AEGP_KeyframeEase tempEase = {};
      AEGP_KeyframeEase easeOut = {};
      AEGP_KeyframeEase easeIn = {};
      Suites->KeyframeSuite4()->AEGP_GetKeyframeTemporalEase(streamHandle, index - 1, i, &tempEase,
                                                             &easeOut);
      Suites->KeyframeSuite4()->AEGP_GetKeyframeTemporalEase(streamHandle, index, i, &easeIn,
                                                             &tempEase);
      float averageSpeed =
          i < static_cast<int>(averageSpeedList.size()) ? averageSpeedList[i] : averageSpeedList[0];
      if (averageSpeed > 0) {
        if (hasSpatial) {
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

template <typename T>
pag::Property<T>* GetAnimatableProperty(AEGP_StreamRefH streamHandle, StreamParser<T> parser,
                                        const QVariantMap& map, uint8_t dimensionality) {
  A_long keyframeNum = 0;
  GetSuites()->KeyframeSuite4()->AEGP_GetStreamNumKFs(streamHandle, &keyframeNum);
  std::vector<pag::Keyframe<T>*> keyframes;
  for (int i = 1; i < keyframeNum; i++) {
    auto keyframe = GetKeyframe(streamHandle, parser, i, map, dimensionality);
    keyframes.push_back(keyframe);
  }
  auto property = new pag::AnimatableProperty<T>(keyframes);
  return property;
}

template <typename T>
pag::Property<T>* GetProperty(AEGP_StreamRefH streamHandle, StreamParser<T> parser,
                              const QVariantMap& map = {}, uint8_t dimensionality = 1) {
  if (streamHandle == nullptr) {
    return nullptr;
  }
  CheckStreamExpression(streamHandle);
  pag::Property<T>* property;
  A_long numKeys = 0;
  GetSuites()->KeyframeSuite4()->AEGP_GetStreamNumKFs(streamHandle, &numKeys);
  if (numKeys <= 1) {
    property = new pag::Property<T>();
    property->value = GetValue(streamHandle, parser, map);
  } else {
    return GetAnimatableProperty(streamHandle, parser, map, dimensionality);
  }
  return property;
}

template <typename T>
pag::Property<T>* GetProperty(AEGP_StreamRefH groupStreamHandle, int index, StreamParser<T> parser,
                              const QVariantMap& map = {}, uint8_t dimensionality = 1) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(PluginID, groupStreamHandle, index,
                                                             &streamHandle);
  auto result = GetProperty(streamHandle, parser, map, dimensionality);
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

template <typename T>
pag::Property<T>* GetProperty(AEGP_StreamRefH groupStreamHandle, const A_char* key,
                              StreamParser<T> parser, const QVariantMap& map = {},
                              uint8_t dimensionality = 1) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefByMatchname(PluginID, groupStreamHandle, key,
                                                                 &streamHandle);
  auto result = GetProperty(streamHandle, parser, map, dimensionality);
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

template <typename T>
pag::Property<T>* GetProperty(const AEGP_LayerH& layerHandle, AEGP_LayerStream layerStream,
                              StreamParser<T> parser, const QVariantMap& map = {},
                              uint8_t dimensionality = 1) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewLayerStream(PluginID, layerHandle, layerStream, &streamHandle);
  auto result = GetProperty(streamHandle, parser, map, dimensionality);
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

template <typename T>
pag::Property<T>* GetProperty(const AEGP_MaskRefH& maskHandle, AEGP_MaskStream maskStream,
                              StreamParser<T> parser, const QVariantMap& map = {},
                              uint8_t dimensionality = 1) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewMaskStream(PluginID, maskHandle, maskStream, &streamHandle);
  auto result = GetProperty(streamHandle, parser, map, dimensionality);
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  return result;
}

}  // namespace exporter
