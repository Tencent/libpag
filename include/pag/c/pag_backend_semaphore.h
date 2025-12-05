/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag_types.h"

PAG_C_PLUS_PLUS_BEGIN_GUARD

/**
 * Creates a new pag_backend_semaphore. Release it by pag_release.
 */
PAG_EXPORT pag_backend_semaphore* pag_backend_semaphore_create();

PAG_EXPORT bool pag_backend_semaphore_is_initialized(pag_backend_semaphore* semaphore);

PAG_EXPORT void pag_backend_semaphore_init_gl(pag_backend_semaphore* semaphore, void* glSync);

PAG_EXPORT void* pag_backend_semaphore_get_gl_sync(pag_backend_semaphore* semaphore);

PAG_C_PLUS_PLUS_END_GUARD
