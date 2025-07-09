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

#include <cstdlib>
#include "tgfx/platform/Print.h"

namespace pag {
#define ABORT(msg)                                                                \
  do {                                                                            \
    ::tgfx::PrintError("%s:%d: fatal error: \"%s\"\n", __FILE__, __LINE__, #msg); \
    ::abort();                                                                    \
  } while (false)

#ifdef NO_LOG

#define LOGI(...)
#define LOGE(...)
#define ASSERT(assertion)

#else

#define LOGI(...) ::tgfx::PrintLog(__VA_ARGS__)
#define LOGE(...) ::tgfx::PrintError(__VA_ARGS__)
#define ASSERT(assertion) \
  if (!(assertion)) {     \
    ABORT(#assertion);    \
  }

#endif

#if DEBUG

#define DEBUG_ASSERT(assertion) ASSERT(assertion)

#else

#define DEBUG_ASSERT(assertion)

#endif
}  // namespace pag
