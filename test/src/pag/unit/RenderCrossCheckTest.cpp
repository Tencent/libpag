// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 12 Layer 4-extra (§18.7bis) — non-blocking PSNR sanity that
// Path A (direct LayerBuilder render) and Path B (PAGExporter →
// PAGLoader-style round-trip → Inflater render) don't semantically
// diverge. Fails below 30 dB; that's a deliberately loose bar because
// §18.7bis documents three sources of legitimate sub-byte drift
// (Color u8 quantisation, Path 0.0625px, GPU scheduling).
//
// Why still worth running:
//   • Catches Baker/Inflater mapping bugs that Render_Baseline passes
//     (because the baseline itself happens to have been captured from
//     the same buggy Path B output)
//   • Provides cross-path evidence on first-run — lets tech lead focus
//     `/accept-baseline` auditing on samples where both PathA and PathB
//     already agree
//
// Non-blocking: when PSNR < 30 dB, still ADD_FAILURE but also dumps a
// per-channel histogram for post-mortem. Phase 12 CI is expected to
// treat these failures as soft signals (advisory / warnings only) once
// the workflow yaml lands with the exclusion bucket.

#include <cmath>
#include <string>
#include <utility>
#include "base/PAGTest.h"
#include "pag/support/PixelDiff.h"
#include "pag/support/RenderUtil.h"
#include "pag/support/SampleEnumerator.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pag {

class PAGRenderCrossCheck : public PAGXTest,
                            public ::testing::WithParamInterface<pagx::test::PAGXSample> {};

TEST_P(PAGRenderCrossCheck, PathA_vs_PathB) {
  const auto& sample = GetParam();

  auto doc = pagx::PAGXImporter::FromFile(sample.absolutePath);
  ASSERT_NE(doc, nullptr) << "PAGXImporter failed for " << sample.name;
  doc->applyLayout();

  const int canvasW = static_cast<int>(doc->width);
  const int canvasH = static_cast<int>(doc->height);
  ASSERT_GT(canvasW, 0);
  ASSERT_GT(canvasH, 0);

  // Path A: LayerBuilder direct-build.
  auto rootA = pagx::LayerBuilder::Build(doc.get());
  ASSERT_NE(rootA, nullptr) << "LayerBuilder::Build failed for " << sample.name;
  auto surfaceA = pagx::test::RenderLayerToSurface(context, rootA, canvasW, canvasH);
  ASSERT_NE(surfaceA, nullptr);

  // Path B: PAGX → PAGExporter::ToBytes → Codec::Decode → Inflater.
  pagx::PAGExporter::Options opts;
  auto exportResult = pagx::PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(exportResult.ok) << "ToBytes failed for " << sample.name;
  auto decodeResult =
      pagx::pag::Codec::Decode(exportResult.bytes.data(), exportResult.bytes.size());
  ASSERT_NE(decodeResult.doc, nullptr);
  auto inflateResult = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc));
  ASSERT_NE(inflateResult.layer, nullptr);
  auto surfaceB = pagx::test::RenderLayerToSurface(context, inflateResult.layer, canvasW, canvasH);
  ASSERT_NE(surfaceB, nullptr);

  const auto pixelsA = pagx::test::ReadSurfacePixels(surfaceA);
  const auto pixelsB = pagx::test::ReadSurfacePixels(surfaceB);
  ASSERT_FALSE(pixelsA.empty());
  ASSERT_FALSE(pixelsB.empty());
  ASSERT_EQ(pixelsA.size(), pixelsB.size()) << "pixel buffer size drift for " << sample.name;

  const double psnr = pagx::test::ComputePSNR(pixelsA, pixelsB, canvasW, canvasH);
  // PSNR of +infinity = bit-perfect match; still >= 30. We handle infinity
  // explicitly so gtest message is readable on the happy path.
  const bool infinite = std::isinf(psnr) && psnr > 0.0;
  if (psnr < 30.0) {
    pagx::test::DumpPixelDiffHistogram(pixelsA, pixelsB, sample.name);
  }
  EXPECT_TRUE(infinite || psnr >= 30.0)
      << "sample=" << sample.name << " psnr=" << psnr
      << " dB (< 30 dB warning threshold). See test/out/PixelDiff/" << sample.name
      << "_histogram.txt. Likely causes: (1) v2 compression-path bug, "
         "(2) Baker mapping miss, (3) new compression exceeds budget. "
         "Check the Render_Baseline for this sample first.";
}

static std::string CrossCheckParamName(
    const ::testing::TestParamInfo<pagx::test::PAGXSample>& info) {
  return info.param.name;
}

INSTANTIATE_TEST_SUITE_P(AllSamples, PAGRenderCrossCheck,
                         ::testing::ValuesIn(pagx::test::EnumerateFirstBatchSamples()),
                         CrossCheckParamName);

}  // namespace pag
