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
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/StringParser.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ShapedText.h"
#include "renderer/TextLayout.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

static pagx::Group* MakeCenteredTextGroup(pagx::PAGXDocument* doc, const std::string& content,
                                          float fontSize, float centerX, float y,
                                          pagx::Color color) {
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = content;
  text->fontSize = fontSize;
  text->position = {centerX, y};
  text->textAnchor = pagx::TextAnchor::Center;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  return group;
}

static std::vector<std::shared_ptr<Typeface>> CreateFallbackTypefaces() {
  std::vector<std::shared_ptr<Typeface>> result = {};
  auto regularTypeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  if (regularTypeface) {
    result.push_back(regularTypeface);
  }
  auto emojiTypeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"));
  if (emojiTypeface) {
    result.push_back(emojiTypeface);
  }
  auto hebrewTypeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"));
  if (hebrewTypeface) {
    result.push_back(hebrewTypeface);
  }
  return result;
}

static std::vector<std::shared_ptr<Typeface>> GetFallbackTypefaces() {
  static auto typefaces = CreateFallbackTypefaces();
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
PAGX_TEST(PAGXTest, SVGToPAGXAll) {
  constexpr int MinCanvasEdge = 400;

  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }

  std::sort(svgFiles.begin(), svgFiles.end());

  // Create TextLayout for text layout
  pagx::TextLayout textLayout;
  textLayout.addFallbackTypefaces(GetFallbackTypefaces());

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    // Step 1: Parse SVG to PAGXDocument
    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      ADD_FAILURE() << "Failed to parse SVG: " << svgPath;
      continue;
    }

    float pagxWidth = doc->width;
    float pagxHeight = doc->height;
    if (pagxWidth <= 0 || pagxHeight <= 0) {
      ADD_FAILURE() << "Invalid dimensions in SVG: " << svgPath;
      continue;
    }

    // Step 2: Typeset text elements and embed fonts
    auto layoutResult = textLayout.layout(doc.get());
    pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap, layoutResult.textOrder);

    // Step 3: Export to XML and save as PAGX file
    std::string xml = pagx::PAGXExporter::ToXML(*doc);
    std::string pagxPath = SavePAGXFile(xml, "PAGXTest/" + baseName + ".pagx");

    // Step 4: Load PAGX file and build layer tree (this is the viewer's actual path)
    auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
    if (reloadedDoc == nullptr) {
      ADD_FAILURE() << "Failed to reload: " << pagxPath;
      continue;
    }
    if (!reloadedDoc->errors.empty()) {
      std::string errorLog;
      for (const auto& error : reloadedDoc->errors) {
        errorLog += "\n  " + error;
      }
      ADD_FAILURE() << "Parse errors in " << baseName << ":" << errorLog;
    }
    auto layer = pagx::LayerBuilder::Build(reloadedDoc.get());
    if (layer == nullptr) {
      ADD_FAILURE() << "Failed to build layer: " << pagxPath;
      continue;
    }

    // Calculate canvas size
    float maxEdge = std::max(pagxWidth, pagxHeight);
    float scale = (maxEdge < MinCanvasEdge) ? (MinCanvasEdge / maxEdge) : 1.0f;
    int canvasWidth = static_cast<int>(std::ceil(pagxWidth * scale));
    int canvasHeight = static_cast<int>(std::ceil(pagxHeight * scale));

    // Render PAGX (loaded from file)
    auto pagxSurface = Surface::Make(context, canvasWidth, canvasHeight);
    DisplayList displayList;
    auto container = tgfx::Layer::Make();
    container->setMatrix(tgfx::Matrix::MakeScale(scale, scale));
    container->addChild(layer);
    displayList.root()->addChild(container);
    displayList.render(pagxSurface.get(), false);
    EXPECT_TRUE(Baseline::Compare(pagxSurface, "PAGXTest/" + baseName)) << baseName;
  }
}

/**
 * Test case: Verify PAGXImporter::FromFile and FromXML produce identical results when rendered.
 */
PAGX_TEST(PAGXTest, LayerBuilderAPIConsistency) {
  auto pagxPath = ProjectPath::Absolute("resources/apitest/api_consistency.pagx");

  // Load via FromFile
  auto docFromFile = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(docFromFile != nullptr);
  auto layerFromFile = pagx::LayerBuilder::Build(docFromFile.get());
  ASSERT_TRUE(layerFromFile != nullptr);

  // Load via FromXML (read raw bytes from the same file)
  auto fileData = tgfx::Data::MakeFromFile(pagxPath);
  ASSERT_TRUE(fileData != nullptr);
  auto docFromData =
      pagx::PAGXImporter::FromXML(fileData->bytes(), static_cast<size_t>(fileData->size()));
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
}

