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
#include <pag/pag.h>
#include <unordered_map>
#include "src/utils/AEHelper.h"
#include "utils/PAGExportSession.h"

namespace exporter {

enum class ExportLayerType {
  Unknown,
  Null,
  Solid,
  Text,
  Shape,
  Image,
  PreCompose,
  Video,
  Audio,
  Camera
};

ExportLayerType GetLayerType(const AEGP_LayerH& layerHandle);

std::vector<pag::Layer*> ExportLayers(std::shared_ptr<PAGExportSession> session,
                                      const AEGP_CompH& compHandle);

}  // namespace exporter
