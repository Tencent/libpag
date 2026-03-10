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
#include "pagx/SVGExporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
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
PAGX_TEST(PAGXTest, PAGXDocumentRoundTrip) {
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

static void TestPAGXDirectory(tgfx::Context* context, const std::string& directory) {
  std::vector<std::string> files = {};
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());

  pagx::TextLayout textLayout;
  textLayout.addFallbackTypefaces(GetFallbackTypefaces());

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

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + baseName)) << baseName;
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

static std::string SaveSVGFile(const std::string& svg, const std::string& key) {
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  if (file) {
    file.write(svg.data(), static_cast<std::streamsize>(svg.size()));
  }
  return outPath;
}

/**
 * Test SVG export of a simple rectangle with solid fill.
 */
PAGX_TEST(PAGXTest, SVGExport_Rectangle) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {160, 120};
  rect->roundness = 10;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_NE(svg.find("width=\"160\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"120\""), std::string::npos);
  EXPECT_NE(svg.find("rx=\"10\""), std::string::npos);
  EXPECT_NE(svg.find("fill=\"#"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_rectangle.svg");
}

/**
 * Test SVG export of ellipse and circle elements.
 */
PAGX_TEST(PAGXTest, SVGExport_Ellipse) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  // Circle (equal width/height)
  auto layer1 = doc->makeNode<pagx::Layer>();
  auto circle = doc->makeNode<pagx::Ellipse>();
  circle->center = {80, 100};
  circle->size = {100, 100};
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill1->color = solid1;
  layer1->contents.push_back(circle);
  layer1->contents.push_back(fill1);
  doc->layers.push_back(layer1);

  // Ellipse (different width/height)
  auto layer2 = doc->makeNode<pagx::Layer>();
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->center = {220, 100};
  ellipse->size = {120, 80};
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill2->color = solid2;
  layer2->contents.push_back(ellipse);
  layer2->contents.push_back(fill2);
  doc->layers.push_back(layer2);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<circle"), std::string::npos);
  EXPECT_NE(svg.find("<ellipse"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_ellipse.svg");
}

/**
 * Test SVG export of path element.
 */
PAGX_TEST(PAGXTest, SVGExport_Path) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  *pathData = pagx::PathDataFromSVGString("M10 80 C40 10 65 10 95 80 S150 150 180 80");
  path->data = pathData;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.7f, 0.3f, 1.0f};
  stroke->color = solid;
  stroke->width = 3.0f;
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = nullptr;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("d=\""), std::string::npos);
  EXPECT_NE(svg.find("stroke="), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_path.svg");
}

/**
 * Test SVG export of text element.
 */
PAGX_TEST(PAGXTest, SVGExport_Text) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello SVG Export";
  text->position = {150, 50};
  text->fontFamily = "Arial";
  text->fontSize = 24;
  text->textAnchor = pagx::TextAnchor::Center;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.1f, 0.1f, 0.1f, 1.0f};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("Hello SVG Export"), std::string::npos);
  EXPECT_NE(svg.find("font-family=\"Arial\""), std::string::npos);
  EXPECT_NE(svg.find("text-anchor=\"middle\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_text.svg");
}

/**
 * Test SVG export with linear gradient.
 */
PAGX_TEST(PAGXTest, SVGExport_LinearGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {180, 180};

  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<linearGradient"), std::string::npos);
  EXPECT_NE(svg.find("<stop"), std::string::npos);
  EXPECT_NE(svg.find("url(#grad"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_linear_gradient.svg");
}

/**
 * Test SVG export with radial gradient.
 */
PAGX_TEST(PAGXTest, SVGExport_RadialGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->center = {100, 100};
  ellipse->size = {160, 160};

  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {100, 100};
  grad->radius = 80;
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 1.0f, 0.0f, 1.0f};
  auto stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.5f, 0.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<radialGradient"), std::string::npos);
  EXPECT_NE(svg.find("url(#grad"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_radial_gradient.svg");
}

/**
 * Test SVG export with layer transform (translate, scale, rotate via matrix).
 */
PAGX_TEST(PAGXTest, SVGExport_Transform) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Translate(50, 50);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.3f, 0.1f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("transform="), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_transform.svg");
}

/**
 * Test SVG export with opacity.
 */
PAGX_TEST(PAGXTest, SVGExport_Opacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->alpha = 0.5f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.5f, 1.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("opacity=\"0.5\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_opacity.svg");
}

/**
 * Test SVG export with stroke properties (width, cap, join, dashes).
 */
PAGX_TEST(PAGXTest, SVGExport_StrokeProperties) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(20, 50);
  pathData->lineTo(280, 50);
  path->data = pathData;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.0f, 0.5f, 1.0f};
  stroke->color = solid;
  stroke->width = 6.0f;
  stroke->cap = pagx::LineCap::Round;
  stroke->join = pagx::LineJoin::Round;
  stroke->dashes = {15, 8, 5, 8};
  stroke->dashOffset = 3.0f;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-width=\"6\""), std::string::npos);
  EXPECT_NE(svg.find("stroke-linecap=\"round\""), std::string::npos);
  EXPECT_NE(svg.find("stroke-linejoin=\"round\""), std::string::npos);
  EXPECT_NE(svg.find("stroke-dasharray="), std::string::npos);
  EXPECT_NE(svg.find("stroke-dashoffset=\"3\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_stroke_properties.svg");
}

