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
#include "AECompositionPanel.h"
#include <iostream>
#include "AEUtils.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "AEResource.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/utils/ImageData/ImageRawData.h"

// 以本合成为总合成，判断该id所属子合成是否不全部包含在BMP合成内
// 如果都在bmp内，返回false; 如果有不在，返回true
//（如果该id被引用多次，则需全部在BMP内才算在，有一个不在BMP就算不在）
// 另：如果该id所属子合成就是本总合成，返回true。
static bool CheckNotAllInBmp(pagexporter::Context& context, pag::Composition* mainComposition, uint32_t idChild) {
  if (mainComposition->id == idChild) {
    return true;
  }
  auto itemH = context.getCompItemHById(mainComposition->id);
  if (itemH == nullptr) {
    return false;
  }
  if (!AEUtils::CheckIsBmp(itemH)) {
    for (auto layer : static_cast<pag::VectorComposition*>(mainComposition)->layers) {
      if (layer->type() == pag::LayerType::PreCompose) {
        auto composition = static_cast<pag::PreComposeLayer*>(layer)->composition;
        bool flag = CheckNotAllInBmp(context, composition, idChild);
        if (flag) {
          return true;
        }
      }
    }
  }
  return false;
}

void AECompositionPanel::pushToList(pag::ID compId) {
  auto itemH = context.getCompItemHById(compId);
  if (itemH == nullptr) {
    return;
  }
  for (auto& node : list) {
    if (node.itemH == itemH) {
      return; // 排重
    }
  }
  AEComposition aeComp;
  aeComp.itemH = itemH;
  aeComp.name = AEUtils::GetItemName(aeComp.itemH);
  aeComp.isBmp = AEUtils::CheckIsBmp(aeComp.name);
  auto mainComposition = compositions[compositions.size() - 1];
  aeComp.notAllInBmp = CheckNotAllInBmp(context, mainComposition, compId);
  list.push_back(aeComp);
}

void AECompositionPanel::pushCompositions(pag::Composition* composition) {
  pushToList(composition->id);
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      auto preCompLayer = static_cast<pag::PreComposeLayer*>(layer);
      pushCompositions(preCompLayer->composition);
    }
  }
}

AECompositionPanel::AECompositionPanel(AEGP_ItemH mainCompItemH) : context("./tmp.pag") {
  context.sequenceSuffix = "_bmpComplexSuffixesF0324DA"; // 一个不会被命中的名字后缀，以便都用矢量导出
  context.enableRunScript = false;
  context.enableFontFile = false;
  context.enableAudio = false;
  context.enableForceStaticBMP = false;
  auto compHandle = AEUtils::GetCompFromItem(mainCompItemH);
  compositionName = AEUtils::GetCompName(compHandle);
  ExportComposition(&context, mainCompItemH);
  pag::Codec::InstallReferences(context.compositions);
  compositions = PAGExporter::ResortCompositions(context);
  if (compositions.size() == 0) {
    return;
  }

  auto mainComposition = compositions[compositions.size() - 1];
  pushCompositions(mainComposition);
  frameRate = mainComposition->frameRate;
  duration = static_cast<int>(mainComposition->duration);
}

AECompositionPanel::~AECompositionPanel() { releaseImageRgbaData(); }

void AECompositionPanel::releaseImageRgbaData() {
  previewImage.data.reset();
  previewImage.width = 0;
  previewImage.height = 0;
  previewImage.data = nullptr;
}

void AECompositionPanel::refresh() {
  if (compositions.size() == 0) {
    return;
  }
  auto mainComposition = compositions[compositions.size() - 1];
  for (auto& aeComp : list) {
    aeComp.name = AEUtils::GetItemName(aeComp.itemH);
    aeComp.isBmp = AEUtils::CheckIsBmp(aeComp.name);
    auto id = AEUtils::GetItemId(aeComp.itemH);
    aeComp.notAllInBmp = CheckNotAllInBmp(context, mainComposition, id);
  }
}

std::shared_ptr<uint8_t> AECompositionPanel::getPreviewImage(int frame, int width, int height) {
  previewImage.resize(width, height);
  if (list.size() == 0) {
    return nullptr;
  }

  A_long orgWidth = 0;
  A_long orgHeight = 0;
  uint8_t* rgbaBytes = nullptr;
  A_u_long rowBytes = 0;
  GetRenderImageFromComposition(rgbaBytes, rowBytes, orgWidth, orgHeight,
                                context, list[0].itemH, frameRate, frame);
  if (orgWidth > 0 && orgHeight > 0 && rgbaBytes != nullptr) {
    previewImage.scaleTo(rgbaBytes, rowBytes, orgWidth, orgHeight);
    delete[] rgbaBytes;
  }
  return previewImage.data;
}

std::vector<AEComposition>* AECompositionPanel::getList() {
  return &list;
}

std::vector<AEComposition>* AECompositionPanel::getListWithRefresh() {
  refresh();
  return &list;
}

void AECompositionPanel::printList() {
  if (list.size() == 0) {
    return;
  }
  refresh();
  for (int i = 0; i < list.size(); i++) {
    printf("%s%s   -[%s][%s]\n",
           (i == 0) ? "\n" : "    ",
           list[i].name.c_str(),
           list[i].isBmp ? "BMP" : "VEC",
           list[i].notAllInBmp ? "置亮" : "置灰");
  }
}

void PrintAllCompositionPanel(std::shared_ptr<AEResource> root) {
  if (root == nullptr) {
    return;
  }
  if (root->type == AEResourceType::Composition) {
    AECompositionPanel panel(root->itemH);
    panel.printList();
  } else if (root->type == AEResourceType::Folder) {
    for (auto& child : root->childs) {
      PrintAllCompositionPanel(child);
    }
  }
}
