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
 * Creates a new pag_animator with the specified listener. Release it by pag_release.
 */
PAG_EXPORT pag_animator* pag_animator_create(pag_animator_listener* listener, void* user);

PAG_EXPORT bool pag_animator_is_sync(pag_animator* animator);

PAG_EXPORT void pag_animator_set_sync(pag_animator* animator, bool sync);

PAG_EXPORT int64_t pag_animator_get_duration(pag_animator* animator);

PAG_EXPORT void pag_animator_set_duration(pag_animator* animator, int64_t duration);

PAG_EXPORT int pag_animator_get_repeat_count(pag_animator* animator);

PAG_EXPORT void pag_animator_set_repeat_count(pag_animator* animator, int repeatCount);

PAG_EXPORT double pag_animator_get_progress(pag_animator* animator);

PAG_EXPORT void pag_animator_set_progress(pag_animator* animator, double progress);

PAG_EXPORT bool pag_animator_is_running(pag_animator* animator);

PAG_EXPORT void pag_animator_start(pag_animator* animator);

PAG_EXPORT void pag_animator_cancel(pag_animator* animator);

PAG_EXPORT void pag_animator_update(pag_animator* animator);

PAG_C_PLUS_PLUS_END_GUARD
