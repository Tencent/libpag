// Copyright (C) 2026 Tencent. All rights reserved.
//
// libFuzzer harness for the PAG v2 decoder (Phase 12, §18.3ter Layer 6).
// Feeds arbitrary byte streams through Codec::Decode and — when Decode
// accepts them — through LayerInflater::Inflate. libFuzzer hooks
// ASAN/UBSAN so any OOB, UAF, integer overflow, or signed/unsigned
// misuse along the path shows up immediately.
//
// Build (local, only when PAG_BUILD_FUZZ=ON + clang with libFuzzer):
//
//   cmake -G Ninja -DPAG_BUILD_FUZZ=ON -DCMAKE_BUILD_TYPE=Debug \
//         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
//         -B cmake-build-fuzz
//   cmake --build cmake-build-fuzz --target PAGDecodeFuzz
//
// Short-run sanity (30 seconds, ~5k iterations) expected on AI workflow:
//   ./cmake-build-fuzz/PAGDecodeFuzz -runs=5000 -max_total_time=30 \
//        test/fuzz_corpus/decode_seeds
//
// Long-run CI (4 shards × 6 hours = 24 CPU·hours): see
// `.github/workflows/pagx-fuzz.yml`.

#include <cstddef>
#include <cstdint>
#include <utility>
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Intentional no-op on the return value: libFuzzer drives the harness
  // via sanitiser signals, not through the return code. The harness is
  // required to stay side-effect-free (no globals mutation, no heap
  // allocation outside the Decoder / Inflater's own) so one iteration
  // corrupting another cannot happen by construction.
  auto decodeResult = pagx::pag::Codec::Decode(data, size);
  if (decodeResult.doc != nullptr) {
    auto inflated = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc));
    (void)inflated;
  }
  return 0;
}
