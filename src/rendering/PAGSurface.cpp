/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "base/utils/TGFXCast.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/drawables/Drawable.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/utils/GLRestorer.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/shaper/TextShaper.h"
#include "tgfx/core/Clock.h"

namespace pag {

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable, bool externalContext)
    : drawable(std::move(drawable)), externalContext(externalContext) {
  rootLocker = std::make_shared<std::mutex>();
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  if (externalContext) {
    glRestorer = new GLRestorer();
  }
#endif
}

PAGSurface::~PAGSurface() {
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  delete static_cast<GLRestorer*>(glRestorer);
#endif
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
  TextShaper::PurgeCaches();
  if (pagPlayer) {
    pagPlayer->renderCache->releaseAll();
  }
  drawable->freeSurface();
  drawable->updateSize();
}

void PAGSurface::freeCache() {
  LockGuard autoLock(rootLocker);
  onFreeCache();
}

void PAGSurface::onFreeCache() {
  TextShaper::PurgeCaches();
  if (pagPlayer) {
    pagPlayer->renderCache->releaseAll();
  }
  drawable->freeSurface();
  auto context = lockContext();
  if (context) {
    context->purgeResourcesUntilMemoryTo(0);
    unlockContext();
  }
}

bool PAGSurface::clearAll() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (!context) {
    return false;
  }
  auto surface = drawable->getSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return false;
  }
  contentVersion = 0;  // 清空画布后 contentVersion 还原为初始值 0.
  auto canvas = surface->getCanvas();
  canvas->clear();
  context->flush();
  drawable->setTimeStamp(0);
  drawable->present(context);
  unlockContext();
  return true;
}

HardwareBufferRef PAGSurface::getHardwareBuffer() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface = drawable->getSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return nullptr;
  }
  auto hardwareBuffer = surface->getHardwareBuffer();
  unlockContext();
  return hardwareBuffer;
}

BackendTexture PAGSurface::getFrontTexture() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return {};
  }
  auto surface = drawable->getFrontSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return {};
  }
  auto texture = surface->getBackendTexture();
  unlockContext();
  return ToPAG(texture);
}

BackendTexture PAGSurface::getBackTexture() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return {};
  }
  auto surface = drawable->getSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return {};
  }
  auto texture = surface->getBackendTexture();
  unlockContext();
  return ToPAG(texture);
}

HardwareBufferRef PAGSurface::getFrontHardwareBuffer() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface = drawable->getFrontSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return nullptr;
  }
  auto buffer = surface->getHardwareBuffer();
  unlockContext();
  return buffer;
}

HardwareBufferRef PAGSurface::getBackHardwareBuffer() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface = drawable->getSurface(context, false);
  if (surface == nullptr) {
    unlockContext();
    return nullptr;
  }
  auto buffer = surface->getHardwareBuffer();
  unlockContext();
  return buffer;
}

bool PAGSurface::readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels,
                            size_t dstRowBytes) {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return false;
  }
  auto surface = drawable->getSurface(context, true);
  if (surface == nullptr) {
    unlockContext();
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
  auto context = lockContext();
  if (!context) {
    return false;
  }
  cache->prepareLayers();
  auto surface = drawable->getSurface(context, true);
  if (surface != nullptr && autoClear && contentVersion == cache->getContentVersion()) {
    unlockContext();
    return false;
  }
  if (surface == nullptr) {
    surface = drawable->getSurface(context, false);
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
  onDraw(graphic, surface, cache);
  std::unique_ptr<tgfx::Recording> recording = nullptr;
  if (signalSemaphore == nullptr) {
    recording = context->flush();
  } else {
    tgfx::BackendSemaphore semaphore = {};
    recording = context->flush(&semaphore);
    tgfx::GLSyncInfo signalInfo = {};
    if (semaphore.getGLSync(&signalInfo)) {
      signalSemaphore->initGL(signalInfo.sync);
    }
  }
  cache->detachFromContext();
  context->submit(std::move(recording));
  cache->prepareNextFrame();
  drawable->setTimeStamp(pagPlayer->getTimeStampInternal());
  drawable->present(context);
  unlockContext();
  return true;
}

bool PAGSurface::wait(const BackendSemaphore& waitSemaphore) {
  if (!waitSemaphore.isInitialized()) {
    return false;
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  auto semaphore = ToTGFX(waitSemaphore);
  auto success = context->wait(semaphore);
  unlockContext();
  return success;
}

bool PAGSurface::prepare(RenderCache* cache, std::shared_ptr<Graphic> graphic) {
  auto context = lockContext();
  if (!context) {
    return false;
  }
  cache->attachToContext(context, false);
  cache->prepareLayers();
  if (graphic != nullptr) {
    graphic->prepare(cache);
  }
  cache->detachFromContext();
  unlockContext();
  return true;
}

bool PAGSurface::hitTest(RenderCache* cache, std::shared_ptr<Graphic> graphic, float x, float y) {
  if (cache == nullptr || graphic == nullptr) {
    return false;
  }
  auto context = lockContext();
  if (!context) {
    return false;
  }
  cache->attachToContext(context, false);
  auto result = graphic->hitTest(cache, x, y);
  cache->detachFromContext();
  unlockContext();
  return result;
}

tgfx::Context* PAGSurface::lockContext() {
  auto device = drawable->getDevice();
  if (device == nullptr) {
    return nullptr;
  }
  auto context = device->lockContext();
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  if (context != nullptr && glRestorer != nullptr) {
    static_cast<GLRestorer*>(glRestorer)->save();
  }
#endif
  return context;
}

void PAGSurface::unlockContext() {
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  if (glRestorer != nullptr) {
    static_cast<GLRestorer*>(glRestorer)->restore();
  }
#endif
  auto device = drawable->getDevice();
  if (device != nullptr) {
    device->unlock();
  }
}

void PAGSurface::onDraw(std::shared_ptr<Graphic> graphic, std::shared_ptr<tgfx::Surface> target,
                        RenderCache* cache) {
  Canvas canvas(target.get(), cache);
  if (graphic) {
    graphic->prepare(cache);
    graphic->draw(&canvas);
  }
}

}  // namespace pag
