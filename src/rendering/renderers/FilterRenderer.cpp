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

#include "FilterRenderer.h"
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/BrightnessContrastFilter.h"
#include "rendering/filters/BulgeFilter.h"
#include "rendering/filters/CornerPinFilter.h"
#include "rendering/filters/DisplacementMapFilter.h"
#include "rendering/filters/FilterModifier.h"
#include "rendering/filters/HueSaturationFilter.h"
#include "rendering/filters/LayerStylesFilter.h"
#include "rendering/filters/LevelsIndividualFilter.h"
#include "rendering/filters/MosaicFilter.h"
#include "rendering/filters/MotionBlurFilter.h"
#include "rendering/filters/MotionTileFilter.h"
#include "rendering/filters/RadialBlurFilter.h"
#include "rendering/filters/gaussianblur/GaussianBlurFilter.h"
#include "rendering/filters/glow/GlowFilter.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "tgfx/core/Recorder.h"

namespace pag {

static float GetScaleFactorLimit(Layer* layer) {
  if (layer->transform3D != nullptr) {
    return FLT_MAX;
  }
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

tgfx::Rect GetClipBounds(Canvas* canvas, const FilterList* filterList,
                         const tgfx::Rect& contentBounds) {
  auto clip = canvas->getTotalClip();
  if (clip.isInverseFillType()) {
    return contentBounds;
  }
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

static std::shared_ptr<tgfx::Image> ApplyMotionBlur(std::shared_ptr<tgfx::Image> input,
                                                    const FilterList* filterList,
                                                    const tgfx::Rect& clipBounds,
                                                    tgfx::Rect* filterBounds, tgfx::Point* offset) {
  offset->set(0, 0);
  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    if (MotionBlurFilter::ShouldSkipFilter(filterList->layer, filterList->layerFrame)) {
      return input;
    }
    auto oldBounds = *filterBounds;
    MotionBlurFilter::TransformBounds(filterBounds, filterList->layer, filterList->layerFrame);
    filterBounds->roundOut();
    if (!filterBounds->intersect(clipBounds)) {
      return nullptr;
    }
    return MotionBlurFilter::Apply(input, filterList->layer, filterList->layerFrame, oldBounds,
                                   offset);
  }
  return input;
}

std::shared_ptr<tgfx::Image> ApplyFilter(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                         Layer* layer, RenderCache* cache,
                                         const tgfx::Matrix& layerMatrix, Frame layerFrame,
                                         const tgfx::Rect& filterBounds,
                                         const tgfx::Point& effectScale,
                                         const tgfx::Point& sourceScale, tgfx::Point* offset) {
  switch (effect->type()) {
    case EffectType::CornerPin:
      return CornerPinFilter::Apply(std::move(input), effect, layerFrame, sourceScale, offset);
    case EffectType::Bulge:
      return BulgeFilter::Apply(std::move(input), effect, layerFrame, filterBounds, offset);
    case EffectType::MotionTile:
      return MotionTileFilter::Apply(std::move(input), effect, layerFrame, filterBounds, offset);
    case EffectType::Glow:
      return GlowFilter::Apply(std::move(input), effect, layerFrame, offset);
    case EffectType::LevelsIndividual:
      return LevelsIndividualFilter::Apply(std::move(input), effect, layerFrame, offset);
    case EffectType::FastBlur:
      return GaussianBlurFilter::Apply(std::move(input), effect, layerFrame, effectScale,
                                       sourceScale, offset);
    case EffectType::DisplacementMap:
      return DisplacementMapFilter::Apply(std::move(input), effect, layer, cache, layerMatrix,
                                          layerFrame, filterBounds, offset);
    case EffectType::RadialBlur:
      return RadialBlurFilter::Apply(std::move(input), effect, layerFrame, filterBounds, offset);
    case EffectType::Mosaic:
      return MosaicFilter::Apply(std::move(input), effect, layerFrame, offset);
    case EffectType::BrightnessContrast:
      return BrightnessContrastFilter::Apply(std::move(input), effect, layerFrame, offset);
    case EffectType::HueSaturation:
      return HueSaturationFilter::Apply(std::move(input), effect, layerFrame, offset);
    default:
      return nullptr;
  }
}

std::shared_ptr<tgfx::Image> ApplyEffects(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                          const FilterList* filterList,
                                          const tgfx::Rect& clipBounds,
                                          const tgfx::Point& sourceScale, int clipStartIndex,
                                          tgfx::Rect* filterBounds, tgfx::Point* outputOffset) {
  auto effectIndex = 0;
  outputOffset->set(0, 0);
  for (auto& effect : filterList->effects) {
    auto oldBounds = *filterBounds;
    effect->transformBounds(ToPAG(filterBounds), ToPAG(filterList->effectScale),
                            filterList->layerFrame);
    if (effectIndex >= clipStartIndex && !filterBounds->intersect(clipBounds)) {
      return nullptr;
    }
    filterBounds->roundOut();
    tgfx::Point filterOffset = {0, 0};
    input = ApplyFilter(std::move(input), effect, filterList->layer, cache, filterList->layerMatrix,
                        filterList->layerFrame, oldBounds, filterList->effectScale, sourceScale,
                        &filterOffset);
    if (!input) {
      return nullptr;
    }
    *outputOffset += filterOffset;
    effectIndex++;
  }
  return input;
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

static int GetClipStartIndex(const std::vector<Effect*>& effects) {
  for (int i = static_cast<int>(effects.size()) - 1; i >= 0; i--) {
    const auto& effect = effects[i];
    if (!effect->processVisibleAreaOnly()) {
      return i;
    }
  }
  return -1;
}

std::shared_ptr<tgfx::Image> FilterRenderer::ApplyFilters(
    std::shared_ptr<tgfx::Image> input, RenderCache* cache, const FilterList* filterList,
    const tgfx::Point& contentScale, tgfx::Rect contentBounds, tgfx::Rect clipBounds,
    int clipStartIndex, tgfx::Point* outputOffset) {
  auto output = input;
  auto filterBounds = contentBounds;

  auto offset = tgfx::Point::Zero();
  output = ApplyEffects(output, cache, filterList, clipBounds, contentScale, clipStartIndex,
                        &filterBounds, &offset);
  if (!output) {
    return input;
  }
  *outputOffset += offset;

  output = Apply3DEffects(output, filterList, clipBounds, contentScale, &filterBounds, &offset);
  if (!output) {
    return input;
  }
  *outputOffset += offset;

  output = ApplyMotionBlur(output, filterList, clipBounds, &filterBounds, &offset);
  if (!output) {
    return input;
  }
  *outputOffset += offset;
  return output;
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

tgfx::Matrix GetLayerMatrix(const FilterList* filterList, float contentScale) {
  auto matrix = tgfx::Matrix::MakeScale(contentScale);
  if (filterList->useParentSizeInput) {
    matrix.preConcat(filterList->layerMatrix);
  }
  return matrix;
}

static std::shared_ptr<tgfx::Picture> CreateSource(RenderCache* cache, const tgfx::Matrix& matrix,
                                                   std::shared_ptr<Graphic> content) {
  tgfx::Recorder recorder;
  auto canvas = recorder.beginRecording();
  auto contentCanvas = Canvas(canvas, cache);
  contentCanvas.concat(matrix);
  content->draw(&contentCanvas);
  return recorder.finishRecordingAsPicture();
}

void FilterRenderer::DrawWithFilter(Canvas* parentCanvas, const FilterModifier* modifier,
                                    std::shared_ptr<Graphic> content) {
  auto cache = parentCanvas->getCache();
  auto filterList = MakeFilterList(modifier);
  auto contentBounds = GetContentBounds(filterList.get(), content);
  // 相对于content Bounds的clip Bounds
  auto clipBounds = GetClipBounds(parentCanvas, filterList.get(), contentBounds);

  auto contentScale = GetContentScale(parentCanvas, filterList->scaleFactorLimit,
                                      GetScaleFactor(filterList.get(), contentBounds));

  int clipStartIndex = GetClipStartIndex(filterList->effects);
  bool skipClipBounds = NeedToSkipClipBounds(filterList.get());
  if (filterList->layer->motionBlur && !filterList->layer->transform3D) {
    // MotionBlur Fragment Shader中需要用到裁切区域外的像素，
    // 因此默认先把裁切区域过一次TransformBounds
    MotionBlurFilter::TransformBounds(&clipBounds, filterList->layer, filterList->layerFrame);
    clipBounds.roundOut();
  }

  auto filterBounds = contentBounds;  // filterBounds使用contentBounds未裁剪的初始值
  if (!skipClipBounds && clipStartIndex == -1 && !contentBounds.intersect(clipBounds)) {
    // 裁剪后的内容区域为空，直接绘制原始内容
    return content->draw(parentCanvas);
  }

  auto contentMatrix = GetLayerMatrix(filterList.get(), contentScale);

  auto sourcePicture = CreateSource(cache, contentMatrix, content);
  if (sourcePicture == nullptr) {
    return;
  }

  tgfx::Point sourceScale = {};
  sourceScale.x = sourceScale.y = GetMaxScaleFactor(contentMatrix);

  auto totalOffset = tgfx::Point::Zero();
  auto offset = tgfx::Point::Zero();
  auto inputBounds = contentBounds;
  inputBounds.scale(sourceScale.x, sourceScale.y);
  auto input = CreatePictureImage(sourcePicture, &totalOffset, &inputBounds);

  auto output = ApplyFilters(input, cache, filterList.get(), sourceScale, filterBounds, clipBounds,
                             clipStartIndex, &offset);
  totalOffset += offset;

  parentCanvas->save();
  tgfx::Matrix inverted = tgfx::Matrix::I();
  contentMatrix.invert(&inverted);
  parentCanvas->concat(inverted);
  if (!filterList->layerStyles.empty()) {
    parentCanvas->translate(totalOffset.x, totalOffset.y);
    auto filter = LayerStylesFilter::Make(filterList->layerStyles, filterList->layerFrame,
                                          contentScale, filterList->layerStyleScale);
    filter->applyFilter(parentCanvas, std::move(output));
  } else if (input == output) {
    parentCanvas->drawPicture(sourcePicture);
  } else {
    auto canvasMatrix = parentCanvas->getMatrix();
    parentCanvas->translate(totalOffset.x, totalOffset.y);
    parentCanvas->drawImage(output);
    parentCanvas->setMatrix(canvasMatrix);
  }
  parentCanvas->restore();
}
}  // namespace pag
