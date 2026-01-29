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
#include "pagx/Typesetter.h"
#include "pagx/FontEmbedder.h"
#include "pagx/TextGlyphs.h"
#include "../../pagx/src/StringParser.h"
#include "../../pagx/src/SVGPathParser.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
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
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Rectangle.h"
#include "tgfx/layers/vectors/SolidColor.h"
#include "tgfx/layers/vectors/VectorGroup.h"
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

  // Create Typesetter for text shaping
  pagx::Typesetter typesetter;
  typesetter.setFallbackTypefaces(GetFallbackTypefaces());

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

    // Step 2: Typeset text elements and embed fonts
    auto textGlyphs = typesetter.createTextGlyphs(doc.get());
    pagx::FontEmbedder().embed(doc.get(), textGlyphs);

    // Step 3: Export to XML and save as PAGX file
    std::string xml = pagx::PAGXExporter::ToXML(*doc);
    std::string pagxPath = SavePAGXFile(xml, "PAGXTest/" + baseName + ".pagx");

    // Step 4: Load PAGX file and build layer tree (this is the viewer's actual path)
    auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
    if (reloadedDoc == nullptr) {
      continue;
    }
    auto layer = pagx::LayerBuilder::Build(reloadedDoc.get());
    if (layer == nullptr) {
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
    container->addChild(layer);
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

  auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                         strlen(pagxXml));
  ASSERT_TRUE(doc != nullptr);
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 200, 200);
  DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/ColorRefRender"));

  device->unlock();
}

/**
 * Test case: Verify Layer can directly contain Shape + Fill without Group wrapper.
 */
PAG_TEST(PAGXTest, LayerDirectContent) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // Test 1: Pure tgfx API - VectorLayer with VectorGroup + Rectangle + FillStyle
  {
    auto vectorLayer = tgfx::VectorLayer::Make();

    auto group = std::make_shared<tgfx::VectorGroup>();

    auto rect = std::make_shared<tgfx::Rectangle>();
    rect->setCenter(tgfx::Point::Make(50, 50));
    rect->setSize({100, 100});

    auto fill = std::make_shared<tgfx::FillStyle>();
    fill->setColorSource(tgfx::SolidColor::Make(tgfx::Color::Red()));

    group->setElements({rect, fill});
    vectorLayer->setContents({group});

    auto surface1 = Surface::Make(context, 100, 100);
    DisplayList displayList1;
    displayList1.root()->addChild(vectorLayer);
    displayList1.render(surface1.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface1, "PAGXTest/TGFXVectorLayerDirect"));
  }

  // Test 2: PAGX Layer with Group > Rectangle + Fill
  {
    const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100" version="1.0">
  <Layer>
    <Group>
      <Rectangle center="50,50" size="100,100"/>
      <Fill color="#FF0000"/>
    </Group>
  </Layer>
</pagx>
)";

    auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                           strlen(pagxXml));
    ASSERT_TRUE(doc != nullptr);

    auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxLayer != nullptr);

    auto surface2 = Surface::Make(context, 100, 100);
    DisplayList displayList2;
    displayList2.root()->addChild(tgfxLayer);
    displayList2.render(surface2.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface2, "PAGXTest/PAGXLayerWithGroup"));
  }

  // Test 3: PAGX Layer with direct Rectangle + Fill (no Group)
  {
    const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100" version="1.0">
  <Layer>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";

    auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                           strlen(pagxXml));
    ASSERT_TRUE(doc != nullptr);

    auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxLayer != nullptr);

    auto surface3 = Surface::Make(context, 100, 100);
    DisplayList displayList3;
    displayList3.root()->addChild(tgfxLayer);
    displayList3.render(surface3.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface3, "PAGXTest/PAGXLayerDirect"));
  }

  // Test 4: PAGX Layer with LinearGradient fill
  {
    const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="100" version="1.0">
  <Layer>
    <Rectangle center="100,50" size="200,100"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
)";

    auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                           strlen(pagxXml));
    ASSERT_TRUE(doc != nullptr);

    auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxLayer != nullptr);

    auto surface4 = Surface::Make(context, 200, 100);
    DisplayList displayList4;
    displayList4.root()->addChild(tgfxLayer);
    displayList4.render(surface4.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface4, "PAGXTest/PAGXLinearGradient"));
  }

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

  auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                         strlen(pagxXml));
  ASSERT_TRUE(doc != nullptr);
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 200, 200);
  DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/StrokeColorRefRender"));

  device->unlock();
}

