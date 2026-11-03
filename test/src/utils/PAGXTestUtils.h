/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "cli/CommandVerify.h"
#include "gtest/gtest.h"
#include "utils/ProjectPath.h"

namespace pag {

// The shared fallback font fixtures for PAGX text layout: web has no system fonts, so without
// them the text measurement collapses to zero height and the zero-size diagnostics reject files.
// The first fallback covers most text; the later ones serve per-character fallback (emoji/RTL).
inline const std::vector<std::string>& GetFallbackFontPaths() {
  static const std::vector<std::string> paths = {
      ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"),
      ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"),
      ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"),
  };
  return paths;
}

// Invokes a `pagx::cli::Run*` function with a plain `std::vector<std::string>` argv, hiding the
// char* juggling every test would otherwise repeat.
inline int CallRun(int (*fn)(int, char*[]), std::vector<std::string> args) {
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  return fn(static_cast<int>(argv.size()), argv.data());
}

inline static void VerifyFile(const std::string& filePath, const std::string& key) {
  std::streambuf* oldErr = std::cerr.rdbuf();
  std::ostringstream verifyErr;
  std::cerr.rdbuf(verifyErr.rdbuf());
  // Serif coverage is appended on top of the shared list because HTML importer outputs may ask
  // for serif families.
  std::vector<std::string> args = {"verify", "--skip-render", "--skip-layout"};
  for (const auto& fontPath : GetFallbackFontPaths()) {
    args.insert(args.end(), {"--font", fontPath});
  }
  args.insert(
      args.end(),
      {"--font", ProjectPath::Absolute("resources/font/NotoSerifSC-Regular.otf"), filePath});
  auto verifyRet = CallRun(pagx::cli::RunVerify, std::move(args));
  std::cerr.rdbuf(oldErr);
  EXPECT_EQ(verifyRet, 0) << "pagx verify failed for " << key << ":\n" << verifyErr.str();
}

}  // namespace pag
