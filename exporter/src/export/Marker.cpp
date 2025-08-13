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

pag::PAGTimeStretchMode TimeStretchModeConvert::FromString(const std::string& str) {
  const auto& lowerStr = StringHelper::ToLowerCase(str);
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

static std::optional<nlohmann::json> FindKeyFromComment(const std::string& comment,
                                                        const std::string& key) {
  nlohmann::json json = nlohmann::json::parse(comment, nullptr, false);
  if (json.is_discarded() || !json.is_object()) {
    return std::nullopt;
  }

  auto it = json.find(key);
  if (it != json.end()) {
    return *it;
  }

  return std::nullopt;
}

std::vector<std::unique_ptr<pag::Marker>> Marker::ExportMarkers(PAGExportSession* session,
                                                                const AEGP_LayerH& layerH) {
  if (session == nullptr || layerH == nullptr) {
    return {};
  }
  const auto& suites = AEHelper::GetSuites();
  auto pluginID = AEHelper::GetPluginID();

  StreamWrapper streamWrapper(AEHelper::GetMarkerStreamFromLayer(layerH));
  AEGP_StreamRefH streamH = streamWrapper.get();
  if (!streamH) {
    return {};
  }

  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(streamH, &numKeyframes);

  std::vector<std::unique_ptr<pag::Marker>> markers;
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

    std::string comment = StringHelper::AeMemoryHandleToString(unicodeH);
    suites->MemorySuite1()->AEGP_FreeMemHandle(unicodeH);

    A_Time keyTime;
    suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamH, index, AEGP_LTimeMode_CompTime,
                                                   &keyTime);

    A_Time duration;
    suites->MarkerSuite2()->AEGP_GetMarkerDuration(markerP, &duration);

    auto marker = std::make_unique<pag::Marker>();
    marker->comment = comment;
    marker->startTime = session->configParam.frameRate * keyTime.value / keyTime.scale;
    marker->duration = session->configParam.frameRate * duration.value / duration.scale;

    markers.push_back(std::move(marker));
  }
  return markers;
}

void Marker::ParseMarkers(pag::Layer* layer) {
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

std::optional<std::string> Marker::GetMarkerComment(const AEGP_StreamRefH& markerStreamH,
                                                    int index) {
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  const auto& suites = AEHelper::GetSuites();
  auto pluginID = AEHelper::GetPluginID();
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

  std::string comment = StringHelper::AeMemoryHandleToString(unicodeH);
  suites->MemorySuite1()->AEGP_FreeMemHandle(unicodeH);
  suites->StreamSuite3()->AEGP_DisposeStreamValue(&streamValue);

  return comment;
}

std::string Marker::GetMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key) {
  auto optionalJson = FindMarkerFromComposition(itemH, key);

  if (optionalJson.has_value()) {
    if (auto optionalString = ConvertJsonValueToString(*optionalJson)) {
      return *optionalString;
    }
  }
  return "";
}

void Marker::DeleteAllTimeStretchInfo(const AEGP_StreamRefH& markerStreamH) {
  if (markerStreamH == nullptr) {
    return;
  }

  const auto& suites = AEHelper::GetSuites();
  A_long numKeyframes = 0;
  suites->KeyframeSuite4()->AEGP_GetStreamNumKFs(markerStreamH, &numKeyframes);

  for (A_long index = numKeyframes - 1; index >= 0; ++index) {
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
      const auto& lowerComment = StringHelper::ToLowerCase(comment);
      if (lowerComment.find("#timestretchmode") != std::string::npos) {
        suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
      }
    }
  }
}

bool Marker::SetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index,
                              const std::string& comment, A_Time durationTime) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = AEHelper::GetSuites();
  auto pluginID = AEHelper::GetPluginID();
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

  auto fmtComment = StringHelper::Utf8ToUtf16(comment);
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
bool Marker::AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& comment,
                                  A_Time time, A_Time durationTime) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = AEHelper::GetSuites();
  A_long index = 0;
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

bool Marker::SetTimeStretchInfo(const TimeStretchInfo& info, const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return false;
  }

  AEGP_StreamRefH markerStreamH = AEHelper::GetMarkerStreamFromItem(itemH);
  if (markerStreamH == nullptr) {
    return false;
  }

  DeleteAllTimeStretchInfo(markerStreamH);
  auto modeStr = TimeStretchModeToString(info.mode);
  nlohmann::json timeStretchJson = {
      {"TimeStretchMode", modeStr},
  };

  A_Time keyTime;
  A_Time durationTime;
  float frameRate = AEHelper::GetFrameRateFromItem(itemH);
  keyTime.scale = static_cast<A_u_long>(frameRate);
  keyTime.value = static_cast<A_long>(info.start);
  durationTime.scale = static_cast<A_u_long>(frameRate);
  durationTime.value = static_cast<A_long>(info.duration);

  bool success = AddNewMarkerToStream(markerStreamH, timeStretchJson.dump(), keyTime, durationTime);
  AEHelper::DeleteStream(markerStreamH);
  return success;
}

