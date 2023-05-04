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

#include <thread>
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "rendering/Drawable.h"
#include "tgfx/opengl/GLDevice.h"

namespace pag {

static std::mutex threadCacheLocker = {};
static std::unordered_map<std::thread::id, std::weak_ptr<tgfx::GLDevice>> threadCacheMap = {};

static std::shared_ptr<tgfx::GLDevice> MakeDeviceFromThreadPool() {
  std::lock_guard<std::mutex> autoLock(threadCacheLocker);
  auto threadID = std::this_thread::get_id();
  auto result = threadCacheMap.find(threadID);
  if (result != threadCacheMap.end()) {
    auto& weak = result->second;
    auto context = weak.lock();
    if (context) {
      return context;
    }
    threadCacheMap.erase(result);
  }
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
    return nullptr;
  }
  threadCacheMap[threadID] = device;
  return device;
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(std::shared_ptr<Drawable> drawable) {
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable)));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendRenderTarget& renderTarget,
                                                 ImageOrigin origin) {
  auto device = tgfx::GLDevice::Current();
  if (device == nullptr || !renderTarget.isValid()) {
    return nullptr;
  }
  auto drawable =
      std::make_shared<RenderTargetDrawable>(device, ToTGFX(renderTarget), ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), true));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeFrom(const BackendTexture& texture, ImageOrigin origin,
                                                 bool forAsyncThread) {
  std::shared_ptr<tgfx::Device> device = nullptr;
  bool isAdopted = false;
  if (forAsyncThread) {
    auto sharedContext = tgfx::GLDevice::CurrentNativeHandle();
    device = tgfx::GLDevice::Make(sharedContext);
  }
  if (device == nullptr) {
    device = tgfx::GLDevice::Current();
    isAdopted = true;
  }
  if (device == nullptr || !texture.isValid()) {
    return nullptr;
  }
  auto drawable = std::make_shared<TextureDrawable>(device, ToTGFX(texture), ToTGFX(origin));
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGSurface>(new PAGSurface(std::move(drawable), isAdopted));
}

std::shared_ptr<PAGSurface> PAGSurface::MakeOffscreen(int width, int height) {
  auto device = MakeDeviceFromThreadPool();
  if (device == nullptr) {
    device = tgfx::GLDevice::Current();
  }
  if (device == nullptr || width <= 0 || height <= 0) {
    return nullptr;
  }
  auto drawable = std::make_shared<OffscreenDrawable>(width, height, device);
  return std::shared_ptr<PAGSurface>(new PAGSurface(drawable));
}

}  // namespace pag
