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
#include "AEMarker.h"
#include <codecvt>
#include <unordered_map>
#include <unordered_set>
#include "AEUtils.h"
#include "StringUtil.h"
#include "cJSON.h"
// #include "PlaceImagePanel.h"
// #include "PAGFilterUtil.h"

void AEMarker::ExportMarkers(pagexporter::Context* context, const AEGP_LayerH& layerHandle,
                             std::vector<pag::Marker*>& markers) {
  if (context == nullptr || layerHandle == nullptr) {
    return;
  }
  auto& suites = context->suites;
  auto pluginID = context->pluginID;

  AEGP_StreamRefH streamH = AEUtils::GetMarkerStreamFromLayer(layerHandle);

  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(streamH, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; index++) {
    auto marker = new pag::Marker();

    AEGP_StreamValue2 streamValue;
    suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, streamH, index, &streamValue);
    AEGP_MarkerValP markerP = streamValue.val.markerP;
    AEGP_MemHandle unicodeH;
    suites.MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                &unicodeH);
    std::string comment = StringUtil::ToString(suites, unicodeH);
    suites.MemorySuite1()->AEGP_FreeMemHandle(unicodeH);

    if (comment.empty()) {
      delete marker;
      continue;
    }

    A_Time time;
    suites.KeyframeSuite4()->AEGP_GetKeyframeTime(streamH, index, AEGP_LTimeMode_CompTime, &time);

    A_Time duration;
    suites.MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);
    suites.StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

    marker->comment = comment;
    marker->startTime = context->frameRate * time.value / time.scale;
    marker->duration = context->frameRate * duration.value / duration.scale;

    markers.push_back(marker);
  }
  AEUtils::DeleteStream(streamH);
}

void AEMarker::PrintMarkers(pag::Composition* composition) {
  if (composition == nullptr) {
    return;
  }
  if (composition->type() == pag::CompositionType::Bitmap ||
      composition->type() == pag::CompositionType::Video) {
    return;
  }

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {

    printf("layer->markers=%ld\n", layer->markers.size());

    for (auto marker : layer->markers) {
      printf("marker time=%.0f duration=%.0f marker=%s\n", (double)marker->startTime,
             (double)marker->duration, marker->comment.c_str());
    }

    if (layer->type() == pag::LayerType::PreCompose) {
      PrintMarkers(static_cast<pag::PreComposeLayer*>(layer)->composition);
    }
  }
}

void AEMarker::ParseMarkers(pag::Layer* layer) {
  if (layer == nullptr) {
    return;
  }
  for (auto marker : layer->markers) {
    auto comment = &marker->comment;

    auto cjson = cJSON_Parse(comment->c_str());
    if (cjson == nullptr || cjson->type != cJSON_Object) {
      continue;
    }

    // 暂时只处理CachePolicy
    auto item = cJSON_GetObjectItem(cjson, "CachePolicy");
    if (item != nullptr) {
      if (item->type == cJSON_Int) {  // 数字类型：0 = auto; 1 = enable; 2 = disable
        if (item->valueint == 1) {
          layer->cachePolicy = pag::CachePolicy::Enable;
        } else if (item->valueint == 2) {
          layer->cachePolicy = pag::CachePolicy::Disable;
        }
        break;                                                                  // 找到一个就结束
      } else if (item->type == cJSON_String && item->valuestring != nullptr) {  // 文字类型
        if (!strcmp(item->valuestring, "enable")) {
          layer->cachePolicy = pag::CachePolicy::Enable;
        } else if (!strcmp(item->valuestring, "disable")) {
          layer->cachePolicy = pag::CachePolicy::Disable;
        }
        break;  // 找到一个就结束
      }
    }

    cJSON_Delete(cjson);
  }
}

struct {
  std::string name;
  int mode;
} TimeStretchModeTab[4] = {
    // 因为下面的检测方法不严谨，所以这里的顺序不能随意。
    {"repeatinverted", pag::PAGTimeStretchMode::RepeatInverted},
    {"repeat", pag::PAGTimeStretchMode::Repeat},
    {"scale", pag::PAGTimeStretchMode::Scale},
    {"none", pag::PAGTimeStretchMode::None},
};

