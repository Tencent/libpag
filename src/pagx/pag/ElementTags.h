// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 VectorElement Tag readers/writers (TagCodes 40-53). Phase 5a covers
// 11 of the 14 element types — Text / TextPath / TextModifier (codes 50/51/52
// in this enum's slot) defer to Phase 8 with the GlyphRunBlob serialization.
//
// Authoritative byte layouts: §D.11 (per-element field order) + §C.7
// (struct definitions). Each element Tag wraps an "InlineBody" function so
// the inner payload bytes are independent of the wrapping TagHeader, which
// keeps recursion (VectorGroup) trivially correct.
//
// Phase 5a simplifications:
//   - ElementShapePath omits the Property<Path> field — Phase 5b lands the
//     §D.2 quantised path codec. Phase 5a writes / reads only the
//     `position` Property + `reversed` flag.
//   - ElementFillStyle / ElementStrokeStyle ship only the SolidColor branch
//     of ShapeStyleData. Gradient / ImagePattern branches arrive in Phase 6
//     PaintBaker via ShapeStyleData's `sourceType` switch.
//   - ElementText / ElementTextPath / ElementTextModifier are not encoded
//     this Phase — the dispatch in ReadVectorElement returns nullptr +
//     warn for those TagCodes, matching the §6.4 forward-compat skip path.
#pragma once

#include <cstdint>
#include <memory>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/EncodeSession.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/TagCode.h"

namespace pagx::pag {

// ---- VectorPayload (TagCode = 24) ---------------------------------------
// body: varU32 elementCount, repeat[Element<Type> Tag]

void WriteVectorPayload(::pag::EncodeStream* stream, const VectorPayload& payload,
                        EncodeSession* session);

std::unique_ptr<VectorPayload> ReadVectorPayload(::pag::DecodeStream* stream, DecodeContext* ctx,
                                                 uint64_t tagEnd);

// ---- Single VectorElement (dispatch by TagCode) -------------------------
//
// Caller has already read the element's TagHeader; pass `code` and `tagEnd`
// (the absolute end-of-body position computed with the §D.3 uint64_t guard).
//
// Returns nullptr on fatal; on warn-only (unknown TagCode / Phase 8 deferred)
// returns nullptr WITHOUT pushing an error. Callers should treat nullptr as
// "skip + advance to tagEnd" and inspect ctx->hasError() to distinguish
// fatal vs warn.

void WriteVectorElement(::pag::EncodeStream* stream, const VectorElement& element,
                        EncodeSession* session);

std::unique_ptr<VectorElement> ReadVectorElement(::pag::DecodeStream* stream, DecodeContext* ctx,
                                                 TagCode code, uint64_t tagEnd);

}  // namespace pagx::pag
