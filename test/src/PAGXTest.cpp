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
#include "pagx/TextShaper.h"
#include "../../pagx/src/StringParser.h"
#include "../../pagx/src/SVGPathParser.h"
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
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/svg/SVGPathParser.h"
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
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 100;
  rect->size.height = 80;
  rect->roundness = 10;

  EXPECT_EQ(rect->nodeType(), pagx::NodeType::Rectangle);
  EXPECT_FLOAT_EQ(rect->center.x, 50);
  EXPECT_FLOAT_EQ(rect->size.width, 100);

  auto path = doc->makeNode<pagx::Path>();
  path->data = doc->makeNode<pagx::PathData>();
  path->data->moveTo(0, 0);
  path->data->lineTo(100, 100);
  EXPECT_EQ(path->nodeType(), pagx::NodeType::Path);
  EXPECT_GT(path->data->verbs().size(), 0u);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solidColor = doc->makeNode<pagx::SolidColor>();
  solidColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solidColor;
  fill->alpha = 0.8f;
  EXPECT_EQ(fill->nodeType(), pagx::NodeType::Fill);
  EXPECT_NE(fill->color, nullptr);

  auto group = doc->makeNode<pagx::Group>();
  group->elements.push_back(rect);
  group->elements.push_back(fill);
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

  auto layer = doc->makeNode<pagx::Layer>();
  layer->id = "layer1";
  layer->name = "Test Layer";

  auto group = doc->makeNode<pagx::Group>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 80;
  rect->size.height = 60;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solidColor = doc->makeNode<pagx::SolidColor>();
  solidColor->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill->color = solidColor;

  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);

  doc->layers.push_back(layer);

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

  auto layer = doc1->makeNode<pagx::Layer>();
  layer->name = "TestLayer";

  auto rect = doc1->makeNode<pagx::Rectangle>();
  rect->center.x = 50;
  rect->center.y = 50;
  rect->size.width = 80;
  rect->size.height = 60;

  auto fill = doc1->makeNode<pagx::Fill>();
  auto solidColor = doc1->makeNode<pagx::SolidColor>();
  solidColor->color = {0.0f, 1.0f, 0.0f, 1.0f};
  fill->color = solidColor;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc1->layers.push_back(layer);

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
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  EXPECT_EQ(solid->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(solid->color.red, 1.0f);

  auto linear = doc->makeNode<pagx::LinearGradient>();
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

  auto radial = doc->makeNode<pagx::RadialGradient>();
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
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "StyledLayer";
  layer->alpha = 0.8f;
  layer->blendMode = pagx::BlendMode::Multiply;

  auto dropShadow = doc->makeNode<pagx::DropShadowStyle>();
  dropShadow->offsetX = 5;
  dropShadow->offsetY = 5;
  dropShadow->blurrinessX = 10;
  dropShadow->blurrinessY = 10;
  dropShadow->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->styles.push_back(dropShadow);

  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurrinessX = 5;
  blur->blurrinessY = 5;
  layer->filters.push_back(blur);

  EXPECT_EQ(layer->styles.size(), 1u);
  EXPECT_EQ(layer->filters.size(), 1u);
  EXPECT_EQ(layer->styles[0]->nodeType(), pagx::NodeType::DropShadowStyle);
  EXPECT_EQ(layer->filters[0]->nodeType(), pagx::NodeType::BlurFilter);
}

/**
 * Test case: Font and Glyph node creation
 */
PAG_TEST(PAGXTest, FontGlyphNodes) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>();
  font->id = "testFont";

  auto glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = doc->makeNode<pagx::PathData>();
  *glyph1->path = pagx::PathDataFromSVGString("M0 0 L100 0 L100 100 L0 100 Z");

  auto glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = doc->makeNode<pagx::PathData>();
  *glyph2->path = pagx::PathDataFromSVGString("M50 0 L100 100 L0 100 Z");

  font->glyphs.push_back(glyph1);
  font->glyphs.push_back(glyph2);

  EXPECT_EQ(font->nodeType(), pagx::NodeType::Font);
  EXPECT_EQ(font->glyphs.size(), 2u);
  EXPECT_EQ(font->glyphs[0]->nodeType(), pagx::NodeType::Glyph);
  EXPECT_NE(font->glyphs[0]->path, nullptr);
}

