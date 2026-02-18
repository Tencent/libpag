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
#include "pagx/nodes/Ellipse.h"
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
#include "pagx/nodes/TextBox.h"
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
  auto layout = doc->makeNode<pagx::TextBox>();
  layout->position = {centerX, y};
  layout->textAlign = pagx::TextAlign::Center;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(layout);
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

  std::string svgDir = ProjectPath::Absolute("resources/apitest/SVG");
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
      continue;
    }

    float pagxWidth = doc->width;
    float pagxHeight = doc->height;
    if (pagxWidth <= 0 || pagxHeight <= 0) {
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
 * Test case: Verify Layer can directly contain Shape + Fill without Group wrapper.
 */
PAGX_TEST(PAGXTest, LayerDirectContent) {
  // Test 1: Group vs no-Group should render identically (left=Group, right=direct)
  {
    auto pagxPath = ProjectPath::Absolute("resources/pagx/layer_direct_content.pagx");
    auto doc = pagx::PAGXImporter::FromFile(pagxPath);
    ASSERT_TRUE(doc != nullptr);

    auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxLayer != nullptr);

    auto surface = Surface::Make(context, 200, 100);
    DisplayList displayList;
    displayList.root()->addChild(tgfxLayer);
    displayList.render(surface.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PAGXLayerDirect"));
  }

  // Test 2: PAGX Layer with LinearGradient fill
  {
    auto pagxPath = ProjectPath::Absolute("resources/pagx/linear_gradient.pagx");
    auto doc = pagx::PAGXImporter::FromFile(pagxPath);
    ASSERT_TRUE(doc != nullptr);

    auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxLayer != nullptr);

    auto surface = Surface::Make(context, 200, 100);
    DisplayList displayList;
    displayList.root()->addChild(tgfxLayer);
    displayList.render(surface.get(), false);
    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/PAGXLinearGradient"));
  }
}

/**
 * Test case: Verify PAGXImporter::FromFile and FromXML produce identical results when rendered.
 */
PAGX_TEST(PAGXTest, LayerBuilderAPIConsistency) {
  auto pagxPath = ProjectPath::Absolute("resources/pagx/api_consistency.pagx");

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
 * Test case: Text shaping round-trip - verifies that original and pre-shaped renders are identical.
 */
PAGX_TEST(PAGXTest, TextShaperRoundTrip) {
  std::string svgPath = ProjectPath::Absolute("resources/apitest/SVG/text.svg");

  auto doc = pagx::SVGImporter::Parse(svgPath);
  ASSERT_TRUE(doc != nullptr);

  int canvasWidth = static_cast<int>(doc->width);
  int canvasHeight = static_cast<int>(doc->height);

  auto typefaces = GetFallbackTypefaces();

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(typefaces);
  auto layoutResult = textLayout.layout(doc.get());
  EXPECT_FALSE(layoutResult.shapedTextMap.empty());
  pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap,
                             layoutResult.textOrder);

  auto originalLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(originalLayer != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalLayer);
  originalDL.render(originalSurface.get(), false);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  std::string pagxPath = SavePAGXFile(xml, "PAGXTest/text_preshaped.pagx");

  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);
  auto reloadedLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(reloadedLayer != nullptr);

  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedLayer);
  preshapedDL.render(preshapedSurface.get(), false);

  EXPECT_TRUE(Baseline::Compare(originalSurface, "PAGXTest/TextShaper_Original"));
  EXPECT_TRUE(Baseline::Compare(preshapedSurface, "PAGXTest/TextShaper_PreShaped"));
}

/**
 * Test case: Text shaping with emoji (mixed vector and bitmap fonts)
 */
