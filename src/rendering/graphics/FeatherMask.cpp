/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "base/utils/TGFXCast.h"
#include "pag/file.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/gaussianblur/GaussianBlurFilter.h"
#include "rendering/utils/PathUtil.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Surface.h"

namespace pag {
std::shared_ptr<Graphic> FeatherMask::MakeFrom(const std::vector<MaskData*>& masks,
                                               Frame layerFrame) {
  if (masks.empty()) {
    return nullptr;
  }
  return std::shared_ptr<Graphic>(new FeatherMask(masks, layerFrame));
}

tgfx::Rect MeasureFeatherMaskBounds(const std::vector<MaskData*>& masks, Frame layerFrame) {
  auto maskBounds = tgfx::Rect::MakeLTRB(0, 0, FLT_MIN, FLT_MIN);
  for (auto mask : masks) {
    auto maskPath = ToPath(*(mask->maskPath->getValueAt(layerFrame)));
    auto expansion = mask->maskExpansion->getValueAt(layerFrame);
    ExpandPath(&maskPath, expansion);
    auto bounds = maskPath.getBounds();
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
    : masks(masks), layerFrame(layerFrame) {
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

std::shared_ptr<tgfx::Surface> MakeAlphaSurface(tgfx::Context* context, int width, int height) {
  auto surface = tgfx::Surface::Make(context, width, height, true);
  if (surface == nullptr) {
    surface = tgfx::Surface::Make(context, width, height);
  }
  return surface;
}

void FeatherMask::draw(Canvas* parentCanvas) const {
  auto surface =
      MakeAlphaSurface(parentCanvas->getContext(), static_cast<int>(ceilf(bounds.width())),
                       static_cast<int>(ceilf(bounds.height())));
  if (surface == nullptr) {
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->setMatrix(tgfx::Matrix::MakeTrans(bounds.x(), bounds.y()));
  bool isFirst = true;
  for (auto* mask : masks) {
    auto path = mask->maskPath->getValueAt(layerFrame);
    if (path == nullptr || !path->isClosed() || mask->maskMode == MaskMode::None) {
      continue;
    }
    auto maskPath = ToPath(*path);
    auto expansion = mask->maskExpansion->getValueAt(layerFrame);
    ExpandPath(&maskPath, expansion);
    auto inverted = mask->inverted;
    if (isFirst) {
      isFirst = false;
      if (mask->maskMode == MaskMode::Subtract) {
        inverted = !inverted;
      }
    }
    if (inverted) {
      maskPath.toggleInverseFillType();
    }
    auto maskBounds = maskPath.getBounds();
    auto width = static_cast<int>(ceilf(maskBounds.width()));
    auto height = static_cast<int>(ceilf(maskBounds.height()));
    if (width == 0 || height == 0) {
      continue;
    }
    auto maskSurface = MakeAlphaSurface(parentCanvas->getContext(), width, height);
    if (maskSurface == nullptr) {
      return;
    }
    auto maskCanvas = maskSurface->getCanvas();
    maskCanvas->setMatrix(tgfx::Matrix::MakeTrans(-maskBounds.x(), -maskBounds.y()));
    tgfx::Paint maskPaint;
    float alpha = ToAlpha(mask->maskOpacity->getValueAt(layerFrame));
    maskPaint.setAlpha(alpha);
    maskCanvas->drawPath(maskPath, maskPaint);
    auto maskImage = maskSurface->makeImageSnapshot();
    tgfx::Paint blurPaint;
    if (mask->maskFeather) {
      auto blurrinessX = mask->maskFeather->getValueAt(layerFrame).x;
      auto blurrinessY = mask->maskFeather->getValueAt(layerFrame).y;
      blurPaint.setImageFilter(tgfx::ImageFilter::Blur(blurrinessX / 2, blurrinessY / 2));
    }
    canvas->save();
    canvas->setMatrix(tgfx::Matrix::MakeTrans(maskBounds.x(), maskBounds.y()));
    canvas->drawImage(maskImage, &blurPaint);
    canvas->restore();
  }
  parentCanvas->drawImage(surface->makeImageSnapshot());
}
}  // namespace pag
