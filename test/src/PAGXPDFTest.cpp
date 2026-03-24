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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PDFExporter.h"
#include "pagx/SVGImporter.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

static std::string SavePDFFile(const std::string& content, const std::string& key) {
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  if (file) {
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }
  return outPath;
}

/**
 * Read all SVG files from resources/svg, convert each to PAGX, then export to PDF.
 */
PAGX_TEST(PAGXPDFTest, SVGToPAGXToPDF) {
  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());
  ASSERT_FALSE(svgFiles.empty()) << "No SVG files found in resources/svg";

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      ADD_FAILURE() << "Failed to parse SVG: " << svgPath;
      continue;
    }
    if (doc->width <= 0 || doc->height <= 0) {
      continue;
    }

    auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
    ASSERT_FALSE(pagxXml.empty()) << baseName << " PAGX export failed";

    auto reloadedDoc = pagx::PAGXImporter::FromXML(pagxXml);
    ASSERT_NE(reloadedDoc, nullptr) << baseName << " PAGX re-import failed";

    auto pdfContent = pagx::PDFExporter::ToPDF(*reloadedDoc);
    ASSERT_FALSE(pdfContent.empty()) << baseName << " PDF export failed";
    EXPECT_GE(pdfContent.size(), 50u) << baseName << " PDF too small";
    EXPECT_EQ(pdfContent.substr(0, 5), "%PDF-") << baseName << " missing PDF header";
    EXPECT_NE(pdfContent.find("%%EOF"), std::string::npos) << baseName << " missing PDF trailer";

    SavePDFFile(pdfContent, "PAGXPDFTest/" + baseName + ".pdf");
  }
}

}  // namespace pag
