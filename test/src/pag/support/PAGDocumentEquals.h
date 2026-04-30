// Copyright (C) 2026 Tencent. All rights reserved.
//
// Deep field-by-field equality for PAGDocument trees. Used by Baker unit
// tests to assert "Bake(buildA) produced the same document as Bake(buildB)"
// without rolling per-test ad-hoc comparators (P0-9 v2.18).
//
// Float equality is BIT-EXACT (bit_cast<uint32_t> compare) — by design, since
// Baker constructs Property values from canonical defaults (no rounding) and
// any drift indicates a real bug. Callers that need approximate comparison
// should compare Diagnostic fields instead.
#pragma once

#include <string>
#include "pagx/pag/PAGDocument.h"

namespace pagx::test {

// Returns true when every observable field of two PAGDocument trees matches.
// On mismatch, `diff` (when non-null) receives a human-readable summary
// pointing at the first divergence — typical contents:
//   "compositions[0].layers[2].alpha.value: 1.0 vs 0.5"
//   "images.size: 3 vs 4"
bool DocumentsEqual(const pagx::pag::PAGDocument& a, const pagx::pag::PAGDocument& b,
                    std::string* diff = nullptr);

// Same as DocumentsEqual but operates on optional unique_ptr roots — both
// nullptr counts as equal, mismatch on null-vs-non-null reports concisely.
bool DocumentsEqual(const pagx::pag::PAGDocument* a, const pagx::pag::PAGDocument* b,
                    std::string* diff = nullptr);

}  // namespace pagx::test