/**
 * Test case: PathData public API for path construction
 */
PAGX_TEST(PAGXTest, PathDataConstruction) {
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
PAGX_TEST(PAGXTest, PathDataForEach) {
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
 * Test case: PAGXDocument creation and XML export
 */
PAGX_TEST(PAGXTest, PAGXDocumentXMLExport) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_EQ(doc->width, 400.0f);
  EXPECT_EQ(doc->height, 300.0f);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->id = "layer1";
  layer->name = "Test Layer";

  auto group = doc->makeNode<pagx::Group>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position.x = 50;
  rect->position.y = 50;
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
PAGX_TEST(PAGXTest, PAGXDocumentRoundTrip) {
  auto doc1 = pagx::PAGXDocument::Make(200, 150);
  ASSERT_TRUE(doc1 != nullptr);

  auto layer = doc1->makeNode<pagx::Layer>();
  layer->name = "TestLayer";

  auto rect = doc1->makeNode<pagx::Rectangle>();
  rect->position.x = 50;
  rect->position.y = 50;
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
 * Test case: Font and Glyph node creation
 */
PAGX_TEST(PAGXTest, FontGlyphNodes) {
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
PAGX_TEST(PAGXTest, GlyphRunHorizontal) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>("testFont");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2, 1};
  glyphRun->y = 50;
  glyphRun->xOffsets = {10, 60, 110};

  EXPECT_EQ(glyphRun->nodeType(), pagx::NodeType::GlyphRun);
  EXPECT_EQ(glyphRun->font, font);
  EXPECT_EQ(glyphRun->glyphs.size(), 3u);
  EXPECT_EQ(glyphRun->xOffsets.size(), 3u);
  EXPECT_FLOAT_EQ(glyphRun->y, 50.0f);
}

/**
 * Test case: GlyphRun node with point positions
 */
PAGX_TEST(PAGXTest, GlyphRunPointPositions) {
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
 * Test case: GlyphRun node with rotations and scales
 */
PAGX_TEST(PAGXTest, GlyphRunTransforms) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto font = doc->makeNode<pagx::Font>("testFont");
  auto glyphRun = doc->makeNode<pagx::GlyphRun>();
  glyphRun->font = font;
  glyphRun->glyphs = {1, 2};
  glyphRun->positions = {{10, 20}, {60, 80}};
  glyphRun->rotations = {0, 45};
  glyphRun->scales = {{1, 1}, {1.5f, 1.5f}};

  EXPECT_EQ(glyphRun->nodeType(), pagx::NodeType::GlyphRun);
  EXPECT_EQ(glyphRun->rotations.size(), 2u);
  EXPECT_FLOAT_EQ(glyphRun->rotations[1], 45.0f);
  EXPECT_EQ(glyphRun->scales.size(), 2u);
  EXPECT_FLOAT_EQ(glyphRun->scales[1].x, 1.5f);
}

/**
 * Test case: Precomposed text rendering with real text and embedded font.
 * Creates a document with English, mixed Chinese-English text at different sizes, shapes and embeds
 * fonts, exports to XML, reimports and renders from embedded glyphs.
 */
PAGX_TEST(PAGXTest, PrecomposedTextRender) {
  auto doc = pagx::PAGXDocument::Make(240, 140);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->contents.push_back(
      MakeCenteredTextGroup(doc.get(), "Hello PAGX", 36, 120, 35, {0.2f, 0.2f, 0.8f, 1.0f}));
  layer->contents.push_back(MakeCenteredTextGroup(doc.get(), "\xe4\xbd\xa0\xe5\xa5\xbd World", 28,
                                                  120, 80, {0.1f, 0.6f, 0.2f, 1.0f}));
  layer->contents.push_back(
      MakeCenteredTextGroup(doc.get(), "Embedded Font", 18, 120, 115, {0.5f, 0.5f, 0.5f, 1.0f}));
  doc->layers.push_back(layer);

  pagx::TextLayout textLayout;
  textLayout.addFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());
  pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap, layoutResult.textOrder);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  auto pagxPath = SavePAGXFile(xml, "PAGXTest/PrecomposedTextRender.pagx");

  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);

  auto tgfxLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 240, 140);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PrecomposedTextRender"));
}

/**
 * Test case: Font and GlyphRun XML round-trip
 */
