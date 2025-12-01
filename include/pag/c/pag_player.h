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
 * Creates a pag_player object. Release it by pag_release.
 */
PAG_EXPORT pag_player* pag_player_create();

PAG_EXPORT void pag_player_set_composition(pag_player* player, pag_composition* composition);

PAG_EXPORT void pag_player_set_surface(pag_player* player, pag_surface* surface);

PAG_EXPORT bool pag_player_get_cache_enable(pag_player* player);

PAG_EXPORT void pag_player_set_cache_enable(pag_player* player, bool cacheEnable);

PAG_EXPORT double pag_player_get_progress(pag_player* player);

PAG_EXPORT void pag_player_set_progress(pag_player* player, double progress);

PAG_EXPORT bool pag_player_wait(pag_player* player, pag_backend_semaphore* semaphore);

PAG_EXPORT bool pag_player_flush(pag_player* player);

PAG_EXPORT bool pag_player_flush_and_signal_semaphore(pag_player* player,
                                                   pag_backend_semaphore* semaphore);

PAG_EXPORT pag_scale_mode pag_player_get_scale_mode(pag_player* player);

PAG_EXPORT void pag_player_set_scale_mode(pag_player* player, pag_scale_mode scaleMode);

PAG_EXPORT int64_t pag_player_get_duration(pag_player* player);

PAG_EXPORT int64_t pag_player_get_graphics_memory(pag_player* player);

PAG_C_PLUS_PLUS_END_GUARD
