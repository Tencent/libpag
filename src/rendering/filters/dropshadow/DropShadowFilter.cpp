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

#include "DropShadowFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/FilterDefines.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
DropShadowFilter::DropShadowFilter(DropShadowStyle* layerStyle) : layerStyle(layerStyle) {
  blurEffect = new FastBlurEffect();
  blurEffect->repeatEdgePixels = new Property<bool>;
  blurEffect->blurDimensions = new Property<Enum>;
  blurEffect->blurriness = new Property<float>;
  blurEffect->repeatEdgePixels->value = false;
  blurEffect->blurDimensions->value = BlurDimensionsDirection::All;
  blurFilter = new GaussianBlurFilter(blurEffect);
  spreadFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Normal);
  spreadThickFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Thick);
}

DropShadowFilter::~DropShadowFilter() {
  delete blurEffect;
  delete blurFilter;
  delete spreadFilter;
  delete spreadThickFilter;
}

bool DropShadowFilter::initialize(tgfx::Context* context) {
  if (!blurFilter->initialize(context)) {
    return false;
  }
  if (!spreadFilter->initialize(context)) {
    return false;
  }
  if (!spreadThickFilter->initialize(context)) {
    return false;
  }
  return true;
}

void DropShadowFilter::update(Frame frame, const tgfx::Rect& contentBounds,
                              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale) {
  LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);

  color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  color.alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  spread = layerStyle->spread->getValueAt(layerFrame);
  size = layerStyle->size->getValueAt(layerFrame);
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  auto radians = DegreesToRadians(angle - 180);
  offset = tgfx::Point::Make(cosf(radians) * distance * filterScale.x,
                             -sinf(radians) * distance * filterScale.y);
  expendSize = size / DROPSHADOW_EXPEND_FACTOR;

  filterContentBounds = contentBounds;
  auto filterTransformedBounds = contentBounds;
  filterTransformedBounds.outset(contentBounds.width() * expendSize * filterScale.x,
                                 contentBounds.height() * expendSize * filterScale.y);
  filterNotFullSpreadBounds = filterTransformedBounds;
  filterTransformedBounds.offset(offset.x, offset.y);
  filterTransformedBounds.roundOut();

  if (spread == 0.0f) {
    updateParamModeNotSpread(frame, contentBounds, filterTransformedBounds, filterScale);
  } else if (spread == 1.0f) {
    updateParamModeFullSpread(frame, contentBounds, filterTransformedBounds, filterScale);
  } else {
    updateParamModeNotFullSpread(frame, contentBounds, filterTransformedBounds, filterScale);
  }
}

void DropShadowFilter::updateParamModeNotSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                const tgfx::Rect& transformedBounds,
                                                const tgfx::Point& filterScale) {
  blurEffect->blurriness->value = size * 2.0;
  blurFilter->updateParams(expendSize, offset, true, color);
  blurFilter->update(frame, contentBounds, transformedBounds, filterScale);
}

void DropShadowFilter::updateParamModeNotFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                    const tgfx::Rect& transformedBounds,
                                                    const tgfx::Point& filterScale) {
  if (size < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(frame, contentBounds, filterNotFullSpreadBounds, filterScale);
  } else {
    spreadThickFilter->update(frame, contentBounds, filterNotFullSpreadBounds, filterScale);
  }
  blurEffect->blurriness->value = size * 2.0;
  blurFilter->updateParams(0.0, offset, true, color);
  blurFilter->update(frame, filterNotFullSpreadBounds, transformedBounds, filterScale);
}

void DropShadowFilter::updateParamModeFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                 const tgfx::Rect& transformedBounds,
                                                 const tgfx::Point& filterScale) {
  if (size < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(frame, contentBounds, transformedBounds, filterScale);
  } else {
    spreadThickFilter->update(frame, contentBounds, transformedBounds, filterScale);
  }
}

void DropShadowFilter::draw(tgfx::Context* context, const FilterSource* source,
                            const FilterTarget* target) {
  if (source == nullptr || target == nullptr) {
    return;
  }
  if (spread == 0.0) {
    onDrawModeNotSpread(context, source, target);
  } else if (spread == 1.0) {
    onDrawModeFullSpread(context, source, target);
  } else {
    onDrawModeNotFullSpread(context, source, target);
  }
}

void DropShadowFilter::onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                                           const FilterTarget* target) {
  blurFilter->draw(context, source, target);
}

void DropShadowFilter::onDrawModeFullSpread(tgfx::Context* context, const FilterSource* source,
                                            const FilterTarget* target) {
  if (size < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->draw(context, source, target);
  } else {
    spreadThickFilter->draw(context, source, target);
  }
}

void DropShadowFilter::onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                                               const FilterTarget* target) {
  auto targetWidth = static_cast<int>(ceilf(filterNotFullSpreadBounds.width() * source->scale.x));
  auto targetHeight = static_cast<int>(ceilf(filterNotFullSpreadBounds.height() * source->scale.y));
  if (spreadFilterBuffer == nullptr || spreadFilterBuffer->width() != targetWidth ||
      spreadFilterBuffer->height() != targetHeight) {
    spreadFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (spreadFilterBuffer == nullptr) {
    return;
  }
  spreadFilterBuffer->clearColor();
  auto offsetMatrix = tgfx::Matrix::MakeTrans(
      (filterContentBounds.left - filterNotFullSpreadBounds.left) * source->scale.x,
      (filterContentBounds.top - filterNotFullSpreadBounds.top) * source->scale.y);
  auto targetSpread = spreadFilterBuffer->toFilterTarget(offsetMatrix);
  if (size < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->draw(context, source, targetSpread.get());
  } else {
    spreadThickFilter->draw(context, source, targetSpread.get());
  }

  auto sourceSpread = spreadFilterBuffer->toFilterSource(source->scale);
  FilterTarget* filterTarget = new FilterTarget();
  filterTarget->frameBuffer = target->frameBuffer;
  filterTarget->width = target->width;
  filterTarget->height = target->height;
  filterTarget->vertexMatrix = target->vertexMatrix;
  offsetMatrix = tgfx::Matrix::MakeTrans(
      (filterNotFullSpreadBounds.left - filterContentBounds.left) * source->scale.x,
      (filterNotFullSpreadBounds.top - filterContentBounds.top) * source->scale.y);
  PreConcatMatrix(filterTarget, offsetMatrix);
  blurFilter->draw(context, sourceSpread.get(), filterTarget);
}
}  // namespace pag
