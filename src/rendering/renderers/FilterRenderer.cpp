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
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/DisplacementMapFilter.h"
#include "rendering/filters/FilterModifier.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/filters/utils/FilterBuffer.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "rendering/utils/SurfaceUtil.h"
#include "tgfx/gpu/Surface.h"

namespace pag {

float GetScaleFactorLimit(Layer* layer) {
  auto scaleFactorLimit = layer->type() == LayerType::Image ? 1.0f : FLT_MAX;
  return scaleFactorLimit;
}

bool DoesProcessVisibleAreaOnly(Layer* layer) {
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
    MotionBlurFilter::TransformBounds(filterBounds, filterList->effectScale, filterList->layer,
                                      filterList->layerFrame);
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

tgfx::Rect GetClipBounds(tgfx::Canvas* canvas, const FilterList* filterList) {
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

std::shared_ptr<Graphic> GetDisplacementMapGraphic(const FilterList* filterList, Layer* mapLayer,
                                                   tgfx::Rect* mapBounds) {
  // DisplacementMap只支持引用视频序列帧或者位图序列帧图层。
  // TODO(domrjchen): DisplacementMap 支持所有图层
  auto preComposeLayer = static_cast<PreComposeLayer*>(mapLayer);
  auto composition = preComposeLayer->composition;
  mapBounds->setXYWH(0, 0, static_cast<float>(composition->width),
                     static_cast<float>(composition->height));
  auto contentFrame = filterList->layerFrame - mapLayer->startTime;
  auto layerCache = LayerCache::Get(mapLayer);
  auto content = layerCache->getContent(contentFrame);
  return static_cast<GraphicContent*>(content)->graphic;
}

static bool MakeLayerStyleNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                               const FilterList* filterList, RenderCache* renderCache,
                               tgfx::Rect& filterBounds) {
  if (!filterList->layerStyles.empty()) {
    auto filter = renderCache->getLayerStylesFilter(filterList->layer);
    if (nullptr == filter) {
      return false;
    }
    auto layerStyleScale = filterList->layerStyleScale;
    auto oldBounds = filterBounds;
    LayerStylesFilter::TransformBounds(&filterBounds, filterList);
    filterBounds.roundOut();
    filter->update(filterList, oldBounds, filterBounds, layerStyleScale);
    if (!filterBounds.intersect(clipBounds)) {
      return false;
    }
    filterNodes.emplace_back(filter, filterBounds);
  }
  return true;
}

static bool MakeThreeDLayerNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                                const FilterList* filterList, RenderCache* renderCache,
                                tgfx::Rect& filterBounds, tgfx::Point& effectScale) {
   if (filterList->layer->transform3D) {
     auto filter = renderCache->getTransform3DFilter();
     if (filter && filter->updateLayer(filterList->layer, filterList->layerFrame)) {
       auto oldBounds = filterBounds;
       filterBounds = ToTGFX(filterList->layer->getBounds());
       filter->update(filterList->layerFrame, oldBounds, filterBounds, effectScale);
       if (!filterBounds.intersect(clipBounds)) {
         return false;
       }
       filterNodes.emplace_back(filter, filterBounds);
     }
   }
   return true;
 }

static bool MakeMotionBlurNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                               const FilterList* filterList, RenderCache* renderCache,
                               tgfx::Rect& filterBounds, tgfx::Point& effectScale) {
  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    auto filter = renderCache->getMotionBlurFilter();
    if (filter && filter->updateLayer(filterList->layer, filterList->layerFrame)) {
      auto oldBounds = filterBounds;
      MotionBlurFilter::TransformBounds(&filterBounds, effectScale, filterList->layer,
                                        filterList->layerFrame);
      filterBounds.roundOut();
      filter->update(filterList->layerFrame, oldBounds, filterBounds, effectScale);
      if (!filterBounds.intersect(clipBounds)) {
        return false;
      }
      filterNodes.emplace_back(filter, filterBounds);
    }
  }
  return true;
}

