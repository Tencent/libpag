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
  pag::PAGTimeStretchMode mode = pag::PAGTimeStretchMode::None;
  pag::Frame start = 0;
  pag::Frame duration = 0;
};

std::vector<pag::Marker*> ExportMarkers(std::shared_ptr<PAGExportSession> session,
                                        const AEGP_LayerH& layerHandle);

void ParseMarkers(pag::Layer* layer);

std::optional<TimeStretchInfo> GetTimeStretchInfo(const AEGP_ItemH& itemHandle);

std::optional<std::string> GetMarkerComment(const AEGP_StreamRefH& markerStream, int index);

pag::PAGScaleMode GetImageFillMode(const AEGP_ItemH& itemHandle, pag::ID imageID);

bool GetLayerEditable(const AEGP_ItemH& itemHandle, pag::ID id);

std::string GetCompositionStoragePath(const AEGP_ItemH& itemHandle);

std::string GetMarkerFromComposition(const AEGP_ItemH& itemHandle, const std::string& key);

void DeleteAllTimeStretchInfo(const AEGP_ItemH& itemHandle);

bool SetMarkerComment(const AEGP_StreamRefH& markerStream, int index, const std::string& comment,
                      A_Time durationTime);

void SetImageFillMode(pag::PAGScaleMode mode, const AEGP_ItemH& itemHandle, pag::ID imageID);

void SetLayerEditable(bool isEditable, const AEGP_ItemH& itemHandle, pag::ID id);

void SetCompositionStoragePath(const std::string& path, const AEGP_ItemH& itemHandle);

bool AddNewMarkerToStream(const AEGP_StreamRefH& markerStream, const std::string& comment,
                          A_Time time = {0, 1}, A_Time durationTime = {0, 0}, A_long index = 0);

void SetTimeStretchInfo(const TimeStretchInfo& info, const AEGP_ItemH& itemHandle);

void ExportTimeStretch(std::shared_ptr<pag::File> file, std::shared_ptr<PAGExportSession> session,
                       const AEGP_ItemH& itemHandle);

void ExportImageLayerEditable(std::shared_ptr<pag::File> file,
                              std::shared_ptr<PAGExportSession> session,
                              const AEGP_ItemH& itemHandle);

void ExportTextLayerEditable(std::shared_ptr<pag::File> file,
                             std::shared_ptr<PAGExportSession> session,
                             const AEGP_ItemH& itemHandle);

void ExportLayerEditable(std::shared_ptr<pag::File> file, std::shared_ptr<PAGExportSession> session,
                         const AEGP_ItemH& itemHandle);

void ExportImageFillMode(std::shared_ptr<pag::File> file, const AEGP_ItemH& itemHandle);

bool IsTextLayerNonReplaceable(const pag::TextLayer* layer, const AEGP_ItemH& itemHandle,
                               std::shared_ptr<PAGExportSession> session);

bool IsImageLayerNonReplaceable(const pag::ImageLayer* layer, const AEGP_ItemH& itemHandle,
                                std::shared_ptr<PAGExportSession> session);

void AddMarkerToComposition(const AEGP_ItemH& itemHandle, const std::string& key,
                            const nlohmann::json& value, A_Time time = {0, 1},
                            A_Time durationTime = {0, 0});

void AddMarkerToLayer(const AEGP_LayerH& itemHandle, const std::string& key,
                      const nlohmann::json& value);

void AddMarkerToStream(const AEGP_StreamRefH& markerStream, const std::string& key,
                       const nlohmann::json& value, A_Time time = {0, 1},
                       A_Time durationTime = {0, 0});

}  // namespace exporter
