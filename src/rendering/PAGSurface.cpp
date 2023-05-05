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

#include "base/utils/TGFXCast.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/drawables/Drawable.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/utils/GLRestorer.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/shaper/TextShaper.h"
#include "tgfx/utils/Clock.h"

namespace pag {

PAGSurface::PAGSurface(std::shared_ptr<Drawable> drawable, bool contextAdopted)
    : drawable(std::move(drawable)), contextAdopted(contextAdopted) {
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
  onFreeCache();
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
  auto context = drawable->lockContext();
  if (context) {
    context->purgeResourcesNotUsedSince(0);
    drawable->unlockContext();
  }
  drawable->freeDevice();
}

bool PAGSurface::clearAll() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext(true);
  if (!context) {
    return false;
  }
  auto surface = drawable->getSurface(context, true);
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

HardwareBufferRef PAGSurface::getHardwareBuffer() {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface = drawable->getSurface(context);
  if (surface == nullptr) {
    unlockContext();
    return nullptr;
  }
  auto hardwareBuffer = surface->getHardwareBuffer();
  unlockContext();
  return ToPAG(hardwareBuffer);
}

bool PAGSurface::readPixels(ColorType colorType, AlphaType alphaType, void* dstPixels,
                            size_t dstRowBytes) {
  LockGuard autoLock(rootLocker);
  auto context = lockContext();
  if (context == nullptr) {
    return false;
  }
  auto surface = drawable->getSurface(context);
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
  auto context = lockContext(true);
  if (!context) {
    return false;
  }
  cache->prepareLayers();
  auto surface = drawable->getSurface(context);
  if (surface != nullptr && autoClear && contentVersion == cache->getContentVersion()) {
    unlockContext();
    return false;
  }
  if (surface == nullptr) {
    surface = drawable->getSurface(context, true);
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
  if (signalSemaphore == nullptr) {
    surface->flush();
  } else {
    tgfx::BackendSemaphore semaphore = {};
    surface->flush(&semaphore);
    signalSemaphore->initGL(semaphore.glSync());
  }
  cache->detachFromContext();
  context->submit();
  drawable->setTimeStamp(pagPlayer->getTimeStampInternal());
  drawable->present(context);
  unlockContext();
  return true;
}

bool PAGSurface::wait(const BackendSemaphore& waitSemaphore) {
  if (!waitSemaphore.isInitialized()) {
    return false;
  }
  auto context = lockContext(true);
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

tgfx::Context* PAGSurface::lockContext(bool force) {
  auto context = drawable->lockContext(force);
  if (context != nullptr && contextAdopted) {
#ifndef PAG_BUILD_FOR_WEB
    glRestorer = new GLRestorer(tgfx::GLFunctions::Get(context));
#endif
    context->resetState();
  }
  return context;
}

void PAGSurface::unlockContext() {
  if (contextAdopted) {
    delete glRestorer;
    glRestorer = nullptr;
  }
  drawable->unlockContext();
}

void PAGSurface::onDraw(std::shared_ptr<Graphic> graphic, std::shared_ptr<tgfx::Surface> target,
                        RenderCache* cache) {
  auto canvas = target->getCanvas();
  if (graphic) {
    graphic->prepare(cache);
    graphic->draw(canvas, cache);
  }
}

}  // namespace pag
