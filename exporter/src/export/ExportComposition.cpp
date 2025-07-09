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

bool IsStaticComposition(std::shared_ptr<PAGExportSession> /*session*/,
                         AEGP_CompH const& /*compHandle*/) {
  bool isSame = true;
  return isSame;
}

std::shared_ptr<pag::Composition> ExportComposition(std::shared_ptr<PAGExportSession> session,
                                                    const AEGP_ItemH& itemH) {
  auto id = AEHelper::GetItemId(itemH);
  session->compItemHList[id] = itemH;

  ScopedAssign<pag::ID> arCI(session->curCompId, id);

  auto compHandle = AEHelper::GetCompFromItem(itemH);

  auto compositionType = GetCompositionType(session, compHandle);
  if (session->bEarlyExit) {
    return nullptr;
  }

  switch (compositionType) {
    case pag::CompositionType::Video:
      return ExportVideoComposition(session, compHandle);
    case pag::CompositionType::Bitmap:
      return ExportBitmapComposition(session, compHandle);
    default:
      return ExportVectorComposition(session, compHandle);
  }
}

pag::CompositionType GetCompositionType(const std::shared_ptr<PAGExportSession>& session,
                                        AEGP_CompH const& compHandle) {
  auto compName = AEHelper::GetCompName(compHandle);
  compName = StringHelper::ToLowerCase(compName);

  const auto suffixLength = session->configParam.sequenceSuffix.length();

  if (compName.length() < suffixLength) {
    return pag::CompositionType::Vector;
  }

  const auto suffixPos = compName.length() - suffixLength;
  if (compName.find(session->configParam.sequenceSuffix, suffixPos) == std::string::npos) {
    return pag::CompositionType::Vector;
  }

  if (session->configParam.sequenceType == pag::CompositionType::Video &&
      session->configParam.exportTagLevel >= static_cast<uint16_t>(pag::TagCode::VideoSequence)) {
    if (session->enableForceStaticBMP && IsStaticComposition(session, compHandle)) {
      // If the precomposition is entirely static, use a bitmap sequence frame.
      return pag::CompositionType::Bitmap;
    }
    return pag::CompositionType::Video;
  }

  if (session->configParam.exportTagLevel >= static_cast<uint16_t>(pag::TagCode::BitmapSequence)) {
    return pag::CompositionType::Bitmap;
  }

  return pag::CompositionType::Vector;
}

std::shared_ptr<pag::VideoComposition> ExportVideoComposition(
    std::shared_ptr<PAGExportSession> /*session*/, const AEGP_CompH& /*compHandle*/) {
  return nullptr;
}

std::shared_ptr<pag::BitmapComposition> ExportBitmapComposition(
    std::shared_ptr<PAGExportSession> /*session*/, const AEGP_CompH& /*compHandle*/) {
  return nullptr;
}

std::shared_ptr<pag::VectorComposition> ExportVectorComposition(
    std::shared_ptr<PAGExportSession> /*session*/, const AEGP_CompH& /*compHandle*/) {
  return nullptr;
}
}  // namespace exporter