PAGX_TEST(PAGXTest, TextShaperEmoji) {
  std::string svgPath = ProjectPath::Absolute("resources/apitest/SVG/emoji.svg");

  auto doc = pagx::SVGImporter::Parse(svgPath);
  ASSERT_TRUE(doc != nullptr);

  int canvasWidth = static_cast<int>(doc->width);
  int canvasHeight = static_cast<int>(doc->height);

  auto typefaces = GetFallbackTypefaces();

  // Typeset text and embed fonts
  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(typefaces);
  auto layoutResult = textLayout.layout(doc.get());
  EXPECT_FALSE(layoutResult.shapedTextMap.empty());
  pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap,
                             layoutResult.textOrder);

  // Render typeset document
  auto originalLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(originalLayer != nullptr);

  auto originalSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList originalDL;
  originalDL.root()->addChild(originalLayer);
  originalDL.render(originalSurface.get(), false);

  // Export and reload
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  std::string pagxPath = SavePAGXFile(xml, "PAGXTest/emoji_preshaped.pagx");

  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);
  auto reloadedLayer = pagx::LayerBuilder::Build(reloadedDoc.get());
  ASSERT_TRUE(reloadedLayer != nullptr);

  // Render pre-shaped
  auto preshapedSurface = Surface::Make(context, canvasWidth, canvasHeight);
  DisplayList preshapedDL;
  preshapedDL.root()->addChild(reloadedLayer);
  preshapedDL.render(preshapedSurface.get(), false);

  EXPECT_TRUE(Baseline::Compare(originalSurface, "PAGXTest/TextShaperEmoji_Original"));
  EXPECT_TRUE(Baseline::Compare(preshapedSurface, "PAGXTest/TextShaperEmoji_PreShaped"));
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
      FAIL() << "Failed to load: " << filePath;
      continue;
    }

    auto layoutResult = textLayout.layout(doc.get());
    pagx::FontEmbedder().embed(doc.get(), layoutResult.shapedTextMap,
                               layoutResult.textOrder);

    auto layer = pagx::LayerBuilder::Build(doc.get());
    if (!layer) {
      FAIL() << "Failed to build layer: " << filePath;
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
 * TextBox horizontal layout: wordWrap, textAlign (Start/Center/End/Justify),
 * paragraphAlign (Near/Center/Far), and overflow hidden.
 */
PAGX_TEST(PAGXTest, TextBoxHorizontal) {
  auto doc = pagx::PAGXDocument::Make(500, 500);
  auto layer = doc->makeNode<pagx::Layer>();

  auto addBorder = [&](float x, float y, float w, float h) {
    auto borderGroup = doc->makeNode<pagx::Group>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->center = {x + w / 2, y + h / 2};
    rect->size = {w, h};
    auto stroke = doc->makeNode<pagx::Stroke>();
    auto strokeColor = doc->makeNode<pagx::SolidColor>();
    strokeColor->color = {0.8f, 0.8f, 0.8f, 1};
    stroke->color = strokeColor;
    stroke->width = 1;
    borderGroup->elements.push_back(rect);
    borderGroup->elements.push_back(stroke);
    layer->contents.push_back(borderGroup);
  };

  auto addTextBox = [&](const std::string& str, float fontSize, float x, float y, float w, float h,
                        pagx::TextAlign tAlign, pagx::ParagraphAlign pAlign, bool wrap,
                        pagx::Overflow overflow = pagx::Overflow::Visible) {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = str;
    text->fontSize = fontSize;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {x, y};
    textBox->size = {w, h};
    textBox->textAlign = tAlign;
    textBox->paragraphAlign = pAlign;
    textBox->wordWrap = wrap;
    textBox->overflow = overflow;
    group->elements.push_back(textBox);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    layer->contents.push_back(group);
  };

  // Row 1: wordWrap + textAlign Start/Center/End + paragraphAlign Near/Center/Far
  addBorder(10, 10, 150, 120);
  addTextBox("Hello World this is a long text.", 18, 10, 10, 150, 120,
             pagx::TextAlign::Start, pagx::ParagraphAlign::Near, true);

  addBorder(170, 10, 150, 120);
  addTextBox("Line One\nLine Two\nLine Three", 18, 170, 10, 150, 120,
             pagx::TextAlign::Center, pagx::ParagraphAlign::Middle, false);

  // CJK text with End/Far
  addBorder(330, 10, 150, 120);
  addTextBox("\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c\xef\xbc\x8c"
             "\xe8\xbf\x99\xe6\x98\xaf\xe6\xb5\x8b\xe8\xaf\x95\xe6\x96\x87\xe6\x9c\xac\xe3\x80\x82",
             20, 330, 10, 150, 120,
             pagx::TextAlign::End, pagx::ParagraphAlign::Far, true);

  // Row 2: overflow hidden, Justify, ParagraphAlign single-line Near/Center/Far
  addBorder(10, 150, 150, 80);
  addTextBox("Overflow text that is very long and should be clipped by the box boundary.", 18,
             10, 150, 150, 80,
             pagx::TextAlign::Start, pagx::ParagraphAlign::Near, true, pagx::Overflow::Hidden);

  addBorder(170, 150, 150, 80);
  addTextBox("Justify this line\nLast line start", 18, 170, 150, 150, 80,
             pagx::TextAlign::Justify, pagx::ParagraphAlign::Near, false);

  // ParagraphAlign single-line comparison: Near/Center/Far
  addBorder(10, 260, 150, 80);
  addTextBox("aaA\xc3\x82\xe1\xba\xa4", 32, 10, 260, 150, 80,
             pagx::TextAlign::Start, pagx::ParagraphAlign::Near, false);
  addBorder(170, 260, 150, 80);
  addTextBox("aaA\xc3\x82\xe1\xba\xa4", 32, 170, 260, 150, 80,
             pagx::TextAlign::Start, pagx::ParagraphAlign::Middle, false);
  addBorder(330, 260, 150, 80);
  addTextBox("aaA\xc3\x82\xe1\xba\xa4", 32, 330, 260, 150, 80,
             pagx::TextAlign::Start, pagx::ParagraphAlign::Far, false);

  doc->layers.push_back(layer);

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());

  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 500, 500);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/TextBoxHorizontal"));
}

