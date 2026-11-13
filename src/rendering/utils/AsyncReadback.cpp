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
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "rendering/utils/AsyncReadback.h"
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/GPUBuffer.h"

namespace pag {

std::shared_ptr<tgfx::SurfaceReadback> StartAsyncReadback(
    tgfx::Context* context, const std::shared_ptr<tgfx::Surface>& surface, bool* flipY) {
  if (context == nullptr || surface == nullptr) {
    return nullptr;
  }
  auto readback = surface->asyncReadPixels(tgfx::Rect::MakeWH(
      static_cast<float>(surface->width()), static_cast<float>(surface->height())));
  if (readback == nullptr) {
    return nullptr;
  }
  context->flushAndSubmit();
  auto buffer = readback->getGPUBuffer(context);
  if (buffer == nullptr) {
    return nullptr;
  }
  buffer->requestMapAsync();
  if (flipY != nullptr) {
    *flipY = surface->origin() == tgfx::ImageOrigin::BottomLeft;
  }
  return readback;
}

tgfx::ReadbackStatus PollAsyncReadback(tgfx::Context* context,
                                       const std::shared_ptr<tgfx::SurfaceReadback>& readback) {
  if (context == nullptr || readback == nullptr) {
    return tgfx::ReadbackStatus::Failed;
  }
  auto status = readback->status(context);
  if (status != tgfx::ReadbackStatus::Pending) {
    return status;
  }
  auto buffer = readback->getGPUBuffer(context);
  if (buffer == nullptr) {
    return tgfx::ReadbackStatus::Failed;
  }
  buffer->requestMapAsync();
  return status;
}

}  // namespace pag
