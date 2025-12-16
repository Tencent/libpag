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

#include <pag/file.h>
#include "PAGExportSession.h"

namespace exporter {

using LayerHandler = void (*)(std::shared_ptr<exporter::PAGExportSession> session,
                              pag::Layer* layer, void* ctx);

using LayerHandlerWithTime = void (*)(std::shared_ptr<exporter::PAGExportSession> session,
                                      pag::Layer* layer, pag::Frame startTime, void* ctx);

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     pag::Composition* composition, pag::LayerType layerType, pag::Frame startTime,
                     LayerHandlerWithTime handler, void* ctx);

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     pag::Composition* composition, pag::LayerType layerType, LayerHandler handler,
                     void* ctx);

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     std::vector<pag::Composition*>& compositions, pag::LayerType layerType,
                     LayerHandler handler, void* ctx);

}  // namespace exporter