/**
 * Test case: GlyphRun node with horizontal positioning
 */
PAG_TEST(PAGXTest, GlyphRunHorizontal) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>("testFont");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2, 1};
  glyphRun->y = 50;
  glyphRun->xPositions = {10, 60, 110};

  EXPECT_EQ(glyphRun->nodeType(), pagx::NodeType::GlyphRun);
  EXPECT_EQ(glyphRun->font, font);
  EXPECT_EQ(glyphRun->glyphs.size(), 3u);
  EXPECT_EQ(glyphRun->xPositions.size(), 3u);
  EXPECT_FLOAT_EQ(glyphRun->y, 50.0f);
}

/**
 * Test case: GlyphRun node with point positions
 */
PAG_TEST(PAGXTest, GlyphRunPointPositions) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>("testFont");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2};
  glyphRun->positions = {{10, 20}, {60, 80}};

  EXPECT_EQ(glyphRun->nodeType(), pagx::NodeType::GlyphRun);
  EXPECT_EQ(glyphRun->positions.size(), 2u);
  EXPECT_FLOAT_EQ(glyphRun->positions[0].x, 10.0f);
  EXPECT_FLOAT_EQ(glyphRun->positions[1].y, 80.0f);
}

/**
 * Test case: GlyphRun node with RSXform positioning
 */
PAG_TEST(PAGXTest, GlyphRunRSXform) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>("testFont");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2};

  pagx::RSXform xform1 = {1, 0, 10, 20};
  pagx::RSXform xform2 = {0.707f, 0.707f, 60, 80};
  glyphRun->xforms = {xform1, xform2};

  EXPECT_EQ(glyphRun->nodeType(), pagx::NodeType::GlyphRun);
  EXPECT_EQ(glyphRun->xforms.size(), 2u);
  EXPECT_FLOAT_EQ(glyphRun->xforms[0].scos, 1.0f);
  EXPECT_FLOAT_EQ(glyphRun->xforms[1].ssin, 0.707f);
}

/**
 * Test case: Text node with precomposed GlyphRuns
 */
PAG_TEST(PAGXTest, TextPrecomposed) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto text = doc->makeNode<pagx::Text>();
  text->fontSize = 24;

  auto font = doc->makeNode<pagx::Font>("font1");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2, 3};
  glyphRun->y = 24;
  glyphRun->xPositions = {0, 24, 48};
  text->glyphRuns.push_back(glyphRun);

  EXPECT_EQ(text->nodeType(), pagx::NodeType::Text);
  EXPECT_EQ(text->glyphRuns.size(), 1u);
  EXPECT_EQ(text->glyphRuns[0]->glyphs.size(), 3u);
}

/**
 * Test case: PathTypefaceBuilder and TextBlobBuilder basic functionality
 */
PAG_TEST(PAGXTest, PathTypefaceBasic) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // Create a custom typeface from path
  tgfx::PathTypefaceBuilder builder;
  auto path1 = tgfx::SVGPathParser::FromSVGString("M0 0 L40 0 L40 50 L0 50 Z");
  ASSERT_TRUE(path1 != nullptr);
  auto glyphId1 = builder.addGlyph(*path1);
  EXPECT_EQ(glyphId1, 1);

  auto path2 = tgfx::SVGPathParser::FromSVGString("M0 0 L20 50 L40 0 Z");
  ASSERT_TRUE(path2 != nullptr);
  auto glyphId2 = builder.addGlyph(*path2);
  EXPECT_EQ(glyphId2, 2);

  auto typeface = builder.detach();
  ASSERT_TRUE(typeface != nullptr);

  // Render using canvas->drawGlyphs directly
  auto surface = Surface::Make(context, 200, 100);
  auto canvas = surface->getCanvas();

  // For CustomTypeface, fontSize should be 1.0 since path coordinates define the actual size
  tgfx::Font font(typeface, 1);
  std::vector<tgfx::GlyphID> glyphIDs = {1, 2, 1};
  std::vector<tgfx::Point> positions = {
      tgfx::Point::Make(10, 60), tgfx::Point::Make(60, 60), tgfx::Point::Make(110, 60)};
  tgfx::Paint paint;
  paint.setColor(tgfx::Color::Red());
  canvas->drawGlyphs(glyphIDs.data(), positions.data(), glyphIDs.size(), font, paint);

  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PathTypefaceBasic"));

  device->unlock();
}

