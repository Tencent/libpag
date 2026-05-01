// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAG v2 framework Tag readers/writers — Phase 4a covers FileHeader,
// CompositionList, and Composition (header-only fields, layers stub).
// ImageAssetTable / FontAssetTable / LayerBlock land in Phase 4b.
//
// Function-signature contract (§D.13):
//   Write<TagName>(EncodeStream*, const <Data>&, EncodeSession*) -> void
//   Read<TagName>(DecodeStream*, DecodeContext*, uint64_t tagEnd) -> <Data>
//
// `tagEnd` is the absolute stream position one byte past this tag's body
// (computed by the dispatcher from tagStart + headerSize + tagLength using
// uint64_t to defeat the 32-bit overflow attack from §D.3).
#pragma once

#include <cstdint>
#include <memory>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/pag/DecodeContext.h"
#include "pagx/pag/EncodeSession.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

// ---- FileHeader (TagCode = 1) -------------------------------------------
// body: f32 width, f32 height, Color backgroundColor, Ratio frameRate,
//       u32 duration

void WriteFileHeader(::pag::EncodeStream* stream, const FileHeader& header, EncodeSession* session);

// On any malformed-canvas-size or stream truncation, pushes an error into
// `ctx` and returns a default-constructed FileHeader (caller should detect
// via ctx->hasError() before using the value).
FileHeader ReadFileHeader(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd);

// ---- CompositionList (TagCode = 4) --------------------------------------
// body: varU32 compositionCount, repeat[Composition Tag]
//
// Phase 4a writes only the wrapper + count + nested Composition Tags
// (each Composition body is delegated to WriteComposition below).

void WriteCompositionList(::pag::EncodeStream* stream,
                          const std::vector<std::unique_ptr<Composition>>& compositions,
                          EncodeSession* session);

// Reads the wrapper and dispatches each nested Composition Tag. Pushes
// fatal on count overflow (StructureLimitExceeded=105) or malformed nested
// TagHeader. Successfully-read compositions are appended to `out`.
void ReadCompositionList(::pag::DecodeStream* stream, DecodeContext* ctx, uint64_t tagEnd,
                         std::vector<std::unique_ptr<Composition>>* out);

// ---- Composition (TagCode = 5) ------------------------------------------
// body: utf8string id, u32 width, u32 height, varU32 layerCount,
//       repeat[LayerBlock Tag]
//
// Phase 4a treats `layers` as opaque — emits layerCount=0 always (Baker
// fills compositions with empty `layers` vectors; LayerBlock encode/decode
// land in Phase 4b together with the LayerBlock Tag readers).

void WriteComposition(::pag::EncodeStream* stream, const Composition& comp, EncodeSession* session);

// Reads a single Composition body. Returns nullptr on fatal; warns on
// width/height==0 (regenerates the field as 1 per design §D.7 P2-11).
std::unique_ptr<Composition> ReadComposition(::pag::DecodeStream* stream, DecodeContext* ctx,
                                             uint64_t tagEnd);

}  // namespace pagx::pag
