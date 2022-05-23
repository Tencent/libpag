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

#include "GaussianBlurFilter.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
GaussianBlurFilter::GaussianBlurFilter(Effect* effect) : effect(effect) {
  auto* blurEffect = static_cast<FastBlurEffect*>(effect);

  blurParam.repeatEdgePixels = blurEffect->repeatEdgePixels->getValueAt(0);
  blurParam.blurDimensions = blurEffect->blurDimensions->getValueAt(0);

  BlurOptions options = BlurOptions::None;

  if (blurParam.repeatEdgePixels) {
    options |= BlurOptions::RepeatEdgePixels;
  }

  if (blurParam.blurDimensions == BlurDimensionsDirection::Vertical) {
    options |= BlurOptions::Vertical;
  } else if (blurParam.blurDimensions == BlurDimensionsDirection::Horizontal) {
    options |= BlurOptions::Horizontal;
  } else {
    options |= BlurOptions::Horizontal | BlurOptions::Vertical;
  }

  downBlurPass = new GaussianBlurFilterPass(options | BlurOptions::Down);
  upBlurPass = new GaussianBlurFilterPass(options | BlurOptions::Up);
}

GaussianBlurFilter::~GaussianBlurFilter() {
  delete downBlurPass;
  delete upBlurPass;
  bufferCache.clear();
}

bool GaussianBlurFilter::initialize(tgfx::Context* context) {
  if (!downBlurPass->initialize(context)) {
    return false;
  }
  if (!upBlurPass->initialize(context)) {
    return false;
  }

  return true;
}

void GaussianBlurFilter::updateBlurParam(float blurriness) {
  blurriness = blurriness < BLUR_LEVEL_MAX_LIMIT ? blurriness : BLUR_LEVEL_MAX_LIMIT;
  if (blurriness < BLUR_LEVEL_1_LIMIT) {
    blurParam.depth = BLUR_LEVEL_1_DEPTH;
    blurParam.scale = BLUR_LEVEL_1_SCALE;
    blurParam.value = blurriness / BLUR_LEVEL_1_LIMIT * 2.0;
  } else if (blurriness < BLUR_LEVEL_2_LIMIT) {
    blurParam.depth = BLUR_LEVEL_2_DEPTH;
    blurParam.scale = BLUR_LEVEL_2_SCALE;
    blurParam.value = (blurriness - BLUR_STABLE) / (BLUR_LEVEL_2_LIMIT - BLUR_STABLE) * 3.0;
  } else if (blurriness < BLUR_LEVEL_3_LIMIT) {
    blurParam.depth = BLUR_LEVEL_3_DEPTH;
    blurParam.scale = BLUR_LEVEL_3_SCALE;
    blurParam.value = (blurriness - BLUR_STABLE) / (BLUR_LEVEL_3_LIMIT - BLUR_STABLE) * 5.0;
  } else if (blurriness < BLUR_LEVEL_4_LIMIT) {
    blurParam.depth = BLUR_LEVEL_4_DEPTH;
    blurParam.scale = BLUR_LEVEL_4_SCALE;
    blurParam.value = (blurriness - BLUR_STABLE) / (BLUR_LEVEL_4_LIMIT - BLUR_STABLE) * 6.0;
  } else {
    blurParam.depth = BLUR_LEVEL_5_DEPTH;
    blurParam.scale = BLUR_LEVEL_5_SCALE;
    blurParam.value =
        6.0 + (blurriness - BLUR_STABLE * 12.0) / (BLUR_LEVEL_5_LIMIT - BLUR_STABLE * 12.0) * 5.0;
  }
}

std::shared_ptr<FilterBuffer> GaussianBlurFilter::getBuffer(tgfx::Context* context, int width,
                                                            int height) {
  std::shared_ptr<FilterBuffer> filterBuffer = nullptr;
  for (auto buffer = bufferCache.begin(); buffer != bufferCache.end(); ++buffer) {
    if ((*buffer)->width() == width && (*buffer)->height() == height) {
      filterBuffer = *buffer;
      bufferCache.erase(buffer);
      break;
    }
  }

  if (filterBuffer == nullptr) {
    filterBuffer = FilterBuffer::Make(context, width, height);
  }

  return filterBuffer;
}

void GaussianBlurFilter::cacheBuffer(std::shared_ptr<FilterBuffer> buffer) {
  bufferCache.push_back(buffer);
}