bool FilterRenderer::MakeEffectNode(std::vector<FilterNode>& filterNodes, tgfx::Rect& clipBounds,
                                    const FilterList* filterList, RenderCache* renderCache,
                                    tgfx::Rect& filterBounds, tgfx::Point& effectScale,
                                    int clipIndex) {
  auto effectIndex = 0;
  for (auto& effect : filterList->effects) {
    auto filter = renderCache->getFilterCache(effect);
    if (filter) {
      auto oldBounds = filterBounds;
      effect->transformBounds(ToPAG(&filterBounds), ToPAG(effectScale), filterList->layerFrame);
      filterBounds.roundOut();
      filter->update(filterList->layerFrame, oldBounds, filterBounds, effectScale);
      if (effect->type() == EffectType::DisplacementMap) {
        auto mapEffect = static_cast<DisplacementMapEffect*>(effect);
        auto mapFilter = static_cast<DisplacementMapFilter*>(filter);
        auto mapBounds = tgfx::Rect::MakeEmpty();
        auto graphic =
            GetDisplacementMapGraphic(filterList, mapEffect->displacementMapLayer, &mapBounds);
        mapBounds.roundOut();
        mapFilter->updateMapTexture(renderCache, graphic.get(), mapBounds);
      }
      if (effectIndex >= clipIndex && !filterBounds.intersect(clipBounds)) {
        return false;
      }
      filterNodes.emplace_back(filter, filterBounds);
    }
    effectIndex++;
  }
  return true;
}

bool NeedToSkipClipBounds(const FilterList* filterList) {
  bool skipClipBounds = false;
  for (auto& effect : filterList->effects) {
    if (effect->type() == EffectType::FastBlur) {
      auto blurEffect = static_cast<FastBlurEffect*>(effect);
      if (blurEffect->repeatEdgePixels->getValueAt(0) == false) {
        skipClipBounds = true;
        break;
      }
    }
  }
  for (auto& layerStyle : filterList->layerStyles) {
    if (layerStyle->type() == LayerStyleType::DropShadow) {
      skipClipBounds = true;
      break;
    }
  }
  return skipClipBounds;
}

std::vector<FilterNode> FilterRenderer::MakeFilterNodes(const FilterList* filterList,
                                                        RenderCache* renderCache,
                                                        tgfx::Rect* contentBounds,
                                                        const tgfx::Rect& clipRect) {
  // 滤镜应用顺序：Effects->PAGFilter->motionBlur->LayerStyles
  std::vector<FilterNode> filterNodes = {};
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
    MotionBlurFilter::TransformBounds(&clipBounds, effectScale, filterList->layer,
                                      filterList->layerFrame);
    clipBounds.roundOut();
  }
  if (!skipClipBounds && clipIndex == -1 && !contentBounds->intersect(clipBounds)) {
    return {};
  }

  if (!MakeEffectNode(filterNodes, clipBounds, filterList, renderCache, filterBounds, effectScale,
                      clipIndex)) {
    return {};
  }
  
  if (!MakeThreeDLayerNode(filterNodes, clipBounds, filterList, renderCache, filterBounds, effectScale)) {
    return {};
  }

  if (!MakeMotionBlurNode(filterNodes, clipBounds, filterList, renderCache, filterBounds,
                          effectScale)) {
    return {};
  }

  if (!MakeLayerStyleNode(filterNodes, clipBounds, filterList, renderCache, filterBounds)) {
    return {};
  }
  return filterNodes;
}

