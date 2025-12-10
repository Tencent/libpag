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
#include <QImage>
#include <memory>
#include <string>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "rendering/caches/FrameCache.h"

namespace exporter {

struct AEVersion {
  static int32_t MajorVerison;
  static int32_t MinorVersion;
};

std::shared_ptr<AEGP_SuiteHandler> GetSuites();

std::string GetAEVersion();

AEGP_PluginID GetPluginID();

QString GetProjectName();

QString GetProjectPath();

AEGP_ItemH GetActiveCompositionItem();

void GetRenderFrame(uint8* rgbaBytes, A_u_long srcStride, A_u_long dstStride, A_long width,
                    A_long height, AEGP_RenderOptionsH& renderOptions);

void GetRenderFrameSize(AEGP_RenderOptionsH& renderOptions, A_u_long& stride, A_long& width,
                        A_long& height);

void GetLayerRenderFrame(uint8* rgbaBytes, A_u_long srcStride, A_u_long dstStride, A_long width,
                         A_long height, AEGP_LayerRenderOptionsH& renderOptions);

void GetLayerRenderFrameSize(AEGP_LayerRenderOptionsH& renderOptions, A_u_long& stride,
                             A_long& width, A_long& height);

std::vector<char> GetProjectFileBytes();

void SetRenderTime(const AEGP_RenderOptionsH& renderOptions, float frameRate, pag::Frame frame);

void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id);

std::string RunScript(const std::string& scriptText);

void RunScriptPreWarm();

bool CheckAeVersion();

void SetMajorVersion(int32_t majorVersion);

void setMinorVersion(int32_t minorVersion);

void RegisterTextDocumentScript();

uint32_t GetLayerID(const AEGP_LayerH& layerHandle);

uint32_t GetLayerItemID(const AEGP_LayerH& layerHandle);

std::string GetLayerName(const AEGP_LayerH& layerHandle);

AEGP_ItemH GetLayerItemH(const AEGP_LayerH& layerHandle);

pag::Ratio GetLayerStretch(const AEGP_LayerH& layerHandle);

pag::Frame GetLayerStartTime(const AEGP_LayerH& layerHandle, float frameRate);

pag::Frame GetLayerDuration(const AEGP_LayerH& layerHandle, float frameRate);

AEGP_LayerFlags GetLayerFlags(const AEGP_LayerH& layerHandle);

AEGP_LayerH GetLayerParentLayerH(const AEGP_LayerH& layerHandle);

pag::BlendMode GetLayerBlendMode(const AEGP_LayerH& layerHandle);

AEGP_LayerH GetLayerTrackMatteLayerH(const AEGP_LayerH& layerHandle);

pag::TrackMatteType GetLayerTrackMatteType(const AEGP_LayerH& layerHandle);

A_long GetLayerEffectNum(const AEGP_LayerH& layerHandle);

void SelectLayer(const AEGP_ItemH& itemHandle, const AEGP_LayerH& layerHandle);

uint32_t GetItemID(const AEGP_ItemH& itemHandle);

uint32_t GetItemParentID(const AEGP_ItemH& itemHandle);

std::string GetItemName(const AEGP_ItemH& itemHandle);

AEGP_CompH GetItemCompH(const AEGP_ItemH& itemHandle);

float GetItemFrameRate(const AEGP_ItemH& itemHandle);

pag::Frame GetItemDuration(const AEGP_ItemH& itemHandle);

QSize GetItemDimensions(const AEGP_ItemH& itemHandle);

QImage GetCompositionFrameImage(const AEGP_ItemH& itemHandle, pag::Frame frame);

void SetItemName(const AEGP_ItemH& itemHandle, const std::string& name);

void SelectItem(const AEGP_ItemH& itemHandle);

std::string GetCompName(const AEGP_CompH& compositionHandle);

AEGP_ItemH GetCompItemH(const AEGP_CompH& compositionHandle);

pag::Color GetCompBackgroundColor(const AEGP_CompH& compositionHandle);

bool IsStaticComposition(const AEGP_CompH& compositionHandle);

std::string GetStreamMatchName(const AEGP_StreamRefH& streamHandle);

bool IsStreamHidden(const AEGP_StreamRefH& streamHandle);

bool IsStreamActive(const AEGP_StreamRefH& streamHandle);

void DeleteStream(AEGP_StreamRefH streamHandle);

AEGP_StreamRefH GetLayerMarkerStream(const AEGP_LayerH& layerHandle);

AEGP_StreamRefH GetItemMarkerStream(const AEGP_ItemH& itemHandle);

AEGP_StreamRefH GetCompositionMarkerStream(const AEGP_CompH& compositionHandle);

}  // namespace exporter
