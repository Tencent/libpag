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

#include "NativeGraphicBufferInterface.h"
#include <dlfcn.h>
#include <cstdlib>

namespace pag {

const int GRAPHICBUFFER_SIZE = 1024;

#if defined(__aarch64__)
#define CPU_ARM_64
#elif defined(__arm__) || defined(__ARM__) || defined(__ARM_NEON__) || defined(ARM_BUILD)
#define CPU_ARM
#elif defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#define CPU_X86_64
#elif defined(__i386__) || defined(_M_X86) || defined(_M_IX86) || defined(X86_BUILD)
#define CPU_X86
#else
#warning "target CPU does not support ABI"
#endif

template <typename RT, typename T1, typename T2, typename T3, typename T4>
RT* callConstructor4(void (*fptr)(), void* memory, T1 param1, T2 param2, T3 param3, T4 param4) {
#if defined(CPU_ARM)
  // C1 constructors return pointer
  typedef RT* (*ABIFptr)(void*, T1, T2, T3, T4);
  (void)((ABIFptr)fptr)(memory, param1, param2, param3, param4);
  return reinterpret_cast<RT*>(memory);
#elif defined(CPU_ARM_64)
  // C1 constructors return void
  typedef void (*ABIFptr)(void*, T1, T2, T3, T4);
  ((ABIFptr)fptr)(memory, param1, param2, param3, param4);
  return reinterpret_cast<RT*>(memory);
#elif defined(CPU_X86) || defined(CPU_X86_64)
  // ctor returns void
  typedef void (*ABIFptr)(void*, T1, T2, T3, T4);
  ((ABIFptr)fptr)(memory, param1, param2, param3, param4);
  return reinterpret_cast<RT*>(memory);
#else
  return nullptr;
#endif
}

template <typename T>
void callDestructor(void (*fptr)(), T* obj) {
#if defined(CPU_ARM)
  // D1 destructor returns ptr
  typedef void* (*ABIFptr)(T * obj);
  (void)((ABIFptr)fptr)(obj);
#elif defined(CPU_ARM_64)
  // D1 destructor returns void
  typedef void (*ABIFptr)(T * obj);
  ((ABIFptr)fptr)(obj);
#elif defined(CPU_X86) || defined(CPU_X86_64)
  // dtor returns void
  typedef void (*ABIFptr)(T * obj);
  ((ABIFptr)fptr)(obj);
#endif
}

template <typename T>
static void loadSymbol(void* libHandle, T*& pfn, const char* symbol) {
  pfn = (T*)dlsym(libHandle, symbol);
}
NativeGraphicBufferInterface* NativeGraphicBufferInterface::Get() {
  static NativeGraphicBufferInterface instance;
  return &instance;
}

NativeGraphicBufferInterface::NativeGraphicBufferInterface() {
  libHandle = dlopen("libui.so", RTLD_LAZY);
  if (!libHandle) return;
  loadSymbol(libHandle, constructor, "_ZN7android13GraphicBufferC1Ejjij");
  loadSymbol(libHandle, destructor, "_ZN7android13GraphicBufferD1Ev");
  loadSymbol(libHandle, getNativeBuffer, "_ZNK7android13GraphicBuffer15getNativeBufferEv");
  loadSymbol(libHandle, lock, "_ZN7android13GraphicBuffer4lockEjPPv");
  loadSymbol(libHandle, unlock, "_ZN7android13GraphicBuffer6unlockEv");
  loadSymbol(libHandle, initCheck, "_ZNK7android13GraphicBuffer9initCheckEv");
  if (constructor && destructor && getNativeBuffer && lock && unlock && initCheck) {
    return;
  }
  constructor = nullptr;
}

NativeGraphicBufferInterface::~NativeGraphicBufferInterface() {
  if (libHandle) {
    dlclose(libHandle);
  }
}

template <typename T1, typename T2>
T1* pointerToOffset(T2* ptr, size_t bytes) {
  return reinterpret_cast<T1*>((uint8_t*)ptr + bytes);
}

android::GraphicBuffer* NativeGraphicBufferInterface::MakeGraphicBuffer(uint32_t width,
                                                                        uint32_t height,
                                                                        PixelFormat format,
                                                                        uint32_t usage) {
  auto interface = NativeGraphicBufferInterface::Get();
  void* const memory = malloc(GRAPHICBUFFER_SIZE);
  if (memory == nullptr) {
    return nullptr;
  }
  android::GraphicBuffer* graphicBuffer =
      callConstructor4<android::GraphicBuffer, uint32_t, uint32_t, PixelFormat, uint32_t>(
          interface->constructor, memory, width, height, format, usage);
  android::android_native_base_t* const base = GetAndroidNativeBase(graphicBuffer);
  status_t ctorStatus = interface->initCheck(graphicBuffer);
  const uint32_t expectedVersion = sizeof(void*) == 4 ? 96 : 168;
  if (ctorStatus || base->magic != 0x5f626672u || base->version != expectedVersion) {
    callDestructor<android::GraphicBuffer>(interface->destructor, graphicBuffer);
    return nullptr;
  }
  return graphicBuffer;
}

android::android_native_base_t* NativeGraphicBufferInterface::GetAndroidNativeBase(
    const android::GraphicBuffer* buffer) {
  return pointerToOffset<android::android_native_base_t>(buffer, 2 * sizeof(void*));
}

void NativeGraphicBufferInterface::Acquire(android::GraphicBuffer* buffer) {
  auto nativeBuffer = GetAndroidNativeBase(buffer);
  nativeBuffer->incRef(nativeBuffer);
}

void NativeGraphicBufferInterface::Release(android::GraphicBuffer* buffer) {
  auto nativeBuffer = GetAndroidNativeBase(buffer);
  if (nativeBuffer) {
    nativeBuffer->decRef(nativeBuffer);
  }
}

}  // namespace pag