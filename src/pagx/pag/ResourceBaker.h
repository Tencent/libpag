// Copyright (C) 2026 Tencent. All rights reserved.
//
// Resource pre-pass — walks every layer once and interns ImageAsset instances
// into PAGDocument::images. Subsequent baker submodules (VectorBaker,
// TextBaker, ...) reach into BakeContext::imageIndex* maps to translate raw
// PAGX node references into compact uint32 indices.
//
// Phase 16 (v2.20): font resources moved out of the .pag container entirely —
// ElementTextData carries fontFamily/fontStyle strings verbatim and the
// Inflater resolves typefaces through FontProvider at load time. The
// RegisterFont helper is therefore gone; only RegisterImage remains.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

// Intern an ImageAsset using the three-tier lookup chain on BakeContext:
//   1. byNode    — PAGX node identity (fastest path; same XML node visited twice)
//   2. byDataPtr — embedded tgfx::Data pointer (e.g. inline data URI shared across
//                  multiple PAGX nodes after import deduplication)
//   3. byKey     — semantic key string (URI / absolute path; survives node
//                  identity loss across separate PAGX documents merged in)
//
// `nodePtr` may be nullptr when the caller has no node identity to track
// (typical for synthetic test fixtures); the function still proceeds via the
// data-pointer / key tiers.
//
// Returns the asset's index inside doc.images. The caller is expected to have
// constructed the ImageAsset already; ownership transfers in on a fresh
// insertion and stays with the existing slot on a hit.
uint32_t RegisterImage(BakeContext& ctx, PAGDocument& doc, std::unique_ptr<ImageAsset> asset,
                       const void* nodePtr, std::string semanticKey);

}  // namespace pagx::pag
