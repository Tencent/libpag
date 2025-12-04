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
#include "utils/AEDataTypeConverter.h"
#include "utils/LayerHelper.h"
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

static std::optional<nlohmann::json> FindMarkerFromStream(const AEGP_StreamRefH& markerStream,
                                                          const std::string& key) {
  if (markerStream == nullptr) {
    return std::nullopt;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStream, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; ++index) {
    if (auto optComment = GetMarkerComment(markerStream, index)) {
      if (auto jsonValue = FindKeyFromComment(*optComment, key)) {
        return jsonValue;
      }
    }
  }
  return std::nullopt;
}

static std::optional<nlohmann::json> FindMarkerFromComposition(const AEGP_ItemH& itemHandle,
                                                               const std::string& key) {
  if (itemHandle == nullptr) {
    return std::nullopt;
  }

  AEGP_StreamRefH markerStream = GetItemMarkerStream(itemHandle);
  if (markerStream == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStream, key);
  DeleteStream(markerStream);
  return result;
}

static std::optional<nlohmann::json> FindMarkerFromLayer(const AEGP_LayerH& layerHandle,
                                                         const std::string& key) {
  if (layerHandle == nullptr) {
    return std::nullopt;
  }
  AEGP_StreamRefH markerStream = GetLayerMarkerStream(layerHandle);
  if (markerStream == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStream, key);
  DeleteStream(markerStream);
  return result;
}

static bool DeleteMarkerByIndex(const AEGP_StreamRefH markerStream, int index) {
  if (markerStream == nullptr) {
    return false;
  }

  const auto& suites = GetSuites();
  A_Err err = suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStream, index);
  return err == A_Err_NONE;
}

static void DeleteMarkerFromStream(const AEGP_StreamRefH markerStream, const std::string& key) {
  if (markerStream == nullptr || key.empty()) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStream, &numKeyframes);

  for (A_long index = numKeyframes - 1; index >= 0; --index) {
    auto optComment = GetMarkerComment(markerStream, index);
    if (!optComment.has_value()) {
      continue;
    }

    nlohmann::json json = nlohmann::json::parse(*optComment, nullptr, false);
    if (json.is_discarded() || !json.is_object() || !json.contains(key)) {
      continue;
    }

    json.erase(key);
    if (json.empty()) {
      DeleteMarkerByIndex(markerStream, index);
    } else {
      SetMarkerComment(markerStream, index, json.dump(), {});
    }
  }
}

static void DeleteMarkerFromComposition(const AEGP_ItemH& itemHandle, const std::string& key) {
  if (itemHandle == nullptr) {
    return;
  }

  auto markerStream = GetItemMarkerStream(itemHandle);
  DeleteMarkerFromStream(markerStream, key);
  DeleteStream(markerStream);
}

std::vector<pag::Marker*> ExportMarkers(std::shared_ptr<PAGExportSession> session,
                                        const AEGP_LayerH& layerHandle) {
  if (session == nullptr || layerHandle == nullptr) {
    return {};
  }
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();

  auto streamHandle = GetLayerMarkerStream(layerHandle);
  if (!streamHandle) {
    return {};
  }

  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(streamHandle, &numKeyframes);

  std::vector<pag::Marker*> markers;
  if (numKeyframes > 0) {
    markers.reserve(numKeyframes);
  }

  for (A_long index = 0; index < numKeyframes; ++index) {
    A_Time keyTime;
    A_Err err = suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamHandle, index,
                                                               AEGP_LTimeMode_CompTime, &keyTime);
    if (err != A_Err_NONE || keyTime.scale == 0) {
      continue;
    }

    AEGP_StreamValue2 streamValue;
    suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, streamHandle, index, &streamValue);
    AEGP_MarkerValP markerP = streamValue.val.markerP;
    if (!markerP) {
      continue;
    }

    AEGP_MemHandle memoryHandle = nullptr;
    suites->MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                 &memoryHandle);

    std::string comment = AeMemoryHandleToString(memoryHandle);
    suites->MemorySuite1()->AEGP_FreeMemHandle(memoryHandle);

    A_Time duration;
    suites->MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);

    auto marker = new pag::Marker();
    marker->comment = comment;
    marker->startTime = session->configParam.frameRate * keyTime.value / keyTime.scale;
    marker->duration = session->configParam.frameRate * duration.value / duration.scale;

    markers.push_back(marker);
    suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
  }
  DeleteStream(streamHandle);
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

