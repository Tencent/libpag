// Copyright (C) 2026 Tencent. All rights reserved.
//
// See SampleEnumerator.h for contract. Implementation uses std::filesystem
// because the project already builds with C++17 and the enumerator runs
// once per test binary start — portability of std::filesystem on the
// currently-supported targets (macOS 10.15+, Ubuntu 20.04+, MSVC 2019+)
// is a non-issue.

#include "SampleEnumerator.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "utils/ProjectPath.h"

namespace pagx::test {

namespace {

// Returns true if `path` contains either "<Text" or "<TextBox" anywhere in
// the file body. Case-sensitive; PAGX authors and pagx-cli both emit the
// canonical capitalised element names so this is accurate in practice.
bool FileContainsText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  const std::string content = buf.str();
  return content.find("<Text") != std::string::npos;
}

// Named comparator so we don't reach for a lambda — keeps §2.1 honoured.
bool SampleByName(const PAGXSample& a, const PAGXSample& b) {
  return a.name < b.name;
}

}  // namespace

std::vector<PAGXSample> EnumerateSpecSamples() {
  const auto dir = pag::ProjectPath::Absolute("spec/samples");
  std::vector<PAGXSample> out;
  std::error_code ec;
  std::filesystem::directory_iterator it(dir, ec);
  if (ec) {
    return out;
  }

  for (const auto& entry : it) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() != ".pagx") {
      continue;
    }
    PAGXSample sample;
    sample.name = entry.path().stem().string();
    sample.absolutePath = entry.path().string();
    sample.hasText = FileContainsText(entry.path());
    out.push_back(std::move(sample));
  }
  std::sort(out.begin(), out.end(), SampleByName);
  return out;
}

std::vector<PAGXSample> EnumerateFirstBatchSamples() {
  // Post-Phase-11.5: all 48 fixtures traverse Bake→Encode→Decode→Inflate
  // end-to-end, so the first-batch is the full enumeration today. If
  // future fixtures introduce non-deterministic Bake fatals, add a
  // knownBroken std::set here and filter before returning.
  return EnumerateSpecSamples();
}

}  // namespace pagx::test
