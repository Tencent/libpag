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

#include "OuterGlowFilter.h"
#include "base/utils/MathUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
OuterGlowFilter::OuterGlowFilter(OuterGlowStyle* layerStyle) : layerStyle(layerStyle) {
  strokeFilter = new SolidStrokeFilter(SolidStrokeMode::Normal);
  strokeThickFilter = new SolidStrokeFilter(SolidStrokeMode::Thick);
}

OuterGlowFilter::~OuterGlowFilter() {
  delete strokeFilter;
  delete strokeThickFilter;
}

bool OuterGlowFilter::initialize(tgfx::Context* context) {
  if (!strokeFilter->initialize(context) || !strokeThickFilter->initialize(context)) {
    return false;
  }
  return true;
}

void OuterGlowFilter::update(Frame frame, const tgfx::Rect& contentBounds,
                              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale) {
  LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);
                                                           
  spread = layerStyle->spread->getValueAt(layerFrame);
  color = ToTGFX(layerStyle->color->getValueAt(layerFrame));
  auto size = layerStyle->size->getValueAt(layerFrame);
  auto range = layerStyle->range->getValueAt(layerFrame);
  spread *= (spread == 1.f) ? 1.f : 0.8f;
  spreadSize = size * spread / range;
  auto blurSize = size * (1.f - spread) * 2.f / range;
  blurXSize = blurSize * filterScale.x;
  blurYSize = blurSize * filterScale.y;
  
  strokeOption = SolidStrokeOption();
  strokeOption.color = layerStyle->color->getValueAt(layerFrame);
  strokeOption.opacity = layerStyle->opacity->getValueAt(layerFrame);
  strokeOption.spreadSize = spreadSize;

  filtersBounds.clear();
  filtersBounds.emplace_back(contentBounds);
  if (spread == 1.f) {
    updateParamModeFullSpread(contentBounds);
  } else if (spread != 0.f) {
    updateParamModeNotFullSpread(contentBounds);
  }
}

void OuterGlowFilter::draw(tgfx::Context* context, const FilterSource* source,
                           const FilterTarget* target) {
  if (source == nullptr || target == nullptr) {
    return;
  }
  if (spread == 0.f) {
    onDrawModeNotSpread(context, source, target);
  } else if (spread == 1.f) {
    onDrawModeFullSpread(context, source, target);
  } else {
    onDrawModeNotFullSpread(context, source, target);
  }
}

void OuterGlowFilter::updateParamModeNotFullSpread(const tgfx::Rect& contentBounds) {
  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.roundOut();
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->onUpdateOption(strokeOption);
    strokeFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  } else {
    strokeThickFilter->onUpdateOption(strokeOption);
    strokeThickFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  }
  filtersBounds.emplace_back(filterBounds);
}

void OuterGlowFilter::updateParamModeFullSpread(const tgfx::Rect& contentBounds) {
  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.offset(offsetX, offsetY);
  filterBounds.roundOut();
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->onUpdateOption(strokeOption);
    strokeFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  } else {
    strokeThickFilter->onUpdateOption(strokeOption);
    strokeThickFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  }
}

void OuterGlowFilter::onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                                           const FilterTarget* target) {
  tgfx::BackendRenderTarget renderTarget = {target->frameBuffer, target->width, target->height};
  auto targetSurface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::TopLeft);
  auto targetCanvas = targetSurface->getCanvas();
  tgfx::BackendTexture backendTexture = {source->sampler, source->width, source->height};
  auto image = tgfx::Image::MakeFrom(context, backendTexture);
  targetCanvas->setMatrix(ToMatrix(target));
  tgfx::Paint paint;
  paint.setImageFilter(tgfx::ImageFilter::DropShadowOnly(
      offsetX * source->scale.x, offsetY * source->scale.y, blurXSize * source->scale.x,
      blurYSize * source->scale.y, color));
  targetCanvas->drawImage(std::move(image), &paint);
  targetCanvas->flush();
}

void OuterGlowFilter::onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                                               const FilterTarget* target) {
  auto contentBounds = filtersBounds[0];
  auto filterBounds = filtersBounds[1];
  auto targetWidth = static_cast<int>(ceilf(filterBounds.width() * source->scale.x));
  auto targetHeight = static_cast<int>(ceilf(filterBounds.height() * source->scale.y));
  if (solidStrokeFilterBuffer == nullptr || solidStrokeFilterBuffer->width() != targetWidth ||
      solidStrokeFilterBuffer->height() != targetHeight) {
    solidStrokeFilterBuffer = FilterBuffer::Make(context, targetWidth, targetHeight);
  }
  if (solidStrokeFilterBuffer == nullptr) {
    return;
  }
  solidStrokeFilterBuffer->clearColor();
  auto offsetMatrix = tgfx::Matrix::MakeTrans((contentBounds.left - filterBounds.left),
                                              (contentBounds.top - filterBounds.top));
  auto targetSpread = solidStrokeFilterBuffer->toFilterTarget(offsetMatrix);
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->draw(context, source, targetSpread.get());
  } else {
    strokeThickFilter->draw(context, source, targetSpread.get());
  }

  auto sourceV = solidStrokeFilterBuffer->toFilterSource(source->scale);

  tgfx::BackendRenderTarget renderTarget = {target->frameBuffer, target->width, target->height};
  auto targetSurface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::TopLeft);
  auto targetCanvas = targetSurface->getCanvas();
  tgfx::BackendTexture backendTexture = {sourceV->sampler, sourceV->width, sourceV->height};
  auto image = tgfx::Image::MakeFrom(context, backendTexture);
  targetCanvas->setMatrix(ToMatrix(target));
  targetCanvas->concat(
      tgfx::Matrix::MakeTrans(static_cast<float>((source->width - sourceV->width)) * 0.5f,
                              static_cast<float>((source->height - sourceV->height)) * 0.5f));
  tgfx::Paint paint;
  paint.setImageFilter(tgfx::ImageFilter::DropShadowOnly(
      offsetX * source->scale.x, offsetY * source->scale.y, blurXSize * source->scale.x,
      blurYSize * source->scale.y, color));
  targetCanvas->drawImage(std::move(image), &paint);
  targetCanvas->flush();
}

void OuterGlowFilter::onDrawModeFullSpread(tgfx::Context* context, const FilterSource* source,
                                            const FilterTarget* target) {
  if (spreadSize < STROKE_SPREAD_MIN_THICK_SIZE) {
    strokeFilter->draw(context, source, target);
  } else {
    strokeThickFilter->draw(context, source, target);
  }
}
}  // namespace pag