static std::string TimeStretchModeToString(pag::Enum mode) {
  switch (mode) {
    case pag::PAGTimeStretchMode::None:
      return "None";
      break;
    case pag::PAGTimeStretchMode::Scale:
      return "Scale";
      break;
    case pag::PAGTimeStretchMode::Repeat:
      return "Repeat";
      break;
    case pag::PAGTimeStretchMode::RepeatInverted:
      return "RepeatInverted";
      break;
    default:
      return "Repeat";
      break;
  }
}

static pag::Enum StringToTimeStretchMode(std::string str) {
  auto newStr = str;
  transform(newStr.begin(), newStr.end(), newStr.begin(), ::tolower);
  auto p = newStr.c_str();
  if (strcmp(p, "none") == 0) {
    return pag::PAGTimeStretchMode::None;
  } else if (strcmp(p, "scale") == 0) {
    return pag::PAGTimeStretchMode::Scale;
  } else if (strcmp(p, "repeat") == 0) {
    return pag::PAGTimeStretchMode::Repeat;
  } else if (strcmp(p, "repeatinverted") == 0) {
    return pag::PAGTimeStretchMode::RepeatInverted;
  } else {
    return pag::PAGTimeStretchMode::Repeat;
  }
}

static bool FindKeyFromComment(std::string comment, std::string key, cJSON** ppValue) {
  bool ret = false;
  if (ppValue != nullptr) {
    *ppValue = nullptr;
  }
  auto cjson = cJSON_Parse(comment.c_str());
  if (cjson != nullptr) {
    auto item = cJSON_GetObjectItem(cjson, key.c_str());
    if (item != nullptr) {
      ret = true;
      if (ppValue != nullptr) {
        if (item->type == cJSON_Int) {
          *ppValue = cJSON_CreateInt(item->valueint, item->sign);
        } else if (item->type == cJSON_String) {
          *ppValue = cJSON_CreateString(item->valuestring);
        }
        (*ppValue)->string = cJSON_strdup(item->string);
      }
    }
    cJSON_Delete(cjson);
  }
  return ret;
}

bool AEMarker::GetTimeStretchInfoOld(pag::Enum& timeStretchMode, pag::Frame& timeStretchStart,
                                     pag::Frame& timeStretchDuration, const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  bool find = false;

  timeStretchMode = pag::PAGTimeStretchMode::Repeat;
  timeStretchStart = 0;
  timeStretchDuration = 0;

  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);

  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    AEGP_StreamValue2 streamValue;
    suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index, &streamValue);

    AEGP_MarkerValP markerP = streamValue.val.markerP;

    AEGP_MemHandle unicodeH;
    suites.MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                &unicodeH);
    std::string comment = StringUtil::ToString(suites, unicodeH);
    suites.MemorySuite1()->AEGP_FreeMemHandle(unicodeH);

    transform(comment.begin(), comment.end(), comment.begin(), ::tolower);
    auto pos = comment.find("#timestretchmode");  // TimeStretchMode
    if (pos == std::string::npos) {
      continue;
    }

    for (int i = 0; i < sizeof(TimeStretchModeTab) / sizeof(TimeStretchModeTab[0]); i++) {
      pos = comment.find(TimeStretchModeTab[i].name);
      if (pos != std::string::npos) {
        timeStretchMode = TimeStretchModeTab[i].mode;
        find = true;
        break;
      }
    }
    if (!find) {
      continue;
    }

    A_Time time;
    suites.KeyframeSuite4()->AEGP_GetKeyframeTime(markerStreamH, index, AEGP_LTimeMode_CompTime,
                                                  &time);

    A_Time duration;
    suites.MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);

    float frameRate = AEUtils::GetFrameRateFromItem(itemH);
    timeStretchStart = ExportTime(time, frameRate);
    timeStretchDuration = ExportTime(duration, frameRate);
    break;
  }

  AEUtils::DeleteStream(markerStreamH);
  return find;
}

