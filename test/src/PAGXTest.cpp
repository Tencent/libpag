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

#include <cmath>
#include <filesystem>
#include "pagx/LayerBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/PathData.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
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
  // Minimum canvas edge length for rendering. Small images will be scaled up for better visibility.
  constexpr int MinCanvasEdge = 400;

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

    // Convert to PAGX using new API
    pagx::LayerBuilder::Options options;
    options.fallbackTypefaces = GetFallbackTypefaces();
    auto content = pagx::LayerBuilder::FromSVGFile(svgPath, options);
    if (content.root == nullptr) {
      continue;
    }

    // Use PAGX document size for rendering.
    // PAGX uses viewBox dimensions when viewBox is present, avoiding unit conversion issues
    // (e.g., "1080pt" would become 1440px but viewBox coordinates remain 1080).
    float pagxWidth = content.width;
    float pagxHeight = content.height;
    if (pagxWidth <= 0 || pagxHeight <= 0) {
      continue;
    }

    // Scale up small images for better visibility.
    float maxEdge = std::max(pagxWidth, pagxHeight);
    float scale = (maxEdge < MinCanvasEdge) ? (MinCanvasEdge / maxEdge) : 1.0f;
    int canvasWidth = static_cast<int>(std::ceil(pagxWidth * scale));
    int canvasHeight = static_cast<int>(std::ceil(pagxHeight * scale));

    // Load original SVG with text shaper.
    auto svgStream = Stream::MakeFromFile(svgPath);
    if (svgStream == nullptr) {
      continue;
    }
    auto svgDOM = SVGDOM::Make(*svgStream, textShaper);
    if (svgDOM == nullptr) {
      continue;
    }

    // Get SVG container size (may differ from PAGX size due to unit conversion, e.g., pt -> px).
    auto containerSize = svgDOM->getContainerSize();
    float svgWidth = containerSize.width;
    float svgHeight = containerSize.height;

    // Calculate scale to fit SVG into the same canvas as PAGX.
    // SVG needs to scale from its container size to the canvas size.
    float svgScaleX = static_cast<float>(canvasWidth) / svgWidth;
    float svgScaleY = static_cast<float>(canvasHeight) / svgHeight;

    // Render SVG with calculated scale to match PAGX canvas.
    auto svgSurface = Surface::Make(context, canvasWidth, canvasHeight);
    auto svgCanvas = svgSurface->getCanvas();
    svgCanvas->scale(svgScaleX, svgScaleY);
    svgDOM->render(svgCanvas);
    EXPECT_TRUE(Baseline::Compare(svgSurface, "PAGXTest/" + baseName + "_svg"));

    // Save PAGX file to output directory
    pagx::SVGImporter::Options parserOptions;
    auto doc = pagx::SVGImporter::Parse(svgPath, parserOptions);
    if (doc) {
      std::string xml = pagx::PAGXExporter::ToXML(*doc);
      auto pagxData = Data::MakeWithCopy(xml.data(), xml.size());
      SaveFile(pagxData, "PAGXTest/" + baseName + ".pagx");
    }

    // Render PAGX using DisplayList (required for mask to work).
    // Create a container layer for scaling to preserve content.root's original matrix.
    auto pagxSurface = Surface::Make(context, canvasWidth, canvasHeight);
    DisplayList displayList;
    auto container = tgfx::Layer::Make();
    container->setMatrix(tgfx::Matrix::MakeScale(scale, scale));
    container->addChild(content.root);
    displayList.root()->addChild(container);
    displayList.render(pagxSurface.get(), false);
    EXPECT_TRUE(Baseline::Compare(pagxSurface, "PAGXTest/" + baseName + "_pagx"));
  }

  device->unlock();
}

/**
 * Test case: PathData public API for path construction
 */
PAG_TEST(PAGXTest, PathDataConstruction) {
  // Test basic path commands using public API
  pagx::PathData pathData;
  pathData.moveTo(10, 20);
  pathData.lineTo(30, 40);
  pathData.lineTo(50, 60);
  pathData.cubicTo(70, 80, 90, 100, 110, 120);
  pathData.quadTo(130, 140, 150, 160);
  pathData.close();

  EXPECT_GT(pathData.verbs().size(), 0u);
  EXPECT_GT(pathData.countPoints(), 0u);
  EXPECT_EQ(pathData.verbs().size(), 6u);  // M, L, L, C, Q, Z

  // Test bounds calculation
  auto bounds = pathData.getBounds();
  EXPECT_FALSE(pathData.isEmpty());
  EXPECT_GT(bounds.width, 0.0f);
}

/**
 * Test case: PathData forEach iteration
 */
