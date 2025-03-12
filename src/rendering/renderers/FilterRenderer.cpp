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

#include "FilterRenderer.h"
#include <tgfx/core/Recorder.h>
#include <utility>
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/DisplacementMapFilter.h"
#include "rendering/filters/FilterModifier.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/filters/MotionTileFilter.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "rendering/utils/SurfaceUtil.h"
#include "tgfx/core/Surface.h"

namespace pag {

static float GetScaleFactorLimit(Layer* layer) {
  auto scaleFactorLimit = layer->type() == LayerType::Image ? 1.0f : FLT_MAX;
  return scaleFactorLimit;
}

static bool DoesProcessVisibleAreaOnly(Layer* layer) {
  for (auto& effect : layer->effects) {
    if (!effect->processVisibleAreaOnly()) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<FilterList> FilterRenderer::MakeFilterList(const FilterModifier* modifier) {
  auto filterList = std::make_unique<FilterList>();
  auto layer = modifier->layer;
  auto layerFrame = modifier->layerFrame;
  filterList->layer = layer;
  filterList->layerFrame = layerFrame;
  auto contentFrame = layerFrame - layer->startTime;
  filterList->layerMatrix = LayerCache::Get(layer)->getTransform(contentFrame)->matrix;
  filterList->scaleFactorLimit = GetScaleFactorLimit(filterList->layer);
  filterList->processVisibleAreaOnly = DoesProcessVisibleAreaOnly(filterList->layer);
  for (auto& effect : layer->effects) {
    if (effect->visibleAt(layerFrame)) {
      filterList->effects.push_back(effect);
    }
  }
  for (auto& layerStyle : layer->layerStyles) {
    if (layerStyle->visibleAt(layerFrame)) {
      filterList->layerStyles.push_back(layerStyle);
    }
  }
  // 当存在非只处理可见区域的Effect滤镜时，对于Shape和Text图层会直接取父级Composition的的尺寸，
  // 应用图层本身的matrix之后再作为
  // 输入纹理，超出部分截断。而对于Solid，Image，PreCompose按内容实际尺寸作为输入纹理，并且不包含图层本身
  // 的matrix。
  auto needParentSizeInput = !layer->effects.empty() && (layer->type() == LayerType::Shape ||
                                                         layer->type() == LayerType::Text);
  filterList->useParentSizeInput = !filterList->processVisibleAreaOnly && needParentSizeInput;
  if (!filterList->useParentSizeInput) {
    // LayerStyle本应该在图层matrix之后应用的，但是我们为了简化渲染改到了matrix之前，因此需要反向排除图层matrix的缩放值。
    filterList->layerStyleScale = GetScaleFactor(filterList->layerMatrix, 1.0f, true);
    if (needParentSizeInput) {
      // 含有本应该在图层matrix之后应用的Effect，但是我们为了简化渲染改到了matrix之前，因此需要反向排除图层matrix
      // 的缩放值。
      filterList->effectScale = filterList->layerStyleScale;
    }
  }

  return filterList;
}

tgfx::Rect FilterRenderer::GetContentBounds(const FilterList* filterList,
                                            std::shared_ptr<Graphic> content) {
  tgfx::Rect contentBounds = tgfx::Rect::MakeEmpty();
  if (filterList->processVisibleAreaOnly) {
    content->measureBounds(&contentBounds);
    contentBounds.roundOut();
  } else {
    contentBounds = ToTGFX(filterList->layer->getBounds());
  }
  return contentBounds;
}

void TransformFilterBounds(tgfx::Rect* filterBounds, const FilterList* filterList) {
  // 滤镜应用顺序：Effects->motionBlur/3DLayer(withMotionBlur)->LayerStyles
  for (auto& effect : filterList->effects) {
    effect->transformBounds(ToPAG(filterBounds), ToPAG(filterList->effectScale),
                            filterList->layerFrame);
    filterBounds->roundOut();
  }

  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    MotionBlurFilter::TransformBounds(filterBounds, filterList->layer, filterList->layerFrame);
  }

  if (!filterList->layerStyles.empty()) {
    LayerStylesFilter::TransformBounds(filterBounds, filterList);
  }
}

void FilterRenderer::MeasureFilterBounds(tgfx::Rect* bounds, const FilterModifier* modifier) {
  auto filterList = MakeFilterList(modifier);
  if (filterList->processVisibleAreaOnly) {
    bounds->roundOut();
  } else {
    *bounds = ToTGFX(filterList->layer->getBounds());
  }
  TransformFilterBounds(bounds, filterList.get());
  if (filterList->useParentSizeInput) {
    tgfx::Matrix inverted = tgfx::Matrix::I();
    filterList->layerMatrix.invert(&inverted);
    inverted.mapRect(bounds);
  }
}

tgfx::Rect GetClipBounds(Canvas* canvas, const FilterList* filterList) {
  auto clip = canvas->getTotalClip();
  auto matrix = canvas->getMatrix();
  if (filterList->useParentSizeInput) {
    tgfx::Matrix inverted = tgfx::Matrix::I();
    filterList->layerMatrix.invert(&inverted);
    matrix.preConcat(inverted);
  }
  tgfx::Matrix inverted = tgfx::Matrix::I();
  matrix.invert(&inverted);
  clip.transform(inverted);
  return clip.getBounds();
}

static bool MakeLayerStyleNode(std::vector<std::shared_ptr<Filter>>& filters,
                               tgfx::Rect& clipBounds, const FilterList* filterList,
                               tgfx::Rect& filterBounds) {
  if (!filterList->layerStyles.empty()) {
    auto filter = LayerStylesFilter::Make(filterList->layerStyles);
    if (nullptr == filter) {
      return false;
    }
    filter->setFilterScale(filterList->layerStyleScale);
    LayerStylesFilter::TransformBounds(&filterBounds, filterList);
    filterBounds.roundOut();
    if (!filterBounds.intersect(clipBounds)) {
      return false;
    }
    filters.push_back(filter);
  }
  return true;
}

static bool MakeMotionBlurNode(std::vector<std::shared_ptr<Filter>>& filterNodes,
                               tgfx::Rect& clipBounds, const FilterList* filterList,
                               tgfx::Rect& filterBounds) {
  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    auto filter = std::make_shared<MotionBlurFilter>(filterList->layer);
    filter->setContentBounds(filterBounds);
    MotionBlurFilter::TransformBounds(&filterBounds, filterList->layer, filterList->layerFrame);
    filterBounds.roundOut();
    if (!filterBounds.intersect(clipBounds)) {
      return false;
    }
    filterNodes.emplace_back(filter);
  }
  return true;
}

bool FilterRenderer::MakeEffectNode(RenderCache* cache,
                                    std::vector<std::shared_ptr<Filter>>& filterNodes,
                                    tgfx::Rect& clipBounds, const FilterList* filterList,
                                    tgfx::Rect& filterBounds, tgfx::Point& effectScale,
                                    int clipIndex) {
  auto effectIndex = 0;
  for (auto& effect : filterList->effects) {
    if (auto filter = EffectFilter::Make(effect)) {
      filter->setContentBounds(filterBounds);
      filter->setFilterScale(effectScale);
      effect->transformBounds(ToPAG(&filterBounds), ToPAG(effectScale), filterList->layerFrame);
      filterBounds.roundOut();
      if (effectIndex >= clipIndex && !filterBounds.intersect(clipBounds)) {
        return false;
      }
      if (effect->type() == EffectType::DisplacementMap) {
        auto displacementMapFilter = static_cast<DisplacementMapFilter*>(filter.get());
        displacementMapFilter->updateMapTexture(filterList->layerFrame, cache, filterList->layer,
                                                filterList->layerMatrix);
      }
      filterNodes.emplace_back(filter);
    }
    effectIndex++;
  }
  return true;
}

static bool NeedToSkipClipBounds(const FilterList* filterList) {
  for (auto& effect : filterList->effects) {
    if (effect->type() == EffectType::FastBlur) {
      auto blurEffect = static_cast<FastBlurEffect*>(effect);
      if (blurEffect->repeatEdgePixels->getValueAt(0) == false) {
        return true;
      }
    } else if (effect->type() == EffectType::DisplacementMap) {
      return true;
    }
  }
  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->type() == LayerStyleType::DropShadow) {
      return true;
    }
  }
  if (filterList->layer->transform3D) {
    return true;
  }
  return false;
}

