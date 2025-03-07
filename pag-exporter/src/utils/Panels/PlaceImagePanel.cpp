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
#include "PlaceImagePanel.h"
#include "AEUtils.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/utils/PAGFilterUtil.h"
#include "src/exports/Effect/Effect.h"
#include "src/exports/Stream/StreamValue.h"
#include "src/exports/AEMarker/AEMarker.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/utils/ImageData/ImageRawData.h"

static AEGP_InstalledEffectKey GetKeyImageFillRule() {
  static AEGP_InstalledEffectKey key = 0;
  if (key == 0) {
    key = AEUtils::GetInstalledEffectKey("Image Fill Rule");
  }
  return key;
}

using ProcessEffectFP = bool (*)(pagexporter::Context& context, AEGP_EffectRefH effectH, void* ctx);

static void TraversalEffects(pagexporter::Context& context,
                             AEGP_LayerH layerH,
                             AEGP_InstalledEffectKey expectKey,
                             ProcessEffectFP processEffectFP,
                             void* ctx) {
  auto& suites = context.suites;
  auto pluginID = context.pluginID;
  int numEffects = AEUtils::GetLayerNumEffects(layerH);
  bool bExit = false;
  for (int i = 0; i < numEffects && !bExit; i++) {
    AEGP_EffectRefH effectH;
    suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(pluginID, layerH, i, &effectH);
    AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
    if (expectKey != AEGP_InstalledEffectKey_NONE) {
      suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectH, &key);
    }
    if (key == expectKey) {
      bExit = processEffectFP(context, effectH, ctx);
    }
    suites.EffectSuite4()->AEGP_DisposeEffect(effectH);
  }
}

static AEGP_StreamRefH GetEffectStream(AEGP_EffectRefH effectH) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  A_long numParams;
  suites.StreamSuite4()->AEGP_GetEffectNumParamStreams(effectH, &numParams);
  if (numParams < 2) {
    return nullptr;
  }
  AEGP_EffectFlags effectFlags;
  suites.EffectSuite4()->AEGP_GetEffectFlags(effectH, &effectFlags);
  if ((effectFlags & AEGP_EffectFlags_ACTIVE) == 0) {
    return nullptr;
  }
  AEGP_StreamRefH firstStream;
  suites.StreamSuite4()->AEGP_GetNewEffectStreamByIndex(pluginID, effectH, 1, &firstStream);
  AEGP_StreamRefH effectStream;
  suites.DynamicStreamSuite4()->AEGP_GetNewParentStreamRef(pluginID, firstStream, &effectStream);
  suites.StreamSuite4()->AEGP_DisposeStream(firstStream);
  return effectStream;
}

static void DisposeStream(AEGP_StreamRefH stream) {
  auto& suites = SUITES();
  suites.StreamSuite4()->AEGP_DisposeStream(stream);
}

static bool ProcessEffectGetImageFillMode(pagexporter::Context& context, AEGP_EffectRefH effectH, void* ctx) {
  AEGP_StreamRefH effectStream = GetEffectStream(effectH);
  if (effectStream == nullptr) {
    return false;
  }

  auto pFillMode = static_cast<ImageFillMode*>(ctx);
  auto scaleMode = ExportValue(&context, effectStream, 1, Parsers::ScaleMode);
  *pFillMode = static_cast<ImageFillMode>(scaleMode);

  DisposeStream(effectStream);
  return true;
}

/*
static ImageFillMode GetImageFillMode(pagexporter::Context& context, std::vector<AEGP_LayerH>& layerHs) {
  ImageFillMode fillMode = ImageFillMode::NotFind;
  for (auto layerH : layerHs) {
    TraversalEffects(context, layerH, GetKeyImageFillRule(), &ProcessEffectGetImageFillMode, &fillMode);
    if (fillMode != ImageFillMode::NotFind) {
      break;
    }
  }
  return fillMode;
}//*/

