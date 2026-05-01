// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 8 GlyphRunBlob inline byte codec. GlyphRunBlob is the smallest text
// payload atom — each ElementText Tag holds one or more of these, each
// referencing a font and carrying the glyph data for a run of characters.
// Wire format is defined in §D.11 "GlyphRunBlob inline"; quantisation layers
// borrow v1's int32List / floatList / UBits primitives so bits-per-value
// adapts to the run's dynamic range (empty runs collapse to ~3-byte headers).
//
// The codec is split into its own file because it mixes byte-aligned header
// reads (u8 kind / u32 fontIndex / f32 fontSize / Matrix / glyphPrecisionLog2
// / varU32 glyphCount / u16[] glyphIds) with bit-aligned quantised arrays
// (positionXY / hasXformBits / xOffsets / anchorXY / scaleXY / rotations /
// skews); keeping it isolated makes the alignment dance auditable in one
// place.
#pragma once

#include <memory>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

// Inline (not a standalone Tag). Caller supplies the enclosing Tag end so
// Property reads inside the blob can honour the §4.4 rule 1 skip path.
void WriteGlyphRunBlobInline(::pag::EncodeStream* body, const GlyphRunBlob& blob);

// Reads one GlyphRunBlob starting at the current stream position. `tagEnd`
// is the enclosing ElementText Tag end (clamp target if the wire runs
// short). Returns nullopt on fatal errors already pushed to `ctx`.
bool ReadGlyphRunBlobInline(::pag::DecodeStream* stream, DecodeContext* ctx, uint32_t tagEnd,
                            GlyphRunBlob* out);

}  // namespace pagx::pag
