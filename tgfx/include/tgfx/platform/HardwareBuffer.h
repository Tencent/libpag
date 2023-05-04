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

#include "tgfx/core/ImageInfo.h"

#if defined(__ANDROID__) || defined(ANDROID)

struct AHardwareBuffer;

#elif defined(__APPLE__)

struct __CVBuffer;

#endif

namespace tgfx {
#if defined(__ANDROID__) || defined(ANDROID)

typedef AHardwareBuffer* HardwareBufferRef;

#elif defined(__APPLE__)

// __CVBuffer == CVPixelBufferRef
typedef __CVBuffer* HardwareBufferRef;

#else

struct HardwareBuffer {};

typedef HardwareBuffer* HardwareBufferRef;

#endif

/**
 * Returns true if the current platform has hardware buffer support. Otherwise, returns false.
 */
bool HardwareBufferAvailable();

/**
 * Returns true if the given hardware buffer object is valid and can be bind to a texture.
 * Otherwise, returns false.
 */
bool HardwareBufferCheck(HardwareBufferRef buffer);

/**
 * Allocates a hardware buffer for a given size and pixel format (alphaOnly). Returns nullptr if
 * allocation fails. The returned buffer has a reference count of 1.
 */
HardwareBufferRef HardwareBufferAllocate(int width, int height, bool alphaOnly = false);

/**
 * Retains a reference on the given hardware buffer object. This prevents the object from being
 * deleted until the last reference is removed.
 */
HardwareBufferRef HardwareBufferRetain(HardwareBufferRef buffer);

/**
 * Removes a reference that was previously acquired with HardwareBufferRef_acquire().
 */
void HardwareBufferRelease(HardwareBufferRef buffer);

/**
 * Locks and returns the base address of the hardware buffer. Returns nullptr if the lock fails for
 * any reason and leave the buffer unchanged. The caller must call HardwareBufferUnlock() when
 * finished with the buffer.
 */
void* HardwareBufferLock(HardwareBufferRef buffer);

/**
 * Unlocks the base address of the hardware buffer. Call this to balance a successful call to
 * HardwareBufferLock().
 */
void HardwareBufferUnlock(HardwareBufferRef buffer);

/**
 * Returns an ImageInfo describing the width, height, color type, alpha type, and row bytes of the
 * given hardware buffer object. Returns an empty ImageInfo if the buffer is nullptr or not
 * recognized.
 */
ImageInfo HardwareBufferGetInfo(HardwareBufferRef buffer);
}  // namespace tgfx
