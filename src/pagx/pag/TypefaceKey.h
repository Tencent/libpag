// Copyright (C) 2026 Tencent. All rights reserved.
//
// Shared helper used by TextBaker and LayerInflater to generate a signature
// string for a tgfx::Typeface. The Baker serializes the signature of the
// typeface it shaped against into
// ElementTextData::ShapedGlyphRun::typefaceKey; the Inflater recomputes the
// signature of the typeface it just resolved and emits a TextShapingHintMiss
// diagnostic when the two strings differ — the layout still replays with the
// host-substituted typeface (case B's by-design fallback).
//
// Signature content is the four-tuple `family|style|unitsPerEm|glyphsCount`.
// Rationale:
//   - family + style alone collide across font versions / subsets / vendor
//     forks (e.g. macOS Arial across minor OS updates).
//   - unitsPerEm pins the em-square (almost always 1000/1024/2048) and
//     protects against typeface objects that claim the same name but carry
//     different metrics.
//   - glyphsCount catches subsetted or stripped variants (e.g. embedded
//     fonts with only the glyphs used by the source doc).
//
// Miss semantics: a mismatch is safe — Latin glyph IDs are stable across
// font versions, so the substitution typically manifests as subtle shape
// drift rather than layout corruption. The info-level diagnostic lets
// observability tooling spot font-substitution drift in production.

#pragma once

#include <string>
#include "tgfx/core/Typeface.h"

namespace pagx::pag {

inline std::string MakeTypefaceKey(const tgfx::Typeface& typeface) {
  return typeface.fontFamily() + "|" + typeface.fontStyle() + "|" +
         std::to_string(typeface.unitsPerEm()) + "|" + std::to_string(typeface.glyphsCount());
}

}  // namespace pagx::pag
