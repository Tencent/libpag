/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "StrokeFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
StrokeFilter::StrokeFilter(StrokeStyle* layerStyle) : layerStyle(layerStyle) {
  strokeFilter = new SolidStrokeFilter(SolidStrokeMode::Normal);
  strokeThickFilter = new SolidStrokeFilter(SolidStrokeMode::Thick);
  alphaEdgeDetectFilter = new AlphaEdgeDetectFilter();
}

StrokeFilter::~StrokeFilter() {
  delete strokeFilter;
  delete strokeThickFilter;
  delete alphaEdgeDetectFilter;
}

bool StrokeFilter::initialize(tgfx::Context* context) {
  if (!strokeFilter->initialize(context) || !strokeThickFilter->initialize(context)
      || !alphaEdgeDetectFilter->initialize(context)) {
    return false;
  }
  return true;
}

void StrokeFilter::update(Frame frame, const tgfx::Rect& contentBounds,
                          const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale) {
  LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);

  strokePosition = layerStyle->position->getValueAt(layerFrame);
  spreadSize = layerStyle->size->getValueAt(layerFrame);
  
  strokeOption = SolidStrokeOption();
  strokeOption.color = layerStyle->color->getValueAt(layerFrame);
  strokeOption.opacity = layerStyle->opacity->getValueAt(layerFrame);
  strokeOption.spreadSize = spreadSize;
  strokeOption.position = strokePosition;
  
  if (strokePosition == StrokePosition::Center) {
    strokeOption.spreadSize *= 0.4;
  } else if (strokePosition == StrokePosition::Inside) {
    strokeOption.spreadSize *= 0.8;
  }

  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.roundOut();
  alphaEdgeDetectFilter->update(layerFrame, contentBounds, contentBounds, filterScale);
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->onUpdateOption(strokeOption);
    strokeFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  } else {
    strokeThickFilter->onUpdateOption(strokeOption);
    strokeThickFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  }
}

void StrokeFilter::draw(tgfx::Context* context, const FilterSource* source,
                           const FilterTarget* target) {
  if (source == nullptr || target == nullptr) {
    return;
  }
  if (strokePosition == StrokePosition::Outside) {
    onDrawPositionOutside(context, source, target);
  } else {
    onDrawPositionInsideOrCenter(context, source, target);
  }
}

void StrokeFilter::onDrawPositionOutside(tgfx::Context* context, const FilterSource* source,
                                         const FilterTarget* target) {
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->draw(context, source, target);
  } else {
    strokeThickFilter->draw(context, source, target);
  }
}

void StrokeFilter::onDrawPositionInsideOrCenter(tgfx::Context* context, const FilterSource* source,
                                                const FilterTarget* target) {
  auto targetWidth = source->width;
  auto targetHeight = source->height;
  if (alphaEdgeDetectFilterBuffer == nullptr || alphaEdgeDetectFilterBuffer->width() != targetWidth ||
      alphaEdgeDetectFilterBuffer->height() != targetHeight) {
    alphaEdgeDetectFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (alphaEdgeDetectFilterBuffer == nullptr) {
    return;
  }
  
  alphaEdgeDetectFilterBuffer->clearColor();
  auto alphaEdgeDetectTarget = alphaEdgeDetectFilterBuffer->toFilterTarget(tgfx::Matrix::I());
  alphaEdgeDetectFilter->draw(context, source, alphaEdgeDetectTarget.get());

  auto newSource = alphaEdgeDetectFilterBuffer->toFilterSource(source->scale);

  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->onUpdateOriginalTexture(&source->sampler);
    strokeFilter->draw(context, newSource.get(), target);
  } else {
    strokeThickFilter->onUpdateOriginalTexture(&source->sampler);
    strokeThickFilter->draw(context, newSource.get(), target);
  }
}
}  // namespace pag
