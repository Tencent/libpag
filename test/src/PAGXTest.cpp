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
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include "pagx/FontConfig.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/TextLayoutParams.h"
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
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Padding.h"
#include "pagx/utils/StringParser.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#include "renderer/TextLayout.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

static pagx::Layer* MakeTextLayer(pagx::PAGXDocument* doc, const std::string& content,
                                  float fontSize, pagx::Color color) {
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = content;
  text->fontSize = fontSize;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  return layer;
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

  // Create FontConfig for text layout
  pagx::FontConfig svgFontConfig;
  svgFontConfig.addFallbackTypefaces(GetFallbackTypefaces());

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

    // Step 2: Layout and embed fonts
    doc->setFontConfig(svgFontConfig);
    doc->applyLayout();
    pagx::FontEmbedder().embed(doc.get());

    // Step 3: Export to XML and save as PAGX file
    std::string xml = pagx::PAGXExporter::ToXML(*doc);
    auto key = "svg_" + baseName;
    std::string pagxPath = SavePAGXFile(xml, "PAGXTest/" + key + ".pagx");

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
    EXPECT_TRUE(Baseline::Compare(pagxSurface, "PAGXTest/" + key)) << key;
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
  layer->width = 240;
  layer->height = 140;
  layer->layout = pagx::LayoutMode::Vertical;
  layer->alignment = pagx::Alignment::Center;
  layer->arrangement = pagx::Arrangement::SpaceEvenly;
  layer->children.push_back(MakeTextLayer(doc.get(), "Hello PAGX", 36, {0.2f, 0.2f, 0.8f, 1.0f}));
  layer->children.push_back(
      MakeTextLayer(doc.get(), "\xe4\xbd\xa0\xe5\xa5\xbd World", 28, {0.1f, 0.6f, 0.2f, 1.0f}));
  layer->children.push_back(
      MakeTextLayer(doc.get(), "Embedded Font", 18, {0.5f, 0.5f, 0.5f, 1.0f}));
  doc->layers.push_back(layer);

  pagx::FontConfig embedFontConfig;
  embedFontConfig.addFallbackTypefaces(GetFallbackTypefaces());
  doc->setFontConfig(embedFontConfig);
  doc->applyLayout();
  pagx::FontEmbedder().embed(doc.get());

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
 * Test case: TextBox with embedded GlyphRun applies inverse matrix correctly.
 * Verifies that embedded GlyphRun positions (in layout coordinates) are properly
 * transformed by the inverse matrix when rendered inside a TextBox.
 */
PAGX_TEST(PAGXTest, TextBoxEmbeddedGlyphRun) {
  // Create a document where Text is inside a TextBox (not standalone).
  // The Text has a non-zero position inside the TextBox, so the inverse matrix is non-trivial.
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->width = 300;
  layer->height = 100;
  doc->layers.push_back(layer);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 300;
  textBox->height = 100;
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello PAGX";
  text->fontFamily = "NotoSansSC";
  text->fontSize = 30;
  textBox->elements.push_back(text);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solidColor = doc->makeNode<pagx::SolidColor>();
  solidColor->color = {0.2f, 0.2f, 0.8f, 1.0f};
  fill->color = solidColor;
  textBox->elements.push_back(fill);
  layer->contents.push_back(textBox);

  // Step 1: Layout + embed fonts (writes GlyphRun in layout coordinates with bounds).
  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(GetFallbackTypefaces());
  doc->setFontConfig(fontConfig);
  doc->applyLayout();
  pagx::FontEmbedder().embed(doc.get());

  // Step 2: Export and reload to simulate loading a precomposed file.
  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  auto pagxPath = SavePAGXFile(xml, "PAGXTest/TextBoxEmbeddedGlyphRun.pagx");
  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);

  // Step 3: Render the reloaded document (BuildTextBlob should apply inverse matrix).
  auto tgfxLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 300, 100);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/TextBoxEmbeddedGlyphRun"));
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

static std::string TitleToKey(const std::string& title) {
  // Convert to lowercase with underscores, ignoring parenthesized content (e.g., "(Composition)")
  std::string key;
  int parenDepth = 0;
  for (char ch : title) {
    if (ch == '(') {
      parenDepth++;
      continue;
    }
    if (ch == ')') {
      if (parenDepth > 0) {
        parenDepth--;
      }
      continue;
    }
    if (parenDepth > 0) {
      continue;
    }
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      key += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    } else if (!key.empty() && key.back() != '_') {
      key += '_';
    }
  }
  // Trim trailing underscore
  if (!key.empty() && key.back() == '_') {
    key.pop_back();
  }
  return key;
}

static std::vector<std::pair<std::string, std::string>> ExtractMarkdownExamples(
    const std::string& markdownPath) {
  std::vector<std::pair<std::string, std::string>> examples;
  std::ifstream file(markdownPath);
  if (!file.is_open()) {
    return examples;
  }

  std::string currentTitle;
  bool inCodeBlock = false;
  std::string codeContent;
  std::unordered_map<std::string, int> keyCounts;

  std::string line;
  while (std::getline(file, line)) {
    if (!inCodeBlock) {
      // Track the most recent ### heading
      if (line.size() > 4 && line.substr(0, 4) == "### ") {
        currentTitle = line.substr(4);
      }
      // Detect start of xml code block
      if (line.size() >= 5 && line.substr(0, 5) == "```xm") {
        inCodeBlock = true;
        codeContent.clear();
      }
    } else {
      if (line == "```") {
        inCodeBlock = false;
        // Only keep complete PAGX documents (skip code snippets)
        auto trimmed = codeContent;
        auto pos = trimmed.find_first_not_of(" \t\n\r");
        if (pos != std::string::npos) {
          trimmed = trimmed.substr(pos);
        }
        if (trimmed.substr(0, 5) == "<pagx" && !currentTitle.empty()) {
          auto baseKey = TitleToKey(currentTitle);
          auto count = ++keyCounts[baseKey];
          auto key = count > 1 ? baseKey + "_" + std::to_string(count) : baseKey;
          examples.emplace_back(key, codeContent);
        }
      } else {
        if (!codeContent.empty()) {
          codeContent += "\n";
        }
        codeContent += line;
      }
    }
  }
  return examples;
}

static void TestMarkdownExamples(tgfx::Context* context, const std::string& markdownPath,
                                 const std::string& prefix = "") {
  auto examples = ExtractMarkdownExamples(markdownPath);
  ASSERT_FALSE(examples.empty()) << "No examples found in: " << markdownPath;

  // Ensure output directory exists, then copy test image for examples referencing "avatar.jpg".
  auto outDir = std::filesystem::path(ProjectPath::Absolute("test/out/PAGXTest"));
  std::filesystem::create_directories(outDir);
  std::filesystem::copy_file(ProjectPath::Absolute("resources/apitest/imageReplacement.jpg"),
                             outDir / "avatar.jpg",
                             std::filesystem::copy_options::overwrite_existing);

  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(GetFallbackTypefaces());

  for (const auto& [name, xmlContent] : examples) {
    auto key = prefix + name;

    auto pagxPath = SavePAGXFile(xmlContent, "PAGXTest/" + key + ".pagx");

    // Load via FromFile so relative image paths resolve against the pagx directory.
    auto doc = pagx::PAGXImporter::FromFile(pagxPath);
    if (!doc) {
      ADD_FAILURE() << "Failed to parse XML for: " << key;
      continue;
    }
    if (!doc->errors.empty()) {
      std::string errorLog;
      for (const auto& error : doc->errors) {
        errorLog += "\n  " + error;
      }
      ADD_FAILURE() << "Parse errors in " << key << ":" << errorLog;
    }

    auto layer = pagx::LayerBuilder::Build(doc.get(), &fontConfig);
    if (!layer) {
      ADD_FAILURE() << "Failed to build layer for: " << key;
      continue;
    }

    auto surface =
        Surface::Make(context, static_cast<int>(doc->width), static_cast<int>(doc->height));
    if (!surface) {
      ADD_FAILURE() << "Failed to create surface for: " << key;
      continue;
    }
    DisplayList displayList;
    displayList.root()->addChild(layer);
    displayList.render(surface.get(), false);

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + key)) << key;
  }
}

static void TestPAGXDirectory(tgfx::Context* context, const std::string& directory,
                              const std::string& prefix = "") {
  std::vector<std::string> files = {};
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());

  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(GetFallbackTypefaces());

  for (const auto& filePath : files) {
    auto key = prefix + std::filesystem::path(filePath).stem().string();

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
      ADD_FAILURE() << "Parse errors in " << key << ":" << errorLog;
    }

    auto layer = pagx::LayerBuilder::Build(doc.get(), &fontConfig);
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

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + key)) << key;
  }
}

/**
 * Test all PAGX sample files in spec/samples directory.
 * Renders each sample and compares with baseline screenshots.
 */
PAGX_TEST(PAGXTest, SpecSamples) {
  TestPAGXDirectory(context, ProjectPath::Absolute("spec/samples"));
}

/**
 * Test all text-related PAGX files in resources/text directory.
 */
PAGX_TEST(PAGXTest, TextFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("resources/text"), "text_");
}

/**
 * Test all layout-related PAGX files in resources/layout directory.
 * Tests container layout and constraint positioning behaviors with visual verification.
 */
PAGX_TEST(PAGXTest, LayoutFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("resources/layout"), "layout_");
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

// =====================================================================================
// Auto Layout - Container Layout - Basic
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutHorizontalEqualWidth) {
  auto doc = pagx::PAGXDocument::Make(920, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 920;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 14;
  parent->padding = pagx::Padding{20, 20, 20, 20};

  auto card1 = doc->makeNode<pagx::Layer>();
  auto card2 = doc->makeNode<pagx::Layer>();
  auto card3 = doc->makeNode<pagx::Layer>();

  parent->children = {card1, card2, card3};

  for (auto* card : {card1, card2, card3}) {
    card->flex = 1;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->size = {100, 100};
    rect->position = {0, 0};
    card->contents.push_back(rect);
  }

  doc->applyLayout();

  float expectedChildWidth = 284.0f;

  EXPECT_EQ(card1->actualWidth, expectedChildWidth);
  EXPECT_EQ(card2->actualWidth, expectedChildWidth);
  EXPECT_EQ(card3->actualWidth, expectedChildWidth);

  EXPECT_EQ(card1->x, 20.0f);
  EXPECT_EQ(card2->x, 20 + 284 + 14);
  EXPECT_EQ(card3->x, 20 + 284 * 2 + 14 * 2);
}

PAGX_TEST(PAGXTest, LayoutVerticalStart) {
  auto doc = pagx::PAGXDocument::Make(400, 600);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 600;
  parent->layout = pagx::LayoutMode::Vertical;
  parent->gap = 10;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();

  parent->children = {child1, child2};

  child1->height = 100;
  child2->height = 150;

  doc->applyLayout();

  EXPECT_EQ(child1->y, 0.0f);
  EXPECT_EQ(child2->y, 110.0f);
}

PAGX_TEST(PAGXTest, LayoutArrangementCenter) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->arrangement = pagx::Arrangement::Center;
  parent->gap = 0;

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 100;

  doc->applyLayout();

  EXPECT_EQ(child->x, 150.0f);
}

PAGX_TEST(PAGXTest, LayoutArrangementSpaceBetween) {
  auto doc = pagx::PAGXDocument::Make(400, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->arrangement = pagx::Arrangement::SpaceBetween;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();

  parent->children = {child1, child2};

  child1->width = 50;
  child1->height = 50;
  child2->width = 50;
  child2->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 350.0f);
}

PAGX_TEST(PAGXTest, LayoutAlignmentCenter) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Center;

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child->y, 125.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Arrangement End
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutArrangementEnd) {
  auto doc = pagx::PAGXDocument::Make(400, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->arrangement = pagx::Arrangement::End;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2};

  child1->width = 50;
  child1->height = 50;
  child2->width = 50;
  child2->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->x, 300.0f);
  EXPECT_EQ(child2->x, 350.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Alignment End
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutAlignmentEnd) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::End;

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child->y, 250.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Padding
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutPadding) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->padding = pagx::Padding{40, 30, 20, 50};

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 80;

  doc->applyLayout();

  EXPECT_EQ(child->x, 50.0f);
  EXPECT_EQ(child->y, 40.0f);
}

PAGX_TEST(PAGXTest, LayoutPaddingWithFlex) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->padding = pagx::Padding{10, 10, 10, 10};
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2};

  child1->flex = 1;
  child1->height = 50;
  child2->flex = 1;
  child2->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->actualWidth, 190.0f);
  EXPECT_EQ(child2->actualWidth, 190.0f);
  EXPECT_EQ(child1->x, 10.0f);
  EXPECT_EQ(child2->x, 200.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Flex Distribution
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutFlexEqualDistribution) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 300;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->flex = 1;
  child1->height = 50;
  child2->flex = 1;
  child2->height = 50;
  child3->flex = 1;
  child3->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->actualWidth, 100.0f);
  EXPECT_EQ(child2->actualWidth, 100.0f);
  EXPECT_EQ(child3->actualWidth, 100.0f);
  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutFlexMixedWithFixed) {
  auto doc = pagx::PAGXDocument::Make(400, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->width = 100;
  child1->height = 50;
  child2->flex = 1;
  child2->height = 50;
  child3->flex = 1;
  child3->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->actualWidth, 100.0f);
  EXPECT_EQ(child2->actualWidth, 150.0f);
  EXPECT_EQ(child3->actualWidth, 150.0f);
  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 250.0f);
}