PAGX_TEST(PAGXTest, FontGlyphRoundTrip) {
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
  glyphRun->xOffsets = {10, 60};
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

static void TestPAGXDirectory(tgfx::Context* context, const std::string& directory,
                              const std::string& subdir = "") {
  std::vector<std::string> files = {};
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());

  pagx::TextLayout textLayout;
  textLayout.addFallbackTypefaces(GetFallbackTypefaces());

  auto baselinePrefix = subdir.empty() ? std::string("PAGXTest/") : ("PAGXTest/" + subdir + "/");

  for (const auto& filePath : files) {
    auto baseName = std::filesystem::path(filePath).stem().string();

    auto doc = pagx::PAGXImporter::FromFile(filePath);
    if (!doc) {
      ADD_FAILURE() << "Failed to load: " << filePath;
      continue;
    }
    if (!doc->errors.empty()) {
      std::string errorLog;
      for (const auto& error : doc->errors) {
        errorLog += "\n  " + error;
      }
      ADD_FAILURE() << "Parse errors in " << baseName << ":" << errorLog;
    }

    auto layer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
    if (!layer) {
      ADD_FAILURE() << "Failed to build layer: " << filePath;
      continue;
    }

    auto surface =
        Surface::Make(context, static_cast<int>(doc->width), static_cast<int>(doc->height));
    if (!surface) {
      ADD_FAILURE() << "Failed to create surface for: " << filePath;
      continue;
    }
    DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);

    EXPECT_TRUE(Baseline::Compare(surface, baselinePrefix + baseName)) << baseName;
  }
}

/**
 * Test all PAGX sample files in spec/samples directory.
 * Renders each sample and compares with baseline screenshots.
 */
PAGX_TEST(PAGXTest, SampleFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("spec/samples"));
}

/**
 * Test all text-related PAGX files in resources/text directory.
 */
PAGX_TEST(PAGXTest, TextFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("resources/text"));
}

/**
 * Test case: Verify data-* custom attributes round-trip on Document, Layer, and Element nodes.
 */
PAGX_TEST(PAGXTest, CustomDataRoundTrip) {
  // Step 1: Create document with customData on various nodes.
  auto doc = pagx::PAGXDocument::Make(200, 100);
  ASSERT_TRUE(doc != nullptr);

  doc->customData["app-name"] = "test-tool";
  doc->customData["version"] = "2.0";

  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "TestLayer";
  layer->customData["layer-role"] = "background";
  layer->customData["export"] = "true";

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 80};
  rect->customData["figma-node"] = "123:456";

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>("red");
  solid->color = {1, 0, 0, 1};
  solid->customData["source"] = "design-token";
  fill->color = solid;
  fill->customData["auto-generated"] = "true";

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  // Step 2: Export to XML.
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  EXPECT_NE(xml.find("data-app-name=\"test-tool\""), std::string::npos);
  EXPECT_NE(xml.find("data-version=\"2.0\""), std::string::npos);
  EXPECT_NE(xml.find("data-layer-role=\"background\""), std::string::npos);
  EXPECT_NE(xml.find("data-export=\"true\""), std::string::npos);
  EXPECT_NE(xml.find("data-figma-node=\"123:456\""), std::string::npos);
  EXPECT_NE(xml.find("data-auto-generated=\"true\""), std::string::npos);
  EXPECT_NE(xml.find("data-source=\"design-token\""), std::string::npos);

  // Step 3: Re-import from XML.
  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_TRUE(doc2->errors.empty());

  // Step 4: Verify Document customData.
  ASSERT_EQ(doc2->customData.size(), 2u);
  EXPECT_EQ(doc2->customData.at("app-name"), "test-tool");
  EXPECT_EQ(doc2->customData.at("version"), "2.0");

  // Step 5: Verify Layer customData.
  ASSERT_EQ(doc2->layers.size(), 1u);
  auto* layer2 = doc2->layers[0];
  ASSERT_EQ(layer2->customData.size(), 2u);
  EXPECT_EQ(layer2->customData.at("layer-role"), "background");
  EXPECT_EQ(layer2->customData.at("export"), "true");

  // Step 6: Verify Element customData.
  ASSERT_GE(layer2->contents.size(), 2u);
  auto* rect2 = layer2->contents[0];
  ASSERT_EQ(rect2->nodeType(), pagx::NodeType::Rectangle);
  ASSERT_EQ(rect2->customData.size(), 1u);
  EXPECT_EQ(rect2->customData.at("figma-node"), "123:456");

  auto* fill2 = layer2->contents[1];
  ASSERT_EQ(fill2->nodeType(), pagx::NodeType::Fill);
  ASSERT_EQ(fill2->customData.size(), 1u);
  EXPECT_EQ(fill2->customData.at("auto-generated"), "true");

  // Step 7: Verify SolidColor resource customData.
  auto* solid2 = doc2->findNode<pagx::SolidColor>("red");
  ASSERT_TRUE(solid2 != nullptr);
  ASSERT_EQ(solid2->customData.size(), 1u);
  EXPECT_EQ(solid2->customData.at("source"), "design-token");

  // Step 8: Re-export and re-import to verify consistency.
  std::string xml2 = pagx::PAGXExporter::ToXML(*doc2);
  ASSERT_FALSE(xml2.empty());
  auto doc3 = pagx::PAGXImporter::FromXML(xml2);
  ASSERT_TRUE(doc3 != nullptr);
  EXPECT_TRUE(doc3->errors.empty());
  EXPECT_EQ(doc3->customData, doc2->customData);
  ASSERT_EQ(doc3->layers.size(), 1u);
  ASSERT_GE(doc3->layers[0]->contents.size(), 2u);
  EXPECT_EQ(doc3->layers[0]->customData, layer2->customData);
  EXPECT_EQ(doc3->layers[0]->contents[0]->customData, rect2->customData);
  EXPECT_EQ(doc3->layers[0]->contents[1]->customData, fill2->customData);
  auto* solid3 = doc3->findNode<pagx::SolidColor>("red");
  ASSERT_TRUE(solid3 != nullptr);
  EXPECT_EQ(solid3->customData, solid2->customData);
}

