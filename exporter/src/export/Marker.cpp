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

#include "Marker.h"
#include <unordered_map>
#include <vector>
#include "nlohmann/json.hpp"
#include "utils/AETypeTransform.h"
#include "utils/Helper.h"
#include "utils/StringHelper.h"

using json = nlohmann::json;

namespace exporter {

struct TimeStretchModeConvert {
  static pag::PAGTimeStretchMode FromString(const std::string& str);
  static std::string ToString(pag::PAGTimeStretchMode mode);
  static const std::unordered_map<std::string, pag::PAGTimeStretchMode> stringToModeMap;
  static const std::unordered_map<pag::PAGTimeStretchMode, std::string> modeToStringMap;
  static const std::vector<std::pair<std::string, pag::PAGTimeStretchMode>> oldLookupVector;
};

struct ScaleModeConvert {
  static const std::unordered_map<std::string, pag::PAGScaleMode> stringToModeMap;
  static const std::unordered_map<pag::PAGScaleMode, std::string> modeToStringMap;
};

const std::unordered_map<std::string, pag::PAGTimeStretchMode>
    TimeStretchModeConvert::stringToModeMap = {
        {"none", pag::PAGTimeStretchMode::None},
        {"scale", pag::PAGTimeStretchMode::Scale},
        {"repeat", pag::PAGTimeStretchMode::Repeat},
        {"repeatinverted", pag::PAGTimeStretchMode::RepeatInverted}};

const std::unordered_map<pag::PAGTimeStretchMode, std::string>
    TimeStretchModeConvert::modeToStringMap = {
        {pag::PAGTimeStretchMode::None, "none"},
        {pag::PAGTimeStretchMode::Scale, "scale"},
        {pag::PAGTimeStretchMode::Repeat, "repeat"},
        {pag::PAGTimeStretchMode::RepeatInverted, "repeatinverted"}};

const std::vector<std::pair<std::string, pag::PAGTimeStretchMode>>
    TimeStretchModeConvert::oldLookupVector = {
        {"repeatinverted", pag::PAGTimeStretchMode::RepeatInverted},
        {"repeat", pag::PAGTimeStretchMode::Repeat},
        {"scale", pag::PAGTimeStretchMode::Scale},
        {"none", pag::PAGTimeStretchMode::None}};

const std::unordered_map<std::string, pag::PAGScaleMode> ScaleModeConvert::stringToModeMap = {
    {"None", pag::PAGScaleMode::None},
    {"Stretch", pag::PAGScaleMode::Stretch},
    {"LetterBox", pag::PAGScaleMode::LetterBox},
    {"Zoom", pag::PAGScaleMode::Zoom}};

const std::unordered_map<pag::PAGScaleMode, std::string> modeToStringMap = {
    {pag::PAGScaleMode::None, "None"},
    {pag::PAGScaleMode::Stretch, "Stretch"},
    {pag::PAGScaleMode::LetterBox, "LetterBox"},
    {pag::PAGScaleMode::Zoom, "Zoom"}};

pag::PAGTimeStretchMode TimeStretchModeConvert::FromString(const std::string& str) {
  const auto& lowerStr = ToLowerCase(str);
  auto it = stringToModeMap.find(lowerStr);
  if (it != stringToModeMap.end()) {
    return it->second;
  }
  return pag::PAGTimeStretchMode::Repeat;
}

std::string TimeStretchModeConvert::ToString(pag::PAGTimeStretchMode mode) {
  auto it = modeToStringMap.find(mode);
  if (it != modeToStringMap.end()) {
    return it->second;
  }
  return "repeat";
}

static std::string TimeStretchModeToString(pag::PAGTimeStretchMode mode) {
  return TimeStretchModeConvert::ToString(mode);
}

static pag::PAGTimeStretchMode StringToTimeStretchMode(const std::string& str) {
  return TimeStretchModeConvert::FromString(str);
}

static pag::PAGScaleMode StringToScaleMode(const std::string& str) {
  auto iter = ScaleModeConvert::stringToModeMap.find(str);
  if (iter != ScaleModeConvert::stringToModeMap.end()) {
    return iter->second;
  }
  return pag::PAGScaleMode::LetterBox;
}

static std::string GetKeyStringWithId(std::string key, pag::ID id) {
  return key + "-" + std::to_string(id);
}

static std::vector<int>* CreateDefaultIndices(int count) {
  if (count <= 0) {
    return nullptr;
  }

  auto indices = new std::vector<int>(count);
  std::iota(indices->begin(), indices->end(), 0);
  return indices;
}

static std::optional<std::string> ConvertJsonValueToString(const nlohmann::json& jsonValue) {
  if (jsonValue.is_string()) {
    return jsonValue.get<std::string>();
  }
  if (jsonValue.is_number()) {
    return jsonValue.dump();
  }
  if (jsonValue.is_boolean()) {
    return jsonValue.get<bool>() ? "true" : "false";
  }

  return std::nullopt;
}

static std::optional<nlohmann::json> FindKeyFromComment(const std::string& comment,
                                                        const std::string& key) {
  nlohmann::json json = nlohmann::json::parse(comment, nullptr, false);
  if (json.is_discarded() || !json.is_object() || !json.contains(key)) {
    return std::nullopt;
  }

  return json[key];
}

static std::optional<nlohmann::json> FindMarkerFromStream(const AEGP_StreamRefH& markerStreamH,
                                                          const std::string& key) {
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; ++index) {
    if (auto optComment = GetMarkerComment(markerStreamH, index)) {
      if (auto jsonValue = FindKeyFromComment(*optComment, key)) {
        return jsonValue;
      }
    }
  }
  return std::nullopt;
}

