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

enum class ImageFillMode {
  None = 0,
  Stretch,
  LetterBox,
  Zoom,
};

struct AEVersion {
  static int32_t MajorVerison;
  static int32_t MinorVersion;
};

/* Common Interface */
std::shared_ptr<AEGP_SuiteHandler> GetSuites();

AEGP_PluginID GetPluginID();

QString GetProjectName();

QString GetProjectPath();

AEGP_ItemH GetActiveCompositionItem();

void GetRenderFrame(uint8*& rgbaBytes, A_u_long& rowBytesLength, A_u_long& stride, A_long& width,
                    A_long& height, AEGP_RenderOptionsH& renderOptions);

void GetLayerRenderFrame(uint8*& rgbaBytes, A_u_long& rowBytesLength, A_u_long& stride,
                         A_long& width, A_long& height, AEGP_LayerRenderOptionsH& renderOptions);

std::vector<char> GetProjectFileBytes();

void SetRenderTime(const AEGP_RenderOptionsH& renderOptions, float frameRate, pag::Frame frame);

void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id);

std::string RunScript(const std::string& scriptText);

void RunScriptPreWarm();

bool CheckAeVersion();

void SetMajorVersion(int32_t majorVersion);

void setMinorVersion(int32_t minorVersion);

void RegisterTextDocumentScript();

/* Layer Interface */
uint32_t GetLayerID(const AEGP_LayerH& layerH);

uint32_t GetLayerItemID(const AEGP_LayerH& layerH);

std::string GetLayerName(const AEGP_LayerH& layerH);

AEGP_ItemH GetLayerItemH(const AEGP_LayerH& layerH);

pag::Ratio GetLayerStretch(const AEGP_LayerH& layerH);

pag::Frame GetLayerStartTime(const AEGP_LayerH& layerH, float frameRate);

pag::Frame GetLayerDuration(const AEGP_LayerH& layerH, float frameRate);

AEGP_LayerFlags GetLayerFlags(const AEGP_LayerH& layerH);

AEGP_LayerH GetLayerParentLayerH(const AEGP_LayerH& layerH);

pag::BlendMode GetLayerBlendMode(const AEGP_LayerH& layerH);

AEGP_LayerH GetLayerTrackMatteLayerH(const AEGP_LayerH& layerH);

pag::TrackMatteType GetLayerTrackMatteType(const AEGP_LayerH& layerH);

A_long GetLayerEffectNum(const AEGP_LayerH& layerH);

void SelectLayer(const AEGP_ItemH& itemH, const AEGP_LayerH& layerH);

/* Item Interface */
uint32_t GetItemID(const AEGP_ItemH& itemH);

uint32_t GetItemParentID(const AEGP_ItemH& item);

std::string GetItemName(const AEGP_ItemH& item);

AEGP_CompH GetItemCompH(const AEGP_ItemH& item);

float GetItemFrameRate(const AEGP_ItemH& item);

pag::Frame GetItemDuration(const AEGP_ItemH& item);

QSize GetItemDimensions(const AEGP_ItemH& itemH);

QImage GetCompositionFrameImage(const AEGP_ItemH& itemH, pag::Frame frame);

void SetItemName(const AEGP_ItemH& item, const std::string& name);

void SelectItem(const AEGP_ItemH& itemH);

/* Composition Interface  */
std::string GetCompName(const AEGP_CompH& compH);

AEGP_ItemH GetCompItemH(const AEGP_CompH& compH);

pag::Color GetCompBackgroundColor(const AEGP_CompH& compH);

bool IsStaticComposition(const AEGP_CompH& compH);

/* Stream Interface */
std::string GetStreamMatchName(const AEGP_StreamRefH& streamH);

bool IsStreamHidden(const AEGP_StreamRefH& streamH);

bool IsStreamActive(const AEGP_StreamRefH& streamH);

void DeleteStream(AEGP_StreamRefH streamRefH);

AEGP_StreamRefH GetLayerMarkerStream(const AEGP_LayerH& layerH);

AEGP_StreamRefH GetItemMarkerStream(const AEGP_ItemH& litemH);

AEGP_StreamRefH GetCompositionMarkerStream(const AEGP_CompH& compH);

}  // namespace exporter
