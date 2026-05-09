// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 12.0 probe: discover which spec/samples pagx files can traverse
// the full PAGX → Bake → Encode → Decode → Inflate pipeline without a
// fatal error. Its output informs the hard-coded "first-batch" sample
// list that Phase 12.2 RenderEquivalenceTest instantiates against.
//
// Intentionally DISABLED_ so it doesn't run on PR gates — invoke with
// --gtest_also_run_disabled_tests --gtest_filter="Phase12Probe.*"
// whenever the spec/samples/ folder gains new fixtures.

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "base/PAGTest.h"
#include "gtest/gtest.h"
#include "pag/support/RenderUtil.h"
#include "pagx/Diagnostic.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "pagx/pag/PAGDocument.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Typeface.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"

namespace pag {

namespace {
[[maybe_unused]] const char* LayerTypeName(pagx::pag::LayerType t) {
  switch (t) {
    case pagx::pag::LayerType::Layer:
      return "Layer";
    case pagx::pag::LayerType::CompositionRef:
      return "CompositionRef";
    case pagx::pag::LayerType::Vector:
      return "Vector";
    case pagx::pag::LayerType::Solid:
      return "Solid";
    case pagx::pag::LayerType::Image:
      return "Image";
    case pagx::pag::LayerType::Shape:
      return "Shape";
    case pagx::pag::LayerType::Text:
      return "Text";
    case pagx::pag::LayerType::Mesh:
      return "Mesh";
  }
  return "?";
}

void DumpRectangles(const std::vector<pagx::Layer*>& layers, int depth) {
  for (auto* layer : layers) {
    if (layer == nullptr) continue;
    for (auto* el : layer->contents) {
      if (el == nullptr) continue;
      if (el->nodeType() == pagx::NodeType::Rectangle) {
        auto* r = static_cast<const pagx::Rectangle*>(el);
        auto renderPos = r->renderPosition();
        auto renderSize = r->renderSize();
        std::cerr << std::string(depth * 2, ' ') << "  Rectangle raw position=(" << r->position.x
                  << "," << r->position.y << ") raw size=(" << r->size.width << ","
                  << r->size.height << ") render position=(" << renderPos.x << "," << renderPos.y
                  << ") render size=(" << renderSize.width << "," << renderSize.height << ")\n";
      }
    }
    DumpRectangles(layer->children, depth + 1);
  }
}
}  // namespace

TEST(Phase12Probe, DISABLED_AllSamplesStatus) {
  auto samplesDir = ProjectPath::Absolute("spec/samples");

  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(samplesDir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".pagx") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());

  size_t bakeFatal = 0;
  size_t encodeFatal = 0;
  size_t decodeFatal = 0;
  size_t inflateFatal = 0;
  size_t allOk = 0;

  std::cerr << "\n=== Phase 12.0 sample probe (" << files.size() << " samples) ===\n";
  std::cerr << "STATUS  BAKE_WARN  ENCODE_WARN  DECODE_WARN  SAMPLE\n";

