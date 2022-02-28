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
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
DropShadowFilter::DropShadowFilter(DropShadowStyle* layerStyle) : layerStyle(layerStyle) {
  blurFilterV = new SinglePassBlurFilter(BlurDirection::Vertical);
  blurFilterH = new SinglePassBlurFilter(BlurDirection::Horizontal);
  spreadFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Normal);
  spreadThickFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Thick);
}

DropShadowFilter::~DropShadowFilter() {
  delete blurFilterV;
  delete blurFilterH;
  delete spreadFilter;
  delete spreadThickFilter;
}

bool DropShadowFilter::initialize(tgfx::Context* context) {
  if (!blurFilterV->initialize(context)) {
    return false;
  }
  if (!blurFilterH->initialize(context)) {
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
  alpha = ToAlpha(layerStyle->opacity->getValueAt(layerFrame));
  spread = layerStyle->spread->getValueAt(layerFrame);
  auto size = layerStyle->size->getValueAt(layerFrame);
  spread *= (spread == 1.0) ? 1.0 : 0.8;
  blurSize = (1.0f - spread) * size;
  spreadSize = size * spread;

  filtersBounds.clear();
  filtersBounds.emplace_back(contentBounds);
  if (spread == 0.0f) {
    updateParamModeNotSpread(frame, contentBounds, transformedBounds, filterScale);
  } else if (spread == 1.0f) {
    updateParamModeFullSpread(frame, contentBounds, transformedBounds, filterScale);
  } else {
    updateParamModeNotFullSpread(frame, contentBounds, transformedBounds, filterScale);
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

void DropShadowFilter::updateParamModeNotSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                const tgfx::Rect&, const tgfx::Point& filterScale) {
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  auto radians = DegreesToRadians(angle - 180);
  float offsetY = -sinf(radians) * distance;
  float offsetX = cosf(radians) * distance;

  auto filterVBounds = contentBounds;
  filterVBounds.offset(0, offsetY * filterScale.y);
  filterVBounds.outset(0, blurSize * filterScale.y);
  filterVBounds.roundOut();
  blurFilterV->update(frame, contentBounds, filterVBounds, filterScale);
  filtersBounds.emplace_back(filterVBounds);
  auto filterHBounds = filterVBounds;
  filterHBounds.offset(offsetX * filterScale.x, 0);
  filterHBounds.outset(blurSize * filterScale.x, 0);
  filterHBounds.roundOut();
  blurFilterH->update(frame, filterVBounds, filterHBounds, filterScale);
  filtersBounds.emplace_back(filterHBounds);
}

void DropShadowFilter::updateParamModeNotFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                    const tgfx::Rect&,
                                                    const tgfx::Point& filterScale) {
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  auto radians = DegreesToRadians(angle - 180);
  float offsetY = -sinf(radians) * distance;
  float offsetX = cosf(radians) * distance;

  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.roundOut();
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(frame, contentBounds, filterBounds, filterScale);
  } else {
    spreadThickFilter->update(frame, contentBounds, filterBounds, filterScale);
  }
  filtersBounds.emplace_back(filterBounds);
  auto lastBounds = filterBounds;
  filterBounds.offset(0, offsetY * filterScale.y);
  filterBounds.outset(0, blurSize * filterScale.y);
  blurFilterV->update(frame, lastBounds, filterBounds, filterScale);
  filterBounds.roundOut();
  filtersBounds.emplace_back(filterBounds);
  lastBounds = filterBounds;
  filterBounds.offset(offsetX * filterScale.x, 0);
  filterBounds.outset(blurSize * filterScale.x, 0);
  blurFilterH->update(frame, lastBounds, filterBounds, filterScale);
  filtersBounds.emplace_back(filterBounds);
}

void DropShadowFilter::updateParamModeFullSpread(Frame frame, const tgfx::Rect& contentBounds,
                                                 const tgfx::Rect&,
                                                 const tgfx::Point& filterScale) {
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  auto radians = DegreesToRadians(angle - 180);
  float offsetY = -sinf(radians) * distance;
  float offsetX = cosf(radians) * distance;

  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.offset(offsetX * filterScale.x, offsetY * filterScale.y);
  filterBounds.roundOut();
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(frame, contentBounds, filterBounds, filterScale);
  } else {
    spreadThickFilter->update(frame, contentBounds, filterBounds, filterScale);
  }
  filtersBounds.emplace_back(filterBounds);
}