/**
 * TextBox vertical layout: CJK+Latin mixed, wordWrap, textAlign (Start/Center/End/Justify),
 * paragraphAlign (Near/Center/Far), and overflow hidden.
 */
PAGX_TEST(PAGXTest, TextBoxVertical) {
  auto doc = pagx::PAGXDocument::Make(600, 500);
  auto layer = doc->makeNode<pagx::Layer>();

  auto addBorder = [&](float x, float y, float w, float h) {
    auto borderGroup = doc->makeNode<pagx::Group>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->center = {x + w / 2, y + h / 2};
    rect->size = {w, h};
    auto stroke = doc->makeNode<pagx::Stroke>();
    auto strokeColor = doc->makeNode<pagx::SolidColor>();
    strokeColor->color = {0.8f, 0.8f, 0.8f, 1};
    stroke->color = strokeColor;
    stroke->width = 1;
    borderGroup->elements.push_back(rect);
    borderGroup->elements.push_back(stroke);
    layer->contents.push_back(borderGroup);
  };

  auto addVerticalTextBox = [&](const std::string& str, float fontSize, float x, float y,
                                float w, float h, pagx::TextAlign tAlign,
                                pagx::ParagraphAlign pAlign, bool wrap,
                                pagx::Overflow overflow = pagx::Overflow::Visible) {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = str;
    text->fontSize = fontSize;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {x, y};
    textBox->size = {w, h};
    textBox->writingMode = pagx::WritingMode::Vertical;
    textBox->textAlign = tAlign;
    textBox->paragraphAlign = pAlign;
    textBox->wordWrap = wrap;
    textBox->overflow = overflow;
    group->elements.push_back(textBox);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    layer->contents.push_back(group);
  };

  // Row 1: CJK+Latin single column, two columns with \n, wordWrap auto-wrap
  // CJK+Latin mixed single column
  addBorder(10, 10, 80, 230);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd" "Hello" "\xe4\xb8\x96\xe7\x95\x8c",
                     24, 10, 10, 80, 230,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, false);

  // Two columns: explicit \n
  addBorder(100, 10, 100, 230);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd" "Hello\n" "\xe4\xb8\x96\xe7\x95\x8c" "Test",
                     24, 100, 10, 100, 230,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, false);

  // CJK wordWrap auto-wrap
  addBorder(210, 10, 100, 168);
  addVerticalTextBox("\xe6\x98\xa5\xe7\x9c\xa0\xe4\xb8\x8d\xe8\xa7\x89\xe6\x99\x93"
                     "\xe5\xa4\x84\xe5\xa4\x84\xe9\x97\xbb\xe5\x95\xbc\xe9\xb8\x9f"
                     "\xe5\xa4\x9c\xe6\x9d\xa5\xe9\xa3\x8e\xe9\x9b\xa8\xe5\xa3\xb0"
                     "\xe8\x8a\xb1\xe8\x90\xbd\xe7\x9f\xa5\xe5\xa4\x9a\xe5\xb0\x91",
                     24, 210, 10, 100, 168,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, true);

  // Row 2: ParagraphAlign Near/Center/Far + TextAlign Start/Center/End/Justify + overflow hidden
  // ParagraphAlign Near/Center/Far (single column positioning)
  addBorder(320, 10, 80, 150);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd", 24, 320, 10, 80, 150,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, false);
  addBorder(410, 10, 80, 150);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd", 24, 410, 10, 80, 150,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Middle, false);
  addBorder(500, 10, 80, 150);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd", 24, 500, 10, 80, 150,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Far, false);

  // TextAlign Start/Center/End (two columns with different lengths)
  addBorder(10, 260, 100, 230);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd\n\xe4\xb8\x96\xe7\x95\x8c\xe5\xa4\xa7\xe5\x9c\xb0",
                     24, 10, 260, 100, 230,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, false);
  addBorder(120, 260, 100, 230);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd\n\xe4\xb8\x96\xe7\x95\x8c\xe5\xa4\xa7\xe5\x9c\xb0",
                     24, 120, 260, 100, 230,
                     pagx::TextAlign::Center, pagx::ParagraphAlign::Near, false);
  addBorder(230, 260, 100, 230);
  addVerticalTextBox("\xe4\xbd\xa0\xe5\xa5\xbd\n\xe4\xb8\x96\xe7\x95\x8c\xe5\xa4\xa7\xe5\x9c\xb0",
                     24, 230, 260, 100, 230,
                     pagx::TextAlign::End, pagx::ParagraphAlign::Near, false);

  // Justify (two columns, last column Start-aligned)
  addBorder(340, 260, 80, 230);
  addVerticalTextBox("\xe6\x98\xa5\xe7\x9c\xa0\xe4\xb8\x8d\xe8\xa7\x89\xe6\x99\x93\n"
                     "\xe8\x8a\xb1\xe8\x90\xbd\xe7\x9f\xa5\xe5\xa4\x9a\xe5\xb0\x91",
                     24, 340, 260, 80, 230,
                     pagx::TextAlign::Justify, pagx::ParagraphAlign::Near, false);

  // Overflow hidden
  addBorder(430, 260, 80, 200);
  addVerticalTextBox("\xe6\x98\xa5\xe7\x9c\xa0\xe4\xb8\x8d\xe8\xa7\x89\xe6\x99\x93"
                     "\xe5\xa4\x84\xe5\xa4\x84\xe9\x97\xbb\xe5\x95\xbc\xe9\xb8\x9f"
                     "\xe5\xa4\x9c\xe6\x9d\xa5\xe9\xa3\x8e\xe9\x9b\xa8\xe5\xa3\xb0"
                     "\xe8\x8a\xb1\xe8\x90\xbd\xe7\x9f\xa5\xe5\xa4\x9a\xe5\xb0\x91",
                     24, 430, 260, 80, 200,
                     pagx::TextAlign::Start, pagx::ParagraphAlign::Near, true,
                     pagx::Overflow::Hidden);

  doc->layers.push_back(layer);

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());

  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 600, 500);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/TextBoxVertical"));
}