void AEMarker::DeleteTimeStretchMarkerOld(const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();

  std::vector<int> list;

  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);

  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = numKeyframes - 1; index >= 0; index--) {
    AEGP_StreamValue2 streamValue;
    suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index, &streamValue);

    AEGP_MarkerValP markerP = streamValue.val.markerP;

    AEGP_MemHandle unicodeH;
    suites.MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                &unicodeH);
    std::string comment = StringUtil::ToString(suites, unicodeH);
    suites.MemorySuite1()->AEGP_FreeMemHandle(unicodeH);

    transform(comment.begin(), comment.end(), comment.begin(), ::tolower);
    auto pos = comment.find("#timestretchmode");  // TimeStretchMode
    if (pos != std::string::npos) {
      suites.KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
    }
  }

  AEUtils::DeleteStream(markerStreamH);
}

bool AEMarker::GetTimeStretchInfo(pag::Enum& timeStretchMode, pag::Frame& timeStretchStart,
                                  pag::Frame& timeStretchDuration, const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  bool find = false;

  timeStretchMode = pag::PAGTimeStretchMode::Repeat;
  timeStretchStart = 0;
  timeStretchDuration = 0;

  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    auto comment = GetMarkerComment(markerStreamH, index);
    cJSON* cjson = nullptr;
    find = FindKeyFromComment(comment, "TimeStretchMode", &cjson);
    if (find) {
      timeStretchMode = StringToTimeStretchMode(cjson->valuestring);
      cJSON_Delete(cjson);

      A_Time time;
      suites.KeyframeSuite4()->AEGP_GetKeyframeTime(markerStreamH, index, AEGP_LTimeMode_CompTime,
                                                    &time);

      AEGP_StreamValue2 streamValue;
      suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index,
                                                        &streamValue);
      AEGP_MarkerValP markerP = streamValue.val.markerP;
      A_Time duration;
      suites.MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);

      float frameRate = AEUtils::GetFrameRateFromItem(itemH);
      timeStretchStart = ExportTime(time, frameRate);
      timeStretchDuration = ExportTime(duration, frameRate);
      break;
    }
  }

  AEUtils::DeleteStream(markerStreamH);
  return find;
}

void AEMarker::SetTimeStretchInfo(pag::Enum timeStretchMode, pag::Frame timeStretchStart,
                                  pag::Frame timeStretchDuration, const AEGP_ItemH& itemH) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);

  DeleteTimeStretchMarkerOld(itemH);
  DeleteMarkersFromStream(markerStreamH, "TimeStretchMode");

  auto modeString = TimeStretchModeToString(timeStretchMode);
  auto node = AEMarker::KeyValueToJsonNode("TimeStretchMode", modeString);
  A_Time keyTime;
  A_Time durationTime;
  float frameRate = AEUtils::GetFrameRateFromItem(itemH);
  keyTime.scale = static_cast<A_u_long>(frameRate);
  keyTime.value = static_cast<A_long>(timeStretchStart);
  durationTime.scale = static_cast<A_u_long>(frameRate);
  durationTime.value = static_cast<A_long>(timeStretchDuration);

  int mergedIndex = MergeMarkerToStream(markerStreamH, node);
  if (mergedIndex >= 0) {
    auto comment = GetMarkerComment(markerStreamH, mergedIndex);
    DeleteMarkerByIndex(markerStreamH, mergedIndex);
    AddNewMarkerToStream(markerStreamH, comment, keyTime, durationTime);
  } else {
    AddNewMarkerToStream(markerStreamH, node, keyTime, durationTime);
  }

  AEUtils::DeleteStream(markerStreamH);
}