/**
 * Test case: Verify customData round-trip for LayerStyle, LayerFilter, sub-nodes (ColorStop,
 * RangeSelector), resource nodes (Composition, LinearGradient, PathData, Image), nested children,
 * XML special characters, and empty customData preservation.
 */
PAGX_TEST(PAGXTest, CustomDataEdgeCases) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  ASSERT_TRUE(doc != nullptr);

  // LayerStyle with customData.
  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "StyledLayer";
  auto shadow = doc->makeNode<pagx::DropShadowStyle>();
  shadow->offsetX = 5;
  shadow->offsetY = 5;
  shadow->color = {0, 0, 0, 0.5f};
  shadow->customData["priority"] = "high";
  layer->styles.push_back(shadow);

  // LayerFilter with customData.
  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 10;
  blur->blurY = 10;
  blur->customData["source"] = "user-input";
  layer->filters.push_back(blur);

  // LinearGradient resource with ColorStop sub-nodes.
  auto gradient = doc->makeNode<pagx::LinearGradient>("grad1");
  gradient->startPoint = {0, 0};
  gradient->endPoint = {100, 0};
  gradient->customData["origin"] = "figma";
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0;
  stop1->color = {1, 0, 0, 1};
  stop1->customData["role"] = "start";
  auto stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1;
  stop2->color = {0, 0, 1, 1};
  stop2->customData["role"] = "end";
  gradient->colorStops.push_back(stop1);
  gradient->colorStops.push_back(stop2);
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = gradient;
  layer->contents.push_back(fill);

  // Ellipse with empty customData — should produce no data-* attributes.
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {50, 50};
  layer->contents.push_back(ellipse);

  // XML special characters in customData value.
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 60};
  rect->customData["formula"] = "a<b&c>d\"e";
  layer->contents.push_back(rect);

  // Nested child layer.
  auto child = doc->makeNode<pagx::Layer>();
  child->name = "ChildLayer";
  child->customData["depth"] = "1";
  layer->children.push_back(child);

  // Composition resource with customData.
  auto comp = doc->makeNode<pagx::Composition>("comp1");
  comp->width = 100;
  comp->height = 100;
  comp->customData["scene"] = "intro";
  auto compLayer = doc->makeNode<pagx::Layer>();
  compLayer->name = "CompLayer";
  comp->layers.push_back(compLayer);

  doc->layers.push_back(layer);

  // Export.
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  // Verify special characters are XML-escaped.
  EXPECT_NE(xml.find("data-formula=\"a&lt;b&amp;c>d&quot;e\""), std::string::npos);

  // Verify empty customData produces no data-* for Ellipse.
  auto ellipsePos = xml.find("<Ellipse");
  ASSERT_NE(ellipsePos, std::string::npos);
  auto ellipseEnd = xml.find("/>", ellipsePos);
  ASSERT_NE(ellipseEnd, std::string::npos);
  std::string ellipseTag = xml.substr(ellipsePos, ellipseEnd - ellipsePos);
  EXPECT_EQ(ellipseTag.find("data-"), std::string::npos);

  // Re-import.
  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_TRUE(doc2->errors.empty());

  // Verify LayerStyle customData.
  ASSERT_EQ(doc2->layers.size(), 1u);
  auto* layer2 = doc2->layers[0];
  ASSERT_EQ(layer2->styles.size(), 1u);
  ASSERT_EQ(layer2->styles[0]->customData.count("priority"), 1u);
  EXPECT_EQ(layer2->styles[0]->customData.at("priority"), "high");

  // Verify LayerFilter customData.
  ASSERT_EQ(layer2->filters.size(), 1u);
  ASSERT_EQ(layer2->filters[0]->customData.count("source"), 1u);
  EXPECT_EQ(layer2->filters[0]->customData.at("source"), "user-input");

  // Verify LinearGradient resource customData.
  auto* grad2 = doc2->findNode<pagx::LinearGradient>("grad1");
  ASSERT_TRUE(grad2 != nullptr);
  ASSERT_EQ(grad2->customData.count("origin"), 1u);
  EXPECT_EQ(grad2->customData.at("origin"), "figma");

  // Verify ColorStop customData.
  ASSERT_EQ(grad2->colorStops.size(), 2u);
  ASSERT_EQ(grad2->colorStops[0]->customData.count("role"), 1u);
  EXPECT_EQ(grad2->colorStops[0]->customData.at("role"), "start");
  ASSERT_EQ(grad2->colorStops[1]->customData.count("role"), 1u);
  EXPECT_EQ(grad2->colorStops[1]->customData.at("role"), "end");

  // Verify empty customData.
  ASSERT_GE(layer2->contents.size(), 3u);
  EXPECT_TRUE(layer2->contents[1]->customData.empty());

  // Verify XML special characters survived round-trip.
  ASSERT_EQ(layer2->contents[2]->customData.count("formula"), 1u);
  EXPECT_EQ(layer2->contents[2]->customData.at("formula"), "a<b&c>d\"e");

  // Verify nested child layer customData.
  ASSERT_EQ(layer2->children.size(), 1u);
  ASSERT_EQ(layer2->children[0]->customData.count("depth"), 1u);
  EXPECT_EQ(layer2->children[0]->customData.at("depth"), "1");

  // Verify Composition resource customData.
  auto* comp2 = doc2->findNode<pagx::Composition>("comp1");
  ASSERT_TRUE(comp2 != nullptr);
  ASSERT_EQ(comp2->customData.count("scene"), 1u);
  EXPECT_EQ(comp2->customData.at("scene"), "intro");

  // Re-export and re-import to verify consistency.
  std::string xml2 = pagx::PAGXExporter::ToXML(*doc2);
  auto doc3 = pagx::PAGXImporter::FromXML(xml2);
  ASSERT_TRUE(doc3 != nullptr);
  EXPECT_TRUE(doc3->errors.empty());
  ASSERT_EQ(doc3->layers.size(), 1u);
  auto* layer3 = doc3->layers[0];
  ASSERT_EQ(layer3->styles.size(), 1u);
  EXPECT_EQ(layer3->styles[0]->customData, layer2->styles[0]->customData);
  ASSERT_EQ(layer3->filters.size(), 1u);
  EXPECT_EQ(layer3->filters[0]->customData, layer2->filters[0]->customData);
  auto* grad3 = doc3->findNode<pagx::LinearGradient>("grad1");
  ASSERT_TRUE(grad3 != nullptr);
  ASSERT_EQ(grad3->colorStops.size(), 2u);
  EXPECT_EQ(grad3->colorStops[0]->customData, grad2->colorStops[0]->customData);
  EXPECT_EQ(grad3->colorStops[1]->customData, grad2->colorStops[1]->customData);
  ASSERT_GE(layer3->contents.size(), 3u);
  EXPECT_EQ(layer3->contents[2]->customData, layer2->contents[2]->customData);
  ASSERT_EQ(layer3->children.size(), 1u);
  EXPECT_EQ(layer3->children[0]->customData, layer2->children[0]->customData);
}