static ImageFillMode StringToImageFillMode(const char* str) {
  if (strcmp(str, "None") == 0) {
    return ImageFillMode::None;
  } else if (strcmp(str, "Stretch") == 0) {
    return ImageFillMode::Stretch;
  } else if (strcmp(str, "LetterBox") == 0) {
    return ImageFillMode::LetterBox;
  } else if (strcmp(str, "Zoom") == 0) {
    return ImageFillMode::Zoom;
  } else {
    return ImageFillMode::LetterBox;
  }
}

static std::string ImageFillModeToString(ImageFillMode mode) {
  switch (mode) {
    case ImageFillMode::None:
      return "None";
      break;
    case ImageFillMode::Stretch:
      return "Stretch";
      break;
    case ImageFillMode::Zoom:
      return "Zoom";
      break;
    default:
      return "LetterBox";
      break;
  }
}

ImageFillMode PlaceImageLayer::GetFillModeFromLayer(const AEGP_LayerH& layerH) {
  auto fillMode = ImageFillMode::NotFind;
  cJSON* cjson = nullptr;
  if (AEMarker::FindMarkerFromLayer(layerH, "ImageFillMode", &cjson)) {
    fillMode = StringToImageFillMode(cjson->valuestring);
    cJSON_Delete(cjson);
  }
  return fillMode;
}

static ImageFillMode GetFillModeFromLayers(const std::vector<AEGP_LayerH>& layerHs) {
  auto fillMode = ImageFillMode::NotFind;
  for (auto layerH : layerHs) {
    fillMode = PlaceImageLayer::GetFillModeFromLayer(layerH);
    if (fillMode != ImageFillMode::NotFind) {
      break;
    }
  }
  return fillMode;
}

ImageFillMode PlaceImageLayer::GetFillModeFromComposition(const AEGP_ItemH& mainCompItemH, pag::ID imageId) {
  auto fillMode = ImageFillMode::NotFind;
  cJSON* cjson = nullptr;
  auto keyString = AEMarker::GetKeyStringWithId("ImageFillMode", imageId);
  if (AEMarker::FindMarkerFromComposition(mainCompItemH, keyString, &cjson)) {
    fillMode = StringToImageFillMode(cjson->valuestring);
  }
  return fillMode;
}

ImageFillMode PlaceImageLayer::getFillMode() {
  fillMode = GetFillModeFromComposition(mainPanel->mainCompItemH, AEUtils::GetItemId(imageH));
  if (fillMode == ImageFillMode::NotFind) {
    fillMode = GetFillModeFromLayers(layerHs);
  }
  if (fillMode == ImageFillMode::NotFind) {
    fillMode = ImageFillMode::Default;
  }
  return fillMode;
}

static bool ProcessEffectSetImageFillMode(pagexporter::Context& context, AEGP_EffectRefH effectH, void* ctx) {
  AEGP_StreamRefH effectStream = GetEffectStream(effectH);
  if (effectStream == nullptr) {
    return false;
  }

  auto& suites = context.suites;
  auto pluginID = context.pluginID;
  auto pFillMode = static_cast<ImageFillMode*>(ctx);
  AEGP_StreamRefH fillModeStream;
  suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefByIndex(pluginID, effectStream,
                                                            1, &fillModeStream);

  static A_Time time = {0, 100};
  AEGP_StreamValue2 streamValue = {};
  suites.StreamSuite4()->AEGP_GetNewStreamValue(pluginID, fillModeStream,
                                                AEGP_LTimeMode_CompTime, &time,
                                                TRUE, &streamValue);
  streamValue.val.one_d = static_cast<double>(*pFillMode) + 1.0;
  suites.StreamSuite4()->AEGP_SetStreamValue(pluginID, fillModeStream, &streamValue);
  suites.StreamSuite4()->AEGP_DisposeStream(fillModeStream);

  DisposeStream(effectStream);
  return false;
}

