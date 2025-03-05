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
#include "BitmapComposition.h"
#include "Composition.h"
#include "src/exports/Layer/ExporterLayer.h"
#include "src/exports/Sequence/BitmapSequence.h"
#include <src/utils/StringUtil.h>

pag::BitmapComposition* ExportBitmapComposition(pagexporter::Context* context, const AEGP_CompH& compHandle) {
  auto composition = new pag::BitmapComposition();

  GetCompositionAttributes(context, compHandle, composition);

  // 位图序列帧现在不导出，延后再真正导出。

  context->compositions.push_back(composition);
  return composition;
}

void ReExportBitmapComposition(pagexporter::Context* context, pag::BitmapComposition* composition, float compositionFactor) {
  auto itemH = context->getCompItemHById(composition->id);
  if (itemH != nullptr) {
    for (auto scaleAndFps : context->scaleAndFpsList) {
      ExportBitmapSequence(context, itemH, composition, scaleAndFps.first, scaleAndFps.second, compositionFactor);
    }
  }
}