void GaussianBlurFilter::update(Frame frame, const tgfx::Rect& contentBounds,
                                const tgfx::Rect& transformedBounds,
                                const tgfx::Point& filterScale) {
  LayerFilter::update(frame, contentBounds, transformedBounds, filterScale);

  auto blurriness = static_cast<FastBlurEffect*>(effect)->blurriness->getValueAt(layerFrame);
  updateBlurParam(blurriness);

  auto bounds = contentBounds;
  auto scale = blurParam.repeatEdgePixels
                   ? tgfx::Point::Make(1.0, 1.0)
                   : tgfx::Point::Make(1.0f + BLUR_EXPEND * 2.0f, 1.0f + BLUR_EXPEND * 2.0f);

  for (int i = 0; i < blurParam.depth; i++) {
    filtersBounds[i].inputBounds = bounds;
    bounds = tgfx::Rect::MakeLTRB(
        floor(bounds.left * blurParam.scale), floor(bounds.top * blurParam.scale),
        floor(bounds.right * blurParam.scale), floor(bounds.bottom * blurParam.scale));

    filtersBounds[i].outputBounds = bounds;
    bounds = tgfx::Rect::MakeWH(bounds.width(), bounds.height());
  }

  for (int i = blurParam.depth; i < blurParam.depth * 2; i++) {
    filtersBounds[i].inputBounds = bounds;
    bounds = tgfx::Rect::MakeWH(floor(bounds.width() / blurParam.scale),
                                floor(bounds.height() / blurParam.scale));
    if (!blurParam.repeatEdgePixels && i == blurParam.depth) {
      auto expandX = (blurParam.blurDimensions == BlurDimensionsDirection::All ||
                      blurParam.blurDimensions == BlurDimensionsDirection::Horizontal)
                         ? floor(bounds.width() * BLUR_EXPEND * filterScale.x)
                         : 0.0;
      auto expandY = (blurParam.blurDimensions == BlurDimensionsDirection::All ||
                      blurParam.blurDimensions == BlurDimensionsDirection::Vertical)
                         ? floor(bounds.height() * BLUR_EXPEND * filterScale.x)
                         : 0.0;
      filtersBounds[i].outputBounds = bounds;
      filtersBounds[i].outputBounds.outset(expandX, expandY);
      bounds =
          tgfx::Rect::MakeWH(floor(bounds.width() * scale.x), floor(bounds.height() * scale.y));
    } else if (i == blurParam.depth * 2 - 1) {
      filtersBounds[i].outputBounds = transformedBounds;
    } else {
      filtersBounds[i].outputBounds = bounds;
    }
  }
  filtersBoundsScale = filterScale;
}

void GaussianBlurFilter::draw(tgfx::Context* context, const FilterSource* source,
                              const FilterTarget* target) {
  if (source == nullptr || target == nullptr) {
    LOGE("GaussianBlurFilter::draw() can not draw filter");
    return;
  }

  std::shared_ptr<FilterBuffer> filterBuffer = nullptr;
  std::shared_ptr<FilterBuffer> filterBufferNeedToCache = nullptr;

  std::unique_ptr<FilterSource> filterSourcePtr;
  std::unique_ptr<FilterTarget> filterTargetPtr;

  FilterSource* filterSource = const_cast<FilterSource*>(source);
  FilterTarget* filterTarget = nullptr;

  for (int i = 0; i < blurParam.depth; i++) {
    auto sourceBounds = filtersBounds[i].inputBounds;
    auto targetBounds = filtersBounds[i].outputBounds;
    filterBuffer = getBuffer(context, targetBounds.width() * source->scale.x,
                             targetBounds.height() * source->scale.y);
    if (filterBuffer == nullptr) {
      return;
    }
    auto offsetMatrix =
        tgfx::Matrix::MakeTrans((sourceBounds.left - targetBounds.left) * source->scale.x,
                                (sourceBounds.top - targetBounds.top) * source->scale.y);
    filterBuffer->clearColor();
    filterTargetPtr = filterBuffer->toFilterTarget(offsetMatrix);
    filterTarget = filterTargetPtr.get();
    downBlurPass->update(layerFrame, sourceBounds, targetBounds, filtersBoundsScale);
    downBlurPass->updateParams(blurParam.value, blurParam.scale, false);
    downBlurPass->draw(context, filterSource, filterTarget);
    filterSourcePtr = filterBuffer->toFilterSource(source->scale);
    filterSource = filterSourcePtr.get();
    if (filterBufferNeedToCache != nullptr) {
      cacheBuffer(filterBufferNeedToCache);
      filterBufferNeedToCache = nullptr;
    }
    if (filterBuffer != nullptr) {
      filterBufferNeedToCache = filterBuffer;
    }
  }

  for (int i = blurParam.depth; i < blurParam.depth * 2; i++) {
    auto sourceBounds = filtersBounds[i].inputBounds;
    auto targetBounds = filtersBounds[i].outputBounds;
    if (i != blurParam.depth * 2 - 1) {
      filterBuffer = getBuffer(context, targetBounds.width() * source->scale.x,
                               targetBounds.height() * source->scale.y);
    }
    if (filterBuffer == nullptr && i != blurParam.depth * 2 - 1) {
      return;
    }
    auto offsetMatrix =
        tgfx::Matrix::MakeTrans((sourceBounds.left - targetBounds.left) * source->scale.x,
                                (sourceBounds.top - targetBounds.top) * source->scale.y);
    if (i == blurParam.depth * 2 - 1) {
      filterTarget = const_cast<FilterTarget*>(target);
      if (!blurParam.repeatEdgePixels) {
        filterTarget->vertexMatrix = {1, 0, 0, 0, 1, 0, 0, 0, 1};
      }
      PreConcatMatrix(filterTarget, offsetMatrix);
    } else {
      filterBuffer->clearColor();
      filterTargetPtr = filterBuffer->toFilterTarget(offsetMatrix);
      filterTarget = filterTargetPtr.get();
    }
    upBlurPass->update(layerFrame, sourceBounds, targetBounds, filtersBoundsScale);
    upBlurPass->updateParams(blurParam.value, blurParam.scale,
                             !blurParam.repeatEdgePixels && i == blurParam.depth);
    upBlurPass->draw(context, filterSource, filterTarget);
    if (i != blurParam.depth * 2 - 1) {
      filterSourcePtr = filterBuffer->toFilterSource(source->scale);
      filterSource = filterSourcePtr.get();
    }
    if (filterBufferNeedToCache != nullptr) {
      cacheBuffer(filterBufferNeedToCache);
      filterBufferNeedToCache = nullptr;
    }
    if (filterBuffer != nullptr) {
      filterBufferNeedToCache = filterBuffer;
    }
  }
}
}  // namespace pag
