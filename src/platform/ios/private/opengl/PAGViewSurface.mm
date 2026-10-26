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
#include "EAGLDrawable.h"
#import "platform/ios/PAGSurface+OpenGL.h"
#import "platform/ios/PAGView.h"

namespace pag::cocoa {

Class PAGViewSurface::LayerClass() {
  return [CAEAGLLayer class];
}

void PAGViewSurface::ObserveSurfacePrepared(id view, SEL callback) {
  // EAGLDrawable defers CAEAGLLayer surface creation to the main queue to avoid issue #1870.
  [[NSNotificationCenter defaultCenter] addObserver:view
                                           selector:callback
                                               name:AsyncSurfacePreparedNotification
                                             object:[view layer]];
}

::PAGSurface* PAGViewSurface::CreateSurface(id view) {
  PAGView* pagView = view;
  id layer = pagView.layer;
  return [::PAGSurface FromLayer:layer];
}

void PAGViewSurface::UpdateSize(id, ::PAGSurface* surface) {
  [surface updateSize];
}

void PAGViewSurface::UpdateWindowSize(id, ::PAGSurface*) {
}

std::unique_lock<std::mutex> PAGViewSurface::Lock(std::mutex& mutex) {
  return std::unique_lock<std::mutex>(mutex);
}

}  // namespace pag::cocoa