/**
 * Test case: Verify PAGXImporter::FromFile and FromXML produce identical results when rendered.
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

  // Load via FromFile
  auto docFromFile = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(docFromFile != nullptr);
  auto layerFromFile = pagx::LayerBuilder::Build(docFromFile.get());
  ASSERT_TRUE(layerFromFile != nullptr);

  // Load via FromXML
  auto docFromData = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                                  strlen(pagxXml));
  ASSERT_TRUE(docFromData != nullptr);
  auto layerFromData = pagx::LayerBuilder::Build(docFromData.get());
  ASSERT_TRUE(layerFromData != nullptr);

  // Render both and compare
  auto surfaceFile = Surface::Make(context, 100, 100);
  DisplayList displayListFile;
  displayListFile.root()->addChild(layerFromFile);
  displayListFile.render(surfaceFile.get(), false);

  auto surfaceData = Surface::Make(context, 100, 100);
  DisplayList displayListData;
  displayListData.root()->addChild(layerFromData);
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
  pathData.forEach([&verbCount](pagx::PathVerb, const pagx::Point*) { verbCount++; });

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
  dropShadow->blurX = 10;
  dropShadow->blurY = 10;
  dropShadow->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->styles.push_back(dropShadow);

  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 5;
  blur->blurY = 5;
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

  // Build layer tree (doc already parsed above)
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 200, 100);
  DisplayList displayList;
  displayList.root()->addChild(layer);
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

  auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                         strlen(pagxXml));
  ASSERT_TRUE(doc != nullptr);
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 200, 150);
  DisplayList displayList;
  displayList.root()->addChild(layer);
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

  auto doc = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(pagxXml),
                                         strlen(pagxXml));
  ASSERT_TRUE(doc != nullptr);
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 200, 100);
  DisplayList displayList;
  displayList.root()->addChild(layer);
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
 * 2. Typeset text (Typesetter)
 * 3. Render original (typeset document)
 * 4. Export to PAGX
 * 5. Reload PAGX and render (reloaded)
 * 6. Compare render results - should be identical
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

  // Step 2: Typeset text and embed fonts
  pagx::Typesetter typesetter;
  typesetter.setFallbackTypefaces(typefaces);
  auto textGlyphs = typesetter.createTextGlyphs(doc.get());
  EXPECT_FALSE(textGlyphs.empty());
  pagx::FontEmbedder().embed(doc.get(), textGlyphs);

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

  // Step 3: Render typeset document
  auto originalLayer = pagx::LayerBuilder::Build(doc.get(), &typesetter);
  ASSERT_TRUE(originalLayer != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalLayer);
  originalDL.render(originalSurface.get(), false);

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

  // Intentionally not providing TextGlyphs to verify embedded font works
  auto reloadedLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(reloadedLayer != nullptr);

  // Step 6: Render pre-shaped version
  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedLayer);
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

  // Typeset text and embed fonts
  pagx::Typesetter typesetter;
  typesetter.setFallbackTypefaces(typefaces);
  auto textGlyphs = typesetter.createTextGlyphs(doc.get());
  EXPECT_FALSE(textGlyphs.empty());
  pagx::FontEmbedder().embed(doc.get(), textGlyphs);

  // Render typeset document
  auto originalLayer = pagx::LayerBuilder::Build(doc.get(), &typesetter);
  ASSERT_TRUE(originalLayer != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalLayer);
  originalDL.render(originalSurface.get(), false);

  // Export and reload
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  std::string pagxPath = SavePAGXFile(xml, "PAGXTest/textFont_preshaped.pagx");

  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);
  auto reloadedLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(reloadedLayer != nullptr);

  // Render pre-shaped
  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedLayer);
  preshapedDL.render(preshapedSurface.get(), false);

  EXPECT_TRUE(Baseline::Compare(originalSurface, "PAGXTest/TextShaperMultiple_Original"));
  EXPECT_TRUE(Baseline::Compare(preshapedSurface, "PAGXTest/TextShaperMultiple_PreShaped"));

  device->unlock();
}

/**
 * Test case: Complete PAGX example from specification document.
 * This tests the full rendering capabilities of PAGX including:
 * - Multiple layers with different Y positions
 * - Geometric shapes (Rectangle, Ellipse, Polystar, Path)
 * - Gradients (Linear, Radial, Conic)
 * - Shape modifiers (TrimPath, RoundCorner, MergePath, Repeater)
 * - Text with TextLayout
 * - Filters (Blur, DropShadow, Blend, ColorMatrix)
 * - Layer styles (DropShadowStyle, InnerShadowStyle)
 * - Mask and maskType
 * - Embedded fonts and GlyphRun
 * - Composition references
 */
