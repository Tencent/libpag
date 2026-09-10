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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(TGFX_USE_METAL)

#import "MetalGPUDrawable.h"

#import <MetalKit/MetalKit.h>
#include "tgfx/core/Surface.h"

namespace pag {

std::shared_ptr<MetalGPUDrawable> MetalGPUDrawable::FromLayer(CAMetalLayer* layer) {
  if (layer == nil) {
    return nullptr;
  }
  return std::shared_ptr<MetalGPUDrawable>(new MetalGPUDrawable(layer));
}

std::shared_ptr<MetalGPUDrawable> MetalGPUDrawable::FromView(MTKView* view) {
  if (view == nil) {
    return nullptr;
  }
  auto layer = static_cast<CAMetalLayer*>(view.layer);
  if (layer == nil) {
    return nullptr;
  }
  return std::shared_ptr<MetalGPUDrawable>(new MetalGPUDrawable(view, layer));
}

MetalGPUDrawable::MetalGPUDrawable(CAMetalLayer* layer) : layer(layer) {
  // Do not retain layer here — the ObjC facade already owns the layer for the drawable's
  // lifetime, and retaining it here would risk a strong reference cycle in typical UIKit /
  // AppKit setups where the layer is bound to a view.
  updateSize();
}

MetalGPUDrawable::MetalGPUDrawable(MTKView* view, CAMetalLayer* layer) : layer(layer), view(view) {
  updateSize();
}

void MetalGPUDrawable::updateSize() {
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

std::shared_ptr<tgfx::Device> MetalGPUDrawable::getDevice() {
  if (_width <= 0 || _height <= 0) {
    return nullptr;
  }
  if (window == nullptr) {
    if (view != nil) {
      window = tgfx::MetalWindow::MakeFrom(view);
    } else {
      window = tgfx::MetalWindow::MakeFrom(layer);
    }
  }
  return window ? window->getDevice() : nullptr;
}

std::shared_ptr<tgfx::Surface> MetalGPUDrawable::onCreateSurface(tgfx::Context* context) {
  if (window == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(context, window);
}

void MetalGPUDrawable::onFreeSurface() {
}

void MetalGPUDrawable::present(tgfx::Context*) {
  // tgfx's MetalWindow presents through the DrawingBuffer::presentWindows() path invoked by
  // Context::submit(). Nothing to do here.
}

}  // namespace pag

#endif  // TGFX_USE_METAL