PAGX_TEST(PAGXTest, LayoutFlexWeightedDistribution) {
  auto doc = pagx::PAGXDocument::Make(600, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->flex = 1;
  child1->height = 50;
  child2->flex = 2;
  child2->height = 50;
  child3->flex = 3;
  child3->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->actualWidth, 100.0f);
  EXPECT_EQ(child2->actualWidth, 200.0f);
  EXPECT_EQ(child3->actualWidth, 300.0f);
  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 300.0f);
}

PAGX_TEST(PAGXTest, LayoutFlexContentMeasuredDefault) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  // Child with no width and flex=0 (default) should use content-measured size.
  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 40};
  rect->position = {40, 20};
  child->contents.push_back(rect);

  doc->applyLayout();

  // Content-measured width = 80 (from rectangle).
  EXPECT_EQ(child->actualWidth, 80.0f);
  EXPECT_EQ(child->x, 0.0f);
}

PAGX_TEST(PAGXTest, LayoutFlexMixedWithContentMeasured) {
  auto doc = pagx::PAGXDocument::Make(600, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  // Fixed child: width=100.
  auto fixedChild = doc->makeNode<pagx::Layer>();
  fixedChild->width = 100;
  fixedChild->height = 50;

  // Content-measured child: no width, flex=0, has a 80px-wide rectangle.
  auto measuredChild = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 40};
  rect->position = {40, 20};
  measuredChild->contents.push_back(rect);

  // Flex child: no width, flex=1, takes remaining space.
  auto flexChild = doc->makeNode<pagx::Layer>();
  flexChild->flex = 1;
  flexChild->height = 50;

  parent->children = {fixedChild, measuredChild, flexChild};

  doc->applyLayout();

  // fixed=100, measured=80, flex gets 600-100-80=420.
  EXPECT_EQ(fixedChild->actualWidth, 100.0f);
  EXPECT_EQ(measuredChild->actualWidth, 80.0f);
  EXPECT_EQ(flexChild->actualWidth, 420.0f);
  EXPECT_EQ(fixedChild->x, 0.0f);
  EXPECT_EQ(measuredChild->x, 100.0f);
  EXPECT_EQ(flexChild->x, 180.0f);
}

PAGX_TEST(PAGXTest, LayoutFlexRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;

  auto child = doc->makeNode<pagx::Layer>();
  child->flex = 2.5f;
  child->height = 100;
  parent->children.push_back(child);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* parent2 = doc2->layers[0];
  ASSERT_FALSE(parent2->children.empty());

  auto* child2 = parent2->children[0];
  EXPECT_FLOAT_EQ(child2->flex, 2.5f);
}

PAGX_TEST(PAGXTest, LayoutFlexZeroNotExported) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;
  layer->flex = 0;

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  // flex=0 (default) should not appear in XML output.
  EXPECT_EQ(xml.find("flex="), std::string::npos);
}

// =====================================================================================
// Auto Layout - Container Layout - Visibility
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutHiddenChildNotSkipped) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 300;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->width = 100;
  child1->height = 100;
  child2->width = 100;
  child2->height = 100;
  child2->visible = false;
  child3->width = 100;
  child3->height = 100;

  doc->applyLayout();

  // Visibility does not affect layout — invisible children still participate.
  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 200.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Edge Cases
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutEmptyContainer) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;

  doc->applyLayout();

  EXPECT_EQ(parent->width, 400.0f);
  EXPECT_EQ(parent->height, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutAllFixedNoFlexSpace) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 300;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->width = 100;
  child1->height = 100;
  child2->width = 100;
  child2->height = 100;
  child3->width = 100;
  child3->height = 100;

  doc->applyLayout();

  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutOverflow) {
  auto doc = pagx::PAGXDocument::Make(250, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 250;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->width = 100;
  child1->height = 100;
  child2->width = 100;
  child2->height = 100;
  child3->width = 100;
  child3->height = 100;

  doc->applyLayout();

  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 100.0f);
  EXPECT_EQ(child3->x, 200.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Nested
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutNested) {
  auto doc = pagx::PAGXDocument::Make(500, 200);
  auto outer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(outer);

  outer->width = 500;
  outer->height = 200;
  outer->layout = pagx::LayoutMode::Horizontal;
  outer->gap = 0;

  auto left = doc->makeNode<pagx::Layer>();
  left->width = 200;
  left->height = 200;

  auto right = doc->makeNode<pagx::Layer>();
  right->flex = 1;
  right->height = 200;
  right->layout = pagx::LayoutMode::Vertical;
  right->gap = 10;

  outer->children = {left, right};

  auto inner1 = doc->makeNode<pagx::Layer>();
  auto inner2 = doc->makeNode<pagx::Layer>();
  inner1->height = 80;
  inner2->height = 80;
  right->children = {inner1, inner2};

  doc->applyLayout();

  EXPECT_EQ(right->actualWidth, 300.0f);
  EXPECT_EQ(right->x, 200.0f);

  EXPECT_EQ(inner1->y, 0.0f);
  EXPECT_EQ(inner2->y, 90.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Measurement (bottom-up)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutMeasureFromChildren) {
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 20;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2};

  child1->width = 100;
  child1->height = 80;
  child2->width = 100;
  child2->height = 60;

  doc->applyLayout();

  EXPECT_EQ(parent->actualWidth, 220.0f);
  EXPECT_EQ(parent->actualHeight, 80.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Pixel Grid Snap
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutSnapToPixelGrid) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 100;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  auto child3 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2, child3};

  child1->flex = 1;
  child1->height = 50;
  child2->flex = 1;
  child2->height = 50;
  child3->flex = 1;
  child3->height = 50;

  doc->applyLayout();

  EXPECT_EQ(child1->x, std::round(child1->x));
  EXPECT_EQ(child1->actualWidth, std::round(child1->actualWidth));
  EXPECT_EQ(child2->x, std::round(child2->x));
  EXPECT_EQ(child2->actualWidth, std::round(child2->actualWidth));
  EXPECT_EQ(child3->x, std::round(child3->x));
  EXPECT_EQ(child3->actualWidth, std::round(child3->actualWidth));

  EXPECT_EQ(child1->actualWidth + child2->actualWidth + child3->actualWidth, 100.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Measurement Cache
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutMeasureCacheIdempotent) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  auto child1 = doc->makeNode<pagx::Layer>();
  auto child2 = doc->makeNode<pagx::Layer>();
  parent->children = {child1, child2};

  child1->flex = 1;
  child1->height = 80;
  child2->width = 100;
  child2->height = 80;

  doc->applyLayout();
  float w1 = child1->actualWidth;
  float x1 = child1->x;
  float w2 = child2->actualWidth;
  float x2 = child2->x;

  doc->applyLayout();
  EXPECT_EQ(child1->actualWidth, w1);
  EXPECT_EQ(child1->x, x1);
  EXPECT_EQ(child2->actualWidth, w2);
  EXPECT_EQ(child2->x, x2);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Single Edge
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintLeft) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->left = 20;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.x, 70.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintRight) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->right = 30;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.x, 320.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintTop) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->top = 25;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.y, 50.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintBottom) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->bottom = 40;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.y, 135.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintCenterX) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {100, 100};
  rect->centerX = 0;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.x, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintCenterY) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->centerY = 10;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.y, 110.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Stretch (Ellipse)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintStretchEllipse) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {100, 50};
  ellipse->position = {0, 0};
  ellipse->left = 20;
  ellipse->right = 20;

  layer->contents.push_back(ellipse);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(ellipse->size.width, 360.0f);
  EXPECT_FLOAT_EQ(ellipse->position.x, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintStretchEllipseVertical) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {100, 50};
  ellipse->position = {0, 0};
  ellipse->top = 15;
  ellipse->bottom = 15;

  layer->contents.push_back(ellipse);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(ellipse->size.height, 170.0f);
  EXPECT_FLOAT_EQ(ellipse->position.y, 100.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Stretch (Rectangle, TextBox)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintStretchRectangle) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->left = 10;
  rect->right = 10;

  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->size.width, 380.0f);
  EXPECT_FLOAT_EQ(rect->position.x, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintStretchTextBox) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 100;
  textBox->height = 50;
  textBox->position = {0, 0};
  textBox->left = 30;
  textBox->right = 30;
  textBox->top = 20;
  textBox->bottom = 20;

  layer->contents.push_back(textBox);

  doc->applyLayout();

  // Horizontal: 400 - 30 - 30 = 340
  EXPECT_FLOAT_EQ(textBox->actualWidth, 340.0f);
  EXPECT_FLOAT_EQ(textBox->position.x, 30.0f);
  // Vertical: 200 - 20 - 20 = 160
  EXPECT_FLOAT_EQ(textBox->actualHeight, 160.0f);
  EXPECT_FLOAT_EQ(textBox->position.y, 20.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Proportional Scaling
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintScalePolystarHorizontal) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto star = doc->makeNode<pagx::Polystar>();
  star->outerRadius = 30;
  star->innerRadius = 15;
  star->position = {0, 0};
  star->left = 50;
  star->right = 50;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions, not simplified square):
  // For Star with pointCount=5, rotation=0, width ≈ 2.0 * outerRadius, height ≈ 1.809 * outerRadius
  // bounds.width ≈ 57.06 → ceil → 58, area width = 400 - 50 - 50 = 300
  // scale = 300 / 58 ≈ 5.172
  EXPECT_FLOAT_EQ(star->outerRadius, 155.17241f);
  EXPECT_FLOAT_EQ(star->innerRadius, 77.586205f);
  EXPECT_NEAR(star->position.x, 200.0f, 0.5f);
}

PAGX_TEST(PAGXTest, LayoutConstraintScalePolystarVertical) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto star = doc->makeNode<pagx::Polystar>();
  star->outerRadius = 25;
  star->innerRadius = 10;
  star->position = {0, 0};
  star->top = 20;
  star->bottom = 20;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions):
  // bounds.height ≈ 1.809 * outerRadius ≈ 45.225 → ceil → 46
  // area height = 200 - 20 - 20 = 160
  // scale = 160 / 46 ≈ 3.478
  EXPECT_FLOAT_EQ(star->outerRadius, 86.95652f);
  EXPECT_FLOAT_EQ(star->innerRadius, 34.782608f);
  EXPECT_NEAR(star->position.y, 108.0f, 0.5f);
}

PAGX_TEST(PAGXTest, LayoutConstraintScalePolystarBothAxes) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto star = doc->makeNode<pagx::Polystar>();
  star->outerRadius = 20;
  star->innerRadius = 10;
  star->position = {0, 0};
  star->left = 20;
  star->right = 20;
  star->top = 10;
  star->bottom = 10;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions):
  // bounds.width ≈ 38.042 → ceil → 39, bounds.height ≈ 36.180 → ceil → 37
  // areaWidth = 360, areaHeight = 180
  // scaleX = 360 / 39 ≈ 9.231, scaleY = 180 / 37 ≈ 4.865
  // scale = min(9.231, 4.865) ≈ 4.865
  EXPECT_FLOAT_EQ(star->outerRadius, 97.297295f);
  EXPECT_FLOAT_EQ(star->innerRadius, 48.648647f);
  // Scaled bounds = (-99.5, -90, 199, 180)
  // Horizontal: tx = 20 + (360 - 199) * 0.5 - (-99.5) = 200, position.x = 200
  // Vertical: ty = 10 + (180 - 180) * 0.5 - (-90) = 100, position.y = 100
  // Scaled bounds are computed from trigonometric vertex positions, so position values
  // are not exact integers after subtracting bounds offset.
  EXPECT_NEAR(star->position.x, 200.0f, 0.5f);
  EXPECT_NEAR(star->position.y, 109.5f, 0.5f);
}

PAGX_TEST(PAGXTest, LayoutConstraintScaleTextBothAxes) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);
  auto fontFamily = typeface->fontFamily();
  auto fontStyle = typeface->fontStyle();

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello";
  text->fontFamily = fontFamily;
  text->fontStyle = fontStyle;
  text->fontSize = 24;
  text->position = {0, 0};
  text->left = 20;
  text->right = 20;
  text->top = 10;
  text->bottom = 10;

  // Compute original text bounds (horizontal: advance width, vertical: tight pixel bounds).
  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  pagx::LayoutContext layoutContext(&fontConfig);
  pagx::TextLayoutParams params = {};
  params.baseline = text->baseline;
  auto origBounds = pagx::TextLayout::Layout({text}, params, layoutContext);
  float origWidth = origBounds.bounds.width;
  float origHeight = origBounds.bounds.height;

  layer->contents.push_back(text);

  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // Proportional scaling: areaWidth = 360, areaHeight = 180
  // preferredWidth/Height are ceil'd before scaling.
  float scaleX = 360 / std::ceil(origWidth);
  float scaleY = 180 / std::ceil(origHeight);
  float scale = std::min(scaleX, scaleY);
  float expectedFontSize = 24 * scale;
  EXPECT_FLOAT_EQ(text->fontSize, expectedFontSize);
}

