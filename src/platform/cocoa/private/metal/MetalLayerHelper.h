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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

namespace pag {
namespace cocoa {

/**
 * Sets up the shared CAMetalLayer properties for a PAGView-backed layer: the BGRA8Unorm pixel
 * format and the framebufferOnly optimization. Shared by the iOS and macOS PAGView so the two
 * platforms cannot drift apart.
 */
inline void SetUpPAGMetalLayer(CAMetalLayer* layer) {
  if (layer == nil) {
    return;
  }
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = YES;
}

/**
 * Computes the drawable size (in pixels) from the given logical size and scale factor, and syncs
 * layer.contentsScale with the scale. Callers on macOS must also keep contentsScale in sync with
 * window.backingScaleFactor, which NSView does not track automatically.
 */
inline void UpdateMetalLayerDrawableSize(CAMetalLayer* layer, CGSize size, CGFloat scale) {
  if (layer == nil) {
    return;
  }
  layer.contentsScale = scale;
  layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
}

}  // namespace cocoa
}  // namespace pag
