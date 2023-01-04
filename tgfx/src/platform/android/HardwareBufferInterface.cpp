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

#include "HardwareBufferInterface.h"
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <cstdlib>

namespace tgfx {
template <typename T>
static void LoadSymbol(T*& function, const char* symbol) {
  function = (T*)dlsym(RTLD_DEFAULT, symbol);
}

class AHardwareBufferFunctions {
 public:
  int (*allocate)(const AHardwareBuffer_Desc*, AHardwareBuffer**) = nullptr;
  void (*release)(AHardwareBuffer*) = nullptr;
  int (*lock)(AHardwareBuffer* buffer, uint64_t usage, int32_t fence, const ARect* rect,
              void** outVirtualAddress) = nullptr;
  int (*unlock)(AHardwareBuffer* buffer, int32_t* fence) = nullptr;
  void (*describe)(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc) = nullptr;
  void (*acquire)(AHardwareBuffer* buffer) = nullptr;
  jobject (*AHB_to_HB)(JNIEnv*, AHardwareBuffer*) = nullptr;
  AHardwareBuffer* (*AHB_from_HB)(JNIEnv*, jobject) = nullptr;

  AHardwareBufferFunctions() {
    char sdk[PROP_VALUE_MAX] = "0";
    __system_property_get("ro.build.version.sdk", sdk);
    auto version = atoi(sdk);
    if (version < 26) {
      return;
    }
    LoadSymbol(allocate, "AHardwareBuffer_allocate");
    LoadSymbol(release, "AHardwareBuffer_release");
    LoadSymbol(lock, "AHardwareBuffer_lock");
    LoadSymbol(unlock, "AHardwareBuffer_unlock");
    LoadSymbol(describe, "AHardwareBuffer_describe");
    LoadSymbol(acquire, "AHardwareBuffer_acquire");
    LoadSymbol(AHB_to_HB, "AHardwareBuffer_toHardwareBuffer");
    LoadSymbol(AHB_from_HB, "AHardwareBuffer_fromHardwareBuffer");
  }
};

static const AHardwareBufferFunctions* GetFunctions() {
  static AHardwareBufferFunctions functions = {};
  return &functions;
}

bool HardwareBufferInterface::Available() {
  return GetFunctions()->allocate != nullptr && GetFunctions()->release != nullptr &&
         GetFunctions()->lock != nullptr && GetFunctions()->unlock != nullptr &&
         GetFunctions()->describe != nullptr && GetFunctions()->acquire != nullptr &&
         GetFunctions()->AHB_to_HB != nullptr && GetFunctions()->AHB_from_HB != nullptr;
}

int HardwareBufferInterface::Allocate(const AHardwareBuffer_Desc* desc,
                                      AHardwareBuffer** outBuffer) {
  return GetFunctions()->allocate(desc, outBuffer);
}

void HardwareBufferInterface::Acquire(AHardwareBuffer* buffer) {
  GetFunctions()->acquire(buffer);
}

void HardwareBufferInterface::Release(AHardwareBuffer* buffer) {
  GetFunctions()->release(buffer);
}

void HardwareBufferInterface::Describe(const AHardwareBuffer* buffer,
                                       AHardwareBuffer_Desc* outDesc) {
  GetFunctions()->describe(buffer, outDesc);
}

int HardwareBufferInterface::Lock(AHardwareBuffer* buffer, uint64_t usage, int32_t fence,
                                  const ARect* rect, void** outVirtualAddress) {
  return GetFunctions()->lock(buffer, usage, fence, rect, outVirtualAddress);
}

int HardwareBufferInterface::Unlock(AHardwareBuffer* buffer, int32_t* fence) {
  return GetFunctions()->unlock(buffer, fence);
}

jobject HardwareBufferInterface::AHardwareBuffer_toHardwareBuffer(JNIEnv* env,
                                                                  AHardwareBuffer* buffer) {
  return GetFunctions()->AHB_to_HB(env, buffer);
}

AHardwareBuffer* HardwareBufferInterface::AHardwareBuffer_fromHardwareBuffer(
    JNIEnv* env, jobject hardwareBufferObj) {
  return GetFunctions()->AHB_from_HB(env, hardwareBufferObj);
}

}  // namespace tgfx