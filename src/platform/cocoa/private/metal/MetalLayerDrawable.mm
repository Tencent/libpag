/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(TGFX_USE_METAL)

#import "platform/cocoa/private/metal/MetalLayerDrawable.h"

#include <cmath>
#include "tgfx/core/Surface.h"

namespace pag {

std::shared_ptr<MetalLayerDrawable> MetalLayerDrawable::FromLayer(CAMetalLayer* layer) {
  if (layer == nil) {
    return nullptr;
  }
  // Fail fast when Metal is unavailable (no usable MTLDevice): create the window eagerly so
  // FromLayer returns nil instead of a surface that silently never renders. The window is
  // kept and reused by getDevice(), so this costs nothing on the happy path.
  auto window = tgfx::MetalWindow::MakeFrom(layer);
  if (window == nullptr) {
    return nullptr;
  }
  auto drawable = std::shared_ptr<MetalLayerDrawable>(new MetalLayerDrawable(layer));
  drawable->window = std::move(window);
  return drawable;
}

MetalLayerDrawable::MetalLayerDrawable(CAMetalLayer* layer) : layer(layer) {
  // Do not retain the layer — retaining it would risk a strong reference cycle in typical UIKit /
  // AppKit setups where the layer is bound to a view. The caller keeps the layer alive for the
  // drawable's lifetime; the contract is documented on the public Metal category API.
  updateSize();
}

void MetalLayerDrawable::updateSize() {
  auto drawableSize = layer.drawableSize;
  if (drawableSize.width <= 0 || drawableSize.height <= 0) {
    // drawableSize can be zero before the layer has been laid out (first frame). Fall back to
    // bounds * contentsScale so callers still see a sensible size.
    auto bounds = layer.bounds;
    auto scale = layer.contentsScale;
    drawableSize.width = bounds.size.width * scale;
    drawableSize.height = bounds.size.height * scale;
  }
  _width = static_cast<int>(roundf(drawableSize.width));
  _height = static_cast<int>(roundf(drawableSize.height));
}

std::shared_ptr<tgfx::Device> MetalLayerDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    window = tgfx::MetalWindow::MakeFrom(layer);
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> MetalLayerDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(context, window);
}

void MetalLayerDrawable::onFreeSurface() {
}

void MetalLayerDrawable::present(tgfx::Context*) {
  // tgfx's MetalWindow presents through the DrawingBuffer::presentWindows() path invoked by
  // Context::submit(). Nothing to do here.
}

}  // namespace pag

#endif  // TGFX_USE_METAL