PAGX_TEST(PAGXTest, LayoutConstraintScaleTextSingleAxis) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);
  auto fontFamily = typeface->fontFamily();
  auto fontStyle = typeface->fontStyle();

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hi";
  text->fontFamily = fontFamily;
  text->fontStyle = fontStyle;
  text->fontSize = 50;
  text->position = {0, 0};
  text->left = 10;
  text->right = 10;

  // Compute original text bounds (horizontal: advance width, vertical: tight pixel bounds).
  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  pagx::LayoutContext layoutContext(&fontConfig);
  pagx::TextLayoutParams params = {};
  params.baseline = text->baseline;
  auto origBounds = pagx::TextLayout::Layout({text}, params, layoutContext);
  float origWidth = origBounds.bounds.width;

  layer->contents.push_back(text);

  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // areaWidth = 380, scale = 380 / ceil(origWidth)
  float expectedFontSize = 50 * 380 / std::ceil(origWidth);
  EXPECT_FLOAT_EQ(text->fontSize, expectedFontSize);
}

PAGX_TEST(PAGXTest, LayoutConstraintScaleTextSkipWithTextBox) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 200;
  textBox->height = 100;
  textBox->position = {100, 50};

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello";
  text->fontSize = 24;
  text->position = {0, 0};
  text->left = 20;
  text->right = 20;
  text->top = 10;
  text->bottom = 10;

  layer->contents = {textBox, text};

  doc->applyLayout();

  // TextBox exists in the same scope, so Text should skip scaling.
  // fontSize remains unchanged.
  EXPECT_FLOAT_EQ(text->fontSize, 24.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintScaleExactFit) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto star = doc->makeNode<pagx::Polystar>();
  star->outerRadius = 50;
  star->innerRadius = 25;
  star->position = {0, 0};
  // Polystar bounds: bounds.width ≈ 95.106 → ceil → 96
  // areaWidth = 400 - 150 - 150 = 100
  // scale = 100 / 96 ≈ 1.0417 (not 1.0, so scaling applies)
  star->left = 150;
  star->right = 150;

  layer->contents.push_back(star);

  doc->applyLayout();

  // scale ≈ 1.0417
  EXPECT_FLOAT_EQ(star->outerRadius, 52.083332f);
  EXPECT_FLOAT_EQ(star->innerRadius, 26.041666f);
  EXPECT_NEAR(star->position.x, 200.0f, 0.5f);
}

PAGX_TEST(PAGXTest, LayoutConstraintScalePath) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto path = doc->makeNode<pagx::Path>();
  path->data = doc->makeNode<pagx::PathData>();
  path->data->moveTo(0, 0);
  path->data->lineTo(60, 0);
  path->data->lineTo(30, 40);
  path->data->close();
  path->position = {0, 0};
  path->left = 20;
  path->right = 20;

  layer->contents.push_back(path);

  doc->applyLayout();

  // Path bounds = (0, 0, 60, 40). Single-axis constraint: areaWidth = 360
  // Proportional scaling: scale = 360 / 60 = 6.0
  // Scaled points: (0,0)->(360,0)->(180,240), new bounds = (0, 0, 360, 240)
  EXPECT_FLOAT_EQ(path->position.x, 20.0f);
  // Verify points were actually scaled
  auto& points = path->data->points();
  EXPECT_FLOAT_EQ(points[1].x, 360.0f);  // 60 * 6
  EXPECT_FLOAT_EQ(points[2].x, 180.0f);  // 30 * 6
  EXPECT_FLOAT_EQ(points[2].y, 240.0f);  // 40 * 6
}

PAGX_TEST(PAGXTest, LayoutConstraintScalePathBothAxes) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 300;

  auto path = doc->makeNode<pagx::Path>();
  path->data = doc->makeNode<pagx::PathData>();
  path->data->moveTo(0, 0);
  path->data->lineTo(100, 0);
  path->data->lineTo(100, 50);
  path->data->lineTo(0, 50);
  path->data->close();
  path->position = {0, 0};
  path->left = 50;
  path->right = 50;
  path->top = 50;
  path->bottom = 50;

  layer->contents.push_back(path);

  doc->applyLayout();

  // Path bounds = (0, 0, 100, 50)
  // areaWidth = 300, scaleX = 300 / 100 = 3.0
  // areaHeight = 200, scaleY = 200 / 50 = 4.0
  // scale = min(3.0, 4.0) = 3.0
  // Scaled bounds: (0, 0, 300, 150)
  auto& points = path->data->points();
  EXPECT_FLOAT_EQ(points[1].x, 300.0f);  // 100 * 3
  EXPECT_FLOAT_EQ(points[2].y, 150.0f);  // 50 * 3
  // Centered vertically: 50 + (200 - 150) * 0.5 - 0 = 75
  EXPECT_FLOAT_EQ(path->position.x, 50.0f);
  EXPECT_FLOAT_EQ(path->position.y, 75.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Group
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutGroupDerivedSize) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 300;

  auto group = doc->makeNode<pagx::Group>();
  group->position = {0, 0};
  group->left = 20;
  group->right = 20;
  group->top = 30;
  group->bottom = 30;

  layer->contents.push_back(group);

  doc->applyLayout();

  EXPECT_EQ(group->actualWidth, 360.0f);
  EXPECT_EQ(group->actualHeight, 240.0f);
  EXPECT_EQ(group->position.x, 20.0f);
  EXPECT_EQ(group->position.y, 30.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintGroupRecursive) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto group = doc->makeNode<pagx::Group>();
  group->position = {0, 0};
  group->left = 20;
  group->right = 20;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->left = 10;
  rect->right = 10;

  group->elements.push_back(rect);
  layer->contents.push_back(group);

  doc->applyLayout();

  EXPECT_EQ(group->actualWidth, 360.0f);
  EXPECT_EQ(group->position.x, 20.0f);
  EXPECT_FLOAT_EQ(rect->size.width, 340.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Multiple Elements
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintMultipleElements) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 300;

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {80, 40};
  rect1->position = {0, 0};
  rect1->left = 10;

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {80, 40};
  rect2->position = {0, 0};
  rect2->right = 10;

  auto rect3 = doc->makeNode<pagx::Rectangle>();
  rect3->size = {80, 40};
  rect3->position = {0, 0};
  rect3->centerX = 0;
  rect3->centerY = 0;

  layer->contents = {rect1, rect2, rect3};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect1->position.x, 50.0f);
  EXPECT_FLOAT_EQ(rect2->position.x, 350.0f);
  EXPECT_FLOAT_EQ(rect3->position.x, 200.0f);
  EXPECT_FLOAT_EQ(rect3->position.y, 150.0f);
}

// =====================================================================================
// Auto Layout - Constraint Positioning - Validation
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintConflictCenterXWithLeftRight) {
  // In the new layout system, conflicting constraints are resolved by priority:
  // centerX has higher priority than left/right. No error is generated.
  auto pagx = R"(<pagx width="100" height="100"><Layer width="100" height="100">
    <Rectangle centerX="0" left="10" /></Layer></pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(pagx);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());
}

PAGX_TEST(PAGXTest, LayoutConstraintConflictCenterYWithTopBottom) {
  // centerY has higher priority than top/bottom. No error is generated.
  auto pagx = R"(<pagx width="100" height="100"><Layer width="100" height="100">
    <Rectangle centerY="0" bottom="10" /></Layer></pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(pagx);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());
}

PAGX_TEST(PAGXTest, LayoutConstraintValidCombination) {
  auto pagx = R"(<pagx width="100" height="100"><Layer width="100" height="100">
    <Rectangle left="10" right="10" top="20" bottom="20" /></Layer></pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(pagx);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());
}

// =====================================================================================
// Auto Layout - Container Layout - includeInLayout
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutContainerIncludeInLayout) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  // A decoration layer that should not participate in layout.
  auto badge = doc->makeNode<pagx::Layer>();
  badge->width = 40;
  badge->height = 40;
  badge->x = 550;
  badge->y = 5;
  badge->includeInLayout = false;

  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 200;
  child1->height = 100;

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  child2->height = 100;

  parent->children = {badge, child1, child2};

  doc->applyLayout();

  // badge is excluded from layout: its x/y should remain unchanged.
  EXPECT_FLOAT_EQ(badge->x, 550.0f);
  EXPECT_FLOAT_EQ(badge->y, 5.0f);

  // child1 and child2 are laid out as if badge doesn't exist.
  EXPECT_FLOAT_EQ(child1->x, 0.0f);
  EXPECT_FLOAT_EQ(child2->x, 210.0f);  // 200 + 10
}

PAGX_TEST(PAGXTest, LayoutContainerIncludeInLayoutMeasure) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  // Parent has no explicit width — should be measured from participating children only.
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  auto badge = doc->makeNode<pagx::Layer>();
  badge->width = 500;
  badge->height = 40;
  badge->includeInLayout = false;

  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 100;
  child1->height = 80;

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 100;
  child2->height = 80;

  parent->children = {badge, child1, child2};

  doc->applyLayout();

  // Measured width = 100 + 10 + 100 = 210 (badge excluded).
  EXPECT_FLOAT_EQ(parent->actualWidth, 210.0f);
}

// =====================================================================================
// Auto Layout - Container Layout - Alignment::Stretch
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutContainerStretch) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;
  parent->gap = 10;

  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 200;
  // height is NaN — should be stretched to 200.

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  // height is NaN — should be stretched to 200.

  parent->children = {child1, child2};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child1->actualHeight, 200.0f);
  EXPECT_FLOAT_EQ(child2->actualHeight, 200.0f);
  EXPECT_FLOAT_EQ(child1->y, 0.0f);
  EXPECT_FLOAT_EQ(child2->y, 0.0f);
}

PAGX_TEST(PAGXTest, LayoutContainerStretchWithPadding) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;
  parent->padding = {10, 20, 10, 20};

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;
  // height is NaN — should be stretched to 200 - 10 - 10 = 180.

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->actualHeight, 180.0f);
  EXPECT_FLOAT_EQ(child->y, 10.0f);
}

PAGX_TEST(PAGXTest, LayoutContainerStretchExplicitSize) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;

  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 200;
  child1->height = 80;  // Explicit height — should NOT be stretched.

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  // height is NaN — should be stretched to 200.

  parent->children = {child1, child2};

  doc->applyLayout();

  // child1 has explicit height, not affected by Stretch.
  EXPECT_FLOAT_EQ(child1->height, 80.0f);
  // child2 has no explicit height, stretched.
  EXPECT_FLOAT_EQ(child2->actualHeight, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutContainerStretchVertical) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Vertical;
  parent->alignment = pagx::Alignment::Stretch;

  auto child = doc->makeNode<pagx::Layer>();
  child->height = 100;
  // width is NaN — in vertical layout, cross axis is width, should be stretched to 600.

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->actualWidth, 600.0f);
  EXPECT_FLOAT_EQ(child->x, 0.0f);
}

PAGX_TEST(PAGXTest, LayoutIncludeInLayoutWithStretch) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;

  // badge: excluded from layout, should NOT be stretched.
  auto badge = doc->makeNode<pagx::Layer>();
  badge->width = 40;
  badge->height = 40;
  badge->x = 550;
  badge->y = 5;
  badge->includeInLayout = false;

  // child: no explicit height, should be stretched to 200.
  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;

  parent->children = {badge, child};

  doc->applyLayout();

  // badge is excluded: its height remains 40, position unchanged.
  EXPECT_FLOAT_EQ(badge->height, 40.0f);
  EXPECT_FLOAT_EQ(badge->x, 550.0f);
  EXPECT_FLOAT_EQ(badge->y, 5.0f);

  // child is stretched.
  EXPECT_FLOAT_EQ(child->actualHeight, 200.0f);
  EXPECT_FLOAT_EQ(child->x, 0.0f);
}

PAGX_TEST(PAGXTest, LayoutIncludeInLayoutMixedVisibility) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  // Parent has no explicit width — measured from participating children.
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  // child1: visible=true, includeInLayout=false -> excluded from layout flow.
  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 500;
  child1->height = 100;
  child1->includeInLayout = false;

  // child2: visible=false, includeInLayout=true -> still participates in layout.
  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 300;
  child2->height = 100;
  child2->visible = false;

  // child3: visible=true, includeInLayout=true -> participates normally.
  auto child3 = doc->makeNode<pagx::Layer>();
  child3->width = 100;
  child3->height = 80;

  // child4: visible=true, includeInLayout=true -> participates normally.
  auto child4 = doc->makeNode<pagx::Layer>();
  child4->width = 100;
  child4->height = 80;

  parent->children = {child1, child2, child3, child4};

  doc->applyLayout();

  // child2, child3, child4 participate: measured width = 300 + 10 + 100 + 10 + 100 = 520.
  EXPECT_FLOAT_EQ(parent->actualWidth, 520.0f);
  // child2 (invisible but still participates) is positioned first.
  EXPECT_FLOAT_EQ(child2->x, 0.0f);
  EXPECT_FLOAT_EQ(child3->x, 310.0f);
  EXPECT_FLOAT_EQ(child4->x, 420.0f);
}

