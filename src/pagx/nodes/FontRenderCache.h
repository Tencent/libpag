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

#include <memory>
#include "tgfx/core/Typeface.h"

namespace pagx {

// Internal render-side cache attached to each Font node. Defined in a private header so that
// public consumers of pagx::Font never see tgfx types in their include graph.
//
// Lifetime: owned by Font via std::unique_ptr<FontRenderCache>; created lazily by
// GlyphRunRenderer on the first render and dropped via Font::resetRenderCache (called from
// PAGXDocument::clearEmbed) or when the owning Font is destroyed.
struct FontRenderCache {
  // Built tgfx typeface, or nullptr if the build was attempted and produced no result.
  std::shared_ptr<tgfx::Typeface> typeface = nullptr;

  // Sentinel that tracks whether the typeface build was already attempted (success or failure).
  // Used to avoid re-running the (potentially expensive or pathological) build on every render.
  bool built = false;
};

}  // namespace pagx