void DropShadowFilter::onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                                           const FilterTarget* target) {
  auto contentBounds = filtersBounds[0];
  auto filterBounds = filtersBounds[1];
  auto targetWidth = static_cast<int>(ceilf(filterBounds.width() * source->scale.x));
  auto targetHeight = static_cast<int>(ceilf(filterBounds.height() * source->scale.y));
  if (blurFilterBuffer == nullptr || blurFilterBuffer->width() != targetWidth ||
      blurFilterBuffer->height() != targetHeight) {
    blurFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (blurFilterBuffer == nullptr) {
    return;
  }
  auto gl = tgfx::GLInterface::Get(context);
  blurFilterBuffer->clearColor(gl);

  auto offsetMatrix =
      tgfx::Matrix::MakeTrans((contentBounds.left - filterBounds.left) * source->scale.x,
                              (contentBounds.top - filterBounds.top) * source->scale.y);
  auto targetV = blurFilterBuffer->toFilterTarget(offsetMatrix);

  blurFilterV->updateParams(blurSize, 1.0, false, BlurMode::Shadow);
  blurFilterV->enableBlurColor(color);
  blurFilterV->draw(context, source, targetV.get());
  blurFilterV->disableBlurColor();

  auto sourceH = blurFilterBuffer->toFilterSource(source->scale);

  blurFilterH->updateParams(blurSize, alpha, false, BlurMode::Shadow);
  tgfx::Matrix revertMatrix =
      tgfx::Matrix::MakeTrans((filterBounds.left - contentBounds.left) * source->scale.x,
                              (filterBounds.top - contentBounds.top) * source->scale.y);

  auto targetH = *target;
  PreConcatMatrix(&targetH, revertMatrix);
  blurFilterH->draw(context, sourceH.get(), &targetH);
}

void DropShadowFilter::onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                                               const FilterTarget* target) {
  auto contentBounds = filtersBounds[0];
  auto lastBounds = contentBounds;
  auto filterBounds = filtersBounds[1];
  auto targetWidth = static_cast<int>(ceilf(filterBounds.width() * source->scale.x));
  auto targetHeight = static_cast<int>(ceilf(filterBounds.height() * source->scale.y));
  if (spreadFilterBuffer == nullptr || spreadFilterBuffer->width() != targetWidth ||
      spreadFilterBuffer->height() != targetHeight) {
    spreadFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (spreadFilterBuffer == nullptr) {
    return;
  }
  auto gl = tgfx::GLInterface::Get(context);
  spreadFilterBuffer->clearColor(gl);
  auto offsetMatrix =
      tgfx::Matrix::MakeTrans((lastBounds.left - filterBounds.left) * source->scale.x,
                              (lastBounds.top - filterBounds.top) * source->scale.y);
  auto targetSpread = spreadFilterBuffer->toFilterTarget(offsetMatrix);
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->draw(context, source, targetSpread.get());
  } else {
    spreadThickFilter->draw(context, source, targetSpread.get());
  }

  auto sourceV = spreadFilterBuffer->toFilterSource(source->scale);
  lastBounds = filterBounds;
  filterBounds = filtersBounds[2];
  targetWidth = static_cast<int>(ceilf(filterBounds.width() * source->scale.x));
  targetHeight = static_cast<int>(ceilf(filterBounds.height() * source->scale.y));
  if (blurFilterBuffer == nullptr || blurFilterBuffer->width() != targetWidth ||
      blurFilterBuffer->height() != targetHeight) {
    blurFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (blurFilterBuffer == nullptr) {
    return;
  }
  blurFilterBuffer->clearColor(gl);
  offsetMatrix = tgfx::Matrix::MakeTrans((lastBounds.left - filterBounds.left) * source->scale.x,
                                         (lastBounds.top - filterBounds.top) * source->scale.y);
  auto targetV = blurFilterBuffer->toFilterTarget(offsetMatrix);
  blurFilterV->updateParams(blurSize, 1.0, false, BlurMode::Shadow);
  blurFilterV->draw(context, sourceV.get(), targetV.get());

  auto sourceH = blurFilterBuffer->toFilterSource(source->scale);
  tgfx::Matrix revertMatrix =
      tgfx::Matrix::MakeTrans((filterBounds.left - contentBounds.left) * source->scale.x,
                              (filterBounds.top - contentBounds.top) * source->scale.y);
  auto targetH = *target;
  PreConcatMatrix(&targetH, revertMatrix);
  blurFilterH->updateParams(blurSize, alpha, false, BlurMode::Shadow);
  blurFilterH->draw(context, sourceH.get(), &targetH);
}

void DropShadowFilter::onDrawModeFullSpread(tgfx::Context* context, const FilterSource* source,
                                            const FilterTarget* target) {
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->draw(context, source, target);
  } else {
    spreadThickFilter->draw(context, source, target);
  }
}
}  // namespace pag