/**
 * Test case: Verify customData round-trip for TextModifier with RangeSelector sub-node.
 */
PAGX_TEST(PAGXTest, CustomDataTextModifierRangeSelector) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  ASSERT_TRUE(doc != nullptr);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "TextLayer";

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello";
  text->fontSize = 24;
  layer->contents.push_back(text);

  auto modifier = doc->makeNode<pagx::TextModifier>();
  modifier->customData["effect"] = "fade-in";
  auto selector = doc->makeNode<pagx::RangeSelector>();
  selector->start = 0;
  selector->end = 1;
  selector->customData["timing"] = "ease";
  modifier->selectors.push_back(selector);
  layer->contents.push_back(modifier);

  doc->layers.push_back(layer);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  EXPECT_NE(xml.find("data-effect=\"fade-in\""), std::string::npos);
  EXPECT_NE(xml.find("data-timing=\"ease\""), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_TRUE(doc2->errors.empty());
  ASSERT_EQ(doc2->layers.size(), 1u);
  ASSERT_GE(doc2->layers[0]->contents.size(), 2u);
  auto* mod2 = doc2->layers[0]->contents[1];
  ASSERT_EQ(mod2->nodeType(), pagx::NodeType::TextModifier);
  ASSERT_EQ(mod2->customData.count("effect"), 1u);
  EXPECT_EQ(mod2->customData.at("effect"), "fade-in");
  auto* modifier2 = static_cast<pagx::TextModifier*>(mod2);
  ASSERT_EQ(modifier2->selectors.size(), 1u);
  ASSERT_EQ(modifier2->selectors[0]->customData.count("timing"), 1u);
  EXPECT_EQ(modifier2->selectors[0]->customData.at("timing"), "ease");
}

