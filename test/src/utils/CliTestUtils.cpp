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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "utils/CliTestUtils.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include "cli/CommandRender.h"
#include "utils/ProjectPath.h"
#include "utils/TestDir.h"

namespace pag {

std::string TempDir() {
#ifdef GENERATE_BASELINE_IMAGES
  auto dir = TestDir::GetRoot() + "/baseline-out/PAGXCliTest";
#else
  auto dir = TestDir::GetRoot() + "/out/PAGXCliTest";
#endif
  std::filesystem::create_directories(dir);
  return dir;
}

std::string TestResourcePath(const std::string& name) {
  return ProjectPath::Absolute("resources/cli/" + name);
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

tgfx::Bitmap RenderAnimatedTimelinesAt(const std::string& timeSeconds, bool opaqueBackground) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  std::vector<std::string> args = {"render", "--time", timeSeconds};
  if (opaqueBackground) {
    args.insert(args.end(), {"--background", "#FFFFFF"});
  }
  args.emplace_back(inputPath);
  std::vector<char*> argv = {};
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  return pagx::cli::RenderToBitmap(static_cast<int>(argv.size()), argv.data());
}

}  // namespace pag
