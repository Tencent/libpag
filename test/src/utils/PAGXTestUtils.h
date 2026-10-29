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
#include <vector>
#include "cli/CommandVerify.h"
#include "gtest/gtest.h"

namespace pag {

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
  // Register the repo font fixtures as fallbacks: web has no system fonts, so without them the
  // text measurement collapses to zero height and the zero-size diagnostics reject the file.
  // The first fallback must cover most text; later ones serve per-character fallback (emoji/RTL).
  auto verifyRet = CallRun(pagx::cli::RunVerify,
                           {"verify", "--skip-render", "--skip-layout",
                            "--font", "resources/font/NotoSansSC-Regular.otf",
                            "--font", "resources/font/NotoColorEmoji.ttf",
                            "--font", "resources/font/NotoSansHebrew-Regular.ttf",
                            "--font", "resources/font/NotoSerifSC-Regular.otf",
                            filePath});
  std::cerr.rdbuf(oldErr);
  EXPECT_EQ(verifyRet, 0) << "pagx verify failed for " << key << ":\n" << verifyErr.str();
}

}  // namespace pag
