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

#pragma once

#import <QuartzCore/QuartzCore.h>
#import "PAGSurface.h"

PAG_API @interface PAGSurface(Metal)

/**
 * Creates a new PAGSurface from a CAMetalLayer. The MTLDevice on the layer (or the system
 * default) is adopted internally. Multiple PAGSurfaces whose layers share the same MTLDevice
 * share the same GPU caches. The caller must keep the CAMetalLayer alive for the lifetime of the
 * PAGSurface — libpag does not retain it (retaining would risk a reference cycle in typical
 * view-layer setups), so releasing the layer while the PAGSurface is still in use is undefined
 * behavior. The caller must also maintain layer.drawableSize (in pixels) — CAMetalLayer does not
 * derive it from bounds automatically, and if it stays at zero the surface reports a fallback
 * size but never actually renders. When the size changes, update layer.drawableSize and call
 * updateSize on the PAGSurface.
 */
+ (PAGSurface*)FromLayer:(CAMetalLayer*)metalLayer;

@end