void AEMarker::ExportTimeStretch(std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                 const AEGP_ItemH& itemH) {
  auto& suites = SUITES();

  pag::Enum timeStretchMode = pag::PAGTimeStretchMode::Repeat;
  pag::Frame timeStretchStart = 0;
  pag::Frame timeStretchDuration = 0;

  auto finded = GetTimeStretchInfo(timeStretchMode, timeStretchStart, timeStretchDuration, itemH);
  if (!finded) {
    finded = GetTimeStretchInfoOld(timeStretchMode, timeStretchStart, timeStretchDuration, itemH);
  }

  if (finded) {
    A_Time durationTime = {};
    suites.ItemSuite6()->AEGP_GetItemDuration(itemH, &durationTime);
    auto compositionDuration = ExportTime(durationTime, &context);

    timeStretchStart = std::min(std::max(timeStretchStart, (pag::Frame)0), compositionDuration);
    timeStretchDuration = std::min(std::max(timeStretchDuration, (pag::Frame)0),
                                   compositionDuration - timeStretchStart);
    if (timeStretchDuration == 0) {
      timeStretchStart = 0;
      timeStretchDuration = compositionDuration;
    }
  }

  file->scaledTimeRange.start = timeStretchStart;
  file->scaledTimeRange.end = timeStretchStart + timeStretchDuration;
  file->timeStretchMode = timeStretchMode;
}

static void ExportImageLayerEditable(std::shared_ptr<pag::File>& file,
                                     pagexporter::Context& context,
                                     const AEGP_ItemH& mainCompItemH) {
  // int count = static_cast<int>(file->images.size());
  // if (count == 0) {
  //   return;
  // }
  // auto list = new int[count];
  // for (int i = 0; i < count; i++) {
  //   list[i] = i;
  // }
  // bool hasNoPlace = false;
  // for(auto pair : context.imageLayerHList) {
  //   auto layerH = pair.first;
  //   auto isNoPlace = PlaceImageLayer::IsNoReplaceImageLayer(context, mainCompItemH, layerH);
  //   if (isNoPlace) {
  //     auto id = AEUtils::GetItemIdFromLayer(layerH);
  //     for (int index = 0; index < count; index++) {
  //       if (id == file->images[index]->id) {
  //         hasNoPlace = true;
  //         list[index] = -1;
  //       }
  //     }
  //   }
  // }
  // if (hasNoPlace) {
  //   file->editableImages = new std::vector<int>();
  //   for (int i = 0; i < count; i++) {
  //     if (list[i] != -1) {
  //       file->editableImages->push_back(list[i]);
  //     }
  //   }
  // }
  // delete []list;
}

static void ExportTextLayerEditable(std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                    const AEGP_ItemH& mainCompItemH) {
  int count = static_cast<int>(file->numTexts());
  if (count == 0) {
    return;
  }
  auto list = new int[count];
  for (int i = 0; i < count; i++) {
    list[i] = i;
  }
  bool hasNoPlace = false;
  for (int i = 0; i < count; i++) {
    auto layer = file->getTextAt(i);
    auto keyString = AEMarker::GetKeyStringWithId("noReplace", layer->id);
    auto isNoPlace = AEMarker::FindMarkerFromComposition(mainCompItemH, keyString);
    if (!isNoPlace) {
      auto layerH = context.getLayerHById(layer->id);
      if (layerH == nullptr) {
        continue;
      }
      isNoPlace = AEMarker::FindMarkerFromLayer(layerH, "noReplace");
    }
    if (isNoPlace) {
      hasNoPlace = true;
      list[i] = -1;
    }
  }
  if (hasNoPlace) {
    file->editableTexts = new std::vector<int>();
    for (int i = 0; i < count; i++) {
      if (list[i] != -1) {
        file->editableTexts->push_back(list[i]);
      }
    }
  }
  delete[] list;
}

static std::vector<int>* MakeDefaultEditableIndices(int count) {
  auto indices = new std::vector<int>();
  for (int i = 0; i < count; i++) {
    indices->push_back(i);
  }
  return indices;
}