std::optional<std::string> GetMarkerComment(const AEGP_StreamRefH& markerStream, int index) {
  if (markerStream == nullptr) {
    return std::nullopt;
  }

  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  A_Err err = A_Err_NONE;

  A_Time keyTime = {};
  err = suites->KeyframeSuite4()->AEGP_GetKeyframeTime(markerStream, index, AEGP_LTimeMode_CompTime,
                                                       &keyTime);
  if (err != A_Err_NONE || keyTime.scale == 0) {
    return std::nullopt;
  }

  AEGP_StreamValue2 streamValue = {};
  err = suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStream, index,
                                                           &streamValue);
  if (err != A_Err_NONE) {
    return std::nullopt;
  }

  AEGP_MarkerValP markerP = streamValue.val.markerP;
  AEGP_MemHandle memoryHandle = nullptr;
  err = suites->MarkerSuite2()->AEGP_GetMarkerString(pluginID, markerP, AEGP_MarkerString_COMMENT,
                                                     &memoryHandle);
  if (err != A_Err_NONE || memoryHandle == nullptr) {
    suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);
    return std::nullopt;
  }

  std::string comment = AeMemoryHandleToString(memoryHandle);
  suites->MemorySuite1()->AEGP_FreeMemHandle(memoryHandle);
  suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

  return comment;
}

pag::PAGScaleMode GetImageFillMode(const AEGP_ItemH& itemHandle, pag::ID imageID) {
  pag::PAGScaleMode mode = pag::PAGScaleMode::LetterBox;
  std::string keyString = GetKeyStringWithId("ImageFillMode", imageID);
  auto value = FindMarkerFromComposition(itemHandle, keyString);
  if (value.has_value()) {
    mode = StringToScaleMode(value->get<std::string>());
  }
  return mode;
}

bool GetLayerEditable(const AEGP_ItemH& itemHandle, pag::ID id) {
  bool isEditable = true;
  std::string keyString = GetKeyStringWithId("noReplace", id);
  auto value = FindMarkerFromComposition(itemHandle, keyString);
  if (value.has_value()) {
    isEditable = false;
  }
  return isEditable;
}

std::string GetCompositionStoragePath(const AEGP_ItemH& itemHandle) {
  std::string storagePath = "";
  std::string keyString = "storePath";
  auto value = FindMarkerFromComposition(itemHandle, keyString);
  if (value.has_value()) {
    storagePath = value->get<std::string>();
  }
  return storagePath;
}

std::string GetMarkerFromComposition(const AEGP_ItemH& itemHandle, const std::string& key) {
  auto optionalJson = FindMarkerFromComposition(itemHandle, key);

  if (optionalJson.has_value()) {
    if (auto optionalString = ConvertJsonValueToString(*optionalJson)) {
      return *optionalString;
    }
  }
  return "";
}

void DeleteAllTimeStretchInfo(const AEGP_ItemH& itemHandle) {
  AEGP_StreamRefH markerStream = GetItemMarkerStream(itemHandle);
  if (markerStream == nullptr) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStream, &numKeyframes);

  for (A_long index = numKeyframes - 1; index >= 0; --index) {
    auto optComment = GetMarkerComment(markerStream, index);
    if (!optComment.has_value()) {
      continue;
    }

    const std::string& comment = *optComment;
    nlohmann::json json = nlohmann::json::parse(comment, nullptr, false);
    if (!json.is_discarded() && json.is_object() && json.contains("TimeStretchMode")) {
      json.erase("TimeStretchMode");

      if (json.empty()) {
        suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStream, index);
      } else {
        SetMarkerComment(markerStream, index, json.dump(), {});
      }
    } else {
      if (comment.empty()) {
        continue;
      }
      const auto& lowerComment = ToLowerCase(comment);
      if (lowerComment.find("#timestretchmode") != std::string::npos) {
        suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStream, index);
      }
    }
  }
  DeleteStream(markerStream);
}

bool SetMarkerComment(const AEGP_StreamRefH& markerStream, int index, const std::string& comment,
                      A_Time durationTime) {
  if (markerStream == nullptr) {
    return false;
  }

  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  A_Err err = A_Err_NONE;
  A_Time keyTime = {};
  err = suites->KeyframeSuite4()->AEGP_GetKeyframeTime(markerStream, index, AEGP_LTimeMode_CompTime,
                                                       &keyTime);
  if (err != A_Err_NONE || keyTime.scale == 0) {
    return false;
  }

  AEGP_StreamValue2 streamValue = {};
  AEGP_MarkerValP markerP = nullptr;

  err = suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStream, index,
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
  err = suites->KeyframeSuite4()->AEGP_SetKeyframeValue(markerStream, index, &streamValue);

  if (err != A_Err_NONE) {
    suites->MarkerSuite1()->AEGP_DisposeMarker(markerP);
    streamValue.val.markerP = oldMarkerP;
  }

  suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

  return err == A_Err_NONE;
}

