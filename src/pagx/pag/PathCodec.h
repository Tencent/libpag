// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 tgfx::Path serialization — design doc §D.2.
//
// Wire layout (Phase 5b: format=0 only):
//   u8 pathFormat     // 0 = raw float (Phase 5b); 1 = quantised (Phase 12)
//   varU32 verbCount
//   repeat verbCount times:
//     u8 verbKind     // 0=Move, 1=Line, 2=Quad, 3=Conic, 4=Cubic, 5=Close
//     [point bytes]   // Move=1pt, Line=1pt, Quad=2pt, Conic=2pt+f32 weight,
//                     // Cubic=3pt, Close=0pt
//
// All point coordinates and conic weights are validated for finite values
// before being fed into tgfx::Path — NaN / Inf would propagate to the
// rasterizer / PathMeasure as undefined behaviour.
#pragma once

#include <cstdint>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DecodeContext.h"
#include "tgfx/core/Path.h"

namespace pagx::pag {

// Returns the chosen pathFormat for the given path. Phase 5b always returns
// 0 (raw float). Phase 12 may upgrade to 1 (quantised) when verbCount >
// QUANTIZATION_MIN_VERBS and all coords fit in int24 — see §D.2 P1-8.
uint8_t ChoosePathFormat(const tgfx::Path& path);

// Writes a path with the framing pathFormat byte + payload. Always emits
// format=0 in Phase 5b. Pushes a warning into `session->diag` if the input
// has unsupported state, but never fails fatally on the encode side.
void WritePath(::pag::EncodeStream* stream, const tgfx::Path& path);

// Reads a path that was written by WritePath. Pushes ctx fatal on:
//   - Unknown pathFormat byte
//   - verbCount > limits::MAX_PATH_VERBS (404 PathVerbLimitExceeded)
//   - NaN / Inf / out-of-range coord or conic weight (304 MalformedTag)
//   - Invalid verb kind byte
// Returns an empty path on any of the above (caller checks ctx->hasError()
// to decide whether to bail out).
tgfx::Path ReadPath(::pag::DecodeStream* stream, DecodeContext* ctx);

}  // namespace pagx::pag
