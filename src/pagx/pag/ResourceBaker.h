// Copyright (C) 2026 Tencent. All rights reserved.
//
// Resource pre-pass — walks every layer once and interns ImageAsset / FontAsset
// instances into PAGDocument::images / fonts. Subsequent baker submodules
// (VectorBaker, TextBaker, ...) reach into BakeContext::imageIndex* /
// fontIndex* maps to translate raw PAGX node references into compact uint32
// indices.
//
// Phase 2 deliverable scope: the deduplication primitives (RegisterImage /
// RegisterFont) plus the BakeContext map plumbing. The PAGX-tree-walking
// driver lands in Phase 3 alongside LayerBaker, since the walker depends on
// PAGX node types that Phase 3 introduces.
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

// Same three-tier intern path for fonts. semanticKey convention is
// "<system|embedded>\0<family>\0<style>" so identical fonts coming from
// different XML nodes still collapse to one slot.
uint32_t RegisterFont(BakeContext& ctx, PAGDocument& doc, std::unique_ptr<FontAsset> asset,
                      const void* nodePtr, std::string semanticKey);

}  // namespace pagx::pag