// =====================================================================================
// Auto Layout - Container + Constraint combined
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutContainerWithConstraints) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 0;

  auto left = doc->makeNode<pagx::Layer>();
  auto right = doc->makeNode<pagx::Layer>();
  parent->children = {left, right};

  left->flex = 1;
  left->height = 400;
  right->flex = 1;
  right->height = 400;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 80};
  rect->position = {0, 0};
  rect->centerX = 0;
  rect->centerY = 0;
  left->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_EQ(left->actualWidth, 300.0f);
  EXPECT_EQ(right->actualWidth, 300.0f);

  EXPECT_FLOAT_EQ(rect->position.x, 150.0f);
  EXPECT_FLOAT_EQ(rect->position.y, 200.0f);
}

// =====================================================================================
// Auto Layout - Round-trip: Layout Attributes
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutRoundTripAttributes) {
  auto doc = pagx::PAGXDocument::Make(500, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->name = "LayoutLayer";
  layer->width = 500;
  layer->height = 400;
  layer->layout = pagx::LayoutMode::Vertical;
  layer->gap = 15;
  layer->padding = pagx::Padding{10, 20, 30, 40};
  layer->alignment = pagx::Alignment::Center;
  layer->arrangement = pagx::Arrangement::SpaceBetween;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 50;
  layer->children.push_back(child);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* layer2 = doc2->layers[0];

  EXPECT_EQ(layer2->layout, pagx::LayoutMode::Vertical);

  EXPECT_FLOAT_EQ(layer2->gap, 15.0f);

  EXPECT_FLOAT_EQ(layer2->padding.top, 10.0f);
  EXPECT_FLOAT_EQ(layer2->padding.right, 20.0f);
  EXPECT_FLOAT_EQ(layer2->padding.bottom, 30.0f);
  EXPECT_FLOAT_EQ(layer2->padding.left, 40.0f);

  EXPECT_EQ(layer2->alignment, pagx::Alignment::Center);
  EXPECT_EQ(layer2->arrangement, pagx::Arrangement::SpaceBetween);

  auto* child2 = layer2->children[0];
  // includeInLayout defaults to true and is not written when true.
  EXPECT_TRUE(child2->includeInLayout);
}

PAGX_TEST(PAGXTest, LayoutRoundTripIncludeInLayout) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  auto badge = doc->makeNode<pagx::Layer>();
  badge->width = 40;
  badge->height = 40;
  badge->x = 550;
  badge->y = 5;
  badge->includeInLayout = false;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;
  child->height = 100;

  parent->children = {badge, child};

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  EXPECT_NE(xml.find("includeInLayout=\"false\""), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* parent2 = doc2->layers[0];
  ASSERT_EQ(parent2->children.size(), 2u);
  EXPECT_FALSE(parent2->children[0]->includeInLayout);
  EXPECT_TRUE(parent2->children[1]->includeInLayout);

  // Verify layout still works correctly after round-trip.
  doc2->applyLayout();
  EXPECT_FLOAT_EQ(parent2->children[0]->x, 550.0f);
  EXPECT_FLOAT_EQ(parent2->children[0]->y, 5.0f);
  EXPECT_FLOAT_EQ(parent2->children[1]->x, 0.0f);
}

PAGX_TEST(PAGXTest, LayoutRoundTripAlignmentStretch) {
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;
  parent->children = {child};

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  // Stretch is the default value, so it should NOT appear in the exported XML.
  EXPECT_EQ(xml.find("alignment=\"stretch\""), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* parent2 = doc2->layers[0];
  EXPECT_EQ(parent2->alignment, pagx::Alignment::Stretch);

  // Verify stretch behavior works after round-trip.
  doc2->applyLayout();
  EXPECT_FLOAT_EQ(parent2->children[0]->actualHeight, 200.0f);
}

PAGX_TEST(PAGXTest, LayoutRoundTripConstraints) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 300;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->left = 10;
  rect->right = 20;
  rect->top = 30;
  rect->bottom = 40;

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {80, 60};
  ellipse->position = {0, 0};
  ellipse->centerX = 5;
  ellipse->centerY = -10;

  layer->contents = {rect, ellipse};

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  EXPECT_NE(xml.find("left=\"10\""), std::string::npos);
  EXPECT_NE(xml.find("right=\"20\""), std::string::npos);
  EXPECT_NE(xml.find("top=\"30\""), std::string::npos);
  EXPECT_NE(xml.find("bottom=\"40\""), std::string::npos);
  EXPECT_NE(xml.find("centerX=\"5\""), std::string::npos);
  EXPECT_NE(xml.find("centerY=\"-10\""), std::string::npos);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Single Edge
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintLeft) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->left = 30;

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->x, 30.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintRight) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->right = 20;

  parent->children = {child};

  doc->applyLayout();

  // x = parentWidth - right - childWidth = 400 - 20 - 100 = 280
  EXPECT_FLOAT_EQ(child->x, 280.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintTop) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->top = 40;

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->y, 40.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintBottom) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->bottom = 25;

  parent->children = {child};

  doc->applyLayout();

  // y = parentHeight - bottom - childHeight = 300 - 25 - 60 = 215
  EXPECT_FLOAT_EQ(child->y, 215.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Center
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintCenterX) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->centerX = 0;

  parent->children = {child};

  doc->applyLayout();

  // x = (parentWidth - childWidth) / 2 + centerX = (400 - 100) / 2 + 0 = 150
  EXPECT_FLOAT_EQ(child->x, 150.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintCenterY) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->centerY = 0;

  parent->children = {child};

  doc->applyLayout();

  // y = (parentHeight - childHeight) / 2 + centerY = (300 - 60) / 2 + 0 = 120
  EXPECT_FLOAT_EQ(child->y, 120.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintCenterXWithOffset) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->centerX = 20;
  child->centerY = -10;

  parent->children = {child};

  doc->applyLayout();

  // x = (400 - 100) / 2 + 20 = 170
  EXPECT_FLOAT_EQ(child->x, 170.0f);
  // y = (300 - 60) / 2 + (-10) = 110
  EXPECT_FLOAT_EQ(child->y, 110.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Opposite Edges Derive Size
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintLeftRightDeriveWidth) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->height = 60;
  // width is NAN — should be derived from left + right.
  child->left = 30;
  child->right = 50;

  parent->children = {child};

  doc->applyLayout();

  // x = left = 30
  EXPECT_FLOAT_EQ(child->x, 30.0f);
  // width = parentWidth - left - right = 400 - 30 - 50 = 320
  EXPECT_FLOAT_EQ(child->actualWidth, 320.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintTopBottomDeriveHeight) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  // height is NAN — should be derived from top + bottom.
  child->top = 20;
  child->bottom = 40;

  parent->children = {child};

  doc->applyLayout();

  // y = top = 20
  EXPECT_FLOAT_EQ(child->y, 20.0f);
  // height = parentHeight - top - bottom = 300 - 20 - 40 = 240
  EXPECT_FLOAT_EQ(child->actualHeight, 240.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintAllFourEdges) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  // Both width and height are NAN — both should be derived.
  auto child = doc->makeNode<pagx::Layer>();
  child->left = 10;
  child->right = 20;
  child->top = 30;
  child->bottom = 40;

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->x, 10.0f);
  EXPECT_FLOAT_EQ(child->y, 30.0f);
  EXPECT_FLOAT_EQ(child->actualWidth, 370.0f);   // 400 - 10 - 20
  EXPECT_FLOAT_EQ(child->actualHeight, 230.0f);  // 300 - 30 - 40
}

PAGX_TEST(PAGXTest, LayerConstraintLeftRightOverridesExplicitWidth) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;  // Explicit width — overridden by left+right.
  child->height = 60;
  child->left = 30;
  child->right = 50;

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->x, 30.0f);
  EXPECT_FLOAT_EQ(child->actualWidth,
                  320.0f);  // 400 - 30 - 50, left+right overrides explicit width.
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Combined Axes
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintLeftAndTop) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->left = 15;
  child->top = 25;

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->x, 15.0f);
  EXPECT_FLOAT_EQ(child->y, 25.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintRightAndBottom) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->right = 10;
  child->bottom = 20;

  parent->children = {child};

  doc->applyLayout();

  // x = 400 - 10 - 100 = 290
  EXPECT_FLOAT_EQ(child->x, 290.0f);
  // y = 300 - 20 - 60 = 220
  EXPECT_FLOAT_EQ(child->y, 220.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - includeInLayout=false
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintWithIncludeInLayoutFalse) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  // This child opts out of layout flow but has constraints.
  auto overlay = doc->makeNode<pagx::Layer>();
  overlay->width = 200;
  overlay->height = 100;
  overlay->includeInLayout = false;
  overlay->right = 20;
  overlay->bottom = 30;

  // Normal layout children.
  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 150;
  child1->height = 100;

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 150;
  child2->height = 100;

  parent->children = {overlay, child1, child2};

  doc->applyLayout();

  // overlay: excluded from container layout, constraints apply.
  // x = 600 - 20 - 200 = 380
  EXPECT_FLOAT_EQ(overlay->x, 380.0f);
  // y = 400 - 30 - 100 = 270
  EXPECT_FLOAT_EQ(overlay->y, 270.0f);

  // child1 and child2 are laid out normally without overlay.
  EXPECT_FLOAT_EQ(child1->x, 0.0f);
  EXPECT_FLOAT_EQ(child2->x, 160.0f);  // 150 + 10
}

PAGX_TEST(PAGXTest, LayerConstraintIncludeInLayoutFalseDeriveSize) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Horizontal;

  // Overlay with no explicit size — derived from constraints.
  auto overlay = doc->makeNode<pagx::Layer>();
  overlay->includeInLayout = false;
  overlay->left = 50;
  overlay->right = 50;
  overlay->top = 40;
  overlay->bottom = 60;

  parent->children = {overlay};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(overlay->x, 50.0f);
  EXPECT_FLOAT_EQ(overlay->y, 40.0f);
  EXPECT_FLOAT_EQ(overlay->actualWidth, 500.0f);   // 600 - 50 - 50
  EXPECT_FLOAT_EQ(overlay->actualHeight, 300.0f);  // 400 - 40 - 60
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Priority (container layout wins)
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintIgnoredInContainerLayout) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  // This child has constraints but also participates in container layout.
  // Container layout should take priority — constraints ignored.
  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;
  child->height = 100;
  child->left = 999;  // Should be ignored.
  child->top = 999;   // Should be ignored.

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  child2->height = 100;

  parent->children = {child, child2};

  doc->applyLayout();

  // Container layout positions: child at x=0, child2 at x=210.
  EXPECT_FLOAT_EQ(child->x, 0.0f);
  EXPECT_FLOAT_EQ(child->y, 0.0f);
  EXPECT_FLOAT_EQ(child2->x, 210.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - No constraint, no change
// =====================================================================================

PAGX_TEST(PAGXTest, LayerNoConstraintUnchanged) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->x = 55;
  child->y = 77;
  // No constraints — x/y should remain unchanged.

  parent->children = {child};

  doc->applyLayout();

  EXPECT_FLOAT_EQ(child->x, 55.0f);
  EXPECT_FLOAT_EQ(child->y, 77.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Invisible child still gets layout
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintInvisibleChildNotSkipped) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->visible = false;
  child->left = 30;
  child->top = 40;
  child->x = 0;
  child->y = 0;

  parent->children = {child};

  doc->applyLayout();

  // Visibility does not affect layout — invisible child still gets constraint positioning.
  EXPECT_FLOAT_EQ(child->x, 30.0f);
  EXPECT_FLOAT_EQ(child->y, 40.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Multiple children
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintMultipleChildren) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto topLeft = doc->makeNode<pagx::Layer>();
  topLeft->width = 80;
  topLeft->height = 60;
  topLeft->left = 10;
  topLeft->top = 10;

  auto bottomRight = doc->makeNode<pagx::Layer>();
  bottomRight->width = 80;
  bottomRight->height = 60;
  bottomRight->right = 10;
  bottomRight->bottom = 10;

  auto centered = doc->makeNode<pagx::Layer>();
  centered->width = 120;
  centered->height = 80;
  centered->centerX = 0;
  centered->centerY = 0;

  parent->children = {topLeft, bottomRight, centered};

  doc->applyLayout();

  // topLeft: x=10, y=10
  EXPECT_FLOAT_EQ(topLeft->x, 10.0f);
  EXPECT_FLOAT_EQ(topLeft->y, 10.0f);

  // bottomRight: x=400-10-80=310, y=300-10-60=230
  EXPECT_FLOAT_EQ(bottomRight->x, 310.0f);
  EXPECT_FLOAT_EQ(bottomRight->y, 230.0f);

  // centered: x=(400-120)/2=140, y=(300-80)/2=110
  EXPECT_FLOAT_EQ(centered->x, 140.0f);
  EXPECT_FLOAT_EQ(centered->y, 110.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Measured child size (no explicit width/height)
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintMeasuredChildSize) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  // Child has no explicit width/height, but has a Rectangle content to measure.
  auto child = doc->makeNode<pagx::Layer>();
  child->right = 20;
  child->bottom = 30;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 50};
  rect->position = {40, 25};  // Center-anchored: bounds are (0, 0, 80, 50)
  child->contents.push_back(rect);

  parent->children = {child};

  doc->applyLayout();

  // Measured childWidth = 80, childHeight = 50.
  // x = parentWidth - right - childWidth = 400 - 20 - 80 = 300
  EXPECT_FLOAT_EQ(child->x, 300.0f);
  // y = parentHeight - bottom - childHeight = 300 - 30 - 50 = 220
  EXPECT_FLOAT_EQ(child->y, 220.0f);
}

