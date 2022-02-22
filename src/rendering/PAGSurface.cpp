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
//  and limitations under the license.˙
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/GetTimer.h"
#include "base/utils/TGFXCast.h"
#include "gpu/Canvas.h"
#include "gpu/opengl/GLDevice.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Drawable.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/utils/LockGuard.h"

namespace pag {

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(std::shared_ptr<Drawable> drawable) {
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable)));
}

static std::shared_ptr<tgfx::Device> GetCurrentDevice(bool forAsyncThread) {
  if (forAsyncThread) {
    auto sharedContext = tgfx::GLDevice::CurrentNativeHandle();
    auto device = tgfx::GLDevice::Make(sharedContext);
    if (device) {
      return device;
    }
  }
  return tgfx::GLDevice::Current();
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendRenderTarget& renderTarget,
                                                 ImageOrigin origin) {
  auto device = tgfx::GLDevice::Current();
  if (device == nullptr || !renderTarget.isValid()) {
    return nullptr;
  }
  auto drawable = std::make_shared<RenderTargetDrawable>(device, renderTarget, ToTGFX(origin));
  return MakeFrom(std::move(drawable));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendTexture& texture, ImageOrigin origin,
                                                 bool forAsyncThread) {
  auto device = GetCurrentDevice(forAsyncThread);
  if (device == nullptr || !texture.isValid()) {
    return nullptr;
  }
  auto drawable = std::make_shared<TextureDrawable>(device, texture, ToTGFX(origin));
  return MakeFrom(std::move(drawable));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeOffscreen(int width, int height) {
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto drawable = std::make_shared<OffscreenDrawable>(width, height, device);
  return std::shared_ptr<PAGSurface>(new PAGSurface(drawable));
}

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable) : drawable(std::move(drawable)) {
  rootLocker = std::make_shared<std::mutex>();
}

int PAGSurface::width() {
  LockGuard autoLock(rootLocker);
  return drawable->width();
}

int PAGSurface::height() {
  LockGuard autoLock(rootLocker);
  return drawable->height();
}

void PAGSurface::updateSize() {
  LockGuard autoLock(rootLocker);
  surface = nullptr;
  device = nullptr;
  drawable->updateSize();
}

void PAGSurface::freeCache() {
  LockGuard autoLock(rootLocker);
  if (pagPlayer) {
    pagPlayer->renderCache->releaseAll();
  }
  surface = nullptr;
  if (device) {
    auto context = device->lockContext();
    if (context) {
      context->purgeResourcesNotUsedIn(0);
      device->unlock();
    }
  }
  device = nullptr;
}

bool PAGSurface::clearAll() {
  LockGuard autoLock(rootLocker);
  if (device == nullptr) {
    device = drawable->getDevice();
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  if (surface == nullptr) {
    surface = drawable->createSurface(context);
  }
  if (surface == nullptr) {
    unlockContext();
    return false;
  }
  contentVersion = 0;  // 清空画布后 contentVersion 还原为初始值 0.
  auto canvas = surface->getCanvas();
  canvas->clear();
  canvas->flush();
  drawable->setTimeStamp(0);
  drawable->present(context);
  unlockContext();
  return true;
}

bool PAGSurface::readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels,
                            size_t dstRowBytes) {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (surface == nullptr || !context) {
    return false;
  }
  auto info = tgfx::ImageInfo::Make(surface->width(), surface->height(), ToTGFX(colorType),
                                    ToTGFX(alphaType), dstRowBytes);
  auto result = surface->readPixels(info, dstPixels);
  unlockContext();
  return result;
}

bool PAGSurface::draw(RenderCache* cache, std::shared_ptr<Graphic> graphic,
                      BackendSemaphore* signalSemaphore, bool autoClear) {
  if (device == nullptr) {
    device = drawable->getDevice();
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  if (surface != nullptr && autoClear && contentVersion == cache->getContentVersion()) {
    unlockContext();
    return false;
  }
  if (surface == nullptr) {
    surface = drawable->createSurface(context);
  }
  if (surface == nullptr) {
    unlockContext();
    return false;
  }
  contentVersion = cache->getContentVersion();
  cache->attachToContext(context);
  auto canvas = surface->getCanvas();
  if (autoClear) {
    canvas->clear();
  }
  if (graphic) {
    graphic->draw(canvas, cache);
  }
  if (signalSemaphore == nullptr) {
    surface->flush();
  } else {
    tgfx::GLSemaphore semaphore = {};
    surface->flush(&semaphore);
    signalSemaphore->initGL(semaphore.glSync);
  }
  cache->detachFromContext();
  drawable->setTimeStamp(pagPlayer->getTimeStampInternal());
  drawable->present(context);
  unlockContext();
  return true;
}

bool PAGSurface::wait(const BackendSemaphore& waitSemaphore) {
  if (!waitSemaphore.isInitialized()) {
    return false;
  }
  if (device == nullptr) {
    device = drawable->getDevice();
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  if (surface == nullptr) {
    surface = drawable->createSurface(context);
  }
  if (surface == nullptr) {
    unlockContext();
    return false;
  }
  auto semaphore = ToTGFX(waitSemaphore);
  auto ret = surface->wait(&semaphore);
  unlockContext();
  return ret;
}

bool PAGSurface::hitTest(RenderCache* cache, std::shared_ptr<Graphic> graphic, float x, float y) {
  if (cache == nullptr || graphic == nullptr) {
    return false;
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  cache->attachToContext(context, true);
  auto result = graphic->hitTest(cache, x, y);
  cache->detachFromContext();
  unlockContext();
  return result;
}

tgfx::Context* PAGSurface::lockContext() {
  if (device == nullptr) {
    return nullptr;
  }
  return device->lockContext();
}

void PAGSurface::unlockContext() {
  if (device == nullptr) {
    return;
  }
  device->unlock();
}
}  // namespace pag
