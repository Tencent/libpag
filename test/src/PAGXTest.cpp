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
#include <cstring>
#include <filesystem>
#include <fstream>
#include "pagx/LayerBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
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

static std::string SavePAGXFile(const std::string& xml, const std::string& key) {
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  if (file) {
    file.write(xml.data(), static_cast<std::streamsize>(xml.size()));
  }
  return outPath;
}

/**
 * Test case: Convert all SVG files to PAGX format, save as files, then load and render.
 * This tests the complete round-trip: SVG -> PAGX file -> Load -> Render
 */
PAG_TEST(PAGXTest, SVGToPAGXAll) {
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

  auto textShaper = TextShaper::Make(GetFallbackTypefaces());

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    // Step 1: Parse SVG to PAGXDocument
    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      continue;
    }

    float pagxWidth = doc->width;
    float pagxHeight = doc->height;
    if (pagxWidth <= 0 || pagxHeight <= 0) {
      continue;
    }

    // Step 2: Export to XML and save as PAGX file
    std::string xml = pagx::PAGXExporter::ToXML(*doc);
    std::string pagxPath = SavePAGXFile(xml, "PAGXTest/" + baseName + ".pagx");

    // Step 3: Load PAGX file and build layer tree (this is the viewer's actual path)
    pagx::LayerBuilder::Options options;
    options.fallbackTypefaces = GetFallbackTypefaces();
    auto content = pagx::LayerBuilder::FromFile(pagxPath, options);
    if (content.root == nullptr) {
      continue;
    }

    // Calculate canvas size
    float maxEdge = std::max(pagxWidth, pagxHeight);
    float scale = (maxEdge < MinCanvasEdge) ? (MinCanvasEdge / maxEdge) : 1.0f;
    int canvasWidth = static_cast<int>(std::ceil(pagxWidth * scale));
    int canvasHeight = static_cast<int>(std::ceil(pagxHeight * scale));

    // Render original SVG for comparison
    auto svgStream = Stream::MakeFromFile(svgPath);
    if (svgStream == nullptr) {
      continue;
    }
    auto svgDOM = SVGDOM::Make(*svgStream, textShaper);
    if (svgDOM == nullptr) {
      continue;
    }

    auto containerSize = svgDOM->getContainerSize();
    float svgWidth = containerSize.width;
    float svgHeight = containerSize.height;
    float svgScaleX = static_cast<float>(canvasWidth) / svgWidth;
    float svgScaleY = static_cast<float>(canvasHeight) / svgHeight;

    auto svgSurface = Surface::Make(context, canvasWidth, canvasHeight);
    auto svgCanvas = svgSurface->getCanvas();
    svgCanvas->scale(svgScaleX, svgScaleY);
    svgDOM->render(svgCanvas);
    EXPECT_TRUE(Baseline::Compare(svgSurface, "PAGXTest/" + baseName + "_svg"));

    // Render PAGX (loaded from file)
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
 * Test case: Verify colorRef parsing for both hex colors and resource references.
 * This ensures Fill and Stroke with colorRef are rendered correctly.
 */
PAG_TEST(PAGXTest, ColorRefRender) {
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200" version="1.0">
  <Resources>
    <LinearGradient id="grad1" startPoint="0,0" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
  <Layer>
    <Group>
      <Rectangle center="50,100" size="80,80"/>
      <Fill color="#00FF00"/>
    </Group>
    <Group>
      <Rectangle center="150,100" size="80,80"/>
      <Fill color="@grad1"/>
    </Group>
  </Layer>
</pagx>
)";

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  pagx::LayerBuilder::Options options;
  options.fallbackTypefaces = GetFallbackTypefaces();
  auto content = pagx::LayerBuilder::FromData(reinterpret_cast<const uint8_t*>(pagxXml),
                                              strlen(pagxXml), options);
  ASSERT_TRUE(content.root != nullptr);
  EXPECT_FLOAT_EQ(content.width, 200.0f);
  EXPECT_FLOAT_EQ(content.height, 200.0f);

  auto surface = Surface::Make(context, 200, 200);
  DisplayList displayList;
  displayList.root()->addChild(content.root);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/ColorRefRender"));

  device->unlock();
}

/**
 * Test case: Verify Stroke with colorRef renders correctly.
 */
