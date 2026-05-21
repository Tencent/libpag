/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/PAGFile.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/DisplayList.h"

namespace pagx {

// Concrete definition of the opaque LayerTreeStorage forward-declared in PAGFile.h. Kept in an
// internal header so PAGFile.cpp and unit tests can access the runtime PAGLayerTree without
// pulling tgfx layer types into the public API.
struct PAGFile::LayerTreeStorage {
  PAGLayerTree tree = {};
  tgfx::DisplayList displayList;
  bool rootAttached = false;
};

}  // namespace pagx