void Marker::ExportTimeStretch(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                               const AEGP_ItemH& itemH) {
  if (file == nullptr || itemH == nullptr) {
    return;
  }

  const auto& suites = AEHelper::GetSuites();

  A_Time durationTime = {};
  suites->ItemSuite6()->AEGP_GetItemDuration(itemH, &durationTime);
  auto compositionDuration = ExportTime(durationTime, &session);

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
void Marker::ExportImageLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                      const AEGP_ItemH& itemH) {
  if (!file || file->images.empty()) {
    return;
  }

  const auto imageCount = static_cast<int>(file->images.size());
  std::vector<bool> isEditable(imageCount, true);
  bool hasNonEditableImage = false;

  std::unordered_map<pag::ID, int> imageToIndex;
  imageToIndex.reserve(imageCount);
  for (int i = 0; i < imageCount; ++i) {
    if (file->images[i]) {
      imageToIndex[file->images[i]->id] = i;
    }
  }

  for (const auto& pair : session.imageLayerHList) {
    AEGP_LayerH layerH = pair.first;
    if (PlaceImageLayer::IsNoReplaceImageLayer(session, itemH, layerH)) {
      pag::ID imageId = AEHelper::GetItemIdFromLayer(layerH);

      auto it = imageToIndex.find(imageId);
      if (it != imageToIndex.end()) {
        int index = it->second;
        isEditable[index] = false;
        hasNonEditableImage = true;
      }
    }
  }

  if (hasNonEditableImage) {
    file->editableImages = new std::vector<int>();
    for (int i = 0; i < imageCount; ++i) {
      if (isEditable[i]) {
        file->editableImages->push_back(i);
      }
    }
  }
}
bool Marker::IsTextLayerNonReplaceable(const pag::TextLayer* layer, const AEGP_ItemH& itemH,
                                       PAGExportSession& session) {
  if (!layer) {
    return false;
  }

  auto keyString = GetKeyStringWithId("noReplace", layer->id);
  if (FindMarkerFromComposition(itemH, keyString).has_value()) {
    return true;
  }

  AEGP_LayerH layerH = session.getLayerHById(layer->id);
  if (layerH != nullptr) {
    if (FindMarkerFromLayer(layerH, "noReplace").has_value()) {
      return true;
    }
  }

  return false;
}
std::unordered_map<pag::ID, pag::PAGScaleMode> Marker::CollectImageFillModes(
    PAGExportSession& session, const AEGP_ItemH& itemH) {
  std::unordered_map<pag::ID, pag::PAGScaleMode> imageModes;

  auto processLayer = [&](pag::Layer* layer) {
    if (layer->type() != pag::LayerType::Image) {
      return;
    }
    auto imageLayer = static_cast<pag::ImageLayer*>(layer);
    if (!imageLayer || !imageLayer->imageBytes) {
      return;
    }

    auto layerH = session.getLayerHById(layer->id);
    auto mode = PlaceImageLayer::GetImageFillMode(layerH);
    if (mode == ImageFillMode::NoFind) {
      mode = ImageFillMode::Default;
    }

    auto imageId = imageLayer->imageBytes->id;
    auto it = imageModes.find(imageId);
    auto scaleMode = static_cast<pag::PAGScaleMode>(mode);

    if (it == imageModes.end()) {
      imageModes[imageId] = scaleMode;
    } else if (scaleMode != it->second) {
      session.pushWarning(AlertInfoType::TextPathAnimator);
    }
  };

  PAGFilterUtil::TraversalLayers(
      session, session.compositions.back(), pag::LayerType::Image,
      [&](PAGExportSession&, pag::Layer* layer, void*) { processLayer(layer); }, nullptr);

  for (auto& pair : imageModes) {
    auto compMode = PlaceImageLayer::GetFillModeFromComposition(itemH, pair.first);
    if (compMode != ImageFillMode::NotFind) {
      pair.second = static_cast<pag::PAGScaleMode>(compMode);
    }
  }

  return imageModes;
}

std::vector<int>* Marker::CreateDefaultIndices(int count) {
  if (count <= 0) {
    return nullptr;
  }

  auto indices = new std::vector<int>(count);
  std::iota(indices->begin(), indices->end(), 0);
  return indices;
}

std::optional<std::string> Marker::ConvertJsonValueToString(const nlohmann::json& jsonValue) {
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

void Marker::ExportTextLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                     const AEGP_ItemH& mainCompItemH) {
  if (!file || file->numTexts() == 0) {
    return;
  }

  const auto textCount = file->numTexts();
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

void Marker::ExportLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
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

void Marker::ExportImageFillMode(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                 const AEGP_ItemH& itemH) {
  if (!file) {
    return;
  }

  auto imageModes = CollectImageFillModes(session, itemH);
  if (imageModes.empty()) {
    return;
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
        auto it = imageModes.find(image->id);
        if (it != imageModes.end()) {
          file->imageScaleModes->push_back(it->second);
        }
      }
    }
  }
}