void SetImageFillMode(pag::PAGScaleMode mode, const AEGP_ItemH& itemHandle, pag::ID imageID) {
  std::string keyString = GetKeyStringWithId("ImageFillMode", imageID);
  if (mode == pag::PAGScaleMode::LetterBox) {
    DeleteMarkerFromComposition(itemHandle, keyString);
    return;
  }
  std::string modeString = modeToStringMap.at(mode);
  nlohmann::json value = modeString;
  AddMarkerToComposition(itemHandle, keyString, value);
}

void SetLayerEditable(bool isEditable, const AEGP_ItemH& itemHandle, pag::ID id) {
  std::string keyString = GetKeyStringWithId("noReplace", id);
  DeleteMarkerFromComposition(itemHandle, keyString);
  if (isEditable) {
    return;
  }
  nlohmann::json value = 1;
  AddMarkerToComposition(itemHandle, keyString, value);
}

void SetCompositionStoragePath(const std::string& path, const AEGP_ItemH& itemHandle) {
  std::string keyString = "storePath";
  DeleteMarkerFromComposition(itemHandle, keyString);
  if (path.empty()) {
    return;
  }
  nlohmann::json value = path;
  AddMarkerToComposition(itemHandle, keyString, value);
}

bool AddNewMarkerToStream(const AEGP_StreamRefH& markerStream, const std::string& comment,
                          A_Time time, A_Time durationTime, A_long index) {
  if (markerStream == nullptr) {
    return false;
  }

  if (time.scale == 0) {
    time.scale = 1;
  }

  const auto& suites = GetSuites();
  A_Err err = suites->KeyframeSuite4()->AEGP_InsertKeyframe(markerStream, AEGP_LTimeMode_CompTime,
                                                            &time, &index);

  if (err != A_Err_NONE) {
    return false;
  }

  if (!comment.empty()) {
    return SetMarkerComment(markerStream, index, comment, durationTime);
  }

  return true;
}

void SetTimeStretchInfo(const TimeStretchInfo& info, const AEGP_ItemH& itemHandle) {
  if (itemHandle == nullptr) {
    return;
  }

  float frameRate = GetItemFrameRate(itemHandle);
  if (frameRate <= 0) {
    return;
  }

  DeleteAllTimeStretchInfo(itemHandle);
  std::string keyString = "TimeStretchMode";
  nlohmann::json value = TimeStretchModeToString(info.mode);
  std::string modeString = TimeStretchModeToString(info.mode);

  A_Time keyTime;
  A_Time durationTime;
  keyTime.scale = static_cast<A_u_long>(frameRate);
  keyTime.value = static_cast<A_long>(info.start);
  durationTime.scale = static_cast<A_u_long>(frameRate);
  durationTime.value = static_cast<A_long>(info.duration);

  AddMarkerToComposition(itemHandle, keyString, value, keyTime, durationTime);
}