static std::optional<nlohmann::json> FindMarkerFromComposition(const AEGP_ItemH& itemH,
                                                               const std::string& key) {
  if (itemH == nullptr) {
    return std::nullopt;
  }

  AEGP_StreamRefH markerStreamH = GetItemMarkerStream(itemH);
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStreamH, key);
  DeleteStream(markerStreamH);
  return result;
}

static std::optional<nlohmann::json> FindMarkerFromLayer(const AEGP_LayerH& layerH,
                                                         const std::string& key) {
  if (layerH == nullptr) {
    return std::nullopt;
  }
  AEGP_StreamRefH markerStreamH = GetLayerMarkerStream(layerH);
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStreamH, key);
  DeleteStream(markerStreamH);
  return result;
}

static bool DeleteMarkerByIndex(const AEGP_StreamRefH markerStreamH, int index) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = GetSuites();
  A_Err err = suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
  return err == A_Err_NONE;
}

static void DeleteMarkerFromStream(const AEGP_StreamRefH markerStreamH, const std::string& key) {
  if (markerStreamH == nullptr || key.empty()) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = numKeyframes - 1; index >= 0; --index) {
    auto optComment = GetMarkerComment(markerStreamH, index);
    if (!optComment.has_value()) {
      continue;
    }

    nlohmann::json json = nlohmann::json::parse(*optComment, nullptr, false);
    if (json.is_discarded() || !json.is_object() || !json.contains(key)) {
      continue;
    }

    json.erase(key);
    if (json.empty()) {
      DeleteMarkerByIndex(markerStreamH, index);
    } else {
      SetMarkerComment(markerStreamH, index, json.dump(), {});
    }
  }
}

static void DeleteMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key) {
  if (itemH == nullptr) {
    return;
  }

  auto markerStreamH = GetItemMarkerStream(itemH);
  DeleteMarkerFromStream(markerStreamH, key);
  DeleteStream(markerStreamH);
}

