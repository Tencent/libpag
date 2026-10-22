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
#include "EAGLDrawable.h"
#include "base/utils/Log.h"
#include "rendering/drawables/HardwareBufferDrawable.h"
#include "tgfx/gpu/opengl/eagl/EAGLDevice.h"

namespace pag::cocoa {

std::shared_ptr<PAGSurface> MakeSurfaceFromHost(id host) {
  return PAGSurface::MakeFrom(EAGLDrawable::FromLayer(host));
}

#if TARGET_IPHONE_SIMULATOR
// Only EAGL disables hardware buffers on the simulator; Metal supports arm64 simulators.
std::shared_ptr<PAGSurface> MakeSurfaceFromPixelBuffer(CVPixelBufferRef, id) {
  LOGE("The simulator does not support [PAGSurface FromCVPixelBuffer:context:].");
  return nullptr;
}
#else
std::shared_ptr<PAGSurface> MakeSurfaceFromPixelBuffer(CVPixelBufferRef pixelBuffer, id device) {
  auto eaglDevice = tgfx::EAGLDevice::MakeFrom(device);
  return PAGSurface::MakeFrom(HardwareBufferDrawable::MakeFrom(pixelBuffer, eaglDevice));
}
#endif

}  // namespace pag::cocoa