/**
 * Test case: Precomposed text rendering with embedded font
 */
PAG_TEST(PAGXTest, PrecomposedTextRender) {
  // PAGX with embedded font and precomposed text
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="100" version="1.0">
  <Resources>
    <Font id="pathFont">
      <Glyph path="M0 0 L40 0 L40 50 L0 50 Z"/>
      <Glyph path="M0 0 L20 50 L40 0 Z"/>
    </Font>
  </Resources>
  <Layer>
    <Group>
      <Text fontSize="50">
        <GlyphRun font="@pathFont" glyphs="1,2,1" y="60" xPositions="10,60,110"/>
      </Text>
      <Fill color="#FF0000"/>
    </Group>
  </Layer>
</pagx>
)";

  // First verify the XML parsing
  auto doc = pagx::PAGXImporter::FromXML(pagxXml);
  ASSERT_TRUE(doc != nullptr);
  ASSERT_FALSE(doc->nodes.empty());
  
  // Find the Font resource
  pagx::Font* fontNode = nullptr;
  for (const auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Font && node->id == "pathFont") {
      fontNode = static_cast<pagx::Font*>(node.get());
      break;
    }
  }
  ASSERT_TRUE(fontNode != nullptr);
  EXPECT_EQ(fontNode->glyphs.size(), 2u);

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  pagx::LayerBuilder::Options options;
  auto content = pagx::LayerBuilder::FromData(reinterpret_cast<const uint8_t*>(pagxXml),
                                              strlen(pagxXml), options);
  ASSERT_TRUE(content.root != nullptr);

  auto surface = Surface::Make(context, 200, 100);
  DisplayList displayList;
  displayList.root()->addChild(content.root);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PrecomposedTextRender"));

  device->unlock();
}

/**
 * Test case: Precomposed text with point positions
 */
PAG_TEST(PAGXTest, PrecomposedTextPointPositions) {
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="150" version="1.0">
  <Resources>
    <Font id="pathFont">
      <Glyph path="M0 0 L30 0 L30 40 L0 40 Z"/>
      <Glyph path="M15 0 L30 40 L0 40 Z"/>
    </Font>
  </Resources>
  <Layer>
    <Group>
      <Text fontSize="40">
        <GlyphRun font="@pathFont" glyphs="1,2,1" positions="20,30;80,60;140,90"/>
      </Text>
      <Fill color="#0000FF"/>
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

  auto surface = Surface::Make(context, 200, 150);
  DisplayList displayList;
  displayList.root()->addChild(content.root);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PrecomposedTextPointPositions"));

  device->unlock();
}

/**
 * Test case: Text with missing glyph (GlyphID 0)
 */
PAG_TEST(PAGXTest, PrecomposedTextMissingGlyph) {
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="100" version="1.0">
  <Resources>
    <Font id="testFont">
      <Glyph path="M0 0 L40 0 L40 50 L0 50 Z"/>
    </Font>
  </Resources>
  <Layer>
    <Group>
      <Text fontSize="50">
        <GlyphRun font="@testFont" glyphs="1,0,1" y="60" xPositions="10,60,110"/>
      </Text>
      <Fill color="#00FF00"/>
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

  auto surface = Surface::Make(context, 200, 100);
  DisplayList displayList;
  displayList.root()->addChild(content.root);
  displayList.render(surface.get(), false);
  // GlyphID 0 should not be rendered, so only two glyphs appear
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PrecomposedTextMissingGlyph"));

  device->unlock();
}

