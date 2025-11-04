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

#include "Mask.h"
#include "export/stream/StreamProperty.h"
#include "utils/AEDataTypeConverter.h"

namespace exporter {

static pag::MaskData* GetMask(const AEGP_MaskRefH& maskHandle) {
  const auto Suites = GetSuites();
  auto mask = new pag::MaskData();

  A_long id = 0;
  Suites->MaskSuite6()->AEGP_GetMaskID(maskHandle, &id);
  mask->id = static_cast<pag::ID>(id);
  A_Boolean inverted = 0;
  Suites->MaskSuite6()->AEGP_GetMaskInvert(maskHandle, &inverted);
  mask->inverted = static_cast<bool>(inverted);
  PF_MaskMode mode;
  Suites->MaskSuite6()->AEGP_GetMaskMode(maskHandle, &mode);
  mask->maskMode = AEMaskModeToMaskMode(mode);
  mask->maskPath = GetProperty(maskHandle, AEGP_MaskStream_OUTLINE, AEStreamParser::PathParser);
  mask->maskFeather = GetProperty(maskHandle, AEGP_MaskStream_FEATHER, AEStreamParser::PointParser);
  mask->maskOpacity =
      GetProperty(maskHandle, AEGP_MaskStream_OPACITY, AEStreamParser::Opacity0_100Parser);
  mask->maskExpansion =
      GetProperty(maskHandle, AEGP_MaskStream_EXPANSION, AEStreamParser::FloatParser);
  return mask;
}

std::vector<pag::MaskData*> GetMasks(const AEGP_LayerH& layerHandle) {
  const auto Suites = GetSuites();
  std::vector<pag::MaskData*> masks = {};

  A_long numMask = 0;
  Suites->MaskSuite6()->AEGP_GetLayerNumMasks(layerHandle, &numMask);
  for (int i = 0; i < numMask; i++) {
    AEGP_MaskRefH maskHandle = nullptr;
    Suites->MaskSuite6()->AEGP_GetLayerMaskByIndex(layerHandle, i, &maskHandle);
    auto mask = GetMask(maskHandle);
    masks.push_back(mask);
    Suites->MaskSuite6()->AEGP_DisposeMask(maskHandle);
  }
  return masks;
}

}  // namespace exporter