PAG_TEST(PAGXTest, PathDataForEach) {
  pagx::PathData pathData;
  pathData.moveTo(0, 0);
  pathData.lineTo(100, 0);
  pathData.lineTo(100, 100);
  pathData.lineTo(0, 100);
  pathData.close();

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

  EXPECT_EQ(rect->nodeType(), pagx::NodeType::Rectangle);
  EXPECT_FLOAT_EQ(rect->center.x, 50);
  EXPECT_FLOAT_EQ(rect->size.width, 100);

  // Test Path creation
  auto path = std::make_unique<pagx::Path>();
  path->data.moveTo(0, 0);
  path->data.lineTo(100, 100);
  EXPECT_EQ(path->nodeType(), pagx::NodeType::Path);
  EXPECT_GT(path->data.verbs().size(), 0u);

  // Test Fill creation
  auto fill = std::make_unique<pagx::Fill>();
  auto solidColor = std::make_unique<pagx::SolidColor>();
  solidColor->color = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
  fill->color = std::move(solidColor);
  fill->alpha = 0.8f;
  EXPECT_EQ(fill->nodeType(), pagx::NodeType::Fill);
  EXPECT_NE(fill->color, nullptr);

  // Test Group with children
  auto group = std::make_unique<pagx::Group>();
  group->elements.push_back(std::move(rect));
  group->elements.push_back(std::move(fill));
  EXPECT_EQ(group->nodeType(), pagx::NodeType::Group);
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
  auto solidColor = std::make_unique<pagx::SolidColor>();
  solidColor->color = {0.0f, 0.0f, 1.0f, 1.0f};  // Blue
  fill->color = std::move(solidColor);

  group->elements.push_back(std::move(rect));
  group->elements.push_back(std::move(fill));
  layer->contents.push_back(std::move(group));

  doc->layers.push_back(std::move(layer));

  // Export to XML
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
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
  auto solidColor = std::make_unique<pagx::SolidColor>();
  solidColor->color = {0.0f, 1.0f, 0.0f, 1.0f};  // Green
  fill->color = std::move(solidColor);

  layer->contents.push_back(std::move(rect));
  layer->contents.push_back(std::move(fill));
  doc1->layers.push_back(std::move(layer));

  // Export to XML
  std::string xml = pagx::PAGXExporter::ToXML(*doc1);
  EXPECT_FALSE(xml.empty());

  // Parse the XML back
  auto doc2 = pagx::PAGXImporter::FromXML(xml);
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

  // Test Color with wide gamut
  pagx::Color c2 = {1.0f, 0.5f, 0.0f, 1.0f, pagx::ColorSpace::DisplayP3};
  EXPECT_FLOAT_EQ(c2.red, 1.0f);
  EXPECT_FLOAT_EQ(c2.green, 0.5f);
  EXPECT_FLOAT_EQ(c2.blue, 0.0f);
  EXPECT_EQ(c2.colorSpace, pagx::ColorSpace::DisplayP3);

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
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
  EXPECT_EQ(solid->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(solid->color.red, 1.0f);

  // Test LinearGradient
  auto linear = std::make_unique<pagx::LinearGradient>();
  linear->startPoint.x = 0;
  linear->startPoint.y = 0;
  linear->endPoint.x = 100;
  linear->endPoint.y = 0;

  pagx::ColorStop stop1;
  stop1.offset = 0;
  stop1.color = {1.0f, 0.0f, 0.0f, 1.0f};  // Red

  pagx::ColorStop stop2;
  stop2.offset = 1;
  stop2.color = {0.0f, 0.0f, 1.0f, 1.0f};  // Blue

  linear->colorStops.push_back(stop1);
  linear->colorStops.push_back(stop2);

  EXPECT_EQ(linear->nodeType(), pagx::NodeType::LinearGradient);
  EXPECT_EQ(linear->colorStops.size(), 2u);

  // Test RadialGradient
  auto radial = std::make_unique<pagx::RadialGradient>();
  radial->center.x = 50;
  radial->center.y = 50;
  radial->radius = 50;
  radial->colorStops = linear->colorStops;

  EXPECT_EQ(radial->nodeType(), pagx::NodeType::RadialGradient);
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
  dropShadow->color = {0.0f, 0.0f, 0.0f, 0.5f};  // Semi-transparent black
  layer->styles.push_back(std::move(dropShadow));

  // Add blur filter
  auto blur = std::make_unique<pagx::BlurFilter>();
  blur->blurrinessX = 5;
  blur->blurrinessY = 5;
  layer->filters.push_back(std::move(blur));

  EXPECT_EQ(layer->styles.size(), 1u);
  EXPECT_EQ(layer->filters.size(), 1u);
  EXPECT_EQ(layer->styles[0]->nodeType(), pagx::NodeType::DropShadowStyle);
  EXPECT_EQ(layer->filters[0]->nodeType(), pagx::NodeType::BlurFilter);
}

}  // namespace pag
