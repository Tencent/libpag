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

// Per-document render-time typeface cache. Held by PAGXDocument purely as a storage host so the
// cache shares the document's lifetime; PAGXDocument itself never reads or writes this data.
// GlyphRunRenderer is the sole consumer and accesses the map directly via friend access.
//
// Key: raw Font pointer. Sound only because the cache is bounded to a single document — Font
// addresses become invalid on document destruction, but so does the cache. Single-threaded by
// contract: rendering on a given document is serialized by the caller.
//
// Immutability contract: once a Font node is cached its content (glyphs, unitsPerEm, glyph
// offsets/advances) MUST be treated as immutable for the remaining lifetime of the document. The
// cache uses the Font pointer as identity and does not detect content changes; mutating a cached
// Font in place will silently return a stale typeface on subsequent lookups. Callers that mutate
// Font content (e.g. PAGXDocument::clearEmbed) are responsible for clearing the cache.
struct TypefaceCache {
  std::unordered_map<const Font*, std::shared_ptr<tgfx::Typeface>> typefaces = {};
};

}  // namespace pagx
