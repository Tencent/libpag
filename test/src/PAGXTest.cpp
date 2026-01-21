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
#include "pagx/LayerBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXSVGParser.h"
#include "pagx/PAGXModel.h"
#include "pagx/PathData.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
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
 * Test case: Convert all SVG files in apitest/SVG directory to PAGX format and render them.
 */
PAG_TEST(PAGXTest, SVGToPAGXAll) {
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

    // Convert to PAGX using new API
    pagx::LayerBuilder::Options options;
    options.fallbackTypefaces = GetFallbackTypefaces();
    auto content = pagx::LayerBuilder::FromSVGFile(svgPath, options);
    if (content.root == nullptr) {
      continue;
    }

    // Save PAGX file to output directory
    pagx::PAGXSVGParser::Options parserOptions;
    auto doc = pagx::PAGXSVGParser::Parse(svgPath, parserOptions);
    if (doc) {
      std::string xml = doc->toXML();
      auto pagxData = Data::MakeWithCopy(xml.data(), xml.size());
      SaveFile(pagxData, "PAGXTest/" + baseName + ".pagx");
    }

    // Render PAGX using DisplayList (required for mask to work).
    auto pagxSurface = Surface::Make(context, width, height);
    DisplayList displayList;
    displayList.root()->addChild(content.root);
    displayList.render(pagxSurface.get(), false);
    EXPECT_TRUE(Baseline::Compare(pagxSurface, "PAGXTest/" + baseName + "_pagx"));
  }

  device->unlock();
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
 * Test case: Strong-typed PAGX node creation
 */
PAG_TEST(PAGXTest, PAGXNodeBasic) {
  // Test Rectangle creation
  auto rect = std::make_unique<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 100;
  rect->size.height = 80;
  rect->roundness = 10;

  EXPECT_EQ(rect->type(), pagx::NodeType::Rectangle);
  EXPECT_STREQ(pagx::NodeTypeName(rect->type()), "Rectangle");
  EXPECT_FLOAT_EQ(rect->center.x, 50);
  EXPECT_FLOAT_EQ(rect->size.width, 100);

  // Test Path creation
  auto path = std::make_unique<pagx::Path>();
  path->data = pagx::PathData::FromSVGString("M0 0 L100 100");
  EXPECT_EQ(path->type(), pagx::NodeType::Path);
  EXPECT_GT(path->data.verbs().size(), 0u);

  // Test Fill creation
  auto fill = std::make_unique<pagx::Fill>();
  fill->color = "#FF0000";
  fill->alpha = 0.8f;
  EXPECT_EQ(fill->type(), pagx::NodeType::Fill);
  EXPECT_EQ(fill->color, "#FF0000");

  // Test Group with children
  auto group = std::make_unique<pagx::Group>();
  group->elements.push_back(std::move(rect));
  group->elements.push_back(std::move(fill));
  EXPECT_EQ(group->type(), pagx::NodeType::Group);
  EXPECT_EQ(group->elements.size(), 2u);
}

/**
 * Test case: PAGXDocument creation and XML export
 */
