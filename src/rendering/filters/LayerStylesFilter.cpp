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
#include "layerstyle/LayerStyleFilter.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"
#include "tgfx/core/Surface.h"

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

bool LayerStylesFilter::initialize(tgfx::Context*) {
  return true;
}

void LayerStylesFilter::update(const FilterList* list, const tgfx::Point& extraScale) {
  filterList = list;
  filterScale = extraScale;
}

void LayerStylesFilter::draw(tgfx::Context* context, const FilterSource* source,
                             const FilterTarget* target) {
  tgfx::BackendTexture backendTexture = {source->sampler, source->width, source->height};
  auto sourceMatrix = ToMatrix(source->textureMatrix);
  auto origin = tgfx::ImageOrigin::TopLeft;
  if (!sourceMatrix.isIdentity()) {
    origin = tgfx::ImageOrigin::BottomLeft;
  }
  auto sourceImage = tgfx::Image::MakeFrom(context, backendTexture, origin);
  if (sourceImage == nullptr) {
    return;
  }

  tgfx::BackendRenderTarget renderTarget = {target->frameBuffer, target->width, target->height};
  auto surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::TopLeft);
  if (surface == nullptr) {
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->concat(ToMatrix(target));
  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->drawPosition() == LayerStylePosition::Blow) {
      auto filter = LayerStyleFilter::Make(layerStyle);
      if (filter) {
        filter->update(filterList->layerFrame, filterScale, source->scale);
        filter->draw(canvas, sourceImage);
      }
    }
  }

  // The filter above source will draw the source with blend.
  bool drawSource = true;
  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->drawPosition() == LayerStylePosition::Above) {
      auto filter = LayerStyleFilter::Make(layerStyle);
      if (filter) {
        filter->update(filterList->layerFrame, filterScale, source->scale);
        filter->draw(canvas, sourceImage);
        drawSource = false;
      }
    }
  }

  if (drawSource) {
    canvas->drawImage(sourceImage);
  }
}
}  // namespace pag
