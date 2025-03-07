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
#ifndef AEMARKER_H
#define AEMARKER_H

#include "src/cJSON/cJSON.h"
#include "src/exports/PAGDataTypes.h"

class AEMarker {
 public:
  static std::string GetKeyStringWithId(const std::string& key, const pag::ID id) {
    return key + "-" + std::to_string(id);
  }
  static void ExportMarkers(const pagexporter::Context* context, const AEGP_LayerH& layerHandle,
                            std::vector<pag::Marker*>& markers);
  static void ParseMarkers(pag::Layer* layer);
  static void PrintMarkers(pag::Composition* composition);

  static void DeleteTimeStretchMarkerOld(const AEGP_ItemH& itemH);
  static bool GetTimeStretchInfoOld(pag::Enum& timeStretchMode, pag::Frame& timeStretchStart,
                                    pag::Frame& timeStretchDuration, const AEGP_ItemH& itemH);
  static void DeleteTimeStretchMarker(const AEGP_ItemH& itemH);
  static bool GetTimeStretchInfo(pag::Enum& timeStretchMode, pag::Frame& timeStretchStart,
                                 pag::Frame& timeStretchDuration, const AEGP_ItemH& itemH);
  static void SetTimeStretchInfo(pag::Enum timeStretchMode, pag::Frame timeStretchStart,
                                 pag::Frame timeStretchDuration, const AEGP_ItemH& itemH);
  static void ExportTimeStretch(const std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                const AEGP_ItemH& itemH);
  static void ExportLayerEditable(const std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                  const AEGP_ItemH& itemH);
  static bool FindMarkerFromLayer(const AEGP_LayerH& layerH, const std::string& key,
                                  cJSON** ppValue = nullptr);
  static bool FindMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key,
                                        cJSON** ppValue = nullptr);
  static void DeleteMarkersFromLayer(const AEGP_LayerH& layerH, const std::string& key);
  static void DeleteMarkersFromComposition(const AEGP_ItemH& itemH, const std::string& key);
  static std::string GetMarkerFromComposition(const AEGP_ItemH& itemH, const std::string& key);

  static void AddMarkerToLayer(const AEGP_LayerH& layerH, cJSON* node);
  static void AddMarkerToComposition(const AEGP_ItemH& itemH, cJSON* node);

  static cJSON* KeyValueToJsonNode(const std::string& key, const std::string& value);
  static cJSON* KeyValueToJsonNode(const std::string& key, int value);
  static void ExportImageFillMode(const std::shared_ptr<pag::File>& file, pagexporter::Context& context,
                                  const AEGP_ItemH& itemH);

 private:
  static void AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, const std::string& comment,
                                   A_Time keyTime = {0, 100}, A_Time durationTime = {-1, 100});
  static void AddNewMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node,
                                   A_Time keyTime = {0, 100}, A_Time durationTime = {-1, 100});
  static void DeleteMarkerByIndex(const AEGP_StreamRefH& markerStreamH, int index);
  static void SetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index, const std::string& comment,
                               A_Time durationTime = {-1, 100});
  static std::string GetMarkerComment(const AEGP_StreamRefH& markerStreamH, int index);
  static bool MergeToMarkerWithKey(const AEGP_StreamRefH& markerStreamH, std::string& key,
                                   cJSON* node);
  static int MergeMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node);
  static void AddMarkerToStream(const AEGP_StreamRefH& markerStreamH, cJSON* node);
  static void DeleteMarkersFromStream(const AEGP_StreamRefH& markerStreamH, const std::string& key);
  static bool FindMarkerFromStream(const AEGP_StreamRefH& markerStreamH, const std::string& key,
                                   cJSON** ppValue = nullptr);
};

#endif  //AEMARKER_H