std::vector<std::shared_ptr<Filter>> FilterRenderer::MakeFilterNodes(const FilterList* filterList,
                                                                     RenderCache* renderCache,
                                                                     tgfx::Rect* contentBounds,
                                                                     const tgfx::Rect& clipRect) {
  // 滤镜应用顺序：Effects->PAGFilter->motionBlur/3DLayer->LayerStyles
  std::vector<std::shared_ptr<Filter>> filters;
  int clipIndex = -1;
  for (int i = static_cast<int>(filterList->effects.size()) - 1; i >= 0; i--) {
    auto effect = filterList->effects[i];
    if (!effect->processVisibleAreaOnly()) {
      clipIndex = i;
      break;
    }
  }
  auto clipBounds = clipRect;
  auto filterBounds = *contentBounds;
  auto effectScale = filterList->effectScale;
  bool skipClipBounds = NeedToSkipClipBounds(filterList);

  // MotionBlur Fragment Shader中需要用到裁切区域外的像素，
  // 因此默认先把裁切区域过一次TransformBounds
  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    MotionBlurFilter::TransformBounds(&clipBounds, filterList->layer, filterList->layerFrame);
    clipBounds.roundOut();
  }
  if (!skipClipBounds && clipIndex == -1 && !contentBounds->intersect(clipBounds)) {
    return {};
  }

  if (!MakeEffectNode(renderCache, filters, clipBounds, filterList, filterBounds, effectScale,
                      clipIndex)) {
    return {};
  }

  if (!Make3DLayerNode(filters, clipBounds, filterList, filterBounds, effectScale)) {
    return {};
  }

  if (!MakeMotionBlurNode(filters, clipBounds, filterList, filterBounds)) {
    return {};
  }

  if (!MakeLayerStyleNode(filters, clipBounds, filterList, filterBounds)) {
    return {};
  }
  return filters;
}

