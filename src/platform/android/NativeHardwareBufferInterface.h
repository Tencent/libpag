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

#pragma once

#include <android/hardware_buffer.h>
#include <jni.h>
#include "gpu/Texture.h"

#if __ANDROID_API__ >= 26
#define PLATFORM_HAS_HARDWAREBUFFER
#endif

namespace pag {
class NativeHardwareBufferInterface {
public:
    static NativeHardwareBufferInterface* Get();
    NativeHardwareBufferInterface(const NativeHardwareBufferInterface&) = delete;
    NativeHardwareBufferInterface& operator=(const NativeHardwareBufferInterface&) = delete;
#ifndef PLATFORM_HAS_HARDWAREBUFFER
    // if we compile for API 26 (Oreo) and above, we're guaranteed to have AHardwareBuffer
    // in all other cases, we need to get them at runtime.
    int (*AHardwareBuffer_allocate)(const AHardwareBuffer_Desc*, AHardwareBuffer**) = nullptr;
    void (*AHardwareBuffer_release)(AHardwareBuffer*) = nullptr;
    int (*AHardwareBuffer_lock)(AHardwareBuffer* buffer, uint64_t usage, int32_t fence,
                                const ARect* rect, void** outVirtualAddress) = nullptr;
    int (*AHardwareBuffer_unlock)(AHardwareBuffer* buffer, int32_t* fence) = nullptr;
    void (*AHardwareBuffer_describe)(const AHardwareBuffer* buffer,
                                     AHardwareBuffer_Desc* outDesc) = nullptr;
    void (*AHardwareBuffer_acquire)(AHardwareBuffer* buffer) = nullptr;
#endif
private:
    NativeHardwareBufferInterface();
};
}  // namespace pag