/**
 * TextBox line height and rich text: auto/fixed lineHeight, mixed font sizes,
 * fixed lineHeight + Near/Center/Far, and multi-line mixed sizes + Far.
 */
PAGX_TEST(PAGXTest, TextBoxLineHeight) {
  auto doc = pagx::PAGXDocument::Make(500, 400);
  auto layer = doc->makeNode<pagx::Layer>();

  auto addBorder = [&](float x, float y, float w, float h) {
    auto borderGroup = doc->makeNode<pagx::Group>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->center = {x + w / 2, y + h / 2};
    rect->size = {w, h};
    auto stroke = doc->makeNode<pagx::Stroke>();
    auto strokeColor = doc->makeNode<pagx::SolidColor>();
    strokeColor->color = {0.8f, 0.8f, 0.8f, 1};
    stroke->color = strokeColor;
    stroke->width = 1;
    borderGroup->elements.push_back(rect);
    borderGroup->elements.push_back(stroke);
    layer->contents.push_back(borderGroup);
  };

  auto addRichText = [&](pagx::Layer* targetLayer, const std::string& str, float fontSize) {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = str;
    text->fontSize = fontSize;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    targetLayer->contents.push_back(group);
  };

  auto addSimpleTextBox = [&](pagx::Layer* targetLayer, const std::string& str, float fontSize,
                              float x, float y, float w, float h, float lineH,
                              pagx::ParagraphAlign pAlign) {
    auto textGroup = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = str;
    text->fontSize = fontSize;
    text->fontFamily = "NotoSansSC";
    textGroup->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    textGroup->elements.push_back(fill);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {x, y};
    textBox->size = {w, h};
    textBox->paragraphAlign = pAlign;
    textBox->lineHeight = lineH;
    textGroup->elements.push_back(textBox);
    targetLayer->contents.push_back(textGroup);
  };

  // Box A: Auto lineHeight, mixed font sizes (40+20+60), Near
  {
    addBorder(10, 10, 200, 180);
    auto boxLayer = doc->makeNode<pagx::Layer>();
    addRichText(boxLayer, "Hg\n", 40);
    addRichText(boxLayer, "Xy", 20);
    addRichText(boxLayer, "\nAB", 60);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {10, 10};
    textBox->size = {200, 180};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    boxLayer->contents.push_back(textBox);
    layer->children.push_back(boxLayer);
  }

  // Box B: Fixed lineHeight=40, same mixed text, Near
  {
    addBorder(220, 10, 200, 180);
    auto boxLayer = doc->makeNode<pagx::Layer>();
    addRichText(boxLayer, "Hg\n", 40);
    addRichText(boxLayer, "Xy", 20);
    addRichText(boxLayer, "\nAB", 60);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {220, 10};
    textBox->size = {200, 180};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    textBox->lineHeight = 40;
    boxLayer->contents.push_back(textBox);
    layer->children.push_back(boxLayer);
  }

  // Box C: Fixed lineHeight=50 + Far (bottom alignment)
  {
    addBorder(10, 210, 150, 160);
    addSimpleTextBox(layer, "Line1\nLine2", 24, 10, 210, 150, 160, 50,
                     pagx::ParagraphAlign::Far);
  }

  // Box D: Fixed lineHeight=50 + Center
  {
    addBorder(170, 210, 150, 160);
    addSimpleTextBox(layer, "Line1\nLine2", 24, 170, 210, 150, 160, 50,
                     pagx::ParagraphAlign::Middle);
  }

  // Box E: 3-line mixed sizes + Far (multi-step bottom-up recursion)
  {
    addBorder(330, 210, 150, 180);
    auto boxLayer = doc->makeNode<pagx::Layer>();
    addRichText(boxLayer, "Large\n", 36);
    addRichText(boxLayer, "Medium\n", 24);
    addRichText(boxLayer, "Tiny", 16);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {330, 210};
    textBox->size = {150, 180};
    textBox->paragraphAlign = pagx::ParagraphAlign::Far;
    boxLayer->contents.push_back(textBox);
    layer->children.push_back(boxLayer);
  }

  doc->layers.push_back(layer);

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());

  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 500, 400);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/TextBoxLineHeight"));
}

