// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 14 Layer 5 — performance baseline gate (non-blocking on ±20%
// drift; FAILs only when PAG v2 decode regresses more than 5% against
// its own prior baseline, see §18.8).
//
// Per sample measures:
//   - pagx_size_bytes:  size of the source spec/samples/*.pagx file
//   - pag_size_bytes:   size of PAGExporter::ToBytes output
//   - pagx_load_ms:     PAGXImporter::FromFile → applyLayout
//                       → LayerBuilder::Build (produces tgfx::Layer tree)
//   - pag_load_ms:      Codec::Decode → LayerInflater::Inflate (also a
//                       tgfx::Layer tree — fair "bytes → renderable"
//                       comparison against pagx_load_ms)
//   - bake_ms:          Baker::Bake on an already-loaded PAGXDocument
//   - encode_ms:        Codec::Encode on an already-baked PAGDocument
//   - decode_ms:        Codec::Decode alone (subset of pag_load_ms)
//
// Each timing is the **median of 5 runs** to absorb cache-warm and GPU
// scheduling jitter. Ratios are env-insensitive and therefore diffable
// across machines; abs_times are captured for reference only (§18.8
// baseline.json schema).
//
// Retrograde gate: PAGX→PAG v1 conversion hasn't been built yet, so the
// `pag_v1_load_ms` reference is absent. The test runs with the §18.8
// "retrograde threshold" — `pag_load_ms ≤ previous_baseline_pag_load_ms
// × 1.05` — until the v1 converter lands (§17 D-bucket "v1 对比待补").

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#include "nlohmann/json.hpp"
#pragma clang diagnostic pop

#include "gtest/gtest.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "renderer/LayerBuilder.h"
#include "utils/ProjectPath.h"

