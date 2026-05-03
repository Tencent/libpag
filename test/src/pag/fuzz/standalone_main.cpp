// Copyright (C) 2026 Tencent. All rights reserved.
//
// Standalone runner for PAGDecodeFuzz / PAGInflaterFuzz when the build
// environment lacks libFuzzer runtime (stock Apple clang 15+ on macOS
// drops libclang_rt.fuzzer_osx.a). Walks every file under the supplied
// seed directory, feeding each buffer to LLVMFuzzerTestOneInput exactly
// as libFuzzer would.
//
// Usage:
//   ./PAGDecodeFuzz  test/fuzz_corpus/decode_seeds
//   ./PAGInflaterFuzz test/fuzz_corpus/inflater_seeds
//
// Exit codes:
//   0 — every seed processed without ASAN/UBSAN trips
//   1 — IO error (seed dir missing / unreadable)
//
// Real libFuzzer coverage is obtained on CI via
// `.github/workflows/pagx-fuzz.yml`, which runs on a clang toolchain
// where `-fsanitize=fuzzer` is available. This runner is a local
// compile-through + sanity gate only.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <seed-dir>\n"
              << "(standalone fallback when libFuzzer runtime is unavailable)\n";
    return 1;
  }
  const std::filesystem::path seedDir = argv[1];
  std::error_code ec;
  if (!std::filesystem::is_directory(seedDir, ec)) {
    std::cerr << "seed dir does not exist: " << seedDir << "\n";
    return 1;
  }
  size_t count = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(seedDir, ec)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    std::ifstream in(entry.path(), std::ios::binary);
    if (!in.is_open()) {
      continue;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    (void)LLVMFuzzerTestOneInput(bytes.data(), bytes.size());
    ++count;
  }
  std::cout << "standalone runner processed " << count << " seeds in " << seedDir << "\n";
  return 0;
}
