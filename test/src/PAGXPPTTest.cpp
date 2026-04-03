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

#ifdef PAG_BUILD_PPT

#include <algorithm>
#include <filesystem>
#include <fstream>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PPTExporter.h"
#include "pagx/SVGImporter.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

/**
 * Read all SVG files from resources/svg, convert each to PAGX, then export to PPTX.
 */
PAGX_TEST(PAGXPPTTest, PPTExport_FromSVG) {
  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles;

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());
  ASSERT_FALSE(svgFiles.empty()) << "No SVG files found in resources/svg";

  auto outDir = ProjectPath::Absolute("test/out/PAGXPPTTest");
  if (!std::filesystem::exists(outDir)) {
    std::filesystem::create_directories(outDir);
  }

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      continue;
    }
    if (doc->width <= 0 || doc->height <= 0) {
      continue;
    }

    auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
    ASSERT_FALSE(pagxXml.empty()) << baseName << " PAGX export failed";

    auto pagxPath = outDir + "/" + baseName + ".pagx";
    {
      std::ofstream file(pagxPath, std::ios::binary);
      ASSERT_TRUE(file.good()) << "Failed to write " << pagxPath;
      file.write(pagxXml.data(), static_cast<std::streamsize>(pagxXml.size()));
    }

    auto reimported = pagx::PAGXImporter::FromFile(pagxPath);
    ASSERT_NE(reimported, nullptr) << baseName << " PAGX re-import failed";

    auto pptxPath = outDir + "/" + baseName + ".pptx";
    ASSERT_TRUE(pagx::PPTExporter::ToFile(*reimported, pptxPath))
        << baseName << " PPT export failed";
    EXPECT_TRUE(std::filesystem::exists(pptxPath));
    EXPECT_GT(std::filesystem::file_size(pptxPath), 0u) << baseName << " PPTX file is empty";
  }
}

}  // namespace pag

#endif
