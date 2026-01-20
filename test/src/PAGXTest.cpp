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
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXNode.h"
#include "pagx/PAGXSVGParser.h"
#include "pagx/PAGXTypes.h"
#include "pagx/PathData.h"
#include "tgfx/core/Data.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

static void SaveFile(const std::shared_ptr<tgfx::Data>& data, const std::string& key) {
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
 * Test case: PathData SVG string parsing and round-trip conversion
 */
PAG_TEST(PAGXTest, PathDataSVGRoundTrip) {
  // Test basic path commands
  std::string pathStr = "M10 20 L30 40 H50 V60 C70 80 90 100 110 120 S130 140 150 160 "
                        "Q170 180 190 200 T210 220 A10 20 30 1 0 230 240 Z";

  auto pathData = pagx::PathData::FromSVGString(pathStr);
  EXPECT_GT(pathData.verbs().size(), 0u);
  EXPECT_GT(pathData.countPoints(), 0u);

  // Verify round-trip conversion
  std::string outputStr = pathData.toSVGString();
  EXPECT_FALSE(outputStr.empty());

  // Parse the output string and verify it produces the same structure
  auto pathData2 = pagx::PathData::FromSVGString(outputStr);
  EXPECT_EQ(pathData.verbs().size(), pathData2.verbs().size());
}

/**
 * Test case: PathData forEach iteration
 */
PAG_TEST(PAGXTest, PathDataForEach) {
  std::string pathStr = "M0 0 L100 0 L100 100 L0 100 Z";

  auto pathData = pagx::PathData::FromSVGString(pathStr);

  int verbCount = 0;
  pathData.forEach([&verbCount](pagx::PathVerb, const float*) { verbCount++; });

  EXPECT_EQ(verbCount, 5);  // M, L, L, L, Z
}

/**
 * Test case: PAGXNode basic operations
 */
PAG_TEST(PAGXTest, PAGXNodeBasic) {
  auto node = pagx::PAGXNode::Make(pagx::PAGXNodeType::Group);
  ASSERT_TRUE(node != nullptr);
  EXPECT_EQ(node->type(), pagx::PAGXNodeType::Group);
  EXPECT_STREQ(pagx::PAGXNodeTypeName(node->type()), "Group");
  EXPECT_EQ(node->childCount(), 0u);

  // Add child nodes
  auto path1 = pagx::PAGXNode::Make(pagx::PAGXNodeType::Path);
  auto path2 = pagx::PAGXNode::Make(pagx::PAGXNodeType::Path);
  auto* path1Ptr = path1.get();
  node->appendChild(std::move(path1));
  node->appendChild(std::move(path2));
  EXPECT_EQ(node->childCount(), 2u);

  // Set attributes
  path1Ptr->setAttribute("d", "M0 0 L100 100");
  path1Ptr->setAttribute("fill", "#FF0000");

  EXPECT_TRUE(path1Ptr->hasAttribute("d"));
  EXPECT_TRUE(path1Ptr->hasAttribute("fill"));
  EXPECT_FALSE(path1Ptr->hasAttribute("stroke"));
  EXPECT_EQ(path1Ptr->getAttribute("fill"), "#FF0000");
}

/**
 * Test case: PAGXDocument creation and XML export
 */
PAG_TEST(PAGXTest, PAGXDocumentXMLExport) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_EQ(doc->width(), 400.0f);
  EXPECT_EQ(doc->height(), 300.0f);

  auto root = doc->root();
  ASSERT_TRUE(root != nullptr);
  EXPECT_EQ(root->type(), pagx::PAGXNodeType::Document);

  // Add a group with a path
  auto group = pagx::PAGXNode::Make(pagx::PAGXNodeType::Group);
  group->setId("testGroup");

  auto path = pagx::PAGXNode::Make(pagx::PAGXNodeType::Path);
  path->setAttribute("d", "M10 10 L90 10 L90 90 L10 90 Z");
  path->setAttribute("fill", "#0000FF");

  group->appendChild(std::move(path));
  root->appendChild(std::move(group));

  // Export to XML
  std::string xml = doc->toXML();
  EXPECT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<Document"), std::string::npos);
  EXPECT_NE(xml.find("width=\"400\""), std::string::npos);
  EXPECT_NE(xml.find("height=\"300\""), std::string::npos);
  EXPECT_NE(xml.find("<Group"), std::string::npos);
  EXPECT_NE(xml.find("<Path"), std::string::npos);
  EXPECT_NE(xml.find("fill=\"#0000FF\""), std::string::npos);

  // Save the XML for inspection
  auto xmlData = tgfx::Data::MakeWithCopy(xml.data(), xml.size());
  SaveFile(xmlData, "PAGXTest/document_export.pagx");
}