/**
 * Test case: Font and GlyphRun XML round-trip
 */
PAG_TEST(PAGXTest, FontGlyphRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  ASSERT_TRUE(doc != nullptr);

  auto font = doc->makeNode<pagx::Font>();
  font->id = "roundTripFont";

  auto glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = doc->makeNode<pagx::PathData>();
  *glyph1->path = pagx::PathDataFromSVGString("M0 0 L40 0 L40 50 L0 50 Z");
  font->glyphs.push_back(glyph1);

  auto glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = doc->makeNode<pagx::PathData>();
  *glyph2->path = pagx::PathDataFromSVGString("M20 0 L40 50 L0 50 Z");
  font->glyphs.push_back(glyph2);

  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();

  auto text = doc->makeNode<pagx::Text>();
  text->fontSize = 50;

  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2};
  glyphRun->y = 60;
  glyphRun->xPositions = {10, 60};
  text->glyphRuns.push_back(glyphRun);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solidColor = doc->makeNode<pagx::SolidColor>();
  solidColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solidColor;

  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<Font"), std::string::npos);
  EXPECT_NE(xml.find("<Glyph"), std::string::npos);
  EXPECT_NE(xml.find("<GlyphRun"), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_GE(doc2->nodes.size(), 1u);
  EXPECT_GE(doc2->layers.size(), 1u);

  SavePAGXFile(xml, "PAGXTest/font_glyph_roundtrip.pagx");
}

/**
 * Test case: Text shaping round-trip.
 * 1. Load SVG with text content
 * 2. Render original (runtime shaping)
 * 3. Shape text (TextShaper)
 * 4. Export to PAGX
 * 5. Reload PAGX and render (pre-shaped)
 * 6. Compare render results
 */
PAG_TEST(PAGXTest, TextShaperRoundTrip) {
  // Use text.svg as the test file
  std::string svgPath = ProjectPath::Absolute("resources/apitest/SVG/text.svg");

  // Step 1: Parse SVG
  auto doc = pagx::SVGImporter::Parse(svgPath);
  ASSERT_TRUE(doc != nullptr);
  ASSERT_GT(doc->width, 0);
  ASSERT_GT(doc->height, 0);

  int canvasWidth = static_cast<int>(doc->width);
  int canvasHeight = static_cast<int>(doc->height);

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto typefaces = GetFallbackTypefaces();
  ASSERT_FALSE(typefaces.empty());

  // Step 2: Render original (runtime shaping)
  pagx::LayerBuilder::Options buildOptions;
  buildOptions.fallbackTypefaces = typefaces;
  auto originalContent = pagx::LayerBuilder::Build(*doc, buildOptions);
  ASSERT_TRUE(originalContent.root != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalContent.root);
  originalDL.render(originalSurface.get(), false);

  // Step 3: Shape text
  auto textShaper = pagx::TextShaper::Make();
  ASSERT_TRUE(textShaper != nullptr);
  textShaper->setFallbackTypefaces(typefaces);
  bool shaped = textShaper->shape(doc.get());
  EXPECT_TRUE(shaped);

  // Verify Font resources were added
  bool hasFontResource = false;
  for (const auto& res : doc->nodes) {
    if (res->nodeType() == pagx::NodeType::Font) {
      hasFontResource = true;
      auto fontNode = static_cast<const pagx::Font*>(res.get());
      EXPECT_FALSE(fontNode->glyphs.empty());
      break;
    }
  }
  EXPECT_TRUE(hasFontResource);

  // Step 4: Export to PAGX
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  EXPECT_NE(xml.find("<Font"), std::string::npos);
  EXPECT_NE(xml.find("<GlyphRun"), std::string::npos);

  std::string pagxPath = SavePAGXFile(xml, "PAGXTest/text_preshaped.pagx");

  // Step 5: Reload PAGX (without typefaces - should use embedded font)
  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);

  // Verify Font resource was imported
  bool fontFound = false;
  for (const auto& res : reloadedDoc->nodes) {
    if (res->nodeType() == pagx::NodeType::Font) {
      auto fontNode = static_cast<const pagx::Font*>(res.get());
      EXPECT_FALSE(fontNode->id.empty());
      EXPECT_FALSE(fontNode->glyphs.empty());
      fontFound = true;
    }
  }
  EXPECT_TRUE(fontFound);

  // Verify Text has GlyphRun
  bool glyphRunFound = false;
  for (const auto& layer : reloadedDoc->layers) {
    for (const auto& content : layer->contents) {
      if (content->nodeType() == pagx::NodeType::Group) {
        auto group = static_cast<const pagx::Group*>(content);
        for (const auto& elem : group->elements) {
          if (elem->nodeType() == pagx::NodeType::Text) {
            auto text = static_cast<const pagx::Text*>(elem);
            if (!text->glyphRuns.empty()) {
              glyphRunFound = true;
            }
          }
        }
      }
    }
  }
  EXPECT_TRUE(glyphRunFound);

  pagx::LayerBuilder::Options reloadOptions;
  // Intentionally not providing fallback typefaces to verify embedded font works
  auto reloadedContent = pagx::LayerBuilder::Build(*reloadedDoc, reloadOptions);
  ASSERT_TRUE(reloadedContent.root != nullptr);

  // Step 6: Render pre-shaped version
  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedContent.root);
  preshapedDL.render(preshapedSurface.get(), false);

  // Step 7: Compare renders - they should be identical
  auto originalPixels = originalSurface->makeImageSnapshot();
  auto preshapedPixels = preshapedSurface->makeImageSnapshot();
  ASSERT_TRUE(originalPixels != nullptr);
  ASSERT_TRUE(preshapedPixels != nullptr);

  // Save outputs for visual inspection
  EXPECT_TRUE(Baseline::Compare(originalSurface, "PAGXTest/TextShaper_Original"));
  EXPECT_TRUE(Baseline::Compare(preshapedSurface, "PAGXTest/TextShaper_PreShaped"));

  device->unlock();
}

