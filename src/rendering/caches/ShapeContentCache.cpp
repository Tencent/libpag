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

#include <rendering/graphics/Shape.h>
#include <base/utils/TGFXCast.h>
#include "ShapeContentCache.h"
#include "rendering/renderers/ShapeRenderer.h"

namespace pag {
ShapeContentCache::ShapeContentCache(ShapeLayer* layer) : ContentCache(layer) {
}

void ShapeContentCache::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  for (auto& element : static_cast<ShapeLayer*>(layer)->contents) {
    element->excludeVaryingRanges(timeRanges);
  }
}

GraphicContent* ShapeContentCache::createContent(Frame layerFrame) const {
  auto shapeLayer = static_cast<ShapeLayer*>(layer);
  auto pagColor = shapeLayer->getTintColor();

  std::shared_ptr<Graphic> graphic;
  if (pagColor == nullptr) {
    graphic = RenderShapes(layer->uniqueID, static_cast<ShapeLayer *>(layer)->contents, layerFrame);
  } else {
    auto tgfxColor = ToTGFX(*shapeLayer->getTintColor());
    graphic = RenderShapes(layer->uniqueID, static_cast<ShapeLayer *>(layer)->contents, layerFrame,
                           &tgfxColor);
  };

  return new GraphicContent(graphic);
}
}  // namespace pag
