// Copyright (C) 2026 Tencent. All rights reserved.
//
// Internal builders for Diagnostic objects + the canonical full-enum table.
// Named DiagBuild on purpose to avoid clashing with the public Diagnostic.h
// (the include resolution would otherwise be ambiguous inside src/pagx/pag/**).
#pragma once

#include <array>
#include <string>
#include <utility>
#include "codec/utils/DecodeStream.h"
#include "pagx/Diagnostic.h"
#include "pagx/pag/ErrorCode.h"

namespace pagx::pag {

// Decoder-side builder: auto-fills byteOffset from the stream's read pointer.
// stream MUST be non-null at the call site (Decoder always has a live stream
// when emitting diagnostics).
inline Diagnostic MakeDecodeDiag(ErrorCode code, ::pag::DecodeStream* stream, std::string msg = {},
                                 uint32_t contextIndex = UINT32_MAX) {
  return {code, std::move(msg), static_cast<uint32_t>(stream->position()), contextIndex};
}

// Baker / Inflater / Loader-side builder: byteOffset is always 0.
inline Diagnostic MakeDiag(ErrorCode code, std::string msg = {},
                           uint32_t contextIndex = UINT32_MAX) {
  return {code, std::move(msg), 0, contextIndex};
}

// Full enumeration of every DiagnosticCode value (Phase 0 deliverable, P0-6
// v2.16). DiagnosticTest.CodeToString.AllEnumValues iterates this table and
// asserts every entry resolves to a non-empty CodeToString result. Adding a
// new enum value REQUIRES adding it here too — otherwise:
//   * either the size mismatch trips the static_assert below, OR
//   * the test catches the missing CodeToString switch case at runtime.
inline constexpr std::array<DiagnosticCode, 43> kAllDiagnosticCodes = {
    DiagnosticCode::Ok,
    // Baker fatal 100-199
    DiagnosticCode::LayoutNotApplied,
    DiagnosticCode::UnresolvedImports,
    DiagnosticCode::NullDocument,
    DiagnosticCode::EmptyCompositions,
    DiagnosticCode::CompositionCycleDepth,
    DiagnosticCode::StructureLimitExceeded,
    DiagnosticCode::ProducerFatal,
    // Baker warning 200-299
    DiagnosticCode::ImageSourceMissing,
    DiagnosticCode::FontSourceMissing,
    DiagnosticCode::MaskTargetMissing,
    DiagnosticCode::MaskSelfReference,
    DiagnosticCode::BlendModeUnmapped,
    DiagnosticCode::InverseMatrixNonInvertible,
    DiagnosticCode::TextGlyphDataEmpty,
    DiagnosticCode::EmptyDocument,
    DiagnosticCode::TextGlyphRunsDowngraded,
    // Codec fatal 300-399
    DiagnosticCode::InvalidMagic,
    DiagnosticCode::UnsupportedVersion,
    DiagnosticCode::UnsupportedCompression,
    DiagnosticCode::TruncatedData,
    DiagnosticCode::MalformedTag,
    DiagnosticCode::BodyLengthOutOfRange,
    DiagnosticCode::InvalidCompositionIndex,
    DiagnosticCode::FileReadFailed,
    // Codec warning 400-499
    DiagnosticCode::UnknownTagCode,
    DiagnosticCode::UnknownPropertyEncoding,
    DiagnosticCode::ImageResourceSizeExceeded,
    DiagnosticCode::StringInvalidUtf8,
    DiagnosticCode::PathVerbLimitExceeded,
    DiagnosticCode::GlyphCountLimitExceeded,
    DiagnosticCode::LayerDepthLimitExceeded,
    DiagnosticCode::InvalidEnumValue,
    DiagnosticCode::FontResourceSizeExceeded,
    DiagnosticCode::PrematureEndTag,
    // Inflater warning 600-699
    DiagnosticCode::InflateImageDecodeFailed,
    DiagnosticCode::InflateFontCreateFailed,
    DiagnosticCode::InflateGlyphRunBuildFailed,
    DiagnosticCode::InflateMaskResolveFailed,
    DiagnosticCode::InflaterEmptyDocument,
    DiagnosticCode::InflateCompositionCycle,
    DiagnosticCode::InflaterLayerBudgetExceeded,
    DiagnosticCode::InflateMaskCycle,
};

}  // namespace pagx::pag
