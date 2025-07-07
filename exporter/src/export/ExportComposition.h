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

pag::CompositionType GetCompositionType(const std::shared_ptr<PAGExportSession>& session,
                                        AEGP_CompH const& compHandle);

bool IsStaticComposition(std::shared_ptr<PAGExportSession> session, AEGP_CompH const& compHandle);

std::shared_ptr<pag::Composition> ExportComposition(std::shared_ptr<PAGExportSession> session,
                                                    const AEGP_ItemH& itemH);

std::shared_ptr<pag::VideoComposition> ExportVideoComposition(
    std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compHandle);

std::shared_ptr<pag::BitmapComposition> ExportBitmapComposition(
    std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compHandle);

std::shared_ptr<pag::VectorComposition> ExportVectorComposition(
    std::shared_ptr<PAGExportSession> session, const AEGP_CompH& compHandle);

}  // namespace exporter
