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

#include <jni.h>
#include "tgfx/platform/HardwareBuffer.h"

namespace tgfx {
/**
 * Return the HardwareBufferRef associated with a Java HardwareBuffer object, for interacting with
 * it through native code. This method does not acquire any additional reference to the returned
 * HardwareBufferRef. To keep the HardwareBufferRef live after the Java HardwareBuffer object got
 * garbage collected, make sure to use HardwareBufferRetain() to acquire an additional reference.
 */
HardwareBufferRef HardwareBufferFromJavaObject(JNIEnv* env, jobject hardwareBufferObject);

/**
 * Return a new Java HardwareBuffer object that wraps the passed native HardwareBufferRef object.
 */
jobject HardwareBufferToJavaObject(JNIEnv* env, HardwareBufferRef hardwareBuffer);
}  // namespace tgfx
