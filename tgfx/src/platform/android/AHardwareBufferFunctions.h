/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace tgfx {
static constexpr int HARDWAREBUFFER_FORMAT_R8_UNORM = 0x38;

using Allocate = int(const AHardwareBuffer_Desc* desc, AHardwareBuffer** outBuffer);
using Acquire = void(AHardwareBuffer* buffer);
using Release = void(AHardwareBuffer* buffer);
using Describe = void(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc);
using Lock = int(AHardwareBuffer* buffer, uint64_t usage, int32_t fence, const ARect* rect,
                 void** outVirtualAddress);
using Unlock = int(AHardwareBuffer* buffer, int32_t* fence);
using FromHardwareBuffer = AHardwareBuffer*(JNIEnv* env, jobject hardwareBufferObj);
using ToHardwareBuffer = jobject(JNIEnv* env, AHardwareBuffer* hardwareBuffer);
using FromBitmap = int(JNIEnv* env, jobject bitmap, AHardwareBuffer** outBuffer);

class AHardwareBufferFunctions {
 public:
  static const AHardwareBufferFunctions* Get();

  Allocate* allocate = nullptr;
  Acquire* acquire = nullptr;
  Release* release = nullptr;
  Describe* describe = nullptr;
  Lock* lock = nullptr;
  Unlock* unlock = nullptr;
  FromHardwareBuffer* fromHardwareBuffer = nullptr;
  ToHardwareBuffer* toHardwareBuffer = nullptr;
  FromBitmap* fromBitmap = nullptr;

 private:
  AHardwareBufferFunctions();
};
}  // namespace tgfx
