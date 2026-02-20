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
#include "LayerBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "TextLayout.h"
#include "FontEmbedder.h"
#include "ShapedText.h"
#include "utils/StringParser.h"
#include "svg/SVGPathParser.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
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
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());

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
    pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap,
                               layoutResult.textOrder);

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
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());
  pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap,
                             layoutResult.textOrder);

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

/**
 * Test all PAGX sample files in spec/samples directory.
 * Renders each sample and compares with baseline screenshots.
 */
PAGX_TEST(PAGXTest, SampleFiles) {
  auto samplesDir = ProjectPath::Absolute("spec/samples");
  std::vector<std::string> sampleFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(samplesDir)) {
    if (entry.path().extension() == ".pagx") {
      sampleFiles.push_back(entry.path().string());
    }
  }
  std::sort(sampleFiles.begin(), sampleFiles.end());

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());

  for (const auto& filePath : sampleFiles) {
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

    textLayout.layout(doc.get());

    auto layer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
    if (!layer) {
      ADD_FAILURE() << "Failed to build layer: " << filePath;
      continue;
    }

    auto surface =
        Surface::Make(context, static_cast<int>(doc->width), static_cast<int>(doc->height));
    DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + baseName)) << baseName;
  }
}

static void TestPAGXDirectory(tgfx::Context* context, const std::string& resourceSubDir) {
  auto resourceDir = ProjectPath::Absolute("resources/" + resourceSubDir);
  std::vector<std::string> files = {};
  for (const auto& entry : std::filesystem::directory_iterator(resourceDir)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());

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

    textLayout.layout(doc.get());

    auto layer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
    if (!layer) {
      ADD_FAILURE() << "Failed to build layer: " << filePath;
      continue;
    }

    auto surface =
        Surface::Make(context, static_cast<int>(doc->width), static_cast<int>(doc->height));
    DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + baseName)) << baseName;
  }
}

/**
 * Test all text-related PAGX files in resources/text directory.
 */
PAGX_TEST(PAGXTest, TextFiles) {
  TestPAGXDirectory(context, "text");
}

}  // namespace pag
