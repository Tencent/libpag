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

#include "LayerStylesFilter.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/opengl/GLRenderTarget.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "rendering/filters/GradientOverlayFilter.h"

namespace pag {
void LayerStylesFilter::TransformBounds(tgfx::Rect* bounds, const FilterList* filterList) {
  for (auto& layerStyle : filterList->layerStyles) {
    auto rect = *bounds;
    layerStyle->transformBounds(ToPAG(&rect), ToPAG(filterList->layerStyleScale),
                                filterList->layerFrame);
    rect.roundOut();
    bounds->join(rect);
  }
}

LayerStylesFilter::LayerStylesFilter(RenderCache* renderCache) : renderCache(renderCache) {
  drawFilter = new LayerFilter();
}

LayerStylesFilter::~LayerStylesFilter() {
  delete drawFilter;
}

bool LayerStylesFilter::initialize(tgfx::Context* context) {
  return drawFilter->initialize(context);
}

void LayerStylesFilter::update(const FilterList* list, const tgfx::Rect& inputBounds,
                               const tgfx::Rect& outputBounds, const tgfx::Point& extraScale) {
  filterList = list;
  contentBounds = inputBounds;
  transformedBounds = outputBounds;
  filterScale = extraScale;
}

void LayerStylesFilter::draw(tgfx::Context* context, const FilterSource* source,
                             const FilterTarget* target) {
  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->drawPosition() == LayerStylePosition::Blow) {
      auto filter = renderCache->getFilterCache(layerStyle);
      if (filter) {
        filter->update(filterList->layerFrame, contentBounds, transformedBounds, filterScale);
        filter->draw(context, source, target);
      }
    }
  }

  drawFilter->update(filterList->layerFrame, contentBounds, contentBounds, filterScale);
  drawFilter->draw(context, source, target);

  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->drawPosition() == LayerStylePosition::Above) {
      auto filter = renderCache->getFilterCache(layerStyle);
      if (!filter) {
        continue;
      }
      if (layerStyle->type() == LayerStyleType::GradientOverlay) {
        auto gradientOverlayFilter = static_cast<GradientOverlayFilter*>(filter);
        auto newSurface = tgfx::Surface::Make(context, target->width, target->height, false, 1);
        auto newTarget = ToFilterTarget(newSurface.get(), tgfx::Matrix::I());
        newTarget->vertexMatrix = target->vertexMatrix;
        filter->update(filterList->layerFrame, contentBounds, transformedBounds, filterScale);
        filter->draw(context, source, newTarget.get());
        
        auto renderTarget = tgfx::GLRenderTarget::MakeFrom(context, target->frameBuffer,
                                                           target->width, target->height, tgfx::ImageOrigin::TopLeft);
        auto targetSurface = tgfx::Surface::MakeFrom(renderTarget);
        auto targetCanvas = targetSurface->getCanvas();
        targetCanvas->setBlendMode(gradientOverlayFilter->getBlendMode());
        targetCanvas->drawTexture(newSurface->getTexture().get());
      } else {
        filter->update(filterList->layerFrame, contentBounds, transformedBounds, filterScale);
        filter->draw(context, source, target);
      }
    }
  }
}
}  // namespace pag
