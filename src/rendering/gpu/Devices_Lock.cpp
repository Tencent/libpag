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

namespace pag {

DeviceLockScope::DeviceLockScope() : DeviceLockScope(Devices::MakeDefault()) {
}

DeviceLockScope::DeviceLockScope(std::shared_ptr<tgfx::Device> device)
    : _device(std::move(device)) {
  if (_device != nullptr) {
    _context = _device->lockContext();
  }
}

DeviceLockScope::~DeviceLockScope() {
  if (_context != nullptr) {
    _device->unlock();
  }
}

}  // namespace pag