void ExportTimeStretch(std::shared_ptr<pag::File> file, std::shared_ptr<PAGExportSession> session,
                       const AEGP_ItemH& itemHandle) {
  if (file == nullptr || itemHandle == nullptr) {
    return;
  }

  const auto& suites = GetSuites();

  A_Time durationTime = {};
  AEGP_CompH compHandle = GetItemCompH(itemHandle);
  suites->CompSuite6()->AEGP_GetCompWorkAreaDuration(compHandle, &durationTime);
  auto compositionDuration = AEDurationToFrame(durationTime, session->frameRate);

  auto optInfo = GetTimeStretchInfo(itemHandle);
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

void ExportImageLayerEditable(std::shared_ptr<pag::File> file,
                              std::shared_ptr<PAGExportSession> session,
                              const AEGP_ItemH& itemHandle) {
  if (!file || file->images.empty()) {
    return;
  }

  auto imageCount = static_cast<int>(file->images.size());
  std::vector<bool> isEditable(imageCount, false);
  bool hasNonEditableImage = false;

  for (int index = 0; index < file->numImages(); index++) {
    const auto& layerVector = file->getImageAt(index);
    for (const auto& layer : layerVector) {
      if (layer->imageBytes == nullptr) {
        continue;
      }
      if (IsImageLayerNonReplaceable(layer, itemHandle, session)) {
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

bool IsTextLayerNonReplaceable(const pag::TextLayer* layer, const AEGP_ItemH& itemHandle,
                               std::shared_ptr<PAGExportSession> session) {
  if (!layer) {
    return false;
  }

  auto keyString = GetKeyStringWithId("noReplace", layer->id);
  if (FindMarkerFromComposition(itemHandle, keyString).has_value()) {
    return true;
  }

  AEGP_LayerH layerHandle = session->getLayerHByID(layer->id);
  if (FindMarkerFromLayer(layerHandle, "noReplace").has_value()) {
    return true;
  }

  return false;
}

bool IsImageLayerNonReplaceable(const pag::ImageLayer* layer, const AEGP_ItemH& itemHandle,
                                std::shared_ptr<PAGExportSession> session) {
  if (session->isVideoLayer(layer->id)) {
    return false;
  }

  auto keyString = GetKeyStringWithId("noReplace", layer->id);
  if (FindMarkerFromComposition(itemHandle, keyString).has_value()) {
    return true;
  }

  AEGP_LayerH layerHandle = session->getLayerHByID(layer->id);
  if (FindMarkerFromLayer(layerHandle, "noReplace").has_value()) {
    return true;
  }
  return false;
}

void ExportTextLayerEditable(std::shared_ptr<pag::File> file,
                             std::shared_ptr<PAGExportSession> session,
                             const AEGP_ItemH& mainCompositionItemHandle) {
  if (!file || file->numTexts() == 0) {
    return;
  }

  auto textCount = file->numTexts();
  std::vector<bool> isEditable(textCount, true);
  bool hasNonEditableText = false;

  for (int i = 0; i < textCount; ++i) {
    auto layer = file->getTextAt(i);
    if (IsTextLayerNonReplaceable(layer, mainCompositionItemHandle, session)) {
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

void ExportLayerEditable(std::shared_ptr<pag::File> file, std::shared_ptr<PAGExportSession> session,
                         const AEGP_ItemH& itemHandle) {
  ExportImageLayerEditable(file, session, itemHandle);
  ExportTextLayerEditable(file, session, itemHandle);

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

void ExportImageFillMode(std::shared_ptr<pag::File> file, const AEGP_ItemH& itemHandle) {
  if (!file) {
    return;
  }

  std::unordered_map<pag::ID, pag::PAGScaleMode> imageFillModeMap(file->images.size());
  for (const auto& image : file->images) {
    imageFillModeMap.emplace(image->id, GetImageFillMode(itemHandle, image->id));
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

void AddMarkerToStream(const AEGP_StreamRefH& markerStream, const std::string& key,
                       const nlohmann::json& value, A_Time time, A_Time durationTime) {
  if (markerStream == nullptr || key.empty()) {
    return;
  }

  const auto& suites = GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStream, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; ++index) {
    auto optComment = GetMarkerComment(markerStream, index);
    if (!optComment.has_value()) {
      continue;
    }

    nlohmann::json json = nlohmann::json::parse(*optComment, nullptr, false);
    if (json.is_object()) {
      json[key] = value;
      if (time.value == 0 && time.scale == 1 && durationTime.value == 0 &&
          durationTime.scale == 0) {
        SetMarkerComment(markerStream, index, json.dump(), {});
      } else {
        DeleteMarkerByIndex(markerStream, index);
        AddNewMarkerToStream(markerStream, json.dump(), time, durationTime, index);
      }
      return;
    }
  }

  nlohmann::json newJson = {{key, value}};
  AddNewMarkerToStream(markerStream, newJson.dump(), time, durationTime);
}

void AddMarkerToComposition(const AEGP_ItemH& itemHandle, const std::string& key,
                            const nlohmann::json& value, A_Time time, A_Time durationTime) {
  if (itemHandle == nullptr) {
    return;
  }
  auto markerStream = GetItemMarkerStream(itemHandle);
  if (markerStream == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStream, key);
  AddMarkerToStream(markerStream, key, value, time, durationTime);
  DeleteStream(markerStream);
}

void AddMarkerToLayer(const AEGP_LayerH& layerHandle, const std::string& key,
                      const nlohmann::json& value) {
  if (layerHandle == nullptr) {
    return;
  }
  auto markerStream = GetLayerMarkerStream(layerHandle);
  if (markerStream == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStream, key);
  AddMarkerToStream(markerStream, key, value);
  DeleteStream(markerStream);
}

std::optional<TimeStretchInfo> GetTimeStretchInfo(const AEGP_ItemH& itemHandle) {
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();

  AEGP_StreamRefH markerStream = GetItemMarkerStream(itemHandle);
  if (!markerStream) {
    return std::nullopt;
  }

  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStream, &numKeyframes);

  for (A_long index = 0; index < numKeyframes; index++) {
    auto optComment = GetMarkerComment(markerStream, index);
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
      suites->KeyframeSuite4()->AEGP_GetKeyframeTime(markerStream, index, AEGP_LTimeMode_CompTime,
                                                     &time);

      AEGP_StreamValue2 streamValue;
      suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(pluginID, markerStream, index,
                                                         &streamValue);
      AEGP_MarkerValP markerP = streamValue.val.markerP;
      A_Time duration;
      suites->MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);
      suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

      float frameRate = GetItemFrameRate(itemHandle);

      TimeStretchInfo result = {*foundMode, AEDurationToFrame(time, frameRate),
                                AEDurationToFrame(duration, frameRate)};

      DeleteStream(markerStream);
      return result;
    }
  }

  DeleteStream(markerStream);
  return std::nullopt;
}

}  // namespace exporter