PAGX_TEST(PAGXTest, CustomDataSVGRootElement) {
  // Verify that data-* attributes on the root <svg> element are parsed into PAGXDocument.
  std::string svg = R"(
    <svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"
         data-source="figma" data-version="3">
      <rect x="0" y="0" width="50" height="50" fill="red" data-role="bg"/>
    </svg>
  )";
  auto doc = pagx::SVGImporter::ParseString(svg);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty());

  // Verify document-level customData from root <svg>.
  ASSERT_EQ(doc->customData.count("source"), 1u);
  EXPECT_EQ(doc->customData.at("source"), "figma");
  ASSERT_EQ(doc->customData.count("version"), 1u);
  EXPECT_EQ(doc->customData.at("version"), "3");

  // Verify child layer customData is also parsed (SVG rect becomes a Layer with data-* on it).
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->customData.count("role"), 1u);
  EXPECT_EQ(doc->layers[0]->customData.at("role"), "bg");

  // Export to PAGX XML and re-import to verify round-trip.
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_TRUE(doc2->errors.empty());
  EXPECT_EQ(doc2->customData, doc->customData);
  ASSERT_EQ(doc2->layers.size(), 1u);
  EXPECT_EQ(doc2->layers[0]->customData, doc->layers[0]->customData);
}

PAGX_TEST(PAGXTest, CustomDataKeyValidation) {
  // Valid keys.
  EXPECT_TRUE(pagx::Node::IsValidCustomDataKey("role"));
  EXPECT_TRUE(pagx::Node::IsValidCustomDataKey("figma-node"));
  EXPECT_TRUE(pagx::Node::IsValidCustomDataKey("v2"));
  EXPECT_TRUE(pagx::Node::IsValidCustomDataKey("a"));
  EXPECT_TRUE(pagx::Node::IsValidCustomDataKey("abc-123-def"));

  // Invalid keys.
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey(""));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("-leading"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("trailing-"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("-"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("UPPER"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("has space"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("under_score"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("dot.name"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("a<b"));
  EXPECT_FALSE(pagx::Node::IsValidCustomDataKey("a\"b"));

  // Verify invalid keys are rejected during PAGX import.
  // Note: XML attribute names with uppercase (data-INVALID) and underscores (data-has_underscore)
  // are valid XML, but should be rejected by our key validation.
  std::string xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"100\" height=\"100\" data-valid=\"yes\" data-INVALID=\"no\">\n"
      "  <Layer name=\"Test\" data-ok=\"1\" data-Bad=\"2\" data-has_underscore=\"3\"/>\n"
      "</pagx>";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);

  // Only valid keys should survive import.
  ASSERT_EQ(doc->customData.count("valid"), 1u);
  EXPECT_EQ(doc->customData.at("valid"), "yes");
  EXPECT_EQ(doc->customData.count("INVALID"), 0u);

  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->customData.count("ok"), 1u);
  EXPECT_EQ(doc->layers[0]->customData.at("ok"), "1");
  EXPECT_EQ(doc->layers[0]->customData.count("Bad"), 0u);
  EXPECT_EQ(doc->layers[0]->customData.count("has_underscore"), 0u);
}

/**
 * Test case: Verify resource cross-references resolve regardless of XML declaration order.
 * Resources declared later in <Resources> should be referenceable by earlier resources via '@id'.
 */
PAGX_TEST(PAGXTest, ResourceCrossReference) {
  // ImagePattern references an Image that is declared AFTER it in the XML.
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Resources>
    <ImagePattern id="pattern1" image="@img1"/>
    <Image id="img1" source="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="/>
  </Resources>
  <Layer>
    <Rectangle size="100,100"/>
    <Fill color="@pattern1"/>
  </Layer>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty())
      << "Unexpected errors: " << (doc->errors.empty() ? "" : doc->errors[0]);

  auto* pattern = doc->findNode<pagx::ImagePattern>("pattern1");
  ASSERT_TRUE(pattern != nullptr);
  EXPECT_TRUE(pattern->image != nullptr)
      << "ImagePattern should resolve forward reference to Image";

  auto* image = doc->findNode<pagx::Image>("img1");
  ASSERT_TRUE(image != nullptr);
  EXPECT_EQ(pattern->image, image);
  EXPECT_TRUE(image->data != nullptr);
}

/**
 * Test case: Verify Glyph nodes inside Font can reference Image resources declared after the Font.
 */
PAGX_TEST(PAGXTest, ResourceCrossReferenceGlyphImage) {
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Resources>
    <Font id="font1">
      <Glyph image="@emoji1" advance="16"/>
    </Font>
    <Image id="emoji1" source="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="/>
  </Resources>
  <Layer>
    <Text text="A" fontFamily="Test" fontSize="16">
      <GlyphRun font="@font1" fontSize="16" glyphs="1" positions="0,0"/>
    </Text>
    <Fill color="#000000"/>
  </Layer>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty())
      << "Unexpected errors: " << (doc->errors.empty() ? "" : doc->errors[0]);

  auto* font = doc->findNode<pagx::Font>("font1");
  ASSERT_TRUE(font != nullptr);
  ASSERT_EQ(font->glyphs.size(), 1u);
  EXPECT_TRUE(font->glyphs[0]->image != nullptr)
      << "Glyph should resolve forward reference to Image";

  auto* image = doc->findNode<pagx::Image>("emoji1");
  ASSERT_TRUE(image != nullptr);
  EXPECT_EQ(font->glyphs[0]->image, image);
}

/**
 * Test case: Verify Layer composition attribute can reference a Composition declared after it.
 */
PAGX_TEST(PAGXTest, ResourceCrossReferenceComposition) {
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer composition="@comp1">
    <Rectangle size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <Composition id="comp1" width="100" height="100">
      <Layer>
        <Rectangle size="50,50"/>
        <Fill color="#00FF00"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty())
      << "Unexpected errors: " << (doc->errors.empty() ? "" : doc->errors[0]);

  ASSERT_GE(doc->layers.size(), 1u);
  auto* layer = doc->layers[0];
  EXPECT_TRUE(layer->composition != nullptr)
      << "Layer should resolve forward reference to Composition";

  auto* comp = doc->findNode<pagx::Composition>("comp1");
  ASSERT_TRUE(comp != nullptr);
  EXPECT_EQ(layer->composition, comp);
  EXPECT_FLOAT_EQ(comp->width, 100);
  EXPECT_FLOAT_EQ(comp->height, 100);
  ASSERT_EQ(comp->layers.size(), 1u);
}