/**
 * Test case: Text shaping with textFont.svg (multiple text elements)
 */
PAG_TEST(PAGXTest, TextShaperMultipleText) {
  std::string svgPath = ProjectPath::Absolute("resources/apitest/SVG/textFont.svg");

  auto doc = pagx::SVGImporter::Parse(svgPath);
  ASSERT_TRUE(doc != nullptr);

  int canvasWidth = static_cast<int>(doc->width);
  int canvasHeight = static_cast<int>(doc->height);

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto typefaces = GetFallbackTypefaces();

  // Render original
  pagx::LayerBuilder::Options buildOptions;
  buildOptions.fallbackTypefaces = typefaces;
  auto originalContent = pagx::LayerBuilder::Build(*doc, buildOptions);
  ASSERT_TRUE(originalContent.root != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalContent.root);
  originalDL.render(originalSurface.get(), false);

  // Shape text
  auto textShaper = pagx::TextShaper::Make();
  ASSERT_TRUE(textShaper != nullptr);
  textShaper->setFallbackTypefaces(typefaces);
  bool shaped = textShaper->shape(doc.get());
  EXPECT_TRUE(shaped);

  // Export and reload
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  std::string pagxPath = SavePAGXFile(xml, "PAGXTest/textFont_preshaped.pagx");

  pagx::LayerBuilder::Options reloadOptions;
  auto reloadedContent = pagx::LayerBuilder::FromFile(pagxPath, reloadOptions);
  ASSERT_TRUE(reloadedContent.root != nullptr);

  // Render pre-shaped
  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedContent.root);
  preshapedDL.render(preshapedSurface.get(), false);

  EXPECT_TRUE(Baseline::Compare(originalSurface, "PAGXTest/TextShaperMultiple_Original"));
  EXPECT_TRUE(Baseline::Compare(preshapedSurface, "PAGXTest/TextShaperMultiple_PreShaped"));

  device->unlock();
}

}  // namespace pag
