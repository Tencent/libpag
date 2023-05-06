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

#include "tgfx/gpu/Context.h"
#include "gpu/DrawingManager.h"
#include "gpu/ProgramCache.h"
#include "gpu/ProxyProvider.h"
#include "gpu/ResourceProvider.h"
#include "tgfx/gpu/ResourceCache.h"
#include "tgfx/utils/Clock.h"
#include "utils/Log.h"

namespace tgfx {
Context::Context(Device* device) : _device(device) {
  _programCache = new ProgramCache(this);
  _resourceCache = new ResourceCache(this);
  _drawingManager = new DrawingManager(this);
  _resourceProvider = new ResourceProvider(this);
  _proxyProvider = new ProxyProvider(this);
}

Context::~Context() {
  // The Device owner must call releaseAll() before deleting this Context, otherwise, GPU resources
  // may leak.
  DEBUG_ASSERT(_resourceCache->empty());
  DEBUG_ASSERT(_programCache->empty());
  delete _programCache;
  delete _resourceCache;
  delete _drawingManager;
  delete _gpu;
  delete _resourceProvider;
  delete _proxyProvider;
}

bool Context::flush(BackendSemaphore* signalSemaphore) {
  auto semaphore = Semaphore::Wrap(signalSemaphore);
  auto success = _drawingManager->flush(semaphore.get());
  if (signalSemaphore != nullptr) {
    *signalSemaphore = semaphore->getBackendSemaphore();
  }
  return success;
}

bool Context::submit(bool syncCpu) {
  return _gpu->submitToGpu(syncCpu);
}

void Context::flushAndSubmit(bool syncCpu) {
  flush();
  submit(syncCpu);
}

bool Context::wait(const BackendSemaphore& waitSemaphore) {
  auto semaphore = Semaphore::Wrap(&waitSemaphore);
  if (semaphore == nullptr) {
    return false;
  }
  return caps()->semaphoreSupport && _gpu->waitSemaphore(semaphore.get());
}

void Context::onLocked() {
  _resourceCache->attachToCurrentThread();
}

void Context::onUnlocked() {
  _resourceCache->detachFromCurrentThread();
}

size_t Context::memoryUsage() const {
  return _resourceCache->getResourceBytes();
}

size_t Context::purgeableBytes() const {
  return _resourceCache->getPurgeableBytes();
}

size_t Context::getCacheLimit() const {
  return _resourceCache->getCacheLimit();
}

void Context::setCacheLimit(size_t bytesLimit) {
  _resourceCache->setCacheLimit(bytesLimit);
}

void Context::purgeResourcesNotUsedSince(int64_t purgeTime, bool scratchResourcesOnly) {
  _resourceCache->purgeNotUsedSince(purgeTime, scratchResourcesOnly);
}

bool Context::purgeResourcesUntilMemoryTo(size_t bytesLimit, bool scratchResourcesOnly) {
  return _resourceCache->purgeUntilMemoryTo(bytesLimit, scratchResourcesOnly);
}

void Context::releaseAll(bool releaseGPU) {
  _resourceProvider->releaseAll();
  _programCache->releaseAll(releaseGPU);
  _resourceCache->releaseAll(releaseGPU);
}
}  // namespace tgfx