/**
 * Test case: Verify multiple resources with mutual cross-references all resolve correctly.
 * A gradient references an image pattern which references an image — all in reverse declaration
 * order.
 */
PAGX_TEST(PAGXTest, ResourceCrossReferenceChain) {
  // Declare resources in reverse dependency order: PathData -> Font -> Image.
  // Font's Glyph references PathData declared after it, and also references Image declared after it.
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Resources>
    <Font id="font1">
      <Glyph path="@path1" advance="10"/>
      <Glyph image="@img1" advance="16"/>
    </Font>
    <PathData id="path1" data="M0 0 L10 0 L10 10 L0 10 Z"/>
    <Image id="img1" source="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="/>
  </Resources>
  <Layer>
    <Text text="AB" fontFamily="Test" fontSize="16">
      <GlyphRun font="@font1" fontSize="16" glyphs="1,2" positions="0,0;16,0"/>
    </Text>
    <Fill color="#000000"/>
  </Layer>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty())
      << "Unexpected errors: " << (doc->errors.empty() ? "" : doc->errors[0]);

  auto* font = doc->findNode<pagx::Font>("font1");
  ASSERT_TRUE(font != nullptr);
  ASSERT_EQ(font->glyphs.size(), 2u);

  // First glyph references PathData declared after Font.
  EXPECT_TRUE(font->glyphs[0]->path != nullptr)
      << "Glyph should resolve forward reference to PathData";
  auto* pathData = doc->findNode<pagx::PathData>("path1");
  ASSERT_TRUE(pathData != nullptr);
  EXPECT_EQ(font->glyphs[0]->path, pathData);
  EXPECT_FALSE(pathData->isEmpty());

  // Second glyph references Image declared after Font.
  EXPECT_TRUE(font->glyphs[1]->image != nullptr)
      << "Glyph should resolve forward reference to Image";
  auto* image = doc->findNode<pagx::Image>("img1");
  ASSERT_TRUE(image != nullptr);
  EXPECT_EQ(font->glyphs[1]->image, image);
}

