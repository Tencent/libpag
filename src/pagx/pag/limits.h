// Copyright (C) 2026 Tencent. All rights reserved.
//
// Hard limits enforced by Codec / Baker / Inflater. Authoritative source:
// docs/pagx_to_pag_v2_design.md §H.1. Establish the full set in Phase 0 to
// avoid back-and-forth edits across later phases — only the diagnostic-level
// caps are consumed by Phase 0 itself; the rest are referenced by Phase 1+.
#pragma once

#include <cstddef>
#include <cstdint>

namespace pagx::pag::limits {

// ---- File level ----
constexpr size_t MAX_TOTAL_BODY_BYTES = 1024ull * 1024 * 1024;  // 1 GB

// ---- Resource level ----
constexpr size_t MAX_IMAGE_BYTES = 100ull * 1024 * 1024;  // 100 MB / image
constexpr size_t MAX_FONT_BYTES = 50ull * 1024 * 1024;    // 50 MB / font
constexpr uint32_t MAX_IMAGES = 10000;
constexpr uint32_t MAX_FONTS = 1000;
constexpr uint32_t MAX_FONT_AXES = 64;  // OpenType fvar table convention

// ---- Layer / structural ----
constexpr uint32_t MAX_LAYER_DEPTH = 64;  // composition + child combined
constexpr uint32_t MAX_LAYERS_PER_COMPOSITION = 10000;
constexpr uint32_t MAX_COMPOSITIONS = 1000;

// ---- FileHeader / canvas size (P0-5 v2.15): guard NaN / Inf / int overflow ----
constexpr uint32_t MAX_CANVAS_DIM = 16384;  // GPU texture upper bound (WebGL / iOS Metal)

// ---- Path / Glyph ----
constexpr uint32_t MAX_PATH_VERBS = 100000;
constexpr uint32_t MAX_GLYPHS_PER_RUN = 100000;

// ---- Strings ----
constexpr size_t MAX_NAME_STRING_BYTES = 64ull * 1024;  // 64 KB (name / id / family / style)
constexpr size_t MAX_TEXT_STRING_BYTES =
    10ull * 1024 * 1024;  // 10 MB (TextPayload::text and similar bodies)

// ---- Gradient stops ----
constexpr uint32_t MAX_GRADIENT_STOPS = 256;

// ---- Filters / Styles ----
constexpr uint32_t MAX_FILTERS_PER_LAYER = 64;
constexpr uint32_t MAX_STYLES_PER_LAYER = 64;

// ---- VectorElement children ----
constexpr uint32_t MAX_VECTOR_ELEMENTS_PER_PAYLOAD = 100000;
constexpr uint32_t MAX_VECTOR_ELEMENT_DEPTH = 64;
constexpr uint32_t MAX_CHILDREN_PER_LAYER = 10000;

// ---- Other count limits (P1-A; see §D.9 / §D.11) ----
constexpr uint32_t MAX_MASK_PATH_DEPTH = 64;
constexpr uint32_t MAX_RANGE_SELECTORS_PER_MODIFIER = 16;
constexpr uint32_t MAX_RUNS_PER_TEXT = 256;
constexpr uint32_t MAX_VECTOR_VALUE_ELEMENTS = 1024;

// ---- Composition reference graph (P0-A1; see §9.4): guard self-loops / chain cycles / deep nesting ----
constexpr uint32_t MAX_COMPOSITION_REF_DEPTH = 32;

// ---- Inflater global Layer budget (P1-3 v2.15): guard against decoder-legal but cumulative N^6 explosion ----
constexpr uint32_t MAX_INFLATED_LAYER_COUNT = 1000000;

// P0-2 v2.17 (security): pendingMask vector has its own cap separate from the
// per-Layer budget. A single Layer can hold N masks, so a malicious file could
// stage N × MAX_INFLATED_LAYER_COUNT pending entries to bypass code 606.
// The independent cap is sized as Layer total × constant factor.
constexpr uint32_t MAX_PENDING_MASKS = 262144;

// ---- Baker global structural cumulative caps (P0-4 v2.15): PAGX XML is attacker-controlled, so Baker
// needs its own pre-pass cumulative break before the Decoder's per-node MAX_*. ----
constexpr uint32_t BAKE_MAX_TOTAL_LAYERS = 100000;
constexpr uint32_t BAKE_MAX_TOTAL_VECTOR_ELEMENTS = 1000000;
// P2-1 v2.18: BAKE_MAX_TOTAL_COMPOSITIONS used to be an independent value (=1000) duplicating
// MAX_COMPOSITIONS. Aliased now; if the Decoder ever loosens MAX_COMPOSITIONS this can be re-split.
constexpr uint32_t BAKE_MAX_TOTAL_COMPOSITIONS = MAX_COMPOSITIONS;

// ---- Diagnostic accumulation caps (consumed by Phase 0 DiagnosticCollector) ----
constexpr uint32_t MAX_DIAGNOSTICS = 1000;  // shared by Decoder / Baker / Inflater
constexpr size_t MAX_DIAGNOSTIC_MESSAGE_BYTES =
    256;  // single message hard cap; warn / error truncate internally

}  // namespace pagx::pag::limits

// P2-4 v2.18: Path encoding format thresholds live in their own sub-namespace —
// they are format selection parameters, not safety hard limits.
namespace pagx::pag::path_format {
constexpr uint32_t QUANTIZATION_MIN_VERBS = 3;  // verbCount < this → fall back to format=0
constexpr int64_t QUANTIZATION_MAX_COORD =
    (1LL << 23);  // |coord| >= this → fall back to format=0 (int64 to avoid P0-14 UB)
constexpr uint8_t QUANTIZATION_DEFAULT_LOG2 = 4;  // 0.0625 px precision
}  // namespace pagx::pag::path_format
