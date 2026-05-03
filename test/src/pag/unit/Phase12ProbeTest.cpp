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
#include <vector>
#include "gtest/gtest.h"
#include "pagx/Diagnostic.h"
#include "pagx/PAGXImporter.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "utils/ProjectPath.h"

namespace pag {

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

  // Probe test always passes — it's informational. The sample list that
  // Phase 12.2 hard-codes is derived from this output manually.
  SUCCEED();
}

}  // namespace pag
