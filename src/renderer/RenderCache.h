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
#include <unordered_map>
#include "tgfx/core/Typeface.h"

namespace pagx {

class Font;

// Per-document render-time cache. Owned by PAGXDocument and shares its lifetime, so cached
// entries cannot outlive the Font nodes they key on. Keying on the raw Font pointer is sound only
// because the cache is bounded to a single document (Font addresses become invalid on document
// destruction, but so does the cache). Single-threaded by contract: rendering on a given document
// is serialized by the caller.
class RenderCache {
 public:
  std::shared_ptr<tgfx::Typeface> getTypeface(const Font* fontNode) const;
  void setTypeface(const Font* fontNode, std::shared_ptr<tgfx::Typeface> typeface);

 private:
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> typefaces = {};
};

}  // namespace pagx