void AEMarker::ExportLayerEditable(std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                   const AEGP_ItemH& itemH) {
  ExportImageLayerEditable(file, context, itemH);
  ExportTextLayerEditable(file, context, itemH);

  if (file->editableImages == nullptr && file->images.size() > 0 &&
      file->editableTexts != nullptr) {
    file->editableImages = MakeDefaultEditableIndices(static_cast<int>(file->images.size()));
  }
  if (file->editableTexts == nullptr && file->numTexts() > 0 && file->editableImages != nullptr) {
    file->editableTexts = MakeDefaultEditableIndices(file->numTexts());
  }
}

void AEMarker::AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, std::string comment,
                                    A_Time keyTime, A_Time durationTime) {
  if (markerStreamH == nullptr) {
    return;
  }
  auto& suites = SUITES();
  A_long index = 0;
  suites.KeyframeSuite4()->AEGP_InsertKeyframe(markerStreamH, AEGP_LTimeMode_LayerTime, &keyTime,
                                               &index);
  if (!comment.empty()) {
    SetMarkerComment(markerStreamH, index, comment, durationTime);
  }
}

void AEMarker::AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node,
                                    A_Time keyTime, A_Time durationTime) {
  if (markerStreamH == nullptr || node == nullptr) {
    return;
  }
  auto cjson = cJSON_CreateObject();
  cJSON_AddItemToArray(cjson, node);
  std::string comment = cJSON_Print(cjson);
  AddNewMarkerToStream(markerStreamH, comment, keyTime, durationTime);
  cJSON_Delete(cjson);
}

void AEMarker::DeleteMarkerByIndex(const AEGP_StreamRefH& markerStreamH, int index) {
  if (markerStreamH == nullptr) {
    return;
  }
  auto& suites = SUITES();
  suites.KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
}

void AEMarker::SetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index,
                                std::string comment, A_Time durationTime) {
  if (markerStreamH == nullptr) {
    return;
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_StreamValue2 streamValue;
  suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index, &streamValue);
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  auto fmtComment = convert.from_bytes(comment);  // utf-8 to utf-16
  AEGP_MarkerValP newMarkerP = nullptr;
  suites.MarkerSuite1()->AEGP_NewMarker(&newMarkerP);
  suites.MarkerSuite1()->AEGP_SetMarkerString(newMarkerP, AEGP_MarkerString_COMMENT,
                                              (A_u_short*)fmtComment.c_str(),
                                              (A_long)fmtComment.length());
  if (durationTime.value < 0 || durationTime.scale == 0) {
    auto markerP = streamValue.val.markerP;
    suites.MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &durationTime);
  }
  suites.MarkerSuite2()->AEGP_SetMarkerDuration(newMarkerP, &durationTime);

  streamValue.val.markerP = newMarkerP;
  suites.KeyframeSuite4()->AEGP_SetKeyframeValue(markerStreamH, index, &streamValue);
  suites.StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
}

std::string AEMarker::GetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index) {
  if (markerStreamH == nullptr) {
    return "";
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_StreamValue2 streamValue;
  suites.KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index, &streamValue);
  AEGP_MarkerValP markerP = streamValue.val.markerP;
  AEGP_MemHandle unicodeH;
  suites.MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                              &unicodeH);
  std::string comment = StringUtil::ToString(suites, unicodeH);
  suites.MemorySuite1()->AEGP_FreeMemHandle(unicodeH);
  suites.StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
  return comment;
}

bool AEMarker::MergeToMarkerWithKey(const AEGP_StreamRefH& markerStreamH, std::string& key,
                                    cJSON* node) {
  auto& suites = SUITES();
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    auto comment = GetMarkerComment(markerStreamH, index);
    auto cjson = cJSON_Parse(comment.c_str());
    if (cjson != nullptr) {
      auto item = cJSON_GetObjectItem(cjson, key.c_str());
      if (item != nullptr) {
        cJSON_AddItemToArray(cjson, node);
        auto text = cJSON_Print(cjson);
        SetMarkerComment(markerStreamH, index, text);
        delete text;
        cJSON_Delete(cjson);
        return true;
      }
      cJSON_Delete(cjson);
    }
  }
  return false;
}

