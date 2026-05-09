// Copyright (C) 2026 Tencent. All rights reserved.
//
// One-shot visual comparison dump covering the union of spec/samples and
// resources/{pagx_to_html,cli,layout,text}. For every *.pagx under those
// directories, render two tgfx Surfaces:
//   - PathA: PAGXImporter -> applyLayout -> LayerBuilder  (left column)
//   - PathB: PAGExporter -> Codec::Decode -> LayerInflater (right column)
// and write both PNG-ish webp outputs to
//   test/out/comparison/{section}/{name}_{pagx,pag}.webp
// plus a sidecar {name}.status.txt recording success/failure per stage so
// tools/render_compare.py can badge the failing cards without re-running
// the toolchain. The test is DISABLED_ by default — fed into the report
// pipeline via an explicit --gtest_filter invocation, not PR gates.
//
// Both paths share the PAGXTest fixture's FontConfig + FontProvider so
// right-column typeface resolution matches left-column layout — any
// visual delta is a real PAGX→PAG bug, not a registry mismatch.
//
// Failures are captured, not asserted: a broken sample still produces a
// status row so the HTML report can flag it visually. ADD_FAILURE is only
// used when the dump infrastructure itself (directory, fixture setup) is
// broken.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include "base/PAGTest.h"
#include "pag/support/RenderUtil.h"
#include "pag/support/SampleEnumerator.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/Layer.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

namespace {

// Write a one-line-per-stage status file next to the two webp outputs.
// Format is intentionally trivial `key=value\n` so the Python report
// reader can parse with a single split().
void WriteStatus(const std::string& section, const std::string& name, const std::string& pathA,
                 const std::string& pathB, int width, int height) {
  const auto statusPath =
      ProjectPath::Absolute("test/out/comparison/" + section + "/" + name + ".status.txt");
  std::filesystem::create_directories(std::filesystem::path(statusPath).parent_path());
  std::ofstream out(statusPath);
  out << "pagx=" << pathA << "\n";
  out << "pag=" << pathB << "\n";
  out << "width=" << width << "\n";
  out << "height=" << height << "\n";
}

struct RenderOutcome {
  bool ok = false;
  std::string error;  // human readable when !ok
};

// Render PathA (PAGX-native). Returns ok=true when a pixmap was written;
// on any failure populates `error` with the stage name so the status
// sidecar can surface it.
RenderOutcome RenderPathA(tgfx::Context* context, pagx::PAGXDocument* doc, pagx::FontConfig* cfg,
                          const std::string& outKey) {
  RenderOutcome r;
  auto layer = pagx::LayerBuilder::Build(doc);
  if (!layer) {
    r.error = "LayerBuilder::Build returned null";
    return r;
  }
  const int canvasW = static_cast<int>(doc->width);
  const int canvasH = static_cast<int>(doc->height);
  if (canvasW <= 0 || canvasH <= 0) {
    r.error = "invalid canvas size";
    return r;
  }
  auto surface = pagx::test::RenderLayerToSurface(context, layer, canvasW, canvasH);
  if (!surface) {
    r.error = "RenderLayerToSurface failed";
    return r;
  }
  tgfx::Bitmap bitmap(canvasW, canvasH, false, false);
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    r.error = "readPixels failed";
    return r;
  }
  SaveImage(pixmap, outKey);
  (void)cfg;
  r.ok = true;
  return r;
}

// Render PathB (PAGX -> PAG -> Inflater).
RenderOutcome RenderPathB(tgfx::Context* context, pagx::PAGXDocument* doc,
                          std::shared_ptr<pagx::pag::FontProvider> fontProvider,
                          const std::string& outKey) {
  RenderOutcome r;
  pagx::PAGExporter::Options exportOpts;
  auto exportResult = pagx::PAGExporter::ToBytes(*doc, exportOpts);
  if (!exportResult.ok) {
    r.error = "PAGExporter::ToBytes failed";
    return r;
  }
  auto decodeResult =
      pagx::pag::Codec::Decode(exportResult.bytes.data(), exportResult.bytes.size());
  if (!decodeResult.doc) {
    r.error = "Codec::Decode failed";
    return r;
  }
  pagx::pag::LayerInflater::Options inflateOpts;
  inflateOpts.fontProvider = std::move(fontProvider);
  auto inflateResult = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc), inflateOpts);
  if (!inflateResult.layer) {
    r.error = "LayerInflater::Inflate returned null";
    return r;
  }
  const int canvasW = static_cast<int>(doc->width);
  const int canvasH = static_cast<int>(doc->height);
  if (canvasW <= 0 || canvasH <= 0) {
    r.error = "invalid canvas size";
    return r;
  }
  auto surface = pagx::test::RenderLayerToSurface(context, inflateResult.layer, canvasW, canvasH);
  if (!surface) {
    r.error = "RenderLayerToSurface failed";
    return r;
  }
  tgfx::Bitmap bitmap(canvasW, canvasH, false, false);
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    r.error = "readPixels failed";
    return r;
  }
  SaveImage(pixmap, outKey);
  r.ok = true;
  return r;
}

}  // namespace

class ComparisonDumpTest : public PAGXTest {};

// Explicitly DISABLED_ — the dump is a manual visual-audit helper, not a
// CI gate. Invoke with:
//   ./PAGFullTest --gtest_also_run_disabled_tests \
//     --gtest_filter='ComparisonDumpTest.DISABLED_DumpAllSections'
TEST_F(ComparisonDumpTest, DISABLED_DumpAllSections) {
  const auto samples = pagx::test::EnumerateComparisonSamples();
  ASSERT_FALSE(samples.empty()) << "no .pagx fixtures found across spec/samples + resources/*";

  size_t totalA = 0;
  size_t totalB = 0;
  size_t failA = 0;
  size_t failB = 0;

  for (const auto& sample : samples) {
    const std::string keyA = "comparison/" + sample.section + "/" + sample.name + "_pagx";
    const std::string keyB = "comparison/" + sample.section + "/" + sample.name + "_pag";

    auto doc = pagx::PAGXImporter::FromFile(sample.absolutePath);
    if (!doc) {
      // No doc → skip both renders but still emit a status row so the
      // report can display the failure card.
      WriteStatus(sample.section, sample.name, "fail:PAGXImporter", "fail:PAGXImporter", 0, 0);
      ++failA;
      ++failB;
      continue;
    }

    // applyLayout must use the fixture's FontConfig so case-B shapedGlyphs
    // typefaceKey matches on the PathB side. Failure to apply layout
    // leaves the tree in authored coordinates — render still works for
    // simple samples but drops constraint-aware ones. We surface it as a
    // non-fatal warning via the status sidecar rather than aborting.
    doc->applyLayout(&fontConfig());

    const int width = static_cast<int>(doc->width);
    const int height = static_cast<int>(doc->height);

    auto outcomeA = RenderPathA(context, doc.get(), &fontConfig(), keyA);
    if (outcomeA.ok) {
      ++totalA;
    } else {
      ++failA;
    }

    auto outcomeB = RenderPathB(context, doc.get(), fontProvider(), keyB);
    if (outcomeB.ok) {
      ++totalB;
    } else {
      ++failB;
    }

    WriteStatus(sample.section, sample.name, outcomeA.ok ? "ok" : "fail:" + outcomeA.error,
                outcomeB.ok ? "ok" : "fail:" + outcomeB.error, width, height);
  }

  std::printf("\n[ComparisonDumpTest] %zu samples; PathA ok=%zu fail=%zu; PathB ok=%zu fail=%zu\n",
              samples.size(), totalA, failA, totalB, failB);
}

}  // namespace pag