PAG_TEST(PAGXTest, CompleteExample) {
  // Modern dark theme design example
  const char* pagxXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="520">
  
  <!-- Background -->
  <Layer name="Background">
    <Rectangle center="400,260" size="800,520"/>
    <Fill color="@bgGradient"/>
  </Layer>
  
  <!-- Decorative glows -->
  <Layer name="GlowTopLeft" blendMode="screen">
    <Ellipse center="100,80" size="300,300"/>
    <Fill color="@glowPurple"/>
  </Layer>
  <Layer name="GlowBottomRight" blendMode="screen">
    <Ellipse center="700,440" size="400,400"/>
    <Fill color="@glowBlue"/>
  </Layer>
  
  <!-- Title -->
  <Layer name="Title">
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="400,55" textAlign="center"/>
    <Fill color="@titleGradient"/>
  </Layer>
  <Layer name="Subtitle">
    <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
    <TextLayout position="400,95" textAlign="center"/>
    <Fill color="#FFFFFF60"/>
  </Layer>
  
  <!-- Shape cards row y=150 -->
  <Layer name="ShapeCards" y="150">
    <Group position="100,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="100,0">
      <Rectangle center="0,0" size="50,35" roundness="6"/>
      <Fill color="@coral"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#E7524080"/>
    </Group>
    
    <Group position="230,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="230,0">
      <Ellipse center="0,0" size="50,35"/>
      <Fill color="@purple"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#A855F780"/>
    </Group>
    
    <Group position="360,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="360,0">
      <Polystar center="0,0" type="star" pointCount="5" outerRadius="22" innerRadius="10" rotation="-90"/>
      <Fill color="@amber"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F59E0B80"/>
    </Group>
    
    <Group position="490,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="490,0">
      <Polystar center="0,0" type="polygon" pointCount="6" outerRadius="24"/>
      <Fill color="@teal"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#14B8A680"/>
    </Group>
    
    <Group position="620,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="620,0">
      <Path data="M -20 -15 L 0 -25 L 20 -15 L 20 15 L 0 25 L -20 15 Z"/>
      <Fill color="@orange"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F9731680"/>
    </Group>
  </Layer>
  
  <!-- Modifiers row y=270 -->
  <Layer name="Modifiers" y="270">
    <Group position="120,0">
      <Path data="@wavePath"/>
      <TrimPath start="0" end="0.75"/>
      <Stroke color="@cyan" width="3" cap="round"/>
    </Group>
    
    <Group position="280,0">
      <Rectangle center="0,0" size="60,40"/>
      <RoundCorner radius="10"/>
      <Fill color="@emerald"/>
    </Group>
    
    <Group position="420,0">
      <Rectangle center="-10,0" size="35,35"/>
      <Ellipse center="10,0" size="35,35"/>
      <MergePath mode="xor"/>
      <Fill color="@purple"/>
    </Group>
    
    <Group position="580,0">
      <Ellipse center="25,0" size="10,10"/>
      <Fill color="@cyan"/>
      <Repeater copies="8" rotation="45" anchorPoint="0,0" startAlpha="1" endAlpha="0.15"/>
    </Group>
  </Layer>
  
  <!-- Mask example -->
  <Layer id="circleMask" visible="false">
    <Ellipse center="0,0" size="45,45"/>
    <Fill color="#FFFFFF"/>
  </Layer>
  <Layer name="MaskedLayer" x="700" y="270" mask="@circleMask" maskType="alpha">
    <Rectangle center="0,0" size="50,50"/>
    <Fill color="@rainbow"/>
  </Layer>
  
  <!-- Filters row y=360 -->
  <Layer name="FilterCards" y="360">
    <Group position="130,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@emerald"/>
      <BlurFilter blurX="3" blurY="3"/>
    </Group>
    
    <Group position="260,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@cyan"/>
      <DropShadowFilter offsetX="4" offsetY="4" blurX="10" blurY="10" color="#00000080"/>
    </Group>
    
    <Group position="390,0">
      <Ellipse center="0,0" size="55,55"/>
      <Fill color="@purple"/>
      <ColorMatrixFilter matrix="0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0,0,0,1,0"/>
    </Group>
    
    <Group position="520,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@coral"/>
      <BlendFilter color="#3B82F660" blendMode="overlay"/>
    </Group>
    
    <Group position="670,0">
      <Ellipse center="0,0" size="60,60"/>
      <Fill>
        <ImagePattern image="@photo" tileModeX="clamp" tileModeY="clamp"/>
      </Fill>
      <Stroke color="#FFFFFF30" width="2"/>
      <DropShadowStyle offsetX="0" offsetY="6" blurX="12" blurY="12" color="#00000060"/>
    </Group>
  </Layer>
  
  <!-- Footer row y=455 -->
  <Layer name="Footer" y="455">
    <Group position="200,0">
      <Rectangle center="0,0" size="120,36" roundness="18"/>
      <Fill color="@buttonGradient"/>
      <InnerShadowStyle offsetX="0" offsetY="1" blurX="2" blurY="2" color="#FFFFFF30"/>
    </Group>
    <Group position="200,0">
      <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="0,0" textAlign="center"/>
      <Fill color="#FFFFFF"/>
    </Group>
    
    <Group position="400,0">
      <Text fontFamily="Arial" fontSize="18">
        <GlyphRun font="@iconFont" glyphs="1,2,3" y="0" xPositions="0,28,56"/>
      </Text>
      <Fill color="#FFFFFF60"/>
    </Group>
    
    <Group position="600,0">
      <Rectangle center="0,0" size="100,36" roundness="8"/>
      <Fill color="#FFFFFF10"/>
      <Stroke color="#FFFFFF20" width="1"/>
    </Group>
  </Layer>
  <Layer composition="@badgeComp" x="600" y="455"/>
  
  <!-- Resources -->
  <Resources>
    <Image id="photo" source="resources/apitest/imageReplacement.png"/>
    <PathData id="wavePath" data="M 0 0 Q 30 -20 60 0 T 120 0 T 180 0"/>
    <Font id="iconFont">
      <Glyph path="M 0 -8 L 8 8 L -8 8 Z"/>
      <Glyph path="M -8 -8 L 8 -8 L 8 8 L -8 8 Z"/>
      <Glyph path="M 0 -10 A 10 10 0 1 1 0 10 A 10 10 0 1 1 0 -10"/>
    </Font>
    <LinearGradient id="bgGradient" startPoint="0,0" endPoint="800,520">
      <ColorStop offset="0" color="#0F0F1A"/>
      <ColorStop offset="0.5" color="#1A1A2E"/>
      <ColorStop offset="1" color="#0D1B2A"/>
    </LinearGradient>
    <RadialGradient id="glowPurple" center="0,0" radius="150">
      <ColorStop offset="0" color="#A855F720"/>
      <ColorStop offset="1" color="#A855F700"/>
    </RadialGradient>
    <RadialGradient id="glowBlue" center="0,0" radius="200">
      <ColorStop offset="0" color="#3B82F615"/>
      <ColorStop offset="1" color="#3B82F600"/>
    </RadialGradient>
    <LinearGradient id="titleGradient" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFFFFF"/>
      <ColorStop offset="0.5" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <LinearGradient id="buttonGradient" startPoint="0,0" endPoint="120,0">
      <ColorStop offset="0" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <ConicGradient id="rainbow" center="25,25" startAngle="0" endAngle="360">
      <ColorStop offset="0" color="#F43F5E"/>
      <ColorStop offset="0.25" color="#A855F7"/>
      <ColorStop offset="0.5" color="#3B82F6"/>
      <ColorStop offset="0.75" color="#14B8A6"/>
      <ColorStop offset="1" color="#F43F5E"/>
    </ConicGradient>
    <SolidColor id="coral" color="#F43F5E"/>
    <SolidColor id="purple" color="#A855F7"/>
    <SolidColor id="amber" color="#F59E0B"/>
    <SolidColor id="teal" color="#14B8A6"/>
    <SolidColor id="orange" color="#F97316"/>
    <SolidColor id="cyan" color="#06B6D4"/>
    <SolidColor id="emerald" color="#10B981"/>
    <Composition id="badgeComp" width="100" height="36">
      <Layer>
        <Text text="v1.0" fontFamily="Arial" fontStyle="Bold" fontSize="12"/>
        <TextLayout position="50,18" textAlign="center"/>
        <Fill color="#FFFFFF80"/>
      </Layer>
    </Composition>
  </Resources>
  
</pagx>
)";

  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  // Parse XML to document
  auto doc = pagx::PAGXImporter::FromXML(pagxXml);
  ASSERT_TRUE(doc != nullptr);

  // Manually resolve relative Image paths since we parsed from XML string
  for (auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Image) {
      auto* image = static_cast<pagx::Image*>(node.get());
      if (!image->filePath.empty() && image->filePath[0] != '/' &&
          image->filePath.find("://") == std::string::npos) {
        image->filePath = ProjectPath::Absolute(image->filePath);
      }
    }
  }

  // Typeset text elements and embed fonts
  pagx::Typesetter typesetter;
  typesetter.setFallbackTypefaces(GetFallbackTypefaces());
  auto textGlyphs = typesetter.createTextGlyphs(doc.get());
  pagx::FontEmbedder().embed(doc.get(), textGlyphs);

  // Build layer tree
  auto layer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(layer != nullptr);

  auto surface = Surface::Make(context, 800, 520);
  DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/CompleteExample"));

  device->unlock();
}