/**
 * TextBox edge cases: empty lines, anchor mode (size=0), fixed lineHeight + empty first line,
 * and End alignment with rich text.
 */
PAGX_TEST(PAGXTest, TextBoxEdgeCases) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<pagx::Layer>();

  auto addBorder = [&](float x, float y, float w, float h) {
    auto borderGroup = doc->makeNode<pagx::Group>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->center = {x + w / 2, y + h / 2};
    rect->size = {w, h};
    auto stroke = doc->makeNode<pagx::Stroke>();
    auto strokeColor = doc->makeNode<pagx::SolidColor>();
    strokeColor->color = {0.8f, 0.8f, 0.8f, 1};
    stroke->color = strokeColor;
    stroke->width = 1;
    borderGroup->elements.push_back(rect);
    borderGroup->elements.push_back(stroke);
    layer->contents.push_back(borderGroup);
  };

  auto addRichText = [&](pagx::Layer* targetLayer, const std::string& str, float fontSize) {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = str;
    text->fontSize = fontSize;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    targetLayer->contents.push_back(group);
  };

  // Box A: Empty lines (\n at start + consecutive \n\n)
  {
    addBorder(10, 10, 120, 130);
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = "\nLine2\n\nLine4";
    text->fontSize = 20;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {10, 10};
    textBox->size = {120, 130};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    group->elements.push_back(textBox);
    layer->contents.push_back(group);
  }

  // Box B: Anchor mode (size=0) Start + marker
  {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = "Anchor";
    text->fontSize = 16;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {180, 30};
    textBox->size = {0, 0};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    textBox->textAlign = pagx::TextAlign::Start;
    group->elements.push_back(textBox);
    layer->contents.push_back(group);

    auto marker = doc->makeNode<pagx::Group>();
    auto circle = doc->makeNode<pagx::Ellipse>();
    circle->center = {180, 30};
    circle->size = {6, 6};
    marker->elements.push_back(circle);
    auto markerFill = doc->makeNode<pagx::Fill>();
    auto markerColor = doc->makeNode<pagx::SolidColor>();
    markerColor->color = {1, 0, 0, 1};
    markerFill->color = markerColor;
    marker->elements.push_back(markerFill);
    layer->contents.push_back(marker);
  }

  // Box C: Anchor mode (size=0) Center/Center + marker
  {
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = "Center";
    text->fontSize = 30;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {300, 30};
    textBox->size = {0, 0};
    textBox->paragraphAlign = pagx::ParagraphAlign::Middle;
    textBox->textAlign = pagx::TextAlign::Center;
    group->elements.push_back(textBox);
    layer->contents.push_back(group);

    auto marker = doc->makeNode<pagx::Group>();
    auto circle = doc->makeNode<pagx::Ellipse>();
    circle->center = {300, 30};
    circle->size = {6, 6};
    marker->elements.push_back(circle);
    auto markerFill = doc->makeNode<pagx::Fill>();
    auto markerColor = doc->makeNode<pagx::SolidColor>();
    markerColor->color = {1, 0, 0, 1};
    markerFill->color = markerColor;
    marker->elements.push_back(markerFill);
    layer->contents.push_back(marker);
  }

  // Box D: Fixed lineHeight + \n at start
  {
    addBorder(10, 160, 120, 120);
    auto group = doc->makeNode<pagx::Group>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = "\nSecond";
    text->fontSize = 24;
    text->fontFamily = "NotoSansSC";
    group->elements.push_back(text);
    auto fill = doc->makeNode<pagx::Fill>();
    auto solid = doc->makeNode<pagx::SolidColor>();
    solid->color = {0, 0, 0, 1};
    fill->color = solid;
    group->elements.push_back(fill);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {10, 160};
    textBox->size = {120, 120};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    textBox->lineHeight = 50;
    group->elements.push_back(textBox);
    layer->contents.push_back(group);
  }

  // Box E: End alignment + rich text
  {
    addBorder(150, 160, 230, 120);
    auto boxLayer = doc->makeNode<pagx::Layer>();
    addRichText(boxLayer, "Right", 24);
    addRichText(boxLayer, "Align", 36);
    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->position = {150, 160};
    textBox->size = {230, 120};
    textBox->paragraphAlign = pagx::ParagraphAlign::Near;
    textBox->textAlign = pagx::TextAlign::End;
    boxLayer->contents.push_back(textBox);
    layer->children.push_back(boxLayer);
  }

  doc->layers.push_back(layer);

  pagx::TextLayout textLayout;
  textLayout.setFallbackTypefaces(GetFallbackTypefaces());
  auto layoutResult = textLayout.layout(doc.get());
  ASSERT_FALSE(layoutResult.shapedTextMap.empty());

  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get(), &textLayout);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, 400, 300);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/TextBoxEdgeCases"));
}

}  // namespace pag
