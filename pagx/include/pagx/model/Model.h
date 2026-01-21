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
#include "pagx/model/Enums.h"
#include "pagx/model/Types.h"

// Base classes
#include "pagx/model/Node.h"
#include "pagx/model/VectorElement.h"

// Color sources
#include "pagx/model/ColorSource.h"

// Layer styles and filters
#include "pagx/model/LayerFilter.h"
#include "pagx/model/LayerStyle.h"

// Vector elements
#include "pagx/model/Geometry.h"
#include "pagx/model/Group.h"
#include "pagx/model/Painter.h"
#include "pagx/model/Repeater.h"
#include "pagx/model/ShapeModifier.h"
#include "pagx/model/TextModifier.h"

// Resources and Layer
#include "pagx/model/Layer.h"
#include "pagx/model/Resource.h"

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
