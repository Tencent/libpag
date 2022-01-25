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
#include "rendering/caches/RenderCache.h"
#include "rendering/renderers/FilterRenderer.h"

namespace pag {
void LayerStylesFilter::TransformBounds(Rect* bounds, const FilterList* filterList) {
    for (auto& layerStyle : filterList->layerStyles) {
        auto rect = *bounds;
        layerStyle->transformBounds(&rect, filterList->layerStyleScale, filterList->layerFrame);
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

bool LayerStylesFilter::initialize(Context* context) {
    return drawFilter->initialize(context);
}

void LayerStylesFilter::update(const FilterList* list, const Rect& inputBounds,
                               const Rect& outputBounds, const Point& extraScale) {
    filterList = list;
    contentBounds = inputBounds;
    transformedBounds = outputBounds;
    filterScale = extraScale;
}

void LayerStylesFilter::draw(Context* context, const FilterSource* source,
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
            if (filter) {
                filter->update(filterList->layerFrame, contentBounds, transformedBounds, filterScale);
                filter->draw(context, source, target);
            }
        }
    }
}
}  // namespace pag
