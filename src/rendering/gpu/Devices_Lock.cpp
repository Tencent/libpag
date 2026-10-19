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

#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"

namespace pag {

namespace {
// Innermost DeviceLockScope that actually acquired a device lock on the calling thread, linked to
// its outer scopes through DeviceLockScope::_outerScope. Scopes consult this chain to find out
// whether the device they want is already locked by an enclosing scope, and reuse its context
// instead of re-entering tgfx's non-recursive device mutex (see the class comment in Devices.h).
// Only DeviceLockScope maintains this chain; raw tgfx::Device::lockContext() callers are outside
// its protection.
thread_local DeviceLockScope* tlsInnermostScope = nullptr;
}  // namespace

DeviceLockScope::DeviceLockScope() {
  if (tlsInnermostScope != nullptr) {
    // The caller did not name a device, so any context already locked on this thread satisfies it.
    // Reusing the innermost one also avoids a second device: on the OpenGL backend every
    // Devices::MakeDefault() builds a brand-new GL context (see GPUContext::lockContext).
    _context = tlsInnermostScope->_context;
    return;
  }
  lockDevice(Devices::MakeDefault());
}

DeviceLockScope::DeviceLockScope(std::shared_ptr<tgfx::Device> device) {
  lockDevice(std::move(device));
}

void DeviceLockScope::lockDevice(std::shared_ptr<tgfx::Device> device) {
  if (!device) {
    return;
  }
  for (auto* scope = tlsInnermostScope; scope != nullptr; scope = scope->_outerScope) {
    if (scope->_device == device) {
      _context = scope->_context;
      return;
    }
  }
  auto* context = device->lockContext();
  if (!context) {
    return;
  }
  _device = std::move(device);
  _ownsLock = true;
  _context = context;
  _outerScope = tlsInnermostScope;
  tlsInnermostScope = this;
}

DeviceLockScope::~DeviceLockScope() {
  if (!_ownsLock) {
    // A failed scope, or one that reused the context of an enclosing scope.
    return;
  }
  if (tlsInnermostScope == this) {
    // The usual case: scopes are destroyed in reverse construction order.
    tlsInnermostScope = _outerScope;
  } else {
    // Out-of-order destruction of a heap-allocated scope. Splice it out of the thread's chain so
    // the scopes created after it still consult the right enclosing device.
    for (auto* scope = tlsInnermostScope; scope != nullptr; scope = scope->_outerScope) {
      if (scope->_outerScope == this) {
        scope->_outerScope = _outerScope;
        break;
      }
    }
  }
  _device->unlock();
  _context = nullptr;
}

}  // namespace pag