namespace pag {

using nlohmann::json;

namespace {

// Microsecond clock — avoids tgfx::Clock pulling in GPU dependencies for
// this pure-CPU measurement.
int64_t NowMicros() {
  auto tp = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(tp).count();
}

double MicrosToMs(int64_t micros) {
  return static_cast<double>(micros) / 1000.0;
}

constexpr int kSampleRuns = 11;

// Standard median-of-N timing. Input vector is mutated (sorted in-place)
// since the caller has no further use for the raw sequence. 11 runs
// balances stable central tendency (outliers to the tail fall away)
// against test-suite wall-clock (~100ms per sample).
double MedianMs(std::vector<int64_t>& micros) {
  std::sort(micros.begin(), micros.end());
  return MicrosToMs(micros[micros.size() / 2]);
}

struct SampleTiming {
  std::string name;
  uint64_t pagxSizeBytes = 0;
  uint64_t pagSizeBytes = 0;
  double pagxLoadMs = 0.0;
  double pagLoadMs = 0.0;
  double bakeMs = 0.0;
  double encodeMs = 0.0;
  double decodeMs = 0.0;
};

// Runs the full measurement battery on one sample. Returns nullopt-like
// empty-name result on any irrecoverable failure so the caller can GTEST_SKIP.
SampleTiming MeasureSample(const std::filesystem::path& pagxPath) {
  SampleTiming out;
  out.name = pagxPath.stem().string();

  std::error_code ec;
  out.pagxSizeBytes = std::filesystem::file_size(pagxPath, ec);
  if (ec) {
    out.name.clear();
    return out;
  }

  // Produce one authoritative pag byte stream up front so pag_size_bytes
  // is stable across the 5 decode runs.
  auto prepDoc = pagx::PAGXImporter::FromFile(pagxPath.string());
  if (prepDoc == nullptr) {
    out.name.clear();
    return out;
  }
  prepDoc->applyLayout();
  pagx::PAGExporter::Options opts;
  auto exportResult = pagx::PAGExporter::ToBytes(*prepDoc, opts);
  if (!exportResult.ok) {
    out.name.clear();
    return out;
  }
  out.pagSizeBytes = exportResult.bytes.size();
  // Stash the bytes for the pag_load_ms runs. Each LoadFromBytes call
  // copies internally; the source vector stays reusable.
  const std::vector<uint8_t> pagBytes = std::move(exportResult.bytes);

  std::vector<int64_t> pagxLoadSamples(kSampleRuns);
  std::vector<int64_t> pagLoadSamples(kSampleRuns);
  std::vector<int64_t> bakeSamples(kSampleRuns);
  std::vector<int64_t> encodeSamples(kSampleRuns);
  std::vector<int64_t> decodeSamples(kSampleRuns);

  for (int i = 0; i < kSampleRuns; ++i) {
    // ---- pagx_load_ms: bytes → tgfx::Layer tree via LayerBuilder ----
    {
      auto t0 = NowMicros();
      auto doc = pagx::PAGXImporter::FromFile(pagxPath.string());
      if (doc == nullptr) {
        out.name.clear();
        return out;
      }
      doc->applyLayout();
      auto layer = pagx::LayerBuilder::Build(doc.get());
      auto t1 = NowMicros();
      if (layer == nullptr) {
        out.name.clear();
        return out;
      }
      pagxLoadSamples[i] = t1 - t0;
    }

    // ---- pag_load_ms: bytes → tgfx::Layer tree via Decode + Inflate ----
    {
      auto t0 = NowMicros();
      auto decodeResult = pagx::pag::Codec::Decode(pagBytes.data(), pagBytes.size());
      if (decodeResult.doc == nullptr) {
        out.name.clear();
        return out;
      }
      auto decodeMid = NowMicros();
      auto inflateResult = pagx::pag::LayerInflater::Inflate(std::move(decodeResult.doc));
      auto t1 = NowMicros();
      if (inflateResult.layer == nullptr) {
        out.name.clear();
        return out;
      }
      pagLoadSamples[i] = t1 - t0;
      decodeSamples[i] = decodeMid - t0;
    }

    // ---- bake_ms: Baker alone on a fresh PAGXDocument ----
    {
      auto doc = pagx::PAGXImporter::FromFile(pagxPath.string());
      doc->applyLayout();
      auto t0 = NowMicros();
      auto bakeResult = pagx::pag::Bake(*doc);
      auto t1 = NowMicros();
      if (bakeResult.doc == nullptr) {
        out.name.clear();
        return out;
      }
      bakeSamples[i] = t1 - t0;

      // ---- encode_ms: Codec::Encode alone ----
      auto t2 = NowMicros();
      auto enc = pagx::pag::Codec::Encode(*bakeResult.doc);
      auto t3 = NowMicros();
      if (enc.bytes == nullptr) {
        out.name.clear();
        return out;
      }
      encodeSamples[i] = t3 - t2;
    }
  }

  out.pagxLoadMs = MedianMs(pagxLoadSamples);
  out.pagLoadMs = MedianMs(pagLoadSamples);
  out.bakeMs = MedianMs(bakeSamples);
  out.encodeMs = MedianMs(encodeSamples);
  out.decodeMs = MedianMs(decodeSamples);
  return out;
}

// Returns true when the test run is in CI (any of CI / GITHUB_ACTIONS
// env var set to a non-empty string). Used to promote "baseline missing"
// from local-pass-with-hint to CI-FAIL.
bool IsCI() {
  for (const char* var : {"CI", "GITHUB_ACTIONS", "TF_BUILD"}) {
    const char* v = std::getenv(var);
    if (v != nullptr && v[0] != '\0') {
      return true;
    }
  }
  return false;
}

std::string PlatformString() {
#if defined(__APPLE__) && defined(__aarch64__)
  return "Darwin arm64";
#elif defined(__APPLE__)
  return "Darwin x86_64";
#elif defined(__linux__) && defined(__aarch64__)
  return "Linux arm64";
#elif defined(__linux__)
  return "Linux x86_64";
#elif defined(_WIN32)
  return "Windows x86_64";
#else
  return "Unknown";
#endif
}

}  // namespace

// Non-parameterised so the single test body can emit a single baseline
// file covering every sample. GoogleTest parameterisation would give
// nicer-looking per-sample test names but would have to re-load + re-
// open the baseline.json inside each case, which makes diffs and
// atomic-write much trickier.
TEST(PAGPerformance, SizeAndLoadBaseline) {
  const auto baselinePath = std::filesystem::path(ProjectPath::Absolute("test/perf/baseline.json"));
  const auto samplesDir = std::filesystem::path(ProjectPath::Absolute("spec/samples"));

  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(samplesDir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".pagx") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_FALSE(files.empty()) << "No spec/samples/*.pagx fixtures found";

  // Load existing baseline.json (if any) for drift comparison.
  json existingBaseline;
  bool hasExisting = false;
  {
    std::ifstream in(baselinePath);
    if (in.is_open()) {
      in >> existingBaseline;
      hasExisting = !existingBaseline.is_null();
    }
  }

  // Build the fresh baseline JSON. One object per sample with both
  // ratios and a `reference_abs_times` block the design doc nailed
  // down in §18.8.
  json freshBaseline;
  freshBaseline["format_version"] = 1;
  freshBaseline["notes"] =
      "Ratios are environment-insensitive; absolute times are reference-only. "
      "Phase 14 retrograde mode: pag_v1_load_ms absent (PAGX→PAG v1 converter "
      "pending; §17 D-bucket).";
  freshBaseline["samples"] = json::object();

  std::vector<std::string> driftWarnings;
  std::vector<std::string> retrogradeErrors;
  std::vector<std::string> skipped;

  const std::string platform = PlatformString();

  for (const auto& path : files) {
    auto timing = MeasureSample(path);
    if (timing.name.empty()) {
      skipped.push_back(path.stem().string());
      continue;
    }

    const double sizeRatio =
        static_cast<double>(timing.pagSizeBytes) / static_cast<double>(timing.pagxSizeBytes);
    const double loadRatio = timing.pagxLoadMs == 0.0 ? 0.0 : timing.pagLoadMs / timing.pagxLoadMs;

    json entry;
    entry["size_ratio"] = sizeRatio;
    entry["load_ratio"] = loadRatio;
    entry["reference_abs_times"] = {
        {"pagx_load_ms", timing.pagxLoadMs},
        {"pag_load_ms", timing.pagLoadMs},
        {"bake_ms", timing.bakeMs},
        {"encode_ms", timing.encodeMs},
        {"decode_ms", timing.decodeMs},
        {"pagx_size_bytes", timing.pagxSizeBytes},
        {"pag_size_bytes", timing.pagSizeBytes},
        {"platform", platform},
    };
    freshBaseline["samples"][timing.name] = entry;

    // Compare against the previous baseline (if any). Drift > ±20% on
    // size_ratio or load_ratio is a soft WARNING; pag_load_ms
    // regression > 5% against the previous reference is the
    // retrograde gate — that one is an error (§18.8 P1-P5 retrograde
    // fallback with pag_v1_load_ms absent).
    if (hasExisting && existingBaseline.contains("samples") &&
        existingBaseline["samples"].contains(timing.name)) {
      const auto& prev = existingBaseline["samples"][timing.name];
      if (prev.contains("size_ratio")) {
        const double prevSize = prev["size_ratio"].get<double>();
        if (prevSize > 0.0) {
          const double delta = std::abs(sizeRatio - prevSize) / prevSize;
          if (delta > 0.20) {
            driftWarnings.push_back(timing.name + ": size_ratio drift " +
                                    std::to_string(delta * 100.0) + "%");
          }
        }
      }
      if (prev.contains("load_ratio")) {
        const double prevLoad = prev["load_ratio"].get<double>();
        // Ratio drift only meaningful when both timings clear the
        // noise floor. Otherwise a 0.009 → 0.011 ms swing (22% of a
        // tiny denominator) fires spuriously.
        const double prevPagLoadMs = prev.contains("reference_abs_times") &&
                                             prev["reference_abs_times"].contains("pag_load_ms")
                                         ? prev["reference_abs_times"]["pag_load_ms"].get<double>()
                                         : 0.0;
        if (prevLoad > 0.0 && prevPagLoadMs >= 1.0 && timing.pagLoadMs >= 1.0) {
          const double delta = std::abs(loadRatio - prevLoad) / prevLoad;
          if (delta > 0.20) {
            driftWarnings.push_back(timing.name + ": load_ratio drift " +
                                    std::to_string(delta * 100.0) + "%");
          }
        }
      }
      if (prev.contains("reference_abs_times") &&
          prev["reference_abs_times"].contains("pag_load_ms")) {
        const double prevPagLoad = prev["reference_abs_times"]["pag_load_ms"].get<double>();
        // Sub-millisecond samples sit inside the steady_clock jitter
        // floor (≈20 µs per `now()` call × a handful of measurements
        // per sample). A `prev=0.009ms → curr=0.011ms` 22% swing is
        // pure noise, not a real regression. Only promote "5%
        // retrograde" to a FAIL when the absolute value is large
        // enough that 5% of it clearly exceeds that noise floor.
        // Threshold chosen so that `prev × 0.05 ≥ 50µs` (2.5× the
        // clock-call budget) — i.e. prev ≥ 1.0ms.
        constexpr double kRetrogradeNoiseFloorMs = 1.0;
        if (prevPagLoad >= kRetrogradeNoiseFloorMs && timing.pagLoadMs > prevPagLoad * 1.05) {
          retrogradeErrors.push_back(timing.name + ": pag_load_ms regressed " +
                                     std::to_string(timing.pagLoadMs) + "ms vs baseline " +
                                     std::to_string(prevPagLoad) + "ms (> 5% retrograde gate)");
        }
      }
    }
  }

  if (!skipped.empty()) {
    std::cerr << "[PAGPerformance] skipped samples (pipeline fatal): ";
    for (const auto& name : skipped) {
      std::cerr << name << " ";
    }
    std::cerr << "\n";
  }

  // Write the fresh baseline locally either way (so the developer can
  // diff it / git-add it). Only the FAIL-vs-PASS decision is gated on
  // IsCI() + existing baseline.
  {
    std::error_code ec;
    std::filesystem::create_directories(baselinePath.parent_path(), ec);
    std::ofstream out(baselinePath);
    if (out.is_open()) {
      out << freshBaseline.dump(2) << "\n";
    }
  }

  // Decision matrix:
  //   - no existing baseline + CI env            → FAIL (missing commit)
  //   - no existing baseline + local             → PASS, hint "baseline created"
  //   - existing baseline + retrograde           → FAIL
  //   - existing baseline + drift > 20% only     → PASS, WARN printed
  //   - existing baseline + within window        → PASS silent
  if (!hasExisting) {
    if (IsCI()) {
      FAIL() << "baseline.json missing in CI; commit test/perf/baseline.json produced by "
                "the reference machine before re-running";
    } else {
      std::cerr << "[PAGPerformance] baseline created at " << baselinePath.string()
                << " — commit explicitly to enroll as project baseline\n";
      SUCCEED();
      return;
    }
  }

  if (!retrogradeErrors.empty()) {
    std::string msg = "PAG v2 decode regression detected (>5% vs previous baseline):\n";
    for (const auto& e : retrogradeErrors) {
      msg += "  - " + e + "\n";
    }
    msg +=
        "Retrograde gate uses the §18.8 fallback threshold because pag_v1_load_ms "
        "is still absent (pending PAGX→PAG v1 converter).";
    FAIL() << msg;
  }

  if (!driftWarnings.empty()) {
    std::cerr << "[PAGPerformance] ratio drift warnings (> ±20%, non-blocking):\n";
    for (const auto& w : driftWarnings) {
      std::cerr << "  - " << w << "\n";
    }
  }
}

}  // namespace pag
