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

#include "cli/CommandRenderFrames.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace pagx::cli {

namespace {
struct RenderFramesOptions {
  std::string inputFile = {};
  std::string outputDir = "frames";
  float fps = 0.0f;
};

void PrintUsage() {
  std::cout
      << "Usage: pagx render-frames [options] <file.pagx>\n"
      << "\n"
      << "Renders every frame of the default animation to PNG files in an output directory.\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <dir>   Output directory (default: frames)\n"
      << "  --fps <float>        Frame rate in frames per second (default: animation frame rate)\n";
}

int ParseOptions(int argc, char* argv[], RenderFramesOptions* options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      PrintUsage();
      return -1;
    }
    if (arg == "-o" || arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "pagx render-frames: missing value for " << arg << "\n";
        return 1;
      }
      options->outputDir = argv[++i];
      continue;
    }
    if (arg == "--fps") {
      if (i + 1 >= argc) {
        std::cerr << "pagx render-frames: missing value for --fps\n";
        return 1;
      }
      options->fps = strtof(argv[++i], nullptr);
      if (options->fps <= 0.0f) {
        std::cerr << "pagx render-frames: invalid --fps value\n";
        return 1;
      }
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "pagx render-frames: unknown option '" << arg << "'\n";
      return 1;
    }
    options->inputFile = arg;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx render-frames: missing input file\n";
    PrintUsage();
    return 1;
  }
  return 0;
}
}  // namespace

int RunRenderFrames(int argc, char* argv[]) {
  RenderFramesOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = LoadDocument(options.inputFile, "pagx render-frames");
  if (document == nullptr) {
    return 1;
  }

  auto scene = PAGScene::Make(document);
  if (scene == nullptr) {
    std::cerr << "pagx render-frames: failed to build scene\n";
    return 1;
  }

  auto timeline = scene->getDefaultTimeline();
  if (timeline == nullptr || timeline->type() != pagx::TimelineType::Animation) {
    std::cerr << "pagx render-frames: no animation found in document\n";
    return 1;
  }
  auto anim = std::static_pointer_cast<pagx::PAGAnimation>(timeline);

  float fps = options.fps > 0.0f ? options.fps : anim->frameRate();
  if (fps <= 0.0f) {
    fps = 60.0f;
  }
  int64_t durationUs = anim->duration();
  int totalFrames = static_cast<int>(std::round(static_cast<double>(durationUs) / 1e6 * fps));

  int width = static_cast<int>(std::ceilf(document->width));
  int height = static_cast<int>(std::ceilf(document->height));
  if (width <= 0 || height <= 0) {
    std::cerr << "pagx render-frames: invalid document dimensions\n";
    return 1;
  }

  auto surface = pagx::PAGSurface::MakeOffscreen(width, height);
  if (surface == nullptr) {
    std::cerr << "pagx render-frames: failed to create surface\n";
    return 1;
  }

  std::error_code ec;
  std::filesystem::create_directories(options.outputDir, ec);
  if (ec) {
    std::cerr << "pagx render-frames: failed to create output directory '" << options.outputDir
              << "'\n";
    return 1;
  }

  int written = 0;
  for (int f = 0; f <= totalFrames; ++f) {
    int64_t us = static_cast<int64_t>(std::round(static_cast<double>(f) * 1e6 / fps));
    anim->setCurrentTime(us);
    anim->apply();
    scene->draw(surface);

    // Read the frame into a fresh bitmap and encode it within the same scope. A Pixmap locks the
    // bitmap's pixels, so read and encode must share one Pixmap instance rather than holding a
    // second lock (which would deadlock).
    tgfx::Bitmap bitmap(width, height, false, false);
    if (bitmap.isEmpty()) {
      std::cerr << "pagx render-frames: failed to allocate bitmap at frame " << f << "\n";
      return 1;
    }
    tgfx::Pixmap pixmap(bitmap);
    if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
      std::cerr << "pagx render-frames: failed to read pixels at frame " << f << "\n";
      return 1;
    }
    auto data = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
    if (data == nullptr) {
      std::cerr << "pagx render-frames: failed to encode frame " << f << "\n";
      return 1;
    }
    char name[32] = {};
    snprintf(name, sizeof(name), "frame_%03d.png", f);
    auto path = (std::filesystem::path(options.outputDir) / name).string();
    if (!WriteDataToFile(path, data)) {
      std::cerr << "pagx render-frames: failed to write '" << path << "'\n";
      return 1;
    }
    ++written;
  }

  std::cout << "pagx render-frames: wrote " << written << " frames to " << options.outputDir << " ("
            << width << "x" << height << ")\n";
  return 0;
}

}  // namespace pagx::cli
