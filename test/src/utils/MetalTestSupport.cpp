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

#include "MetalTestSupport.h"
#include "base/utils/Log.h"
#include "core/AtlasManager.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/gpu/Context.h"

namespace pag {
bool ResetMetalAtlasForComparison() {
  DeviceLockScope scope(Devices::MakeDefault());
  if (!scope) {
    LOGE("ResetMetalAtlasForComparison: failed to create or lock the Metal device.");
    return false;
  }
  auto context = scope.context();
  context->flushAndSubmit(true);
  // Test-only workaround matching tgfx's ContextScope: Linear glyph sampling can change pixels
  // with atlas placement. Clear history for independent baselines, not for reuse behavior tests.
  // This does not fix production sampling. Replace the private AtlasManager dependency when tgfx
  // provides a supported test-reset API with equivalent isolation semantics.
  context->atlasManager()->releaseAll();
  return true;
}
}  // namespace pag