std::vector<pag::Marker*> ExportMarkers(const std::shared_ptr<PAGExportSession>& session,
                                        const AEGP_LayerH& layerH) {
  if (session == nullptr || layerH == nullptr) {
    return {};
  }
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();

  StreamWrapper streamWrapper(GetLayerMarkerStream(layerH));
  AEGP_StreamRefH streamH = streamWrapper.get();
  if (!streamH) {
    return {};
  }

  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(streamH, &numKeyframes);

  std::vector<pag::Marker*> markers;
  if (numKeyframes > 0) {
    markers.reserve(numKeyframes);
  }

  for (A_long index = 0; index < numKeyframes; ++index) {
    AEGP_StreamValue2 streamValue;
    StreamValueWrapper streamValueWrapper(&streamValue, &suites);
    suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, streamH, index, &streamValue);
    AEGP_MarkerValP markerP = streamValue.val.markerP;
    if (!markerP) {
      continue;
    }

    AEGP_MemHandle unicodeH;
    suites->MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                 &unicodeH);

    std::string comment = AeMemoryHandleToString(unicodeH);
    suites->MemorySuite1()->AEGP_FreeMemHandle(unicodeH);

    A_Time keyTime;
    suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamH, index, AEGP_LTimeMode_CompTime,
                                                   &keyTime);

    A_Time duration;
    suites->MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);

    auto* marker = new pag::Marker();
    marker->comment = comment;
    marker->startTime = session->configParam.frameRate * keyTime.value / keyTime.scale;
    marker->duration = session->configParam.frameRate * duration.value / duration.scale;

    markers.push_back(marker);
  }
  return markers;
}

void ParseMarkers(pag::Layer* layer) {
  if (layer == nullptr) {
    return;
  }

  static constexpr const char* CachePolicyKey = "CachePolicy";

  for (const auto& marker : layer->markers) {
    const std::string& comment = marker->comment;
    if (comment.empty()) {
      continue;
    }

    const json j = json::parse(comment, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
      continue;
    }

    if (j.contains(CachePolicyKey)) {
      const auto& item = j.at(CachePolicyKey);
      if (item.is_number_integer()) {
        const auto value = item.get<int>();
        if (value == 1) {
          layer->cachePolicy = pag::CachePolicy::Enable;
        } else if (value == 2) {
          layer->cachePolicy = pag::CachePolicy::Disable;
        }
        break;
      }

      if (item.is_string()) {
        const auto& value = item.get<std::string>();
        if (value == "enable") {
          layer->cachePolicy = pag::CachePolicy::Enable;
        } else if (value == "disable") {
          layer->cachePolicy = pag::CachePolicy::Disable;
        }
        break;
      }
    }
  }
}

std::optional<std::string> GetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index) {
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  A_Err err = A_Err_NONE;
  AEGP_StreamValue2 streamValue = {};
  err = suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index,
                                                           &streamValue);
  if (err != A_Err_NONE) {
    return std::nullopt;
  }

  AEGP_MarkerValP markerP = streamValue.val.markerP;
  AEGP_MemHandle unicodeH = nullptr;
  err = suites->MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                     &unicodeH);
  if (err != A_Err_NONE || unicodeH == nullptr) {
    suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
    return std::nullopt;
  }

  std::string comment = AeMemoryHandleToString(unicodeH);
  suites->MemorySuite1()->AEGP_FreeMemHandle(unicodeH);
  suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

  return comment;
}

pag::PAGScaleMode GetImageFillMode(const AEGP_ItemH& itemH, pag::ID imageID) {
  pag::PAGScaleMode mode = pag::PAGScaleMode::LetterBox;
  std::string keyString = GetKeyStringWithId("ImageFillMode", imageID);
  auto value = FindMarkerFromComposition(itemH, keyString);
  if (value.has_value()) {
    mode = StringToScaleMode(value->get<std::string>());
  }
  return mode;
}

bool GetLayerEditable(const AEGP_ItemH& itemH, pag::ID id) {
  bool isEditable = true;
  std::string keyString = GetKeyStringWithId("noReplace", id);
  auto value = FindMarkerFromComposition(itemH, keyString);
  if (value.has_value()) {
    isEditable = false;
  }
  return isEditable;
}

std::string GetCompositionStoragePath(const AEGP_ItemH& itemH) {
  std::string storagePath = "";
  std::string keyString = "storePath";
  auto value = FindMarkerFromComposition(itemH, keyString);
  if (value.has_value()) {
    storagePath = value->get<std::string>();
  }
  return storagePath;
}