PAG_TEST(PAGXTest, StrokeColorRefRender) {
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200" version="1.0">
  <Resources>
    <LinearGradient id="strokeGrad" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#00FF00"/>
    </LinearGradient>
  </Resources>
  <Layer>
    <Group>
      <Rectangle center="60,100" size="80,80"/>
      <Stroke color="#0000FF" width="4"/>
    </Group>
    <Group>
      <Rectangle center="140,100" size="80,80"/>
      <Stroke color="@strokeGrad" width="4"/>
    </Group>
  </Layer>
</pagx>
)";

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  pagx::LayerBuilder::Options options;
  auto content = pagx::LayerBuilder::FromData(reinterpret_cast<const uint8_t*>(pagxXml),
                                              strlen(pagxXml), options);
  ASSERT_TRUE(content.root != nullptr);

  auto surface = Surface::Make(context, 200, 200);
  DisplayList displayList;
  displayList.root()->addChild(content.root);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/StrokeColorRefRender"));

  device->unlock();
}

/**
 * Test case: Verify LayerBuilder::FromFile and FromData produce identical results.
 */
PAG_TEST(PAGXTest, LayerBuilderAPIConsistency) {
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100" version="1.0">
  <Layer>
    <Group>
      <Rectangle center="50,50" size="60,60"/>
      <Fill color="#FF5500"/>
    </Group>
  </Layer>
</pagx>
)";

  // Save to file
  std::string pagxPath = SavePAGXFile(pagxXml, "PAGXTest/api_test.pagx");

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  pagx::LayerBuilder::Options options;

  // Load via FromFile
  auto contentFromFile = pagx::LayerBuilder::FromFile(pagxPath, options);
  ASSERT_TRUE(contentFromFile.root != nullptr);

  // Load via FromData
  auto contentFromData = pagx::LayerBuilder::FromData(reinterpret_cast<const uint8_t*>(pagxXml),
                                                      strlen(pagxXml), options);
  ASSERT_TRUE(contentFromData.root != nullptr);

  // Render both and compare
  auto surfaceFile = Surface::Make(context, 100, 100);
  DisplayList displayListFile;
  displayListFile.root()->addChild(contentFromFile.root);
  displayListFile.render(surfaceFile.get(), false);

  auto surfaceData = Surface::Make(context, 100, 100);
  DisplayList displayListData;
  displayListData.root()->addChild(contentFromData.root);
  displayListData.render(surfaceData.get(), false);

  // Both should match the same baseline
  EXPECT_TRUE(Baseline::Compare(surfaceFile, "PAGXTest/LayerBuilderAPIConsistency"));
  EXPECT_TRUE(Baseline::Compare(surfaceData, "PAGXTest/LayerBuilderAPIConsistency"));

  device->unlock();
}

/**
 * Test case: PathData public API for path construction
 */
PAG_TEST(PAGXTest, PathDataConstruction) {
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
  auto rect = std::make_unique<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 100;
  rect->size.height = 80;
  rect->roundness = 10;

  EXPECT_EQ(rect->nodeType(), pagx::NodeType::Rectangle);
  EXPECT_FLOAT_EQ(rect->center.x, 50);
  EXPECT_FLOAT_EQ(rect->size.width, 100);

  auto path = std::make_unique<pagx::Path>();
  path->data.moveTo(0, 0);
  path->data.lineTo(100, 100);
  EXPECT_EQ(path->nodeType(), pagx::NodeType::Path);
  EXPECT_GT(path->data.verbs().size(), 0u);

  auto fill = std::make_unique<pagx::Fill>();
  auto solidColor = std::make_unique<pagx::SolidColor>();
  solidColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = std::move(solidColor);
  fill->alpha = 0.8f;
  EXPECT_EQ(fill->nodeType(), pagx::NodeType::Fill);
  EXPECT_NE(fill->color, nullptr);

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

  auto layer = std::make_unique<pagx::Layer>();
  layer->id = "layer1";
  layer->name = "Test Layer";

  auto group = std::make_unique<pagx::Group>();

  auto rect = std::make_unique<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 80;
  rect->size.height = 60;

  auto fill = std::make_unique<pagx::Fill>();
  auto solidColor = std::make_unique<pagx::SolidColor>();
  solidColor->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill->color = std::move(solidColor);

  group->elements.push_back(std::move(rect));
  group->elements.push_back(std::move(fill));
  layer->contents.push_back(std::move(group));

  doc->layers.push_back(std::move(layer));

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<pagx"), std::string::npos);
  EXPECT_NE(xml.find("width=\"400\""), std::string::npos);
  EXPECT_NE(xml.find("height=\"300\""), std::string::npos);

  SavePAGXFile(xml, "PAGXTest/document_export.pagx");
}

