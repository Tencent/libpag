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

#include "Filter3DFactory.h"
#include <cmath>
#include "rendering/filters/Transform3DFilter.h"
#include "rendering/renderers/FilterRenderer.h"

namespace pag {

std::shared_ptr<tgfx::Image> Apply3DEffects(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                            const FilterList* filterList,
                                            const tgfx::Rect& clipBounds,
                                            const tgfx::Point& sourceScale,
                                            tgfx::Rect* filterBounds, tgfx::Point* offset) {
  offset->set(0, 0);
  if (Has3DTransform(filterList->layer)) {
    auto context = filterList->transform3DContext;
    if (context == nullptr) {
      filterBounds->setEmpty();
      return nullptr;
    }
    auto oldBounds = *filterBounds;
    *filterBounds = tgfx::Rect::MakeWH(context->width, context->height);
    if (!filterBounds->intersect(clipBounds)) {
      return nullptr;
    }
    return Transform3DFilter::Apply(input, cache, filterList->layer, filterList->layerFrame,
                                    oldBounds, sourceScale, *context, offset);
  }
  offset->set(0, 0);
  return input;
}

// Depth attachment support is reported by a dedicated capability bit, because the OpenGL format
// query only covers color attachments.
bool Has3DSupport(const tgfx::GPU* gpu) {
  if (gpu == nullptr) {
    return false;
  }
  auto limits = gpu->limits();
  return gpu->features()->stencilAttachmentSupported && limits->maxTextureDimension2D > 0 &&
         limits->maxSamplersPerShaderStage >= 3 && limits->maxUniformBufferBindingSize >= 64 &&
         gpu->isFormatRenderable(tgfx::PixelFormat::RGBA_8888);
}

float Clamp3DScale(const tgfx::GPU* gpu, size_t memoryBudget, int textureCount, float width,
                   float height, float scale) {
  if (gpu == nullptr || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(scale) ||
      width <= 0 || height <= 0 || scale <= 0) {
    return 0;
  }
  auto limit = gpu->limits()->maxTextureDimension2D;
  if (limit <= 0) {
    return 0;
  }
  auto maximumScale = std::nextafter(static_cast<float>(limit) / std::max(width, height), 0.0f);
  scale = std::min(scale, maximumScale);
  if (memoryBudget > 0 && textureCount > 0) {
    // Estimate the working set from textureCount full-size 4-byte-per-pixel textures at the target
    // resolution and downsample by area ratio when over budget, so a 4K composition at a high
    // scale does not take gigabytes of VRAM.
    double perPixel = static_cast<double>(textureCount) * 4.0;
    double budgetScale = std::sqrt(static_cast<double>(memoryBudget) /
                                   (perPixel * static_cast<double>(width) * height));
    scale = std::min(scale, static_cast<float>(budgetScale));
  }
  return scale;
}

bool Has3DTransform(const Layer* layer) {
  while (layer != nullptr) {
    if (layer->transform3D != nullptr) {
      return true;
    }
    layer = layer->parent;
  }
  return false;
}

bool NeedsClipSpaceYFlip() {
#ifdef TGFX_USE_WEBGPU
  return true;
#else
  return false;
#endif
}

}  // namespace pag
