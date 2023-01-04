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

namespace tgfx {
/**
 * This utility class allows us to use AHardwareBuffer interfaces without compiling for API 26
 * (Oreo) and above.
 */
class HardwareBufferInterface {
 public:
  /**
   * Returns true if the local system has AHardwareBuffer support.
   */
  static bool Available();

  /**
   * Allocates a buffer that backs an AHardwareBuffer using the passed AHardwareBuffer_Desc.
   * Returns 0 on success, or an error number of the allocation fails for any reason. The returned
   * buffer has a reference count of 1.
   */
  static int Allocate(const AHardwareBuffer_Desc* desc, AHardwareBuffer** outBuffer);

  /**
   * Acquire a reference on the given AHardwareBuffer object. This prevents the object from being
   * deleted until the last reference is removed.
   */
  static void Acquire(AHardwareBuffer* buffer);

  /**
   * Remove a reference that was previously acquired with HardwareBufferInterface::Acquire().
   */
  static void Release(AHardwareBuffer* buffer);

  /**
   * Return a description of the AHardwareBuffer in the passed AHardwareBuffer_Desc struct.
   */
  static void Describe(const AHardwareBuffer* buffer, AHardwareBuffer_Desc* outDesc);

  /**
   * Lock the AHardwareBuffer for reading or writing, depending on the usage flags passed.  This
   * call may block if the hardware needs to finish rendering or if CPU caches need to be
   * synchronized, or possibly for other implementation- specific reasons.  If fence is not
   * negative, then it specifies a fence file descriptor that will be signaled when the buffer is
   * locked, otherwise the caller will block until the buffer is available.
   *
   * If rect is not NULL, the caller promises to modify only data in the area specified by rect.
   * If rect is NULL, the caller may modify the contents of the entire buffer.
   *
   * The content of the buffer outside of the specified rect is NOT modified by this call.
   *
   * The usage parameter may only specify AHARDWAREBUFFER_USAGE_CPU_*. If set, then
   * outVirtualAddress is filled with the address of the buffer in virtual memory.
   *
   * THREADING CONSIDERATIONS:
   *
   * It is legal for several different threads to lock a buffer for read access; none of the threads
   * are blocked.
   *
   * Locking a buffer simultaneously for write or read/write is undefined, but will neither
   * terminate the process nor block the caller; AHardwareBuffer_lock may return an error or leave
   * the buffer's content into an indeterminate state.
   *
   * @returns 0 on success, -EINVAL if buffer is NULL or if the usage flags are not a combination of
   * AHARDWAREBUFFER_USAGE_CPU_*, or an error number of the lock fails for any reason.
   */
  static int Lock(AHardwareBuffer* buffer, uint64_t usage, int32_t fence, const ARect* rect,
                  void** outVirtualAddress);
  /**
   * Unlock the AHardwareBuffer; must be called after all changes to the buffer are completed by the
   * caller. If fence is not NULL then it will be set to a file descriptor that is signaled when all
   * pending work on the buffer is completed. The caller is responsible for closing the fence when
   * it is no longer needed.
   *
   * @returns 0 on success, -EINVAL if \a buffer is NULL, or an error number if the unlock fails for
   * any reason.
   */
  static int Unlock(AHardwareBuffer* buffer, int32_t* fence);

  static jobject AHardwareBuffer_toHardwareBuffer(JNIEnv* env, AHardwareBuffer* buffer);

  static AHardwareBuffer* AHardwareBuffer_fromHardwareBuffer(JNIEnv* env,
                                                             jobject hardwareBufferObj);
};
}  // namespace tgfx