/**
 * Test case: PAGXDocument XML round-trip (create -> export -> parse)
 */
PAG_TEST(PAGXTest, PAGXDocumentRoundTrip) {
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
  solidColor->color = {0.0f, 1.0f, 0.0f, 1.0f};
  fill->color = std::move(solidColor);

  layer->contents.push_back(std::move(rect));
  layer->contents.push_back(std::move(fill));
  doc1->layers.push_back(std::move(layer));

  std::string xml = pagx::PAGXExporter::ToXML(*doc1);
  EXPECT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);

  EXPECT_FLOAT_EQ(doc2->width, 200.0f);
  EXPECT_FLOAT_EQ(doc2->height, 150.0f);
  EXPECT_GE(doc2->layers.size(), 1u);
}

/**
 * Test case: PAGXTypes basic operations
 */
PAG_TEST(PAGXTest, PAGXTypesBasic) {
  pagx::Point p1 = {10.0f, 20.0f};
  EXPECT_FLOAT_EQ(p1.x, 10.0f);
  EXPECT_FLOAT_EQ(p1.y, 20.0f);

  pagx::Rect r1 = {0.0f, 10.0f, 100.0f, 50.0f};
  EXPECT_FLOAT_EQ(r1.x, 0.0f);
  EXPECT_FLOAT_EQ(r1.y, 10.0f);
  EXPECT_FLOAT_EQ(r1.width, 100.0f);
  EXPECT_FLOAT_EQ(r1.height, 50.0f);
  EXPECT_FLOAT_EQ(r1.left(), 0.0f);
  EXPECT_FLOAT_EQ(r1.top(), 10.0f);
  EXPECT_FLOAT_EQ(r1.right(), 100.0f);
  EXPECT_FLOAT_EQ(r1.bottom(), 60.0f);

  auto r2 = pagx::Rect::MakeLTRB(10, 20, 110, 70);
  EXPECT_FLOAT_EQ(r2.x, 10.0f);
  EXPECT_FLOAT_EQ(r2.y, 20.0f);
  EXPECT_FLOAT_EQ(r2.width, 100.0f);
  EXPECT_FLOAT_EQ(r2.height, 50.0f);

  pagx::Color c1 = {1.0f, 0.5f, 0.0f, 1.0f};
  EXPECT_FLOAT_EQ(c1.red, 1.0f);
  EXPECT_FLOAT_EQ(c1.green, 0.5f);
  EXPECT_FLOAT_EQ(c1.blue, 0.0f);
  EXPECT_FLOAT_EQ(c1.alpha, 1.0f);

  pagx::Color c2 = {1.0f, 0.5f, 0.0f, 1.0f, pagx::ColorSpace::DisplayP3};
  EXPECT_FLOAT_EQ(c2.red, 1.0f);
  EXPECT_FLOAT_EQ(c2.green, 0.5f);
  EXPECT_FLOAT_EQ(c2.blue, 0.0f);
  EXPECT_EQ(c2.colorSpace, pagx::ColorSpace::DisplayP3);

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
  auto solid = std::make_unique<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  EXPECT_EQ(solid->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(solid->color.red, 1.0f);

  auto linear = std::make_unique<pagx::LinearGradient>();
  linear->startPoint.x = 0;
  linear->startPoint.y = 0;
  linear->endPoint.x = 100;
  linear->endPoint.y = 0;

  pagx::ColorStop stop1;
  stop1.offset = 0;
  stop1.color = {1.0f, 0.0f, 0.0f, 1.0f};

  pagx::ColorStop stop2;
  stop2.offset = 1;
  stop2.color = {0.0f, 0.0f, 1.0f, 1.0f};

  linear->colorStops.push_back(stop1);
  linear->colorStops.push_back(stop2);

  EXPECT_EQ(linear->nodeType(), pagx::NodeType::LinearGradient);
  EXPECT_EQ(linear->colorStops.size(), 2u);

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

  auto dropShadow = std::make_unique<pagx::DropShadowStyle>();
  dropShadow->offsetX = 5;
  dropShadow->offsetY = 5;
  dropShadow->blurrinessX = 10;
  dropShadow->blurrinessY = 10;
  dropShadow->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->styles.push_back(std::move(dropShadow));

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
