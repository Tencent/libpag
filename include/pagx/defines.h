/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#if !defined(PAGX_API)
#if defined(PAGX_DLL)
#if defined(_MSC_VER)
#define PAGX_API __declspec(dllexport)
#else
#define PAGX_API __attribute__((visibility("default")))
#endif
#else
#define PAGX_API
#endif
#endif

#if !defined(RTTR_AUTO_REGISTER_CLASS)
#define RTTR_AUTO_REGISTER_CLASS
#endif

#if !defined(RTTR_SKIP_REGISTER_PROPERTY)
#define RTTR_SKIP_REGISTER_PROPERTY
#endif

#if !defined(RTTR_REGISTER_FUNCTION_AS_PROPERTY)
#define RTTR_REGISTER_FUNCTION_AS_PROPERTY(propertyName, function)
#endif

// Only define empty RTTR_ENABLE if real RTTR is not being used.
// The real RTTR library defines RTTR_RTTR_ENABLE_H_ guard in rttr/rttr_enable.h

#ifdef PAG_USE_RTTR
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "rttr/array_range.h"
#include "rttr/constructor.h"
#include "rttr/destructor.h"
#include "rttr/enum_flags.h"
#include "rttr/enumeration.h"
#include "rttr/library.h"
#include "rttr/method.h"
#include "rttr/property.h"
#include "rttr/rttr_cast.h"
#include "rttr/rttr_enable.h"
#include "rttr/type.h"
#pragma clang diagnostic pop
#else

#define RTTR_ENABLE(...)

#endif
