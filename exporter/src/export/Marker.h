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
#include <nlohmann/adl_serializer.hpp>
#include <string>
#include "pag/types.h"
#include "utils/AEHelper.h"
#include "utils/PAGExportSession.h"

namespace exporter {

struct TimeStretchInfo {
  pag::PAGTimeStretchMode mode;
  pag::Frame start;
  pag::Frame duration;
};

class StreamWrapper {
 public:
  explicit StreamWrapper(AEGP_StreamRefH streamH) : streamHandle(streamH) {
  }
  ~StreamWrapper() {
    if (streamHandle) {
      AEHelper::DeleteStream(streamHandle);
    }
  }

  StreamWrapper(const StreamWrapper&) = delete;
  StreamWrapper& operator=(const StreamWrapper&) = delete;
  AEGP_StreamRefH GetStreamHandle() const {
    return streamHandle;
  }

  AEGP_StreamRefH get() const {
    return streamHandle;
  }

 private:
  AEGP_StreamRefH streamHandle;
};

class MemHandleWrapper {
 public:
  explicit MemHandleWrapper(AEGP_MemHandle handle, const AEGP_SuiteHandler* suite)
      : memHandle(handle), suitesH(suite) {
  }
  ~MemHandleWrapper() {
    if (memHandle && suitesH) {
      suitesH->MemorySuite1()->AEGP_FreeMemHandle(memHandle);
    }
  }
  MemHandleWrapper(const MemHandleWrapper&) = delete;
  MemHandleWrapper& operator=(const MemHandleWrapper&) = delete;

 private:
  AEGP_MemHandle memHandle;
  const AEGP_SuiteHandler* suitesH;
};

class StreamValueWrapper {
 public:
  explicit StreamValueWrapper(AEGP_StreamValue2* value,
                              const std::shared_ptr<AEGP_SuiteHandler>* suites)
      : streamValue(value), suitesH(suites) {
  }
  ~StreamValueWrapper() {
    if (streamValue && suitesH) {
      if (streamValue && suitesH && *suitesH) {
        (*suitesH)->StreamSuite3()->AEGP_DisposeStreamValue(streamValue);
      }
    }
  }

 private:
  AEGP_StreamValue2* streamValue;
  const std::shared_ptr<AEGP_SuiteHandler>* suitesH;
};

class Marker {
 public:
  static std::string GetKeyStringWithId(std::string key, pag::ID id) {
    return key + "-" + std::to_string(id);
  }

  static std::vector<std::unique_ptr<pag::Marker>> ExportMarkers(PAGExportSession* session,
                                                                 const AEGP_LayerH& layerH);
  static void ParseMarkers(pag::Layer* layer);

  static std::optional<TimeStretchInfo> GetTimeStretchInfo(const AEGP_ItemH& itemH);

  static std::optional<std::string> GetMarkerComment(const AEGP_StreamRefH& markerStreamH,
                                                     int index);

  std::string GetMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key);

  static void DeleteAllTimeStretchInfo(const AEGP_StreamRefH& markerStreamH);

  static bool SetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index,
                               const std::string& comment, A_Time durationTime);

  static bool AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& comment,
                                   A_Time time, A_Time durationTime);

  static bool SetTimeStretchInfo(const TimeStretchInfo& info, const AEGP_ItemH& itemH);

  static void ExportTimeStretch(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                const AEGP_ItemH& itemH);

  static void ExportImageLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                       const AEGP_ItemH& itemH);

  static void ExportTextLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                      const AEGP_ItemH& itemH);

  static void ExportLayerEditable(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                  const AEGP_ItemH& itemH);

  static void ExportImageFillMode(std::shared_ptr<pag::File>& file, PAGExportSession& session,
                                  const AEGP_ItemH& itemH);

  static bool IsTextLayerNonReplaceable(const pag::TextLayer* layer, const AEGP_ItemH& itemH,
                                        PAGExportSession& session);

  static std::unordered_map<pag::ID, pag::PAGScaleMode> CollectImageFillModes(
      PAGExportSession& session, const AEGP_ItemH& itemH);

  static void AddMarkerToComposition(const AEGP_ItemH& itemH, const std::string& key,
                                     const nlohmann::json& value);

  static void AddMarkerToLayer(const AEGP_LayerH& layerH, const std::string& key,
                               const nlohmann::json& value);

  static void AddMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& key,
                                const nlohmann::json& value);

 private:
  static std::vector<int>* CreateDefaultIndices(int count);

  static std::optional<std::string> ConvertJsonValueToString(const nlohmann::json& jsonValue);

  static std::optional<nlohmann::json> FindKeyFromComment(const std::string& comment,
                                                          const std::string& key);

  static std::optional<nlohmann::json> FindMarkerFromStream(const AEGP_StreamRefH& markerStreamH,
                                                            const std::string& key);

  static std::optional<nlohmann::json> FindMarkerFromComposition(const AEGP_ItemH& itemH,
                                                                 const std::string& key);

  static std::optional<nlohmann::json> FindMarkerFromLayer(const AEGP_LayerH& layerH,
                                                           const std::string& key);

  static bool DeleteMarkerByIndex(const AEGP_StreamRefH markerStreamH, int index);

  static void DeleteMarkerFromStream(const AEGP_StreamRefH markerStreamH, const std::string& key);

  static void DeleteMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key);

  static void DeleteMarkerFromLayer(const AEGP_LayerH& layerH, const std::string& key);
};

}  // namespace exporter
