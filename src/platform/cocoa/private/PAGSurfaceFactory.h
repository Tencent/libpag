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

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#include "pag/pag.h"

namespace pag::cocoa {

/**
 * Creates a surface from the selected backend's native host: a layer on iOS and Metal, or an NSView
 * on macOS OpenGL. The caller keeps the host alive for the lifetime of the surface. Returns nullptr
 * if the host is nil or has an incompatible type for the selected backend.
 */
__attribute__((visibility("hidden"))) std::shared_ptr<PAGSurface> MakeSurfaceFromHost(id host);

#if TARGET_OS_IPHONE
/**
 * Creates an iOS pixel-buffer surface using an optional native device or context for the selected
 * backend. The caller retains ownership of the pixel buffer and native device. A nil device uses
 * the default device. Returns nullptr if a non-nil device has an incompatible type for the backend.
 */
__attribute__((visibility("hidden"))) std::shared_ptr<PAGSurface> MakeSurfaceFromPixelBuffer(
    CVPixelBufferRef pixelBuffer, id device);
#endif

}  // namespace pag::cocoa
