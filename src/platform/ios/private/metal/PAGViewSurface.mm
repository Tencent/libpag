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

#include "platform/cocoa/private/PAGViewSurface.h"
#include "platform/cocoa/private/metal/MetalLayerHelper.h"
#import "platform/ios/PAGSurface+Metal.h"
#import "platform/ios/PAGView.h"

namespace pag::cocoa {

static void UpdateDrawableSize(PAGView* view) {
  UpdateMetalLayerDrawableSize((CAMetalLayer*)view.layer, view.bounds.size,
                               view.contentScaleFactor);
}

Class PAGViewSurface::LayerClass() {
  return [CAMetalLayer class];
}

void PAGViewSurface::Configure(id) {
}

void PAGViewSurface::ObserveSurfacePrepared(id, SEL) {
}

::PAGSurface* PAGViewSurface::CreateSurface(id view) {
  PAGView* pagView = view;
  auto layer = (CAMetalLayer*)pagView.layer;
  SetUpPAGMetalLayer(layer);
  UpdateDrawableSize(pagView);
  return [::PAGSurface FromLayer:layer];
}

void PAGViewSurface::UpdateSize(id view, ::PAGSurface* surface) {
  UpdateDrawableSize(view);
  [surface updateSize];
}

void PAGViewSurface::UpdateWindowSize(id view, ::PAGSurface* surface) {
  PAGView* pagView = view;
  if (pagView.window == nil || surface == nil) {
    return;
  }
  auto size = pagView.bounds.size;
  auto scale = pagView.contentScaleFactor;
  auto drawableSize = CGSizeMake(size.width * scale, size.height * scale);
  auto layer = (CAMetalLayer*)pagView.layer;
  // Reattaching at the same size must not discard rendering caches.
  if (!CGSizeEqualToSize(layer.drawableSize, drawableSize)) {
    UpdateSize(view, surface);
  }
}

std::unique_lock<std::mutex> PAGViewSurface::Lock(std::mutex& mutex) {
  return std::unique_lock<std::mutex>(mutex);
}

}  // namespace pag::cocoa