  for (const auto& path : files) {
    const std::string name = path.stem().string();
    auto doc = pagx::PAGXImporter::FromFile(path.string());
    if (doc == nullptr) {
      std::cerr << "IMPORT_FAIL                                   " << name << "\n";
      continue;
    }
    doc->applyLayout();

    auto bakeResult = pagx::pag::Bake(*doc);
    if (bakeResult.doc == nullptr) {
      ++bakeFatal;
      std::cerr << "BAKE_FATAL                                    " << name
                << " (errors=" << bakeResult.errors.size() << ")\n";
      continue;
    }
    const size_t bakeWarn = bakeResult.warnings.size();

    auto encodeResult = pagx::pag::Codec::Encode(*bakeResult.doc);
    if (encodeResult.bytes == nullptr || encodeResult.bytes->length() == 0) {
      ++encodeFatal;
      std::cerr << "ENCODE_FAIL  " << bakeWarn << "                             " << name << "\n";
      continue;
    }
    const size_t encodeWarn = encodeResult.warnings.size();

    auto decodeResult =
        pagx::pag::Codec::Decode(encodeResult.bytes->data(), encodeResult.bytes->length());
    if (decodeResult.doc == nullptr) {
      ++decodeFatal;
      std::cerr << "DECODE_FAIL  " << bakeWarn << "           " << encodeWarn << "               "
                << name << " (errors:";
      for (const auto& e : decodeResult.errors) {
        std::cerr << " " << static_cast<int>(e.code);
      }
      std::cerr << ")\n";
      continue;
    }
    const size_t decodeWarn = decodeResult.warnings.size();

    auto inflateResult = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc));
    if (inflateResult.layer == nullptr) {
      ++inflateFatal;
      std::cerr << "INFLATE_FAIL " << bakeWarn << "           " << encodeWarn << "           "
                << decodeWarn << "           " << name << "\n";
      continue;
    }

    ++allOk;
    std::cerr << "OK           " << bakeWarn << "           " << encodeWarn << "           "
              << decodeWarn << "           " << name << "\n";
  }

  std::cerr << "\n--- Summary ---\n";
  std::cerr << "all_ok:       " << allOk << "\n";
  std::cerr << "bake_fatal:   " << bakeFatal << "\n";
  std::cerr << "encode_fatal: " << encodeFatal << "\n";
  std::cerr << "decode_fatal: " << decodeFatal << "\n";
  std::cerr << "inflate_fatal:" << inflateFatal << "\n";
  std::cerr << "total:        " << files.size() << "\n\n";

  SUCCEED();
}

TEST(Phase12Probe, DISABLED_RectangleLayoutDump) {
  const auto path = ProjectPath::Absolute("spec/samples/masking.pagx");
  auto doc = pagx::PAGXImporter::FromFile(path);
  ASSERT_NE(doc, nullptr);
  doc->applyLayout();

  std::cerr << "\n=== rectangle.pagx applyLayout dump ===\n";
  DumpRectangles(doc->layers, 0);
  SUCCEED();
}

// Phase 11.6 follow-up: render Path A (LayerBuilder) output to
// test/out/Phase12Probe/{sample}_pathA.webp so we can eyeball why Path B
// differs. Writes via Baseline::Compare against a bogus key so the file
// ends up in the out/ dir (Baseline writes out-of-tree on any mismatch;
// the "baseline" key is guaranteed missing so we always get the dump).
class Phase12ProbeFixture : public PAGXTest {};

TEST_F(Phase12ProbeFixture, DISABLED_PathAVsPathB) {
  const auto path = ProjectPath::Absolute("spec/samples/masking.pagx");
  auto doc = pagx::PAGXImporter::FromFile(path);
  ASSERT_NE(doc, nullptr);
  doc->applyLayout();

  const int w = static_cast<int>(doc->width);
  const int h = static_cast<int>(doc->height);

  // Path A dump
  auto rootA = pagx::LayerBuilder::Build(doc.get());
  ASSERT_NE(rootA, nullptr);
  auto surfaceA = pagx::test::RenderLayerToSurface(context, rootA, w, h);
  ASSERT_NE(surfaceA, nullptr);
  (void)Baseline::Compare(surfaceA, "Phase12Probe/masking_pathA");

  // Path B dump
  pagx::PAGExporter::Options opts;
  auto exportResult = pagx::PAGExporter::ToBytes(*doc, opts);
  ASSERT_TRUE(exportResult.ok);
  auto dec = pagx::pag::Codec::Decode(exportResult.bytes.data(), exportResult.bytes.size());
  ASSERT_NE(dec.doc, nullptr);
  auto inf = pagx::pag::LayerInflater::Inflate(std::move(dec.doc));
  ASSERT_NE(inf.layer, nullptr);
  auto surfaceB = pagx::test::RenderLayerToSurface(context, inf.layer, w, h);
  ASSERT_NE(surfaceB, nullptr);
  (void)Baseline::Compare(surfaceB, "Phase12Probe/masking_pathB");
}

}  // namespace pag
