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
#include "core/PixelBuffer.h"
#include "gpu/RenderTarget.h"
#include "utils/Log.h"

namespace tgfx {
std::shared_ptr<Surface> Surface::Make(Context* context, int width, int height, bool alphaOnly,
                                       int sampleCount, bool mipMapped,
                                       const SurfaceOptions* options) {
  auto caps = context->caps();
  auto pixelFormat = alphaOnly ? PixelFormat::ALPHA_8 : PixelFormat::RGBA_8888;
  if (!caps->isFormatRenderable(pixelFormat)) {
    return nullptr;
  }
  std::shared_ptr<Texture> texture = nullptr;
  if (texture == nullptr) {
    if (alphaOnly) {
      texture = Texture::MakeAlpha(context, width, height, SurfaceOrigin::TopLeft, mipMapped);
    } else {
      texture = Texture::MakeRGBA(context, width, height, SurfaceOrigin::TopLeft, mipMapped);
    }
  }
  if (texture == nullptr) {
    return nullptr;
  }
  sampleCount = caps->getSampleCount(sampleCount, pixelFormat);
  auto renderTarget = RenderTarget::MakeFrom(texture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  auto surface = new Surface(renderTarget, texture, options, false);
  // 对于内部创建的 RenderTarget 默认清屏。
  surface->getCanvas()->clear();
  return std::shared_ptr<Surface>(surface);
}

std::shared_ptr<Surface> Surface::MakeFrom(Context* context,
                                           const BackendRenderTarget& renderTarget,
                                           SurfaceOrigin origin, const SurfaceOptions* options) {
  auto rt = RenderTarget::MakeFrom(context, renderTarget, origin);
  return MakeFrom(std::move(rt), options);
}

std::shared_ptr<Surface> Surface::MakeFrom(Context* context, const BackendTexture& backendTexture,
                                           SurfaceOrigin origin, int sampleCount,
                                           const SurfaceOptions* options) {
  auto texture = Texture::MakeFrom(context, backendTexture, origin);
  return MakeFrom(std::move(texture), sampleCount, options);
}

std::shared_ptr<Surface> Surface::MakeFrom(Context* context, HardwareBufferRef hardwareBuffer,
                                           int sampleCount, const SurfaceOptions* options) {
  auto texture = Texture::MakeFrom(context, hardwareBuffer);
  return MakeFrom(std::move(texture), sampleCount, options);
}

std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<RenderTarget> renderTarget,
                                           const SurfaceOptions* options) {
  if (renderTarget == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<Surface>(new Surface(std::move(renderTarget), nullptr, options));
}

std::shared_ptr<Surface> Surface::MakeFrom(std::shared_ptr<Texture> texture, int sampleCount,
                                           const SurfaceOptions* options) {
  if (texture == nullptr || texture->isYUV()) {
    return nullptr;
  }
  auto renderTarget = RenderTarget::MakeFrom(texture.get(), sampleCount);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<Surface>(new Surface(renderTarget, std::move(texture), options));
}

Surface::Surface(std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<Texture> texture,
                 const SurfaceOptions* options, bool externalTexture)
    : renderTarget(std::move(renderTarget)),
      texture(std::move(texture)),
      externalTexture(externalTexture) {
  DEBUG_ASSERT(this->renderTarget != nullptr);
  if (options != nullptr) {
    surfaceOptions = *options;
  }
}

Surface::~Surface() {
  delete canvas;
}

Context* Surface::getContext() const {
  return renderTarget->getContext();
}

int Surface::width() const {
  return renderTarget->width();
}

int Surface::height() const {
  return renderTarget->height();
}

SurfaceOrigin Surface::origin() const {
  return renderTarget->origin();
}

std::shared_ptr<Texture> Surface::getTexture() {
  flush();
  return texture;
}

BackendRenderTarget Surface::getBackendRenderTarget() {
  flush();
  return renderTarget->getBackendRenderTarget();
}

BackendTexture Surface::getBackendTexture() {
  flush();
  return texture->getBackendTexture();
}

bool Surface::wait(const BackendSemaphore& waitSemaphore) {
  auto semaphore = Semaphore::Wrap(&waitSemaphore);
  return renderTarget->getContext()->wait(semaphore.get());
}

Canvas* Surface::getCanvas() {
  if (canvas == nullptr) {
    canvas = new Canvas(this);
  }
  return canvas;
}

bool Surface::flush(BackendSemaphore* signalSemaphore) {
  auto semaphore = Semaphore::Wrap(signalSemaphore);
  renderTarget->getContext()->drawingManager()->newTextureResolveRenderTask(this);
  auto result = renderTarget->getContext()->drawingManager()->flush(semaphore.get());
  if (signalSemaphore != nullptr) {
    *signalSemaphore = semaphore->getBackendSemaphore();
  }
  return result;
}

void Surface::flushAndSubmit(bool syncCpu) {
  flush();
  renderTarget->getContext()->submit(syncCpu);
}

static std::shared_ptr<Texture> MakeTextureFromRenderTarget(const RenderTarget* renderTarget,
                                                            bool discardContent = false) {
  auto context = renderTarget->getContext();
  auto width = renderTarget->width();
  auto height = renderTarget->height();
  if (discardContent) {
    return Texture::MakeFormat(context, width, height, renderTarget->format(),
                               renderTarget->origin());
  }
  auto texture =
      Texture::MakeFormat(context, width, height, renderTarget->format(), renderTarget->origin());
  if (texture == nullptr) {
    return nullptr;
  }
  context->gpu()->copyRenderTargetToTexture(renderTarget, texture.get(),
                                            Rect::MakeWH(width, height), Point::Zero());
  return texture;
}

std::shared_ptr<Image> Surface::makeImageSnapshot() {
  flush();
  if (cachedImage != nullptr) {
    return cachedImage;
  }
  if (texture != nullptr && !externalTexture) {
    cachedImage = Image::MakeFrom(texture);
  } else {
    auto textureCopy = MakeTextureFromRenderTarget(renderTarget.get());
    cachedImage = Image::MakeFrom(textureCopy);
  }
  return cachedImage;
}

void Surface::aboutToDraw(bool discardContent) {
  if (cachedImage == nullptr) {
    return;
  }
  cachedImage = nullptr;
  if (texture == nullptr || externalTexture) {
    return;
  }
  auto newTexture = MakeTextureFromRenderTarget(renderTarget.get(), discardContent);
  auto success = renderTarget->replaceTexture(newTexture.get());
  if (!success) {
    LOGE("Surface::aboutToDraw(): Failed to replace the backing texture of the renderTarget!");
  }
  texture = newTexture;
}

Color Surface::getColor(int x, int y) {
  uint8_t color[4];
  auto info = ImageInfo::Make(1, 1, ColorType::RGBA_8888, AlphaType::Premultiplied);
  if (!readPixels(info, color, x, y)) {
    return Color::Transparent();
  }
  return Color::FromRGBA(color[0], color[1], color[2], color[3]);
}

bool Surface::readPixels(const ImageInfo& dstInfo, void* dstPixels, int srcX, int srcY) {
  flushAndSubmit();
  return renderTarget->readPixels(dstInfo, dstPixels, srcX, srcY);
}
}  // namespace tgfx
