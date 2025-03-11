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
#include "PrintStream.h"
#include <iostream>
#include "src/utils/AEUtils.h"

static std::string GetStreamType(AEGP_StreamType streamType) {
  switch (streamType) {
    case AEGP_StreamType_NO_DATA:
      return "NO_DATA";
    case AEGP_StreamType_ThreeD_SPATIAL:
      return "ThreeD_SPATIAL";
    case AEGP_StreamType_ThreeD:
      return "ThreeD";
    case AEGP_StreamType_TwoD_SPATIAL:
      return "TwoD_SPATIAL";
    case AEGP_StreamType_TwoD:
      return "TwoD";
    case AEGP_StreamType_OneD:
      return "OneD";
    case AEGP_StreamType_COLOR:
      return "COLOR";
    case AEGP_StreamType_ARB:
      return "ARB";
    case AEGP_StreamType_MARKER:
      return "MARKER";
    case AEGP_StreamType_LAYER_ID:
      return "LAYER_ID";
    case AEGP_StreamType_MASK_ID:
      return "MASK_ID";
    case AEGP_StreamType_MASK:
      return "MASK";
    case AEGP_StreamType_TEXT_DOCUMENT:
      return "TEXT_DOCUMENT";
    default:
      return "";
  }
}

static std::string GetGroupType(AEGP_StreamGroupingType groupingType) {
  switch (groupingType) {
    case AEGP_StreamGroupingType_NONE:
      return "NONE";
    case AEGP_StreamGroupingType_LEAF:
      return "LEAF";
    case AEGP_StreamGroupingType_NAMED_GROUP:
      return "NAMED_GROUP";
    case AEGP_StreamGroupingType_INDEXED_GROUP:
      return "INDEXED_GROUP";
    default:
      return "";
  }
}

void PrintStream(const AEGP_StreamRefH& streamH, int indent) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  char matchName[200];
  AEGP_DynStreamFlags streamFlags;
  AEGP_StreamGroupingType groupingType;
  char name[200];
  AEGP_StreamType streamType;
  AEGP_StreamValue2 streamValue = {};
  A_long numKeyframes;

  suites.DynamicStreamSuite4()->AEGP_GetMatchName(streamH, matchName);
  suites.DynamicStreamSuite4()->AEGP_GetDynamicStreamFlags(streamH, &streamFlags);
  if (streamFlags & AEGP_DynStreamFlag_HIDDEN && strcmp(matchName, "ADBE Vector Blend Mode")) {
    return;
  }
  suites.DynamicStreamSuite4()->AEGP_GetStreamGroupingType(streamH, &groupingType);
  suites.StreamSuite3()->AEGP_GetStreamName(streamH, FALSE, name);
  suites.StreamSuite4()->AEGP_GetStreamType(streamH, &streamType);
  if (streamType != AEGP_StreamType_NO_DATA) {
    A_Time time = {0, 100};
    suites.StreamSuite4()->AEGP_GetNewStreamValue(pluginID, streamH,
                                                  AEGP_LTimeMode_CompTime, &time,
                                                  TRUE, &streamValue);
  }
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(streamH, &numKeyframes);
  char indentString[100];
  int index = 0;
  while (index < indent) {
    indentString[index] = ' ';
    index++;
  }
  indentString[index] = '\0';
  std::cout << indentString << matchName;
  std::cout << " [" << name << " | " << GetGroupType(groupingType) << " | " << GetStreamType(streamType);
  if (streamType == AEGP_StreamType_OneD) {
    std::cout << " | " << streamValue.val.one_d;
  } else if (streamType == AEGP_StreamType_TwoD) {
    std::cout << " | " << streamValue.val.two_d.x << "," << streamValue.val.two_d.y;
  } else if (streamType == AEGP_StreamType_ThreeD) {
    std::cout << " | " << streamValue.val.three_d.x << "," << streamValue.val.three_d.y << ","
              << streamValue.val.three_d.z;
  }
  std::cout << "]";
  std::cout << std::endl;
  switch (groupingType) {
    case AEGP_StreamGroupingType_LEAF:
      break;
    case AEGP_StreamGroupingType_NAMED_GROUP:
    case AEGP_StreamGroupingType_INDEXED_GROUP:
      A_long numStreams;
      suites.DynamicStreamSuite4()->AEGP_GetNumStreamsInGroup(streamH, &numStreams);
      for (int i = 0; i < numStreams; i++) {
        AEGP_StreamRefH childStreamH;
        suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(pluginID,
                                                                  streamH, i,
                                                                  &childStreamH);
        PrintStream(childStreamH, indent + 1);
        suites.StreamSuite4()->AEGP_DisposeStream(childStreamH);
      }
      break;
    default:

      break;
  }
  if (streamType != AEGP_StreamType_NO_DATA) {
    suites.StreamSuite4()->AEGP_DisposeStreamValue(&streamValue);
  }
}
