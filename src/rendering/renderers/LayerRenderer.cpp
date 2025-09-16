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

#include "LayerRenderer.h"
#include "base/utils/MatrixUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/editing/StillImage.h"

namespace pag {

static bool TransformIllegal(Transform* transform) {
  return transform && !transform->visible();
}

static bool TrackMatteIsEmpty(TrackMatte* trackMatte) {
  if (trackMatte == nullptr) {
    return false;
  }
  return trackMatte->modifier->isEmpty();
}

void LayerRenderer::DrawLayer(Recorder* recorder, Layer* layer, Frame layerFrame,
                              std::shared_ptr<FilterModifier> filterModifier,
                              TrackMatte* trackMatte, Content* layerContent,
                              Transform* extraTransform) {
  if (TransformIllegal(extraTransform) || TrackMatteIsEmpty(trackMatte)) {
    return;
  }
  auto contentFrame = layerFrame - layer->startTime;
  auto layerCache = LayerCache::Get(layer);
  if (!layerCache->contentVisible(contentFrame)) {
    return;
  }
  auto content = layerContent ? layerContent : layerCache->getContent(contentFrame);
  auto layerTransform = layerCache->getTransform(contentFrame);
  auto alpha = layerTransform->alpha;
  if (extraTransform) {
    alpha *= extraTransform->alpha;
  }
  recorder->saveLayer(alpha, ToTGFX(layer->blendMode));
  if (trackMatte) {
    recorder->saveLayer(trackMatte->modifier);
  }
  auto saveCount = recorder->getSaveCount();
  if (extraTransform) {
    recorder->concat(extraTransform->matrix);
  }
  recorder->concat(layerTransform->matrix);
  if (filterModifier) {
    recorder->saveLayer(filterModifier);
  }
  auto masks = layerCache->getMasks(contentFrame);
  if (masks) {
    recorder->saveClip(*masks);
  } else {
    auto featherMask = layerCache->getFeatherMask(contentFrame);
    if (featherMask) {
      recorder->saveLayer(featherMask);
    }
  }
  content->draw(recorder);
  recorder->restoreToCount(saveCount);
  if (trackMatte) {
    recorder->restore();
    // 若遮罩图层是文本图层，对内部自带颜色的字符（如 emoji ）多执行一次叠加的绘制，
    // 让自带颜色的字符能正常显示出来。
    recorder->drawGraphic(trackMatte->colorGlyphs);
  }
  recorder->restore();
}

static void ApplyClipToBounds(const tgfx::Path& clipPath, tgfx::Rect* bounds) {
  if (!clipPath.isInverseFillType()) {
    auto clipBounds = clipPath.getBounds();
    if (!bounds->intersect(clipBounds)) {
      bounds->setEmpty();
    }
    return;
  }
  tgfx::Path boundsPath = {};
  boundsPath.addRect(*bounds);
  boundsPath.addPath(clipPath, tgfx::PathOp::Intersect);
  *bounds = boundsPath.getBounds();
}

static bool BoundsIsEmpty(tgfx::Rect* bounds) {
  return bounds && bounds->isEmpty();
}

void LayerRenderer::MeasureLayerBounds(tgfx::Rect* bounds, Layer* layer, Frame layerFrame,
                                       std::shared_ptr<FilterModifier> filterModifier,
                                       tgfx::Rect* trackMatteBounds, Content* layerContent,
                                       Transform* extraTransform) {
  bounds->setEmpty();
  if (TransformIllegal(extraTransform) || BoundsIsEmpty(trackMatteBounds)) {
    return;
  }
  auto contentFrame = layerFrame - layer->startTime;
  auto layerCache = LayerCache::Get(layer);
  if (!layerCache->contentVisible(contentFrame)) {
    return;
  }
  auto content = layerContent ? layerContent : layerCache->getContent(contentFrame);
  auto masks = layerCache->getMasks(contentFrame);
  content->measureBounds(bounds);
  if (masks) {
    ApplyClipToBounds(*masks, bounds);
  }
  if (filterModifier) {
    FilterRenderer::MeasureFilterBounds(bounds, filterModifier.get());
  }
  auto layerMatrix = layerCache->getTransform(contentFrame)->matrix;
  if (extraTransform) {
    layerMatrix.postConcat(extraTransform->matrix);
  }
  layerMatrix.mapRect(bounds);
  if (trackMatteBounds != nullptr) {
    if (!bounds->intersect(*trackMatteBounds)) {
      bounds->setEmpty();
    }
  }
}
}  // namespace pag
