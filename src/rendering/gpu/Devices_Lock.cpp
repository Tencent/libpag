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
// Per-thread record of the context locked by the outermost DeviceLockScope. Inner scopes created
// on the same thread reuse the recorded context instead of re-entering tgfx's non-recursive
// device mutex (see the class comment in Devices.h). Only DeviceLockScope maintains this record;
// raw tgfx::Device::lockContext() callers are outside its protection.
thread_local tgfx::Context* tlsLockedContext = nullptr;
thread_local int tlsLockDepth = 0;
}  // namespace

DeviceLockScope::DeviceLockScope() {
  if (tlsLockDepth > 0) {
    // The calling thread already holds the default device through an outer scope: reuse its
    // context. The outer scope keeps the device alive, so the raw pointers stay valid.
    tlsLockDepth++;
    _context = tlsLockedContext;
    return;
  }
  _device = Devices::MakeDefault();
  if (!_device) {
    return;
  }
  auto* context = _device->lockContext();
  if (!context) {
    _device = nullptr;
    return;
  }
  _ownsLock = true;
  _context = context;
  tlsLockedContext = context;
  tlsLockDepth = 1;
}

DeviceLockScope::~DeviceLockScope() {
  if (tlsLockDepth == 0) {
    // A failed scope (no context acquired) or a scope destroyed after an unbalanced unlock.
    return;
  }
  tlsLockDepth--;
  if (tlsLockDepth > 0) {
    // An inner scope: the outermost owner keeps the device locked.
    return;
  }
  if (_ownsLock && _device) {
    _device->unlock();
  }
  tlsLockedContext = nullptr;
  _context = nullptr;
}

}  // namespace pag
