/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

namespace pag {

/**
 * The USE(x, ...) template is used to silence C++ compiler warnings issued for (yet) unused
 * variables (typically parameters).
 */

#if defined(_MSC_VER)
#define USE(expr)                                                                         \
  __pragma(warning(push)) /* Disable warning C4127: conditional expression is constant */ \
      __pragma(warning(disable : 4127)) do {                                              \
    if (false) {                                                                          \
      (void)(expr);                                                                       \
    }                                                                                     \
  }                                                                                       \
  while (false) __pragma(warning(pop))
#else
#define USE(expr)   \
  do {              \
    if (false) {    \
      (void)(expr); \
    }               \
  } while (false)
#endif

}  // namespace pag
