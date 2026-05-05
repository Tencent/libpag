// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

#include <cstdint>
#include <string>

namespace pagx {

/**
 * Diagnostic codes for PAG v2 export/load operations. Codes are partitioned
 * by module + severity; new codes must be appended within a segment (never
 * reuse deleted numbers, never cross segments).
 *
 *   100-199: Baker fatal    (document cannot be produced)
 *   200-299: Baker warning  (document produced with degradation)
 *   300-399: Codec fatal    (decode/encode failed)
 *   400-499: Codec warning  (decode succeeded with skipped content)
 *   600-699: Inflater warning (layer tree produced with degradation)
 *
 * The full code list is in 附录 G.2 of the design doc. Adding a new code
 * here is an ABI-level change; follow the segment-append rule strictly.
 *
 * Maintenance checklist when adding a new code (Phase 0 invariant; missing
 * any step makes DiagnosticTest.CodeToString.AllEnumValues fail):
 *   (a) declare it inside the proper segment of the enum below
 *   (b) append it to kAllDiagnosticCodes[] in src/pagx/pag/DiagBuild.h and
 *       bump the std::array<DiagnosticCode, N> size by 1
 *   (c) add a switch case in CodeToString in src/pagx/Diagnostic.cpp
 *   (d) extend the §G.6 test matrix in the design doc with a triggering test
 *   (e) if contextIndex carries a structured meaning, extend the ABI table
 *       in this header's struct comment below
 */
enum class DiagnosticCode : uint16_t {
  Ok = 0,

  // Baker fatal
  LayoutNotApplied           = 100,
  UnresolvedImports          = 101,
  NullDocument               = 102,
  EmptyCompositions          = 103,
  CompositionCycleDepth      = 104,
  StructureLimitExceeded     = 105,   // any structural count/length above appendix-H limits
                                      // (MAX_COMPOSITIONS / MAX_LAYERS_PER_COMPOSITION /
                                      //  MAX_VECTOR_ELEMENTS_PER_PAYLOAD / MAX_FILTERS_PER_LAYER /
                                      //  MAX_STYLES_PER_LAYER / MAX_GRADIENT_STOPS /
                                      //  MAX_PATH_VERBS / MAX_GLYPHS_PER_RUN /
                                      //  MAX_NAME_STRING_BYTES / MAX_TEXT_STRING_BYTES, etc.)
                                      // distinct from StringInvalidUtf8(403):
                                      //   this one  = length overrun  (structural pathology)
                                      //   that one  = byte-level UTF-8 corruption
                                      // message should name the offending field, e.g.
                                      // "MAX_VECTOR_ELEMENTS_PER_PAYLOAD exceeded (got 120000)".
  ProducerFatal              = 106,   // P1-5 v2.17: Decoder hit TagCode::ErrorMarker —
                                      // upstream Baker fatal aborted the byte stream (§8.3ter).
                                      // Do NOT reset doc (partial result is usable),
                                      // but stop parsing the remaining body.

  // Baker warning
  ImageSourceMissing         = 200,
  FontSourceMissing          = 201,   // Phase 16: pagx::Text.fontFamily empty; Inflater falls back to FontProvider chain
  MaskTargetMissing          = 202,
  MaskSelfReference          = 203,
  BlendModeUnmapped          = 204,
  InverseMatrixNonInvertible = 205,
  TextGlyphDataEmpty         = 206,   // deprecated in v2.20 (runtime-shape mode): numeric value reserved, do not re-use
  EmptyDocument              = 207,
  TextGlyphRunsDowngraded    = 208,   // Phase 16: pre-shaped pagx::Text.glyphRuns dropped; runtime-shape fallback emitted

  // Codec fatal
  InvalidMagic               = 300,
  UnsupportedVersion         = 301,
  UnsupportedCompression     = 302,
  TruncatedData              = 303,
  MalformedTag               = 304,
  BodyLengthOutOfRange       = 305,
  InvalidCompositionIndex    = 306,
  FileReadFailed             = 307,   // PAGLoader::LoadFromFile path-specific

  // Codec warning
  UnknownTagCode             = 400,
  UnknownPropertyEncoding    = 401,
  ImageResourceSizeExceeded  = 402,   // single image bytes above MAX_IMAGE_BYTES (contextIndex = imageIndex)
  StringInvalidUtf8          = 403,
  PathVerbLimitExceeded      = 404,
  GlyphCountLimitExceeded    = 405,
  LayerDepthLimitExceeded    = 406,
  InvalidEnumValue           = 407,
  FontResourceSizeExceeded   = 408,   // single font bytes above MAX_FONT_BYTES (contextIndex = fontIndex; v2.16 split from 402)
  PrematureEndTag            = 409,   // P0-15 v2.18: TagCode::End=0 reached but bodyLength bytes still remain — upstream truncated