bool PlaceImageLayer::IsNoReplaceImageLayer(pagexporter::Context& context, const AEGP_ItemH& mainCompItemH,
                                            const AEGP_LayerH& layerH) {
  auto isVideo = context.isVideoReplaceLayer(layerH);
  if (isVideo) {
    return false;
  }
  auto keyString = AEMarker::GetKeyStringWithId("noReplace", AEUtils::GetItemIdFromLayer(layerH));
  if (AEMarker::FindMarkerFromComposition(mainCompItemH, keyString)) {
    return true;
  }
  if (AEMarker::FindMarkerFromLayer(layerH, "noReplace")) {
    return true;
  }
  return false;
}

void PlaceImageLayer::getFillResourceType() {
  if (isVideo) {
    resourceType = FillResourceType::Replace;
    editableFlag = true;
    return;
  }

  auto keyString = AEMarker::GetKeyStringWithId("noReplace", AEUtils::GetItemId(imageH));
  if (AEMarker::FindMarkerFromComposition(mainPanel->mainCompItemH, keyString)) {
    resourceType = FillResourceType::NoReplace;
    editableFlag = false;
    return;
  }

  bool finded = false;
  for (auto layerH : layerHs) {
    if (AEMarker::FindMarkerFromLayer(layerH, "noReplace")) {
      finded = true;
      break;
    }
  }
  resourceType = finded ? FillResourceType::NoReplace : FillResourceType::Replace;
  editableFlag = (resourceType == FillResourceType::Replace);
}

std::shared_ptr<uint8_t> PlaceImageLayer::refreshThumbnail(int width, int height) {
  if (width == thumbnail.width && height == thumbnail.height) {
    return thumbnail.data;
  }
  thumbnail.resize(width, height);

  A_long orgWidth = 0;
  A_long orgHeight = 0;
  uint8_t* rgbaBytes = nullptr;
  A_u_long rowBytes = 0;
  GetRenderImageFromLayer(rgbaBytes, rowBytes, orgWidth, orgHeight,
                          context, layerHs[0], isVideo);
  if (orgWidth > 0 && orgHeight > 0 && rgbaBytes != nullptr) {
    thumbnail.scaleTo(rgbaBytes, rowBytes, orgWidth, orgHeight);
    delete[] rgbaBytes;
  }
  return thumbnail.data;
}

PlaceImageLayer::PlaceImageLayer(pagexporter::Context& context, PlaceImagePanel* mainPanel, AEGP_LayerH layerH)
    : context(context), mainPanel(mainPanel) {
  layerHs.push_back(layerH);
  imageH = AEUtils::GetItemFromLayer(layerH);
  name = AEUtils::GetItemName(imageH);
  isVideo = context.isVideoReplaceLayer(layerH);
  fillMode = getFillMode();
  getFillResourceType();
  refreshThumbnail();
}

PlaceImageLayer::~PlaceImageLayer() {
}

void PlaceImageLayer::addImageFillRule() {
  auto& suites = context.suites;
  auto pluginID = context.pluginID;
  auto key = GetKeyImageFillRule();
  for (auto layerH : layerHs) {
    AEGP_EffectRefH effectH;
    suites.EffectSuite4()->AEGP_ApplyEffect(pluginID, layerH, key, &effectH);
    if (fillMode != ImageFillMode::Default) {
      ProcessEffectSetImageFillMode(context, effectH, &fillMode);
    }
    suites.EffectSuite4()->AEGP_DisposeEffect(effectH);
  }
}

void PlaceImageLayer::deleteImageFillRule() {
  auto& suites = context.suites;
  auto pluginID = context.pluginID;
  auto expectKey = GetKeyImageFillRule();
  for (auto layerH : layerHs) {
    int numEffects = AEUtils::GetLayerNumEffects(layerHs[0]);
    for (int i = numEffects - 1; i >= 0; i--) {
      AEGP_EffectRefH effectH;
      suites.EffectSuite4()->AEGP_GetLayerEffectByIndex(pluginID, layerH, i, &effectH);
      AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
      suites.EffectSuite4()->AEGP_GetInstalledKeyFromLayerEffect(effectH, &key);
      if (key == expectKey) {
        suites.EffectSuite4()->AEGP_DeleteLayerEffect(effectH);
      }
      suites.EffectSuite4()->AEGP_DisposeEffect(effectH);
    }
  }
}