std::shared_ptr<tgfx::Image> CreatePictureImage(std::shared_ptr<tgfx::Picture> picture,
                                                tgfx::Point* offset, tgfx::Rect* sourceBounds) {
  auto bounds = sourceBounds ? *sourceBounds : picture->getBounds();
  bounds.roundOut();
  auto width = static_cast<int>(bounds.width());
  auto height = static_cast<int>(bounds.height());
  auto matrix = tgfx::Matrix::MakeTrans(-bounds.x(), -bounds.y());
  auto image = tgfx::Image::MakeFrom(std::move(picture), width, height, &matrix);
  if (offset) {
    offset->x = bounds.x();
    offset->y = bounds.y();
  }
  return image;
}

static void ApplyFilters(Canvas* canvas, const std::vector<std::shared_ptr<Filter>>& filters,
                         std::shared_ptr<tgfx::Image> image, Frame layerFrame,
                         const tgfx::Point& sourceScale) {
  auto source = std::move(image);
  auto totalOffset = tgfx::Point::Zero();
  for (auto& filter : filters) {
    filter->update(layerFrame, sourceScale);
    if (filter->shouldSkipFilter()) {
      continue;
    }
    tgfx::Recorder recorder;
    auto filterCanvas = recorder.beginRecording();
    filter->applyFilter(filterCanvas, source);
    auto picture = recorder.finishRecordingAsPicture();
    if (picture == nullptr) {
      canvas->drawImage(source);
      return;
    }
    auto offset = tgfx::Point::Zero();
    source = CreatePictureImage(picture, &offset, nullptr);
    totalOffset += offset;
  }
  canvas->drawImage(source, totalOffset.x, totalOffset.y);
}

static float GetScaleFactor(FilterList* filterList, const tgfx::Rect& contentBounds) {
  float scale = 1.f;
  for (auto& effect : filterList->effects) {
    auto effectScale = effect->getMaxScaleFactor(ToPAG(contentBounds));
    scale *= std::max(effectScale.x, effectScale.y);
  }
  return scale;
}

#define CONTENT_SCALE_STEP 20.0f

float GetContentScale(Canvas* parentCanvas, float scaleFactorLimit, float scale) {
  auto maxScale = GetMaxScaleFactor(parentCanvas->getMatrix());
  maxScale *= scale;
  if (maxScale > scaleFactorLimit) {
    maxScale = scaleFactorLimit;
  } else {
    // Snap the scale value to 1/20 to prevent edge shaking when rendering zoom-in animations.
    maxScale = ceilf(maxScale * CONTENT_SCALE_STEP) / CONTENT_SCALE_STEP;
  }
  return maxScale;
}

void FilterRenderer::DrawWithFilter(Canvas* parentCanvas, const FilterModifier* modifier,
                                    std::shared_ptr<Graphic> content) {
  auto cache = parentCanvas->getCache();
  auto filterList = MakeFilterList(modifier);
  auto contentBounds = GetContentBounds(filterList.get(), content);
  // 相对于content Bounds的clip Bounds
  auto clipBounds = GetClipBounds(parentCanvas, filterList.get());
  auto filters =
      MakeFilterNodes(filterList.get(), parentCanvas->getCache(), &contentBounds, clipBounds);
  if (filters.empty()) {
    content->draw(parentCanvas);
    return;
  }

  auto scale = GetScaleFactor(filterList.get(), contentBounds);
  auto contentScale = GetContentScale(parentCanvas, filterList->scaleFactorLimit, scale);
  tgfx::Recorder recorder;
  auto canvas = recorder.beginRecording();
  auto contentCanvas = Canvas(canvas, cache);
  contentCanvas.scale(contentScale, contentScale);
  if (filterList->useParentSizeInput) {
    contentCanvas.concat(filterList->layerMatrix);
  }
  auto contentMatrix = contentCanvas.getMatrix();
  content->draw(&contentCanvas);
  tgfx::Point sourceScale = {};
  sourceScale.x = sourceScale.y = GetMaxScaleFactor(contentMatrix);
  auto sourcePicture = recorder.finishRecordingAsPicture();
  if (!sourcePicture) {
    return;
  }
  contentBounds.scale(sourceScale.x, sourceScale.y);
  auto offset = tgfx::Point::Zero();
  auto sourceImage = CreatePictureImage(sourcePicture, &offset, &contentBounds);
  parentCanvas->save();
  tgfx::Matrix inverted = tgfx::Matrix::I();
  contentMatrix.invert(&inverted);
  parentCanvas->concat(inverted);
  parentCanvas->concat(tgfx::Matrix::MakeTrans(offset.x, offset.y));
  ApplyFilters(parentCanvas, filters, sourceImage, filterList->layerFrame, sourceScale);
  parentCanvas->restore();
}
}  // namespace pag
