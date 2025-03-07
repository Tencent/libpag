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
#include "TextLayerEditablePanel.h"
#include "AEUtils.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/utils/PAGFilterUtil.h"
#include "src/exports/AEMarker/AEMarker.h"

PlaceTextLayer::PlaceTextLayer(pagexporter::Context& context, PlaceTextPanel* mainPanel, AEGP_LayerH layerH)
    : layerH(layerH), context(context), mainPanel(mainPanel) {
  name = AEUtils::GetLayerName(layerH);
  getEditable();
}

PlaceTextLayer::~PlaceTextLayer() {
}

bool PlaceTextLayer::getEditable() {
  auto keyString = AEMarker::GetKeyStringWithId("noReplace", AEUtils::GetLayerId(layerH));
  editableFlag = !AEMarker::FindMarkerFromComposition(mainPanel->mainCompItemH, keyString);
  if (editableFlag) {
    editableFlag = !AEMarker::FindMarkerFromLayer(layerH, "noReplace");
  }
  return editableFlag;
}

void PlaceTextLayer::setEditable(bool flag) {
  if (editableFlag == flag) {
    return;
  }
  AEMarker::DeleteMarkersFromLayer(layerH, "noReplace");
  auto keyString = AEMarker::GetKeyStringWithId("noReplace", AEUtils::GetLayerId(layerH));
  if (flag) {
    AEMarker::DeleteMarkersFromComposition(mainPanel->mainCompItemH, keyString);
  } else {
    auto node = AEMarker::KeyValueToJsonNode(keyString, 1);
    AEMarker::AddMarkerToComposition(mainPanel->mainCompItemH, node);
  }
  editableFlag = flag;
}

static void ProcessLayerPlaceTextLayer(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  if (layer->type() != pag::LayerType::Text) {
    return;
  }
  auto mainPanel = static_cast<PlaceTextPanel*>(ctx);
  auto layerH = context.getLayerHById(context.curLayerId);
  for (auto& node : mainPanel->list) {
    if (node.layerH == layerH) {
      return;
    }
  }
  mainPanel->list.push_back(PlaceTextLayer(context, mainPanel, layerH));
}

PlaceTextPanel::PlaceTextPanel(AEGP_ItemH mainCompItemH) : mainCompItemH(mainCompItemH) {
  refresh();
}

PlaceTextPanel::~PlaceTextPanel() {
  if (context != nullptr) {
    delete context;
    context = nullptr;
  }
}

void PlaceTextPanel::refresh() {
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

  auto mainComposition = compositions[compositions.size() - 1];
  PAGFilterUtil::TraversalLayers(*context, mainComposition, pag::LayerType::Text,
                                 &ProcessLayerPlaceTextLayer, this);
}

// 获取占位图层列表。
std::vector<PlaceTextLayer>* PlaceTextPanel::getList() {
  return &list;
}
