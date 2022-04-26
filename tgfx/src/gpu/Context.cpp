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
#include "core/utils/Log.h"
#include "gpu/GradientCache.h"
#include "gpu/ProgramCache.h"
#include "tgfx/core/Clock.h"
#include "tgfx/gpu/ResourceCache.h"

namespace tgfx {
Context::Context(Device* device) : _device(device) {
  _gradientCache = new GradientCache(this);
  _programCache = new ProgramCache(this);
  _resourceCache = new ResourceCache(this);
}

Context::~Context() {
  // The Device owner must call releaseAll() before deleting this Context, otherwise, GPU resources
  // may leak.
  DEBUG_ASSERT(_resourceCache->empty());
  DEBUG_ASSERT(_programCache->empty());
  DEBUG_ASSERT(_gradientCache->empty())
  delete _gradientCache;
  delete _programCache;
  delete _resourceCache;
}

void Context::onLocked() {
  _resourceCache->attachToCurrentThread();
}

void Context::onUnlocked() {
  _resourceCache->detachFromCurrentThread();
}

void Context::purgeResourcesNotUsedIn(int64_t usNotUsed) {
  _resourceCache->purgeNotUsedIn(usNotUsed);
}

void Context::releaseAll(bool releaseGPU) {
  _gradientCache->releaseAll();
  _programCache->releaseAll(releaseGPU);
  _resourceCache->releaseAll(releaseGPU);
}
}  // namespace tgfx