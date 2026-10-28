/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "DevicePool.h"
#include "rendering/gpu/Devices.h"
#if defined(TGFX_USE_METAL)
#include "base/utils/Log.h"
#include "core/AtlasManager.h"
#include "tgfx/gpu/Context.h"
#endif

namespace pag {
thread_local std::shared_ptr<tgfx::Device> cachedDevice = nullptr;

std::shared_ptr<tgfx::Device> DevicePool::Make() {
  auto device = cachedDevice;
  if (device == nullptr) {
    device = Devices::MakeDefault();
    cachedDevice = device;
  }
  return device;
}

bool DevicePool::ResetAtlasForComparison() {
#if defined(TGFX_USE_METAL)
  DeviceLockScope scope(Devices::MakeDefault());
  if (!scope) {
    LOGE("ResetAtlasForComparison: failed to create or lock the Metal device.");
    return false;
  }
  auto context = scope.context();
  context->flushAndSubmit(true);
  // Test-only workaround matching tgfx's ContextScope: Linear glyph sampling can change pixels
  // with atlas placement. Clear history for independent baselines, not for reuse behavior tests.
  context->atlasManager()->releaseAll();
#endif
  return true;
}
}  // namespace pag