  // Inflater warning
  InflateImageDecodeFailed   = 600,
  InflateFontCreateFailed    = 601,
  InflateGlyphRunBuildFailed = 602,
  InflateMaskResolveFailed   = 603,
  InflaterEmptyDocument      = 604,   // compositions[0] missing or empty; Result.layer == nullptr
  InflateCompositionCycle    = 605,   // compositions reference graph self-loop / chain cycle / depth > MAX_COMPOSITION_REF_DEPTH
  InflaterLayerBudgetExceeded = 606,  // cumulative tgfx::Layer count above MAX_INFLATED_LAYER_COUNT=1e6; subtree degraded to empty Layer
  InflateMaskCycle           = 607,   // P0-16 v2.18: Layer mask cycle (A.mask=B + B.mask=A, or self-loop A.mask=A); masks on the cycle are skipped
  TextShapingHintMiss        = 608,   // Phase 16.6: ElementText.shapedRuns typefaceKey did not match the Inflater-resolved typeface; hint dropped and runtime shape used instead (contextIndex = layerIndex)
};

/**
 * Structured diagnostic emitted by export/load operations.
 *   - `code`         : machine-readable code; callers should dispatch on this.
 *                      NUMERIC VALUE is part of the stable ABI — callers MAY
 *                      persist the underlying uint16_t in logs/crash reports
 *                      and expect it to retain semantic meaning across pagx
 *                      versions. Deprecating a code means leaving its number
 *                      reserved, never reusing it for new semantics.
 *   - `message`      : optional human-readable context; may be empty.
 *                      WARNING: `message` text is unstable English debug text,
 *                      NOT a translation key. Do NOT persist or switch on it;
 *                      future pagx may introduce a separate `messageKey` field
 *                      for i18n (see §G.4).
 *   - `byteOffset`   : decoder path = stream->position() at error time; all
 *                      other paths (baker/encoder/inflater) = 0.
 *   - `contextIndex` : STRUCTURED resource/layer reference for codes where
 *                      the caller typically needs to identify WHICH asset
 *                      failed (see table below). UINT32_MAX means "not
 *                      applicable / unknown". Callers MAY dispatch on this.
 *                      MANDATORY GUARD (P1-2 v2.17): before using
 *                      contextIndex as an array index, callers MUST check
 *                      `d.contextIndex != UINT32_MAX` — some code paths
 *                      (bridge push across Tag streams, fatal error without
 *                      resource association) push UINT32_MAX sentinel even
 *                      for codes in the semantic table. Raw indexing without
 *                      the guard triggers UB (OOB read) on sentinel rows.
 *
 * `code → contextIndex` semantic table (stable ABI):
 *   200 ImageSourceMissing         → imageIndex in PAGDocument::images[]
 *   201 FontSourceMissing          → fontIndex in PAGDocument::fonts[]
 *   202 MaskTargetMissing          → layerIndex in enclosing Composition
 *   203 MaskSelfReference          → layerIndex
 *   206 TextGlyphDataEmpty         → layerIndex (the ElementText holder)
 *   208 GlyphRunKindInferred       → layerIndex
 *   306 InvalidCompositionIndex    → compositionIndex AS READ FROM BYTES (raw value).
 *                                    May be ≥ doc.compositions.size()! Consumers MUST
 *                                    range-check before indexing into compositions[].
 *   402 ImageResourceSizeExceeded  → imageIndex
 *   407 InvalidEnumValue           → raw enum byte (0-255 zero-extended)
 *   408 FontResourceSizeExceeded   → fontIndex
 *   600 InflateImageDecodeFailed   → imageIndex
 *   601 InflateFontCreateFailed    → fontIndex
 *   602 InflateGlyphRunBuildFailed → layerIndex
 *   603 InflateMaskResolveFailed   → layerIndex
 *   605 InflateCompositionCycle    → compositionIndex of the self/cyclic reference
 *   606 InflaterLayerBudgetExceeded → layerIndex of the first Layer that triggered budget exhaustion in the failing subtree
 *   607 InflateMaskCycle            → layerIndex of any host layer participating in the SCC mask cycle
 *   106 ProducerFatal               → UINT32_MAX (byteOffset locates ErrorMarker Tag position)
 *   409 PrematureEndTag             → UINT32_MAX (byteOffset locates End Tag position)
 *   All other codes                → UINT32_MAX (not meaningful)
 */
struct Diagnostic {
  DiagnosticCode code         = DiagnosticCode::Ok;
  std::string    message;
  uint32_t       byteOffset   = 0;
  uint32_t       contextIndex = UINT32_MAX;
};

/**
 * Format a Diagnostic as "[<CodeName>] <message>" with a " @0x<hex>" suffix
 * when byteOffset is non-zero, and a " #ctx=<n>" suffix when contextIndex is
 * not UINT32_MAX. Example:
 *   "[TruncatedData] unexpected EOF @0x4a2c"
 *   "[LayoutNotApplied] document must have applyLayout() called first"
 *   "[ImageSourceMissing] path not found #ctx=3"
 *   "[InflateCompositionCycle] cycle @0x14 #ctx=0"
 */
std::string FormatDiagnostic(const Diagnostic& d);

// ---- Helper classifiers (cheap adds) — implementation may change, but the
// returned enum value + the code→segment mapping are part of the ABI ----

/** Severity category derived from code segment. */
enum class DiagnosticSeverity : uint8_t { Fatal, Warning };
DiagnosticSeverity SeverityOf(DiagnosticCode code);

/** Stage category derived from code segment. */
enum class DiagnosticStage : uint8_t { Baker, Codec, Inflater };
DiagnosticStage StageOf(DiagnosticCode code);

// ---- Version query (cheap add) ----
struct VersionInfo {
  uint16_t    formatVersion;   // PAG v2 = 0x0002 (low byte = major, high byte = minor).
                               // Kept in sync with PeekResult::formatVersion — same width and semantics.
  const char* gitSha;          // build-time git SHA, short. Process-lifetime pointer into .rodata.
  const char* buildDate;       // __DATE__.             Process-lifetime pointer into .rodata.
};
VersionInfo Version();

}  // namespace pagx