std::string GetMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key) {
  auto optionalJson = FindMarkerFromComposition(itemH, key);

  if (optionalJson.has_value()) {
    if (auto optionalString = ConvertJsonValueToString(*optionalJson)) {
      return *optionalString;
    }
  }
  return "";
}

void DeleteAllTimeStretchInfo(const AEGP_ItemH& itemH) {
  AEGP_StreamRefH markerStreamH = GetItemMarkerStream(itemH);
  if (markerStreamH == nullptr) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; ++index) {
    auto optComment = GetMarkerComment(markerStreamH, index);
    if (!optComment.has_value()) {
      continue;
    }

    const std::string& comment = *optComment;
    nlohmann::json json = nlohmann::json::parse(comment, nullptr, false);
    if (!json.is_discarded() && json.is_object() && json.contains("TimeStretchMode")) {
      json.erase("TimeStretchMode");

      if (json.empty()) {
        suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
      } else {
        SetMarkerComment(markerStreamH, index, json.dump(), {});
      }
    } else {
      if (comment.empty()) {
        continue;
      }
      const auto& lowerComment = ToLowerCase(comment);
      if (lowerComment.find("#timestretchmode") != std::string::npos) {
        suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
      }
    }
  }
  DeleteStream(markerStreamH);
}

bool SetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index, const std::string& comment,
                      A_Time durationTime) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  A_Err err = A_Err_NONE;

  AEGP_StreamValue2 streamValue = {};
  AEGP_MarkerValP markerP = nullptr;

  err = suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index,
                                                           &streamValue);
  if (err != A_Err_NONE) {
    return false;
  }

  err = suites->MarkerSuite1()->AEGP_NewMarker(&markerP);
  if (err != A_Err_NONE) {
    suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
    return false;
  }

  auto fmtComment = Utf8ToUtf16(comment);
  err = suites->MarkerSuite1()->AEGP_SetMarkerString(
      markerP, AEGP_MarkerString_COMMENT, reinterpret_cast<const A_u_short*>(fmtComment.c_str()),
      static_cast<A_long>(fmtComment.length()));
  if (err != A_Err_NONE) {
    suites->MarkerSuite1()->AEGP_DisposeMarker(markerP);
    suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
    return false;
  }

  if (durationTime.value < 0 || durationTime.scale == 0) {
    suites->MarkerSuite2()->AEGP_GetMarkerDuration(streamValue.val.markerP, &durationTime);
  }
  suites->MarkerSuite2()->AEGP_SetMarkerDuration(markerP, &durationTime);

  AEGP_MarkerValP oldMarkerP = streamValue.val.markerP;
  streamValue.val.markerP = markerP;
  err = suites->KeyframeSuite4()->AEGP_SetKeyframeValue(markerStreamH, index, &streamValue);

  if (err != A_Err_NONE) {
    suites->MarkerSuite1()->AEGP_DisposeMarker(markerP);
    streamValue.val.markerP = oldMarkerP;
  }

  suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

  return err == A_Err_NONE;
}

void SetImageFillMode(pag::PAGScaleMode mode, const AEGP_ItemH& itemH, pag::ID imageID) {
  std::string keyString = GetKeyStringWithId("ImageFillMode", imageID);
  if (mode == pag::PAGScaleMode::LetterBox) {
    DeleteMarkerFromComposition(itemH, keyString);
    return;
  }
  std::string modeString = modeToStringMap.at(mode);
  nlohmann::json value = json::value_t::string;
  value = modeString;
  AddMarkerToComposition(itemH, keyString, value);
}

void SetLayerEditable(bool isEditable, const AEGP_ItemH& itemH, pag::ID id) {
  std::string keyString = GetKeyStringWithId("noReplace", id);
  DeleteMarkerFromComposition(itemH, keyString);
  if (isEditable) {
    return;
  }
  nlohmann::json value = json::value_t::number_integer;
  value = 1;
  AddMarkerToComposition(itemH, keyString, value);
}

