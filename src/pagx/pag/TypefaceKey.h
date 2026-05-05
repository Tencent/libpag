// Copyright (C) 2026 Tencent. All rights reserved.
//
// Shared helper used by TextBaker and LayerInflater to generate a signature
// string for a tgfx::Typeface. The Baker serializes the signature of the
// typeface it shaped against into ElementTextData::ShapedRun::typefaceKey;
// the Inflater recomputes the signature of the typeface it just resolved
// and only consumes the hint when the two strings match byte-for-byte.
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
// Miss semantics: a mismatch is safe — the Inflater falls back to runtime
// shape (HarfBuzz or primitive), which is what a host without the exact
// same font would produce anyway. An info-level diagnostic is emitted so
// observability tooling can spot font-substitution drift.

#pragma once

#include <string>
#include "tgfx/core/Typeface.h"

namespace pagx::pag {

inline std::string MakeTypefaceKey(const tgfx::Typeface& typeface) {
  return typeface.fontFamily() + "|" + typeface.fontStyle() + "|" +
         std::to_string(typeface.unitsPerEm()) + "|" + std::to_string(typeface.glyphsCount());
}

}  // namespace pagx::pag
