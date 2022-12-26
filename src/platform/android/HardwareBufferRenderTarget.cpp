/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "HardwareBufferRenderTarget.h"
#include "tgfx/core/Bitmap.h"

namespace pag {
std::shared_ptr<HardwareBufferRenderTarget> HardwareBufferRenderTarget::MakeFrom(
    tgfx::Context* context, std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer,
    std::shared_ptr<tgfx::Texture> texture) {
  if (!hardwareBuffer || texture == nullptr) {
    return nullptr;
  }
  auto glTexture = std::static_pointer_cast<tgfx::GLTexture>(texture);
  auto renderTarget = tgfx::GLRenderTarget::MakeFrom(glTexture.get(), -1);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto rt = new HardwareBufferRenderTarget(hardwareBuffer, renderTarget);
  return tgfx::Resource::Wrap(context, rt);
}

HardwareBufferRenderTarget::HardwareBufferRenderTarget(
    std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer,
    std::shared_ptr<tgfx::GLRenderTarget> glRenderTarget)
    : tgfx::GLRenderTarget(glRenderTarget->width(), glRenderTarget->height(),
                           glRenderTarget->origin(), -1, {}, {}),
      hardwareBuffer(hardwareBuffer), glRenderTarget(glRenderTarget) {
}

bool HardwareBufferRenderTarget::readPixels(const tgfx::ImageInfo& dstInfo, void* dstPixels,
                                            int srcX, int srcY) const {
  dstPixels = dstInfo.computeOffset(dstPixels, -srcX, -srcY);
  auto outInfo = dstInfo.makeIntersect(-srcX, -srcY, width(), height());
  if (outInfo.isEmpty()) {
    return false;
  }
  auto srcPixels = hardwareBuffer->lockPixels();
  if (!srcPixels) {
    return false;
  }
  tgfx::Bitmap bitmap(hardwareBuffer->info(), srcPixels);
  bitmap.readPixels(dstInfo, dstPixels);
  hardwareBuffer->unlockPixels();
  return true;
}

const tgfx::Swizzle& HardwareBufferRenderTarget::writeSwizzle() const {
  return glRenderTarget->writeSwizzle();
}

void HardwareBufferRenderTarget::onReleaseGPU() {
  // glRenderTarget has been wrapped in Resource, so no need to explicitly release
}

void HardwareBufferRenderTarget::resolve() const {
  glRenderTarget->resolve();
}

tgfx::GLFrameBuffer HardwareBufferRenderTarget::glFrameBuffer() const {
  return glRenderTarget->glFrameBuffer();
}
}  // namespace pag