void SetCompositionStoragePath(const std::string& path, const AEGP_ItemH& itemH) {
  std::string keyString = "storePath";
  DeleteMarkerFromComposition(itemH, keyString);
  if (path.empty()) {
    return;
  }
  nlohmann::json value = json::value_t::string;
  value = path;
  AddMarkerToComposition(itemH, keyString, value);
}

bool AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& comment,
                          A_Time time, A_Time durationTime, A_long index) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = GetSuites();
  A_Err err = suites->KeyframeSuite4()->AEGP_InsertKeyframe(markerStreamH, AEGP_LTimeMode_CompTime,
                                                            &time, &index);

  if (err != A_Err_NONE) {
    return false;
  }

  if (!comment.empty()) {
    return SetMarkerComment(markerStreamH, index, comment, durationTime);
  }

  return true;
}

void SetTimeStretchInfo(const TimeStretchInfo& info, const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return;
  }

  DeleteAllTimeStretchInfo(itemH);
  std::string keyString = "TimeStretchMode";
  nlohmann::json value = nlohmann::json::value_t::string;
  value = TimeStretchModeToString(info.mode);
  std::string modeString = TimeStretchModeToString(info.mode);

  A_Time keyTime;
  A_Time durationTime;
  float frameRate = GetItemFrameRate(itemH);
  keyTime.scale = static_cast<A_u_long>(frameRate);
  keyTime.value = static_cast<A_long>(info.start);
  durationTime.scale = static_cast<A_u_long>(frameRate);
  durationTime.value = static_cast<A_long>(info.duration);

  AddMarkerToComposition(itemH, keyString, value, keyTime, durationTime);
}

void ExportTimeStretch(std::shared_ptr<pag::File>& file,
                       const std::shared_ptr<PAGExportSession>& session, const AEGP_ItemH& itemH) {
  if (file == nullptr || itemH == nullptr) {
    return;
  }

  const auto& suites = GetSuites();

  A_Time durationTime = {};
  suites->ItemSuite6()->AEGP_GetItemDuration(itemH, &durationTime);
  auto compositionDuration = AETimeToTime(durationTime, session->frameRate);

  auto optInfo = GetTimeStretchInfo(itemH);
  if (optInfo.has_value()) {
    const auto& info = *optInfo;
    pag::Frame timeStretchStart =
        std::min(std::max(info.start, (pag::Frame)0), compositionDuration);
    pag::Frame timeStretchDuration =
        std::min(std::max(info.duration, (pag::Frame)0), compositionDuration - timeStretchStart);

    if (timeStretchDuration == 0) {
      file->scaledTimeRange.start = 0;
      file->scaledTimeRange.end = compositionDuration;
    } else {
      file->scaledTimeRange.start = timeStretchStart;
      file->scaledTimeRange.end = timeStretchStart + timeStretchDuration;
    }
    file->timeStretchMode = info.mode;
  } else {
    file->timeStretchMode = pag::PAGTimeStretchMode::Repeat;
    file->scaledTimeRange.start = 0;
    file->scaledTimeRange.end = compositionDuration;
  }
}

void ExportImageLayerEditable(std::shared_ptr<pag::File>& file,
                              const std::shared_ptr<PAGExportSession>& session,
                              const AEGP_ItemH& itemH) {
  if (!file || file->images.empty()) {
    return;
  }

  int imageCount = static_cast<int>(file->images.size());
  std::vector<bool> isEditable(imageCount, false);
  bool hasNonEditableImage = false;

  for (int index = 0; index < file->numImages(); index++) {
    const auto& layerVector = file->getImageAt(index);
    for (const auto& layer : layerVector) {
      if (layer->imageBytes == nullptr) {
        continue;
      }
      if (IsImageLayerNonReplaceable(layer, itemH, session)) {
        hasNonEditableImage = true;
        continue;
      }
      for (int imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        const auto& image = file->images[imageIndex];
        if (image->id == layer->imageBytes->id) {
          isEditable[imageIndex] = true;
          break;
        }
      }
    }
  }

  if (hasNonEditableImage) {
    file->editableImages = new std::vector<int>();
    for (int index = 0; index < imageCount; index++) {
      if (isEditable[index]) {
        file->editableImages->push_back(index);
      }
    }
  }
}

