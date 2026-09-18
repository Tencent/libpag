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
#include <cmath>
#include "ExportLayer.h"
#include "sequence/BitmapSequence.h"
#include "sequence/VideoSequence.h"
#include "utils/AEHelper.h"
#include "utils/ScopedHelper.h"
#include "utils/StringHelper.h"

namespace exporter {

pag::CompositionType GetCompositionType(std::shared_ptr<PAGExportSession> session,
                                        const AEGP_CompH& compositionHandle) {
  auto compName = GetCompName(compositionHandle);
  compName = ToLowerCase(compName);

  if (compName.length() < CompositionBmpSuffix.size()) {
    return pag::CompositionType::Vector;
  }

  if (!IsEndWidthSuffix(compName, CompositionBmpSuffix)) {
    return pag::CompositionType::Vector;
  }

  if (session->configParam.sequenceType == pag::CompositionType::Video &&
      session->configParam.isTagCodeSupport(pag::TagCode::VideoSequence)) {
    if (session->exportStaticCompAsBmp && IsStaticComposition(compositionHandle)) {
      return pag::CompositionType::Bitmap;
    }
    return pag::CompositionType::Video;
  }

  if (session->configParam.isTagCodeSupport(pag::TagCode::BitmapSequence)) {
    return pag::CompositionType::Bitmap;
  }

  return pag::CompositionType::Vector;
}

void GetCompositionAttributes(std::shared_ptr<PAGExportSession> session,
                              const AEGP_CompH& compositionHandle, pag::Composition* composition) {
  AEGP_ItemH itemHandle = GetCompItemH(compositionHandle);
  composition->id = GetItemID(itemHandle);
  auto itemFrameRate = GetItemFrameRate(itemHandle);
  composition->duration = GetItemDuration(itemHandle);
  composition->backgroundColor = GetCompBackgroundColor(compositionHandle);
  if (session->frameRate == -1) {
    session->frameRate = itemFrameRate;
    composition->frameRate = itemFrameRate;
  } else {
    composition->frameRate = session->frameRate;
    // The item is authored at its own native frame rate but exported on the session frame rate,
    // so rescale the duration from native frames to session-rate frames to keep this composition's
    // timeline length consistent with the rest of the export.
    if (itemFrameRate > 0 && itemFrameRate != session->frameRate) {
      composition->duration = static_cast<pag::Frame>(
          std::round(composition->duration * session->frameRate / itemFrameRate));
    }
  }
  auto size = GetItemDimensions(itemHandle);
  composition->width = size.width();
  composition->height = size.height();
  session->itemHandleMap[composition->id] = itemHandle;

  if (composition->type() != pag::CompositionType::Vector) {
    // Sequence encoding walks frames at the sequence frame rate (the smaller of the configured and
    // the native item rate), not the session rate, so count progress steps in that same unit.
    // Using the session-rate duration here would overshoot and finish the progress bar early.
    auto sequenceFrameRate = std::min(session->configParam.frameRate, itemFrameRate);
    auto sequenceFrames = session->frameRate > 0
                              ? static_cast<uint64_t>(std::ceil(
                                    composition->duration * sequenceFrameRate / session->frameRate))
                              : static_cast<uint64_t>(composition->duration);
    session->progressModel.addTotalSteps(sequenceFrames);
  }
}

void ExportComposition(std::shared_ptr<PAGExportSession> session, const AEGP_ItemH& itemHandle) {
  auto id = GetItemID(itemHandle);
  session->itemHandleMap[id] = itemHandle;

  ScopedAssign<pag::ID> compID(session->compID, id);

  if (session->stopExport) {
    return;
  }

  auto compositionHandle = GetItemCompH(itemHandle);
  if (compositionHandle == nullptr) {
    return;
  }
  auto compositionType = GetCompositionType(session, compositionHandle);
  switch (compositionType) {
    case pag::CompositionType::Video:
      ExportVideoComposition(session, compositionHandle);
      break;
    case pag::CompositionType::Bitmap:
      ExportBitmapComposition(session, compositionHandle);
      break;
    case pag::CompositionType::Vector:
      ExportVectorComposition(session, compositionHandle);
      break;
    default:
      break;
  }
}

void ExportVideoComposition(std::shared_ptr<PAGExportSession> session,
                            const AEGP_CompH& compositionHandle) {
  auto composition = new pag::VideoComposition();
  GetCompositionAttributes(session, compositionHandle, composition);
  session->compositions.push_back(composition);
}

void ExportBitmapComposition(std::shared_ptr<PAGExportSession> session,
                             const AEGP_CompH& compositionHandle) {
  auto composition = new pag::BitmapComposition();
  GetCompositionAttributes(session, compositionHandle, composition);
  session->compositions.push_back(composition);
}

void ExportVectorComposition(std::shared_ptr<PAGExportSession> session,
                             const AEGP_CompH& compHandle) {
  auto composition = new pag::VectorComposition();
  GetCompositionAttributes(session, compHandle, composition);
  ScopedAssign<std::vector<pag::Marker*>*> markers(session->audioMarkers,
                                                   &composition->audioMarkers);
  composition->layers = ExportLayers(session, compHandle);
  session->compositions.push_back(composition);

  session->progressModel.addTotalSteps(session->imageBytesList.size());
}

void ExportBitmapComposition(std::shared_ptr<PAGExportSession> session,
                             pag::BitmapComposition* composition, float factor) {
  GetBitmapSequence(std::move(session), composition, factor);
}

void ExportVideoComposition(std::shared_ptr<PAGExportSession> session,
                            std::vector<pag::Composition*>& compositions,
                            pag::VideoComposition* composition, float factor) {
  GetVideoSequence(std::move(session), compositions, composition, factor);
}

}  // namespace exporter