PAG_TEST(PAGXTest, PAGXDocumentXMLExport) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_EQ(doc->width, 400.0f);
  EXPECT_EQ(doc->height, 300.0f);

  // Create a layer with contents
  auto layer = std::make_unique<pagx::Layer>();
  layer->id = "layer1";
  layer->name = "Test Layer";

  // Add a group with rectangle and fill
  auto group = std::make_unique<pagx::Group>();

  auto rect = std::make_unique<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 80;
  rect->size.height = 60;

  auto fill = std::make_unique<pagx::Fill>();
  fill->color = "#0000FF";

  group->elements.push_back(std::move(rect));
  group->elements.push_back(std::move(fill));
  layer->contents.push_back(std::move(group));

  doc->layers.push_back(std::move(layer));

  // Export to XML
  std::string xml = doc->toXML();
  EXPECT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<pagx"), std::string::npos);
  EXPECT_NE(xml.find("width=\"400\""), std::string::npos);
  EXPECT_NE(xml.find("height=\"300\""), std::string::npos);

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

  auto layer = std::make_unique<pagx::Layer>();
  layer->name = "TestLayer";

  auto rect = std::make_unique<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 80;
  rect->size.height = 60;

  auto fill = std::make_unique<pagx::Fill>();
  fill->color = "#00FF00";

  layer->contents.push_back(std::move(rect));
  layer->contents.push_back(std::move(fill));
  doc1->layers.push_back(std::move(layer));

  // Export to XML
  std::string xml = doc1->toXML();
  EXPECT_FALSE(xml.empty());

  // Parse the XML back
  auto doc2 = pagx::PAGXDocument::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);

  // Verify the dimensions
  EXPECT_FLOAT_EQ(doc2->width, 200.0f);
  EXPECT_FLOAT_EQ(doc2->height, 150.0f);

  // Verify the structure
  EXPECT_GE(doc2->layers.size(), 1u);
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
  pagx::Rect r1 = {0.0f, 10.0f, 100.0f, 50.0f};
  EXPECT_FLOAT_EQ(r1.x, 0.0f);
  EXPECT_FLOAT_EQ(r1.y, 10.0f);
  EXPECT_FLOAT_EQ(r1.width, 100.0f);
  EXPECT_FLOAT_EQ(r1.height, 50.0f);
  EXPECT_FLOAT_EQ(r1.left(), 0.0f);
  EXPECT_FLOAT_EQ(r1.top(), 10.0f);
  EXPECT_FLOAT_EQ(r1.right(), 100.0f);
  EXPECT_FLOAT_EQ(r1.bottom(), 60.0f);

  // Test Rect::MakeLTRB
  auto r2 = pagx::Rect::MakeLTRB(10, 20, 110, 70);
  EXPECT_FLOAT_EQ(r2.x, 10.0f);
  EXPECT_FLOAT_EQ(r2.y, 20.0f);
  EXPECT_FLOAT_EQ(r2.width, 100.0f);
  EXPECT_FLOAT_EQ(r2.height, 50.0f);

  // Test Color
  pagx::Color c1 = {1.0f, 0.5f, 0.0f, 1.0f};
  EXPECT_FLOAT_EQ(c1.red, 1.0f);
  EXPECT_FLOAT_EQ(c1.green, 0.5f);
  EXPECT_FLOAT_EQ(c1.blue, 0.0f);
  EXPECT_FLOAT_EQ(c1.alpha, 1.0f);

  // Test Color parsing
  auto c2 = pagx::Color::Parse("#FF8000");
  EXPECT_FLOAT_EQ(c2.red, 1.0f);
  EXPECT_NEAR(c2.green, 0.5f, 0.01f);
  EXPECT_FLOAT_EQ(c2.blue, 0.0f);

  // Test Matrix (identity)
  pagx::Matrix m1 = {};
  EXPECT_TRUE(m1.isIdentity());
  EXPECT_FLOAT_EQ(m1.a, 1.0f);
  EXPECT_FLOAT_EQ(m1.b, 0.0f);
  EXPECT_FLOAT_EQ(m1.c, 0.0f);
  EXPECT_FLOAT_EQ(m1.d, 1.0f);
  EXPECT_FLOAT_EQ(m1.tx, 0.0f);
  EXPECT_FLOAT_EQ(m1.ty, 0.0f);
}

/**
 * Test case: Color source nodes
 */
PAG_TEST(PAGXTest, ColorSources) {
  // Test SolidColor
  auto solid = std::make_unique<pagx::SolidColor>();
  solid->color = pagx::Color::FromRGBA(1.0f, 0.0f, 0.0f, 1.0f);
  EXPECT_EQ(solid->type(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(solid->color.red, 1.0f);

  // Test LinearGradient
  auto linear = std::make_unique<pagx::LinearGradient>();
  linear->startPoint.x = 0;
  linear->startPoint.y = 0;
  linear->endPoint.x = 100;
  linear->endPoint.y = 0;

  pagx::ColorStop stop1;
  stop1.offset = 0;
  stop1.color = pagx::Color::FromRGBA(1.0f, 0.0f, 0.0f, 1.0f);

  pagx::ColorStop stop2;
  stop2.offset = 1;
  stop2.color = pagx::Color::FromRGBA(0.0f, 0.0f, 1.0f, 1.0f);

  linear->colorStops.push_back(stop1);
  linear->colorStops.push_back(stop2);

  EXPECT_EQ(linear->type(), pagx::NodeType::LinearGradient);
  EXPECT_EQ(linear->colorStops.size(), 2u);

  // Test RadialGradient
  auto radial = std::make_unique<pagx::RadialGradient>();
  radial->center.x = 50;
  radial->center.y = 50;
  radial->radius = 50;
  radial->colorStops = linear->colorStops;

  EXPECT_EQ(radial->type(), pagx::NodeType::RadialGradient);
}

/**
 * Test case: Layer node with styles and filters
 */
PAG_TEST(PAGXTest, LayerStylesFilters) {
  auto layer = std::make_unique<pagx::Layer>();
  layer->name = "StyledLayer";
  layer->alpha = 0.8f;
  layer->blendMode = pagx::BlendMode::Multiply;

  // Add drop shadow style
  auto dropShadow = std::make_unique<pagx::DropShadowStyle>();
  dropShadow->offsetX = 5;
  dropShadow->offsetY = 5;
  dropShadow->blurrinessX = 10;
  dropShadow->blurrinessY = 10;
  dropShadow->color = pagx::Color::FromRGBA(0, 0, 0, 0.5f);
  layer->styles.push_back(std::move(dropShadow));

  // Add blur filter
  auto blur = std::make_unique<pagx::BlurFilter>();
  blur->blurrinessX = 5;
  blur->blurrinessY = 5;
  layer->filters.push_back(std::move(blur));

  EXPECT_EQ(layer->styles.size(), 1u);
  EXPECT_EQ(layer->filters.size(), 1u);
  EXPECT_EQ(layer->styles[0]->type(), pagx::NodeType::DropShadowStyle);
  EXPECT_EQ(layer->filters[0]->type(), pagx::NodeType::BlurFilter);
}

}  // namespace pag
