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

#include "pag/c/pag_player.h"
#include "pag_types_priv.h"

pag_player* pag_player_create() {
  return new pag_player(std::make_shared<pag::PAGPlayer>());
}

void PAGPlayerRelease(pag_player* player) {
  delete player;
}

void pag_player_set_composition(pag_player* player, pag_composition* composition) {
  if (player == nullptr) {
    return;
  }
  if (auto pagComposition = ToPAGComposition(composition)) {
    player->p->setComposition(pagComposition);
  } else {
    player->p->setComposition(nullptr);
  }
}

void pag_player_set_surface(pag_player* player, pag_surface* surface) {
  if (player == nullptr) {
    return;
  }
  if (surface) {
    player->p->setSurface(surface->p);
  } else {
    player->p->setSurface(nullptr);
  }
}

bool pag_player_get_cache_enable(pag_player* player) {
  if (player == nullptr) {
    return false;
  }
  return player->p->cacheEnabled();
}

void pag_player_set_cache_enable(pag_player* player, bool cacheEnable) {
  if (player == nullptr) {
    return;
  }
  player->p->setCacheEnabled(cacheEnable);
}

double pag_player_get_progress(pag_player* player) {
  if (player == nullptr) {
    return 0.0;
  }
  return player->p->getProgress();
}

void pag_player_set_progress(pag_player* player, double progress) {
  if (player == nullptr) {
    return;
  }
  player->p->setProgress(progress);
}

bool pag_player_wait(pag_player* player, pag_backend_semaphore* semaphore) {
  if (player == nullptr || semaphore == nullptr) {
    return false;
  }
  return player->p->wait(semaphore->p);
}

bool pag_player_flush(pag_player* player) {
  if (player == nullptr) {
    return false;
  }
  return player->p->flush();
}

bool pag_player_flush_and_signal_semaphore(pag_player* player, pag_backend_semaphore* semaphore) {
  if (player == nullptr) {
    return false;
  }
  if (semaphore == nullptr) {
    return player->p->flush();
  }
  return player->p->flushAndSignalSemaphore(&semaphore->p);
}

pag_scale_mode pag_player_get_scale_mode(pag_player* player) {
  if (player == nullptr) {
    return pag_scale_mode_none;
  }
  pag_scale_mode scaleMode;
  if (ToCScaleMode(player->p->scaleMode(), &scaleMode)) {
    return scaleMode;
  }
  return pag_scale_mode_none;
}

void pag_player_set_scale_mode(pag_player* player, pag_scale_mode scaleMode) {
  if (player == nullptr) {
    return;
  }
  pag::PAGScaleMode pagScaleMode;
  if (FromCScaleMode(scaleMode, &pagScaleMode)) {
    player->p->setScaleMode(pagScaleMode);
  }
}

int64_t pag_player_get_duration(pag_player* player) {
  if (player == nullptr) {
    return 0;
  }
  return player->p->duration();
}

int64_t pag_player_get_graphics_memory(pag_player* player) {
  if (player == nullptr) {
    return 0;
  }
  return player->p->graphicsMemory();
}
