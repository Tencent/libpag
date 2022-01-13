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

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if TARGET_OS_IPHONE

#include "gpu/opengl/eagl/EAGLHardwareTexture.h"

namespace pag {
using GLHardwareTexture = EAGLHardwareTexture;
}  // namespace pag

#elif TARGET_OS_MAC

#include "gpu/opengl/cgl/CGLHardwareTexture.h"

namespace pag {
using GLHardwareTexture = CGLHardwareTexture;
}  // namespace pag

#endif
