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

#include "FilterModifier.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"

namespace pag {
std::shared_ptr<FilterModifier> FilterModifier::Make(PAGLayer* pagLayer) {
  if (pagLayer == nullptr || pagLayer->contentFrame < 0 ||
      pagLayer->contentFrame >= pagLayer->layer->duration) {
    return nullptr;
  }
  auto modifier = Make(pagLayer->layer, pagLayer->layer->startTime + pagLayer->contentFrame);
  return modifier;
}

std::shared_ptr<FilterModifier> FilterModifier::Make(Layer* layer, Frame layerFrame) {
  if (layer == nullptr || layerFrame < layer->startTime ||
      layerFrame >= layer->startTime + layer->duration ||
      (!layer->motionBlur && layer->effects.empty() && layer->layerStyles.empty() && !layer->transform3D)) {
    return nullptr;
  }
  auto modifier = std::shared_ptr<FilterModifier>(new FilterModifier());
  modifier->layer = layer;
  modifier->layerFrame = layerFrame;
  return modifier;
}

void FilterModifier::applyToBounds(tgfx::Rect* bounds) const {
  FilterRenderer::MeasureFilterBounds(bounds, this);
}

void FilterModifier::applyToGraphic(tgfx::Canvas* canvas, RenderCache* cache,
                                    std::shared_ptr<Graphic> graphic) const {
  FilterRenderer::DrawWithFilter(canvas, cache, this, graphic);
}

void FilterModifier::prepare(RenderCache* renderCache) const {
  for (auto* effect : layer->effects) {
    if (effect->type() != EffectType::DisplacementMap) {
      continue;
    }
    auto mapEffect = static_cast<DisplacementMapEffect*>(effect);
    auto mapLayer = static_cast<PreComposeLayer*>(mapEffect->displacementMapLayer);
    auto content = static_cast<GraphicContent*>(LayerCache::Get(mapLayer)->getContent(layerFrame));
    content->graphic->prepare(renderCache);
  }
}
}  // namespace pag
