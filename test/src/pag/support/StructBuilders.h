// Copyright (C) 2026 Tencent. All rights reserved.
//
// Synthetic PAGDocument fixtures used by Decoder / Inflater edge-case tests.
// Each helper produces a single-purpose corner-case tree: deep stacks,
// flat-but-wide trees, layers with many masks, composition cycles, etc.
// Phase 4-9 tests reach into these to drive boundary assertions without
// hand-rolling per-test trees.
#pragma once

#include <cstdint>
#include <memory>
#include "pagx/pag/PAGDocument.h"

namespace pagx::test {

// Builds a single composition whose root layer holds `depth-1` nested
// children — total layer chain of `depth` levels. Used to drive
// MAX_LAYER_DEPTH boundary tests (depth=64 should pass; depth=65 should be
// rejected by Baker / Inflater).
std::unique_ptr<pagx::pag::PAGDocument> MakeDeepLayerStack(uint32_t depth);

// Builds one composition whose root layer has `width` direct children — flat
// and wide tree. Used to exercise BAKE_MAX_TOTAL_LAYERS / per-composition
// breadth caps without piling on depth.
std::unique_ptr<pagx::pag::PAGDocument> MakeWideSiblingTree(uint32_t width);

// Builds one composition with two layers; the second carries `k` mask
// references targetting the first. Used to drive MAX_PENDING_MASKS in the
// Inflater (P0-2 v2.17). The mask chain is intentionally well-formed —
// callers wanting cycle scenarios should use MakeCompositionRefCycle below or
// flip child indices manually.
std::unique_ptr<pagx::pag::PAGDocument> MakeLayerWithKMasks(uint32_t k);

// Builds two compositions whose CompositionRef layers point at each other,
// producing a 2-cycle (comp[0] → comp[1] → comp[0]). Used by
// InflaterParity.CompositionSelfRef / CompositionRefTooDeep. Callers can
// extend the chain length by editing compositionIndex post-creation.
std::unique_ptr<pagx::pag::PAGDocument> MakeCompositionRefCycle();

}  // namespace pagx::test
