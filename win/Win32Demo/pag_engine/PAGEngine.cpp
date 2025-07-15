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

#include "PAGEngine.h"

#include "PAGEngineImp.h"
#include "framework.h"

namespace pagengine {

PAGEngine* CreatePAGEngine() {
  return new PAGEngineImpl();
}

bool ReleasePAGEngine(PAGEngine* pag_engine) {
  if (!pag_engine) {
    return false;
  }

  delete pag_engine;
  return true;
}

}  // namespace pagengine