void ApplyFilters(tgfx::Context* context, std::vector<FilterNode> filterNodes,
                  const tgfx::Rect& contentBounds, FilterSource* filterSource,
                  FilterTarget* filterTarget) {
  auto scale = filterSource->scale;
  std::shared_ptr<FilterBuffer> freeBuffer = nullptr;
  std::shared_ptr<FilterBuffer> lastBuffer = nullptr;
  std::shared_ptr<FilterSource> lastSource = nullptr;
  auto lastBounds = contentBounds;
  auto lastUsesMSAA = false;
  auto size = static_cast<int>(filterNodes.size());
  for (int i = 0; i < size; i++) {
    auto& node = filterNodes[i];
    auto source = lastSource == nullptr ? filterSource : lastSource.get();
    if (i == size - 1) {
      node.filter->draw(context, source, filterTarget);
      break;
    }
    std::shared_ptr<FilterBuffer> currentBuffer = nullptr;
    if (freeBuffer && node.bounds.width() == lastBounds.width() &&
        node.bounds.height() == lastBounds.height() && node.filter->needsMSAA() == lastUsesMSAA) {
      currentBuffer = freeBuffer;
    } else {
      currentBuffer = FilterBuffer::Make(
          context, static_cast<int>(ceilf(node.bounds.width() * scale.x)),
          static_cast<int>(ceilf(node.bounds.height() * scale.y)), node.filter->needsMSAA());
    }
    if (currentBuffer == nullptr) {
      return;
    }
    currentBuffer->clearColor();
    auto offsetMatrix = tgfx::Matrix::MakeTrans((lastBounds.left - node.bounds.left) * scale.x,
                                                (lastBounds.top - node.bounds.top) * scale.y);
    auto currentTarget = currentBuffer->toFilterTarget(offsetMatrix);
    node.filter->draw(context, source, currentTarget.get());
    lastSource = currentBuffer->toFilterSource(scale);
    freeBuffer = lastBuffer;
    lastBuffer = currentBuffer;
    lastBounds = node.bounds;
    lastUsesMSAA = currentBuffer->usesMSAA();
  }
}

static bool HasComplexPaint(tgfx::Canvas* parentCanvas, const tgfx::Rect& drawingBounds) {
  if (parentCanvas->getAlpha() != 1.0f) {
    return true;
  }
  if (parentCanvas->getBlendMode() != tgfx::BlendMode::SrcOver) {
    return true;
  }
  auto bounds = drawingBounds;
  auto matrix = parentCanvas->getMatrix();
  matrix.mapRect(&bounds);
  auto surface = parentCanvas->getSurface();
  auto surfaceBounds = tgfx::Rect::MakeWH(static_cast<float>(surface->width()),
                                          static_cast<float>(surface->height()));
  bounds.intersect(surfaceBounds);
  auto clip = parentCanvas->getTotalClip();
  if (!clip.contains(bounds)) {
    return true;
  }
  return false;
}

std::unique_ptr<FilterTarget> GetDirectFilterTarget(tgfx::Canvas* parentCanvas,
                                                    const FilterList* filterList,
                                                    const std::vector<FilterNode>& filterNodes,
                                                    const tgfx::Rect& contentBounds,
                                                    const tgfx::Point& sourceScale) {
  // 在高分辨率下，模糊滤镜的开销会增大，需要降采样降低开销；当模糊为最后一个滤镜时，需要离屏绘制
  if (!filterList->effects.empty() && filterList->effects.back()->type() == EffectType::FastBlur) {
    return nullptr;
  }
  if (filterNodes.back().filter->needsMSAA()) {
    return nullptr;
  }
  // 是否能直接上屏，应该用没有经过裁切的transformBounds来判断，
  // 因为计算filter的顶点位置的bounds都是没有经过裁切的
  auto transformBounds = contentBounds;
  TransformFilterBounds(&transformBounds, filterList);
  if (HasComplexPaint(parentCanvas, transformBounds)) {
    return nullptr;
  }
  auto surface = parentCanvas->getSurface();
  auto totalMatrix = parentCanvas->getMatrix();
  if (totalMatrix.getSkewX() != 0 || totalMatrix.getSkewY() != 0) {
    return nullptr;
  }
  auto secondToLastBounds =
      filterNodes.size() > 1 ? filterNodes[filterNodes.size() - 2].bounds : contentBounds;
  totalMatrix.preTranslate(secondToLastBounds.left, secondToLastBounds.top);
  totalMatrix.preScale(1.0f / sourceScale.x, 1.0f / sourceScale.y);
  return ToFilterTarget(surface, totalMatrix);
}

