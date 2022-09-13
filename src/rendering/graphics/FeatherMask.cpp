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

#include "FeatherMask.h"
#include "pag/file.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/gaussianblur/GaussianBlurFilter.h"
#include "rendering/utils/PathUtil.h"
#include "tgfx/core/Mask.h"
#include "tgfx/gpu/Canvas.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
std::shared_ptr<Graphic> FeatherMask::MakeFrom(const std::vector<MaskData*>& masks,
                                               Frame layerFrame) {
  if (masks.empty()) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(new FeatherMask(masks, layerFrame));
}

tgfx::Rect MeasureFeatherMaskBounds(const std::vector<MaskData*>& masks, Frame layerFrame) {
  auto maskBounds = tgfx::Rect::MakeLTRB(FLT_MAX, FLT_MAX, FLT_MIN, FLT_MIN);
  for (auto mask : masks) {
    auto maskPath = ToPath(*(mask->maskPath->getValueAt(layerFrame)));
    auto bounds = maskPath.getBounds();
    if (bounds.left < maskBounds.left) {
      maskBounds.left = bounds.left;
    }
    if (bounds.top < maskBounds.top) {
      maskBounds.top = bounds.top;
    }
    if (bounds.right > maskBounds.right) {
      maskBounds.right = bounds.right;
    }
    if (bounds.bottom > maskBounds.bottom) {
      maskBounds.bottom = bounds.bottom;
    }
  }
  maskBounds.outset(maskBounds.width() * 0.1, maskBounds.height() * 0.1);
  return maskBounds;
}

FeatherMask::FeatherMask(const std::vector<MaskData*>& masks, Frame layerFrame)
    : masks(std::move(masks)), layerFrame(layerFrame) {
  bounds = MeasureFeatherMaskBounds(masks, layerFrame);
}

void FeatherMask::measureBounds(tgfx::Rect* rect) const {
  *rect = bounds;
}

bool FeatherMask::hitTest(RenderCache*, float, float) {
  return false;
}

bool FeatherMask::getPath(tgfx::Path*) const {
  return false;
}

void FeatherMask::prepare(RenderCache*) const {
}

std::unique_ptr<Snapshot> FeatherMask::drawFeatherMask(const std::vector<MaskData*>& masks,
                                                       Frame layerFrame, RenderCache* cache,
                                                       float scaleFactor) const {
  bool isFirst = true;
  auto surface = tgfx::Surface::Make(cache->getContext(),
                                     static_cast<int>(ceilf(bounds.width() * scaleFactor)),
                                     static_cast<int>(ceilf(bounds.height() * scaleFactor)), true);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  canvas->setMatrix(tgfx::Matrix::MakeTrans(bounds.x(), bounds.y()));
  for (auto& mask : masks) {
    auto path = mask->maskPath->getValueAt(layerFrame);
    if (path == nullptr || !path->isClosed() || mask->maskMode == MaskMode::None) {
      continue;
    }
    auto maskPath = ToPath(*path);
    auto expansion = mask->maskExpansion->getValueAt(layerFrame);
    ExpandPath(&maskPath, expansion);
    auto inverted = mask->inverted;
    if (isFirst) {
      if (mask->maskMode == MaskMode::Subtract) {
        inverted = !inverted;
      }
    }
    if (inverted) {
      maskPath.toggleInverseFillType();
    }
    tgfx::Paint maskPaint;
    float alpha = mask->maskOpacity->getValueAt(layerFrame) / 255.0;
    maskPaint.setAlpha(alpha);
    auto maskBounds = maskPath.getBounds();
    auto width = static_cast<int>(ceilf(maskBounds.width() * scaleFactor));
    auto height = static_cast<int>(ceilf(maskBounds.height() * scaleFactor));
    auto maskSurface = tgfx::Surface::Make(cache->getContext(), width, height, true);
    auto maskCanvas = maskSurface->getCanvas();
    maskCanvas->setMatrix(tgfx::Matrix::MakeTrans(-maskBounds.x(), -maskBounds.y()));
    maskCanvas->drawPath(maskPath, maskPaint);
    auto maskTexture = maskSurface->getTexture();
    tgfx::Paint blurPaint;
    float blurrinessX = 0.0f;
    float blurrinessY = 0.0f;
    if (mask->maskFeather != nullptr) {
      blurrinessX = mask->maskFeather->getValueAt(layerFrame).x * scaleFactor;
      blurrinessY = mask->maskFeather->getValueAt(layerFrame).y * scaleFactor;
      tgfx::TileMode tileMode = tgfx::TileMode::Decal;
      tgfx::Rect cropRect = tgfx::Rect::MakeEmpty();
      auto blurFilter = tgfx::ImageFilter::Blur(blurrinessX, blurrinessY, tileMode, cropRect);
      blurPaint.setImageFilter(blurFilter);
    }
    canvas->save();
    canvas->setMatrix(tgfx::Matrix::MakeTrans(maskBounds.x(), maskBounds.y()));
    canvas->drawTexture(maskTexture, &blurPaint);
    canvas->restore();
  }

  auto texture = surface->getTexture();
  if (texture == nullptr) {
    return nullptr;
  }

  auto matrix = tgfx::Matrix::MakeScale(scaleFactor);
  auto drawingMatrix = tgfx::Matrix::I();
  matrix.invert(&drawingMatrix);

  return std::make_unique<Snapshot>(texture, drawingMatrix);
}

void FeatherMask::draw(tgfx::Canvas* canvas, RenderCache* renderCache) const {
  auto snapshot = drawFeatherMask(masks, layerFrame, renderCache);
  canvas->drawTexture(snapshot->getTexture());
}
}  // namespace pag
