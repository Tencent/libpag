/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

// Basic types and enums
#include "pagx/model/types/Enums.h"
#include "pagx/model/types/Types.h"

// Base classes
#include "pagx/model/nodes/Node.h"
#include "pagx/model/nodes/VectorElement.h"

// Color sources
#include "pagx/model/nodes/ColorSource.h"

// Layer styles and filters
#include "pagx/model/nodes/LayerFilter.h"
#include "pagx/model/nodes/LayerStyle.h"

// Vector elements
#include "pagx/model/nodes/Geometry.h"
#include "pagx/model/nodes/Group.h"
#include "pagx/model/nodes/Painter.h"
#include "pagx/model/nodes/Repeater.h"
#include "pagx/model/nodes/ShapeModifier.h"
#include "pagx/model/nodes/TextModifier.h"

// Resources and Layer
#include "pagx/model/nodes/Layer.h"
#include "pagx/model/nodes/Resource.h"

namespace pagx {

// Implementation of Composition::clone (requires Layer to be fully defined)
inline std::unique_ptr<Node> Composition::clone() const {
  auto node = std::make_unique<Composition>();
  node->id = id;
  node->width = width;
  node->height = height;
  for (const auto& layer : layers) {
    node->layers.push_back(
        std::unique_ptr<Layer>(static_cast<Layer*>(layer->clone().release())));
  }
  return node;
}

}  // namespace pagx