/**
 * Test all HTML-related PAGX files in resources/pagx_to_html directory.
 */
PAGX_TEST(PAGXTest, HtmlFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("resources/pagx_to_html"), "html");
}

/**
 * Generate a side-by-side comparison page: PAGX native rendering vs HTML rendering.
 * Run: PAGFullTest --gtest_filter=PAGXTest.GenerateComparisonPage
 * Then: open test/out/PAGXHtmlTest/comparison.html
 */
PAGX_TEST(PAGXTest, GenerateComparisonPage) {
  auto directory = ProjectPath::Absolute("resources/pagx_to_html");
  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);

  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_FALSE(files.empty());

  pagx::TextLayout textLayout;
  textLayout.addFallbackTypefaces(GetFallbackTypefaces());

  // Build comparison page
  std::string page = R"(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>PAGX vs HTML Rendering Comparison</title>
<style>
body{font-family:-apple-system,Arial,sans-serif;background:#f1f5f9;margin:0;padding:20px}
h1{text-align:center;color:#1e293b;margin-bottom:4px}
.sub{text-align:center;color:#94a3b8;font-size:14px;margin-bottom:24px}
.card{background:white;border-radius:12px;box-shadow:0 2px 8px rgba(0,0,0,.08);margin-bottom:20px;overflow:hidden}
.hd{padding:12px 16px;border-bottom:1px solid #e2e8f0;display:flex;align-items:center;justify-content:space-between}
.hd h3{margin:0;font-size:14px;color:#334155}
.sz{font-size:11px;padding:2px 8px;border-radius:10px;background:#dcfce7;color:#166534}
.cmp{display:flex}
.cmp>div{flex:1;padding:12px;text-align:center}
.cmp>div:first-child{border-right:1px solid #e2e8f0}
.cmp label{display:block;font-size:11px;color:#94a3b8;margin-bottom:6px;font-weight:600;text-transform:uppercase;letter-spacing:.5px}
.cmp img{max-width:100%;height:auto;border:1px solid #e2e8f0;border-radius:4px;background:repeating-conic-gradient(#f0f0f0 0% 25%,white 0% 50%) 50%/16px 16px}
</style></head><body>
<h1>PAGX vs HTML Rendering Comparison</h1>
<p class="sub">Left: PAGX native rendering &nbsp;|&nbsp; Right: HTML rendering in browser</p>
)";

  int count = 0;
  for (const auto& filePath : files) {
    auto baseName = std::filesystem::path(filePath).stem().string();
    auto doc = pagx::PAGXImporter::FromFile(filePath);
    if (!doc || doc->width <= 0 || doc->height <= 0) {
      continue;
    }
    int w = static_cast<int>(doc->width);
    int h = static_cast<int>(doc->height);

    // Render PAGX native
    auto nativePng = outDir + "/" + baseName + "_pagx.png";
    auto layer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
    if (layer) {
      auto surface = tgfx::Surface::Make(context, w, h);
      if (surface) {
        tgfx::DisplayList displayList;
        displayList.root()->addChild(layer);
        displayList.render(surface.get(), false);
        tgfx::Bitmap bitmap(w, h, false, false);
        tgfx::Pixmap pixmap(bitmap);
        surface->readPixels(pixmap.info(), pixmap.writablePixels());
        auto data = tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
        if (data) {
          std::ofstream f(nativePng, std::ios::binary);
          f.write(reinterpret_cast<const char*>(data->data()),
                  static_cast<std::streamsize>(data->size()));
        }
      }
    }

    auto htmlPng = baseName + ".png";
    page += "<div class=\"card\" id=\"" + baseName + "\">\n";
    page += "  <div class=\"hd\"><h3>" + baseName + "</h3><span class=\"sz\">" + std::to_string(w) +
            "x" + std::to_string(h) + "</span></div>\n";
    page += "  <div class=\"cmp\">\n";
    page += "    <div><label>PAGX Native</label><img src=\"" + baseName + "_pagx.png\" width=\"" +
            std::to_string(w) + "\"></div>\n";
    page += "    <div><label>HTML (Browser)</label><img src=\"" + htmlPng + "\" width=\"" +
            std::to_string(w) + "\"></div>\n";
    page += "  </div>\n</div>\n";
    count++;
  }

  page += "<p class=\"sub\">" + std::to_string(count) + " files compared</p></body></html>";

  auto pagePath = outDir + "/comparison.html";
  std::ofstream f(pagePath, std::ios::binary);
  f.write(page.data(), static_cast<std::streamsize>(page.size()));
  std::cout << "\nComparison page: " << pagePath << std::endl;
  std::cout << "Open: open \"" << pagePath << "\"" << std::endl;
}

}  // namespace pag