std::optional<nlohmann::json> Marker::FindKeyFromComment(const std::string& comment,
                                                         const std::string& key) {
  nlohmann::json json = nlohmann::json::parse(comment, nullptr, false);
  if (json.is_discarded() || !json.is_object() || !json.contains(key)) {
    return std::nullopt;
  }

  return json[key];
}
std::optional<nlohmann::json> Marker::FindMarkerFromStream(const AEGP_StreamRefH& markerStreamH,
                                                           const std::string& key) {
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  const auto& suites = AEHelper::GetSuites();
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
std::optional<nlohmann::json> Marker::FindMarkerFromComposition(const AEGP_ItemH& itemH,
                                                                const std::string& key) {
  if (itemH == nullptr) {
    return std::nullopt;
  }

  AEGP_StreamRefH markerStreamH = AEHelper::GetMarkerStreamFromItem(itemH);
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStreamH, key);
  AEHelper::DeleteStream(markerStreamH);
  return result;
}

std::optional<nlohmann::json> Marker::FindMarkerFromLayer(const AEGP_LayerH& layerH,
                                                          const std::string& key) {
  if (layerH == nullptr) {
    return std::nullopt;
  }
  AEGP_StreamRefH markerStreamH = AEHelper::GetMarkerStreamFromLayer(layerH);
  if (markerStreamH == nullptr) {
    return std::nullopt;
  }

  auto result = FindMarkerFromStream(markerStreamH, key);
  AEHelper::DeleteStream(markerStreamH);
  return result;
}

bool Marker::DeleteMarkerByIndex(const AEGP_StreamRefH markerStreamH, int index) {
  if (markerStreamH == nullptr) {
    return false;
  }

  const auto& suites = AEHelper::GetSuites();
  A_Err err = suites->KeyframeSuite4()->AEGP_DeleteKeyframe(markerStreamH, index);
  return err == A_Err_NONE;
}

void Marker::DeleteMarkerFromStream(const AEGP_StreamRefH markerStreamH, const std::string& key) {
  if (markerStreamH == nullptr || key.empty()) {
    return;
  }

  const auto& suites = AEHelper::GetSuites();
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

void Marker::DeleteMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key) {
  if (itemH == nullptr) {
    return;
  }

  auto markerStreamH = AEHelper::GetMarkerStreamFromItem(itemH);
  DeleteMarkerFromStream(markerStreamH, key);
  AEHelper::DeleteStream(markerStreamH);
}

void Marker::DeleteMarkerFromLayer(const AEGP_LayerH& layerH, const std::string& key) {
  if (layerH == nullptr) {
    return;
  }

  auto markerStreamH = AEHelper::GetMarkerStreamFromLayer(layerH);
  DeleteMarkerFromStream(markerStreamH, key);
  AEHelper::DeleteStream(markerStreamH);
}

void Marker::AddMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& key,
                               const nlohmann::json& value) {
  if (markerStreamH == nullptr || key.empty()) {
    return;
  }

  const auto& suites = AEHelper::GetSuites();
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
      SetMarkerComment(markerStreamH, index, json.dump(), {});
      return;
    }
  }

  nlohmann::json newJson = {{key, value}};
  A_Time time = {0, 1};
  AddNewMarkerToStream(markerStreamH, newJson.dump(), time, {});
}

void Marker::AddMarkerToComposition(const AEGP_ItemH& itemH, const std::string& key,
                                    const nlohmann::json& value) {
  if (itemH == nullptr) {
    return;
  }
  auto markerStreamH = AEHelper::GetMarkerStreamFromItem(itemH);
  if (markerStreamH == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStreamH, key);
  AddMarkerToStream(markerStreamH, key, value);
  AEHelper::DeleteStream(markerStreamH);
}

void Marker::AddMarkerToLayer(const AEGP_LayerH& layerH, const std::string& key,
                              const nlohmann::json& value) {
  if (layerH == nullptr) {
    return;
  }
  auto markerStreamH = AEHelper::GetMarkerStreamFromLayer(layerH);
  if (markerStreamH == nullptr) {
    return;
  }

  DeleteMarkerFromStream(markerStreamH, key);
  AddMarkerToStream(markerStreamH, key, value);
  AEHelper::DeleteStream(markerStreamH);
}

std::optional<TimeStretchInfo> Marker::GetTimeStretchInfo(const AEGP_ItemH& itemH) {
  const auto& suites = AEHelper::GetSuites();
  auto pluginID = AEHelper::GetPluginID();

  AEGP_StreamRefH markerStreamH = AEHelper::GetMarkerStreamFromItem(itemH);
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
      const auto& lowerComment = StringHelper::ToLowerCase(comment);
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

      float frameRate = AEHelper::GetFrameRateFromItem(itemH);

      TimeStretchInfo result = {*foundMode, ExportTime(time, frameRate),
                                ExportTime(duration, frameRate)};

      AEHelper::DeleteStream(markerStreamH);
      return result;
    }
  }

  AEHelper::DeleteStream(markerStreamH);
  return std::nullopt;
}

}  // namespace exporter