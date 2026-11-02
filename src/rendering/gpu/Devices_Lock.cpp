/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/utils/Log.h"
#include "rendering/gpu/Devices.h"
#if DEBUG && !defined(NO_LOG)
#include <unordered_set>
#endif

namespace pag {
#if DEBUG && !defined(NO_LOG)
static std::unordered_set<const tgfx::Device*>& HeldDeviceScopes() {
  thread_local std::unordered_set<const tgfx::Device*> devices;
  return devices;
}
#endif

DeviceLockScope::DeviceLockScope() : DeviceLockScope(Devices::MakeDefault()) {
}

DeviceLockScope::DeviceLockScope(std::shared_ptr<tgfx::Device> device)
    : _device(std::move(device)) {
  if (_device != nullptr) {
#if DEBUG && !defined(NO_LOG)
    DEBUG_ASSERT(HeldDeviceScopes().count(_device.get()) == 0);
#endif
    _context = _device->lockContext();
#if DEBUG && !defined(NO_LOG)
    if (_context != nullptr) {
      HeldDeviceScopes().insert(_device.get());
    }
#endif
  }
}

DeviceLockScope::~DeviceLockScope() {
  if (_context != nullptr) {
#if DEBUG && !defined(NO_LOG)
    DEBUG_ASSERT(HeldDeviceScopes().count(_device.get()) != 0);
#endif
    _device->unlock();
#if DEBUG && !defined(NO_LOG)
    HeldDeviceScopes().erase(_device.get());
#endif
  }
}

}  // namespace pag