// =====================================================================================
// Auto Layout - Layer Constraint Positioning - Round-trip XML
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->left = 15;
  child->right = 25;
  child->top = 35;
  child->bottom = 45;
  child->centerX = 5;
  child->centerY = -10;
  parent->children = {child};

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  // Verify constraint attributes appear in XML.
  EXPECT_NE(xml.find("left=\"15\""), std::string::npos);
  EXPECT_NE(xml.find("right=\"25\""), std::string::npos);
  EXPECT_NE(xml.find("top=\"35\""), std::string::npos);
  EXPECT_NE(xml.find("bottom=\"45\""), std::string::npos);
  EXPECT_NE(xml.find("centerX=\"5\""), std::string::npos);
  EXPECT_NE(xml.find("centerY=\"-10\""), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* parent2 = doc2->layers[0];
  ASSERT_EQ(parent2->children.size(), 1u);

  auto* child2 = parent2->children[0];
  EXPECT_FLOAT_EQ(child2->left, 15.0f);
  EXPECT_FLOAT_EQ(child2->right, 25.0f);
  EXPECT_FLOAT_EQ(child2->top, 35.0f);
  EXPECT_FLOAT_EQ(child2->bottom, 45.0f);
  EXPECT_FLOAT_EQ(child2->centerX, 5.0f);
  EXPECT_FLOAT_EQ(child2->centerY, -10.0f);
}

PAGX_TEST(PAGXTest, LayerConstraintRoundTripNanOmitted) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  // Child with only left and top set — others remain NAN.
  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->left = 10;
  child->top = 20;
  parent->children = {child};

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  // Only left and top should appear; right/bottom/centerX/centerY should be omitted.
  EXPECT_NE(xml.find("left=\"10\""), std::string::npos);
  EXPECT_NE(xml.find("top=\"20\""), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);

  auto* child2 = doc2->layers[0]->children[0];
  EXPECT_FLOAT_EQ(child2->left, 10.0f);
  EXPECT_FLOAT_EQ(child2->top, 20.0f);
  EXPECT_TRUE(std::isnan(child2->right));
  EXPECT_TRUE(std::isnan(child2->bottom));
  EXPECT_TRUE(std::isnan(child2->centerX));
  EXPECT_TRUE(std::isnan(child2->centerY));
}

/**
 * Test all PAGX examples embedded in the skill examples.md documentation.
 * Extracts complete PAGX documents from markdown code blocks and renders them.
 */
PAGX_TEST(PAGXTest, SkillExamples) {
  TestMarkdownExamples(
      context, ProjectPath::Absolute(".codebuddy/skills/pagx/references/examples.md"), "skills_");
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

// =====================================================================================
// Auto Layout - Constraint Priority (centerX/centerY highest priority)
// =====================================================================================

PAGX_TEST(PAGXTest, LayerConstraintCenterXOverridesLeft) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  // When both centerX and left are set, centerX should win (higher priority).
  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->centerX = 0;  // Horizontal center
  child->left = 50;    // Should be ignored

  parent->children = {child};

  doc->applyLayout();

  // Position should be from centerX: (400 - 100) / 2 + 0 = 150
  // NOT from left: 50
  EXPECT_FLOAT_EQ(child->x, 150.0f) << "centerX should override left";
}

PAGX_TEST(PAGXTest, LayerConstraintCenterYOverridesTop) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  // When both centerY and top are set, centerY should win (higher priority).
  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 60;
  child->centerY = 0;  // Vertical center
  child->top = 40;     // Should be ignored

  parent->children = {child};

  doc->applyLayout();

  // Position should be from centerY: (300 - 60) / 2 + 0 = 120
  // NOT from top: 40
  EXPECT_FLOAT_EQ(child->y, 120.0f) << "centerY should override top";
}

PAGX_TEST(PAGXTest, LayerConstraintCenterYMeasurementContribution) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->layout = pagx::LayoutMode::None;
  // No explicit height

  auto child = doc->makeNode<pagx::Rectangle>();
  child->size = {80, 60};
  child->position = {0, 0};
  child->centerY = -30;  // Contributes |-30| * 2 = 60 to measurement

  parent->contents = {child};

  doc->applyLayout();

  // Parent height should measure from child: |-30| * 2 + 60 = 120
  EXPECT_FLOAT_EQ(parent->actualHeight, 120.0f)
      << "Parent should measure centerY contribution as |centerY| * 2 + content_height";
}

