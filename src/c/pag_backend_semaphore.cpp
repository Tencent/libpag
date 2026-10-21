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

#include "pag/c/pag_backend_semaphore.h"
#include "pag_types_priv.h"

pag_backend_semaphore* pag_backend_semaphore_create() {
  return new pag_backend_semaphore();
}

bool pag_backend_semaphore_is_initialized(pag_backend_semaphore* semaphore) {
  if (semaphore == nullptr) {
    return false;
  }
  return semaphore->p.isInitialized();
}

void pag_backend_semaphore_init_gl(pag_backend_semaphore* semaphore, void* glSync) {
  if (semaphore == nullptr) {
    return;
  }
  semaphore->p.initGL(glSync);
}

void* pag_backend_semaphore_get_gl_sync(pag_backend_semaphore* semaphore) {
  if (semaphore == nullptr) {
    return nullptr;
  }
  return semaphore->p.glSync();
}
