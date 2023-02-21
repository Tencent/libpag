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

#include "tgfx/gpu/Surface.h"
#include "DrawingManager.h"
#include "utils/Log.h"

namespace tgfx {
Surface::Surface(std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<Texture> texture,
                 const SurfaceOptions* options)
    : renderTarget(std::move(renderTarget)), texture(std::move(texture)) {
  DEBUG_ASSERT(this->renderTarget != nullptr);
  if (options != nullptr) {
    surfaceOptions = *options;
  }
}

Surface::~Surface() {
  delete canvas;
}

std::shared_ptr<RenderTarget> Surface::getRenderTarget() {
  flush();
  return renderTarget;
}

std::shared_ptr<Texture> Surface::getTexture() {
  flush();
  return texture;
}

bool Surface::wait(const Semaphore* waitSemaphore) {
  return renderTarget->getContext()->wait(waitSemaphore);
}

Canvas* Surface::getCanvas() {
  if (canvas == nullptr) {
    canvas = new Canvas(this);
  }
  return canvas;
}

bool Surface::flush(Semaphore* signalSemaphore) {
  renderTarget->getContext()->drawingManager()->newTextureResolveRenderTask(this);
  return renderTarget->getContext()->drawingManager()->flush(signalSemaphore);
}

bool Surface::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) {
  flush();
  return onReadPixels(dstInfo, dstPixels, srcX, srcY);
}

bool Surface::hitTest(float x, float y) {
  uint8_t pixel[4];
  auto info = ImageInfo::Make(1, 1, ColorType::RGBA_8888, AlphaType::Premultiplied);
  auto result = readPixels(info, pixel, static_cast<int>(x), static_cast<int>(y));
  return result && pixel[3] > 0;
}
}  // namespace tgfx