// =====================================================================================
// Auto Layout - Priority Combination Tests
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutContainerFlexVsExplicitSize) {
  // Priority: explicit main-axis size > flex distribution.
  // When a child has both flex>0 and explicit width, explicit width wins.
  auto doc = pagx::PAGXDocument::Make(600, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 200;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->gap = 10;

  // child1: has explicit width AND flex — explicit width should win.
  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 150;
  child1->height = 100;
  child1->flex = 1;

  // child2: no explicit width, flex=1 — gets remaining space.
  auto child2 = doc->makeNode<pagx::Layer>();
  child2->height = 100;
  child2->flex = 1;

  parent->children = {child1, child2};

  doc->applyLayout();

  // child1: explicit width=150 wins over flex.
  EXPECT_FLOAT_EQ(child1->actualWidth, 150.0f) << "Explicit width should override flex";
  // child2: remaining = 600 - 150 - 10(gap) = 440, flex share = 440/1 = 440.
  EXPECT_FLOAT_EQ(child2->actualWidth, 440.0f) << "Flex child gets all remaining space";
  // Positioning: child1 at x=0, child2 at x=160.
  EXPECT_FLOAT_EQ(child1->x, 0.0f);
  EXPECT_FLOAT_EQ(child2->x, 160.0f);
}

PAGX_TEST(PAGXTest, LayoutContainerStretchVsExplicitCross) {
  // Priority: explicit cross-axis size > Stretch alignment.
  // Stretch only fills children without explicit cross-axis size.
  auto doc = pagx::PAGXDocument::Make(600, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 300;
  parent->layout = pagx::LayoutMode::Horizontal;
  parent->alignment = pagx::Alignment::Stretch;

  // child1: explicit height — Stretch should NOT override it.
  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 200;
  child1->height = 100;

  // child2: no explicit height — Stretch should fill to container height.
  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;

  parent->children = {child1, child2};

  doc->applyLayout();

  // child1: explicit height=100 preserved despite Stretch.
  EXPECT_FLOAT_EQ(child1->height, 100.0f) << "Explicit cross-axis size overrides Stretch";
  // child2: no explicit height, Stretch fills to container height=300.
  EXPECT_FLOAT_EQ(child2->actualHeight, 300.0f)
      << "Stretch fills child without explicit cross-axis size";
}

PAGX_TEST(PAGXTest, LayoutContainerConstraintIgnoredForParticipant) {
  // Priority: container layout > constraint attributes.
  // A child participating in container layout has its left/right/top/bottom ignored,
  // even if they form opposite-edge pairs.
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  parent->height = 400;
  parent->layout = pagx::LayoutMode::Horizontal;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 100;
  child->height = 80;
  child->left = 20;   // Should be ignored (container layout active).
  child->right = 30;  // Should be ignored.
  child->top = 50;    // Should be ignored.

  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  child2->height = 80;

  parent->children = {child, child2};

  doc->applyLayout();

  // Container layout positions child at x=0 (not left=20).
  EXPECT_FLOAT_EQ(child->x, 0.0f) << "Container layout ignores left constraint";
  EXPECT_FLOAT_EQ(child->y, 0.0f) << "Container layout ignores top constraint";
  // Width remains 100 (explicit), NOT derived from left+right.
  EXPECT_FLOAT_EQ(child->width, 100.0f) << "Explicit width preserved";
  EXPECT_FLOAT_EQ(child2->x, 100.0f) << "Second child positioned after first";
}

PAGX_TEST(PAGXTest, LayerConstraintOppositeEdgeOverridesExplicitSize) {
  // Priority in constraint positioning: opposite-edge constraint > explicit size.
  // When left+right both set, derived width = parent.width - left - right,
  // overriding any explicit width on the child.
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;

  auto child = doc->makeNode<pagx::Layer>();
  child->width = 200;  // Explicit width — will be overridden.
  child->height = 80;
  child->left = 10;
  child->right = 20;
  child->top = 30;
  child->bottom = 40;

  parent->children = {child};

  doc->applyLayout();

  // Horizontal: left+right overrides explicit width.
  EXPECT_FLOAT_EQ(child->x, 10.0f);
  EXPECT_FLOAT_EQ(child->actualWidth, 370.0f) << "left+right derives width: 400 - 10 - 20 = 370";
  // Vertical: top+bottom overrides explicit height.
  EXPECT_FLOAT_EQ(child->y, 30.0f);
  EXPECT_FLOAT_EQ(child->actualHeight, 230.0f) << "top+bottom derives height: 300 - 30 - 40 = 230";
}

PAGX_TEST(PAGXTest, LayoutNestedContainers) {
  // Nested containers: outer Horizontal + inner Vertical, each with flex.
  // Constraints on inner children are ignored because inner parent has container layout.
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto outer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(outer);

  outer->width = 600;
  outer->height = 400;
  outer->layout = pagx::LayoutMode::Horizontal;
  outer->alignment = pagx::Alignment::Stretch;  // Ensure children are stretched on cross-axis

  // Left column: Vertical layout with flex children.
  auto leftCol = doc->makeNode<pagx::Layer>();
  leftCol->flex = 1;
  leftCol->layout = pagx::LayoutMode::Vertical;

  auto topItem = doc->makeNode<pagx::Layer>();
  topItem->flex = 1;
  topItem->centerY = -10;  // Should be ignored (parent has Vertical layout).

  auto bottomItem = doc->makeNode<pagx::Layer>();
  bottomItem->height = 100;

  leftCol->children = {topItem, bottomItem};

  // Right column: simple fixed size.
  auto rightCol = doc->makeNode<pagx::Layer>();
  rightCol->flex = 1;
  rightCol->height = 400;

  outer->children = {leftCol, rightCol};

  doc->applyLayout();

  // Outer Horizontal: each column gets 600 / 2 = 300.
  EXPECT_FLOAT_EQ(leftCol->actualWidth, 300.0f) << "Left column flex=1 gets half width";
  EXPECT_FLOAT_EQ(rightCol->actualWidth, 300.0f) << "Right column flex=1 gets half width";

  // Left column is stretched to 400 on cross-axis (height).
  EXPECT_FLOAT_EQ(leftCol->actualHeight, 400.0f) << "Left column stretched to parent cross-axis";

  // topItem: flex=1 in Vertical, gets remaining = leftCol.height - bottomItem.height = 400 - 100 = 300.
  // centerY should be ignored since parent is Vertical layout.
  EXPECT_FLOAT_EQ(bottomItem->actualHeight, 100.0f) << "Bottom item keeps explicit height";
  EXPECT_FLOAT_EQ(topItem->actualHeight, 300.0f)
      << "Top item flex=1 gets remaining: 400 - 100 = 300";
  EXPECT_FLOAT_EQ(topItem->y, 0.0f) << "centerY ignored in container layout";
  EXPECT_FLOAT_EQ(bottomItem->y, 300.0f) << "Bottom item positioned after top";
}

PAGX_TEST(PAGXTest, LayoutIncludeInLayoutFalseNotAffectingMeasurement) {
  // includeInLayout=false children should not affect parent's measured size.
  // Only participating children contribute to measurement.
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 600;
  // height is NAN — must be measured from children.
  parent->layout = pagx::LayoutMode::Horizontal;

  // overlay: excluded from layout, has large height — should NOT affect measurement.
  auto overlay = doc->makeNode<pagx::Layer>();
  overlay->width = 50;
  overlay->height = 500;
  overlay->includeInLayout = false;
  overlay->centerY = 0;

  // child1: participates in layout, height=80.
  auto child1 = doc->makeNode<pagx::Layer>();
  child1->width = 200;
  child1->height = 80;

  // child2: participates in layout, height=120 — determines measured cross-axis.
  auto child2 = doc->makeNode<pagx::Layer>();
  child2->width = 200;
  child2->height = 120;

  parent->children = {overlay, child1, child2};

  doc->applyLayout();

  // Parent height measured from participating children only: max(80, 120) = 120.
  // overlay's height=500 should NOT influence this.
  EXPECT_FLOAT_EQ(parent->actualHeight, 120.0f)
      << "includeInLayout=false child should not affect parent measurement";
  // Participating children positioned normally.
  EXPECT_FLOAT_EQ(child1->x, 0.0f);
  EXPECT_FLOAT_EQ(child2->x, 200.0f);
}

// =====================================================================================
// Auto Layout - Group Constraint Measurement
// =====================================================================================

PAGX_TEST(PAGXTest, GroupChildConstraintAffectsMeasurement) {
  // When a Group has no explicit dimensions, its measured size should include
  // constraint offsets from child elements (e.g., left, top).
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto root = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(root);

  root->left = 0;
  root->right = 0;
  root->top = 0;
  root->bottom = 0;
  root->layout = pagx::LayoutMode::Vertical;
  root->alignment = pagx::Alignment::Center;

  auto row = doc->makeNode<pagx::Layer>();

  auto group = doc->makeNode<pagx::Group>();
  auto ellipse1 = doc->makeNode<pagx::Ellipse>();
  ellipse1->size = {100, 100};
  // No constraints — default position at center of bounding box.

  auto ellipse2 = doc->makeNode<pagx::Ellipse>();
  ellipse2->size = {100, 100};
  ellipse2->left = 160;  // Offset 160px from container left edge.

  group->elements = {ellipse1, ellipse2};
  row->contents = {group};
  root->children = {row};

  doc->applyLayout();

  // Group measured width should be 260 (0..100 for ellipse1, 160..260 for ellipse2).
  // With alignment=center, row.x = (400 - 260) / 2 = 70.
  EXPECT_FLOAT_EQ(row->x, 70.0f) << "Group child left constraint should contribute to measurement";
  EXPECT_FLOAT_EQ(row->actualWidth, 260.0f)
      << "Row width should be measured as 260 from Group contents";
}

PAGX_TEST(PAGXTest, GroupChildCenterXConstraintAffectsMeasurement) {
  // centerX constraint should contribute |centerX| * 2 + contentWidth to Group measurement.
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto root = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(root);

  root->left = 0;
  root->right = 0;
  root->top = 0;
  root->bottom = 0;
  root->layout = pagx::LayoutMode::Vertical;
  root->alignment = pagx::Alignment::Center;

  auto row = doc->makeNode<pagx::Layer>();

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 60};
  rect->centerX = 50;  // Contributes |50| * 2 + 80 = 180

  group->elements = {rect};
  row->contents = {group};
  root->children = {row};

  doc->applyLayout();

  // Group measured width = |50| * 2 + 80 = 180.
  EXPECT_FLOAT_EQ(row->actualWidth, 180.0f)
      << "Group child centerX constraint should contribute to measurement";
  EXPECT_FLOAT_EQ(row->x, 110.0f) << "Row centered: (400 - 180) / 2 = 110";
}

PAGX_TEST(PAGXTest, GroupConstraintLayoutUseMeasuredSize) {
  // When a Group without explicit dimensions contains constrained children,
  // constraint layout inside the Group should use the measured size (including
  // child constraint offsets) as the reference frame.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;

  auto group = doc->makeNode<pagx::Group>();
  // No explicit width/height — should be measured from children.

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {80, 60};
  // No constraints — at default position.

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {80, 60};
  rect2->left = 200;  // Offset from container left.

  group->elements = {rect1, rect2};
  parent->contents = {group};

  doc->applyLayout();

  // rect2 should be positioned at left=200 within the Group's measured frame.
  auto pos2 = rect2->position;
  EXPECT_FLOAT_EQ(pos2.x, 240.0f) << "rect2.position.x = left + width/2 = 200 + 40 = 240";
}

/**
 * Test case: ImagePattern supports inline image file path and data URI in addition to @id
 * reference.
 */
PAGX_TEST(PAGXTest, ImagePatternInlineImage) {
  // Test 1: Inline file path
  std::string xml1 = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Layer>
    <Rectangle size="100,100"/>
    <Fill>
      <ImagePattern image="avatar.png"/>
    </Fill>
  </Layer>
</pagx>)";

  auto doc1 = pagx::PAGXImporter::FromXML(xml1);
  ASSERT_TRUE(doc1 != nullptr);
  EXPECT_TRUE(doc1->errors.empty())
      << "Unexpected errors: " << (doc1->errors.empty() ? "" : doc1->errors[0]);
  ASSERT_EQ(doc1->layers.size(), 1u);
  auto* fill1 = static_cast<pagx::Fill*>(doc1->layers[0]->contents[1]);
  auto* pattern1 = static_cast<pagx::ImagePattern*>(fill1->color);
  ASSERT_TRUE(pattern1 != nullptr);
  ASSERT_TRUE(pattern1->image != nullptr);
  EXPECT_TRUE(pattern1->image->id.empty());
  EXPECT_EQ(pattern1->image->filePath, "avatar.png");
  EXPECT_TRUE(pattern1->image->data == nullptr);

  // Test 2: Inline base64 data URI
  std::string xml2 = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Layer>
    <Rectangle size="100,100"/>
    <Fill>
      <ImagePattern image="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="/>
    </Fill>
  </Layer>
</pagx>)";

  auto doc2 = pagx::PAGXImporter::FromXML(xml2);
  ASSERT_TRUE(doc2 != nullptr);
  EXPECT_TRUE(doc2->errors.empty())
      << "Unexpected errors: " << (doc2->errors.empty() ? "" : doc2->errors[0]);
  ASSERT_EQ(doc2->layers.size(), 1u);
  auto* fill2 = static_cast<pagx::Fill*>(doc2->layers[0]->contents[1]);
  auto* pattern2 = static_cast<pagx::ImagePattern*>(fill2->color);
  ASSERT_TRUE(pattern2 != nullptr);
  ASSERT_TRUE(pattern2->image != nullptr);
  EXPECT_TRUE(pattern2->image->id.empty());
  EXPECT_TRUE(pattern2->image->data != nullptr);
  EXPECT_TRUE(pattern2->image->filePath.empty());

  // Test 3: Round-trip for inline file path
  std::string exported1 = pagx::PAGXExporter::ToXML(*doc1);
  EXPECT_NE(exported1.find("image=\"avatar.png\""), std::string::npos);
  auto doc3 = pagx::PAGXImporter::FromXML(exported1);
  ASSERT_TRUE(doc3 != nullptr);
  EXPECT_TRUE(doc3->errors.empty());
  auto* fill3 = static_cast<pagx::Fill*>(doc3->layers[0]->contents[1]);
  auto* pattern3 = static_cast<pagx::ImagePattern*>(fill3->color);
  ASSERT_TRUE(pattern3 != nullptr);
  ASSERT_TRUE(pattern3->image != nullptr);
  EXPECT_EQ(pattern3->image->filePath, "avatar.png");

  // Test 4: Round-trip for inline base64 data URI
  std::string exported2 = pagx::PAGXExporter::ToXML(*doc2);
  EXPECT_NE(exported2.find("data:image/png;base64,"), std::string::npos);
  auto doc4 = pagx::PAGXImporter::FromXML(exported2);
  ASSERT_TRUE(doc4 != nullptr);
  EXPECT_TRUE(doc4->errors.empty());
  auto* fill4 = static_cast<pagx::Fill*>(doc4->layers[0]->contents[1]);
  auto* pattern4 = static_cast<pagx::ImagePattern*>(fill4->color);
  ASSERT_TRUE(pattern4 != nullptr);
  ASSERT_TRUE(pattern4->image != nullptr);
  EXPECT_TRUE(pattern4->image->data != nullptr);

  // Test 5: @id reference still works (backward compatibility)
  std::string xml5 = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="100" height="100">
  <Layer>
    <Rectangle size="100,100"/>
    <Fill>
      <ImagePattern image="@img1"/>
    </Fill>
  </Layer>
  <Resources>
    <Image id="img1" source="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg=="/>
  </Resources>
</pagx>)";

  auto doc5 = pagx::PAGXImporter::FromXML(xml5);
  ASSERT_TRUE(doc5 != nullptr);
  EXPECT_TRUE(doc5->errors.empty())
      << "Unexpected errors: " << (doc5->errors.empty() ? "" : doc5->errors[0]);
  auto* fill5 = static_cast<pagx::Fill*>(doc5->layers[0]->contents[1]);
  auto* pattern5 = static_cast<pagx::ImagePattern*>(fill5->color);
  ASSERT_TRUE(pattern5 != nullptr);
  ASSERT_TRUE(pattern5->image != nullptr);
  EXPECT_EQ(pattern5->image->id, "img1");
  EXPECT_TRUE(pattern5->image->data != nullptr);
}

// =====================================================================================
// ClipToBounds
// =====================================================================================

/**
 * Test that clipToBounds sets scrollRect during layout when the layer has resolved dimensions.
 */
PAGX_TEST(PAGXTest, ClipToBoundsDoesNotModifyNode) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);
  parent->width = 200;
  parent->height = 200;
  parent->clipToBounds = true;

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};
  child->height = 100;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {300, 100};
  child->contents.push_back(rect);

  doc->applyLayout();

  // clipToBounds writes scrollRect during layout phase.
  EXPECT_TRUE(parent->hasScrollRect);
  EXPECT_EQ(parent->scrollRect.x, 0);
  EXPECT_EQ(parent->scrollRect.y, 0);
  EXPECT_EQ(parent->scrollRect.width, 200);
  EXPECT_EQ(parent->scrollRect.height, 200);
  EXPECT_TRUE(parent->clipToBounds);
}

/**
 * Test that clipToBounds is parsed correctly from XML.
 */
PAGX_TEST(PAGXTest, ClipToBoundsParseFromXML) {
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer width="200" height="200" clipToBounds="true">
    <Rectangle size="300,300"/>
    <Fill color="#F00"/>
  </Layer>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty());
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_TRUE(doc->layers[0]->clipToBounds);
  EXPECT_FALSE(doc->layers[0]->hasScrollRect);
}

/**
 * Test that explicit scrollRect takes priority over clipToBounds.
 */
PAGX_TEST(PAGXTest, ClipToBoundsScrollRectPriority) {
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer width="200" height="200" scrollRect="10,10,100,100" clipToBounds="true">
    <Rectangle size="300,300"/>
    <Fill color="#F00"/>
  </Layer>
</pagx>)";

  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  ASSERT_EQ(doc->layers.size(), 1u);

  auto* layer = doc->layers[0];
  // scrollRect should be parsed, clipToBounds should also be true.
  EXPECT_TRUE(layer->hasScrollRect);
  EXPECT_TRUE(layer->clipToBounds);
  EXPECT_EQ(layer->scrollRect.x, 10);
  EXPECT_EQ(layer->scrollRect.y, 10);
  EXPECT_EQ(layer->scrollRect.width, 100);
  EXPECT_EQ(layer->scrollRect.height, 100);
}

/**
 * Test that clipToBounds survives export and re-import (round trip).
 */
PAGX_TEST(PAGXTest, ClipToBoundsRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 200;
  layer->clipToBounds = true;

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_EQ(doc2->layers.size(), 1u);
  EXPECT_TRUE(doc2->layers[0]->clipToBounds);
  EXPECT_FALSE(doc2->layers[0]->hasScrollRect);
}

/**
 * Test that clipToBounds works with auto-measured container (no explicit dimensions).
 */
PAGX_TEST(PAGXTest, ClipToBoundsAutoMeasured) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);
  // No explicit width/height — will be measured from children.
  parent->layout = pagx::LayoutMode::Vertical;
  parent->clipToBounds = true;

  auto child = doc->makeNode<pagx::Layer>();
  parent->children = {child};
  child->width = 200;
  child->height = 100;

  doc->applyLayout();

  // After layout, parent should have measured dimensions from children.
  EXPECT_EQ(parent->actualWidth, 200);
  EXPECT_EQ(parent->actualHeight, 100);
  // clipToBounds writes scrollRect based on actualWidth/actualHeight.
  EXPECT_TRUE(parent->hasScrollRect);
  EXPECT_EQ(parent->scrollRect.width, 200);
  EXPECT_EQ(parent->scrollRect.height, 100);
}

