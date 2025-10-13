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

#pragma once
#include "pag/file.h"
#include "pag/pag.h"
#include "utils/PAGExportSession.h"

namespace exporter {

pag::CompositionType GetCompositionType(std::shared_ptr<PAGExportSession> session,
                                        const AEGP_CompH& compH);

void GetCompositionAttributes(std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compH,
                              pag::Composition* composition);

void ExportComposition(std::shared_ptr<PAGExportSession> session, const AEGP_ItemH& itemH);

void ExportVideoComposition(std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compH);

void ExportBitmapComposition(std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compH);

void ExportVectorComposition(std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compH);

void ExportBitmapCompositionActually(std::shared_ptr<PAGExportSession> session,
                                     pag::BitmapComposition* composition, float factor);

void ExportVideoCompositionActually(std::shared_ptr<PAGExportSession> session,
                                    std::vector<pag::Composition*>& compositions,
                                    pag::VideoComposition* composition, float factor);

}  // namespace exporter