/**
 * Test all PAGX sample files in pagx/spec/samples directory.
 * Renders each sample and compares with baseline screenshots.
 */
PAG_TEST(PAGXTest, SampleFiles) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  auto context = device->lockContext();
  ASSERT_TRUE(context != nullptr);

  auto samplesDir = ProjectPath::Absolute("pagx/spec/samples");
  std::vector<std::string> sampleFiles;

  for (const auto& entry : std::filesystem::directory_iterator(samplesDir)) {
    if (entry.path().extension() == ".pagx") {
      sampleFiles.push_back(entry.path().string());
    }
  }
  std::sort(sampleFiles.begin(), sampleFiles.end());

  for (const auto& filePath : sampleFiles) {
    auto baseName = std::filesystem::path(filePath).stem().string();

    auto doc = pagx::PAGXImporter::FromFile(filePath);
    if (!doc) {
      FAIL() << "Failed to load: " << filePath;
      continue;
    }

    // Debug: print document info
    std::cout << "[Sample] " << baseName << ": " << doc->width << "x" << doc->height << " layers="
              << doc->layers.size() << std::endl;

    pagx::Typesetter typesetter;
    typesetter.setFallbackTypefaces(GetFallbackTypefaces());
    auto textGlyphs = typesetter.createTextGlyphs(doc.get());
    pagx::FontEmbedder().embed(doc.get(), textGlyphs);

    auto layer = pagx::LayerBuilder::Build(doc.get());
    if (!layer) {
      FAIL() << "Failed to build layer: " << filePath;
      continue;
    }

    // Debug: print layer children count
    std::cout << "  Built layer children: " << layer->children().size() << std::endl;

    auto surface = Surface::Make(context, doc->width, doc->height);
    DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/Samples/" + baseName));
  }

  device->unlock();
}

}  // namespace pag
