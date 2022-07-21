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
#include "rendering/filters/utils/BlurTypes.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/gpu/ImageFilter.h"

namespace pag {
DropShadowFilter::DropShadowFilter(DropShadowStyle* layerStyle) : layerStyle(layerStyle) {
  spreadFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Normal);
  spreadThickFilter = new DropShadowSpreadFilter(layerStyle, DropShadowStyleMode::Thick);
}

DropShadowFilter::~DropShadowFilter() {
  delete spreadFilter;
  delete spreadThickFilter;
}

bool DropShadowFilter::initialize(tgfx::Context* context) {
  if (!spreadFilter->initialize(context) || !spreadThickFilter->initialize(context)) {
    return false;
  }
  return true;
}

void DropShadowFilter::update(Frame frame, const tgfx::Rect& contentBounds,
                              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale) {
  LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);

  color = ToTGFX(layerStyle->color->getValueAt(layerFrame),
                 layerStyle->opacity->getValueAt(layerFrame));
  spread = layerStyle->spread->getValueAt(layerFrame);
  auto size = layerStyle->size->getValueAt(layerFrame);
  spread *= (spread == 1.f) ? 1.f : 0.8f;
  spreadSize = size * spread;
  auto blurSize = size * (1.f - spread) * 2.f;
  blurXSize = blurSize * filterScale.x;
  blurYSize = blurSize * filterScale.y;
  auto distance = layerStyle->distance->getValueAt(layerFrame);
  if (distance > 0.f) {
    auto angle = layerStyle->angle->getValueAt(layerFrame);
    auto radians = DegreesToRadians(angle - 180.f);
    offsetX = cosf(radians) * distance * filterScale.x;
    offsetY = -sinf(radians) * distance * filterScale.y;
  }

  filtersBounds.clear();
  filtersBounds.emplace_back(contentBounds);
  if (spread == 1.f) {
    updateParamModeFullSpread(contentBounds);
  } else if (spread != 0.f) {
    updateParamModeNotFullSpread(contentBounds);
  }
}

void DropShadowFilter::draw(tgfx::Context* context, const FilterSource* source,
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

void DropShadowFilter::updateParamModeNotFullSpread(const tgfx::Rect& contentBounds) {
  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.roundOut();
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  } else {
    spreadThickFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  }
  filtersBounds.emplace_back(filterBounds);
}

void DropShadowFilter::updateParamModeFullSpread(const tgfx::Rect& contentBounds) {
  auto filterBounds = contentBounds;
  filterBounds.outset(spreadSize * filterScale.x, spreadSize * filterScale.y);
  filterBounds.offset(offsetX, offsetY);
  filterBounds.roundOut();
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  } else {
    spreadThickFilter->update(layerFrame, contentBounds, filterBounds, filterScale);
  }
}

void DropShadowFilter::onDrawModeNotSpread(tgfx::Context* context, const FilterSource* source,
                                           const FilterTarget* target) {
  auto renderTarget = tgfx::GLRenderTarget::MakeFrom(context, target->frameBuffer, target->width,
                                                     target->height, tgfx::ImageOrigin::TopLeft);
  auto targetSurface = tgfx::Surface::MakeFrom(renderTarget);
  auto targetCanvas = targetSurface->getCanvas();
  auto texture = tgfx::GLTexture::MakeFrom(context, source->sampler, source->width, source->height,
                                           tgfx::ImageOrigin::TopLeft);
  targetCanvas->setMatrix(ToMatrix(target));
  tgfx::Paint paint;
  paint.setImageFilter(tgfx::ImageFilter::DropShadowOnly(
      offsetX * source->scale.x, offsetY * source->scale.y, blurXSize * source->scale.x,
      blurYSize * source->scale.y, color));
  targetCanvas->drawTexture(texture.get(), &paint);
}

void DropShadowFilter::onDrawModeNotFullSpread(tgfx::Context* context, const FilterSource* source,
                                               const FilterTarget* target) {
  auto contentBounds = filtersBounds[0];
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
  spreadFilterBuffer->clearColor();
  auto offsetMatrix =
      tgfx::Matrix::MakeTrans((contentBounds.left - filterBounds.left) * source->scale.x,
                              (contentBounds.top - filterBounds.top) * source->scale.y);
  auto targetSpread = spreadFilterBuffer->toFilterTarget(offsetMatrix);
  if (spreadSize < DROPSHADOW_SPREAD_MIN_THICK_SIZE) {
    spreadFilter->draw(context, source, targetSpread.get());
  } else {
    spreadThickFilter->draw(context, source, targetSpread.get());
  }

  auto sourceV = spreadFilterBuffer->toFilterSource(source->scale);

  auto renderTarget = tgfx::GLRenderTarget::MakeFrom(context, target->frameBuffer, target->width,
                                                     target->height, tgfx::ImageOrigin::TopLeft);
  auto targetSurface = tgfx::Surface::MakeFrom(renderTarget);
  auto targetCanvas = targetSurface->getCanvas();
  auto texture = tgfx::GLTexture::MakeFrom(context, sourceV->sampler, sourceV->width,
                                           sourceV->height, tgfx::ImageOrigin::TopLeft);
  targetCanvas->setMatrix(ToMatrix(target));
  targetCanvas->concat(tgfx::Matrix::MakeTrans(
      static_cast<float>((source->width - sourceV->width)) * 0.5f * source->scale.x,
      static_cast<float>((source->height - sourceV->height)) * 0.5f * source->scale.y));
  tgfx::Paint paint;
  paint.setImageFilter(tgfx::ImageFilter::DropShadowOnly(
      offsetX * source->scale.x, offsetY * source->scale.y, blurXSize * source->scale.x,
      blurYSize * source->scale.y, color));
  targetCanvas->drawTexture(texture.get(), &paint);
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