// =====================================================================================
// Auto Layout - Refactoring Correctness (P0)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutIdempotent) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 80};
  rect->left = 20;
  rect->top = 10;
  layer->contents.push_back(rect);

  doc->applyLayout();

  float firstX = rect->position.x;
  float firstY = rect->position.y;
  float firstW = rect->size.width;
  float firstH = rect->size.height;
  float layerActualW = layer->actualWidth;
  float layerActualH = layer->actualHeight;

  // Reset layout cache and re-layout (simulates setFontConfig triggering re-layout).
  pagx::FontConfig config;
  doc->setFontConfig(config);
  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->position.x, firstX);
  EXPECT_FLOAT_EQ(rect->position.y, firstY);
  EXPECT_FLOAT_EQ(rect->size.width, firstW);
  EXPECT_FLOAT_EQ(rect->size.height, firstH);
  EXPECT_FLOAT_EQ(layer->actualWidth, layerActualW);
  EXPECT_FLOAT_EQ(layer->actualHeight, layerActualH);
}

PAGX_TEST(PAGXTest, FlexDoesNotWriteBackWidthHeight) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);
  parent->width = 300;
  parent->height = 100;
  parent->layout = pagx::LayoutMode::Horizontal;

  auto child1 = doc->makeNode<pagx::Layer>();
  child1->flex = 1;
  auto child2 = doc->makeNode<pagx::Layer>();
  child2->flex = 2;
  parent->children = {child1, child2};

  doc->applyLayout();

  // User properties should NOT be modified by layout.
  EXPECT_TRUE(std::isnan(child1->width));
  EXPECT_TRUE(std::isnan(child2->width));
  // Actual sizes should be set.
  EXPECT_FLOAT_EQ(child1->actualWidth, 100);
  EXPECT_FLOAT_EQ(child2->actualWidth, 200);
}

// =====================================================================================
// Auto Layout - Edge Case Fixes (P2)
// =====================================================================================

PAGX_TEST(PAGXTest, ZeroSizeElementWithConstraint) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  // Default size is {0,0}, with left+right should stretch to fill.
  rect->left = 20;
  rect->right = 20;
  rect->top = 30;
  rect->bottom = 30;
  layer->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_FLOAT_EQ(rect->size.width, 160);
  EXPECT_FLOAT_EQ(rect->size.height, 140);
  EXPECT_FLOAT_EQ(rect->actualWidth, 160);
  EXPECT_FLOAT_EQ(rect->actualHeight, 140);
}

PAGX_TEST(PAGXTest, FlexRoundingErrorPropagation) {
  auto doc = pagx::PAGXDocument::Make(100, 50);
  auto parent = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(parent);
  parent->width = 100;
  parent->height = 50;
  parent->layout = pagx::LayoutMode::Horizontal;

  auto c1 = doc->makeNode<pagx::Layer>();
  c1->flex = 1;
  auto c2 = doc->makeNode<pagx::Layer>();
  c2->flex = 1;
  auto c3 = doc->makeNode<pagx::Layer>();
  c3->flex = 1;
  parent->children = {c1, c2, c3};

  doc->applyLayout();

  // Total should exactly equal container width (100), no gaps.
  float total = c1->actualWidth + c2->actualWidth + c3->actualWidth;
  EXPECT_FLOAT_EQ(total, 100);
}

// =====================================================================================
// Auto Layout - Architectural Integrity (P3)
// =====================================================================================

PAGX_TEST(PAGXTest, TransformDoesNotAffectLayout) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 400;

  auto group = doc->makeNode<pagx::Group>();
  group->rotation = 45;
  group->scale = {2, 2};

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  rect->left = 50;
  rect->top = 50;
  group->elements.push_back(rect);
  layer->contents.push_back(group);

  doc->applyLayout();

  // Transform should not affect constraint positioning.
  EXPECT_FLOAT_EQ(rect->position.x, 100);  // left=50 + width*0.5=50
  EXPECT_FLOAT_EQ(rect->position.y, 100);  // top=50 + height*0.5=50
}

PAGX_TEST(PAGXTest, DeepNestingConstraintPropagation) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 400;

  auto outerGroup = doc->makeNode<pagx::Group>();
  outerGroup->width = 300;
  outerGroup->height = 300;
  outerGroup->left = 50;
  outerGroup->top = 50;

  auto innerGroup = doc->makeNode<pagx::Group>();
  innerGroup->width = 200;
  innerGroup->height = 200;
  innerGroup->left = 20;
  innerGroup->top = 20;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 60};
  rect->left = 10;
  rect->top = 10;

  innerGroup->elements.push_back(rect);
  outerGroup->elements.push_back(innerGroup);
  layer->contents.push_back(outerGroup);

  doc->applyLayout();

  // rect position: left=10 + width*0.5=40 -> center at 50
  EXPECT_FLOAT_EQ(rect->position.x, 50);
  EXPECT_FLOAT_EQ(rect->position.y, 40);
  // innerGroup positioned at left=20, top=20
  EXPECT_FLOAT_EQ(innerGroup->position.x, 20);
  EXPECT_FLOAT_EQ(innerGroup->position.y, 20);
  // outerGroup positioned at left=50, top=50
  EXPECT_FLOAT_EQ(outerGroup->position.x, 50);
  EXPECT_FLOAT_EQ(outerGroup->position.y, 50);
}

// =====================================================================================
// Auto Layout - New Feature Tests (P1)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutTextBaselineAlphabetic) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);
  auto fontFamily = typeface->fontFamily();
  auto fontStyle = typeface->fontStyle();

  // Text with LineBox baseline (default).
  auto textLB = doc->makeNode<pagx::Text>();
  textLB->text = "Hello";
  textLB->fontFamily = fontFamily;
  textLB->fontStyle = fontStyle;
  textLB->fontSize = 48;
  textLB->position = {50, 100};
  textLB->baseline = pagx::TextBaseline::LineBox;

  // Text with Alphabetic baseline.
  auto textAB = doc->makeNode<pagx::Text>();
  textAB->text = "Hello";
  textAB->fontFamily = fontFamily;
  textAB->fontStyle = fontStyle;
  textAB->fontSize = 48;
  textAB->position = {200, 100};
  textAB->baseline = pagx::TextBaseline::Alphabetic;

  layer->contents = {textLB, textAB};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // Both texts should have TextBlobs.
  auto blobLB = textLB->getTextBlob();
  auto blobAB = textAB->getTextBlob();
  ASSERT_NE(blobLB, nullptr);
  ASSERT_NE(blobAB, nullptr);

  // LineBox: TextBlob bounds.top should be positive (half-leading pushes glyphs down from
  // linebox top, so the tight bounds top is above 0 relative to position.y = linebox top).
  auto boundsLB = blobLB->getTightBounds();
  EXPECT_GT(boundsLB.top, 0);

  // Alphabetic: TextBlob bounds.top should be negative (ascender above baseline).
  auto boundsAB = blobAB->getTightBounds();
  EXPECT_LT(boundsAB.top, 0);
}

PAGX_TEST(PAGXTest, LayoutTextIndependentConstraint) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Test";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 30;
  text->left = 50;
  text->top = 40;
  layer->contents.push_back(text);

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);

  // Compute original bounds before layout.
  pagx::LayoutContext layoutContext(&fontConfig);
  pagx::TextLayoutParams params = {};
  params.baseline = text->baseline;
  auto origBounds = pagx::TextLayout::Layout({text}, params, layoutContext);

  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // No opposite-edge constraints, so fontSize should remain unchanged.
  EXPECT_FLOAT_EQ(text->fontSize, 30);

  // Position is set by constraint: position = constraintOffset - textBounds.x/y.
  // With left=50: x = 50 - origBounds.bounds.x
  // With top=40: y = 40 - origBounds.bounds.y
  EXPECT_FLOAT_EQ(text->position.x, 50 - origBounds.bounds.x);
  EXPECT_FLOAT_EQ(text->position.y, 40 - origBounds.bounds.y);
}

PAGX_TEST(PAGXTest, LayoutTextPathMeasurement) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto group = doc->makeNode<pagx::Group>();
  // No explicit width/height: Group measures from children.

  auto textPath = doc->makeNode<pagx::TextPath>();
  textPath->path = doc->makeNode<pagx::PathData>();
  textPath->path->moveTo(0, 0);
  textPath->path->lineTo(200, 0);
  textPath->path->lineTo(200, 100);
  textPath->path->lineTo(0, 100);
  textPath->path->close();

  group->elements.push_back(textPath);
  layer->contents.push_back(group);

  doc->applyLayout();

  // TextPath path bounds: 200x100.
  EXPECT_FLOAT_EQ(textPath->preferredWidth, 200);
  EXPECT_FLOAT_EQ(textPath->preferredHeight, 100);

  // Group should have measured from TextPath bounds.
  EXPECT_FLOAT_EQ(group->preferredWidth, 200);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
  EXPECT_FLOAT_EQ(group->actualWidth, 200);
  EXPECT_FLOAT_EQ(group->actualHeight, 100);
}

// =====================================================================================
// Auto Layout - Edge Case Fixes (P2 continued)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutConstraintScalePathSingleAxis) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto path = doc->makeNode<pagx::Path>();
  path->data = doc->makeNode<pagx::PathData>();
  path->data->moveTo(0, 0);
  path->data->lineTo(100, 0);
  path->data->lineTo(100, 80);
  path->data->lineTo(0, 80);
  path->data->close();
  // Only horizontal constraint: left+right gives targetW=360, no targetH.
  path->left = 20;
  path->right = 20;
  layer->contents.push_back(path);

  doc->applyLayout();

  // scale = 360 / 100 = 3.6
  EXPECT_FLOAT_EQ(path->actualWidth, 360);
  // Height should scale proportionally: 80 * 3.6 = 288.
  EXPECT_FLOAT_EQ(path->actualHeight, 288);
}

PAGX_TEST(PAGXTest, LayoutTextScaledPositionAnchor) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 200;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "ABC";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 30;
  text->textAnchor = pagx::TextAnchor::Center;
  text->left = 50;
  text->right = 50;
  layer->contents.push_back(text);

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // Target width = 400 - 50 - 50 = 300.
  // After scaling, text should be centered in the 300px area.
  auto blob = text->getTextBlob();
  ASSERT_NE(blob, nullptr);
  // Verify the scaled text has a valid actual size.
  EXPECT_GT(text->actualWidth, 0);
  // fontSize should have been scaled (target 300px is different from original width).
  EXPECT_NE(text->fontSize, 30);
}

// =====================================================================================
// Auto Layout - Architectural Integrity (P3 continued)
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutTextInTextBoxSkipConstraint) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 300;
  textBox->height = 200;
  textBox->left = 50;
  textBox->top = 50;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Inside TextBox";
  text->fontSize = 20;
  // Set constraints on Text inside TextBox — these should be ignored.
  text->left = 10;
  text->right = 10;
  text->top = 5;
  text->bottom = 5;
  textBox->elements.push_back(text);
  layer->contents.push_back(textBox);

  doc->applyLayout();

  // Text inside TextBox should NOT have fontSize scaled by constraints.
  EXPECT_FLOAT_EQ(text->fontSize, 20);
}

// =====================================================================================
// Variable naming cleanup
// =====================================================================================

// =====================================================================================
// TextBox Group Transform Inverse Compensation
// =====================================================================================

PAGX_TEST(PAGXTest, LayoutTextBoxGroupPosition) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 200;
  textBox->height = 100;

  auto group = doc->makeNode<pagx::Group>();
  group->position = {50, 30};

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();

  group->elements = {text, fill};
  textBox->elements = {group};
  layer->contents = {textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto blob = text->getTextBlob();
  ASSERT_NE(blob, nullptr);

  // The layout engine places text at box-absolute position, then applies inverse of
  // Group(position=50,30) * Text(position=0,0) = translate(50,30).
  // So TextBlob coordinates should be shifted back by (-50, -30) from box-absolute position.
  auto bounds = blob->getTightBounds();
  EXPECT_LT(bounds.left, 0);
  EXPECT_LT(bounds.top, 0);
}

PAGX_TEST(PAGXTest, LayoutTextBoxGroupScale) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 200;
  textBox->height = 100;

  auto group = doc->makeNode<pagx::Group>();
  group->scale = {2, 2};

  auto text = doc->makeNode<pagx::Text>();
  text->text = "AB";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();
  group->elements = {text, fill};
  textBox->elements = {group};
  layer->contents = {textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto blob = text->getTextBlob();
  ASSERT_NE(blob, nullptr);

  // Group scale=2 means inverse scale=0.5.
  // TextBlob coordinates should be half of the box-absolute positions.
  auto bounds = blob->getTightBounds();
  EXPECT_GT(bounds.right, 0);
  EXPECT_LT(bounds.right, 100);
}

PAGX_TEST(PAGXTest, LayoutTextAnchorCenterRegression) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 200;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Center";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 32;
  text->position = {200, 100};
  text->textAnchor = pagx::TextAnchor::Center;

  auto fill = doc->makeNode<pagx::Fill>();
  layer->contents = {text, fill};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto blob = text->getTextBlob();
  ASSERT_NE(blob, nullptr);

  // With Center anchor, TextBlob should be centered around x=0 in Text's local space.
  auto bounds = blob->getTightBounds();
  EXPECT_LT(bounds.left, 0);
  EXPECT_GT(bounds.right, 0);
  EXPECT_NEAR(bounds.left + bounds.right, 0, bounds.right * 0.3f);
}

