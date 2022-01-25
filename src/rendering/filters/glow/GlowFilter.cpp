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

#include "GlowFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
GlowFilter::GlowFilter(Effect* effect) : effect(effect) {
    blurFilterH = new GlowBlurFilter(BlurDirection::Horizontal);
    blurFilterV = new GlowBlurFilter(BlurDirection::Vertical);
    targetFilter = new GlowMergeFilter(effect);
}

GlowFilter::~GlowFilter() {
    delete blurFilterH;
    delete blurFilterV;
    delete targetFilter;
}

bool GlowFilter::initialize(Context* context) {
    if (!blurFilterH->initialize(context)) {
        return false;
    }
    if (!blurFilterV->initialize(context)) {
        return false;
    }
    if (!targetFilter->initialize(context)) {
        return false;
    }
    return true;
}

void GlowFilter::update(Frame frame, const Rect& contentBounds, const Rect& transformedBounds,
                        const Point& filterScale) {
    LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);

    auto glowEffect = static_cast<GlowEffect*>(effect);
    auto glowRadius = glowEffect->glowRadius->getValueAt(layerFrame);
    resizeRatio = 1.0f - glowRadius / 1500.f;
    auto blurBounds = contentBounds;
    blurBounds.scale(resizeRatio, resizeRatio);
    blurBounds.offsetTo(contentBounds.left, contentBounds.top);
    blurBounds.roundOut();
    blurFilterV->update(frame, contentBounds, blurBounds, filterScale);
    blurFilterH->update(frame, contentBounds, blurBounds, filterScale);
    targetFilter->update(frame, contentBounds, transformedBounds, filterScale);
}

bool GlowFilter::checkBuffer(Context* context, int blurWidth, int blurHeight) {
    if (blurFilterBufferH == nullptr || blurFilterBufferH->width() != blurWidth ||
            blurFilterBufferH->height() != blurHeight) {
        blurFilterBufferH = FilterBuffer::Make(context, blurWidth, blurHeight);
    }
    if (blurFilterBufferH == nullptr) {
        return false;
    }
    if (blurFilterBufferV == nullptr || blurFilterBufferV->width() != blurWidth ||
            blurFilterBufferV->height() != blurHeight) {
        blurFilterBufferV = FilterBuffer::Make(context, blurWidth, blurHeight);
    }
    if (blurFilterBufferV == nullptr) {
        blurFilterBufferH = nullptr;
        return false;
    }
    return true;
}

void GlowFilter::draw(Context* context, const FilterSource* source, const FilterTarget* target) {
    if (source == nullptr || target == nullptr) {
        LOGE("GlowFilter::draw() can not draw filter");
        return;
    }
    auto blurWidth = static_cast<int>(ceilf(source->width * resizeRatio));
    auto blurHeight = static_cast<int>(ceilf(source->height * resizeRatio));

    if (!checkBuffer(context, blurWidth, blurHeight)) {
        return;
    }
    auto gl = GLContext::Unwrap(context);
    blurFilterBufferH->clearColor(gl);
    blurFilterBufferV->clearColor(gl);

    auto targetH = blurFilterBufferH->toFilterTarget(Matrix::I());
    blurFilterH->updateOffset(1.0f / blurWidth);
    blurFilterH->draw(context, source, targetH.get());

    auto sourceV = blurFilterBufferH->toFilterSource(source->scale);
    auto targetV = blurFilterBufferV->toFilterTarget(Matrix::I());
    blurFilterV->updateOffset(1.0f / blurHeight);
    blurFilterV->draw(context, sourceV.get(), targetV.get());

    targetFilter->updateTexture(blurFilterBufferV->getTexture().id);
    targetFilter->draw(context, source, target);
}
}  // namespace pag