bool IsTextLayerNonReplaceable(const pag::TextLayer* layer, const AEGP_ItemH& itemH,
                               const std::shared_ptr<PAGExportSession>& session) {
  if (!layer) {
    return false;
  }

  auto keyString = GetKeyStringWithId("noReplace", layer->id);
  if (FindMarkerFromComposition(itemH, keyString).has_value()) {
    return true;
  }

  AEGP_LayerH layerH = session->getLayerHByID(layer->id);
  if (FindMarkerFromLayer(layerH, "noReplace").has_value()) {
    return true;
  }

  return false;
}

bool IsImageLayerNonReplaceable(const pag::ImageLayer* layer, const AEGP_ItemH& itemH,
                                const std::shared_ptr<PAGExportSession>& session) {
  if (session->isVideoLayer(layer->id)) {
    return false;
  }

  auto keyString = GetKeyStringWithId("noReplace", layer->id);
  if (FindMarkerFromComposition(itemH, keyString).has_value()) {
    return true;
  }

  AEGP_LayerH layerH = session->getLayerHByID(layer->id);
  if (FindMarkerFromLayer(layerH, "noReplace").has_value()) {
    return true;
  }
  return false;
}

void ExportTextLayerEditable(std::shared_ptr<pag::File>& file,
                             const std::shared_ptr<PAGExportSession>& session,
                             const AEGP_ItemH& mainCompItemH) {
  if (!file || file->numTexts() == 0) {
    return;
  }

  auto textCount = file->numTexts();
  std::vector<bool> isEditable(textCount, true);
  bool hasNonEditableText = false;

  for (int i = 0; i < textCount; ++i) {
    auto layer = file->getTextAt(i);
    if (IsTextLayerNonReplaceable(layer, mainCompItemH, session)) {
      isEditable[i] = false;
      hasNonEditableText = true;
    }
  }

  if (hasNonEditableText) {
    file->editableTexts = new std::vector<int>();
    for (int i = 0; i < textCount; ++i) {
      if (isEditable[i]) {
        file->editableTexts->push_back(i);
      }
    }
  }
}

void ExportLayerEditable(std::shared_ptr<pag::File>& file,
                         const std::shared_ptr<PAGExportSession>& session,
                         const AEGP_ItemH& itemH) {
  ExportImageLayerEditable(file, session, itemH);
  ExportTextLayerEditable(file, session, itemH);

  const bool imagesWereExplicitlySet = (file->editableImages != nullptr);
  const bool textsWereExplicitlySet = (file->editableTexts != nullptr);

  if (imagesWereExplicitlySet != textsWereExplicitlySet) {
    if (textsWereExplicitlySet && !imagesWereExplicitlySet && !file->images.empty()) {
      file->editableImages = CreateDefaultIndices(static_cast<int>(file->images.size()));
    }

    if (imagesWereExplicitlySet && !textsWereExplicitlySet && file->numTexts() > 0) {
      file->editableTexts = CreateDefaultIndices(file->numTexts());
    }
  }
}

void ExportImageFillMode(std::shared_ptr<pag::File>& file, const AEGP_ItemH& itemH) {
  if (!file) {
    return;
  }

  std::unordered_map<pag::ID, pag::PAGScaleMode> imageFillModeMap = {};
  for (const auto& image : file->images) {
    imageFillModeMap.emplace(image->id, GetImageFillMode(itemH, image->id));
  }

  const std::vector<int>* editableIndices = file->editableImages;
  std::unique_ptr<std::vector<int>> defaultIndices;
  if (editableIndices == nullptr) {
    defaultIndices.reset(CreateDefaultIndices(static_cast<int>(file->images.size())));
    editableIndices = defaultIndices.get();
  }

  if (editableIndices == nullptr || editableIndices->empty()) {
    return;
  }

  file->imageScaleModes = new std::vector<pag::PAGScaleMode>();
  file->imageScaleModes->reserve(editableIndices->size());

  for (auto index : *editableIndices) {
    if (index >= 0 && index < static_cast<int>(file->images.size())) {
      auto image = file->images[index];
      if (image) {
        auto it = imageFillModeMap.find(image->id);
        if (it != imageFillModeMap.end()) {
          file->imageScaleModes->push_back(it->second);
        }
      }
    }
  }
}

void AddMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& key,
                       const nlohmann::json& value, A_Time time, A_Time durationTime) {
  if (markerStreamH == nullptr || key.empty()) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; ++index) {
    auto optComment = GetMarkerComment(markerStreamH, index);
    if (!optComment.has_value()) {
      continue;
    }

    nlohmann::json json = nlohmann::json::parse(*optComment, nullptr, false);
    if (json.is_object()) {
      json[key] = value;
      if (time.value == 0 && time.scale == 1 && durationTime.value == 0 &&
          durationTime.scale == 0) {
        SetMarkerComment(markerStreamH, index, json.dump(), {});
      } else {
        DeleteMarkerByIndex(markerStreamH, index);
        AddNewMarkerToStream(markerStreamH, json.dump(), time, durationTime, index);
      }
      return;
    }
  }

  nlohmann::json newJson = {{key, value}};
  AddNewMarkerToStream(markerStreamH, newJson.dump(), time, durationTime);
}

void AddMarkerToComposition(const AEGP_ItemH& itemH, const std::string& key,
                            const nlohmann::json& value, A_Time time, A_Time durationTime) {
  if (itemH == nullptr) {
    return;
  }
  auto markerStreamH = GetItemMarkerStream(itemH);
  if (markerStreamH == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStreamH, key);
  AddMarkerToStream(markerStreamH, key, value, time, durationTime);
  DeleteStream(markerStreamH);
}

void AddMarkerToLayer(const AEGP_LayerH& layerH, const std::string& key,
                      const nlohmann::json& value) {
  if (layerH == nullptr) {
    return;
  }
  auto markerStreamH = GetLayerMarkerStream(layerH);
  if (markerStreamH == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStreamH, key);
  AddMarkerToStream(markerStreamH, key, value);
  DeleteStream(markerStreamH);
}

std::optional<TimeStretchInfo> GetTimeStretchInfo(const AEGP_ItemH& itemH) {
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();

  AEGP_StreamRefH markerStreamH = GetItemMarkerStream(itemH);
  if (!markerStreamH) {
    return std::nullopt;
  }

  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; index++) {
    auto optComment = GetMarkerComment(markerStreamH, index);
    if (!optComment.has_value()) {
      continue;
    }

    const std::string& comment = *optComment;
    std::optional<pag::PAGTimeStretchMode> foundMode;
    if (auto jsonValue = FindKeyFromComment(comment, "TimeStretchMode")) {
      if (jsonValue->is_string()) {
        foundMode = StringToTimeStretchMode(jsonValue->get<std::string>());
      }
    }

    if (!foundMode) {
      const auto& lowerComment = ToLowerCase(comment);
      if (lowerComment.find("#timestretchmode") != std::string::npos) {
        for (const auto& entry : TimeStretchModeConvert::oldLookupVector) {
          if (lowerComment.find(entry.first) != std::string::npos) {
            foundMode = entry.second;
            break;
          }
        }
      }
    }

    if (foundMode) {
      A_Time time;
      suites->KeyframeSuite4()->AEGP_GetKeyframeTime(markerStreamH, index, AEGP_LTimeMode_CompTime,
                                                     &time);

      AEGP_StreamValue2 streamValue;
      suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStreamH, index,
                                                         &streamValue);
      AEGP_MarkerValP markerP = streamValue.val.markerP;
      A_Time duration;
      suites->MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);
      suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

      float frameRate = GetItemFrameRate(itemH);

      TimeStretchInfo result = {*foundMode, AETimeToTime(time, frameRate),
                                AETimeToTime(duration, frameRate)};

      DeleteStream(markerStreamH);
      return result;
    }
  }

  DeleteStream(markerStreamH);
  return std::nullopt;
}

}  // namespace exporter