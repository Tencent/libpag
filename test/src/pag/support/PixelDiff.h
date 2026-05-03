// Copyright (C) 2026 Tencent. All rights reserved.
//
// Pixel-space diagnostic helpers for Phase 12's §18.7bis RenderCrossCheck.
// Keeps PSNR / histogram logic out of the test body so both CLI regressions
// and interactive debugging (`PAG_RENDER_DIFF=1` env) can share one
// implementation.
//
// Stays tgfx-only at the interface — deliberately does NOT take a PAGX /
// PAGDocument so the helper is usable from any renderer path.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tgfx {
class Surface;
}  // namespace tgfx

namespace pagx::test {

// Reads a surface into a linear byte buffer (4 bytes/pixel, native tgfx
// ColorType / premult). Empty vector on any failure path (nullptr
// surface, readback fails) so callers can treat "nothing to diff" and
// "diff has no bytes" identically without enumerating failure modes.
std::vector<uint8_t> ReadSurfacePixels(const std::shared_ptr<tgfx::Surface>& surface);

// PSNR = 20 * log10(255 / sqrt(MSE)). Returns +infinity when pixelsA ==
// pixelsB (MSE = 0) — callers asserting `EXPECT_GE(psnr, 30.0)` handle
// infinity correctly.
//
// Preconditions: pixelsA.size() == pixelsB.size() == width*height*4. On
// any precondition violation the helper returns -infinity so gtest
// reports an out-of-bounds PSNR rather than silently passing.
double ComputePSNR(const std::vector<uint8_t>& pixelsA, const std::vector<uint8_t>& pixelsB,
                   int width, int height);

// Writes a channel-wise diff histogram to test/out/PixelDiff/{sampleName}_
// histogram.txt. Format is human-readable so developers can grep inline
// from a CI log:
//
//   === {sampleName} diff histogram (4 channels × 5 buckets) ===
//   channel R: 0=14523  1-3=102  4-10=15  11-50=0  >50=0
//   channel G: ...
//
// Best-effort: silently returns on any file-system error (tests shouldn't
// crash because the output dir isn't writable).
void DumpPixelDiffHistogram(const std::vector<uint8_t>& pixelsA,
                            const std::vector<uint8_t>& pixelsB, const std::string& sampleName);

}  // namespace pagx::test