PAGX_TEST(PAGXTest, LayoutTextBoxNestedGroupTransform) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 300;
  textBox->height = 200;

  auto outerGroup = doc->makeNode<pagx::Group>();
  outerGroup->position = {20, 10};

  auto innerGroup = doc->makeNode<pagx::Group>();
  innerGroup->position = {30, 20};

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Nested";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();
  innerGroup->elements = {text, fill};
  outerGroup->elements = {innerGroup};
  textBox->elements = {outerGroup};
  layer->contents = {textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto blob = text->getTextBlob();
  ASSERT_NE(blob, nullptr);

  // Combined offset: outerGroup(20,10) + innerGroup(30,20) = (50,30).
  // TextBlob should be shifted back by (-50,-30) from box-absolute position.
  auto bounds = blob->getTightBounds();
  EXPECT_LT(bounds.left, 0);
  EXPECT_LT(bounds.top, 0);
}

/**
 * Test case: TextLayoutGlyphRun data integrity after layout.
 * Verifies that layout produces non-empty glyph runs with correct glyph count and positions.
 */
PAGX_TEST(PAGXTest, TextLayoutGlyphRunIntegrity) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();
  layer->contents = {text, fill};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto& layoutRuns = text->getLayoutRuns();
  ASSERT_FALSE(layoutRuns.empty());

  size_t totalGlyphs = 0;
  for (auto& run : layoutRuns) {
    EXPECT_NE(run.font.getTypeface(), nullptr);
    EXPECT_FALSE(run.glyphs.empty());
    EXPECT_EQ(run.positions.size(), run.glyphs.size());
    totalGlyphs += run.glyphs.size();
  }
  EXPECT_GE(totalGlyphs, 5u);
}

/**
 * Test case: TextBox child Text produces layout runs with correct count.
 */
PAGX_TEST(PAGXTest, TextBoxLayoutGlyphRunIntegrity) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 300;
  layer->height = 200;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 200;
  textBox->height = 100;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Box text";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 20;

  auto fill = doc->makeNode<pagx::Fill>();
  textBox->elements = {text, fill};
  layer->contents = {textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto& layoutRuns = text->getLayoutRuns();
  ASSERT_FALSE(layoutRuns.empty());
  size_t totalGlyphs = 0;
  for (auto& run : layoutRuns) {
    EXPECT_EQ(run.positions.size(), run.glyphs.size());
    totalGlyphs += run.glyphs.size();
  }
  EXPECT_GE(totalGlyphs, 8u);
}

/**
 * Test case: FontEmbedder re-embedding after ClearEmbeddedGlyphRuns.
 * Embeds fonts, clears, re-embeds, and verifies the result is consistent.
 */
PAGX_TEST(PAGXTest, FontEmbedderReEmbed) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Embed";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();
  layer->contents = {text, fill};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();
  pagx::FontEmbedder().embed(doc.get());

  ASSERT_FALSE(text->glyphRuns.empty());
  size_t firstGlyphCount = text->glyphRuns[0]->glyphs.size();
  auto firstGlyphs = text->glyphRuns[0]->glyphs;

  // Re-embed: clear existing glyphs, re-layout, re-embed.
  pagx::FontEmbedder::ClearEmbeddedGlyphRuns(doc.get());
  EXPECT_TRUE(text->glyphRuns.empty());

  doc->applyLayout();
  pagx::FontEmbedder().embed(doc.get());

  ASSERT_FALSE(text->glyphRuns.empty());
  // Glyph count and IDs should match after re-embedding.
  EXPECT_EQ(text->glyphRuns[0]->glyphs.size(), firstGlyphCount);
  EXPECT_EQ(text->glyphRuns[0]->glyphs, firstGlyphs);
}

/**
 * Test case: Vertical text layout produces TextLayoutGlyphRun with rotations.
 */
PAGX_TEST(PAGXTest, VerticalTextLayoutGlyphRun) {
  auto doc = pagx::PAGXDocument::Make(200, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 100;
  textBox->height = 250;
  textBox->writingMode = pagx::WritingMode::Vertical;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "ABC";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 30;

  auto fill = doc->makeNode<pagx::Fill>();
  textBox->elements = {text, fill};
  layer->contents = {textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  auto& layoutRuns = text->getLayoutRuns();
  ASSERT_FALSE(layoutRuns.empty());

  // Vertical Latin text should have per-glyph xforms (rotation for sideways glyphs).
  bool hasXforms = false;
  for (auto& run : layoutRuns) {
    if (!run.xforms.empty()) {
      hasXforms = true;
    }
  }
  EXPECT_TRUE(hasXforms);

  // Verify TextBlob was generated.
  auto blob = text->getTextBlob();
  EXPECT_NE(blob, nullptr);
}

/**
 * Test case: textBounds is set correctly for standalone Text and TextBox child Text.
 */
PAGX_TEST(PAGXTest, TextBoundsDirectValidation) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 400;
  layer->height = 300;

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);

  // Standalone Text.
  auto standalone = doc->makeNode<pagx::Text>();
  standalone->text = "Stand";
  standalone->fontFamily = typeface->fontFamily();
  standalone->fontStyle = typeface->fontStyle();
  standalone->fontSize = 24;

  auto fill1 = doc->makeNode<pagx::Fill>();

  // TextBox child Text.
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 200;
  textBox->height = 100;
  auto boxText = doc->makeNode<pagx::Text>();
  boxText->text = "InBox";
  boxText->fontFamily = typeface->fontFamily();
  boxText->fontStyle = typeface->fontStyle();
  boxText->fontSize = 24;
  auto fill2 = doc->makeNode<pagx::Fill>();
  textBox->elements = {boxText, fill2};

  layer->contents = {standalone, fill1, textBox};

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->setFontConfig(fontConfig);
  doc->applyLayout();

  // Standalone Text: textBounds should have positive width and height.
  auto standaloneBounds = standalone->getTextBounds();
  EXPECT_GT(standaloneBounds.width, 0);
  EXPECT_GT(standaloneBounds.height, 0);

  // TextBox child Text: textBounds should also have positive dimensions.
  auto boxTextBounds = boxText->getTextBounds();
  EXPECT_GT(boxTextBounds.width, 0);
  EXPECT_GT(boxTextBounds.height, 0);
}

// =====================================================================================
// Auto Layout - Repeater Measurement
// =====================================================================================

PAGX_TEST(PAGXTest, RepeaterBasicMeasurement) {
  // Repeater with position offset should expand the Group's measured size.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 3;
  repeater->position = {50, 50};
  repeater->scale = {1, 1};
  repeater->offset = 0;

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // 3 copies: progress 0, 1, 2 → positions (0,0), (50,50), (100,100)
  // Source 100x100 at each position → union: (0,0)-(200,200)
  EXPECT_FLOAT_EQ(group->preferredWidth, 200);
  EXPECT_FLOAT_EQ(group->preferredHeight, 200);
}

PAGX_TEST(PAGXTest, RepeaterScaledMeasurement) {
  // Repeater with scale should expand the measured size exponentially.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {0, 0};
  repeater->scale = {2, 2};

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // 2 copies: progress 0 (scale 1x) → 100x100, progress 1 (scale 2x) → 200x200
  // Union: (0,0)-(200,200)
  EXPECT_FLOAT_EQ(group->preferredWidth, 200);
  EXPECT_FLOAT_EQ(group->preferredHeight, 200);
}

PAGX_TEST(PAGXTest, RepeaterWithOffset) {
  // Offset is an animation parameter and does not affect measurement (initial frame).
  // Measurement always uses progress = i, ignoring offset.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {50, 0};
  repeater->scale = {1, 1};
  repeater->offset = 1;

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // 2 copies: progress 0 -> pos (0,0), progress 1 -> pos (50,0)
  // Transformed: (0,0)-(100,100) and (50,0)-(150,100)
  // totalBounds = (0,0)-(150,100), right=150
  // maxX = max(rect's 100, 150) = 150
  EXPECT_FLOAT_EQ(group->preferredWidth, 150);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

PAGX_TEST(PAGXTest, RepeaterMultipleSourceElements) {
  // Repeater should act on the union bounds of all preceding LayoutNodes.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {100, 50};
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {200, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {0, 100};
  repeater->scale = {1, 1};

  group->elements = {rect1, rect2, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // rect1: extX=100, extY=50. rect2: extX=200, extY=100.
  // sourceBounds = (0,0)-(200,100)
  // 2 copies: progress 0 -> (0,0)-(200,100), progress 1 -> (0,100)-(200,200)
  // Total: (0,0)-(200,200)
  EXPECT_FLOAT_EQ(group->preferredWidth, 200);
  EXPECT_FLOAT_EQ(group->preferredHeight, 200);
}

PAGX_TEST(PAGXTest, RepeaterZeroCopies) {
  // Repeater with copies=0 should not affect measurement.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 0;

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // copies=0 means Repeater is skipped. Group measures only the Rectangle.
  EXPECT_FLOAT_EQ(group->preferredWidth, 100);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

PAGX_TEST(PAGXTest, RepeaterNoSourceLayoutNode) {
  // Repeater with no preceding LayoutNode should be skipped.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto fill = doc->makeNode<pagx::Fill>();
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 3;
  repeater->position = {50, 50};

  group->elements = {fill, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // No LayoutNode before Repeater, so sourceBounds is empty.
  // Group has no measurable children.
  EXPECT_FLOAT_EQ(group->preferredWidth, 0);
  EXPECT_FLOAT_EQ(group->preferredHeight, 0);
}

PAGX_TEST(PAGXTest, RepeaterNegativeCopies) {
  // copies < 0 is a no-op in tgfx, should not affect measurement.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = -1;
  repeater->position = {50, 50};

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  EXPECT_FLOAT_EQ(group->preferredWidth, 100);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

PAGX_TEST(PAGXTest, RepeaterFractionalCopies) {
  // Fractional copies: ceil(2.5) = 3 copies. Measurement should cover all 3.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2.5f;
  repeater->position = {50, 0};
  repeater->scale = {1, 1};

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // ceil(2.5) = 3 copies. progress: 0, 1, 2 -> positions: (0,0), (50,0), (100,0)
  // Union: (0,0)-(200,100)
  EXPECT_FLOAT_EQ(group->preferredWidth, 200);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

PAGX_TEST(PAGXTest, RepeaterSingleCopy) {
  // copies=1 means one copy at progress=0, which is the identity transform.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 1;
  repeater->position = {200, 200};

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // 1 copy at progress=0 -> identity matrix -> same as source bounds.
  EXPECT_FLOAT_EQ(group->preferredWidth, 100);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

PAGX_TEST(PAGXTest, RepeaterWithRotation) {
  // Rotation should expand the bounding box.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {0, 0};
  repeater->scale = {1, 1};
  repeater->rotation = 45;

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // Copy 0: progress=0, identity -> (0,0)-(100,100)
  // Copy 1: progress=1, 45-degree rotation around origin
  // Rotated rect maps to (-70.7, 0)-(70.7, 141.4)
  // totalBounds = (-70.7, 0)-(100, 141.4), right=100, bottom=141.4
  // Width stays at 100 (negative-side extension doesn't increase right edge),
  // height expands to ceil(141.4) = 142.
  EXPECT_FLOAT_EQ(group->preferredWidth, 100);
  EXPECT_GT(group->preferredHeight, 141);
}

PAGX_TEST(PAGXTest, RepeaterWithAnchor) {
  // Non-zero anchor shifts the rotation/scale center.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {0, 0};
  repeater->scale = {2, 2};
  repeater->anchor = {50, 50};

  group->elements = {rect, repeater};
  layer->contents = {group};
  doc->applyLayout();

  // Copy 0: progress=0, identity -> (0,0)-(100,100)
  // Copy 1: progress=1, scale 2x around anchor (50,50)
  // Scaled rect: (-50,-50)-(150,150)
  // totalBounds = (-50,-50)-(150,150), right=150, bottom=150
  EXPECT_FLOAT_EQ(group->preferredWidth, 150);
  EXPECT_FLOAT_EQ(group->preferredHeight, 150);
}

PAGX_TEST(PAGXTest, RepeaterFollowedByLayoutNode) {
  // A LayoutNode after Repeater should still contribute to the Group size.
  auto doc = pagx::PAGXDocument::Make(800, 600);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 800;
  layer->height = 600;

  auto group = doc->makeNode<pagx::Group>();
  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {100, 100};
  auto repeater = doc->makeNode<pagx::Repeater>();
  repeater->copies = 2;
  repeater->position = {50, 0};
  repeater->scale = {1, 1};
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {500, 50};

  group->elements = {rect1, repeater, rect2};
  layer->contents = {group};
  doc->applyLayout();

  // Repeater: (0,0)-(100,100) + (50,0)-(150,100) = (0,0)-(150,100)
  // rect2: (0,0)-(500,50) expands width to 500
  // Final: max(150,500)=500, max(100,50)=100
  EXPECT_FLOAT_EQ(group->preferredWidth, 500);
  EXPECT_FLOAT_EQ(group->preferredHeight, 100);
}

}  // namespace pag