/**
 * Test SVG export with fill rule (evenodd).
 */
PAGX_TEST(PAGXTest, SVGExport_FillRule) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  *pathData = pagx::PathDataFromSVGString(
      "M100,10 L40,198 L190,78 L10,78 L160,198 Z");
  path->data = pathData;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;
  fill->fillRule = pagx::FillRule::EvenOdd;
  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("fill-rule=\"evenodd\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_fill_rule.svg");
}

/**
 * Test SVG export with blur filter.
 */
PAGX_TEST(PAGXTest, SVGExport_BlurFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.4f, 0.8f, 1.0f};
  fill->color = solid;
  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 4;
  blur->blurY = 4;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(blur);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<filter"), std::string::npos);
  EXPECT_NE(svg.find("feGaussianBlur"), std::string::npos);
  EXPECT_NE(svg.find("filter=\"url(#"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_blur.svg");
}

/**
 * Test SVG export with drop shadow filter.
 */
PAGX_TEST(PAGXTest, SVGExport_DropShadow) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.2f, 0.2f, 1.0f};
  fill->color = solid;
  auto shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->offsetX = 4;
  shadow->offsetY = 4;
  shadow->blurX = 6;
  shadow->blurY = 6;
  shadow->color = {0, 0, 0, 0.5f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(shadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<filter"), std::string::npos);
  EXPECT_NE(svg.find("feGaussianBlur"), std::string::npos);
  EXPECT_NE(svg.find("feOffset"), std::string::npos);
  EXPECT_NE(svg.find("feMerge"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_drop_shadow.svg");
}

/**
 * Test SVG export with nested layers (group hierarchy).
 */
PAGX_TEST(PAGXTest, SVGExport_NestedLayers) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto parentLayer = doc->makeNode<pagx::Layer>();
  parentLayer->id = "parent";

  auto childLayer1 = doc->makeNode<pagx::Layer>();
  childLayer1->id = "child1";
  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->center = {75, 75};
  rect1->size = {100, 100};
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill1->color = solid1;
  childLayer1->contents.push_back(rect1);
  childLayer1->contents.push_back(fill1);

  auto childLayer2 = doc->makeNode<pagx::Layer>();
  childLayer2->id = "child2";
  childLayer2->matrix = pagx::Matrix::Translate(100, 100);
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->center = {75, 75};
  rect2->size = {100, 100};
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0.0f, 0.0f, 1.0f, 0.7f};
  fill2->color = solid2;
  childLayer2->contents.push_back(rect2);
  childLayer2->contents.push_back(fill2);

  parentLayer->children.push_back(childLayer1);
  parentLayer->children.push_back(childLayer2);
  doc->layers.push_back(parentLayer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("id=\"parent\""), std::string::npos);
  EXPECT_NE(svg.find("id=\"child1\""), std::string::npos);
  EXPECT_NE(svg.find("id=\"child2\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_nested.svg");
}

/**
 * Test SVG export with semi-transparent fill color.
 */
PAGX_TEST(PAGXTest, SVGExport_FillOpacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  fill->alpha = 0.6f;
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.5f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("fill-opacity="), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_fill_opacity.svg");
}

/**
 * Test SVG export with XML declaration option disabled.
 */
PAGX_TEST(PAGXTest, SVGExport_NoXmlDeclaration) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.xmlDeclaration = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_EQ(svg.find("<?xml"), std::string::npos);
  EXPECT_EQ(svg.find("<svg"), 0u);
}

/**
 * Test SVG export with ToFile method.
 */
PAGX_TEST(PAGXTest, SVGExport_ToFile) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto outPath = ProjectPath::Absolute("test/out/PAGXTest/svg_export_tofile.svg");
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  EXPECT_TRUE(pagx::SVGExporter::ToFile(*doc, outPath));
  EXPECT_TRUE(std::filesystem::exists(outPath));
  EXPECT_GT(std::filesystem::file_size(outPath), 0u);
}