std::unique_ptr<FilterTarget> GetOffscreenFilterTarget(tgfx::Surface* surface,
                                                       const std::vector<FilterNode>& filterNodes,
                                                       const tgfx::Rect& contentBounds,
                                                       const tgfx::Point& sourceScale) {
  auto finalBounds = filterNodes.back().bounds;
  auto secondToLastBounds =
      filterNodes.size() > 1 ? filterNodes[filterNodes.size() - 2].bounds : contentBounds;
  auto totalMatrix =
      tgfx::Matrix::MakeTrans((secondToLastBounds.left - finalBounds.left) * sourceScale.x,
                              (secondToLastBounds.top - finalBounds.top) * sourceScale.y);
  return ToFilterTarget(surface, totalMatrix);
}

std::unique_ptr<FilterSource> ToFilterSource(tgfx::Canvas* canvas) {
  auto surface = canvas->getSurface();
  auto texture = surface->getTexture();
  tgfx::Point scale = {};
  scale.x = scale.y = GetMaxScaleFactor(canvas->getMatrix());
  return ToFilterSource(texture.get(), scale);
}

static float GetScaleFactor(FilterList* filterList, const tgfx::Rect& contentBounds) {
  float scale = 1.f;
  for (auto& effect : filterList->effects) {
    auto effectScale = effect->getMaxScaleFactor(ToPAG(contentBounds));
    scale *= std::max(effectScale.x, effectScale.y);
  }
  return scale;
}

void FilterRenderer::DrawWithFilter(tgfx::Canvas* parentCanvas, RenderCache* cache,
                                    const FilterModifier* modifier,
                                    std::shared_ptr<Graphic> content) {
  auto filterList = MakeFilterList(modifier);
  auto contentBounds = GetContentBounds(filterList.get(), content);
  // 相对于content Bounds的clip Bounds
  auto clipBounds = GetClipBounds(parentCanvas, filterList.get());
  auto filterNodes = MakeFilterNodes(filterList.get(), cache, &contentBounds, clipBounds);
  if (filterNodes.empty()) {
    content->draw(parentCanvas, cache);
    return;
  }
  if (filterList->useParentSizeInput) {
    tgfx::Matrix inverted = tgfx::Matrix::I();
    filterList->layerMatrix.invert(&inverted);
    parentCanvas->concat(inverted);
  }
  auto scale = GetScaleFactor(filterList.get(), contentBounds);
  auto contentSurface = SurfaceUtil::MakeContentSurface(parentCanvas, contentBounds,
                                                        filterList->scaleFactorLimit, scale);
  if (contentSurface == nullptr) {
    return;
  }
  auto contentCanvas = contentSurface->getCanvas();
  if (filterList->useParentSizeInput) {
    contentCanvas->concat(filterList->layerMatrix);
  }
  content->draw(contentCanvas, cache);
  auto filterSource = ToFilterSource(contentCanvas);
  std::shared_ptr<tgfx::Surface> targetSurface = nullptr;
  std::unique_ptr<FilterTarget> filterTarget = GetDirectFilterTarget(
      parentCanvas, filterList.get(), filterNodes, contentBounds, filterSource->scale);
  if (filterTarget == nullptr) {
    // 需要离屏绘制
    targetSurface = SurfaceUtil::MakeContentSurface(parentCanvas, filterNodes.back().bounds,
                                                    filterList->scaleFactorLimit, scale,
                                                    filterNodes.back().filter->needsMSAA());
    if (targetSurface == nullptr) {
      return;
    }
    filterTarget = GetOffscreenFilterTarget(targetSurface.get(), filterNodes, contentBounds,
                                            filterSource->scale);
  }

  // 必须要flush，要不然framebuffer还没真正画到canvas，就被其他图层的filter串改了该framebuffer
  parentCanvas->flush();
  auto context = parentCanvas->getContext();
  ApplyFilters(context, filterNodes, contentBounds, filterSource.get(), filterTarget.get());
  // Reset the GL states stored in the context, they may be modified during the filter being applied.
  context->resetState();

  if (targetSurface) {
    tgfx::Matrix drawingMatrix = {};
    auto targetCanvas = targetSurface->getCanvas();
    if (!targetCanvas->getMatrix().invert(&drawingMatrix)) {
      drawingMatrix.setIdentity();
    }
    parentCanvas->drawTexture(targetSurface->getTexture(), drawingMatrix);
  }
}
}  // namespace pag