int AEMarker::MergeMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node) {
  auto& suites = SUITES();
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    auto comment = GetMarkerComment(markerStreamH, index);
    auto cjson = cJSON_Parse(comment.c_str());
    if (cjson != nullptr && cjson->type == cJSON_Object) {
      cJSON_AddItemToArray(cjson, node);
      auto text = cJSON_Print(cjson);
      SetMarkerComment(markerStreamH, index, text);
      delete text;
      cJSON_Delete(cjson);
      return index;
    }
  }
  return -1;
}

bool AEMarker::FindMarkerFromStream(const AEGP_StreamRefH& markerStreamH, std::string key,
                                    cJSON** ppValue) {
  if (markerStreamH == nullptr) {
    return false;
  }
  bool ret = false;
  auto& suites = SUITES();
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    auto comment = GetMarkerComment(markerStreamH, index);
    ret = FindKeyFromComment(comment, key, ppValue);
    if (ret) {
      break;
    }
  }
  return ret;
}

bool AEMarker::FindMarkerFromLayer(const AEGP_LayerH& layerH, std::string key, cJSON** ppValue) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromLayer(layerH);
  auto ret = FindMarkerFromStream(markerStreamH, key, ppValue);
  AEUtils::DeleteStream(markerStreamH);
  return ret;
}

bool AEMarker::FindMarkerFromComposition(const AEGP_ItemH& itemH, std::string key,
                                         cJSON** ppValue) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);
  auto ret = FindMarkerFromStream(markerStreamH, key, ppValue);
  AEUtils::DeleteStream(markerStreamH);
  return ret;
}

void AEMarker::DeleteMarkersFromStream(const AEGP_StreamRefH& markerStreamH, std::string key) {
  if (markerStreamH == nullptr) {
    return;
  }
  auto& suites = SUITES();
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = numKeyframes - 1; index >= 0; index--) {
    auto comment = GetMarkerComment(markerStreamH, index);
    auto cjson = cJSON_Parse(comment.c_str());
    if (cjson != nullptr && cjson->type == cJSON_Object) {
      cJSON_DeleteItemFromObject(cjson, key.c_str());
      if (cjson->type == cJSON_Object && cjson->child == nullptr) {
        DeleteMarkerByIndex(markerStreamH, index);
      } else {
        auto newStr = cJSON_Print(cjson);
        SetMarkerComment(markerStreamH, index, newStr);
        free(newStr);
      }
      cJSON_Delete(cjson);
    }
  }
}

void AEMarker::DeleteMarkersFromLayer(const AEGP_LayerH& layerH, std::string key) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromLayer(layerH);
  DeleteMarkersFromStream(markerStreamH, key);
  AEUtils::DeleteStream(markerStreamH);
}

void AEMarker::DeleteMarkersFromCompostion(const AEGP_ItemH& itemH, std::string key) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);
  DeleteMarkersFromStream(markerStreamH, key);
  AEUtils::DeleteStream(markerStreamH);
}

static std::string GetValueFromComment(std::string comment, std::string key) {
  std::string ret = "";
  auto cjson = cJSON_Parse(comment.c_str());
  if (cjson != nullptr) {
    auto item = cJSON_GetObjectItem(cjson, key.c_str());
    if (item != nullptr) {
      if (item->type == cJSON_String && item->valuestring != nullptr) {
        ret = item->valuestring;
      } else if (item->type == cJSON_Int) {
        ret = std::to_string(item->valueint);
      }
    }
    cJSON_Delete(cjson);
  }
  return ret;
}

std::string AEMarker::GetMarkerFromCompostion(const AEGP_ItemH& itemH, std::string key) {
  if (itemH == nullptr) {
    return "";
  }
  auto& suites = SUITES();
  std::string value = "";
  AEGP_StreamRefH markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);
  A_long numKeyframes = 0;
  suites.KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);
  for (A_long index = 0; index < numKeyframes; index++) {
    auto comment = GetMarkerComment(markerStreamH, index);
    value = GetValueFromComment(comment, key);
    if (!value.empty()) {
      break;
    }
  }
  AEUtils::DeleteStream(markerStreamH);
  return value;
}

