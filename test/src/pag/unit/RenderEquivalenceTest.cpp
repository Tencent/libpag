// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 12 Layer 4 — the hard PR gate. For every spec/samples/*.pagx
// fixture that passes Bake→Encode→Decode→Inflate end-to-end, render the
// inflated tree to an offscreen tgfx::Surface and compare against the
// webp baseline at test/baseline/PAGRenderTest_Render/{sample}_base.webp.
//
// **First-run expectation** (§19 P1-10 v2.18): the baseline directory
// `test/baseline/PAGRenderTest_Render/` does NOT exist yet. Every sample
// will initially `EXPECT_FALSE` against Baseline::Compare (i.e. the test
// fails). The declared acceptance procedure is:
//   1. This Phase 12.2 commit lands the test + first-run FAIL is expected
//   2. tech lead visually audits test/out/PAGRenderTest_Render/*.webp
//   3. user runs `/accept-baseline` to promote current outputs to baseline
//   4. re-run → all samples PASS
// AI does NOT execute step 3 (hard project rule: .codebuddy/rules/Test.md).

#include <string>
#include <utility>
#include "base/PAGTest.h"
#include "pag/support/RenderUtil.h"
#include "pag/support/SampleEnumerator.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "tgfx/layers/Layer.h"
#include "utils/Baseline.h"

namespace pag {

class PAGRenderEquivalenceTest : public PAGXTest,
                                 public ::testing::WithParamInterface<pagx::test::PAGXSample> {};

TEST_P(PAGRenderEquivalenceTest, Render_Baseline) {
  const auto& sample = GetParam();

  auto doc = pagx::PAGXImporter::FromFile(sample.absolutePath);
  ASSERT_NE(doc, nullptr) << "PAGXImporter failed for " << sample.name;
  // Use the fixture's FontConfig on both sides of the round-trip so the
  // Baker-side layout and the Inflater-side shape resolve typefaces against
  // the same registry. Without this, case B's shapedGlyphs typefaceKey
  // never matches (PAGXTest's FontConfig vs Inflater's default
  // pag::FontManager) and TextShapingHintMiss fires on every Text. The
  // layout still replays correctly, but observability noise muddies real
  // regressions.
  doc->applyLayout(&fontConfig());

  pagx::PAGExporter::Options opts;
  auto exportResult = pagx::PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(exportResult.ok) << "PAGExporter::ToBytes failed for " << sample.name;
  ASSERT_FALSE(exportResult.bytes.empty());

  auto decodeResult =
      pagx::pag::Codec::Decode(exportResult.bytes.data(), exportResult.bytes.size());
  ASSERT_NE(decodeResult.doc, nullptr) << "Codec::Decode failed for " << sample.name;

  pagx::pag::LayerInflater::Options inflateOpts;
  inflateOpts.fontProvider = fontProvider();
  auto inflateResult = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc), inflateOpts);
  ASSERT_NE(inflateResult.layer, nullptr) << "LayerInflater returned null for " << sample.name;

  const int canvasW = static_cast<int>(doc->width);
  const int canvasH = static_cast<int>(doc->height);
  ASSERT_GT(canvasW, 0);
  ASSERT_GT(canvasH, 0);

  auto surface = pagx::test::RenderLayerToSurface(context, inflateResult.layer, canvasW, canvasH);
  ASSERT_NE(surface, nullptr) << "RenderLayerToSurface returned null for " << sample.name;

  // Baseline::Compare writes the current output to test/out/... for
  // diff-then-accept workflow even when it fails — that's what turns the
  // expected first-run fails into a tractable review.
  EXPECT_TRUE(Baseline::Compare(surface, "PAGRenderTest_Render/" + sample.name))
      << "render baseline mismatch (first-run expected until /accept-baseline runs)";
}

// gtest TestParamInfo name-generator — uses a plain static function
// pointer to stay aligned with §2.1 "avoid lambdas" guidance.
static std::string SampleParamName(const ::testing::TestParamInfo<pagx::test::PAGXSample>& info) {
  return info.param.name;
}

// Sample source: full 48-fixture enumeration post-Phase-11.5. gtest emits
// one suite instance per sample so failures point at the exact file name.
INSTANTIATE_TEST_SUITE_P(AllSamples, PAGRenderEquivalenceTest,
                         ::testing::ValuesIn(pagx::test::EnumerateFirstBatchSamples()),
                         SampleParamName);

}  // namespace pag
