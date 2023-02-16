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

typedef void* HardwareBufferRef;

#endif
}  // namespace tgfx
