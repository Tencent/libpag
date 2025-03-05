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
#include "VectorComposition.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/Layer/ExporterLayer.h"
#include "src/utils/CommonMethod.h"
#include "src/utils/StringUtil.h"

pag::VectorComposition* ExportVectorComposition(pagexporter::Context* context,
                                                const AEGP_CompH& compHandle) {
  auto composition = new pag::VectorComposition();

  GetCompositionAttributes(context, compHandle, composition);

  AssignRecover<std::vector<pag::Marker*>*> arAM(context->pAudioMarkers,
                                                 &(composition->audioMarkers));

  composition->layers = ExportLayers(context, compHandle);
  context->compositions.push_back(composition);
  return composition;
}
