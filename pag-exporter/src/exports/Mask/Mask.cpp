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
#include "Mask.h"
#include "src/exports/Stream/StreamProperty.h"

static pag::MaskData* ExportMask(pagexporter::Context* context, const AEGP_MaskRefH& maskHandle) {
  auto mask = new pag::MaskData();
  auto& suites = context->suites;
  A_long id;
  RECORD_ERROR(suites.MaskSuite6()->AEGP_GetMaskID(maskHandle, &id));
  mask->id = static_cast<pag::ID>(id);
  A_Boolean inverted;
  RECORD_ERROR(suites.MaskSuite6()->AEGP_GetMaskInvert(maskHandle, &inverted));
  mask->inverted = static_cast<bool>(inverted);
  PF_MaskMode mode;
  RECORD_ERROR(suites.MaskSuite6()->AEGP_GetMaskMode(maskHandle, &mode));
  mask->maskMode = ExportMaskMode(mode);

  mask->maskPath = ExportProperty(context, maskHandle, AEGP_MaskStream_OUTLINE, Parsers::Path);
  mask->maskFeather = ExportProperty(context, maskHandle, AEGP_MaskStream_FEATHER, Parsers::Point);
  mask->maskOpacity = ExportProperty(context, maskHandle, AEGP_MaskStream_OPACITY, Parsers::Opacity0_100);
  mask->maskExpansion = ExportProperty(context, maskHandle, AEGP_MaskStream_EXPANSION, Parsers::Float);
  return mask;
}

std::vector<pag::MaskData*> ExportMasks(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  std::vector<pag::MaskData*> masks;
  auto& suites = context->suites;
  A_long numMasks = 0;
  RECORD_ERROR(suites.MaskSuite6()->AEGP_GetLayerNumMasks(layerHandle, &numMasks));
  for (int i = 0; i < numMasks; i++) {
    AEGP_MaskRefH maskHandle;
    RECORD_ERROR(suites.MaskSuite6()->AEGP_GetLayerMaskByIndex(layerHandle, i, &maskHandle));
    auto mask = ExportMask(context, maskHandle);
    masks.push_back(mask);
    RECORD_ERROR(suites.MaskSuite6()->AEGP_DisposeMask(maskHandle));
  }
  return masks;
}
