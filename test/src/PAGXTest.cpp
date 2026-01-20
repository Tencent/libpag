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

#include <filesystem>
#include "pagx/layers/LayerBuilder.h"
#include "pagx/layers/TextLayouter.h"
#include "pagx/svg/SVGImporter.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/core/Surface.h"
#include "tgfx/svg/SVGDOM.h"
#include "tgfx/svg/TextShaper.h"
#include "utils/Baseline.h"
#include "utils/DevicePool.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

static std::vector<std::shared_ptr<Typeface>> GetFallbackTypefaces() {
  static std::vector<std::shared_ptr<Typeface>> typefaces;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    auto regularTypeface =
        Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
    if (regularTypeface) {
      typefaces.push_back(regularTypeface);
    }
    auto emojiTypeface =
        Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"));
    if (emojiTypeface) {
      typefaces.push_back(emojiTypeface);
    }
  }
  return typefaces;
}

static void SetupFallbackFonts() {
  pagx::TextLayouter::SetFallbackTypefaces(GetFallbackTypefaces());
}

static void SaveFile(const std::shared_ptr<Data>& data, const std::string& key) {
  if (!data) {
    return;
  }
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  auto file = fopen(outPath.c_str(), "wb");
  if (file) {
    fwrite(data->data(), 1, data->size(), file);
    fclose(file);
  }
}

/**
 * Test case: Convert all SVG files in apitest/SVG directory to PAGX format and render them
 */
PAG_TEST(PAGXTest, SVGToPAGXAll) {
  SetupFallbackFonts();

  std::string svgDir = ProjectPath::Absolute("resources/apitest/SVG");
  std::vector<std::string> svgFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }

  std::sort(svgFiles.begin(), svgFiles.end());

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // Create text shaper for SVG rendering
  auto textShaper = TextShaper::Make(GetFallbackTypefaces());

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    // Load original SVG with text shaper
    auto svgStream = Stream::MakeFromFile(svgPath);
    if (svgStream == nullptr) {
      continue;
    }
    auto svgDOM = SVGDOM::Make(*svgStream, textShaper);
    if (svgDOM == nullptr) {
      continue;
    }

    auto containerSize = svgDOM->getContainerSize();
    int width = static_cast<int>(containerSize.width);
    int height = static_cast<int>(containerSize.height);
    if (width <= 0 || height <= 0) {
      continue;
    }

    // Render original SVG
    auto svgSurface = Surface::Make(context, width, height);
    auto svgCanvas = svgSurface->getCanvas();
    svgDOM->render(svgCanvas);
    EXPECT_TRUE(Baseline::Compare(svgSurface, "PAGXTest/" + baseName + "_svg"));

    // Convert to PAGX
    auto pagxContent = pagx::SVGImporter::ImportFromFile(svgPath);
    if (pagxContent.empty()) {
      continue;
    }

    // Save PAGX file to output directory
    auto pagxData = Data::MakeWithCopy(pagxContent.data(), pagxContent.size());
    SaveFile(pagxData, "PAGXTest/" + baseName + ".pagx");

    auto pagxStream = Stream::MakeFromData(pagxData);
    if (pagxStream == nullptr) {
      continue;
    }

    auto content = pagx::LayerBuilder::FromStream(*pagxStream);
    if (content.root == nullptr) {
      continue;
    }

    // Render PAGX
    auto pagxSurface = Surface::Make(context, width, height);
    auto pagxCanvas = pagxSurface->getCanvas();
    content.root->draw(pagxCanvas);
    EXPECT_TRUE(Baseline::Compare(pagxSurface, "PAGXTest/" + baseName + "_pagx"));
  }

  device->unlock();
}

}  // namespace pag