/**
 * Test case: PAGXDocument XML round-trip (create -> export -> parse)
 */
PAG_TEST(PAGXTest, PAGXDocumentRoundTrip) {
  // Create a document
  auto doc1 = pagx::PAGXDocument::Make(200, 150);
  ASSERT_TRUE(doc1 != nullptr);

  auto root1 = doc1->root();
  auto rect = pagx::PAGXNode::Make(pagx::PAGXNodeType::Rectangle);
  rect->setAttribute("x", "10");
  rect->setAttribute("y", "20");
  rect->setAttribute("width", "80");
  rect->setAttribute("height", "60");
  rect->setAttribute("fill", "#00FF00");
  root1->appendChild(std::move(rect));

  // Export to XML
  std::string xml = doc1->toXML();
  EXPECT_FALSE(xml.empty());

  // Parse the XML back
  auto doc2 = pagx::PAGXDocument::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);

  // Verify the dimensions
  EXPECT_FLOAT_EQ(doc2->width(), 200.0f);
  EXPECT_FLOAT_EQ(doc2->height(), 150.0f);

  // Verify the structure
  auto root2 = doc2->root();
  ASSERT_TRUE(root2 != nullptr);
  EXPECT_GE(root2->childCount(), 1u);
}

/**
 * Test case: Convert SVG files to PAGX format
 */
PAG_TEST(PAGXTest, SVGToPAGXConversion) {
  std::string svgDir = ProjectPath::Absolute("resources/apitest/SVG");

  if (!std::filesystem::exists(svgDir)) {
    return;  // Skip if directory doesn't exist
  }

  std::vector<std::string> svgFiles = {};
  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());

  pagx::PAGXSVGParser::Options options;

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    // Parse SVG to PAGX document
    auto doc = pagx::PAGXSVGParser::Parse(svgPath, options);
    if (doc == nullptr) {
      continue;  // Some SVGs may fail to parse
    }

    EXPECT_GT(doc->width(), 0.0f);
    EXPECT_GT(doc->height(), 0.0f);

    // Export to XML
    std::string xml = doc->toXML();
    EXPECT_FALSE(xml.empty());

    // Save PAGX file to output directory
    auto pagxData = tgfx::Data::MakeWithCopy(xml.data(), xml.size());
    SaveFile(pagxData, "PAGXTest/" + baseName + ".pagx");
  }
}

/**
 * Test case: PAGXTypes basic operations
 */
PAG_TEST(PAGXTest, PAGXTypesBasic) {
  // Test Point
  pagx::Point p1 = {10.0f, 20.0f};
  EXPECT_FLOAT_EQ(p1.x, 10.0f);
  EXPECT_FLOAT_EQ(p1.y, 20.0f);

  // Test Rect
  pagx::Rect r1 = {0.0f, 0.0f, 100.0f, 50.0f};
  EXPECT_FLOAT_EQ(r1.left, 0.0f);
  EXPECT_FLOAT_EQ(r1.top, 0.0f);
  EXPECT_FLOAT_EQ(r1.right, 100.0f);
  EXPECT_FLOAT_EQ(r1.bottom, 50.0f);

  // Test Color
  pagx::Color c1 = {1.0f, 0.5f, 0.0f, 1.0f};
  EXPECT_FLOAT_EQ(c1.red, 1.0f);
  EXPECT_FLOAT_EQ(c1.green, 0.5f);
  EXPECT_FLOAT_EQ(c1.blue, 0.0f);
  EXPECT_FLOAT_EQ(c1.alpha, 1.0f);

  // Test Matrix (identity)
  pagx::Matrix m1 = {};
  EXPECT_FLOAT_EQ(m1.scaleX, 1.0f);
  EXPECT_FLOAT_EQ(m1.skewX, 0.0f);
  EXPECT_FLOAT_EQ(m1.transX, 0.0f);
  EXPECT_FLOAT_EQ(m1.skewY, 0.0f);
  EXPECT_FLOAT_EQ(m1.scaleY, 1.0f);
  EXPECT_FLOAT_EQ(m1.transY, 0.0f);
}

}  // namespace pag