/**
 * Test SVG round-trip: SVG → PAGX → SVG. Verify the re-exported SVG contains expected elements.
 */
PAGX_TEST(PAGXTest, SVGExport_RoundTrip) {
  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      continue;
    }
    if (doc->width <= 0 || doc->height <= 0) {
      continue;
    }

    auto svg = pagx::SVGExporter::ToSVG(*doc);
    EXPECT_FALSE(svg.empty()) << "Failed to export SVG for: " << baseName;
    EXPECT_NE(svg.find("<svg"), std::string::npos) << baseName;
    EXPECT_NE(svg.find("xmlns=\"http://www.w3.org/2000/svg\""), std::string::npos) << baseName;
    EXPECT_NE(svg.find("</svg>"), std::string::npos) << baseName;
    SaveSVGFile(svg, "PAGXTest/roundtrip_" + baseName + ".svg");
  }
}

/**
 * Test SVG export: empty document produces valid SVG.
 */
PAGX_TEST(PAGXTest, SVGExport_EmptyDocument) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("width=\"400\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"300\""), std::string::npos);
  EXPECT_NE(svg.find("</svg>"), std::string::npos);
}

/**
 * Test SVG export with inner shadow filter.
 */
PAGX_TEST(PAGXTest, SVGExport_InnerShadow) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.8f, 0.8f, 1.0f};
  fill->color = solid;
  auto innerShadow = doc->makeNode<pagx::InnerShadowFilter>();
  innerShadow->offsetX = 2;
  innerShadow->offsetY = 2;
  innerShadow->blurX = 4;
  innerShadow->blurY = 4;
  innerShadow->color = {0, 0, 0, 0.3f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(innerShadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<filter"), std::string::npos);
  EXPECT_NE(svg.find("feComposite"), std::string::npos);
  EXPECT_NE(svg.find("arithmetic"), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_inner_shadow.svg");
}

/**
 * Test SVG export with multiple gradient color stops.
 */
PAGX_TEST(PAGXTest, SVGExport_MultiStopGradient) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {150, 50};
  rect->size = {280, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 50};
  grad->endPoint = {290, 50};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 0.25f;
  s2->color = {1, 1, 0, 1};
  auto s3 = doc->makeNode<pagx::ColorStop>();
  s3->offset = 0.5f;
  s3->color = {0, 1, 0, 1};
  auto s4 = doc->makeNode<pagx::ColorStop>();
  s4->offset = 0.75f;
  s4->color = {0, 1, 1, 1};
  auto s5 = doc->makeNode<pagx::ColorStop>();
  s5->offset = 1.0f;
  s5->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2, s3, s4, s5};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Count the number of <stop elements
  size_t count = 0;
  size_t pos = 0;
  while ((pos = svg.find("<stop", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 5u);
  SaveSVGFile(svg, "PAGXTest/svg_export_multi_stop_gradient.svg");
}

/**
 * Test SVG export with combined fill and stroke on same shape.
 */
PAGX_TEST(PAGXTest, SVGExport_FillAndStroke) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {100, 100};
  rect->size = {120, 120};
  rect->roundness = 15;
  auto fill = doc->makeNode<pagx::Fill>();
  auto fillColor = doc->makeNode<pagx::SolidColor>();
  fillColor->color = {0.3f, 0.7f, 1.0f, 1.0f};
  fill->color = fillColor;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto strokeColor = doc->makeNode<pagx::SolidColor>();
  strokeColor->color = {0.0f, 0.3f, 0.6f, 1.0f};
  stroke->color = strokeColor;
  stroke->width = 3.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("fill=\"#"), std::string::npos);
  EXPECT_NE(svg.find("stroke=\"#"), std::string::npos);
  EXPECT_NE(svg.find("stroke-width=\"3\""), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_fill_and_stroke.svg");
}

/**
 * Test SVG export with layer position (x, y).
 */
PAGX_TEST(PAGXTest, SVGExport_LayerPosition) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 50;
  layer->y = 50;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->center = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.7f, 0.2f, 0.9f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("translate("), std::string::npos);
  SaveSVGFile(svg, "PAGXTest/svg_export_layer_position.svg");
}

}  // namespace pag