void PlaceImageLayer::setFillMode(ImageFillMode mode) {
  for (auto layerH : layerHs) {
    AEMarker::DeleteMarkersFromLayer(layerH, "ImageFillMode");
  }
  auto keyString = AEMarker::GetKeyStringWithId("ImageFillMode", AEUtils::GetItemId(imageH));
  if (mode == ImageFillMode::NotFind || mode == ImageFillMode::Default) {
    AEMarker::DeleteMarkersFromComposition(mainPanel->mainCompItemH, keyString);
    fillMode = ImageFillMode::Default;
  } else {
    auto modeString = ImageFillModeToString(mode);
    auto node = AEMarker::KeyValueToJsonNode(keyString, modeString);
    AEMarker::AddMarkerToComposition(mainPanel->mainCompItemH, node);
    fillMode = mode;
  }
}

void PlaceImageLayer::setResourceType(FillResourceType newType) {
  if (isVideo) {
    return;  // 视频占位图一定可以替换为视频，所以不能更改类型
  }
  if (resourceType == newType) {
    return;
  }
  for (auto layerH : layerHs) {
    AEMarker::DeleteMarkersFromLayer(layerH, "noReplace");
  }
  auto keyString = AEMarker::GetKeyStringWithId("noReplace", AEUtils::GetItemId(imageH));
  if (resourceType == FillResourceType::NoReplace) {
    AEMarker::DeleteMarkersFromComposition(mainPanel->mainCompItemH, keyString);
  }
  if (newType == FillResourceType::NoReplace) {
    auto node = AEMarker::KeyValueToJsonNode(keyString, 1);
    AEMarker::AddMarkerToComposition(mainPanel->mainCompItemH, node);
  }
  resourceType = newType;
  editableFlag = (resourceType == FillResourceType::Replace);
}

static void ProcessLayerPlaceImageLayer(pagexporter::Context& context, pag::Layer*, void* ctx) {
  auto mainPanel = static_cast<PlaceImagePanel*>(ctx);
  auto layerH = context.getLayerHById(context.curLayerId);
  auto imageH = AEUtils::GetItemFromLayer(layerH);
  for (auto& node : mainPanel->list) {
    if (node.imageH == imageH) {
      for (auto handle : node.layerHs) {
        if (handle == layerH) {
          return;
        }
      }
      node.layerHs.push_back(layerH);
      return;
    }
  }
  mainPanel->list.push_back(PlaceImageLayer(context, mainPanel, layerH));
}

PlaceImagePanel::PlaceImagePanel(AEGP_ItemH mainCompItemH) : mainCompItemH(mainCompItemH) {
  refresh();
}

PlaceImagePanel::~PlaceImagePanel() {
  if (context != nullptr) {
    delete context;
    context = nullptr;
  }
}

void PlaceImagePanel::refresh() {
  list.clear();
  if (context != nullptr) {
    delete context;
    context = nullptr;
  }
  context = new pagexporter::Context("./tmp.pag");
  context->enableRunScript = false;
  context->enableFontFile = false;
  context->enableAudio = false;
  context->enableForceStaticBMP = false;
  ExportComposition(context, mainCompItemH);
  pag::Codec::InstallReferences(context->compositions);
  auto compositions = PAGExporter::ResortCompositions(*context);

  if (compositions.size() == 0) {
    return;
  }

  auto images = PAGExporter::ResortImages(*context, compositions);

  auto mainComposition = compositions[compositions.size() - 1];
  PAGFilterUtil::TraversalLayers(*context, mainComposition, pag::LayerType::Image,
                                 &ProcessLayerPlaceImageLayer, this);
}

// 获取占位图层列表。
std::vector<PlaceImageLayer>* PlaceImagePanel::getList() {
  return &list;
}

// 设置替换素材格式
void PlaceImagePanel::setResourceType(int index, FillResourceType type) {
  if (index >= static_cast<int>(list.size())) {
    return;
  }
  list[index].setResourceType(type);
}
