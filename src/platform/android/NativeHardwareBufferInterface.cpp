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

#include "NativeHardwareBufferInterface.h"

#include <dlfcn.h>
#include <sys/system_properties.h>

namespace pag {

template <typename T>
static void loadSymbol(T*& pfn, const char* symbol) {
  pfn = (T*)dlsym(RTLD_DEFAULT, symbol);
}

NativeHardwareBufferInterface::NativeHardwareBufferInterface() {
  // if we compile for API 26 (Oreo) and above, we're guaranteed to have AHardwareBuffer
  // in all other cases, we need to get them at runtime.
#ifndef PLATFORM_HAS_HARDWAREBUFFER
  loadSymbol(AHardwareBuffer_allocate, "AHardwareBuffer_allocate");
  loadSymbol(AHardwareBuffer_release, "AHardwareBuffer_release");
  loadSymbol(AHardwareBuffer_lock, "AHardwareBuffer_lock");
  loadSymbol(AHardwareBuffer_unlock, "AHardwareBuffer_unlock");
  loadSymbol(AHardwareBuffer_describe, "AHardwareBuffer_describe");
  loadSymbol(AHardwareBuffer_acquire, "AHardwareBuffer_acquire");
#endif
}

NativeHardwareBufferInterface* NativeHardwareBufferInterface::Get() {
  static NativeHardwareBufferInterface instance;
  return &instance;
}

}  // namespace pag