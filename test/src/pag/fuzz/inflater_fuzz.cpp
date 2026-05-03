// Copyright (C) 2026 Tencent. All rights reserved.
//
// libFuzzer harness targetting the PAG v2 Inflater specifically (Phase 12,
// §18.3ter, P1-19 v2.19).
//
// Rationale: the Decode fuzz harness (decode_fuzz.cpp) only reaches the
// Inflater when Decode returns a non-null document, but most fuzz
// mutations either corrupt the container framing (Decode fatal) or the
// semantic fields that Decode already range-checks. That means
// Inflater-specific state-space — Pass 2 mask resolution cycles,
// pendingMasks cap, layerBudget cap, image data reset lifecycle — is
// *shadowed* by Decode's earlier rejections.
//
// This harness intentionally uses the same Decode→Inflate chain (we
// can't raw-byte mutate a unique_ptr<PAGDocument> without destroying
// vtables). The leverage comes from *the corpus*: seed the fuzzer with
// StructBuilders-produced bytes that pass Decode but carry the gnarly
// structural edge cases (composition cycles, wide sibling trees, deep
// nesting, many masks). libFuzzer mutations from those seeds exercise
// the Inflater's fatal-defence logic far more directly than random
// bytes ever would.
//
// Seed corpus generation: see test/src/pag/fuzz/seed_generator.cpp (to
// be authored separately by whoever runs CI; it's one-shot so not a
// build target).

#include <cstddef>
#include <cstdint>
#include <utility>
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto decodeResult = pagx::pag::Codec::Decode(data, size);
  if (decodeResult.doc == nullptr) {
    // Decode rejected the seed — no Inflater work to do. libFuzzer's
    // coverage-guided mutation will steer toward Decode-acceptable
    // streams over time; returning 0 here doesn't bias against that.
    return 0;
  }
  auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc));
  (void)inflated;
  return 0;
}
