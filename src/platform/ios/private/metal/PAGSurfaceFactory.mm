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

#include "platform/cocoa/private/PAGSurfaceFactory.h"
#include "platform/cocoa/private/metal/MetalLayerDrawable.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "tgfx/gpu/metal/MetalDevice.h"

namespace pag::cocoa {

std::shared_ptr<PAGSurface> MakeSurfaceFromHost(id host) {
  return PAGSurface::MakeFrom(MetalLayerDrawable::FromLayer(host));
}

std::shared_ptr<PAGSurface> MakeSurfaceFromPixelBuffer(CVPixelBufferRef pixelBuffer, id device) {
  std::shared_ptr<tgfx::MetalDevice> metalDevice = nullptr;
  if (device != nil) {
    metalDevice = tgfx::MetalDevice::MakeFrom(device);
  }
  return PAGSurface::MakeFrom(HardwareBufferDrawable::MakeFrom(pixelBuffer, metalDevice));
}

}  // namespace pag::cocoa
