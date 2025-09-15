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

#include "ExportComposition.h"
#include "utils/AEHelper.h"
#include "utils/ScopedHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

pag::CompositionType GetCompositionType(const std::shared_ptr<PAGExportSession>& session,
                                        const AEGP_CompH& compH) {
  auto compName = AEHelper::GetCompName(compH);
  compName = StringHelper::ToLowerCase(compName);

  if (compName.length() < StringHelper::CompositionBmpSuffix.size()) {
    return pag::CompositionType::Vector;
  }

  if (!StringHelper::IsEndWidthSuffix(compName, StringHelper::CompositionBmpSuffix)) {
    return pag::CompositionType::Vector;
  }

  if (session->configParam.sequenceType == pag::CompositionType::Video &&
      session->configParam.isTagCodeEnable(pag::TagCode::VideoSequence)) {
    if (session->exportStaticCompAsBmp && AEHelper::IsStaticComposition(compH)) {
      return pag::CompositionType::Bitmap;
    }
    return pag::CompositionType::Video;
  }

  if (session->configParam.isTagCodeEnable(pag::TagCode::BitmapSequence)) {
    return pag::CompositionType::Bitmap;
  }

  return pag::CompositionType::Vector;
}

void GetCompositionAttributes(const std::shared_ptr<PAGExportSession>& session,
                              const AEGP_CompH& compH, pag::Composition* composition) {
  AEGP_ItemH itemH = AEHelper::GetCompItemH(compH);
  composition->id = AEHelper::GetItemID(itemH);
  composition->duration = AEHelper::GetItemDuration(itemH);
  composition->backgroundColor = AEHelper::GetCompBackgroundColor(compH);
  if (session->frameRate == -1) {
    composition->frameRate = AEHelper::GetItemFrameRate(itemH);
    session->frameRate = composition->frameRate;
  } else {
    composition->frameRate = session->frameRate;
  }
  auto size = AEHelper::GetItemDimensions(itemH);
  composition->width = size.width();
  composition->height = size.height();

  if (session->itemHMap.find(composition->id) == session->itemHMap.end()) {
    session->itemHMap[composition->id] = itemH;
  }

  if (composition->type() != pag::CompositionType::Vector) {
    auto frames =
        static_cast<double>(composition->duration * session->frameRate / composition->frameRate);
    session->progressModel.addTotalFrame(frames);
  }
}

void ExportComposition(const std::shared_ptr<PAGExportSession>& session, const AEGP_ItemH& itemH) {
  auto id = AEHelper::GetItemID(itemH);
  session->itemHMap[id] = itemH;

  ScopedAssign<pag::ID> compID(session->compID, id);

  if (session->stopExport) {
    return;
  }

  auto compH = AEHelper::GetItemCompH(itemH);
  auto compositionType = GetCompositionType(session, compH);
  switch (compositionType) {
    case pag::CompositionType::Video:
      ExportVideoComposition(session, compH);
      return;
    case pag::CompositionType::Bitmap:
      ExportBitmapComposition(session, compH);
      return;
    case pag::CompositionType::Vector:
      ExportVectorComposition(session, compH);
      return;
    default:
      return;
  }
}

void ExportVideoComposition(const std::shared_ptr<PAGExportSession>& session,
                            const AEGP_CompH& compH) {
  auto* composition = new pag::VideoComposition();
  GetCompositionAttributes(session, compH, composition);
  session->compositions.push_back(composition);
}

void ExportBitmapComposition(const std::shared_ptr<PAGExportSession>& session,
                             const AEGP_CompH& compH) {
  auto* composition = new pag::BitmapComposition();
  GetCompositionAttributes(session, compH, composition);
  session->compositions.push_back(composition);
}

void ExportVectorComposition(const std::shared_ptr<PAGExportSession>&, const AEGP_CompH&) {
}

void ExportBitmapCompositionActually(const std::shared_ptr<PAGExportSession>&,
                                     pag::BitmapComposition*, float) {
}

void ExportVideoCompositionActually(const std::shared_ptr<PAGExportSession>&,
                                    std::vector<pag::Composition*>&, pag::VideoComposition*,
                                    float) {
}

}  // namespace exporter