void AEMarker::AddMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node) {
  if (markerStreamH == nullptr || node == nullptr) {
    return;
  }
  DeleteMarkersFromStream(markerStreamH, node->string);
  int mergedIndex = MergeMarkerToStream(markerStreamH, node);
  if (mergedIndex < 0) {
    AddNewMarkerToStream(markerStreamH, node);
  }
}

void AEMarker::AddMarkerToLayer(const AEGP_LayerH& layerH, cJSON* node) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromLayer(layerH);
  AddMarkerToStream(markerStreamH, node);
  AEUtils::DeleteStream(markerStreamH);
}

void AEMarker::AddMarkerToCompostion(const AEGP_ItemH& itemH, cJSON* node) {
  auto markerStreamH = AEUtils::GetMarkerStreamFromItem(itemH);
  AddMarkerToStream(markerStreamH, node);
  AEUtils::DeleteStream(markerStreamH);
}

cJSON* AEMarker::KeyValueToJsonNode(const std::string key, const std::string value) {
  auto node = cJSON_CreateString(value.c_str());
  node->string = cJSON_strdup(key.c_str());
  return node;
}

cJSON* AEMarker::KeyValueToJsonNode(const std::string key, int value) {
  auto node = cJSON_CreateInt(value, -1);
  node->string = cJSON_strdup(key.c_str());
  return node;
}

static void ProcessImageFillMode(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  // if (layer->type() != pag::LayerType::Image) {
  //   return;
  // }
  // auto list = static_cast<std::vector<std::pair<pag::ID, ImageFillMode>>*>(ctx);
  // auto imageLayer = static_cast<pag::ImageLayer*>(layer);
  // auto layerH = context.getLayerHById(layer->id);
  // auto mode = PlaceImageLayer::GetFillModeFromLayer(layerH);
  // if (mode == ImageFillMode::NotFind) {
  //   mode = ImageFillMode::Default;
  // }
  // auto imageId = imageLayer->imageBytes->id;
  // bool finded = false;
  // for (auto pair : *list) {
  //   if (imageId == pair.first) {
  //     finded = true;
  //     if (mode != pair.second) {
  //       context.pushWarning(AlertInfoType::TextPathAnimator);
  //     }
  //     break;
  //   }
  // }
  // if (!finded) {
  //   list->push_back(std::make_pair(imageId, mode));
  // }
}

void AEMarker::ExportImageFillMode(std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                   const AEGP_ItemH& itemH) {
  // std::vector<std::pair<pag::ID, pag::Enum>> list;
  // PAGFilterUtil::TraversalLayers(context, context.compositions.back(), pag::LayerType::Image,
  //                                &ProcessImageFillMode, &list);
  // for (auto& pair : list) {
  //   auto mode = PlaceImageLayer::GetFillModeFromComposition(itemH, pair.first);
  //   if (mode != ImageFillMode::NotFind) {
  //     pair.second = static_cast<pag::Enum>(mode);
  //   }
  // }
  // auto editableImages = file->editableImages;
  // if (editableImages == nullptr) {
  //   editableImages = MakeDefaultEditableIndices(static_cast<int>(file->images.size()));
  // }
  // if (editableImages->size() > 0) {
  //   file->imageScaleModes = new std::vector<pag::Enum>();
  //   for (int i = 0; i < static_cast<int>(editableImages->size()); i++) {
  //     int index = editableImages->at(i);
  //     auto id = file->images[index]->id;
  //     for (auto pair : list) {
  //       if (pair.first == id) {
  //         file->imageScaleModes->push_back(pair.second);
  //       }
  //     }
  //   }
  // }
  // if (file->editableImages == nullptr) {
  //   delete editableImages;
  // }
}
