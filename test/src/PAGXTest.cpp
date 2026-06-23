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
#include <functional>
#include <unordered_map>
#include "cli/CommandResolve.h"
#include "cli/CommandVerify.h"
#include "pagx/FontConfig.h"
#include "pagx/HTMLExporter.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGLayer.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXChannelTable.h"
#include "pagx/PAGXDefaults.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXNodeChannel.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/SVGExporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/TextLayout.h"
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/nodes/NoiseStyle.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Padding.h"
#include "pagx/utils/StringParser.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#ifdef PAG_USE_SWIFTSHADER
#include <GLES3/gl3.h>
#else
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#endif
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlendFilter.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/NoiseFilter.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/NoiseStyle.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Gradient.h"
#include "tgfx/layers/vectors/ImagePattern.h"
#include "tgfx/layers/vectors/Rectangle.h"
#include "tgfx/layers/vectors/SolidColor.h"
#include "tgfx/layers/vectors/Text.h"
#include "utils/Baseline.h"
#include "utils/PAGXTestUtils.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

static std::shared_ptr<pagx::PAGXDocument> LoadAndResolve(const std::string& filePath) {
  auto doc = pagx::PAGXImporter::FromFile(filePath);
  if (doc && doc->hasUnresolvedImports()) {
    CallRun(pagx::cli::RunResolve, {"resolve", filePath});
    doc = pagx::PAGXImporter::FromFile(filePath);
  }
  return doc;
}

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

static void VerifyLayerConsistency(const std::shared_ptr<pagx::PAGScene>& scene,
                                   pagx::RuntimeBinding* binding,
                                   const std::shared_ptr<pagx::PAGLayer>& layer) {
  if (layer == nullptr) {
    return;
  }
  auto* node = layer->getNode();
  if (node != nullptr) {
    auto bound = binding->get<tgfx::Layer>(node);
    EXPECT_EQ(bound.get(), layer->runtimeLayer.get())
        << "binding mismatch: promotion sync may not have run for this layer";
  }
  if (layer->runtimeLayer != nullptr) {
    auto it = scene->layerRegistry.find(layer->runtimeLayer.get());
    EXPECT_NE(it, scene->layerRegistry.end())
        << "layerRegistry missing entry: hit-test will miss this layer";
    if (it != scene->layerRegistry.end()) {
      EXPECT_EQ(it->second, layer.get())
          << "layerRegistry maps to wrong PAGLayer: hit-test returns incorrect layer";
    }
  }
  for (auto& child : layer->getChildren()) {
    VerifyLayerConsistency(scene, binding, child);
  }
}

static void AssertSceneConsistent(const std::shared_ptr<pagx::PAGScene>& scene) {
  if (scene == nullptr || scene->rootComposition() == nullptr) {
    return;
  }
  auto binding = scene->mutableBinding();
  if (binding == nullptr) {
    return;
  }
  VerifyLayerConsistency(scene, binding, scene->rootComposition());
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

    // Step 1b: Simplify the imported structure before layout so verify doesn't flag the raw
    // Layer/Group tree produced by the SVG importer.
    pagx::PAGXOptimizer::Optimize(doc.get());

    // Step 2: Layout and embed fonts
    doc->applyLayout(&svgFontConfig);
    pagx::FontEmbedder().embed(doc.get());

    // Step 3: Export to XML and save as PAGX file
    std::string xml = pagx::PAGXExporter::ToXML(*doc);
    auto key = "svg_" + baseName;
    std::string pagxPath = SavePAGXFile(xml, "PAGXTest/" + key + ".pagx");

    // Step 3b: The PAGXOptimizer is responsible for eliminating every structural warning
    // verify can flag.
    VerifyFile(pagxPath, key);

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
    reloadedDoc->applyLayout();
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
  docFromFile->applyLayout();
  auto layerFromFile = pagx::LayerBuilder::Build(docFromFile.get());
  ASSERT_TRUE(layerFromFile != nullptr);

  // Load via FromXML (read raw bytes from the same file)
  auto fileData = tgfx::Data::MakeFromFile(pagxPath);
  ASSERT_TRUE(fileData != nullptr);
  auto docFromData =
      pagx::PAGXImporter::FromXML(fileData->bytes(), static_cast<size_t>(fileData->size()));
  ASSERT_TRUE(docFromData != nullptr);
  docFromData->applyLayout();
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
 * Test case: Font fileOriginal is preserved when importing XML content directly.
 */
PAGX_TEST(PAGXTest, FontFileOriginalFromXML) {
  auto xml = R"(<pagx width="100" height="100">
  <Resources>
    <Font id="noto" file="fonts/NotoSansSC-Regular.otf"/>
  </Resources>
</pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->errors.empty());

  auto font = doc->findNode<pagx::Font>("noto");
  ASSERT_TRUE(font != nullptr);
  EXPECT_EQ(font->file, "fonts/NotoSansSC-Regular.otf");
  EXPECT_EQ(font->fileOriginal, "fonts/NotoSansSC-Regular.otf");
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
  layer->arrangement = pagx::Arrangement::Center;
  layer->children.push_back(MakeTextLayer(doc.get(), "Hello PAGX", 36, {0.2f, 0.2f, 0.8f, 1.0f}));
  layer->children.push_back(
      MakeTextLayer(doc.get(), "\xe4\xbd\xa0\xe5\xa5\xbd World", 28, {0.1f, 0.6f, 0.2f, 1.0f}));
  layer->children.push_back(
      MakeTextLayer(doc.get(), "Embedded Font", 18, {0.5f, 0.5f, 0.5f, 1.0f}));
  doc->layers.push_back(layer);

  pagx::FontConfig embedFontConfig;
  embedFontConfig.addFallbackTypefaces(GetFallbackTypefaces());
  doc->applyLayout(&embedFontConfig);
  pagx::FontEmbedder().embed(doc.get());

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  auto pagxPath = SavePAGXFile(xml, "PAGXTest/PrecomposedTextRender.pagx");

  auto reloadedDoc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_TRUE(reloadedDoc != nullptr);
  reloadedDoc->applyLayout();
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

static std::vector<std::pair<std::string, std::string>> ExtractMarkdownPatterns(
    const std::string& markdownPath) {
  std::vector<std::pair<std::string, std::string>> patterns;
  std::ifstream file(markdownPath);
  if (!file.is_open()) {
    return patterns;
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
        // Skip XML declaration if present
        if (trimmed.substr(0, 5) == "<?xml") {
          pos = trimmed.find('\n');
          if (pos != std::string::npos) {
            trimmed = trimmed.substr(pos + 1);
            pos = trimmed.find_first_not_of(" \t\n\r");
            if (pos != std::string::npos) {
              trimmed = trimmed.substr(pos);
            }
          }
        }
        if (trimmed.substr(0, 5) == "<pagx" && !currentTitle.empty()) {
          auto baseKey = TitleToKey(currentTitle);
          auto count = ++keyCounts[baseKey];
          auto key = count > 1 ? baseKey + "_" + std::to_string(count) : baseKey;
          patterns.emplace_back(key, codeContent);
        }
      } else {
        if (!codeContent.empty()) {
          codeContent += "\n";
        }
        codeContent += line;
      }
    }
  }
  return patterns;
}

static void TestMarkdownPatterns(tgfx::Context* context, const std::string& markdownPath,
                                 const std::string& prefix = "", float scale = 1.0f) {
  auto patterns = ExtractMarkdownPatterns(markdownPath);
  ASSERT_FALSE(patterns.empty()) << "No patterns found in: " << markdownPath;

  // Ensure output directory exists, then copy test image for patterns referencing "avatar.jpg".
  auto outDir = std::filesystem::path(ProjectPath::Absolute("test/out/PAGXTest"));
  std::filesystem::create_directories(outDir);
  std::filesystem::copy_file(ProjectPath::Absolute("resources/apitest/imageReplacement.jpg"),
                             outDir / "avatar.jpg",
                             std::filesystem::copy_options::overwrite_existing);

  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(GetFallbackTypefaces());

  for (const auto& [name, xmlContent] : patterns) {
    auto key = prefix + name;

    auto pagxPath = SavePAGXFile(xmlContent, "PAGXTest/" + key + ".pagx");

    // Load via FromFile so relative image paths resolve against the pagx directory.
    // LoadAndResolve also expands any <Import> nodes (e.g., inline SVG) before rendering.
    auto doc = LoadAndResolve(pagxPath);
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

    doc->applyLayout(&fontConfig);
    auto layer = pagx::LayerBuilder::Build(doc.get());
    if (!layer) {
      ADD_FAILURE() << "Failed to build layer for: " << key;
      continue;
    }

    int canvasWidth = static_cast<int>(std::ceil(doc->width * scale));
    int canvasHeight = static_cast<int>(std::ceil(doc->height * scale));
    auto surface = Surface::Make(context, canvasWidth, canvasHeight);
    if (!surface) {
      ADD_FAILURE() << "Failed to create surface for: " << key;
      continue;
    }
    DisplayList displayList;
    if (scale != 1.0f) {
      auto container = tgfx::Layer::Make();
      container->setMatrix(tgfx::Matrix::MakeScale(scale, scale));
      container->addChild(layer);
      displayList.root()->addChild(container);
    } else {
      displayList.root()->addChild(layer);
    }
    displayList.render(surface.get(), false);

    EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/" + key)) << key;
    VerifyFile(pagxPath, key);
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

    auto doc = LoadAndResolve(filePath);
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

    doc->applyLayout(&fontConfig);
    auto layer = pagx::LayerBuilder::Build(doc.get());
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
    VerifyFile(filePath, key);
  }
}

/**
 * Test all PAGX sample files in spec/samples directory.
 * Renders each sample and compares with baseline screenshots.
 */
PAGX_TEST(PAGXTest, SpecSamples) {
  TestPAGXDirectory(context, ProjectPath::Absolute("spec/samples"), "spec_");
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
  EXPECT_TRUE(pagx::IsValidCustomDataKey("role"));
  EXPECT_TRUE(pagx::IsValidCustomDataKey("figma-node"));
  EXPECT_TRUE(pagx::IsValidCustomDataKey("v2"));
  EXPECT_TRUE(pagx::IsValidCustomDataKey("a"));
  EXPECT_TRUE(pagx::IsValidCustomDataKey("abc-123-def"));

  // Invalid keys.
  EXPECT_FALSE(pagx::IsValidCustomDataKey(""));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("-leading"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("trailing-"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("-"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("UPPER"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("has space"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("under_score"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("dot.name"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("a<b"));
  EXPECT_FALSE(pagx::IsValidCustomDataKey("a\"b"));

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

  EXPECT_EQ(card1->layoutWidth, expectedChildWidth);
  EXPECT_EQ(card2->layoutWidth, expectedChildWidth);
  EXPECT_EQ(card3->layoutWidth, expectedChildWidth);

  EXPECT_EQ(card1->renderPosition().x, 20.0f);
  EXPECT_EQ(card2->renderPosition().x, 20 + 284 + 14);
  EXPECT_EQ(card3->renderPosition().x, 20 + 284 * 2 + 14 * 2);
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

  EXPECT_EQ(child1->renderPosition().y, 0.0f);
  EXPECT_EQ(child2->renderPosition().y, 110.0f);
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

  EXPECT_EQ(child->renderPosition().x, 150.0f);
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

  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 350.0f);
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

  EXPECT_EQ(child->renderPosition().y, 125.0f);
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

  EXPECT_EQ(child1->renderPosition().x, 300.0f);
  EXPECT_EQ(child2->renderPosition().x, 350.0f);
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

  EXPECT_EQ(child->renderPosition().y, 250.0f);
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

  EXPECT_EQ(child->renderPosition().x, 50.0f);
  EXPECT_EQ(child->renderPosition().y, 40.0f);
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

  EXPECT_EQ(child1->layoutWidth, 190.0f);
  EXPECT_EQ(child2->layoutWidth, 190.0f);
  EXPECT_EQ(child1->renderPosition().x, 10.0f);
  EXPECT_EQ(child2->renderPosition().x, 200.0f);
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

  EXPECT_EQ(child1->layoutWidth, 100.0f);
  EXPECT_EQ(child2->layoutWidth, 100.0f);
  EXPECT_EQ(child3->layoutWidth, 100.0f);
  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 200.0f);
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

  EXPECT_EQ(child1->layoutWidth, 100.0f);
  EXPECT_EQ(child2->layoutWidth, 150.0f);
  EXPECT_EQ(child3->layoutWidth, 150.0f);
  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 250.0f);
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

  EXPECT_EQ(child1->layoutWidth, 100.0f);
  EXPECT_EQ(child2->layoutWidth, 200.0f);
  EXPECT_EQ(child3->layoutWidth, 300.0f);
  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 300.0f);
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
  EXPECT_EQ(child->layoutWidth, 80.0f);
  EXPECT_EQ(child->renderPosition().x, 0.0f);
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
  EXPECT_EQ(fixedChild->layoutWidth, 100.0f);
  EXPECT_EQ(measuredChild->layoutWidth, 80.0f);
  EXPECT_EQ(flexChild->layoutWidth, 420.0f);
  EXPECT_EQ(fixedChild->renderPosition().x, 0.0f);
  EXPECT_EQ(measuredChild->renderPosition().x, 100.0f);
  EXPECT_EQ(flexChild->renderPosition().x, 180.0f);
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
  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 200.0f);
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

  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 200.0f);
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

  EXPECT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_EQ(child2->renderPosition().x, 100.0f);
  EXPECT_EQ(child3->renderPosition().x, 200.0f);
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

  EXPECT_EQ(right->layoutWidth, 300.0f);
  EXPECT_EQ(right->renderPosition().x, 200.0f);

  EXPECT_EQ(inner1->renderPosition().y, 0.0f);
  EXPECT_EQ(inner2->renderPosition().y, 90.0f);
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

  EXPECT_EQ(parent->layoutWidth, 220.0f);
  EXPECT_EQ(parent->layoutHeight, 80.0f);
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

  EXPECT_EQ(child1->renderPosition().x, std::round(child1->renderPosition().x));
  EXPECT_EQ(child1->layoutWidth, std::round(child1->layoutWidth));
  EXPECT_EQ(child2->renderPosition().x, std::round(child2->renderPosition().x));
  EXPECT_EQ(child2->layoutWidth, std::round(child2->layoutWidth));
  EXPECT_EQ(child3->renderPosition().x, std::round(child3->renderPosition().x));
  EXPECT_EQ(child3->layoutWidth, std::round(child3->layoutWidth));

  EXPECT_EQ(child1->layoutWidth + child2->layoutWidth + child3->layoutWidth, 100.0f);
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
  float w1 = child1->layoutWidth;
  float x1 = child1->renderPosition().x;
  float w2 = child2->layoutWidth;
  float x2 = child2->renderPosition().x;

  doc->applyLayout();
  EXPECT_EQ(child1->layoutWidth, w1);
  EXPECT_EQ(child1->renderPosition().x, x1);
  EXPECT_EQ(child2->layoutWidth, w2);
  EXPECT_EQ(child2->renderPosition().x, x2);
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
  rect->left = 20;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: left=20, width=100, so x=20, center would be at 70
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 20.0f);
  EXPECT_FLOAT_EQ(bounds.width, 100.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintRight) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->right = 30;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: right=30, width=100, so x=400-30-100=270, center would be at 320
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 270.0f);
  EXPECT_FLOAT_EQ(bounds.width, 100.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintTop) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->top = 25;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: top=25, height=50, so y=25, center would be at 50
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.y, 25.0f);
  EXPECT_FLOAT_EQ(bounds.height, 50.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintBottom) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->bottom = 40;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: bottom=40, height=50, so y=200-40-50=110, center would be at 135
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.y, 110.0f);
  EXPECT_FLOAT_EQ(bounds.height, 50.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintCenterX) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->centerX = 0;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: centerX=0 means centered, so x=(400-100)/2=150, center at 200
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 150.0f);
  EXPECT_FLOAT_EQ(bounds.width, 100.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintCenterY) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->centerY = 10;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: centerY=10 means y=(200-50)/2+10=85, center at 110
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.y, 85.0f);
  EXPECT_FLOAT_EQ(bounds.height, 50.0f);
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
  ellipse->left = 20;
  ellipse->right = 20;

  layer->contents.push_back(ellipse);

  doc->applyLayout();

  // Layout bounds: left=20, right=20, so width=400-20-20=360, x=20, center at 200
  auto bounds = ellipse->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.width, 360.0f);
  EXPECT_FLOAT_EQ(bounds.x, 20.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintStretchEllipseVertical) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {100, 50};
  ellipse->top = 15;
  ellipse->bottom = 15;

  layer->contents.push_back(ellipse);

  doc->applyLayout();

  // Layout bounds: top=15, bottom=15, so height=200-15-15=170, y=15, center at 100
  auto bounds = ellipse->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.height, 170.0f);
  EXPECT_FLOAT_EQ(bounds.y, 15.0f);
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
  rect->left = 10;
  rect->right = 10;

  layer->contents.push_back(rect);

  doc->applyLayout();

  // Layout bounds: left=10, right=10, so width=400-10-10=380, x=10, center at 200
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.width, 380.0f);
  EXPECT_FLOAT_EQ(bounds.x, 10.0f);
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
  textBox->left = 30;
  textBox->right = 30;
  textBox->top = 20;
  textBox->bottom = 20;

  layer->contents.push_back(textBox);

  doc->applyLayout();

  // Layout bounds: left=30, right=30, so width=400-30-30=340, x=30
  //                top=20, bottom=20, so height=200-20-20=160, y=20
  auto bounds = textBox->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.width, 340.0f);
  EXPECT_FLOAT_EQ(bounds.x, 30.0f);
  EXPECT_FLOAT_EQ(bounds.height, 160.0f);
  EXPECT_FLOAT_EQ(bounds.y, 20.0f);
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
  star->left = 50;
  star->right = 50;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions, not simplified square):
  // For Star with pointCount=5, rotation=0, width ≈ 2.0 * outerRadius, height ≈ 1.809 * outerRadius
  // bounds.width ≈ 57.06, area width = 400 - 50 - 50 = 300
  // scale = 300 / 57.06 ≈ 5.2574
  EXPECT_FLOAT_EQ(star->renderOuterRadius(), 157.71933f);
  EXPECT_FLOAT_EQ(star->renderInnerRadius(), 78.859665f);
  // layoutBounds center should be at area center (50 + 300/2 = 200)
  auto bounds = star->layoutBounds();
  EXPECT_NEAR(bounds.x + bounds.width * 0.5f, 200.0f, 1.0f);
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
  star->top = 20;
  star->bottom = 20;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions):
  // bounds.height ≈ 1.809 * outerRadius ≈ 45.225
  // area height = 200 - 20 - 20 = 160
  // scale = 160 / 45.225 ≈ 3.5378
  EXPECT_FLOAT_EQ(star->renderOuterRadius(), 88.445824f);
  EXPECT_FLOAT_EQ(star->renderInnerRadius(), 35.37833f);
  // layoutBounds center should be at area center (20 + 160/2 = 100)
  auto bounds = star->layoutBounds();
  EXPECT_NEAR(bounds.y + bounds.height * 0.5f, 100.0f, 1.0f);
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
  star->left = 20;
  star->right = 20;
  star->top = 10;
  star->bottom = 10;

  layer->contents.push_back(star);

  doc->applyLayout();

  // Polystar bounds (computed from precise vertex positions):
  // bounds.width ≈ 38.042, bounds.height ≈ 36.180
  // areaWidth = 360, areaHeight = 180
  // scaleX = 360 / 38.042 ≈ 9.463, scaleY = 180 / 36.180 ≈ 4.975
  // scale = min(scaleX, scaleY) ≈ 4.975
  EXPECT_FLOAT_EQ(star->renderOuterRadius(), 99.501556f);
  EXPECT_FLOAT_EQ(star->renderInnerRadius(), 49.750778f);
  // layoutBounds: centered in area, width/height from scaled content
  auto bounds = star->layoutBounds();
  // Scaled content width ≈ 185, centered in 360: x = 20 + (360-185)/2 ≈ 107.5
  // Scaled content height = 180, centered in 180: y = 10 + 0 = 10
  EXPECT_NEAR(bounds.x + bounds.width * 0.5f, 200.0f, 0.5f);   // center X
  EXPECT_NEAR(bounds.y + bounds.height * 0.5f, 100.0f, 0.5f);  // center Y
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
  auto origBounds = pagx::TextLayout::Layout({text}, params, &layoutContext);
  float origWidth = origBounds.bounds.width;
  float origHeight = origBounds.bounds.height;

  layer->contents.push_back(text);

  doc->applyLayout(&fontConfig);

  // Proportional scaling: areaWidth = 360, areaHeight = 180
  // measured text bounds are not ceiled before scaling.
  float scaleX = 360 / origWidth;
  float scaleY = 180 / origHeight;
  float scale = std::min(scaleX, scaleY);
  float expectedFontSize = 24 * scale;
  EXPECT_FLOAT_EQ(text->renderFontSize(), expectedFontSize);
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
  auto origBounds = pagx::TextLayout::Layout({text}, params, &layoutContext);
  float origWidth = origBounds.bounds.width;

  layer->contents.push_back(text);

  doc->applyLayout(&fontConfig);

  // areaWidth = 380, scale = 380 / origWidth (measured text bounds are not ceiled)
  float expectedFontSize = 50 * 380 / origWidth;
  EXPECT_FLOAT_EQ(text->renderFontSize(), expectedFontSize);
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
  // Polystar bounds: bounds.width ≈ 95.106
  // areaWidth = 400 - 150 - 150 = 100
  // scale = 100 / 95.106 ≈ 1.0515
  star->left = 150;
  star->right = 150;

  layer->contents.push_back(star);

  doc->applyLayout();

  // scale ≈ 1.0515
  EXPECT_FLOAT_EQ(star->renderOuterRadius(), 52.573109f);
  EXPECT_FLOAT_EQ(star->renderInnerRadius(), 26.286554f);
  // layoutBounds center at (150 + 100/2) = 200
  auto bounds = star->layoutBounds();
  EXPECT_NEAR(bounds.x + bounds.width * 0.5f, 200.0f, 0.5f);
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
  path->left = 20;
  path->right = 20;

  layer->contents.push_back(path);

  doc->applyLayout();

  // Path bounds = (0, 0, 60, 40). Single-axis constraint: areaWidth = 360
  // Proportional scaling: scale = 360 / 60 = 6.0
  // Scaled points: (0,0)->(360,0)->(180,240), new bounds = (0, 0, 360, 240)
  auto bounds = path->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 20.0f);
  EXPECT_FLOAT_EQ(bounds.width, 360.0f);
  // Verify renderScale is computed from layoutBounds
  auto scale = path->renderScale();
  EXPECT_FLOAT_EQ(scale, 6.0f);  // 360 / 60 = 6.0
  // Original points remain unchanged
  auto& points = path->data->points();
  EXPECT_FLOAT_EQ(points[1].x, 60.0f);
  EXPECT_FLOAT_EQ(points[2].x, 30.0f);
  EXPECT_FLOAT_EQ(points[2].y, 40.0f);
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
  // Verify renderScale is computed from layoutBounds
  auto scale = path->renderScale();
  EXPECT_FLOAT_EQ(scale, 3.0f);  // min(300/100, 200/50) = 3.0
  // Original points remain unchanged
  auto& points = path->data->points();
  EXPECT_FLOAT_EQ(points[1].x, 100.0f);
  EXPECT_FLOAT_EQ(points[2].y, 50.0f);
  // Centered vertically: 50 + (200 - 150) * 0.5 = 75
  auto bounds = path->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 50.0f);
  EXPECT_FLOAT_EQ(bounds.y, 75.0f);
  EXPECT_FLOAT_EQ(bounds.width, 300.0f);
  EXPECT_FLOAT_EQ(bounds.height, 150.0f);
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
  group->left = 20;
  group->right = 20;
  group->top = 30;
  group->bottom = 30;

  layer->contents.push_back(group);

  doc->applyLayout();

  auto bounds = group->layoutBounds();
  EXPECT_EQ(bounds.width, 360.0f);
  EXPECT_EQ(bounds.height, 240.0f);
  EXPECT_EQ(bounds.x, 20.0f);
  EXPECT_EQ(bounds.y, 30.0f);
}

PAGX_TEST(PAGXTest, LayoutConstraintGroupRecursive) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto group = doc->makeNode<pagx::Group>();
  group->left = 20;
  group->right = 20;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {100, 50};
  rect->left = 10;
  rect->right = 10;

  group->elements.push_back(rect);
  layer->contents.push_back(group);

  doc->applyLayout();

  auto groupBounds = group->layoutBounds();
  EXPECT_EQ(groupBounds.width, 360.0f);
  EXPECT_EQ(groupBounds.x, 20.0f);
  auto rectBounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(rectBounds.width, 340.0f);
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
  rect1->left = 10;

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {80, 40};
  rect2->right = 10;

  auto rect3 = doc->makeNode<pagx::Rectangle>();
  rect3->size = {80, 40};
  rect3->centerX = 0;
  rect3->centerY = 0;

  layer->contents = {rect1, rect2, rect3};

  doc->applyLayout();

  // rect1: left=10, width=80, so x=10, center=50
  auto bounds1 = rect1->layoutBounds();
  EXPECT_FLOAT_EQ(bounds1.x + bounds1.width * 0.5f, 50.0f);
  // rect2: right=10, width=80, so x=400-10-80=310, center=350
  auto bounds2 = rect2->layoutBounds();
  EXPECT_FLOAT_EQ(bounds2.x + bounds2.width * 0.5f, 350.0f);
  // rect3: centerX=0, centerY=0, so center=(200,150)
  auto bounds3 = rect3->layoutBounds();
  EXPECT_FLOAT_EQ(bounds3.x + bounds3.width * 0.5f, 200.0f);
  EXPECT_FLOAT_EQ(bounds3.y + bounds3.height * 0.5f, 150.0f);
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
  EXPECT_FLOAT_EQ(badge->renderPosition().x, 550.0f);
  EXPECT_FLOAT_EQ(badge->renderPosition().y, 5.0f);

  // child1 and child2 are laid out as if badge doesn't exist.
  EXPECT_FLOAT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 210.0f);  // 200 + 10
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
  EXPECT_FLOAT_EQ(parent->layoutWidth, 210.0f);
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

  EXPECT_FLOAT_EQ(child1->layoutHeight, 200.0f);
  EXPECT_FLOAT_EQ(child2->layoutHeight, 200.0f);
  EXPECT_FLOAT_EQ(child1->renderPosition().y, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().y, 0.0f);
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

  EXPECT_FLOAT_EQ(child->layoutHeight, 180.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 10.0f);
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
  EXPECT_FLOAT_EQ(child2->layoutHeight, 200.0f);
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

  EXPECT_FLOAT_EQ(child->layoutWidth, 600.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().x, 0.0f);
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
  EXPECT_FLOAT_EQ(badge->renderPosition().x, 550.0f);
  EXPECT_FLOAT_EQ(badge->renderPosition().y, 5.0f);

  // child is stretched.
  EXPECT_FLOAT_EQ(child->layoutHeight, 200.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().x, 0.0f);
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
  EXPECT_FLOAT_EQ(parent->layoutWidth, 520.0f);
  // child2 (invisible but still participates) is positioned first.
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child3->renderPosition().x, 310.0f);
  EXPECT_FLOAT_EQ(child4->renderPosition().x, 420.0f);
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
  rect->centerX = 0;
  rect->centerY = 0;
  left->contents.push_back(rect);

  doc->applyLayout();

  EXPECT_EQ(left->layoutWidth, 300.0f);
  EXPECT_EQ(right->layoutWidth, 300.0f);

  // rect centered in left (300x400): center at (150, 200)
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x + bounds.width * 0.5f, 150.0f);
  EXPECT_FLOAT_EQ(bounds.y + bounds.height * 0.5f, 200.0f);
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
  EXPECT_FLOAT_EQ(parent2->children[0]->renderPosition().x, 550.0f);
  EXPECT_FLOAT_EQ(parent2->children[0]->renderPosition().y, 5.0f);
  EXPECT_FLOAT_EQ(parent2->children[1]->renderPosition().x, 0.0f);
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
  EXPECT_FLOAT_EQ(parent2->children[0]->layoutHeight, 200.0f);
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

  EXPECT_FLOAT_EQ(child->renderPosition().x, 30.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 280.0f);
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

  EXPECT_FLOAT_EQ(child->renderPosition().y, 40.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().y, 215.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 150.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().y, 120.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 170.0f);
  // y = (300 - 60) / 2 + (-10) = 110
  EXPECT_FLOAT_EQ(child->renderPosition().y, 110.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 30.0f);
  // width = parentWidth - left - right = 400 - 30 - 50 = 320
  EXPECT_FLOAT_EQ(child->layoutWidth, 320.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().y, 20.0f);
  // height = parentHeight - top - bottom = 300 - 20 - 40 = 240
  EXPECT_FLOAT_EQ(child->layoutHeight, 240.0f);
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

  EXPECT_FLOAT_EQ(child->renderPosition().x, 10.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 30.0f);
  EXPECT_FLOAT_EQ(child->layoutWidth, 370.0f);   // 400 - 10 - 20
  EXPECT_FLOAT_EQ(child->layoutHeight, 230.0f);  // 300 - 30 - 40
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

  EXPECT_FLOAT_EQ(child->renderPosition().x, 30.0f);
  EXPECT_FLOAT_EQ(child->layoutWidth,
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

  EXPECT_FLOAT_EQ(child->renderPosition().x, 15.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 25.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 290.0f);
  // y = 300 - 20 - 60 = 220
  EXPECT_FLOAT_EQ(child->renderPosition().y, 220.0f);
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
  EXPECT_FLOAT_EQ(overlay->renderPosition().x, 380.0f);
  // y = 400 - 30 - 100 = 270
  EXPECT_FLOAT_EQ(overlay->renderPosition().y, 270.0f);

  // child1 and child2 are laid out normally without overlay.
  EXPECT_FLOAT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 160.0f);  // 150 + 10
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

  EXPECT_FLOAT_EQ(overlay->renderPosition().x, 50.0f);
  EXPECT_FLOAT_EQ(overlay->renderPosition().y, 40.0f);
  EXPECT_FLOAT_EQ(overlay->layoutWidth, 500.0f);   // 600 - 50 - 50
  EXPECT_FLOAT_EQ(overlay->layoutHeight, 300.0f);  // 400 - 40 - 60
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 210.0f);
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

  EXPECT_FLOAT_EQ(child->renderPosition().x, 55.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 77.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 30.0f);
  EXPECT_FLOAT_EQ(child->renderPosition().y, 40.0f);
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
  EXPECT_FLOAT_EQ(topLeft->renderPosition().x, 10.0f);
  EXPECT_FLOAT_EQ(topLeft->renderPosition().y, 10.0f);

  // bottomRight: x=400-10-80=310, y=300-10-60=230
  EXPECT_FLOAT_EQ(bottomRight->renderPosition().x, 310.0f);
  EXPECT_FLOAT_EQ(bottomRight->renderPosition().y, 230.0f);

  // centered: x=(400-120)/2=140, y=(300-80)/2=110
  EXPECT_FLOAT_EQ(centered->renderPosition().x, 140.0f);
  EXPECT_FLOAT_EQ(centered->renderPosition().y, 110.0f);
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 300.0f);
  // y = parentHeight - bottom - childHeight = 300 - 30 - 50 = 220
  EXPECT_FLOAT_EQ(child->renderPosition().y, 220.0f);
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
 * Test all PAGX patterns embedded in the skill patterns.md documentation.
 * Extracts complete PAGX documents from markdown code blocks and renders them.
 */
PAGX_TEST(PAGXTest, SkillPatterns) {
  TestMarkdownPatterns(context,
                       ProjectPath::Absolute(".codebuddy/skills/pagx/references/patterns.md"),
                       "skills_", 2.0f);
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
    <Rectangle width="100" height="100"/>
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
    <Rectangle width="200" height="200"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <Composition id="comp1" width="100" height="100">
      <Layer>
        <Rectangle width="50" height="50"/>
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 150.0f) << "centerX should override left";
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
  EXPECT_FLOAT_EQ(child->renderPosition().y, 120.0f) << "centerY should override top";
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
  EXPECT_FLOAT_EQ(parent->layoutHeight, 120.0f)
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
  EXPECT_FLOAT_EQ(child1->layoutWidth, 150.0f) << "Explicit width should override flex";
  // child2: remaining = 600 - 150 - 10(gap) = 440, flex share = 440/1 = 440.
  EXPECT_FLOAT_EQ(child2->layoutWidth, 440.0f) << "Flex child gets all remaining space";
  // Positioning: child1 at x=0, child2 at x=160.
  EXPECT_FLOAT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 160.0f);
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
  EXPECT_FLOAT_EQ(child2->layoutHeight, 300.0f)
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 0.0f) << "Container layout ignores left constraint";
  EXPECT_FLOAT_EQ(child->renderPosition().y, 0.0f) << "Container layout ignores top constraint";
  // Width remains 100 (explicit), NOT derived from left+right.
  EXPECT_FLOAT_EQ(child->width, 100.0f) << "Explicit width preserved";
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 100.0f) << "Second child positioned after first";
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
  EXPECT_FLOAT_EQ(child->renderPosition().x, 10.0f);
  EXPECT_FLOAT_EQ(child->layoutWidth, 370.0f) << "left+right derives width: 400 - 10 - 20 = 370";
  // Vertical: top+bottom overrides explicit height.
  EXPECT_FLOAT_EQ(child->renderPosition().y, 30.0f);
  EXPECT_FLOAT_EQ(child->layoutHeight, 230.0f) << "top+bottom derives height: 300 - 30 - 40 = 230";
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
  EXPECT_FLOAT_EQ(leftCol->layoutWidth, 300.0f) << "Left column flex=1 gets half width";
  EXPECT_FLOAT_EQ(rightCol->layoutWidth, 300.0f) << "Right column flex=1 gets half width";

  // Left column is stretched to 400 on cross-axis (height).
  EXPECT_FLOAT_EQ(leftCol->layoutHeight, 400.0f) << "Left column stretched to parent cross-axis";

  // topItem: flex=1 in Vertical, gets remaining = leftCol.height - bottomItem.height = 400 - 100 = 300.
  // centerY should be ignored since parent is Vertical layout.
  EXPECT_FLOAT_EQ(bottomItem->layoutHeight, 100.0f) << "Bottom item keeps explicit height";
  EXPECT_FLOAT_EQ(topItem->layoutHeight, 300.0f)
      << "Top item flex=1 gets remaining: 400 - 100 = 300";
  EXPECT_FLOAT_EQ(topItem->renderPosition().y, 0.0f) << "centerY ignored in container layout";
  EXPECT_FLOAT_EQ(bottomItem->renderPosition().y, 300.0f) << "Bottom item positioned after top";
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
  EXPECT_FLOAT_EQ(parent->layoutHeight, 120.0f)
      << "includeInLayout=false child should not affect parent measurement";
  // Participating children positioned normally.
  EXPECT_FLOAT_EQ(child1->renderPosition().x, 0.0f);
  EXPECT_FLOAT_EQ(child2->renderPosition().x, 200.0f);
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
  EXPECT_FLOAT_EQ(row->renderPosition().x, 70.0f)
      << "Group child left constraint should contribute to measurement";
  EXPECT_FLOAT_EQ(row->layoutWidth, 260.0f)
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
  EXPECT_FLOAT_EQ(row->layoutWidth, 180.0f)
      << "Group child centerX constraint should contribute to measurement";
  EXPECT_FLOAT_EQ(row->renderPosition().x, 110.0f) << "Row centered: (400 - 180) / 2 = 110";
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
  // layoutBounds().x = 200 (left constraint), center = 200 + 40 = 240
  auto bounds2 = rect2->layoutBounds();
  EXPECT_FLOAT_EQ(bounds2.x + bounds2.width * 0.5f, 240.0f)
      << "rect2 center.x = left + width/2 = 200 + 40 = 240";
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
    <Rectangle width="100" height="100"/>
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
    <Rectangle width="100" height="100"/>
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
    <Rectangle width="100" height="100"/>
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
    <Rectangle width="300" height="300"/>
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
    <Rectangle width="300" height="300"/>
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
  EXPECT_EQ(parent->layoutWidth, 200);
  EXPECT_EQ(parent->layoutHeight, 100);
  // clipToBounds writes scrollRect based on layoutWidth/layoutHeight.
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

  auto firstBounds = rect->layoutBounds();
  float layerActualW = layer->layoutWidth;
  float layerActualH = layer->layoutHeight;

  // Re-layout with empty font config to verify layout results are stable.
  pagx::FontConfig config;
  doc->applyLayout(&config);

  auto secondBounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(secondBounds.x, firstBounds.x);
  EXPECT_FLOAT_EQ(secondBounds.y, firstBounds.y);
  EXPECT_FLOAT_EQ(secondBounds.width, firstBounds.width);
  EXPECT_FLOAT_EQ(secondBounds.height, firstBounds.height);
  EXPECT_FLOAT_EQ(layer->layoutWidth, layerActualW);
  EXPECT_FLOAT_EQ(layer->layoutHeight, layerActualH);
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
  EXPECT_FLOAT_EQ(child1->layoutWidth, 100);
  EXPECT_FLOAT_EQ(child2->layoutWidth, 200);
}

PAGX_TEST(PAGXTest, VerifyNestedFlexNoFalsePositive) {
  // When a parent container gets its main-axis size from flex in a grandparent layout, child flex
  // items should not trigger the "flex has no effect" diagnostic.
  std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx width="400" height="300">
  <Layer width="400" height="300" layout="vertical" gap="10">
    <Layer height="40">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#3366E6"/>
    </Layer>
    <Layer flex="1" layout="vertical" gap="5">
      <Layer flex="1">
        <Rectangle left="0" right="0" top="0" bottom="0"/>
        <Fill color="#E63333"/>
      </Layer>
      <Layer height="60">
        <Rectangle left="0" right="0" top="0" bottom="0"/>
        <Fill color="#33CC4D"/>
      </Layer>
    </Layer>
  </Layer>
</pagx>)";
  auto pagxPath = SavePAGXFile(xml, "PAGXTest/verify_nested_flex.pagx");
  VerifyFile(pagxPath, "verify_nested_flex");
}

// =====================================================================================
// Auto Layout - Edge Case Fixes (P2)
// =====================================================================================

PAGX_TEST(PAGXTest, DefaultSizeElementWithOppositeEdgeConstraint) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 200;

  auto rect = doc->makeNode<pagx::Rectangle>();
  // Default width/height are NaN; left+right+top+bottom should stretch the element to fill.
  rect->left = 20;
  rect->right = 20;
  rect->top = 30;
  rect->bottom = 30;
  layer->contents.push_back(rect);

  doc->applyLayout();

  // Original size should remain NaN (not modified by layout).
  EXPECT_TRUE(std::isnan(rect->width));
  EXPECT_TRUE(std::isnan(rect->height));
  // Layout dimensions should be computed from constraints.
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.width, 160);
  EXPECT_FLOAT_EQ(bounds.height, 140);
  EXPECT_FLOAT_EQ(bounds.x, 20);
  EXPECT_FLOAT_EQ(bounds.y, 30);
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
  float total = c1->layoutWidth + c2->layoutWidth + c3->layoutWidth;
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
  // layoutBounds reflects constraint: left=50, top=50
  auto bounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x + bounds.width * 0.5f, 100);   // left=50 + width*0.5=50
  EXPECT_FLOAT_EQ(bounds.y + bounds.height * 0.5f, 100);  // top=50 + height*0.5=50
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
  auto rectBounds = rect->layoutBounds();
  EXPECT_FLOAT_EQ(rectBounds.x + rectBounds.width * 0.5f, 50);
  EXPECT_FLOAT_EQ(rectBounds.y + rectBounds.height * 0.5f, 40);
  // innerGroup positioned at left=20, top=20
  auto innerBounds = innerGroup->layoutBounds();
  EXPECT_FLOAT_EQ(innerBounds.x, 20);
  EXPECT_FLOAT_EQ(innerBounds.y, 20);
  // outerGroup positioned at left=50, top=50
  auto outerBounds = outerGroup->layoutBounds();
  EXPECT_FLOAT_EQ(outerBounds.x, 50);
  EXPECT_FLOAT_EQ(outerBounds.y, 50);
}

// =====================================================================================
// Auto Layout - New Feature Tests (P1)
// =====================================================================================

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

  doc->applyLayout(&fontConfig);

  // No opposite-edge constraints, so fontSize should remain unchanged.
  EXPECT_FLOAT_EQ(text->fontSize, 30);

  // layoutBounds reflects the constraint position: left=50, top=40.
  auto bounds = text->layoutBounds();
  EXPECT_FLOAT_EQ(bounds.x, 50);
  EXPECT_FLOAT_EQ(bounds.y, 40);
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
  EXPECT_FLOAT_EQ(group->layoutWidth, 200);
  EXPECT_FLOAT_EQ(group->layoutHeight, 100);
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
  EXPECT_FLOAT_EQ(path->layoutWidth, 360);
  // Height should scale proportionally: 80 * 3.6 = 288.
  EXPECT_FLOAT_EQ(path->layoutHeight, 288);
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
  doc->applyLayout(&fontConfig);

  // Target width = 400 - 50 - 50 = 300.
  // After scaling, text should be centered in the 300px area.
  // Verify the scaled text has a valid actual size.
  EXPECT_GT(text->layoutWidth, 0);
  // Original fontSize should be preserved, but renderFontSize() returns scaled value.
  EXPECT_EQ(text->fontSize, 30);
  EXPECT_NE(text->renderFontSize(), 30);
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
  doc->applyLayout(&fontConfig);

  auto& layoutRuns = text->glyphData->layoutRuns;
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
  doc->applyLayout(&fontConfig);

  auto& layoutRuns = text->glyphData->layoutRuns;
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
  doc->applyLayout(&fontConfig);
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
  doc->applyLayout(&fontConfig);

  auto& layoutRuns = text->glyphData->layoutRuns;
  ASSERT_FALSE(layoutRuns.empty());

  // Vertical Latin text should have per-glyph xforms (rotation for sideways glyphs).
  bool hasXforms = false;
  for (auto& run : layoutRuns) {
    if (!run.xforms.empty()) {
      hasXforms = true;
    }
  }
  EXPECT_TRUE(hasXforms);
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
  doc->applyLayout(&fontConfig);

  // Standalone Text: textBounds should have positive width and height.
  auto standaloneBounds = standalone->textBounds;
  EXPECT_GT(standaloneBounds.width, 0);
  EXPECT_GT(standaloneBounds.height, 0);

  // TextBox child Text: textBounds should also have positive dimensions.
  auto boxTextBounds = boxText->textBounds;
  EXPECT_GT(boxTextBounds.width, 0);
  EXPECT_GT(boxTextBounds.height, 0);
}

// =====================================================================================
// Padding Unified Semantics — Round-Trip Tests
// =====================================================================================

// Group padding round-trip through export/import.
PAGX_TEST(PAGXTest, GroupPaddingRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto group = doc->makeNode<pagx::Group>();
  group->padding = pagx::Padding{5, 10, 15, 20};
  layer->contents.push_back(group);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {50, 30};
  group->elements.push_back(rect);

  auto fill = doc->makeNode<pagx::Fill>();
  group->elements.push_back(fill);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* layer2 = doc2->layers[0];
  ASSERT_FALSE(layer2->contents.empty());
  auto* group2 = static_cast<pagx::Group*>(layer2->contents[0]);
  EXPECT_FLOAT_EQ(group2->padding.top, 5);
  EXPECT_FLOAT_EQ(group2->padding.right, 10);
  EXPECT_FLOAT_EQ(group2->padding.bottom, 15);
  EXPECT_FLOAT_EQ(group2->padding.left, 20);
}

// TextBox padding round-trip through export/import.
PAGX_TEST(PAGXTest, TextBoxPaddingRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->width = 180;
  textBox->height = 60;
  textBox->padding = pagx::Padding{8, 12, 8, 12};
  layer->contents.push_back(textBox);

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Test";
  text->fontFamily = "Arial";
  text->fontSize = 14;
  textBox->elements.push_back(text);

  auto fill = doc->makeNode<pagx::Fill>();
  textBox->elements.push_back(fill);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(doc2 != nullptr);
  ASSERT_GE(doc2->layers.size(), 1u);

  auto* layer2 = doc2->layers[0];
  ASSERT_FALSE(layer2->contents.empty());
  auto* textBox2 = static_cast<pagx::TextBox*>(layer2->contents[0]);
  EXPECT_FLOAT_EQ(textBox2->padding.top, 8);
  EXPECT_FLOAT_EQ(textBox2->padding.right, 12);
  EXPECT_FLOAT_EQ(textBox2->padding.bottom, 8);
  EXPECT_FLOAT_EQ(textBox2->padding.left, 12);
}

/**
 * Test all HTML-related PAGX files in resources/pagx_to_html directory.
 */
PAGX_TEST(PAGXTest, HtmlFiles) {
  TestPAGXDirectory(context, ProjectPath::Absolute("resources/pagx_to_html"), "html_native_");
}

/**
 * Test case: PAGXDocument::embed() returns false when layout is not applied.
 */
PAGX_TEST(PAGXTest, DocumentEmbedWithoutLayout) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  EXPECT_FALSE(doc->embed());
}

/**
 * Test case: PAGXDocument::embed() succeeds when layout is applied.
 */
PAGX_TEST(PAGXTest, DocumentEmbedWithLayout) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto typeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
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
  doc->applyLayout(&fontConfig);

  EXPECT_TRUE(doc->embed());
  EXPECT_FALSE(text->glyphRuns.empty());
}

/**
 * Test case: PAGXDocument::clearEmbed() clears embedded GlyphRuns.
 */
PAGX_TEST(PAGXTest, DocumentClearEmbed) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto typeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
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
  doc->applyLayout(&fontConfig);
  doc->embed();

  ASSERT_FALSE(text->glyphRuns.empty());

  doc->clearEmbed();
  EXPECT_TRUE(text->glyphRuns.empty());
}

/**
 * Test case: PAGXDocument re-embedding after clearEmbed.
 * Embeds fonts, clears, re-embeds, and verifies the result is consistent.
 */
PAGX_TEST(PAGXTest, DocumentReEmbed) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  layer->width = 200;
  layer->height = 100;

  auto typeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
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
  doc->applyLayout(&fontConfig);
  doc->embed();

  ASSERT_FALSE(text->glyphRuns.empty());
  size_t firstGlyphCount = text->glyphRuns[0]->glyphs.size();
  auto firstGlyphs = text->glyphRuns[0]->glyphs;

  // Re-embed: clear existing glyphs, re-layout, re-embed.
  doc->clearEmbed();
  EXPECT_TRUE(text->glyphRuns.empty());

  doc->applyLayout();
  doc->embed();

  ASSERT_FALSE(text->glyphRuns.empty());
  // Glyph count and IDs should match after re-embedding.
  EXPECT_EQ(text->glyphRuns[0]->glyphs.size(), firstGlyphCount);
  EXPECT_EQ(text->glyphRuns[0]->glyphs, firstGlyphs);
}

/**
 * Test case: Top-level Animations / Object / Channel / Keyframe round-trip across all six
 * TypedChannel<T> instantiations (float, bool, int, string, image, color).
 */
PAGX_TEST(PAGXTest, AnimationAllTypesRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto layer = doc->makeNode<pagx::Layer>("targetLayer");
  doc->layers.push_back(layer);

  auto animation = doc->makeNode<pagx::Animation>("main");
  animation->duration = 120;
  animation->frameRate = 60;
  animation->loop = pagx::LoopMode::PingPong;

  auto object = doc->makeNode<pagx::AnimationObject>();
  object->target = "targetLayer";
  animation->objects.push_back(object);

  auto floatProp = doc->makeNode<pagx::TypedChannel<float>>();
  floatProp->name = "alpha";
  floatProp->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  floatProp->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(floatProp);

  auto boolProp = doc->makeNode<pagx::TypedChannel<bool>>();
  boolProp->name = "visible";
  boolProp->keyframes.push_back({0, false, pagx::KeyframeInterpolationType::Hold, {}, {}});
  boolProp->keyframes.push_back({30, true, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(boolProp);

  auto intProp = doc->makeNode<pagx::TypedChannel<int>>();
  intProp->name = "blendMode";
  intProp->keyframes.push_back({0, 1, pagx::KeyframeInterpolationType::Hold, {}, {}});
  intProp->keyframes.push_back({60, 4, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(intProp);

  auto strProp = doc->makeNode<pagx::TypedChannel<std::string>>();
  strProp->name = "text";
  strProp->keyframes.push_back(
      {0, std::string("Hello"), pagx::KeyframeInterpolationType::Hold, {}, {}});
  strProp->keyframes.push_back(
      {60, std::string("World"), pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(strProp);

  auto imgProp = doc->makeNode<pagx::TypedChannel<pagx::ImageRef>>();
  imgProp->name = "image";
  imgProp->keyframes.push_back(
      {0, pagx::ImageRef{"imgA"}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  imgProp->keyframes.push_back(
      {60, pagx::ImageRef{"imgB"}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(imgProp);

  auto colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  pagx::Color colorA{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::Color colorB{0.0f, 1.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  colorProp->keyframes.push_back({0, colorA, pagx::KeyframeInterpolationType::Linear, {}, {}});
  colorProp->keyframes.push_back({60, colorB, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(colorProp);

  doc->animations.push_back(animation);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());

  auto loaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(loaded != nullptr);
  ASSERT_TRUE(loaded->errors.empty()) << "errors: " << loaded->errors.size();
  ASSERT_EQ(loaded->animations.size(), 1u);

  auto* anim2 = loaded->animations[0];
  EXPECT_EQ(anim2->id, "main");
  EXPECT_EQ(anim2->duration, 120);
  EXPECT_FLOAT_EQ(anim2->frameRate, 60.0f);
  EXPECT_EQ(anim2->loop, pagx::LoopMode::PingPong);
  ASSERT_EQ(anim2->objects.size(), 1u);

  auto* obj2 = anim2->objects[0];
  EXPECT_EQ(obj2->target, "targetLayer");
  ASSERT_EQ(obj2->channels.size(), 6u);

  ASSERT_EQ(obj2->channels[0]->valueType(), pagx::ChannelValueType::Float);
  auto* p0 = static_cast<pagx::TypedChannel<float>*>(obj2->channels[0]);
  EXPECT_EQ(p0->name, "alpha");
  ASSERT_EQ(p0->keyframes.size(), 2u);
  EXPECT_FLOAT_EQ(p0->keyframes[1].value, 1.0f);

  ASSERT_EQ(obj2->channels[1]->valueType(), pagx::ChannelValueType::Bool);
  auto* p1 = static_cast<pagx::TypedChannel<bool>*>(obj2->channels[1]);
  EXPECT_EQ(p1->keyframes[0].value, false);
  EXPECT_EQ(p1->keyframes[1].value, true);

  ASSERT_EQ(obj2->channels[2]->valueType(), pagx::ChannelValueType::Int);
  auto* p2 = static_cast<pagx::TypedChannel<int>*>(obj2->channels[2]);
  EXPECT_EQ(p2->keyframes[1].value, 4);

  ASSERT_EQ(obj2->channels[3]->valueType(), pagx::ChannelValueType::String);
  auto* p3 = static_cast<pagx::TypedChannel<std::string>*>(obj2->channels[3]);
  EXPECT_EQ(p3->keyframes[1].value, "World");

  ASSERT_EQ(obj2->channels[4]->valueType(), pagx::ChannelValueType::ImageRef);
  auto* p4 = static_cast<pagx::TypedChannel<pagx::ImageRef>*>(obj2->channels[4]);
  EXPECT_EQ(p4->keyframes[0].value.id, "imgA");
  EXPECT_EQ(p4->keyframes[1].value.id, "imgB");

  ASSERT_EQ(obj2->channels[5]->valueType(), pagx::ChannelValueType::Color);
  auto* p5 = static_cast<pagx::TypedChannel<pagx::Color>*>(obj2->channels[5]);
  EXPECT_FLOAT_EQ(p5->keyframes[0].value.red, 1.0f);
  EXPECT_FLOAT_EQ(p5->keyframes[1].value.green, 1.0f);
}

/**
 * Test case: Keyframe interpolation modes (None / Linear / Bezier / Hold)
 * and bezier-in / bezier-out attributes round-trip.
 */
PAGX_TEST(PAGXTest, KeyframeInterpolationRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);

  auto animation = doc->makeNode<pagx::Animation>("ease");
  animation->duration = 60;
  doc->animations.push_back(animation);

  auto object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  animation->objects.push_back(object);

  auto prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->name = "alpha";
  prop->keyframes.push_back(
      {0, 0.0f, pagx::KeyframeInterpolationType::Bezier, pagx::Point{0.42f, 0.0f}, pagx::Point{}});
  prop->keyframes.push_back(
      {30, 0.5f, pagx::KeyframeInterpolationType::Hold, pagx::Point{}, pagx::Point{0.58f, 1.0f}});
  prop->keyframes.push_back(
      {60, 1.0f, pagx::KeyframeInterpolationType::None, pagx::Point{}, pagx::Point{}});
  object->channels.push_back(prop);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  auto loaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(loaded != nullptr);
  ASSERT_TRUE(loaded->errors.empty());
  ASSERT_EQ(loaded->animations.size(), 1u);

  ASSERT_EQ(loaded->animations[0]->objects[0]->channels[0]->valueType(),
            pagx::ChannelValueType::Float);
  auto* prop2 =
      static_cast<pagx::TypedChannel<float>*>(loaded->animations[0]->objects[0]->channels[0]);
  ASSERT_EQ(prop2->keyframes.size(), 3u);

  EXPECT_EQ(prop2->keyframes[0].interpolation, pagx::KeyframeInterpolationType::Bezier);
  EXPECT_FLOAT_EQ(prop2->keyframes[0].bezierOut.x, 0.42f);
  EXPECT_FLOAT_EQ(prop2->keyframes[0].bezierOut.y, 0.0f);

  EXPECT_EQ(prop2->keyframes[1].interpolation, pagx::KeyframeInterpolationType::Hold);
  EXPECT_FLOAT_EQ(prop2->keyframes[1].bezierIn.x, 0.58f);
  EXPECT_FLOAT_EQ(prop2->keyframes[1].bezierIn.y, 1.0f);

  EXPECT_EQ(prop2->keyframes[2].interpolation, pagx::KeyframeInterpolationType::None);
}

/**
 * Test case: Layer.timelines attribute round-trip (comma-separated names).
 */
PAGX_TEST(PAGXTest, LayerTimelinesRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto card = doc->makeNode<pagx::Composition>("card");
  card->width = 80;
  card->height = 40;

  auto cardAnim = doc->makeNode<pagx::Animation>("enter");
  cardAnim->duration = 30;
  card->animations.push_back(cardAnim);

  auto idleAnim = doc->makeNode<pagx::Animation>("idle");
  idleAnim->duration = 60;
  card->animations.push_back(idleAnim);

  auto slot = doc->makeNode<pagx::Layer>("slot");
  slot->composition = card;
  auto enterDriver = std::make_unique<pagx::AnimationTimeline>();
  enterDriver->animationId = "enter";
  enterDriver->playing = true;
  slot->timelines.push_back(std::move(enterDriver));
  auto idleDriver = std::make_unique<pagx::AnimationTimeline>();
  idleDriver->animationId = "idle";
  idleDriver->playing = false;
  slot->timelines.push_back(std::move(idleDriver));
  doc->layers.push_back(slot);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  auto loaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(loaded != nullptr);
  ASSERT_TRUE(loaded->errors.empty());
  ASSERT_GE(loaded->layers.size(), 1u);

  auto* slot2 = loaded->layers[0];
  ASSERT_TRUE(slot2->composition != nullptr);
  EXPECT_EQ(slot2->composition->id, "card");
  ASSERT_EQ(slot2->timelines.size(), 2u);
  ASSERT_EQ(slot2->timelines[0]->timelineType(), pagx::TimelineType::Animation);
  auto* loadedEnter = static_cast<pagx::AnimationTimeline*>(slot2->timelines[0].get());
  EXPECT_EQ(loadedEnter->animationId, "enter");
  EXPECT_TRUE(loadedEnter->playing);
  auto* loadedIdle = static_cast<pagx::AnimationTimeline*>(slot2->timelines[1].get());
  EXPECT_EQ(loadedIdle->animationId, "idle");
  EXPECT_FALSE(loadedIdle->playing);
  ASSERT_EQ(slot2->composition->animations.size(), 2u);
  EXPECT_EQ(slot2->composition->animations[0]->id, "enter");
  EXPECT_EQ(slot2->composition->animations[1]->id, "idle");
}

/**
 * Test case: TypedChannel<T>::evaluateAt returns the keyframe value at or before the requested
 * frame (Hold-style staircase), validating the binary search baseline behavior.
 */
PAGX_TEST(PAGXTest, TypedChannelEvaluateAt) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  prop->keyframes.push_back({30, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  prop->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});

  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(-10)), 0.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(0)), 0.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(15)), 0.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(30)), 0.5f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(45)), 0.5f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(60)), 1.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(120)), 1.0f);
}

/**
 * Test case: PAGTimeline play/pause/stop/setCurrentTime/advance state machine, including loop
 * modes. Time is measured in microseconds.
 */
PAGX_TEST(PAGXTest, PAGTimelineStateMachine) {
  auto doc = pagx::PAGXDocument::Make(0, 0);

  auto anim = doc->makeNode<pagx::Animation>("loop");
  anim->duration = 60;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Loop;
  doc->animations.push_back(anim);

  auto file = pagx::PAGScene::Make(doc);
  auto timeline = file->getTimeline("loop");
  ASSERT_TRUE(timeline != nullptr);

  // Duration is 60 frames @ 60fps = 1_000_000 microseconds.
  EXPECT_EQ(timeline->duration(), 1'000'000);

  EXPECT_TRUE(timeline->isPlaying());
  EXPECT_EQ(timeline->currentTime(), 0);

  EXPECT_TRUE(timeline->advance(500'000));
  EXPECT_EQ(timeline->currentTime(), 500'000);

  EXPECT_TRUE(timeline->advance(600'000));
  EXPECT_EQ(timeline->currentTime(), 100'000);

  timeline->pause();
  EXPECT_FALSE(timeline->isPlaying());
  EXPECT_FALSE(timeline->advance(200'000));
  EXPECT_EQ(timeline->currentTime(), 100'000);

  timeline->setCurrentTime(400'000);
  EXPECT_EQ(timeline->currentTime(), 400'000);

  timeline->stop();
  EXPECT_FALSE(timeline->isPlaying());
  EXPECT_EQ(timeline->currentTime(), 0);

  auto onceAnim = doc->makeNode<pagx::Animation>("once");
  onceAnim->duration = 60;
  onceAnim->frameRate = 60;
  onceAnim->loop = pagx::LoopMode::Once;
  doc->animations.push_back(onceAnim);

  auto onceTimeline = file->getTimeline("once");
  EXPECT_TRUE(onceTimeline->advance(2'000'000));
  EXPECT_EQ(onceTimeline->currentTime(), 1'000'000);
  EXPECT_FALSE(onceTimeline->isPlaying());
}

/**
 * Test case: PAGTimeline advance in PingPong mode covers forward/backward/mirror paths.
 */
PAGX_TEST(PAGXTest, PAGTimelinePingPong) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto anim = doc->makeNode<pagx::Animation>("pp");
  anim->duration = 60;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::PingPong;
  doc->animations.push_back(anim);

  auto file = pagx::PAGScene::Make(doc);
  auto timeline = file->getTimeline("pp");
  ASSERT_TRUE(timeline != nullptr);
  EXPECT_EQ(timeline->currentTime(), 0);

  // Forward within first half (pos < duration, straight).
  EXPECT_TRUE(timeline->advance(500'000));
  EXPECT_EQ(timeline->currentTime(), 500'000);

  // Forward across the turn-around (pos >= duration, mirror: period - pos).
  EXPECT_TRUE(timeline->advance(1'100'000));
  EXPECT_EQ(timeline->currentTime(), 400'000);

  // Reset and test negative delta before start (pos < 0, pos += period).
  timeline->stop();
  timeline->play();
  EXPECT_TRUE(timeline->advance(-500'000));
  EXPECT_EQ(timeline->currentTime(), 500'000);

  // Negative delta across the turn-around from within the mirror half.
  EXPECT_TRUE(timeline->advance(-600'000));
  EXPECT_EQ(timeline->currentTime(), 100'000);
}

/**
 * Test case: PAGTimeline advance with negative delta in Loop and Once modes.
 */
PAGX_TEST(PAGXTest, PAGTimelineNegativeDelta) {
  auto doc = pagx::PAGXDocument::Make(0, 0);

  // Loop mode with negative delta.
  auto loopAnim = doc->makeNode<pagx::Animation>("loop");
  loopAnim->duration = 60;
  loopAnim->frameRate = 60;
  loopAnim->loop = pagx::LoopMode::Loop;
  doc->animations.push_back(loopAnim);

  // Once mode with negative delta.
  auto onceAnim = doc->makeNode<pagx::Animation>("once");
  onceAnim->duration = 60;
  onceAnim->frameRate = 60;
  onceAnim->loop = pagx::LoopMode::Once;
  doc->animations.push_back(onceAnim);

  auto file = pagx::PAGScene::Make(doc);

  // Loop: negative delta wraps correctly.
  auto loopTl = file->getTimeline("loop");
  loopTl->setCurrentTime(200'000);
  EXPECT_TRUE(loopTl->advance(-500'000));
  EXPECT_EQ(loopTl->currentTime(), 700'000);

  // Once: negative delta stops at 0 and pauses.
  auto onceTl = file->getTimeline("once");
  onceTl->setCurrentTime(300'000);
  EXPECT_TRUE(onceTl->advance(-500'000));
  EXPECT_EQ(onceTl->currentTime(), 0);
  EXPECT_FALSE(onceTl->isPlaying());
}

/**
 * Test case: PAGTimeline advance with duration == 0 short-circuits.
 */
PAGX_TEST(PAGXTest, PAGTimelineZeroDuration) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto anim = doc->makeNode<pagx::Animation>("zero");
  anim->duration = 0;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Loop;
  doc->animations.push_back(anim);

  auto file = pagx::PAGScene::Make(doc);
  auto timeline = file->getTimeline("zero");
  ASSERT_TRUE(timeline != nullptr);
  EXPECT_FALSE(timeline->advance(100'000));
  EXPECT_EQ(timeline->currentTime(), 0);
}

/**
 * Test case: a PAGTimeline outliving its PAGScene detaches instead of dereferencing freed content.
 */
PAGX_TEST(PAGXTest, PAGTimelineOutlivesScene) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto anim = doc->makeNode<pagx::Animation>("loop");
  anim->duration = 60;
  anim->frameRate = 60;
  anim->loop = pagx::LoopMode::Loop;
  doc->animations.push_back(anim);

  auto scene = pagx::PAGScene::Make(doc);
  auto timeline = scene->getTimeline("loop");
  ASSERT_TRUE(timeline != nullptr);
  timeline->setCurrentTime(200'000);

  scene.reset();

  // With the owning scene gone, advance() and apply() are no-ops and must not crash.
  EXPECT_FALSE(timeline->advance(500'000));
  EXPECT_EQ(timeline->currentTime(), 200'000);
  timeline->apply();

  // Getters must also detach: the backing animation is freed with the document, so they must not
  // dereference it and instead return neutral values.
  EXPECT_TRUE(timeline->getId().empty());
  EXPECT_EQ(timeline->duration(), 0);
  EXPECT_FLOAT_EQ(timeline->frameRate(), 0.0f);
}

/**
 * Test case: Layer.alpha channel applies with mix=1 and lerp blends with mix=0.5.
 */
PAGX_TEST(PAGXTest, ChannelLayerAlpha) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* alphaProp = doc->makeNode<pagx::TypedChannel<float>>();
  alphaProp->name = "alpha";
  alphaProp->keyframes.push_back({0, 0.25f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(alphaProp);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto& tree = *file->mutableBinding();
  auto tgfxLayer = tree.get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  tgfxLayer->setAlpha(1.0f);

  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);

  // mix=1: alpha overwritten to keyframe value 0.25.
  timeline->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.25f);

  // mix=0.5 from current 1.0 toward 0.25 = 1 + (0.25-1)*0.5 = 0.625.
  tgfxLayer->setAlpha(1.0f);
  timeline->apply(0.5f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.625f);

  // mix=0: no write.
  tgfxLayer->setAlpha(0.8f);
  timeline->apply(0.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.8f);
}

/**
 * Test case: Layer.visible is a discrete channel; mix is ignored for mix > 0 and skipped at mix=0.
 */
PAGX_TEST(PAGXTest, ChannelLayerVisibleDiscrete) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 10;
  layer->height = 10;
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* visProp = doc->makeNode<pagx::TypedChannel<bool>>();
  visProp->name = "visible";
  visProp->keyframes.push_back({0, false, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(visProp);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxLayer = tree.get<tgfx::Layer>(layer);
  tgfxLayer->setVisible(true);

  auto timeline = file->getDefaultTimeline();

  // mix=0.3 still overwrites because discrete.
  timeline->apply(0.3f);
  EXPECT_FALSE(tgfxLayer->visible());

  // mix=0 skips even discrete.
  tgfxLayer->setVisible(true);
  timeline->apply(0.0f);
  EXPECT_TRUE(tgfxLayer->visible());
}

/**
 * Test case: Layer.x channel writes the matrix tx component, blending with mix.
 */
PAGX_TEST(PAGXTest, ChannelLayerX) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 10;
  layer->height = 10;
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* xProp = doc->makeNode<pagx::TypedChannel<float>>();
  xProp->name = "x";
  xProp->keyframes.push_back({0, 100.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(xProp);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxLayer = tree.get<tgfx::Layer>(layer);

  // The layer transform is held as decomposed state on the runtime target, seeded from the node's
  // authored x (0 here). apply(0.5) mixes that baseline toward the keyframe value 100.
  auto timeline = file->getDefaultTimeline();
  timeline->apply(0.5f);
  EXPECT_FLOAT_EQ(tgfxLayer->matrix().getTranslateX(), 50.0f);  // 0 + (100-0)*0.5
}

/**
 * Test case: Text.x and Text.y channels write the tgfx Text position, blending with mix.
 */
PAGX_TEST(PAGXTest, ChannelTextPosition) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 100;
  layer->height = 100;
  doc->layers.push_back(layer);

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(typeface, nullptr);
  auto text = doc->makeNode<pagx::Text>("T");
  text->text = "Hi";
  text->fontFamily = typeface->fontFamily();
  text->fontStyle = typeface->fontStyle();
  text->fontSize = 20;
  text->position = {0, 0};
  layer->contents.push_back(text);

  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "T";
  anim->objects.push_back(object);
  auto* xProp = doc->makeNode<pagx::TypedChannel<float>>();
  xProp->name = "x";
  xProp->keyframes.push_back({0, 100.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(xProp);
  auto* yProp = doc->makeNode<pagx::TypedChannel<float>>();
  yProp->name = "y";
  yProp->keyframes.push_back({0, 80.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(yProp);

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(typeface);
  doc->applyLayout(&fontConfig);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto& tree = *file->mutableBinding();
  auto tgfxText = tree.get<tgfx::Text>(text);
  ASSERT_TRUE(tgfxText != nullptr);

  tgfxText->setPosition(tgfx::Point::Make(20, 40));

  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(0.5f);
  // x: 20 + (100-20)*0.5 = 60; y: 40 + (80-40)*0.5 = 60.
  EXPECT_FLOAT_EQ(tgfxText->position().x, 60.0f);
  EXPECT_FLOAT_EQ(tgfxText->position().y, 60.0f);
}

/**
 * Test case: SolidColor.color channel applies via solidMap with RGBA lerp.
 */
PAGX_TEST(PAGXTest, ChannelSolidColor) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto solid = doc->makeNode<pagx::SolidColor>("S");
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "S";
  anim->objects.push_back(obj);
  auto* p = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  p->name = "color";
  pagx::Color blue{0.0f, 0.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  p->keyframes.push_back({0, blue, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(p);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxSolid = tree.get<tgfx::SolidColor>(solid);
  ASSERT_TRUE(tgfxSolid != nullptr);

  auto timeline = file->getDefaultTimeline();
  timeline->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxSolid->color().red, 0.0f);
  EXPECT_FLOAT_EQ(tgfxSolid->color().blue, 1.0f);

  // Reset to red, then blend halfway toward blue.
  tgfxSolid->setColor({1.0f, 0.0f, 0.0f, 1.0f});
  timeline->apply(0.5f);
  EXPECT_FLOAT_EQ(tgfxSolid->color().red, 0.5f);
  EXPECT_FLOAT_EQ(tgfxSolid->color().blue, 0.5f);
}

/**
 * Test case: ColorStop.color and ColorStop.offset channels write through RuntimeColorStop into
 * the parent Gradient's colors() / positions() arrays.
 */
PAGX_TEST(PAGXTest, ChannelColorStop) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto gradient = doc->makeNode<pagx::LinearGradient>("G");
  gradient->startPoint = {0, 0};
  gradient->endPoint = {50, 0};

  auto stop0 = doc->makeNode<pagx::ColorStop>("Stop0");
  stop0->offset = 0.0f;
  stop0->color = {1.0f, 0.0f, 0.0f, 1.0f};

  auto stop1 = doc->makeNode<pagx::ColorStop>("Stop1");
  stop1->offset = 1.0f;
  stop1->color = {0.0f, 1.0f, 0.0f, 1.0f};

  gradient->colorStops.push_back(stop0);
  gradient->colorStops.push_back(stop1);

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = gradient;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "Stop1";
  anim->objects.push_back(obj);
  auto* colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  pagx::Color blue{0.0f, 0.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  colorProp->keyframes.push_back({0, blue, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(colorProp);
  auto* offsetProp = doc->makeNode<pagx::TypedChannel<float>>();
  offsetProp->name = "offset";
  offsetProp->keyframes.push_back({0, 0.4f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(offsetProp);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxGrad = tree.get<tgfx::Gradient>(gradient);
  ASSERT_TRUE(tgfxGrad != nullptr);
  auto stopBinding = tree.get<pagx::RuntimeColorStop>(stop1);
  ASSERT_TRUE(stopBinding != nullptr);
  EXPECT_EQ(stopBinding->gradient.get(), tgfxGrad.get());
  EXPECT_EQ(stopBinding->index, 1u);

  auto timeline = file->getDefaultTimeline();
  timeline->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxGrad->colors()[1].blue, 1.0f);
  EXPECT_FLOAT_EQ(tgfxGrad->positions()[1], 0.4f);
}

/**
 * Test case: BlurFilter.blurX/blurY channels write through blurFilterMap.
 */
PAGX_TEST(PAGXTest, ChannelBlurFilter) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto blur = doc->makeNode<pagx::BlurFilter>("B");
  blur->blurX = 2.0f;
  blur->blurY = 2.0f;
  layer->filters.push_back(blur);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "B";
  anim->objects.push_back(obj);
  auto* xProp = doc->makeNode<pagx::TypedChannel<float>>();
  xProp->name = "blurX";
  xProp->keyframes.push_back({0, 10.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(xProp);
  auto* yProp = doc->makeNode<pagx::TypedChannel<float>>();
  yProp->name = "blurY";
  yProp->keyframes.push_back({0, 6.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(yProp);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxBlur = tree.get<tgfx::BlurFilter>(blur);
  ASSERT_TRUE(tgfxBlur != nullptr);

  auto timeline = file->getDefaultTimeline();
  timeline->apply(0.5f);
  EXPECT_FLOAT_EQ(tgfxBlur->blurrinessX(), 6.0f);  // 2 + (10-2)*0.5
  EXPECT_FLOAT_EQ(tgfxBlur->blurrinessY(), 4.0f);  // 2 + (6-2)*0.5
}

/**
 * Test case: DropShadowFilter / DropShadowStyle channel writers cover offset, blur, color.
 */
PAGX_TEST(PAGXTest, ChannelDropShadow) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto dsFilter = doc->makeNode<pagx::DropShadowFilter>("F");
  dsFilter->offsetX = 0.0f;
  dsFilter->offsetY = 0.0f;
  dsFilter->blurX = 0.0f;
  dsFilter->blurY = 0.0f;
  dsFilter->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer->filters.push_back(dsFilter);

  auto dsStyle = doc->makeNode<pagx::DropShadowStyle>("S");
  dsStyle->offsetX = 0.0f;
  dsStyle->offsetY = 0.0f;
  dsStyle->blurX = 0.0f;
  dsStyle->blurY = 0.0f;
  dsStyle->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer->styles.push_back(dsStyle);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);

  auto* fObj = doc->makeNode<pagx::AnimationObject>();
  fObj->target = "F";
  anim->objects.push_back(fObj);
  auto* fx = doc->makeNode<pagx::TypedChannel<float>>();
  fx->name = "offsetX";
  fx->keyframes.push_back({0, 8.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  fObj->channels.push_back(fx);
  auto* fcolor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  fcolor->name = "color";
  fcolor->keyframes.push_back(
      {0, pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  fObj->channels.push_back(fcolor);

  auto* sObj = doc->makeNode<pagx::AnimationObject>();
  sObj->target = "S";
  anim->objects.push_back(sObj);
  auto* sblur = doc->makeNode<pagx::TypedChannel<float>>();
  sblur->name = "blurY";
  sblur->keyframes.push_back({0, 12.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  sObj->channels.push_back(sblur);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxFilter = tree.get<tgfx::DropShadowFilter>(dsFilter);
  ASSERT_TRUE(tgfxFilter != nullptr);
  auto tgfxStyle = tree.get<tgfx::DropShadowStyle>(dsStyle);
  ASSERT_TRUE(tgfxStyle != nullptr);

  auto timeline = file->getDefaultTimeline();
  timeline->apply(1.0f);

  EXPECT_FLOAT_EQ(tgfxFilter->offsetX(), 8.0f);
  EXPECT_FLOAT_EQ(tgfxFilter->color().red, 1.0f);
  EXPECT_FLOAT_EQ(tgfxStyle->blurrinessY(), 12.0f);
}

/**
 * Test case: BlendFilter.color channel writes through to the tgfx BlendFilter. The "color" channel
 * binding is the new path added when wiring BlendFilter into the runtime binding; before the fix
 * the tgfx filter was created but never registered, so the writer had no target and the keyframe
 * was silently dropped. Hold-interpolated at t=1 the channel lands exactly on the keyframe color.
 */
PAGX_TEST(PAGXTest, ChannelBlendFilter) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto blend = doc->makeNode<pagx::BlendFilter>("BL");
  blend->color = {0.0f, 0.0f, 0.0f, 1.0f};
  blend->blendMode = pagx::BlendMode::Multiply;
  layer->filters.push_back(blend);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "BL";
  anim->objects.push_back(obj);
  auto* colorProp = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  colorProp->name = "color";
  colorProp->keyframes.push_back(
      {0, pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(colorProp);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxBlend = tree.get<tgfx::BlendFilter>(blend);
  ASSERT_TRUE(tgfxBlend != nullptr);

  auto timeline = file->getDefaultTimeline();
  timeline->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxBlend->color().red, 0.0f);
  EXPECT_FLOAT_EQ(tgfxBlend->color().green, 1.0f);
  EXPECT_FLOAT_EQ(tgfxBlend->color().blue, 0.0f);
}

namespace {
static pagx::Animation* MakeAlphaAnim(pagx::PAGXDocument* doc, const std::string& id, float value) {
  auto anim = doc->makeNode<pagx::Animation>(id);
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = "L";
  anim->objects.push_back(obj);
  auto* p = doc->makeNode<pagx::TypedChannel<float>>();
  p->name = "alpha";
  p->keyframes.push_back({0, value, pagx::KeyframeInterpolationType::Hold, {}, {}});
  obj->channels.push_back(p);
  return anim;
}
}  // namespace

/**
 * Test case: Two timelines targeting the same channel stack in apply() order; later writer mixes
 * against the earlier writer's result.
 */
PAGX_TEST(PAGXTest, ChannelMultiTimelineStacking) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 10;
  layer->height = 10;
  doc->layers.push_back(layer);

  MakeAlphaAnim(doc.get(), "base", 0.0f);
  MakeAlphaAnim(doc.get(), "hint", 1.0f);

  auto file = pagx::PAGScene::Make(doc);
  auto& tree = *file->mutableBinding();
  auto tgfxLayer = tree.get<tgfx::Layer>(layer);
  tgfxLayer->setAlpha(0.5f);

  auto base = file->getTimeline("base");
  auto hint = file->getTimeline("hint");

  // base writes 0.0 fully, then hint blends 30% toward 1.0 from 0.0.
  base->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.0f);
  hint->apply(0.3f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
}

/**
 * Test case: Linear interpolation between two float keyframes returns midpoint values at half
 * progress and exact endpoints at the boundaries.
 */
PAGX_TEST(PAGXTest, KeyframeLinearFloat) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto* prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  prop->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});

  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(0, 60.0f)), 0.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(500'000, 60.0f)), 0.5f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(1'000'000, 60.0f)), 1.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(2'000'000, 60.0f)), 1.0f);  // clamp end
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(-100'000, 60.0f)), 0.0f);   // clamp start
}

/**
 * Test case: Hold interpolation returns the start value across the entire segment.
 */
PAGX_TEST(PAGXTest, KeyframeHoldFloat) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto* prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->keyframes.push_back({0, 0.25f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  prop->keyframes.push_back({60, 0.75f, pagx::KeyframeInterpolationType::Hold, {}, {}});

  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(0, 60.0f)), 0.25f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(300'000, 60.0f)), 0.25f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(900'000, 60.0f)), 0.25f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(1'000'000, 60.0f)), 0.75f);
}

/**
 * Test case: Bezier interpolation with the standard ease curve cubic-bezier(0.42,0,0.58,1) is
 * monotonic, anchored at endpoints, and crosses 0.5 near the midpoint of the segment.
 */
PAGX_TEST(PAGXTest, KeyframeBezierFloatStandardEase) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto* prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->keyframes.push_back(
      {0, 0.0f, pagx::KeyframeInterpolationType::Bezier, pagx::Point{0.42f, 0.0f}, {}});
  prop->keyframes.push_back(
      {60, 1.0f, pagx::KeyframeInterpolationType::None, {}, pagx::Point{0.58f, 1.0f}});

  // Endpoints anchor exactly.
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(0, 60.0f)), 0.0f);
  EXPECT_FLOAT_EQ(std::get<float>(prop->evaluateAt(1'000'000, 60.0f)), 1.0f);

  // Symmetric ease should pass through 0.5 at the temporal midpoint.
  auto mid = std::get<float>(prop->evaluateAt(500'000, 60.0f));
  EXPECT_NEAR(mid, 0.5f, 1.0e-3f);

  // Monotonic increasing.
  auto q1 = std::get<float>(prop->evaluateAt(250'000, 60.0f));
  auto q3 = std::get<float>(prop->evaluateAt(750'000, 60.0f));
  EXPECT_GT(q1, 0.0f);
  EXPECT_LT(q1, mid);
  EXPECT_GT(q3, mid);
  EXPECT_LT(q3, 1.0f);
}

/**
 * Test case: Color keyframes lerp per RGBA component with linear interpolation.
 */
PAGX_TEST(PAGXTest, KeyframeLinearColor) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto* prop = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  pagx::Color red{1.0f, 0.0f, 0.0f, 1.0f, pagx::ColorSpace::SRGB};
  pagx::Color blue{0.0f, 0.0f, 1.0f, 1.0f, pagx::ColorSpace::SRGB};
  prop->keyframes.push_back({0, red, pagx::KeyframeInterpolationType::Linear, {}, {}});
  prop->keyframes.push_back({60, blue, pagx::KeyframeInterpolationType::Linear, {}, {}});

  auto mid = std::get<pagx::Color>(prop->evaluateAt(500'000, 60.0f));
  EXPECT_FLOAT_EQ(mid.red, 0.5f);
  EXPECT_FLOAT_EQ(mid.green, 0.0f);
  EXPECT_FLOAT_EQ(mid.blue, 0.5f);
  EXPECT_FLOAT_EQ(mid.alpha, 1.0f);
}

/**
 * Test case: Discrete keyframe types (bool, int, std::string) ignore Linear/Bezier interpolation
 * and fall back to Hold semantics — value jumps at the segment boundary instead of blending.
 */
PAGX_TEST(PAGXTest, KeyframeDiscreteHold) {
  auto doc = pagx::PAGXDocument::Make(0, 0);

  auto* boolProp = doc->makeNode<pagx::TypedChannel<bool>>();
  boolProp->keyframes.push_back({0, false, pagx::KeyframeInterpolationType::Linear, {}, {}});
  boolProp->keyframes.push_back({60, true, pagx::KeyframeInterpolationType::Linear, {}, {}});
  EXPECT_FALSE(std::get<bool>(boolProp->evaluateAt(500'000, 60.0f)));
  EXPECT_TRUE(std::get<bool>(boolProp->evaluateAt(1'000'000, 60.0f)));

  auto* intProp = doc->makeNode<pagx::TypedChannel<int>>();
  intProp->keyframes.push_back({0, 1, pagx::KeyframeInterpolationType::Linear, {}, {}});
  intProp->keyframes.push_back({60, 4, pagx::KeyframeInterpolationType::Linear, {}, {}});
  EXPECT_EQ(std::get<int>(intProp->evaluateAt(500'000, 60.0f)), 1);
  EXPECT_EQ(std::get<int>(intProp->evaluateAt(1'000'000, 60.0f)), 4);

  auto* strProp = doc->makeNode<pagx::TypedChannel<std::string>>();
  strProp->keyframes.push_back(
      {0, std::string("a"), pagx::KeyframeInterpolationType::Linear, {}, {}});
  strProp->keyframes.push_back(
      {60, std::string("b"), pagx::KeyframeInterpolationType::Linear, {}, {}});
  EXPECT_EQ(std::get<std::string>(strProp->evaluateAt(500'000, 60.0f)), "a");
  EXPECT_EQ(std::get<std::string>(strProp->evaluateAt(1'000'000, 60.0f)), "b");
}

/**
 * Test case: evaluateAt(microseconds, frameRate) is consistent with evaluateAt(frame) at exact
 * keyframe times for Linear interpolation, and continuous in between for the float path.
 */
PAGX_TEST(PAGXTest, KeyframeMicrosVsFrameAlignment) {
  auto doc = pagx::PAGXDocument::Make(0, 0);
  auto* prop = doc->makeNode<pagx::TypedChannel<float>>();
  prop->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  prop->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});

  // Frame 30 corresponds to 500_000 microseconds at 60 fps.
  auto byMicros = std::get<float>(prop->evaluateAt(500'000, 60.0f));
  auto byFrame = std::get<float>(prop->evaluateAt(30));
  EXPECT_NEAR(byMicros, byFrame, 1.0e-6f);
}

namespace {
// Helper for nested-composition tests: builds a Composition with a single child Layer that holds
// a SolidColor (alpha-target) plus an Animation driving the child's `alpha` channel from 0 to 1.
// Returns (composition, animationId, childLayer, solidColor).
struct NestedCompFixture {
  pagx::Composition* comp = nullptr;
  std::string animationId;
  pagx::Layer* childLayer = nullptr;
  pagx::SolidColor* solid = nullptr;
  pagx::TypedChannel<float>* alphaChannel = nullptr;
};

NestedCompFixture MakeAlphaComposition(pagx::PAGXDocument* doc, const std::string& compId,
                                       const std::string& animId, const std::string& childLayerId) {
  NestedCompFixture fx;
  fx.comp = doc->makeNode<pagx::Composition>(compId);
  fx.comp->width = 50;
  fx.comp->height = 50;

  fx.childLayer = doc->makeNode<pagx::Layer>(childLayerId);
  fx.childLayer->width = 50;
  fx.childLayer->height = 50;
  fx.comp->layers.push_back(fx.childLayer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  fx.childLayer->contents.push_back(rect);

  fx.solid = doc->makeNode<pagx::SolidColor>();
  fx.solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = fx.solid;
  fx.childLayer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>(animId);
  anim->duration = 60;
  anim->frameRate = 60;
  fx.comp->animations.push_back(anim);
  fx.animationId = animId;

  auto* obj = doc->makeNode<pagx::AnimationObject>();
  obj->target = childLayerId;
  anim->objects.push_back(obj);

  auto* alphaProp = doc->makeNode<pagx::TypedChannel<float>>();
  alphaProp->name = "alpha";
  alphaProp->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  alphaProp->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  obj->channels.push_back(alphaProp);
  fx.alphaChannel = alphaProp;

  return fx;
}
}  // namespace

/**
 * Test case: A nested Composition slot with a single AnimationTimeline driver actually drives the
 * child Layer's alpha channel — the slot's per-slot binding is wired through to runtime writers,
 * and the spawned timeline plays automatically by default.
 */
PAGX_TEST(PAGXTest, CompositionSlotSingleDriver) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fx = MakeAlphaComposition(doc.get(), "card", "cardEnter", "cardChild");

  auto slot = doc->makeNode<pagx::Layer>("slot");
  slot->composition = fx.comp;
  slot->width = 50;
  slot->height = 50;
  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = fx.animationId;
  driver->playing = true;
  slot->timelines.push_back(std::move(driver));
  doc->layers.push_back(slot);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(file->rootComposition()->children.size(), 1u);

  // Per-slot binding should expose the child Layer's tgfx instance — and that instance must
  // differ from anything stored in the top-level binding (top-level has the slot Layer only).
  auto& slotTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto tgfxChild = slotTree.get<tgfx::Layer>(fx.childLayer);
  ASSERT_TRUE(tgfxChild != nullptr);

  // Initial state: alpha defaults to 1.0 (tgfx default) until apply runs.
  EXPECT_FLOAT_EQ(tgfxChild->alpha(), 1.0f);

  // Advance by 30 frames @ 60fps = 500_000 us. Linear keyframe should write alpha = 0.5.
  file->advanceAndApply(500'000);
  EXPECT_NEAR(tgfxChild->alpha(), 0.5f, 1.0e-3f);

  // Advance to the end of the segment (another 500_000 us).
  file->advanceAndApply(500'000);
  EXPECT_NEAR(tgfxChild->alpha(), 1.0f, 1.0e-3f);
}

/**
 * Test case: Two slot Layers referencing the same Composition keep their alpha state independent
 * because each slot owns its own per-slot layerMap and its own driver-spawned PAGTimeline.
 *
 * Setup: slotA starts the timeline at t=0, slotB has its driver paused so its child stays at
 * the keyframe's start value. After advancing, slotA's child alpha changes; slotB's stays put.
 */
PAGX_TEST(PAGXTest, CompositionSlotIndependentState) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fx = MakeAlphaComposition(doc.get(), "card", "cardEnter", "cardChild");

  auto slotA = doc->makeNode<pagx::Layer>("slotA");
  slotA->composition = fx.comp;
  slotA->width = 50;
  slotA->height = 50;
  auto driverA = std::make_unique<pagx::AnimationTimeline>();
  driverA->animationId = fx.animationId;
  driverA->playing = true;
  slotA->timelines.push_back(std::move(driverA));
  doc->layers.push_back(slotA);

  auto slotB = doc->makeNode<pagx::Layer>("slotB");
  slotB->composition = fx.comp;
  slotB->width = 50;
  slotB->height = 50;
  auto driverB = std::make_unique<pagx::AnimationTimeline>();
  driverB->animationId = fx.animationId;
  driverB->playing = false;  // paused
  slotB->timelines.push_back(std::move(driverB));
  doc->layers.push_back(slotB);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_EQ(file->rootComposition()->children.size(), 2u);

  auto& treeA =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto& treeB =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[1].get())->binding;
  auto tgfxChildA = treeA.get<tgfx::Layer>(fx.childLayer);
  auto tgfxChildB = treeB.get<tgfx::Layer>(fx.childLayer);
  EXPECT_NE(tgfxChildA.get(), tgfxChildB.get());

  // Reset alphas to a known value, then advance. Slot A is playing, slot B is paused.
  tgfxChildA->setAlpha(1.0f);
  tgfxChildB->setAlpha(1.0f);
  file->advanceAndApply(500'000);

  // Slot A's child reflects the half-progress alpha (0.5).
  EXPECT_NEAR(tgfxChildA->alpha(), 0.5f, 1.0e-3f);
  // Slot B's child apply still ran (apply is independent of playing state) but the timeline's
  // currentTime stayed at 0, so the start-of-segment value (alpha=0) is written.
  EXPECT_NEAR(tgfxChildB->alpha(), 0.0f, 1.0e-3f);
}

/**
 * Test case: A Composition nested inside another Composition has its driver-spawned timeline
 * advanced too. The inner composition is built recursively (as a child runtime node) and its
 * timeline must be reached by PAGScene::advance, otherwise the nested animation stays frozen.
 */
PAGX_TEST(PAGXTest, CompositionNestedDriver) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  // Inner composition with a child Layer whose alpha is animated 0 -> 1.
  auto inner = MakeAlphaComposition(doc.get(), "inner", "innerEnter", "innerChild");

  // Outer composition holds a Layer that references the inner composition and carries a driver
  // for the inner animation. This Layer becomes a nested child composition of the outer one.
  auto outerComp = doc->makeNode<pagx::Composition>("outer");
  outerComp->width = 50;
  outerComp->height = 50;
  auto innerRefLayer = doc->makeNode<pagx::Layer>("innerRef");
  innerRefLayer->width = 50;
  innerRefLayer->height = 50;
  innerRefLayer->composition = inner.comp;
  auto innerDriver = std::make_unique<pagx::AnimationTimeline>();
  innerDriver->animationId = inner.animationId;
  innerDriver->playing = true;
  innerRefLayer->timelines.push_back(std::move(innerDriver));
  outerComp->layers.push_back(innerRefLayer);

  // Top-level Layer references the outer composition (no driver of its own).
  auto outerRefLayer = doc->makeNode<pagx::Layer>("outerRef");
  outerRefLayer->composition = outerComp;
  outerRefLayer->width = 50;
  outerRefLayer->height = 50;
  doc->layers.push_back(outerRefLayer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(file->rootComposition()->children.size(), 1u);

  // The inner child's tgfx layer lives in the nested child composition's binding.
  auto& outerComposition =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get());
  ASSERT_EQ(outerComposition.children.size(), 1u);
  auto& innerComposition = *static_cast<pagx::PAGComposition*>(outerComposition.children[0].get());
  auto tgfxInnerChild = innerComposition.binding->get<tgfx::Layer>(inner.childLayer);
  ASSERT_TRUE(tgfxInnerChild != nullptr);

  // Advance 30 frames @ 60fps = 500_000 us. The nested timeline must be driven to alpha = 0.5.
  file->advanceAndApply(500'000);
  EXPECT_NEAR(tgfxInnerChild->alpha(), 0.5f, 1.0e-3f);

  // Advance to the end of the segment.
  file->advanceAndApply(500'000);
  EXPECT_NEAR(tgfxInnerChild->alpha(), 1.0f, 1.0e-3f);
}

/**
 * Test case: PAGDisplayOptions getters/setters round-trip the configured values.
 */
PAGX_TEST(PAGXTest, DisplayOptionsSetGet) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto* options = file->getDisplayOptions();
  ASSERT_TRUE(options != nullptr);

  options->setZoomScale(2.0f);
  EXPECT_FLOAT_EQ(options->getZoomScale(), 2.0f);

  options->setZoomScalePrecision(100);
  EXPECT_EQ(options->getZoomScalePrecision(), 100);

  options->setContentOffset(12.0f, -7.0f);
  auto offset = options->getContentOffset();
  EXPECT_FLOAT_EQ(offset.x, 12.0f);
  EXPECT_FLOAT_EQ(offset.y, -7.0f);

  options->setRenderMode(pagx::PAGRenderMode::Tiled);
  EXPECT_EQ(options->getRenderMode(), pagx::PAGRenderMode::Tiled);

  options->setTileSize(512);
  EXPECT_EQ(options->getTileSize(), 512);

  options->setMaxTileCount(64);
  EXPECT_EQ(options->getMaxTileCount(), 64);

  EXPECT_EQ(options->getTileUpdateMode(), pagx::PAGTileUpdateMode::Immediate);
  for (auto mode : {pagx::PAGTileUpdateMode::Immediate, pagx::PAGTileUpdateMode::Smooth,
                    pagx::PAGTileUpdateMode::Fast}) {
    options->setTileUpdateMode(mode);
    EXPECT_EQ(options->getTileUpdateMode(), mode);
  }

  options->setMaxTilesRefinedPerFrame(8);
  EXPECT_EQ(options->getMaxTilesRefinedPerFrame(), 8);

  pagx::Color background = {0.1f, 0.2f, 0.3f, 0.4f, pagx::ColorSpace::SRGB};
  options->setBackgroundColor(background);
  EXPECT_EQ(options->getBackgroundColor(), background);

  options->setSubtreeCacheMaxSize(1024);
  EXPECT_EQ(options->getSubtreeCacheMaxSize(), 1024);

  const auto constScene = std::const_pointer_cast<const pagx::PAGScene>(file);
  EXPECT_EQ(constScene->getDisplayOptions(), options);
}

PAGX_TEST(PAGXTest, CompositionDriveSemantics) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fx = MakeAlphaComposition(doc.get(), "card", "cardEnter", "cardChild");

  // Top-level "main" animation drives a top-level Layer's alpha; driven explicitly by the caller.
  auto bgLayer = doc->makeNode<pagx::Layer>("bg");
  bgLayer->width = 100;
  bgLayer->height = 100;
  doc->layers.push_back(bgLayer);

  auto mainAnim = doc->makeNode<pagx::Animation>("main");
  mainAnim->duration = 60;
  mainAnim->frameRate = 60;
  doc->animations.push_back(mainAnim);
  auto* mainObj = doc->makeNode<pagx::AnimationObject>();
  mainObj->target = "bg";
  mainAnim->objects.push_back(mainObj);
  auto* mainProp = doc->makeNode<pagx::TypedChannel<float>>();
  mainProp->name = "alpha";
  mainProp->keyframes.push_back({0, 0.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  mainProp->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  mainObj->channels.push_back(mainProp);

  // Another top-level animation that must stay untouched by PAGScene::advance.
  auto hintAnim = doc->makeNode<pagx::Animation>("hint");
  hintAnim->duration = 60;
  hintAnim->frameRate = 60;
  doc->animations.push_back(hintAnim);
  auto* hintObj = doc->makeNode<pagx::AnimationObject>();
  hintObj->target = "bg";
  hintAnim->objects.push_back(hintObj);
  auto* hintProp = doc->makeNode<pagx::TypedChannel<float>>();
  hintProp->name = "alpha";
  hintProp->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  hintProp->keyframes.push_back({60, 1.0f, pagx::KeyframeInterpolationType::Linear, {}, {}});
  hintObj->channels.push_back(hintProp);

  // Layer referencing a composition + an auto-play driver.
  auto compLayer = doc->makeNode<pagx::Layer>("compLayer");
  compLayer->composition = fx.comp;
  compLayer->width = 50;
  compLayer->height = 50;
  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = fx.animationId;
  driver->playing = true;
  compLayer->timelines.push_back(std::move(driver));
  doc->layers.push_back(compLayer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto mainTimeline = file->getTimeline("main");
  auto hintTimeline = file->getTimeline("hint");
  ASSERT_TRUE(mainTimeline != nullptr);
  ASSERT_TRUE(hintTimeline != nullptr);

  // PAGScene::advance drives only composition-spawned timelines; top-level animations are not
  // touched, regardless of their playing state.
  mainTimeline->play();
  file->rootComposition()->advance(500'000);
  EXPECT_EQ(mainTimeline->currentTime(), 0);
  EXPECT_EQ(hintTimeline->currentTime(), 0);

  // The composition's driver timeline did advance — verify via the child alpha after apply.
  file->rootComposition()->apply();
  auto& compositionTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[1].get())->binding;
  auto tgfxChild = compositionTree.get<tgfx::Layer>(fx.childLayer);
  EXPECT_NEAR(tgfxChild->alpha(), 0.5f, 1.0e-3f);

  // Top-level main is driven explicitly by the caller.
  mainTimeline->advanceAndApply(500'000);
  EXPECT_EQ(mainTimeline->currentTime(), 500'000);
}

namespace {
std::shared_ptr<pagx::Data> MakePAGXData(const std::string& xml) {
  return pagx::Data::MakeWithCopy(xml.data(), xml.size());
}

std::string MakeCompositionRefXML(const std::string& compositionPath) {
  return "<pagx width=\"50\" height=\"50\">\n  <Layer composition=\"" + compositionPath +
         "\"/>\n</pagx>\n";
}

std::string MakeExternalCompositionXML(const std::string& layerId, const std::string& animationId) {
  std::string xml;
  xml += "<pagx width=\"50\" height=\"50\">\n";
  xml += "  <Layer id=\"" + layerId + "\" width=\"50\" height=\"50\">\n";
  xml += "    <Rectangle width=\"50\" height=\"50\"/>\n";
  xml += "    <Fill>\n";
  xml += "      <SolidColor color=\"#FF0000\"/>\n";
  xml += "    </Fill>\n";
  xml += "  </Layer>\n";
  xml += "  <Animations>\n";
  xml += "    <Animation id=\"" + animationId + "\" duration=\"60\" frameRate=\"60\">\n";
  xml += "      <Object target=\"" + layerId + "\">\n";
  xml += "        <Channel name=\"alpha\" type=\"float\">\n";
  xml += "          <Key time=\"0\" value=\"0\" interpolation=\"linear\"/>\n";
  xml += "          <Key time=\"60\" value=\"1\"/>\n";
  xml += "        </Channel>\n";
  xml += "      </Object>\n";
  xml += "    </Animation>\n";
  xml += "  </Animations>\n";
  xml += "</pagx>\n";
  return xml;
}
}  // namespace

/**
 * Test case: external PAGX composition data is loaded through the same getExternalFilePaths() /
 * loadFileData() flow used by external image files.
 */
PAGX_TEST(PAGXTest, ExternalPAGXCompositionLoadFileData) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\">\n"
      "    <Timelines>\n"
      "      <Animation ref=\"@fade\" playing=\"true\"/>\n"
      "    </Timelines>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  auto paths = doc->getExternalFilePaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "child.pagx");

  auto childXML = MakeExternalCompositionXML("childLayer", "fade");
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));
  EXPECT_TRUE(doc->getExternalFilePaths().empty());

  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->composition != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(file->rootComposition()->children.size(), 1u);
  file->advanceAndApply(500'000);

  auto* externalChild = slotLayer->externalDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(externalChild != nullptr);
  auto& slotTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto tgfxChild = slotTree.get<tgfx::Layer>(externalChild);
  ASSERT_TRUE(tgfxChild != nullptr);
  EXPECT_NEAR(tgfxChild->alpha(), 0.5f, 1.0e-3f);
}

/**
 * Test case: editing a child (external) document and calling its own notifyChange refreshes the
 * embedded subtree inside a parent scene that references it. The parent scene reverse-registered
 * with the child document, so the edit is reflected (the scene rebuilds its runtime tree).
 */
PAGX_TEST(PAGXTest, ExternalPAGXChildEditSyncsToParentScene) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx",
                                MakePAGXData(MakeExternalCompositionXML("childLayer", "fade"))));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayerNode = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayerNode != nullptr);
  {
    auto& slotTree =
        *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
    ASSERT_TRUE(slotTree.get<tgfx::Layer>(childLayerNode) != nullptr);
    EXPECT_FLOAT_EQ(slotTree.get<tgfx::Layer>(childLayerNode)->alpha(), 1.0f);
  }

  // Edit a node owned by the CHILD document and notify through the CHILD document. The parent scene
  // is reverse-registered, so it rebuilds its runtime tree and reflects the new value.
  childLayerNode->alpha = 0.4f;
  childDoc->notifyChange({childLayerNode}, false);

  auto& rebuiltTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto refreshed = rebuiltTree.get<tgfx::Layer>(childLayerNode);
  ASSERT_TRUE(refreshed != nullptr);
  EXPECT_FLOAT_EQ(refreshed->alpha(), 0.4f);
}

/**
 * Test case: a top-level timeline cached by the caller keeps driving correctly after the scene
 * rebuilds its runtime tree (triggered by editing an embedded external child document). The
 * timeline resolves the scene's current root binding lazily at apply time, so the cached handle
 * does not dangle when the old binding is freed by the rebuild.
 */
PAGX_TEST(PAGXTest, TopLevelTimelineSurvivesExternalChildRebuild) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"mainLayer\" width=\"100\" height=\"100\">\n"
      "    <Rectangle width=\"100\" height=\"100\"/>\n"
      "    <Fill>\n"
      "      <SolidColor color=\"#00FF00\"/>\n"
      "    </Fill>\n"
      "  </Layer>\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "  <Animations>\n"
      "    <Animation id=\"spin\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"mainLayer\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\" interpolation=\"linear\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx",
                                MakePAGXData(MakeExternalCompositionXML("childLayer", "fade"))));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);
  auto* mainLayer = doc->findNode<pagx::Layer>("mainLayer");
  ASSERT_TRUE(mainLayer != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Cache the top-level timeline handle BEFORE the rebuild, like a caller that keeps it per frame.
  // Drive to a mid value (0.5) distinct from the layer's default alpha (1.0) so the assertion proves
  // the timeline actually wrote through the binding, not that the value happened to match.
  auto cachedTimeline = scene->getDefaultTimeline();
  ASSERT_TRUE(cachedTimeline != nullptr);
  cachedTimeline->setCurrentTime(500'000);  // frame 30 of a 60-frame, 60fps animation
  cachedTimeline->apply(1.0f);
  EXPECT_FLOAT_EQ(scene->mutableBinding()->get<tgfx::Layer>(mainLayer)->alpha(), 0.5f);

  // Edit the CHILD document and notify through it: the parent scene rebuilds its runtime tree, which
  // frees the old root binding the cached timeline was originally built against.
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayerNode = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayerNode != nullptr);
  childLayerNode->alpha = 0.3f;
  childDoc->notifyChange({childLayerNode}, false);

  // Applying the CACHED handle must re-resolve the new binding and drive the value, not crash.
  cachedTimeline->setCurrentTime(500'000);
  cachedTimeline->apply(1.0f);
  EXPECT_FLOAT_EQ(scene->mutableBinding()->get<tgfx::Layer>(mainLayer)->alpha(), 0.5f);
}

/**
 * Test case: the scene's display options (zoom scale and content offset) persist across a runtime
 * tree rebuild triggered by an external-child edit. buildRuntimeTree only swaps the root layer on
 * the persistent displayList, so the zoom/offset stored on that displayList must survive untouched.
 */
PAGX_TEST(PAGXTest, DisplayOptionsSurviveExternalChildRebuild) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx",
                                MakePAGXData(MakeExternalCompositionXML("childLayer", "fade"))));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* options = scene->getDisplayOptions();
  ASSERT_TRUE(options != nullptr);
  options->setZoomScale(2.0f);
  options->setContentOffset(30.0f, 40.0f);

  // Edit the CHILD document and notify through it to force the parent scene to rebuild its tree.
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayerNode = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayerNode != nullptr);
  childLayerNode->alpha = 0.5f;
  childDoc->notifyChange({childLayerNode}, false);

  // Zoom and offset live on the persistent displayList, so the rebuild must leave them unchanged.
  EXPECT_FLOAT_EQ(options->getZoomScale(), 2.0f);
  EXPECT_FLOAT_EQ(options->getContentOffset().x, 30.0f);
  EXPECT_FLOAT_EQ(options->getContentOffset().y, 40.0f);
}

/**
 * Test case: deep (A->B->C) reverse-registration. Document A embeds child B.pagx, which embeds
 * grandchild C.pagx. Editing a node in the grandchild document C and notifying through C must
 * refresh A's embedded subtree, proving the root scene reverse-registered all the way down (C's
 * liveScenes contains A's scene), not just one level.
 */
PAGX_TEST(PAGXTest, ExternalPAGXGrandchildEditSyncsToRootScene) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slotB\" composition=\"b.pagx\"/>\n"
      "</pagx>\n";
  // B embeds C through a layer whose composition points at c.pagx.
  std::string childBXML =
      "<pagx width=\"60\" height=\"60\">\n"
      "  <Layer id=\"slotC\" composition=\"c.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("b.pagx", MakePAGXData(childBXML)));
  // After B is loaded its own external file (c.pagx) becomes enumerable; load it too.
  EXPECT_TRUE(
      doc->loadFileData("c.pagx", MakePAGXData(MakeExternalCompositionXML("grandLayer", "fade"))));
  EXPECT_TRUE(doc->getExternalFilePaths().empty());

  auto* slotB = doc->findNode<pagx::Layer>("slotB");
  ASSERT_TRUE(slotB != nullptr);
  ASSERT_TRUE(slotB->externalDoc != nullptr);
  auto* bDoc = slotB->externalDoc.get();
  auto* slotC = bDoc->findNode<pagx::Layer>("slotC");
  ASSERT_TRUE(slotC != nullptr);
  ASSERT_TRUE(slotC->externalDoc != nullptr);
  auto* cDoc = slotC->externalDoc.get();
  auto* grandLayer = cDoc->findNode<pagx::Layer>("grandLayer");
  ASSERT_TRUE(grandLayer != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // The root scene must have reverse-registered into the GRANDCHILD document, otherwise notifying
  // through cDoc would be a silent no-op (a missing deep-registration bug).
  ASSERT_FALSE(cDoc->liveScenes.empty());
  bool rootRegistered = false;
  for (auto& weakScene : cDoc->liveScenes) {
    if (weakScene.lock() == scene) {
      rootRegistered = true;
      break;
    }
  }
  EXPECT_TRUE(rootRegistered);

  // Locate the grandchild's runtime layer through A's tree: root -> B composition -> C composition.
  auto* bComposition =
      static_cast<pagx::PAGComposition*>(scene->rootComposition()->children[0].get());
  auto* cComposition = static_cast<pagx::PAGComposition*>(bComposition->children[0].get());
  ASSERT_TRUE(cComposition->binding->get<tgfx::Layer>(grandLayer) != nullptr);
  EXPECT_FLOAT_EQ(cComposition->binding->get<tgfx::Layer>(grandLayer)->alpha(), 1.0f);

  // Edit the grandchild node and notify through the grandchild document: A's scene rebuilds and the
  // deeply embedded subtree reflects the new value.
  grandLayer->alpha = 0.25f;
  cDoc->notifyChange({grandLayer}, false);

  auto* rebuiltB = static_cast<pagx::PAGComposition*>(scene->rootComposition()->children[0].get());
  auto* rebuiltC = static_cast<pagx::PAGComposition*>(rebuiltB->children[0].get());
  auto refreshed = rebuiltC->binding->get<tgfx::Layer>(grandLayer);
  ASSERT_TRUE(refreshed != nullptr);
  EXPECT_FLOAT_EQ(refreshed->alpha(), 0.25f);
}

/**
 * Test case: a parent document must not notify a node owned by a child (external) document. Such a
 * node is not in the parent's node list, so notifyChange filters it out and the embedded value
 * stays unchanged. ownsNode() returns false for the foreign node so callers can predicate the call.
 */
PAGX_TEST(PAGXTest, ExternalPAGXParentCannotNotifyChildNode) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx",
                                MakePAGXData(MakeExternalCompositionXML("childLayer", "fade"))));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);
  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayer = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayer != nullptr);
  auto& slotTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto childTgfx = slotTree.get<tgfx::Layer>(childLayer);
  ASSERT_TRUE(childTgfx != nullptr);
  EXPECT_FLOAT_EQ(childTgfx->alpha(), 1.0f);

  // ownsNode lets callers detect cross-document mis-routing before calling notifyChange.
  EXPECT_FALSE(doc->ownsNode(childLayer));
  EXPECT_TRUE(childDoc->ownsNode(childLayer));
  EXPECT_TRUE(doc->ownsNode(slotLayer));

  // The parent document does not own the child layer, so it is filtered out of the dirty list and
  // the embedded value stays unchanged. The call still logs an error reporting the dropped node.
  childLayer->alpha = 0.2f;
  doc->notifyChange({childLayer}, false);
  EXPECT_FLOAT_EQ(childTgfx->alpha(), 1.0f);
}

/**
 * Test case: a mixed dirty list with both an owned node and a foreign (child-document) node has
 * the foreign node filtered out while the owned node still refreshes; one mis-routed entry must
 * not block the rest of the batch.
 */
PAGX_TEST(PAGXTest, ExternalPAGXMixedDirtyListFiltersForeignNode) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"local\" width=\"40\" height=\"40\"/>\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx",
                                MakePAGXData(MakeExternalCompositionXML("childLayer", "fade"))));
  auto* localLayer = doc->findNode<pagx::Layer>("local");
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(localLayer != nullptr);
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);
  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayer = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayer != nullptr);

  auto localTgfx = file->mutableBinding()->get<tgfx::Layer>(localLayer);
  auto& slotTree =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[1].get())->binding;
  auto childTgfx = slotTree.get<tgfx::Layer>(childLayer);
  ASSERT_TRUE(localTgfx != nullptr);
  ASSERT_TRUE(childTgfx != nullptr);
  EXPECT_FLOAT_EQ(localTgfx->alpha(), 1.0f);
  EXPECT_FLOAT_EQ(childTgfx->alpha(), 1.0f);

  // Edit the owned local node and pass both the owned node and a foreign one in the same call.
  // The foreign node is filtered out (the parent does not own it) but the local one still
  // refreshes — the batch is not rejected.
  localLayer->alpha = 0.4f;
  childLayer->alpha = 0.2f;
  doc->notifyChange({localLayer, childLayer}, false);
  EXPECT_FLOAT_EQ(localTgfx->alpha(), 0.4f);
  EXPECT_FLOAT_EQ(childTgfx->alpha(), 1.0f);
}

/**
 * Test case: external file enumeration continues through loaded external PAGX documents.
 */
PAGX_TEST(PAGXTest, ExternalPAGXCompositionNestedFiles) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  std::string childXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Resources>\n"
      "    <Image id=\"img\" source=\"nested.png\"/>\n"
      "  </Resources>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));

  auto paths = doc->getExternalFilePaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "nested.png");

  uint8_t bytes[] = {1, 2, 3, 4};
  auto imageData = pagx::Data::MakeWithCopy(bytes, sizeof(bytes));
  EXPECT_TRUE(doc->loadFileData("nested.png", imageData));
  EXPECT_TRUE(doc->getExternalFilePaths().empty());
}

/**
 * Test case: a self-referencing external composition (a.pagx whose layer references a.pagx) is
 * detected as a cycle, reported on the root document, and getExternalFilePaths() converges to empty
 * so the host load loop terminates.
 */
PAGX_TEST(PAGXTest, ExternalPAGXCompositionSelfReferenceCycle) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer composition=\"a.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  auto paths = doc->getExternalFilePaths();
  ASSERT_EQ(paths.size(), 1u);
  EXPECT_EQ(paths[0], "a.pagx");

  // a.pagx references itself through its only layer.
  auto selfXML = MakeCompositionRefXML("a.pagx");
  doc->loadFileData("a.pagx", MakePAGXData(selfXML));

  // First load resolves the root layer's externalDoc; the self-reference inside it is only reached
  // on the next pass, so drive the host loop until it converges.
  int guard = 0;
  while (!doc->getExternalFilePaths().empty() && guard < 10) {
    doc->loadFileData("a.pagx", MakePAGXData(selfXML));
    guard++;
  }
  EXPECT_TRUE(doc->getExternalFilePaths().empty());

  bool hasCycleError = false;
  for (const auto& error : doc->errors) {
    if (error.find("Cyclic external composition reference detected") != std::string::npos) {
      hasCycleError = true;
    }
  }
  EXPECT_TRUE(hasCycleError);
}

/**
 * Test case: a mutual reference cycle (a.pagx -> b.pagx -> a.pagx) is detected after the chained
 * loads reach the back-reference, reported on the root document, and the external file enumeration
 * converges to empty.
 */
PAGX_TEST(PAGXTest, ExternalPAGXCompositionMutualReferenceCycle) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer composition=\"a.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  auto aXML = MakeCompositionRefXML("b.pagx");
  auto bXML = MakeCompositionRefXML("a.pagx");

  // Drive the host loop: each pass feeds whichever external file is currently requested. a -> b -> a
  // forms a cycle that is hit once the back-reference to a.pagx lands on the chain.
  int guard = 0;
  while (!doc->getExternalFilePaths().empty() && guard < 10) {
    auto paths = doc->getExternalFilePaths();
    if (paths[0] == "a.pagx") {
      doc->loadFileData("a.pagx", MakePAGXData(aXML));
    } else if (paths[0] == "b.pagx") {
      doc->loadFileData("b.pagx", MakePAGXData(bXML));
    } else {
      break;
    }
    guard++;
  }
  EXPECT_TRUE(doc->getExternalFilePaths().empty());

  bool hasCycleError = false;
  for (const auto& error : doc->errors) {
    if (error.find("Cyclic external composition reference detected") != std::string::npos) {
      hasCycleError = true;
    }
  }
  EXPECT_TRUE(hasCycleError);
}

/**
 * Test case: a legal deep nesting (a.pagx -> b.pagx -> c.pagx, no cycle) loads fully without any
 * false cycle detection, and all distinct external files resolve.
 */
PAGX_TEST(PAGXTest, ExternalPAGXCompositionDeepNestingNoFalseCycle) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer composition=\"a.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  auto aXML = MakeCompositionRefXML("b.pagx");
  auto bXML = MakeCompositionRefXML("c.pagx");
  std::string cXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"leaf\" width=\"50\" height=\"50\"/>\n"
      "</pagx>\n";

  int guard = 0;
  while (!doc->getExternalFilePaths().empty() && guard < 10) {
    auto paths = doc->getExternalFilePaths();
    if (paths[0] == "a.pagx") {
      EXPECT_TRUE(doc->loadFileData("a.pagx", MakePAGXData(aXML)));
    } else if (paths[0] == "b.pagx") {
      EXPECT_TRUE(doc->loadFileData("b.pagx", MakePAGXData(bXML)));
    } else if (paths[0] == "c.pagx") {
      EXPECT_TRUE(doc->loadFileData("c.pagx", MakePAGXData(cXML)));
    } else {
      break;
    }
    guard++;
  }
  EXPECT_TRUE(doc->getExternalFilePaths().empty());

  for (const auto& error : doc->errors) {
    EXPECT_TRUE(error.find("Cyclic external composition reference detected") == std::string::npos);
  }
}

/**
 * Test case: loading external composition data via loadFileData AFTER PAGScene::Make() sets the
 * composition on the layer and notifies existing scenes without crashing. The node-level state is
 * updated correctly.
 */
PAGX_TEST(PAGXTest, LoadFileDataExternalCompositionAfterSceneCreation) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\">\n"
      "    <Timelines>\n"
      "      <Animation ref=\"@fade\" playing=\"true\"/>\n"
      "    </Timelines>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  EXPECT_TRUE(slotLayer->composition == nullptr);
  EXPECT_TRUE(slotLayer->externalDoc == nullptr);

  auto childXML = MakeExternalCompositionXML("childLayer", "fade");
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));

  // After loading, the slot layer should have its composition resolved.
  ASSERT_TRUE(slotLayer->composition != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);
  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);

  // Composition child appears in runtime tree after loadFileData triggered notifyChange.
  auto& slotChildren = scene->rootComposition()->children[0]->children;
  ASSERT_FALSE(slotChildren.empty());

  auto hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
}

/**
 * Test case: loading image data via loadFileData AFTER PAGScene::Make() sets the Image node data
 * and notifies existing scenes without crashing. Verifies both node-level state and scene integrity.
 */
PAGX_TEST(PAGXTest, LoadFileDataImageAfterSceneCreation) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 50;
  layer->height = 50;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto img = doc->makeNode<pagx::Image>("img");
  img->filePath = "test.png";
  pattern->image = img;
  fill->color = pattern;
  layer->contents = {rect, fill};
  doc->layers = {layer};
  doc->applyLayout();

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  EXPECT_EQ(img->filePath, "test.png");
  EXPECT_TRUE(img->data == nullptr);

  uint8_t bytes[] = {1, 2, 3, 4};
  auto imageData = pagx::Data::MakeWithCopy(bytes, sizeof(bytes));
  EXPECT_TRUE(doc->loadFileData("test.png", imageData));

  // After loading, image data is set and filePath is cleared.
  EXPECT_TRUE(img->filePath.empty());
  ASSERT_TRUE(img->data != nullptr);
  EXPECT_EQ(img->data->size(), sizeof(bytes));

  // Scene structure intact after notifyChange triggered by loadFileData.
  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
}

/**
 * Test case: loadFileData is a no-op on notifyChange when called before any PAGScene exists.
 * Verifies that empty dirtyNodes or no liveScenes path is handled correctly.
 */
PAGX_TEST(PAGXTest, LoadFileDataNoSceneNotifyChangeNoOp) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto img = doc->makeNode<pagx::Image>("img");
  img->filePath = "test.png";

  uint8_t bytes[] = {1, 2, 3, 4};
  auto imageData = pagx::Data::MakeWithCopy(bytes, sizeof(bytes));
  EXPECT_TRUE(doc->loadFileData("test.png", imageData));

  EXPECT_TRUE(img->filePath.empty());
  ASSERT_TRUE(img->data != nullptr);
}

/**
 * Test case: a same-document composition that references itself through an @id is caught by the
 * runtime cycle guard in PAGComposition::MakeChild. The external-file chain guard only covers
 * compositionFilePath references, so a self-referencing in-document composition would otherwise
 * recurse without bound and overflow the stack when the scene's runtime tree is built. The guard
 * detects the repeat on the ancestor path, reports it on the document, and drops the subtree so
 * scene construction returns normally.
 */
PAGX_TEST(PAGXTest, SameDocCompositionSelfReferenceCycle) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  auto comp = doc->makeNode<pagx::Composition>("selfComp");
  comp->width = 50;
  comp->height = 50;

  // The composition's own layer references the same composition, forming an @selfComp -> @selfComp
  // cycle entirely within this document.
  auto inner = doc->makeNode<pagx::Layer>("inner");
  inner->width = 50;
  inner->height = 50;
  inner->composition = comp;
  comp->layers.push_back(inner);

  // A top-level slot layer instantiates the cyclic composition.
  auto slot = doc->makeNode<pagx::Layer>("slot");
  slot->width = 50;
  slot->height = 50;
  slot->composition = comp;
  doc->layers.push_back(slot);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  // The slot resolves into one child composition; its self-reference is dropped rather than
  // recursing, so the tree is finite.
  ASSERT_EQ(file->rootComposition()->children.size(), 1u);

  bool hasCycleError = false;
  for (const auto& error : doc->errors) {
    if (error.find("Cyclic composition reference detected") != std::string::npos) {
      hasCycleError = true;
    }
  }
  EXPECT_TRUE(hasCycleError);
}

/**
 * Test case: GetInt64Attribute rejects malformed and out-of-range integer attributes instead of
 * silently accepting a truncated or clamped value. The "time" attribute on a keyframe is parsed
 * through GetInt64Attribute; trailing garbage ("60abc") and a value past INT64_MAX both report a
 * parse error and fall back to the default rather than producing a bogus time.
 */
PAGX_TEST(PAGXTest, Int64AttributeRejectsMalformedAndOutOfRange) {
  // Trailing non-numeric characters after a valid prefix must be rejected (strtoll alone would
  // accept the "60" prefix and silently ignore "abc").
  std::string trailingXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"L\" width=\"50\" height=\"50\"/>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"L\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"60abc\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto trailingDoc = pagx::PAGXImporter::FromXML(trailingXML);
  ASSERT_TRUE(trailingDoc != nullptr);
  bool hasTrailingError = false;
  for (const auto& error : trailingDoc->errors) {
    if (error.find("Invalid value '60abc'") != std::string::npos) {
      hasTrailingError = true;
    }
  }
  EXPECT_TRUE(hasTrailingError);

  // A value beyond the int64 range (errno == ERANGE) must report an out-of-range error rather than
  // returning the clamped LLONG_MAX.
  std::string overflowXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"L\" width=\"50\" height=\"50\"/>\n"
      "  <Animations>\n"
      "    <Animation id=\"a\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"L\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"99999999999999999999\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  auto overflowDoc = pagx::PAGXImporter::FromXML(overflowXML);
  ASSERT_TRUE(overflowDoc != nullptr);
  bool hasRangeError = false;
  for (const auto& error : overflowDoc->errors) {
    if (error.find("out of range") != std::string::npos) {
      hasRangeError = true;
    }
  }
  EXPECT_TRUE(hasRangeError);
}

PAGX_TEST(PAGXTest, HitTestSingleLayer) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto layer = doc->makeNode<pagx::Layer>("hitLayerId");
  layer->name = "HitLayer";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 40};
  rect->size = {100, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  auto hits = file->getLayersUnderPoint(50, 40);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "HitLayer");
  // The handle also exposes the source node's id (the "@id" used for document references).
  EXPECT_EQ(hits[0]->id(), "hitLayerId");

  auto miss = file->getLayersUnderPoint(180, 180);
  EXPECT_TRUE(miss.empty());

  // The hit handle can re-test itself against a surface point.
  EXPECT_TRUE(hits[0]->hitTestPoint(50, 40));
  EXPECT_FALSE(hits[0]->hitTestPoint(180, 180));

  // Local bounds are the layer's untransformed content extent (a 100x80 rect), in the layer's own
  // coordinate space (origin at 0,0).
  auto localBounds = hits[0]->getBounds();
  EXPECT_FLOAT_EQ(localBounds.x, 0);
  EXPECT_FLOAT_EQ(localBounds.y, 0);
  EXPECT_FLOAT_EQ(localBounds.width, 100);
  EXPECT_FLOAT_EQ(localBounds.height, 80);

  // With identity zoom/offset the surface bounds match the local bounds.
  auto globalBounds = file->getGlobalBounds(hits[0]);
  EXPECT_FLOAT_EQ(globalBounds.x, 0);
  EXPECT_FLOAT_EQ(globalBounds.y, 0);
  EXPECT_FLOAT_EQ(globalBounds.width, 100);
  EXPECT_FLOAT_EQ(globalBounds.height, 80);

  // Applying a zoom scale and content offset scales and shifts the surface bounds accordingly.
  auto* options = file->getDisplayOptions();
  options->setZoomScale(2.0f);
  options->setContentOffset(10.0f, 5.0f);
  auto zoomedBounds = file->getGlobalBounds(hits[0]);
  EXPECT_FLOAT_EQ(zoomedBounds.x, 10);
  EXPECT_FLOAT_EQ(zoomedBounds.y, 5);
  EXPECT_FLOAT_EQ(zoomedBounds.width, 2.0f * 100);
  EXPECT_FLOAT_EQ(zoomedBounds.height, 2.0f * 80);

  // getGlobalBounds on a null handle returns an empty rect.
  auto emptyBounds = file->getGlobalBounds(nullptr);
  EXPECT_FLOAT_EQ(emptyBounds.width, 0);
  EXPECT_FLOAT_EQ(emptyBounds.height, 0);
}

/**
 * Test case: hitTest returns nullptr when the point lies outside every layer in the file.
 */
PAGX_TEST(PAGXTest, HitTestMiss) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "OnlyLayer";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {25, 25};
  rect->size = {50, 50};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 1, 0, 1};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  EXPECT_TRUE(file->getLayersUnderPoint(150, 150).empty());
  EXPECT_TRUE(file->getLayersUnderPoint(-10, -10).empty());
}

/**
 * Test case: hitTest over a layer that references a Composition resolves to the nested child
 * layer that the point falls on, by probing the child composition's reverse map.
 */
PAGX_TEST(PAGXTest, HitTestNestedComposition) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto comp = doc->makeNode<pagx::Composition>("comp");
  comp->width = 100;
  comp->height = 100;
  auto childLayer = doc->makeNode<pagx::Layer>();
  childLayer->name = "NestedChild";
  auto childRect = doc->makeNode<pagx::Rectangle>();
  childRect->position = {50, 50};
  childRect->size = {100, 100};
  auto childFill = doc->makeNode<pagx::Fill>();
  auto childSolid = doc->makeNode<pagx::SolidColor>();
  childSolid->color = {0, 0, 1, 1};
  childFill->color = childSolid;
  childLayer->contents.push_back(childRect);
  childLayer->contents.push_back(childFill);
  comp->layers.push_back(childLayer);

  auto compLayer = doc->makeNode<pagx::Layer>();
  compLayer->name = "CompLayer";
  compLayer->composition = comp;
  doc->layers.push_back(compLayer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(file->rootComposition()->children.size(), 1u);

  auto hits = file->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "NestedChild");
}

/**
 * Test case: hit test resolves a leaf layer nested under a container layer that has no composition
 * reference. The container itself is not returned; only the leaf is.
 */
PAGX_TEST(PAGXTest, HitTestNestedPureContainer) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto container = doc->makeNode<pagx::Layer>();
  container->name = "Container";
  container->width = 200;
  container->height = 200;

  auto leaf = doc->makeNode<pagx::Layer>("leafId");
  leaf->name = "Leaf";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 1, 0, 1};
  fill->color = solid;
  leaf->contents.push_back(rect);
  leaf->contents.push_back(fill);

  container->children = {leaf};
  doc->layers.push_back(container);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto hits = scene->getLayersUnderPoint(70, 70);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "Leaf");
  EXPECT_EQ(hits[0]->id(), "leafId");

  auto miss = scene->getLayersUnderPoint(180, 180);
  EXPECT_TRUE(miss.empty());
}

/**
 * Test case: when two layers reference the same Composition, each builds an independent runtime
 * composition instance with its own reverse map. Both instances are present and a hit resolves the
 * shared child through the composition subtree, confirming per-instance reverse maps are built and
 * probed rather than a single shared map.
 */
PAGX_TEST(PAGXTest, HitTestSharedCompositionPerInstance) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto comp = doc->makeNode<pagx::Composition>("shared");
  comp->width = 100;
  comp->height = 100;
  auto childLayer = doc->makeNode<pagx::Layer>();
  childLayer->name = "SharedChild";
  auto childRect = doc->makeNode<pagx::Rectangle>();
  childRect->position = {0, 0};
  childRect->size = {100, 100};
  auto childFill = doc->makeNode<pagx::Fill>();
  auto childSolid = doc->makeNode<pagx::SolidColor>();
  childSolid->color = {0, 0, 1, 1};
  childFill->color = childSolid;
  childLayer->contents.push_back(childRect);
  childLayer->contents.push_back(childFill);
  comp->layers.push_back(childLayer);

  // Two layers reference the same Composition; each gets its own runtime composition instance.
  auto compLayerA = doc->makeNode<pagx::Layer>();
  compLayerA->name = "InstanceA";
  compLayerA->composition = comp;
  doc->layers.push_back(compLayerA);

  auto compLayerB = doc->makeNode<pagx::Layer>();
  compLayerB->name = "InstanceB";
  compLayerB->composition = comp;
  doc->layers.push_back(compLayerB);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  // Two independent runtime composition instances were built from the one shared Composition.
  ASSERT_EQ(file->rootComposition()->children.size(), 2u);

  // Each instance has its own reverse map keyed by its own tgfx layers, but resolves to the same
  // shared source child node.
  auto& bindingA =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[0].get())->binding;
  auto& bindingB =
      *static_cast<pagx::PAGComposition*>(file->rootComposition()->children[1].get())->binding;
  auto tgfxChildA = bindingA.get<tgfx::Layer>(childLayer);
  auto tgfxChildB = bindingB.get<tgfx::Layer>(childLayer);
  ASSERT_TRUE(tgfxChildA != nullptr);
  ASSERT_TRUE(tgfxChildB != nullptr);
  // Per-instance isolation: the shared source layer maps to distinct tgfx layers per instance.
  EXPECT_NE(tgfxChildA.get(), tgfxChildB.get());
}

/**
 * Test case: PAGLayer::getGlobalMatrix maps the layer's local space to surface space, combining the
 * layer's position in the tree with the display list's zoomScale and contentOffset.
 */
PAGX_TEST(PAGXTest, HitTestGlobalMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->name = "PosLayer";
  layer->x = 30;
  layer->y = 20;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {40, 40};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);

  // Apply a zoomScale and contentOffset so the matrix has non-trivial surface terms.
  auto* options = file->getDisplayOptions();
  options->setZoomScale(2.0f);
  options->setContentOffset(10.0f, 5.0f);

  auto hits = file->getLayersUnderPoint(2.0f * 30 + 10, 2.0f * 20 + 5);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "PosLayer");

  // local (0,0) maps to surface = zoomScale * (layerPos) + contentOffset.
  // layer translate is (30, 20); surface tx = 2*30 + 10 = 70, ty = 2*20 + 5 = 45; scale = 2.
  auto matrix = hits[0]->getGlobalMatrix();
  EXPECT_FLOAT_EQ(matrix.a, 2.0f);
  EXPECT_FLOAT_EQ(matrix.d, 2.0f);
  EXPECT_FLOAT_EQ(matrix.tx, 70.0f);
  EXPECT_FLOAT_EQ(matrix.ty, 45.0f);
}

static pagx::Layer* MakeTestLayer(pagx::PAGXDocument* doc, float x, float y, float fillR,
                                  float fillG, float fillB) {
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Translate(x, y);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {fillR, fillG, fillB, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  return layer;
}

static pagx::Layer* MakeTestLayerSimple(pagx::PAGXDocument* doc, float x, float y) {
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Translate(x, y);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.8f, 0.8f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  return layer;
}

static pagx::NoiseFilter* MakeMonoNoiseFilter(pagx::PAGXDocument* doc, float density) {
  auto noise = doc->makeNode<pagx::NoiseFilter>();
  noise->mode = pagx::NoiseMode::Mono;
  noise->size = 10;
  noise->density = density;
  noise->seed = 42;
  noise->color = {0.0f, 0.0f, 0.0f, 1.0f};
  return noise;
}

static pagx::NoiseFilter* MakeDuoNoiseFilter(pagx::PAGXDocument* doc, float density) {
  auto noise = doc->makeNode<pagx::NoiseFilter>();
  noise->mode = pagx::NoiseMode::Duo;
  noise->size = 10;
  noise->density = density;
  noise->seed = 42;
  noise->firstColor = {1.0f, 1.0f, 0.0f, 1.0f};
  noise->secondColor = {0.0f, 0.0f, 1.0f, 1.0f};
  return noise;
}

static pagx::NoiseFilter* MakeMultiNoiseFilter(pagx::PAGXDocument* doc, float density) {
  auto noise = doc->makeNode<pagx::NoiseFilter>();
  noise->mode = pagx::NoiseMode::Multi;
  noise->size = 10;
  noise->density = density;
  noise->seed = 42;
  noise->opacity = 1.0f;
  return noise;
}

static pagx::Fill* MakeSolidFill(pagx::PAGXDocument* doc, float r, float g, float b) {
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {r, g, b, 1.0f};
  fill->color = solid;
  return fill;
}

static void WriteSVGFile(const std::string& svgContent, const std::string& relativePath) {
  auto outPath = ProjectPath::Absolute(relativePath);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  file.write(svgContent.data(), static_cast<std::streamsize>(svgContent.size()));
}

/**
 * Test rendering with Mono, Duo, and Multi noise filters side by side.
 */
PAGX_TEST(PAGXTest, NoiseFilterModes) {
  constexpr int canvasW = 400;
  constexpr int canvasH = 150;
  auto doc = pagx::PAGXDocument::Make(canvasW, canvasH);

  auto layer1 = MakeTestLayer(doc.get(), 20, 0, 0.2f, 0.5f, 0.8f);
  auto mono = doc->makeNode<pagx::NoiseFilter>();
  mono->mode = pagx::NoiseMode::Mono;
  mono->size = 8;
  mono->density = 0.5f;
  mono->seed = 42;
  mono->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer1->filters.push_back(mono);
  doc->layers.push_back(layer1);

  auto layer2 = MakeTestLayer(doc.get(), 150, 0, 0.2f, 0.5f, 0.8f);
  auto duo = doc->makeNode<pagx::NoiseFilter>();
  duo->mode = pagx::NoiseMode::Duo;
  duo->size = 8;
  duo->density = 0.5f;
  duo->seed = 42;
  duo->firstColor = {1.0f, 1.0f, 0.0f, 1.0f};
  duo->secondColor = {0.0f, 0.0f, 1.0f, 1.0f};
  layer2->filters.push_back(duo);
  doc->layers.push_back(layer2);

  auto layer3 = MakeTestLayer(doc.get(), 280, 0, 0.2f, 0.5f, 0.8f);
  auto multi = doc->makeNode<pagx::NoiseFilter>();
  multi->mode = pagx::NoiseMode::Multi;
  multi->size = 8;
  multi->density = 0.5f;
  multi->seed = 42;
  multi->opacity = 1.0f;
  layer3->filters.push_back(multi);
  doc->layers.push_back(layer3);

  doc->applyLayout();
  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, canvasW, canvasH);
  ASSERT_TRUE(surface != nullptr);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);

  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NoiseFilterModes"));

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("feTurbulence"), std::string::npos);

  WriteSVGFile(svg, "test/out/PAGXTest/NoiseFilterModes.svg");
}
/**
 * Test NoiseFilter applied to every supported element type (Rectangle, Ellipse, Path, Polystar,
 * Text, Group, TextBox) plus Repeater, outputting SVG for visual inspection of contentBounds
 * correctness.
 */
PAGX_TEST(PAGXTest, NoiseFilterAllElements) {
  constexpr int canvasW = 800;
  constexpr int canvasH = 470;
  auto doc = pagx::PAGXDocument::Make(canvasW, canvasH);
  pagx::FontConfig fontConfig;
  fontConfig.addFallbackTypefaces(GetFallbackTypefaces());

  auto typeface =
      Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSerifSC-Regular.otf"));
  ASSERT_TRUE(typeface != nullptr);
  fontConfig.registerTypeface(typeface);
  auto fontFamily = typeface->fontFamily();
  auto fontStyle = typeface->fontStyle();

  // Row 1: Rectangle(Mono,0), Ellipse(Duo,0.25), Path(Multi,0.5)
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(20, 20);

    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {60, 60};
    rect->size = {100, 100};
    layer->contents.push_back(rect);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.2f, 0.5f, 0.8f));
    layer->filters.push_back(MakeMonoNoiseFilter(doc.get(), 0.0f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(150, 20);

    auto ellipse = doc->makeNode<pagx::Ellipse>();
    ellipse->position = {60, 60};
    ellipse->size = {100, 100};
    layer->contents.push_back(ellipse);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.8f, 0.3f, 0.3f));
    layer->filters.push_back(MakeDuoNoiseFilter(doc.get(), 0.25f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(280, 20);

    auto path = doc->makeNode<pagx::Path>();
    path->data = doc->makeNode<pagx::PathData>();
    path->data->moveTo(0, 0);
    path->data->lineTo(100, 0);
    path->data->lineTo(50, 100);
    path->data->close();
    path->position = {10, 10};
    layer->contents.push_back(path);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.3f, 0.7f, 0.3f));
    layer->filters.push_back(MakeMultiNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }

  // Row 2: Polystar(Mono,0.75), Text(Duo,1), Group(Multi,0.5)
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(20, 160);

    auto polystar = doc->makeNode<pagx::Polystar>();
    polystar->position = {60, 60};
    polystar->outerRadius = 50;
    polystar->innerRadius = 25;
    polystar->pointCount = 5;
    layer->contents.push_back(polystar);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.7f, 0.5f, 0.9f));
    layer->filters.push_back(MakeMonoNoiseFilter(doc.get(), 0.75f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(150, 160);

    auto text = doc->makeNode<pagx::Text>();
    text->text = "Hi";
    text->fontFamily = fontFamily;
    text->fontStyle = fontStyle;
    text->fontSize = 60;
    layer->contents.push_back(text);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.2f, 0.6f, 0.9f));
    layer->filters.push_back(MakeDuoNoiseFilter(doc.get(), 1.0f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(280, 160);

    auto group = doc->makeNode<pagx::Group>();
    group->position = {10, 10};
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {40, 40};
    rect->size = {60, 60};
    group->elements.push_back(rect);
    group->elements.push_back(MakeSolidFill(doc.get(), 0.9f, 0.6f, 0.1f));
    layer->contents.push_back(group);
    layer->filters.push_back(MakeMultiNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }

  // Row 3: TextBox(Duo,0.5), Repeater(Multi,0.5)
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(20, 320);

    auto textBox = doc->makeNode<pagx::TextBox>();
    textBox->width = 120;
    textBox->height = 100;
    auto text = doc->makeNode<pagx::Text>();
    text->text = "AB CD";
    text->fontFamily = fontFamily;
    text->fontStyle = fontStyle;
    text->fontSize = 30;
    textBox->elements.push_back(text);
    textBox->elements.push_back(MakeSolidFill(doc.get(), 0.1f, 0.4f, 0.7f));
    layer->contents.push_back(textBox);
    layer->filters.push_back(MakeDuoNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(200, 320);

    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {30, 30};
    rect->size = {40, 40};
    auto repeater = doc->makeNode<pagx::Repeater>();
    repeater->copies = 3;
    repeater->offset = 0;
    repeater->position = {50, 0};
    layer->contents.push_back(rect);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.3f, 0.8f, 0.6f));
    layer->contents.push_back(repeater);
    layer->filters.push_back(MakeMultiNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }

  // Row 4: Off-center content (contentBounds origin != 0,0)
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(440, 20);

    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {80, 40};
    rect->size = {60, 60};
    layer->contents.push_back(rect);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.6f, 0.2f, 0.8f));
    layer->filters.push_back(MakeMonoNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }
  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->matrix = pagx::Matrix::Translate(580, 20);

    auto ellipse = doc->makeNode<pagx::Ellipse>();
    ellipse->position = {40, 80};
    ellipse->size = {60, 80};
    layer->contents.push_back(ellipse);
    layer->contents.push_back(MakeSolidFill(doc.get(), 0.8f, 0.7f, 0.2f));
    layer->filters.push_back(MakeDuoNoiseFilter(doc.get(), 0.5f));
    doc->layers.push_back(layer);
  }

  doc->applyLayout(&fontConfig);
  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, canvasW, canvasH);
  ASSERT_TRUE(surface != nullptr);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);

  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NoiseFilterAllElements"));

  pagx::SVGExportOptions svgOpts;
  svgOpts.fontConfig = &fontConfig;
  auto svg = pagx::SVGExporter::ToSVG(*doc, svgOpts);
  EXPECT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<filter"), std::string::npos);
  EXPECT_NE(svg.find("feTurbulence"), std::string::npos);

  WriteSVGFile(svg, "test/out/PAGXTest/NoiseFilterAllElements.svg");
}

/**
 * Test NoiseStyle with blendMode applied to an image layer, verifying both rendering and SVG
 * export. The blendMode is set to Multiply so the noise composites differently from Normal.
 */
PAGX_TEST(PAGXTest, NoiseStyleBlendModeOnImage) {
  constexpr int canvasW = 200;
  constexpr int canvasH = 200;
  auto doc = pagx::PAGXDocument::Make(canvasW, canvasH);

  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {200, 200};

  auto* image = doc->makeNode<pagx::Image>();
  auto imageData =
      tgfx::Data::MakeFromFile(ProjectPath::Absolute("resources/apitest/imageReplacement.png"));
  ASSERT_TRUE(imageData != nullptr);
  image->data = pagx::Data::MakeWithCopy(imageData->bytes(), imageData->size());

  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1, 0, 0, 1, 0, 0};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  auto* noise = doc->makeNode<pagx::NoiseStyle>();
  noise->mode = pagx::NoiseMode::Mono;
  noise->size = 8;
  noise->density = 1.0f;
  noise->seed = 7;
  noise->color = {0.5f, 0.5f, 0.5f, 1.0f};
  noise->blendMode = pagx::BlendMode::Multiply;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->styles.push_back(noise);
  doc->layers.push_back(layer);

  doc->applyLayout();
  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, canvasW, canvasH);
  ASSERT_TRUE(surface != nullptr);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);

  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NoiseStyleBlendModeOnImage"));

  pagx::SVGExportOptions svgOpts;
  auto svg = pagx::SVGExporter::ToSVG(*doc, svgOpts);
  EXPECT_FALSE(svg.empty());
  EXPECT_NE(svg.find("feTurbulence"), std::string::npos);
  EXPECT_NE(svg.find("<image"), std::string::npos);
  EXPECT_NE(svg.find("feBlend"), std::string::npos);
  EXPECT_NE(svg.find("mode=\"multiply\""), std::string::npos);

  WriteSVGFile(svg, "test/out/PAGXTest/NoiseStyleBlendModeOnImage.svg");
}

/**
 * Test rendering with Mono, Duo, and Multi noise styles side by side.
 * Covers writeNoiseStyle for all three modes.
 */
PAGX_TEST(PAGXTest, NoiseStyleModes) {
  constexpr int canvasW = 400;
  constexpr int canvasH = 180;
  auto doc = pagx::PAGXDocument::Make(canvasW, canvasH);

  auto layer1 = MakeTestLayerSimple(doc.get(), 20, 40);
  auto mono = doc->makeNode<pagx::NoiseStyle>();
  mono->mode = pagx::NoiseMode::Mono;
  mono->size = 8;
  mono->density = 0.5f;
  mono->seed = 42;
  mono->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer1->styles.push_back(mono);
  doc->layers.push_back(layer1);

  auto layer2 = MakeTestLayerSimple(doc.get(), 150, 40);
  auto duo = doc->makeNode<pagx::NoiseStyle>();
  duo->mode = pagx::NoiseMode::Duo;
  duo->size = 8;
  duo->density = 0.5f;
  duo->seed = 42;
  duo->firstColor = {1.0f, 1.0f, 0.0f, 1.0f};
  duo->secondColor = {0.0f, 0.0f, 1.0f, 1.0f};
  layer2->styles.push_back(duo);
  doc->layers.push_back(layer2);

  auto layer3 = MakeTestLayerSimple(doc.get(), 280, 40);
  auto multi = doc->makeNode<pagx::NoiseStyle>();
  multi->mode = pagx::NoiseMode::Multi;
  multi->size = 8;
  multi->density = 0.5f;
  multi->seed = 42;
  multi->opacity = 1.0f;
  layer3->styles.push_back(multi);
  doc->layers.push_back(layer3);

  doc->applyLayout();
  auto tgfxLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto surface = Surface::Make(context, canvasW, canvasH);
  ASSERT_TRUE(surface != nullptr);
  DisplayList displayList;
  displayList.root()->addChild(tgfxLayer);
  displayList.render(surface.get(), false);

  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NoiseStyleModes"));

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("feTurbulence"), std::string::npos);

  WriteSVGFile(svg, "test/out/PAGXTest/NoiseStyleModes.svg");
}

/**
 * Test animation channel binding for NoiseFilter (Mono/Duo/Multi modes).
 */
PAGX_TEST(PAGXTest, ChannelNoiseFilter) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 100;
  layer->height = 100;
  doc->layers.push_back(layer);

  auto monoFilter = doc->makeNode<pagx::NoiseFilter>("NF_MONO");
  monoFilter->mode = pagx::NoiseMode::Mono;
  monoFilter->size = 4;
  monoFilter->density = 0.2f;
  monoFilter->seed = 10;
  monoFilter->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer->filters.push_back(monoFilter);

  auto duoFilter = doc->makeNode<pagx::NoiseFilter>("NF_DUO");
  duoFilter->mode = pagx::NoiseMode::Duo;
  duoFilter->size = 6;
  duoFilter->density = 0.3f;
  duoFilter->seed = 20;
  duoFilter->firstColor = {1.0f, 0.0f, 0.0f, 1.0f};
  duoFilter->secondColor = {0.0f, 0.0f, 1.0f, 1.0f};
  layer->filters.push_back(duoFilter);

  auto multiFilter = doc->makeNode<pagx::NoiseFilter>("NF_MULTI");
  multiFilter->mode = pagx::NoiseMode::Multi;
  multiFilter->size = 8;
  multiFilter->density = 0.4f;
  multiFilter->seed = 30;
  multiFilter->opacity = 0.5f;
  layer->filters.push_back(multiFilter);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);

  // Mono filter channels
  auto* monoObj = doc->makeNode<pagx::AnimationObject>();
  monoObj->target = "NF_MONO";
  anim->objects.push_back(monoObj);
  auto* monoSize = doc->makeNode<pagx::TypedChannel<float>>();
  monoSize->name = "size";
  monoSize->keyframes.push_back({0, 20.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoSize);
  auto* monoDensity = doc->makeNode<pagx::TypedChannel<float>>();
  monoDensity->name = "density";
  monoDensity->keyframes.push_back({0, 0.8f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoDensity);
  auto* monoSeed = doc->makeNode<pagx::TypedChannel<float>>();
  monoSeed->name = "seed";
  monoSeed->keyframes.push_back({0, 100.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoSeed);
  auto* monoColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  monoColor->name = "color";
  monoColor->keyframes.push_back(
      {0, pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoColor);

  // Duo filter channels
  auto* duoObj = doc->makeNode<pagx::AnimationObject>();
  duoObj->target = "NF_DUO";
  anim->objects.push_back(duoObj);
  auto* duoFirstColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  duoFirstColor->name = "firstColor";
  duoFirstColor->keyframes.push_back(
      {0, pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  duoObj->channels.push_back(duoFirstColor);
  auto* duoSecondColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  duoSecondColor->name = "secondColor";
  duoSecondColor->keyframes.push_back(
      {0, pagx::Color{1.0f, 1.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  duoObj->channels.push_back(duoSecondColor);

  // Multi filter channels
  auto* multiObj = doc->makeNode<pagx::AnimationObject>();
  multiObj->target = "NF_MULTI";
  anim->objects.push_back(multiObj);
  auto* multiOpacity = doc->makeNode<pagx::TypedChannel<float>>();
  multiOpacity->name = "opacity";
  multiOpacity->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  multiObj->channels.push_back(multiOpacity);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto& tree = *file->mutableBinding();

  auto tgfxMono = tree.get<tgfx::NoiseFilter>(monoFilter);
  ASSERT_TRUE(tgfxMono != nullptr);
  auto tgfxDuo = tree.get<tgfx::NoiseFilter>(duoFilter);
  ASSERT_TRUE(tgfxDuo != nullptr);
  auto tgfxMulti = tree.get<tgfx::NoiseFilter>(multiFilter);
  ASSERT_TRUE(tgfxMulti != nullptr);

  EXPECT_FALSE(tree.apply(monoFilter, "firstColor",
                          pagx::KeyValue(pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(monoFilter, "opacity", pagx::KeyValue(1.0f), 1.0f));
  EXPECT_FALSE(
      tree.apply(duoFilter, "color", pagx::KeyValue(pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(duoFilter, "opacity", pagx::KeyValue(1.0f), 1.0f));
  EXPECT_FALSE(
      tree.apply(multiFilter, "color", pagx::KeyValue(pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(multiFilter, "secondColor",
                          pagx::KeyValue(pagx::Color{1.0f, 1.0f, 1.0f, 1.0f}), 1.0f));

  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);

  // Apply at mix=1.0: values fully overwritten by keyframe targets.
  timeline->apply(1.0f);

  // Mono: size 4→20, density 0.2→0.8, seed 10→100, color black→red
  EXPECT_FLOAT_EQ(tgfxMono->size(), 20.0f);
  EXPECT_FLOAT_EQ(tgfxMono->density(), 0.8f);
  EXPECT_FLOAT_EQ(tgfxMono->seed(), 100.0f);
  auto monoNoise = std::static_pointer_cast<tgfx::MonoNoiseFilter>(tgfxMono);
  EXPECT_FLOAT_EQ(monoNoise->color().red, 1.0f);
  EXPECT_FLOAT_EQ(monoNoise->color().green, 0.0f);

  // Duo: firstColor red→green, secondColor blue→yellow
  auto duoNoise = std::static_pointer_cast<tgfx::DuoNoiseFilter>(tgfxDuo);
  EXPECT_FLOAT_EQ(duoNoise->firstColor().green, 1.0f);
  EXPECT_FLOAT_EQ(duoNoise->firstColor().red, 0.0f);
  EXPECT_FLOAT_EQ(duoNoise->secondColor().red, 1.0f);
  EXPECT_FLOAT_EQ(duoNoise->secondColor().green, 1.0f);

  // Multi: opacity 0.5→1.0
  auto multiNoise = std::static_pointer_cast<tgfx::MultiNoiseFilter>(tgfxMulti);
  EXPECT_FLOAT_EQ(multiNoise->opacity(), 1.0f);

  // Apply at mix=0.5: interpolate from initial toward keyframe.
  // Reset initial values by re-building.
  auto file2 = pagx::PAGScene::Make(doc);
  auto& tree2 = *file2->mutableBinding();
  auto tgfxMono2 = tree2.get<tgfx::NoiseFilter>(monoFilter);
  auto timeline2 = file2->getDefaultTimeline();
  timeline2->apply(0.5f);

  // size: 4 + (20-4)*0.5 = 12
  EXPECT_FLOAT_EQ(tgfxMono2->size(), 12.0f);
  // density: 0.2 + (0.8-0.2)*0.5 = 0.5
  EXPECT_FLOAT_EQ(tgfxMono2->density(), 0.5f);
  // seed: 10 + (100-10)*0.5 = 55
  EXPECT_FLOAT_EQ(tgfxMono2->seed(), 55.0f);
}

/**
 * Test animation channel binding for NoiseStyle (Mono/Duo/Multi modes).
 */
PAGX_TEST(PAGXTest, ChannelNoiseStyle) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 100;
  layer->height = 100;
  doc->layers.push_back(layer);

  auto monoStyle = doc->makeNode<pagx::NoiseStyle>("NS_MONO");
  monoStyle->mode = pagx::NoiseMode::Mono;
  monoStyle->size = 4;
  monoStyle->density = 0.2f;
  monoStyle->seed = 10;
  monoStyle->color = {0.0f, 0.0f, 0.0f, 1.0f};
  layer->styles.push_back(monoStyle);

  auto duoStyle = doc->makeNode<pagx::NoiseStyle>("NS_DUO");
  duoStyle->mode = pagx::NoiseMode::Duo;
  duoStyle->size = 6;
  duoStyle->density = 0.3f;
  duoStyle->seed = 20;
  duoStyle->firstColor = {1.0f, 0.0f, 0.0f, 1.0f};
  duoStyle->secondColor = {0.0f, 0.0f, 1.0f, 1.0f};
  layer->styles.push_back(duoStyle);

  auto multiStyle = doc->makeNode<pagx::NoiseStyle>("NS_MULTI");
  multiStyle->mode = pagx::NoiseMode::Multi;
  multiStyle->size = 8;
  multiStyle->density = 0.4f;
  multiStyle->seed = 30;
  multiStyle->opacity = 0.5f;
  layer->styles.push_back(multiStyle);

  auto anim = doc->makeNode<pagx::Animation>("a");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);

  // Mono style channels
  auto* monoObj = doc->makeNode<pagx::AnimationObject>();
  monoObj->target = "NS_MONO";
  anim->objects.push_back(monoObj);
  auto* monoSize = doc->makeNode<pagx::TypedChannel<float>>();
  monoSize->name = "size";
  monoSize->keyframes.push_back({0, 16.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoSize);
  auto* monoDensity = doc->makeNode<pagx::TypedChannel<float>>();
  monoDensity->name = "density";
  monoDensity->keyframes.push_back({0, 0.9f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoDensity);
  auto* monoSeed = doc->makeNode<pagx::TypedChannel<float>>();
  monoSeed->name = "seed";
  monoSeed->keyframes.push_back({0, 80.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoSeed);
  auto* monoColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  monoColor->name = "color";
  monoColor->keyframes.push_back(
      {0, pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  monoObj->channels.push_back(monoColor);

  // Duo style channels
  auto* duoObj = doc->makeNode<pagx::AnimationObject>();
  duoObj->target = "NS_DUO";
  anim->objects.push_back(duoObj);
  auto* duoFirstColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  duoFirstColor->name = "firstColor";
  duoFirstColor->keyframes.push_back(
      {0, pagx::Color{0.0f, 0.0f, 1.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  duoObj->channels.push_back(duoFirstColor);
  auto* duoSecondColor = doc->makeNode<pagx::TypedChannel<pagx::Color>>();
  duoSecondColor->name = "secondColor";
  duoSecondColor->keyframes.push_back(
      {0, pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}, pagx::KeyframeInterpolationType::Hold, {}, {}});
  duoObj->channels.push_back(duoSecondColor);

  // Multi style channels
  auto* multiObj = doc->makeNode<pagx::AnimationObject>();
  multiObj->target = "NS_MULTI";
  anim->objects.push_back(multiObj);
  auto* multiOpacity = doc->makeNode<pagx::TypedChannel<float>>();
  multiOpacity->name = "opacity";
  multiOpacity->keyframes.push_back({0, 1.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  multiObj->channels.push_back(multiOpacity);

  auto file = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(file != nullptr);
  auto& tree = *file->mutableBinding();

  auto tgfxMono = tree.get<tgfx::NoiseStyle>(monoStyle);
  ASSERT_TRUE(tgfxMono != nullptr);
  auto tgfxDuo = tree.get<tgfx::NoiseStyle>(duoStyle);
  ASSERT_TRUE(tgfxDuo != nullptr);
  auto tgfxMulti = tree.get<tgfx::NoiseStyle>(multiStyle);
  ASSERT_TRUE(tgfxMulti != nullptr);

  EXPECT_FALSE(tree.apply(monoStyle, "firstColor",
                          pagx::KeyValue(pagx::Color{0.0f, 1.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(monoStyle, "opacity", pagx::KeyValue(1.0f), 1.0f));
  EXPECT_FALSE(
      tree.apply(duoStyle, "color", pagx::KeyValue(pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(duoStyle, "opacity", pagx::KeyValue(1.0f), 1.0f));
  EXPECT_FALSE(
      tree.apply(multiStyle, "color", pagx::KeyValue(pagx::Color{1.0f, 0.0f, 0.0f, 1.0f}), 1.0f));
  EXPECT_FALSE(tree.apply(multiStyle, "secondColor",
                          pagx::KeyValue(pagx::Color{1.0f, 1.0f, 1.0f, 1.0f}), 1.0f));

  auto timeline = file->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);

  // Apply at mix=1.0
  timeline->apply(1.0f);

  // Mono: size 4→16, density 0.2→0.9, seed 10→80, color black→green
  EXPECT_FLOAT_EQ(tgfxMono->size(), 16.0f);
  EXPECT_FLOAT_EQ(tgfxMono->density(), 0.9f);
  EXPECT_FLOAT_EQ(tgfxMono->seed(), 80.0f);
  auto monoNoise = std::static_pointer_cast<tgfx::MonoNoiseStyle>(tgfxMono);
  EXPECT_FLOAT_EQ(monoNoise->color().green, 1.0f);
  EXPECT_FLOAT_EQ(monoNoise->color().red, 0.0f);

  // Duo: firstColor red→blue, secondColor blue→green
  auto duoNoise = std::static_pointer_cast<tgfx::DuoNoiseStyle>(tgfxDuo);
  EXPECT_FLOAT_EQ(duoNoise->firstColor().blue, 1.0f);
  EXPECT_FLOAT_EQ(duoNoise->firstColor().red, 0.0f);
  EXPECT_FLOAT_EQ(duoNoise->secondColor().green, 1.0f);
  EXPECT_FLOAT_EQ(duoNoise->secondColor().blue, 0.0f);

  // Multi: opacity 0.5→1.0
  auto multiNoise = std::static_pointer_cast<tgfx::MultiNoiseStyle>(tgfxMulti);
  EXPECT_FLOAT_EQ(multiNoise->opacity(), 1.0f);

  // Apply at mix=0.5: interpolate from initial values.
  auto file2 = pagx::PAGScene::Make(doc);
  auto& tree2 = *file2->mutableBinding();
  auto tgfxMono2 = tree2.get<tgfx::NoiseStyle>(monoStyle);
  auto timeline2 = file2->getDefaultTimeline();
  timeline2->apply(0.5f);

  // size: 4 + (16-4)*0.5 = 10
  EXPECT_FLOAT_EQ(tgfxMono2->size(), 10.0f);
  // density: 0.2 + (0.9-0.2)*0.5 = 0.55
  EXPECT_FLOAT_EQ(tgfxMono2->density(), 0.55f);
  // seed: 10 + (80-10)*0.5 = 45
  EXPECT_FLOAT_EQ(tgfxMono2->seed(), 45.0f);
}

/**
 * Render NoiseFilter/NoiseStyle animation frames across all modes and combined with shadows.
 * Layout: two rectangles side-by-side, left with NoiseFilter, right with NoiseStyle.
 * 12 frames total: 3 Mono, 3 Duo, 3 Multi, 3 Multi+DropShadowFilter+InnerShadowStyle.
 * Density animates from 0.1 to 1.0 across all 12 frames.
 * The baseline key is "NoiseFilterAnimation" (without the "Export" prefix) to avoid
 * re-accepting baselines after renaming the test.
 */
PAGX_TEST(PAGXTest, ExportNoiseFilterAnimation) {
  constexpr int canvasW = 500;
  constexpr int canvasH = 260;
  constexpr int totalFrames = 12;
  constexpr int framesPerSection = 3;

  auto outDir = ProjectPath::Absolute("test/out/PAGXTest/NoiseFilterAnimation");
  auto dirPath = std::filesystem::path(outDir);
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }

  auto surface = Surface::Make(context, canvasW, canvasH);
  ASSERT_TRUE(surface != nullptr);

  for (int i = 0; i < totalFrames; i++) {
    float density = 0.1f + 0.9f * (static_cast<float>(i) / (totalFrames - 1));
    int section = i / framesPerSection;

    auto doc = pagx::PAGXDocument::Make(canvasW, canvasH);

    // Left rectangle with NoiseFilter
    auto layerL = doc->makeNode<pagx::Layer>("LL");
    layerL->width = 220;
    layerL->height = 220;
    layerL->matrix = pagx::Matrix::Translate(10, 20);
    auto rectL = doc->makeNode<pagx::Rectangle>();
    rectL->position = {110, 110};
    rectL->size = {220, 220};
    rectL->roundness = 12;
    auto fillL = doc->makeNode<pagx::Fill>();
    auto solidL = doc->makeNode<pagx::SolidColor>();
    solidL->color = {0.9f, 0.9f, 0.9f, 1.0f};
    fillL->color = solidL;
    layerL->contents.push_back(rectL);
    layerL->contents.push_back(fillL);

    auto noiseFilter = doc->makeNode<pagx::NoiseFilter>("NF");
    noiseFilter->size = 8;
    noiseFilter->density = density;
    noiseFilter->seed = 42;
    switch (section) {
      case 0:
        noiseFilter->mode = pagx::NoiseMode::Mono;
        noiseFilter->color = {0.0f, 0.0f, 0.0f, 1.0f};
        break;
      case 1:
        noiseFilter->mode = pagx::NoiseMode::Duo;
        noiseFilter->firstColor = {1.0f, 0.2f, 0.0f, 1.0f};
        noiseFilter->secondColor = {0.0f, 0.2f, 1.0f, 1.0f};
        break;
      default:
        noiseFilter->mode = pagx::NoiseMode::Multi;
        noiseFilter->opacity = 0.8f;
        break;
    }
    layerL->filters.push_back(noiseFilter);

    if (section == 3) {
      auto shadowL = doc->makeNode<pagx::DropShadowFilter>();
      shadowL->offsetX = 4;
      shadowL->offsetY = 4;
      shadowL->blurX = 6;
      shadowL->blurY = 6;
      shadowL->color = {0.0f, 0.0f, 0.0f, 0.6f};
      layerL->filters.push_back(shadowL);

      auto innerShadowL = doc->makeNode<pagx::InnerShadowStyle>();
      innerShadowL->offsetX = 3;
      innerShadowL->offsetY = 3;
      innerShadowL->blurX = 8;
      innerShadowL->blurY = 8;
      innerShadowL->color = {0.0f, 0.0f, 0.0f, 0.5f};
      layerL->styles.push_back(innerShadowL);
    }

    doc->layers.push_back(layerL);

    // Right rectangle with NoiseStyle
    auto layerR = doc->makeNode<pagx::Layer>("LR");
    layerR->width = 220;
    layerR->height = 220;
    layerR->matrix = pagx::Matrix::Translate(270, 20);
    auto rectR = doc->makeNode<pagx::Rectangle>();
    rectR->position = {110, 110};
    rectR->size = {220, 220};
    rectR->roundness = 12;
    auto fillR = doc->makeNode<pagx::Fill>();
    auto solidR = doc->makeNode<pagx::SolidColor>();
    solidR->color = {0.9f, 0.9f, 0.9f, 1.0f};
    fillR->color = solidR;
    layerR->contents.push_back(rectR);
    layerR->contents.push_back(fillR);

    auto noiseStyle = doc->makeNode<pagx::NoiseStyle>("NS");
    noiseStyle->size = 8;
    noiseStyle->density = density;
    noiseStyle->seed = 42;
    switch (section) {
      case 0:
        noiseStyle->mode = pagx::NoiseMode::Mono;
        noiseStyle->color = {0.0f, 0.0f, 0.0f, 1.0f};
        break;
      case 1:
        noiseStyle->mode = pagx::NoiseMode::Duo;
        noiseStyle->firstColor = {1.0f, 0.2f, 0.0f, 1.0f};
        noiseStyle->secondColor = {0.0f, 0.2f, 1.0f, 1.0f};
        break;
      default:
        noiseStyle->mode = pagx::NoiseMode::Multi;
        noiseStyle->opacity = 0.8f;
        break;
    }
    layerR->styles.push_back(noiseStyle);

    if (section == 3) {
      auto shadowR = doc->makeNode<pagx::DropShadowFilter>();
      shadowR->offsetX = 4;
      shadowR->offsetY = 4;
      shadowR->blurX = 6;
      shadowR->blurY = 6;
      shadowR->color = {0.0f, 0.0f, 0.0f, 0.6f};
      layerR->filters.push_back(shadowR);

      auto innerShadowR = doc->makeNode<pagx::InnerShadowStyle>();
      innerShadowR->offsetX = 3;
      innerShadowR->offsetY = 3;
      innerShadowR->blurX = 8;
      innerShadowR->blurY = 8;
      innerShadowR->color = {0.0f, 0.0f, 0.0f, 0.5f};
      layerR->styles.push_back(innerShadowR);
    }

    doc->layers.push_back(layerR);

    doc->applyLayout();
    auto tgfxRoot = pagx::LayerBuilder::Build(doc.get());
    ASSERT_TRUE(tgfxRoot != nullptr);

    tgfx::DisplayList displayList;
    displayList.root()->addChild(tgfxRoot);
    displayList.render(surface.get(), true);

    auto key = "PAGXTest/NoiseFilterAnimation/frame_" + std::to_string(i);
    EXPECT_TRUE(Baseline::Compare(surface, key));
  }
}
/**
 * Test case: notifyChange reflects a render-attribute edit (alpha) on the live tgfx layer in place,
 * preserving the existing tgfx::Layer instance so handles stay valid.
 */
PAGX_TEST(PAGXTest, NotifyChangeRenderAttributeInPlace) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  layer->alpha = 1.0f;
  doc->layers.push_back(layer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 1.0f);

  layer->alpha = 0.3f;
  doc->notifyChange({layer}, true);

  // Same tgfx::Layer instance is reused (in place), and the new alpha is reflected.
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(layer).get(), tgfxLayer.get());
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
}

/**
 * Test case: notifyChange regenerates vector contents so a SolidColor edit is reflected after the
 * refresh.
 */
PAGX_TEST(PAGXTest, NotifyChangeVectorContentColor) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxSolid = scene->mutableBinding()->get<tgfx::SolidColor>(solid);
  ASSERT_TRUE(tgfxSolid != nullptr);
  EXPECT_EQ(tgfxSolid->color(), tgfx::Color({1.0f, 0.0f, 0.0f, 1.0f}));

  solid->color = {0.0f, 1.0f, 0.0f, 1.0f};
  doc->notifyChange({layer}, true);

  // Contents are regenerated, so the binding now points at a fresh tgfx SolidColor with the edit.
  auto refreshedSolid = scene->mutableBinding()->get<tgfx::SolidColor>(solid);
  ASSERT_TRUE(refreshedSolid != nullptr);
  EXPECT_EQ(refreshedSolid->color(), tgfx::Color({0.0f, 1.0f, 0.0f, 1.0f}));
}

/**
 * Test case: notifyChange promotes a layer that was built empty (a plain tgfx::Layer) to a
 * VectorLayer once it gains contents, so the added content renders. The owning PAGLayer's
 * runtimeLayer is re-synced to the new instance and existing nested children are preserved.
 */
PAGX_TEST(PAGXTest, NotifyChangeEmptyLayerGainsContents) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);
  // A nested child layer so we can verify it survives the promotion.
  auto child = doc->makeNode<pagx::Layer>("C");
  child->width = 10;
  child->height = 10;
  layer->children.push_back(child);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  // All plain layers are built as VectorLayer from the start — they can hold contents
  // without promotion. Verify the layer is a VectorLayer, content binding works, and the
  // nested child layer instance survives the refresh.
  auto built = binding->get<tgfx::Layer>(layer);
  ASSERT_TRUE(built != nullptr);
  EXPECT_EQ(built->type(), tgfx::LayerType::Vector);
  auto childBuilt = binding->get<tgfx::Layer>(child);
  ASSERT_TRUE(childBuilt != nullptr);

  // Add a rectangle + fill, then notify. The layer must be promoted to a VectorLayer.
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(fill);
  doc->notifyChange({layer}, true);

  // The node is now bound to a VectorLayer, the added content is bound, and the nested child layer
  // instance is preserved (its handle stays valid).
  auto promoted = binding->get<tgfx::Layer>(layer);
  ASSERT_TRUE(promoted != nullptr);
  EXPECT_EQ(promoted->type(), tgfx::LayerType::Vector);
  EXPECT_TRUE(binding->get<tgfx::SolidColor>(solid) != nullptr);
  EXPECT_EQ(binding->get<tgfx::Layer>(child).get(), childBuilt.get());
}

/**
 * Test case: a composition child's PAGLayer keeps its subtree-root runtimeLayer across a
 * notifyChange and stays hit-testable. refreshNodes re-syncs the cached runtimeLayer only for plain
 * children; a composition child's binding entry is the empty slot, not its subtree root, so it must
 * be skipped or the hit-test would resolve to the slot instead of the nested child.
 */
PAGX_TEST(PAGXTest, NotifyChangeKeepsCompositionChildHitTestable) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  ASSERT_TRUE(doc != nullptr);

  auto comp = doc->makeNode<pagx::Composition>("comp");
  comp->width = 100;
  comp->height = 100;
  auto childLayer = doc->makeNode<pagx::Layer>();
  childLayer->name = "NestedChild";
  auto childRect = doc->makeNode<pagx::Rectangle>();
  childRect->position = {50, 50};
  childRect->size = {100, 100};
  auto childFill = doc->makeNode<pagx::Fill>();
  auto childSolid = doc->makeNode<pagx::SolidColor>();
  childSolid->color = {0, 0, 1, 1};
  childFill->color = childSolid;
  childLayer->contents.push_back(childRect);
  childLayer->contents.push_back(childFill);
  comp->layers.push_back(childLayer);

  auto compLayer = doc->makeNode<pagx::Layer>();
  compLayer->name = "CompLayer";
  compLayer->composition = comp;
  doc->layers.push_back(compLayer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "NestedChild");

  // Mark the top-level composition child dirty so refreshNodes runs its runtimeLayer re-sync loop.
  doc->notifyChange({compLayer}, true);

  // The composition child's runtimeLayer was not overwritten with its slot, so the nested child is
  // still resolved by the hit-test.
  auto hitsAfter = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hitsAfter.empty());
  EXPECT_EQ(hitsAfter[0]->name(), "NestedChild");
}

/**
 * Test case: notifyChange re-runs layout so a geometry edit is reflected in the layer's content
 * bounds, while keeping the layer handle valid (the tgfx::Layer instance is preserved).
 */
PAGX_TEST(PAGXTest, NotifyChangeLayoutWidth) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {50, 50};
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto hits = scene->getLayersUnderPoint(10, 10);
  ASSERT_FALSE(hits.empty());
  EXPECT_FLOAT_EQ(hits[0]->getBounds().width, 50);

  rect->size = {120, 50};
  doc->notifyChange({layer}, true);

  // The same tgfx::Layer instance is kept and the regenerated content reflects the new width.
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(layer).get(), tgfxLayer.get());
  auto hitsAfter = scene->getLayersUnderPoint(10, 10);
  ASSERT_FALSE(hitsAfter.empty());
  EXPECT_FLOAT_EQ(hitsAfter[0]->getBounds().width, 120);
}

/**
 * Test case: a non-timeline edit (a plain layer attribute) does NOT disturb timelines — the
 * timeline keeps its resolved cache and in-progress playback. Only timeline-node edits reset
 * timelines.
 */
PAGX_TEST(PAGXTest, NotifyChangeKeepsTimelineWhenNoTimelineNodeDirty) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* alphaChannel = doc->makeNode<pagx::TypedChannel<float>>();
  alphaChannel->name = "alpha";
  alphaChannel->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(alphaChannel);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  timeline->apply(1.0f);
  EXPECT_TRUE(timeline->resolved);

  // A plain layer edit is not a timeline node, so the timeline is left untouched (cache preserved).
  doc->notifyChange({layer}, true);
  EXPECT_TRUE(timeline->resolved);

  // Playback still drives the channel correctly.
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  tgfxLayer->setAlpha(1.0f);
  timeline->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.5f);
}

/**
 * Test case: editing a timeline node (a Channel keyframe) resets the scene's timelines so the new
 * keyframe value takes effect on the next apply.
 */
PAGX_TEST(PAGXTest, NotifyChangeResetsTimelinesOnChannelEdit) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* alphaChannel = doc->makeNode<pagx::TypedChannel<float>>();
  alphaChannel->name = "alpha";
  alphaChannel->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(alphaChannel);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  scene->getDefaultTimeline()->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.5f);

  // Edit the keyframe value and mark the Channel node dirty: timelines are rebuilt.
  alphaChannel->keyframes[0].value = 0.25f;
  doc->notifyChange({alphaChannel}, true);

  // A freshly rebuilt timeline applies the new value.
  auto rebuilt = scene->getDefaultTimeline();
  ASSERT_TRUE(rebuilt != nullptr);
  tgfxLayer->setAlpha(1.0f);
  rebuilt->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.25f);
}

/**
 * Test case: a reference edit (repointing AnimationObject.target to a different node) takes effect
 * when the caller marks the referencing timeline node dirty, since the timeline is rebuilt and
 * re-resolves its targets. Demonstrates the "mark every node on the reference chain dirty" rule.
 */
PAGX_TEST(PAGXTest, NotifyChangeRetargetAnimationObject) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layerA = doc->makeNode<pagx::Layer>("A");
  layerA->width = 50;
  layerA->height = 50;
  doc->layers.push_back(layerA);
  auto layerB = doc->makeNode<pagx::Layer>("B");
  layerB->width = 50;
  layerB->height = 50;
  doc->layers.push_back(layerB);
  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "A";
  anim->objects.push_back(object);
  auto* alphaChannel = doc->makeNode<pagx::TypedChannel<float>>();
  alphaChannel->name = "alpha";
  alphaChannel->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(alphaChannel);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  auto tgfxA = binding->get<tgfx::Layer>(layerA);
  auto tgfxB = binding->get<tgfx::Layer>(layerB);
  ASSERT_TRUE(tgfxA != nullptr);
  ASSERT_TRUE(tgfxB != nullptr);
  scene->getDefaultTimeline()->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxA->alpha(), 0.5f);

  // Repoint the target from A to B and mark the referencing AnimationObject dirty: the timeline is
  // rebuilt and re-resolves to B.
  object->target = "B";
  tgfxA->setAlpha(1.0f);
  tgfxB->setAlpha(1.0f);
  doc->notifyChange({object}, true);
  scene->getDefaultTimeline()->apply(1.0f);
  EXPECT_FLOAT_EQ(tgfxB->alpha(), 0.5f);
  EXPECT_FLOAT_EQ(tgfxA->alpha(), 1.0f);
}

/**
 * Test case: removing the animation driver from a layer and notifying with the animation node dirty
 * stops it driving. The composition timeline is rebuilt from the owner layer's now-empty driver
 * list, so no timeline drives the target and advancing no longer changes it.
 */
PAGX_TEST(PAGXTest, NotifyChangeRemovedAnimationStopsDriving) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto comp = doc->makeNode<pagx::Composition>("comp");
  comp->width = 50;
  comp->height = 50;
  auto child = doc->makeNode<pagx::Layer>("child");
  child->width = 50;
  child->height = 50;
  comp->layers.push_back(child);

  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "child";
  anim->objects.push_back(object);
  auto* alphaChannel = doc->makeNode<pagx::TypedChannel<float>>();
  alphaChannel->name = "alpha";
  alphaChannel->keyframes.push_back({0, 0.5f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(alphaChannel);

  auto compLayer = doc->makeNode<pagx::Layer>("compLayer");
  compLayer->composition = comp;
  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = "anim";
  driver->playing = true;
  compLayer->timelines.push_back(std::move(driver));
  doc->layers.push_back(compLayer);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = static_cast<pagx::PAGComposition*>(scene->rootComposition()->children[0].get())
                      ->binding.get();
  auto tgfxChild = binding->get<tgfx::Layer>(child);
  ASSERT_TRUE(tgfxChild != nullptr);
  scene->advanceAndApply(0);
  EXPECT_FLOAT_EQ(tgfxChild->alpha(), 0.5f);

  // Remove the driver from the layer and notify with the animation node dirty: timelines are rebuilt
  // from the now-empty driver list, so nothing drives the child.
  compLayer->timelines.clear();
  doc->notifyChange({anim}, true);

  tgfxChild->setAlpha(1.0f);
  scene->advanceAndApply(500'000);
  EXPECT_FLOAT_EQ(tgfxChild->alpha(), 1.0f);
}

/**
 * Test case: notifyChange adds a newly inserted top-level layer to the live scene while keeping the
 * existing sibling's tgfx layer (and handle) unchanged.
 */
PAGX_TEST(PAGXTest, NotifyChangeAddLayer) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto first = doc->makeNode<pagx::Layer>("A");
  first->width = 50;
  first->height = 50;
  doc->layers.push_back(first);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto firstTgfx = scene->mutableBinding()->get<tgfx::Layer>(first);
  ASSERT_TRUE(firstTgfx != nullptr);

  auto second = doc->makeNode<pagx::Layer>("B");
  second->width = 30;
  second->height = 30;
  doc->layers.push_back(second);
  doc->notifyChange({second}, true);

  // The new layer now has a tgfx mapping, and the existing one is untouched (same instance).
  auto secondTgfx = scene->mutableBinding()->get<tgfx::Layer>(second);
  ASSERT_TRUE(secondTgfx != nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(first).get(), firstTgfx.get());
  EXPECT_NE(secondTgfx.get(), firstTgfx.get());
}

/**
 * Test case: notifyChange removes a deleted layer from the live scene and drops its binding, while
 * the surviving sibling's tgfx layer (and handle) stays valid.
 */
PAGX_TEST(PAGXTest, NotifyChangeRemoveLayer) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto first = doc->makeNode<pagx::Layer>("A");
  first->width = 50;
  first->height = 50;
  doc->layers.push_back(first);
  auto second = doc->makeNode<pagx::Layer>("B");
  second->width = 30;
  second->height = 30;
  doc->layers.push_back(second);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto firstTgfx = scene->mutableBinding()->get<tgfx::Layer>(first);
  ASSERT_TRUE(firstTgfx != nullptr);
  ASSERT_TRUE(scene->mutableBinding()->get<tgfx::Layer>(second) != nullptr);

  // Remove the second layer from the document and notify.
  doc->layers.pop_back();
  doc->notifyChange({second}, true);

  // The removed layer's binding is dropped; the surviving layer keeps its instance.
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(second), nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(first).get(), firstTgfx.get());
}

/**
 * Test case: notifyChange reconciles a plain layer's nested children (Layer.children): adding and
 * removing a nested child is reflected, while the parent and surviving children keep their handles.
 */
PAGX_TEST(PAGXTest, NotifyChangeNestedChildAddRemove) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto parent = doc->makeNode<pagx::Layer>("P");
  parent->width = 100;
  parent->height = 100;
  doc->layers.push_back(parent);
  auto childA = doc->makeNode<pagx::Layer>("A");
  childA->width = 40;
  childA->height = 40;
  parent->children.push_back(childA);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto parentTgfx = scene->mutableBinding()->get<tgfx::Layer>(parent);
  auto childATgfx = scene->mutableBinding()->get<tgfx::Layer>(childA);
  ASSERT_TRUE(parentTgfx != nullptr);
  ASSERT_TRUE(childATgfx != nullptr);

  // Add a nested child B under the parent and notify with the parent (container) node.
  auto childB = doc->makeNode<pagx::Layer>("B");
  childB->width = 20;
  childB->height = 20;
  parent->children.push_back(childB);
  doc->notifyChange({parent}, true);

  // B is built and bound; A and the parent keep their original tgfx instances.
  auto childBTgfx = scene->mutableBinding()->get<tgfx::Layer>(childB);
  ASSERT_TRUE(childBTgfx != nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(parent).get(), parentTgfx.get());
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(childA).get(), childATgfx.get());

  // Remove the nested child A and notify; its binding is dropped, B and parent stay valid.
  parent->children.erase(parent->children.begin());
  doc->notifyChange({parent}, true);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(childA), nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(childB).get(), childBTgfx.get());
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(parent).get(), parentTgfx.get());
}

/**
 * Test case: notifyChange adds a newly inserted composition slot layer at runtime. syncChildren has
 * no pre-built slot for the new layer, so it builds one via BuildLayerInto and attaches the
 * MakeChild subtree under it. The nested composition content is built into the scene and stays
 * hit-testable, while the existing sibling layer keeps its handle.
 */
PAGX_TEST(PAGXTest, NotifyChangeAddComposition) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto existing = doc->makeNode<pagx::Layer>("E");
  existing->width = 50;
  existing->height = 50;
  doc->layers.push_back(existing);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto existingTgfx = scene->mutableBinding()->get<tgfx::Layer>(existing);
  ASSERT_TRUE(existingTgfx != nullptr);

  // Build a composition with one filled child layer, then a slot layer that references it.
  auto comp = doc->makeNode<pagx::Composition>("comp");
  comp->width = 100;
  comp->height = 100;
  auto inner = doc->makeNode<pagx::Layer>();
  inner->name = "NestedChild";
  auto innerRect = doc->makeNode<pagx::Rectangle>();
  innerRect->position = {50, 50};
  innerRect->size = {100, 100};
  auto innerFill = doc->makeNode<pagx::Fill>();
  auto innerSolid = doc->makeNode<pagx::SolidColor>();
  innerSolid->color = {0, 0, 1, 1};
  innerFill->color = innerSolid;
  inner->contents.push_back(innerRect);
  inner->contents.push_back(innerFill);
  comp->layers.push_back(inner);

  auto slot = doc->makeNode<pagx::Layer>();
  slot->name = "Slot";
  slot->composition = comp;
  doc->layers.push_back(slot);

  // Notify with the newly inserted slot layer; syncChildren builds its composition subtree.
  doc->notifyChange({slot}, true);

  // The slot is bound, the nested composition content is hit-testable, and the existing sibling
  // layer keeps its original tgfx instance.
  ASSERT_TRUE(scene->mutableBinding()->get<tgfx::Layer>(slot) != nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(existing).get(), existingTgfx.get());
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "NestedChild");
}

/**
 * Test case: notifyChange removes a composition slot layer at runtime. syncChildren detaches the
 * slot (the binding entry, which carries the nested subtree) and drops the bindings of the slot and
 * its composition content, while the surviving sibling layer keeps its handle.
 */
PAGX_TEST(PAGXTest, NotifyChangeRemoveComposition) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto existing = doc->makeNode<pagx::Layer>("E");
  existing->width = 50;
  existing->height = 50;
  doc->layers.push_back(existing);

  auto comp = doc->makeNode<pagx::Composition>("comp");
  comp->width = 100;
  comp->height = 100;
  auto inner = doc->makeNode<pagx::Layer>();
  inner->name = "NestedChild";
  auto innerRect = doc->makeNode<pagx::Rectangle>();
  innerRect->position = {50, 50};
  innerRect->size = {100, 100};
  auto innerFill = doc->makeNode<pagx::Fill>();
  auto innerSolid = doc->makeNode<pagx::SolidColor>();
  innerSolid->color = {0, 0, 1, 1};
  innerFill->color = innerSolid;
  inner->contents.push_back(innerRect);
  inner->contents.push_back(innerFill);
  comp->layers.push_back(inner);

  auto slot = doc->makeNode<pagx::Layer>();
  slot->name = "Slot";
  slot->composition = comp;
  doc->layers.push_back(slot);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto existingTgfx = scene->mutableBinding()->get<tgfx::Layer>(existing);
  ASSERT_TRUE(existingTgfx != nullptr);
  ASSERT_TRUE(scene->mutableBinding()->get<tgfx::Layer>(slot) != nullptr);
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "NestedChild");

  // Remove the slot layer from the document and notify.
  doc->layers.pop_back();
  doc->notifyChange({slot}, true);

  // The slot's binding is dropped, the surviving sibling keeps its instance, and the composition
  // content is no longer hit-testable.
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(slot), nullptr);
  EXPECT_EQ(scene->mutableBinding()->get<tgfx::Layer>(existing).get(), existingTgfx.get());
  auto hitsAfter = scene->getLayersUnderPoint(50, 50);
  for (const auto& hit : hitsAfter) {
    EXPECT_NE(hit->name(), "NestedChild");
  }
}

/**
 * Test case: GetNodeChannel/SetNodeChannel round-trip scalar fields across node types.
 */
PAGX_TEST(PAGXTest, NodeChannelScalarRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);

  EXPECT_TRUE(pagx::SetNodeChannel(layer, "alpha", pagx::KeyValue(0.4f)));
  EXPECT_FLOAT_EQ(layer->alpha, 0.4f);
  pagx::KeyValue out;
  EXPECT_TRUE(pagx::GetNodeChannel(layer, "alpha", &out));
  EXPECT_FLOAT_EQ(std::get<float>(out), 0.4f);

  EXPECT_TRUE(pagx::SetNodeChannel(layer, "visible", pagx::KeyValue(false)));
  EXPECT_FALSE(layer->visible);

  // LayoutNode shared field via the derived type.
  EXPECT_TRUE(pagx::SetNodeChannel(layer, "width", pagx::KeyValue(120.0f)));
  EXPECT_FLOAT_EQ(layer->width, 120.0f);

  auto rect = doc->makeNode<pagx::Rectangle>();
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "roundness", pagx::KeyValue(8.0f)));
  EXPECT_FLOAT_EQ(rect->roundness, 8.0f);

  auto selector = doc->makeNode<pagx::RangeSelector>();
  EXPECT_TRUE(pagx::SetNodeChannel(selector, "randomSeed", pagx::KeyValue(7)));
  EXPECT_EQ(selector->randomSeed, 7);
}

/**
 * Test case: enum channels use the string enum name; invalid strings and wrong types are rejected.
 */
PAGX_TEST(PAGXTest, NodeChannelEnumRoundTrip) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");

  EXPECT_TRUE(pagx::SetNodeChannel(layer, "blendMode", pagx::KeyValue(std::string("multiply"))));
  EXPECT_EQ(layer->blendMode, pagx::BlendMode::Multiply);
  pagx::KeyValue out;
  EXPECT_TRUE(pagx::GetNodeChannel(layer, "blendMode", &out));
  EXPECT_EQ(std::get<std::string>(out), "multiply");

  EXPECT_FALSE(pagx::SetNodeChannel(layer, "blendMode", pagx::KeyValue(std::string("notamode"))));
  EXPECT_EQ(layer->blendMode, pagx::BlendMode::Multiply);
  EXPECT_FALSE(pagx::SetNodeChannel(layer, "blendMode", pagx::KeyValue(1.0f)));
}

/**
 * Test case: Point/Size/Padding fields are addressed by suffixed channels.
 */
PAGX_TEST(PAGXTest, NodeChannelCompositeSuffixes) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto rect = doc->makeNode<pagx::Rectangle>();
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "position.x", pagx::KeyValue(10.0f)));
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "position.y", pagx::KeyValue(20.0f)));
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "size.width", pagx::KeyValue(30.0f)));
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "size.height", pagx::KeyValue(40.0f)));
  EXPECT_FLOAT_EQ(rect->position.x, 10.0f);
  EXPECT_FLOAT_EQ(rect->position.y, 20.0f);
  EXPECT_FLOAT_EQ(rect->size.width, 30.0f);
  EXPECT_FLOAT_EQ(rect->size.height, 40.0f);

  auto group = doc->makeNode<pagx::Group>();
  EXPECT_TRUE(pagx::SetNodeChannel(group, "padding.left", pagx::KeyValue(5.0f)));
  EXPECT_TRUE(pagx::SetNodeChannel(group, "padding.bottom", pagx::KeyValue(6.0f)));
  EXPECT_FLOAT_EQ(group->padding.left, 5.0f);
  EXPECT_FLOAT_EQ(group->padding.bottom, 6.0f);
}

/**
 * Test case: Color channels round-trip; id-based lookup feeds SetNodeChannel.
 */
PAGX_TEST(PAGXTest, NodeChannelColorAndIdLookup) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  doc->makeNode<pagx::SolidColor>("fillColor");

  auto* solid = doc->findNode<pagx::SolidColor>("fillColor");
  ASSERT_TRUE(solid != nullptr);
  pagx::Color green = {0.0f, 1.0f, 0.0f, 1.0f};
  EXPECT_TRUE(pagx::SetNodeChannel(solid, "color", pagx::KeyValue(green)));
  EXPECT_EQ(solid->color, green);
  pagx::KeyValue out;
  EXPECT_TRUE(pagx::GetNodeChannel(solid, "color", &out));
  EXPECT_EQ(std::get<pagx::Color>(out), green);
}

/**
 * Test case: unknown channel, type mismatch, and null node return false.
 */
PAGX_TEST(PAGXTest, NodeChannelRejectsUnsupported) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");

  EXPECT_FALSE(pagx::SetNodeChannel(layer, "nosuchchannel", pagx::KeyValue(1.0f)));
  pagx::KeyValue out;
  EXPECT_FALSE(pagx::GetNodeChannel(layer, "nosuchchannel", &out));
  EXPECT_FALSE(pagx::SetNodeChannel(layer, "alpha", pagx::KeyValue(std::string("x"))));
  EXPECT_FALSE(pagx::SetNodeChannel(nullptr, "alpha", pagx::KeyValue(1.0f)));
}

/**
 * Test case: optional fields read false while unset, then round-trip after a write.
 */
PAGX_TEST(PAGXTest, NodeChannelOptionalFields) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto modifier = doc->makeNode<pagx::TextModifier>();

  // Unset optionals report no value on read.
  pagx::KeyValue out;
  EXPECT_FALSE(pagx::GetNodeChannel(modifier, "strokeWidth", &out));
  EXPECT_FALSE(pagx::GetNodeChannel(modifier, "fillColor", &out));

  // Writing populates the optional, and the value reads back.
  EXPECT_TRUE(pagx::SetNodeChannel(modifier, "strokeWidth", pagx::KeyValue(3.0f)));
  EXPECT_TRUE(modifier->strokeWidth.has_value());
  EXPECT_FLOAT_EQ(*modifier->strokeWidth, 3.0f);
  EXPECT_TRUE(pagx::GetNodeChannel(modifier, "strokeWidth", &out));
  EXPECT_FLOAT_EQ(std::get<float>(out), 3.0f);

  pagx::Color red = {1.0f, 0.0f, 0.0f, 1.0f};
  EXPECT_TRUE(pagx::SetNodeChannel(modifier, "fillColor", pagx::KeyValue(red)));
  EXPECT_TRUE(modifier->fillColor.has_value());
  EXPECT_TRUE(pagx::GetNodeChannel(modifier, "fillColor", &out));
  EXPECT_EQ(std::get<pagx::Color>(out), red);

  // Wrong value type on an optional is still rejected.
  EXPECT_FALSE(pagx::SetNodeChannel(modifier, "strokeWidth", pagx::KeyValue(std::string("x"))));
}

/**
 * Test case: ResetNodeChannel restores a channel to its node type's default value. Covers scalars,
 * enums, component-wise channels (only the addressed axis is reset), and optionals (reset clears the
 * value), plus rejection of null nodes and unknown channels.
 */
PAGX_TEST(PAGXTest, NodeChannelReset) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  auto modifier = doc->makeNode<pagx::TextModifier>();

  // Scalar: edit then reset returns to the default carried by a fresh node.
  float defaultAlpha = pagx::Default<pagx::Layer>().alpha;
  EXPECT_TRUE(pagx::SetNodeChannel(layer, "alpha", pagx::KeyValue(0.3f)));
  EXPECT_TRUE(pagx::ResetNodeChannel(layer, "alpha"));
  EXPECT_FLOAT_EQ(layer->alpha, defaultAlpha);

  // Enum: reset returns to the default blend mode.
  pagx::BlendMode defaultBlend = pagx::Default<pagx::Layer>().blendMode;
  EXPECT_TRUE(pagx::SetNodeChannel(layer, "blendMode", pagx::KeyValue(std::string("multiply"))));
  EXPECT_TRUE(pagx::ResetNodeChannel(layer, "blendMode"));
  EXPECT_EQ(layer->blendMode, defaultBlend);

  // Component-wise: resetting position.x leaves position.y untouched. Rectangle's default position
  // is NaN (the auto-layout sentinel), so the reset is checked via isnan rather than equality.
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "position.x", pagx::KeyValue(10.0f)));
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "position.y", pagx::KeyValue(20.0f)));
  EXPECT_TRUE(pagx::ResetNodeChannel(rect, "position.x"));
  EXPECT_TRUE(std::isnan(rect->position.x));
  EXPECT_TRUE(std::isnan(pagx::Default<pagx::Rectangle>().position.x));
  EXPECT_FLOAT_EQ(rect->position.y, 20.0f);

  // Optional: reset clears a previously written value.
  EXPECT_TRUE(pagx::SetNodeChannel(modifier, "strokeWidth", pagx::KeyValue(3.0f)));
  EXPECT_TRUE(modifier->strokeWidth.has_value());
  EXPECT_TRUE(pagx::ResetNodeChannel(modifier, "strokeWidth"));
  EXPECT_FALSE(modifier->strokeWidth.has_value());

  // Rejection: null node and unknown channel.
  EXPECT_FALSE(pagx::ResetNodeChannel(nullptr, "alpha"));
  EXPECT_FALSE(pagx::ResetNodeChannel(layer, "nosuchchannel"));
}

/**
 * Test case: ChannelsFor exposes every channel of a node type, each with a working accessor and
 * flags that match the IsAnimatableChannel/RequiresLayout queries.
 */
PAGX_TEST(PAGXTest, NodeChannelTableConsistency) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");

  const auto& channels = pagx::ChannelsFor(pagx::NodeType::Layer);
  ASSERT_FALSE(channels.empty());
  bool sawAnimatable = false;
  bool sawRequiresLayout = false;
  bool sawNoFlags = false;
  for (const auto& channel : channels) {
    // The flags reported by the table must agree with the by-name query helpers.
    EXPECT_EQ(pagx::HasFlag(channel.flags, pagx::ChannelFlags::Animatable),
              pagx::IsAnimatableChannel(pagx::NodeType::Layer, channel.channel));
    EXPECT_EQ(pagx::HasFlag(channel.flags, pagx::ChannelFlags::RequiresLayout),
              pagx::RequiresLayout(pagx::NodeType::Layer, channel.channel));
    // Every listed channel must be readable on a freshly built node.
    pagx::KeyValue out;
    EXPECT_TRUE(pagx::GetNodeChannel(layer, channel.channel, &out))
        << "channel '" << channel.channel << "' is listed but not readable";
    if (pagx::HasFlag(channel.flags, pagx::ChannelFlags::Animatable)) {
      sawAnimatable = true;
    }
    if (pagx::HasFlag(channel.flags, pagx::ChannelFlags::RequiresLayout)) {
      sawRequiresLayout = true;
    }
    if (channel.flags == pagx::ChannelFlags::None) {
      sawNoFlags = true;
    }
  }
  EXPECT_TRUE(sawAnimatable);
  EXPECT_TRUE(sawRequiresLayout);
  EXPECT_TRUE(sawNoFlags);

  // A node type with no reflectable channels yields an empty table.
  EXPECT_TRUE(pagx::ChannelsFor(pagx::NodeType::Document).empty());
}

/**
 * Test case: IsAnimatableChannel marks channels that have a runtime writer. Pure render outputs and
 * in-place geometry are animatable; auto-layout inputs (width, padding) and the layer name are not.
 */
PAGX_TEST(PAGXTest, NodeChannelAnimatableClass) {
  EXPECT_TRUE(pagx::IsAnimatableChannel(pagx::NodeType::Layer, "alpha"));
  EXPECT_TRUE(pagx::IsAnimatableChannel(pagx::NodeType::Layer, "x"));
  EXPECT_FALSE(pagx::IsAnimatableChannel(pagx::NodeType::Layer, "width"));
  EXPECT_FALSE(pagx::IsAnimatableChannel(pagx::NodeType::Layer, "padding.left"));
  EXPECT_FALSE(pagx::IsAnimatableChannel(pagx::NodeType::Layer, "name"));

  // Geometry outputs are animatable (driven in place during playback).
  EXPECT_TRUE(pagx::IsAnimatableChannel(pagx::NodeType::Rectangle, "size.width"));
  EXPECT_TRUE(pagx::IsAnimatableChannel(pagx::NodeType::Polystar, "outerRadius"));
  // Unknown channel is not animatable.
  EXPECT_FALSE(pagx::IsAnimatableChannel(pagx::NodeType::Rectangle, "nope"));
}

/**
 * Test case: RequiresLayout marks channels whose document edit only takes effect after a layout
 * pass. This covers both auto-layout inputs and layout-derived geometry, so a channel can be both
 * animatable and layout-requiring (e.g. a shape's size / position, or a layer's x / y).
 */
PAGX_TEST(PAGXTest, NodeChannelRequiresLayout) {
  // Auto-layout inputs require layout.
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Layer, "width"));
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Layer, "padding.left"));
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Text, "fontSize"));
  // Layout-derived geometry requires layout even though it is also animatable.
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Rectangle, "size.width"));
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Rectangle, "position.x"));
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Polystar, "outerRadius"));
  EXPECT_TRUE(pagx::RequiresLayout(pagx::NodeType::Layer, "x"));
  // Pure render channels refresh without layout.
  EXPECT_FALSE(pagx::RequiresLayout(pagx::NodeType::Layer, "alpha"));
  EXPECT_FALSE(pagx::RequiresLayout(pagx::NodeType::Rectangle, "roundness"));
  EXPECT_FALSE(pagx::RequiresLayout(pagx::NodeType::Fill, "alpha"));
  // Unknown channel does not require layout.
  EXPECT_FALSE(pagx::RequiresLayout(pagx::NodeType::Layer, "nope"));
}

/**
 * Test case: notifyChange with layoutChanged=false skips the layout pass — a render edit still
 * takes effect, while a layout edit made in the same call is intentionally NOT reflected (proving
 * layout was skipped).
 */
PAGX_TEST(PAGXTest, NotifyChangeRenderOnlySkipsLayout) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->alpha = 1.0f;
  doc->layers.push_back(layer);
  auto rect = doc->makeNode<pagx::Rectangle>("R");
  rect->position = {0, 0};
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  ASSERT_TRUE(scene->mutableBinding()->get<tgfx::Rectangle>(rect) != nullptr);

  // Edit a render field (alpha) and a layout field (rect size) together, but notify as render-only.
  // dirty is the host layer: geometry size is refreshed via the layer's RefreshLayerInPlace, which
  // rebuilds vector contents from renderSize(). With layout skipped, renderSize() keeps the stale
  // value, so the size edit is intentionally not reflected.
  layer->alpha = 0.3f;
  rect->size = {120, 40};
  doc->notifyChange({layer}, false);

  // Render edit reflected; layout edit NOT reflected because layout was skipped (size stays 40).
  // Re-fetch the tgfx Rectangle since RefreshLayerInPlace rebuilds vector contents.
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);
  EXPECT_FLOAT_EQ(scene->mutableBinding()->get<tgfx::Rectangle>(rect)->size().width, 40.0f);

  // Now notify with layoutChanged=true: re-layout updates renderSize and the edit takes effect.
  doc->notifyChange({layer}, true);
  EXPECT_FLOAT_EQ(scene->mutableBinding()->get<tgfx::Rectangle>(rect)->size().width, 120.0f);
}

/**
 * Test case: the documented edit workflow end to end — mutate a channel via SetNodeChannel, derive
 * the layoutChanged flag from RequiresLayout, then call notifyChange. A render channel (alpha)
 * refreshes with layout skipped; a layout-affecting channel (rect width via size.width) only takes
 * effect once RequiresLayout routes it through a layout pass.
 */
PAGX_TEST(PAGXTest, NotifyChangeFromSetNodeChannel) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->alpha = 1.0f;
  doc->layers.push_back(layer);
  auto rect = doc->makeNode<pagx::Rectangle>("R");
  rect->position = {0, 0};
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);
  ASSERT_TRUE(scene->mutableBinding()->get<tgfx::Rectangle>(rect) != nullptr);

  // Render channel: alpha does not require layout, so the caller-derived flag skips layout. The
  // edit still reaches the live layer.
  EXPECT_TRUE(pagx::SetNodeChannel(layer, "alpha", pagx::KeyValue(0.3f)));
  bool alphaLayout = pagx::RequiresLayout(layer->nodeType(), "alpha");
  EXPECT_FALSE(alphaLayout);
  doc->notifyChange({layer}, alphaLayout);
  EXPECT_FLOAT_EQ(tgfxLayer->alpha(), 0.3f);

  // Layout-affecting channel: size.width requires layout, so the derived flag re-runs layout and
  // the new width is reflected. Re-fetch the tgfx Rectangle since refresh rebuilds vector contents.
  EXPECT_TRUE(pagx::SetNodeChannel(rect, "size.width", pagx::KeyValue(120.0f)));
  bool widthLayout = pagx::RequiresLayout(rect->nodeType(), "size.width");
  EXPECT_TRUE(widthLayout);
  doc->notifyChange({layer}, widthLayout);
  EXPECT_FLOAT_EQ(scene->mutableBinding()->get<tgfx::Rectangle>(rect)->size().width, 120.0f);
}

/**
 * Test case: a Rectangle's size.width channel drives the tgfx Rectangle size in place.
 */
PAGX_TEST(PAGXTest, ChannelRectangleSize) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);
  auto rect = doc->makeNode<pagx::Rectangle>("R");
  rect->position = {0, 0};
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);

  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "R";
  anim->objects.push_back(object);
  auto* widthProp = doc->makeNode<pagx::TypedChannel<float>>();
  widthProp->name = "size.width";
  widthProp->keyframes.push_back({0, 100.0f, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(widthProp);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxRect = scene->mutableBinding()->get<tgfx::Rectangle>(rect);
  ASSERT_TRUE(tgfxRect != nullptr);
  EXPECT_FLOAT_EQ(tgfxRect->size().width, 40.0f);

  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  // mix=0.5 from 40 toward 100 = 70; height untouched.
  timeline->apply(0.5f);
  EXPECT_FLOAT_EQ(tgfxRect->size().width, 70.0f);
  EXPECT_FLOAT_EQ(tgfxRect->size().height, 40.0f);
}

/**
 * Test case: a Layer's matrix channel drives the tgfx layer transform via TRS-decomposed mixing,
 * and stacks with the layer's authored x/y translation.
 */
PAGX_TEST(PAGXTest, ChannelLayerMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->x = 10;
  layer->y = 0;
  doc->layers.push_back(layer);

  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* matrixProp = doc->makeNode<pagx::TypedChannel<pagx::Matrix>>();
  matrixProp->name = "matrix";
  // Target matrix is a pure 2x scale; baseline is identity.
  pagx::Matrix scaled = pagx::Matrix::Scale(2.0f, 2.0f);
  matrixProp->keyframes.push_back({0, scaled, pagx::KeyframeInterpolationType::Hold, {}, {}});
  object->channels.push_back(matrixProp);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  // mix=1: matrix becomes 2x scale, composed with the authored x translation (10).
  timeline->apply(1.0f);
  auto m = tgfxLayer->matrix();
  EXPECT_FLOAT_EQ(m.getScaleX(), 2.0f);
  EXPECT_FLOAT_EQ(m.getScaleY(), 2.0f);
  EXPECT_FLOAT_EQ(m.getTranslateX(), 10.0f);
}

/**
 * Test case: a matrix tween between two rotations on opposite sides of +/-pi (170deg and 190deg,
 * which atan2 recovers as +2.967 rad and -2.967 rad) takes the shortest arc (a continuous 20deg
 * sweep through 180deg) instead of the long way (340deg in the opposite direction). Multi-turn
 * winding is still unrecoverable from a baked matrix and remains documented on the scalar rotation
 * channel; this test only fixes the +/-pi-boundary case.
 */
PAGX_TEST(PAGXTest, ChannelLayerMatrixRotationShortestArc) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);

  auto anim = doc->makeNode<pagx::Animation>("anim");
  anim->duration = 60;
  anim->frameRate = 60;
  doc->animations.push_back(anim);
  auto* object = doc->makeNode<pagx::AnimationObject>();
  object->target = "L";
  anim->objects.push_back(object);
  auto* matrixProp = doc->makeNode<pagx::TypedChannel<pagx::Matrix>>();
  matrixProp->name = "matrix";
  // 170deg and 190deg encode as +2.967 / -2.967 rad after atan2 recovery; without the wrap fix the
  // tween would interpolate -5.934 rad (the long way around).
  pagx::Matrix m170 = pagx::Matrix::Rotate(170.0f);
  pagx::Matrix m190 = pagx::Matrix::Rotate(190.0f);
  matrixProp->keyframes.push_back({0, m170, pagx::KeyframeInterpolationType::Linear, {}, {}});
  matrixProp->keyframes.push_back({60, m190, pagx::KeyframeInterpolationType::Linear, {}, {}});
  object->channels.push_back(matrixProp);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto tgfxLayer = scene->mutableBinding()->get<tgfx::Layer>(layer);
  ASSERT_TRUE(tgfxLayer != nullptr);

  auto timeline = scene->getDefaultTimeline();
  ASSERT_TRUE(timeline != nullptr);
  // Sweep at 25%, 50%, 75% of the segment. The shortest-arc rotation should rise monotonically from
  // 170 -> 175 -> 180 -> 185 -> 190 deg; sin and cos act as a sufficient continuity check.
  // Without the wrap fix, the long-way path (-5.934 * t) would dip below sin(170deg) before
  // climbing back, breaking monotonicity around midway.
  auto sample = [&](int64_t timeUs) {
    timeline->setCurrentTime(timeUs);
    timeline->apply(1.0f);
    return tgfxLayer->matrix();
  };
  auto m25 = sample(250000);
  auto m50 = sample(500000);
  auto m75 = sample(750000);
  // At 50%, rotation should be exactly 180deg: cos = -1, sin = 0.
  EXPECT_NEAR(m50.getScaleX(), -1.0f, 1e-3f);
  EXPECT_NEAR(m50.getSkewY(), 0.0f, 1e-3f);
  // sin(angle) = m.b for a pure rotation. Monotonic decrease 170 -> 180 -> 190 deg means
  // sin is positive (+0.087), then 0, then negative (-0.087) — the long way would flip signs.
  EXPECT_GT(m25.getSkewY(), 0.0f);
  EXPECT_LT(m75.getSkewY(), 0.0f);
}

/**
 * Test case: every channel the reflection registry marks Animatable for a built node type has a
 * matching runtime writer, so animations cannot target a channel that silently does nothing.
 */
PAGX_TEST(PAGXTest, AnimatableChannelsHaveWriters) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  // A vector layer carrying one of each geometry / paint / modifier element.
  auto layer = doc->makeNode<pagx::Layer>("L");
  doc->layers.push_back(layer);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->size = {40, 40};
  layer->contents.push_back(ellipse);
  auto polystar = doc->makeNode<pagx::Polystar>();
  layer->contents.push_back(polystar);
  auto trim = doc->makeNode<pagx::TrimPath>();
  layer->contents.push_back(trim);
  auto roundCorner = doc->makeNode<pagx::RoundCorner>();
  layer->contents.push_back(roundCorner);
  auto repeater = doc->makeNode<pagx::Repeater>();
  layer->contents.push_back(repeater);
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(fill);
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto strokeColor = doc->makeNode<pagx::SolidColor>();
  stroke->color = strokeColor;
  layer->contents.push_back(stroke);

  // A group with transform channels, plus a fill backed by each gradient kind so the gradient point
  // writers and their ColorStop writers are exercised.
  auto group = doc->makeNode<pagx::Group>();
  auto groupRect = doc->makeNode<pagx::Rectangle>();
  groupRect->size = {20, 20};
  group->elements.push_back(groupRect);
  layer->contents.push_back(group);

  auto linear = doc->makeNode<pagx::LinearGradient>();
  auto linearStop = doc->makeNode<pagx::ColorStop>();
  linearStop->color = {1, 0, 0, 1};
  linear->colorStops.push_back(linearStop);
  auto linearFill = doc->makeNode<pagx::Fill>();
  linearFill->color = linear;
  layer->contents.push_back(linearFill);

  auto radial = doc->makeNode<pagx::RadialGradient>();
  auto radialFill = doc->makeNode<pagx::Fill>();
  radialFill->color = radial;
  layer->contents.push_back(radialFill);

  auto conic = doc->makeNode<pagx::ConicGradient>();
  auto conicFill = doc->makeNode<pagx::Fill>();
  conicFill->color = conic;
  layer->contents.push_back(conicFill);

  auto diamond = doc->makeNode<pagx::DiamondGradient>();
  auto diamondFill = doc->makeNode<pagx::Fill>();
  diamondFill->color = diamond;
  layer->contents.push_back(diamondFill);

  // A text modifier carrying a range selector exercises the modifier transform/color writers and
  // the selector writers.
  auto modifier = doc->makeNode<pagx::TextModifier>();
  auto selector = doc->makeNode<pagx::RangeSelector>();
  modifier->selectors.push_back(selector);
  layer->contents.push_back(modifier);

  // One of each layer style and filter kind so their animatable writers are covered.
  auto dropStyle = doc->makeNode<pagx::DropShadowStyle>();
  layer->styles.push_back(dropStyle);
  auto innerStyle = doc->makeNode<pagx::InnerShadowStyle>();
  layer->styles.push_back(innerStyle);
  auto backgroundBlurStyle = doc->makeNode<pagx::BackgroundBlurStyle>();
  layer->styles.push_back(backgroundBlurStyle);
  auto blurFilter = doc->makeNode<pagx::BlurFilter>();
  layer->filters.push_back(blurFilter);
  auto dropFilter = doc->makeNode<pagx::DropShadowFilter>();
  layer->filters.push_back(dropFilter);
  auto innerFilter = doc->makeNode<pagx::InnerShadowFilter>();
  layer->filters.push_back(innerFilter);
  auto blendFilter = doc->makeNode<pagx::BlendFilter>();
  layer->filters.push_back(blendFilter);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding != nullptr);

  // For each built node, every Animatable channel in the registry must have a runtime writer. Text
  // and TextBox are intentionally omitted: they require a registered font to build, while every
  // other node type with animatable channels is covered here.
  pagx::Node* nodes[] = {
      layer,      rect,       ellipse,     polystar,   trim,      roundCorner, repeater,
      fill,       stroke,     solid,       group,      linear,    linearStop,  radial,
      conic,      diamond,    modifier,    selector,   dropStyle, innerStyle,  backgroundBlurStyle,
      blurFilter, dropFilter, innerFilter, blendFilter};
  for (auto* node : nodes) {
    for (const auto& channel : pagx::ChannelsFor(node->nodeType())) {
      if (!pagx::HasFlag(channel.flags, pagx::ChannelFlags::Animatable)) {
        continue;
      }
      EXPECT_TRUE(binding->hasWriter(node, channel.channel))
          << "node type " << static_cast<int>(node->nodeType()) << " channel '" << channel.channel
          << "' is Animatable but has no runtime writer";
    }
  }
}

/**
 * Test: a SolidColor shared by two Fill painters stays bound when only one Fill is removed.
 */
PAGX_TEST(PAGXTest, SharedSolidColorSurvivesAfterOnePainterRemoved) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto solid = doc->makeNode<pagx::SolidColor>("sharedSolid");
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {20, 20};
  layer->contents.push_back(rect1);
  auto fill1 = doc->makeNode<pagx::Fill>();
  fill1->color = solid;
  layer->contents.push_back(fill1);

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {20, 20};
  layer->contents.push_back(rect2);
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = solid;
  layer->contents.push_back(fill2);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding->get<tgfx::SolidColor>(solid) != nullptr);
  ASSERT_TRUE(binding->get<tgfx::FillStyle>(fill1) != nullptr);
  ASSERT_TRUE(binding->get<tgfx::FillStyle>(fill2) != nullptr);

  layer->contents.erase(std::remove(layer->contents.begin(), layer->contents.end(), fill2),
                        layer->contents.end());
  doc->notifyChange({layer}, true);

  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill2) == nullptr);
  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill1) != nullptr);
  EXPECT_TRUE(binding->get<tgfx::SolidColor>(solid) != nullptr);
}

/**
 * Test: a SolidColor shared by two Fill painters is unbound when all Fills are removed.
 */
PAGX_TEST(PAGXTest, BothPaintersRemovedUnbindsSharedSolidColor) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto solid = doc->makeNode<pagx::SolidColor>("sharedSolid");
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};

  auto fill1 = doc->makeNode<pagx::Fill>();
  fill1->color = solid;
  layer->contents.push_back(fill1);
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = solid;
  layer->contents.push_back(fill2);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding->get<tgfx::SolidColor>(solid) != nullptr);

  layer->contents.clear();
  doc->notifyChange({layer}, true);

  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill1) == nullptr);
  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill2) == nullptr);
  EXPECT_TRUE(binding->get<tgfx::SolidColor>(solid) == nullptr);
}

/**
 * Test: an Image shared by two ImagePattern painters stays bound when one pattern is removed.
 */
PAGX_TEST(PAGXTest, SharedImageSurvivesAfterOnePatternRemoved) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto imageNode = doc->makeNode<pagx::Image>("sharedImage");
  imageNode->filePath =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/"
      "5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg==";

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {20, 20};
  layer->contents.push_back(rect1);
  auto pattern1 = doc->makeNode<pagx::ImagePattern>();
  pattern1->image = imageNode;
  auto fill1 = doc->makeNode<pagx::Fill>();
  fill1->color = pattern1;
  layer->contents.push_back(fill1);

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {20, 20};
  layer->contents.push_back(rect2);
  auto pattern2 = doc->makeNode<pagx::ImagePattern>();
  pattern2->image = imageNode;
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = pattern2;
  layer->contents.push_back(fill2);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding->get<tgfx::Image>(imageNode) != nullptr);
  ASSERT_TRUE(binding->contains(pattern1));
  ASSERT_TRUE(binding->contains(pattern2));

  layer->contents.erase(std::remove(layer->contents.begin(), layer->contents.end(), fill2),
                        layer->contents.end());
  doc->notifyChange({layer}, true);

  EXPECT_FALSE(binding->contains(pattern2));
  EXPECT_TRUE(binding->contains(pattern1));
  EXPECT_TRUE(binding->get<tgfx::Image>(imageNode) != nullptr);
}

/**
 * Test: an Image shared by two ImagePattern painters is unbound when all patterns are removed.
 */
PAGX_TEST(PAGXTest, SharedImageUnboundAfterAllPatternsRemoved) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto imageNode = doc->makeNode<pagx::Image>("sharedImage");
  imageNode->filePath =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8/"
      "5+hHgAHggJ/PchI7wAAAABJRU5ErkJggg==";

  auto pattern1 = doc->makeNode<pagx::ImagePattern>();
  pattern1->image = imageNode;
  auto fill1 = doc->makeNode<pagx::Fill>();
  fill1->color = pattern1;
  layer->contents.push_back(fill1);

  auto pattern2 = doc->makeNode<pagx::ImagePattern>();
  pattern2->image = imageNode;
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = pattern2;
  layer->contents.push_back(fill2);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding->get<tgfx::Image>(imageNode) != nullptr);

  layer->contents.clear();
  doc->notifyChange({layer}, true);

  EXPECT_FALSE(binding->contains(fill1));
  EXPECT_FALSE(binding->contains(pattern1));
  EXPECT_FALSE(binding->contains(fill2));
  EXPECT_FALSE(binding->contains(pattern2));
  EXPECT_TRUE(binding->get<tgfx::Image>(imageNode) == nullptr);
}

/**
 * Test: a LinearGradient shared by two Fill painters stays bound (with its ColorStops) when one
 * Fill is removed.
 */
PAGX_TEST(PAGXTest, SharedGradientSurvivesAfterOnePainterRemoved) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto gradient = doc->makeNode<pagx::LinearGradient>("grad");
  gradient->startPoint = {0, 0};
  gradient->endPoint = {1, 1};
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0;
  stop1->color = {1, 0, 0, 1};
  gradient->colorStops.push_back(stop1);
  auto stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1;
  stop2->color = {0, 0, 1, 1};
  gradient->colorStops.push_back(stop2);

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size = {20, 20};
  layer->contents.push_back(rect1);
  auto fill1 = doc->makeNode<pagx::Fill>();
  fill1->color = gradient;
  layer->contents.push_back(fill1);

  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size = {20, 20};
  layer->contents.push_back(rect2);
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = gradient;
  layer->contents.push_back(fill2);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* binding = scene->mutableBinding();
  ASSERT_TRUE(binding->get<tgfx::FillStyle>(fill1) != nullptr);
  ASSERT_TRUE(binding->get<tgfx::FillStyle>(fill2) != nullptr);
  ASSERT_TRUE(binding->get<tgfx::Gradient>(gradient) != nullptr);

  layer->contents.erase(std::remove(layer->contents.begin(), layer->contents.end(), fill2),
                        layer->contents.end());
  doc->notifyChange({layer}, true);

  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill2) == nullptr);
  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill1) != nullptr);
  EXPECT_TRUE(binding->get<tgfx::Gradient>(gradient) != nullptr);
  EXPECT_TRUE(binding->contains(stop1));
  EXPECT_TRUE(binding->contains(stop2));
}

/**
 * Test case: multiple refreshLayerInPlace calls do not cause duplicate entries in the
 * colorSourceUsers reverse index. Each refresh rebuilds vector contents, and convertFill
 * calls trackColorSource again. Without the pre-refresh untrack step, the colorSourceUsers
 * vector would grow by one duplicate entry per refresh.
 */
PAGX_TEST(PAGXTest, ReverseIndexNoDuplicatesAfterRepeatedRefresh) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto solid = doc->makeNode<pagx::SolidColor>("solid");
  solid->color = {1, 0, 0, 1};

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {20, 20};
  layer->contents.push_back(rect);
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = solid;
  layer->contents.push_back(fill);

  auto scene = pagx::PAGScene::Make(doc);
  auto* binding = scene->mutableBinding();

  size_t countBefore = binding->colorSourceUsers.at(solid).size();
  EXPECT_EQ(countBefore, 1u);

  // Refresh the layer 3 times without any content change.
  for (int i = 0; i < 3; i++) {
    doc->notifyChange({layer}, false);
  }

  // After repeated refreshes the reverse index must still have exactly one entry.
  size_t countAfter = binding->colorSourceUsers.at(solid).size();
  EXPECT_EQ(countAfter, 1u);
}

/**
 * Test case: removing a Fill inside a Group and calling notifyChange properly untracks the
 * old Fill from the reverse index and unbinds its shared color source when no other Fill
 * references it. Before the fix, collectElementTree only collected top-level content
 * elements, missing nested Group children.
 */
PAGX_TEST(PAGXTest, GroupInnerFillRemovedUnbindsSharedColor) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 50;
  layer->height = 50;
  doc->layers.push_back(layer);

  auto solid = doc->makeNode<pagx::SolidColor>("solid");
  solid->color = {0, 1, 0, 1};

  auto group = doc->makeNode<pagx::Group>();
  auto ell = doc->makeNode<pagx::Ellipse>();
  ell->size = {20, 20};
  group->elements.push_back(ell);
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = solid;
  group->elements.push_back(fill);

  layer->contents.push_back(group);

  auto scene = pagx::PAGScene::Make(doc);
  auto* binding = scene->mutableBinding();

  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill) != nullptr);
  EXPECT_TRUE(binding->contains(solid));

  // Remove the Fill from inside the Group.
  group->elements.erase(std::remove(group->elements.begin(), group->elements.end(), fill),
                        group->elements.end());
  doc->notifyChange({layer}, true);

  EXPECT_TRUE(binding->get<tgfx::FillStyle>(fill) == nullptr);
  EXPECT_FALSE(binding->contains(solid));
  EXPECT_EQ(binding->colorSourceUsers.find(solid), binding->colorSourceUsers.end());
}

/**
 * Test case: notifyChange on a deeply nested pure-container with added grandchildren. Hit test
 * confirms the full depth is rebuilt correctly after the edit.
 */
PAGX_TEST(PAGXTest, NotifyChangeDeeplyNestedChildHitTest) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto root = doc->makeNode<pagx::Layer>("Root");
  root->name = "Root";
  root->width = 200;
  root->height = 200;
  doc->layers.push_back(root);

  auto middle = doc->makeNode<pagx::Layer>("Middle");
  middle->name = "Middle";
  middle->width = 200;
  middle->height = 200;
  root->children.push_back(middle);

  auto leaf = doc->makeNode<pagx::Layer>("Leaf");
  leaf->name = "Leaf";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  leaf->contents.push_back(rect);
  leaf->contents.push_back(fill);
  middle->children.push_back(leaf);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto hits = scene->getLayersUnderPoint(70, 70);
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0]->name(), "Leaf");
  EXPECT_EQ(hits[1]->name(), "Middle");
  EXPECT_EQ(hits[2]->name(), "Root");

  // Add a grandchild under middle and notify.
  auto grandchild = doc->makeNode<pagx::Layer>("Grandchild");
  grandchild->name = "Grandchild";
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {120, 120};
  rect2->size = {60, 60};
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0, 1, 0, 1};
  fill2->color = solid2;
  grandchild->contents.push_back(rect2);
  grandchild->contents.push_back(fill2);
  middle->children.push_back(grandchild);
  doc->notifyChange({middle}, /*layoutChanged=*/true);

  hits = scene->getLayersUnderPoint(70, 70);
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0]->name(), "Leaf");
  hits = scene->getLayersUnderPoint(120, 120);
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0]->name(), "Grandchild");

  // Remove the grandchild and notify; it is no longer hit.
  middle->children.pop_back();
  doc->notifyChange({middle}, /*layoutChanged=*/true);

  hits = scene->getLayersUnderPoint(70, 70);
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0]->name(), "Leaf");
  // No content covers (120,120) after removing grandchild: Middle has no direct content.
  hits = scene->getLayersUnderPoint(120, 120);
  ASSERT_EQ(hits.size(), 0u);

  // Clear all children of the middle container and notify; all nested layers are removed.
  middle->children.clear();
  doc->notifyChange({middle}, /*layoutChanged=*/true);

  hits = scene->getLayersUnderPoint(70, 70);
  ASSERT_EQ(hits.size(), 0u);
  hits = scene->getLayersUnderPoint(120, 120);
  ASSERT_EQ(hits.size(), 0u);
}

/**
 * Test case: a PAGComposition nested under a plain PAGLayer container has its timelines properly
 * reset when notifyChange marks a timeline node dirty. The two baseline snapshots capture different
 * alpha values, proving the nested composition's timeline was actually rebuilt.
 */
PAGX_TEST(PAGXTest, NotifyChangeResetsTimelinesInNestedContainer) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto fx = MakeAlphaComposition(doc.get(), "comp", "anim", "child");

  auto container = doc->makeNode<pagx::Layer>("Container");
  container->name = "Container";
  container->width = 200;
  container->height = 200;

  auto slot = doc->makeNode<pagx::Layer>("Slot");
  slot->composition = fx.comp;
  slot->width = 100;
  slot->height = 100;
  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = fx.animationId;
  driver->playing = true;
  slot->timelines.push_back(std::move(driver));
  container->children.push_back(slot);
  doc->layers.push_back(container);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto& rootChildren = scene->rootComposition()->children;
  ASSERT_EQ(rootChildren.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
  EXPECT_EQ(rootChildren[0]->children[0]->layerType(), pagx::LayerType::Composition);

  // Advance to frame 30: linear keyframes [0→0, 60→1.0] → alpha = 0.5.
  scene->advanceAndApply(500'000);
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/ResetTimelinesInNestedContainer_before"));

  // Modify the starting keyframe value and mark Animation dirty → triggers resetTimelines.
  fx.alphaChannel->keyframes[0].value = 0.3f;
  doc->notifyChange({fx.comp->animations[0]}, /*layoutChanged=*/true);

  // After reset, the rebuilt timeline uses the new keyframes. At frame 30: alpha = 0.3 + 0.35 = 0.65.
  scene->advanceAndApply(500'000);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/ResetTimelinesInNestedContainer_after"));
}

/**
 * Test case: external composition loaded under a plain container, then editing a child document
 * node and notifying the child document syncs the edit to the parent scene through the container
 * nesting. Verifies that PAGLayer children (introduced for plain-container nesting) don't break
 * the external-composition reverse-registration path.
 */
PAGX_TEST(PAGXTest, ExternalPAGXContainerNotify) {
  std::string mainXML =
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"container\" width=\"200\" height=\"200\">\n"
      "    <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  std::string childXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"childLayer\" name=\"childLayer\" width=\"50\" height=\"50\">\n"
      "    <Rectangle width=\"50\" height=\"50\"/>\n"
      "    <Fill>\n"
      "      <SolidColor color=\"#FF0000\"/>\n"
      "    </Fill>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"fade\" duration=\"60\" frameRate=\"60\">\n"
      "      <Object target=\"childLayer\">\n"
      "        <Channel name=\"alpha\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\" interpolation=\"linear\"/>\n"
      "          <Key time=\"60\" value=\"1\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n";
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  auto* childDoc = slotLayer->externalDoc.get();
  auto* childLayerNode = childDoc->findNode<pagx::Layer>("childLayer");
  ASSERT_TRUE(childLayerNode != nullptr);

  // Verify the scene built correctly (slot nested under container).
  auto& rootChildren = scene->rootComposition()->children;
  ASSERT_EQ(rootChildren.size(), 1u);
  ASSERT_EQ(rootChildren[0]->layerType(), pagx::LayerType::Layer);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children[0]->layerType(), pagx::LayerType::Composition);

  // Edit a node in the child document; notify through the child document.
  childLayerNode->alpha = 0.25f;
  childDoc->notifyChange({childLayerNode}, /*layoutChanged=*/false);

  // Verify the parent scene reflects the change via hit-test position.
  auto hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "childLayer");
}

/**
 * Test case: load external composition, create scene, then edit the slot layer on the parent
 * document and notify the parent. Verifies the parent doc's notifyChange correctly rebuilds a
 * slot that lives under a plain container.
 */
PAGX_TEST(PAGXTest, ExternalPAGXLoadNotifySlot) {
  std::string mainXML =
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"container\" width=\"200\" height=\"200\">\n"
      "    <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "  </Layer>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);

  std::string childXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"childLayer\" name=\"childLayer\" width=\"50\" height=\"50\">\n"
      "    <Rectangle width=\"50\" height=\"50\"/>\n"
      "    <Fill>\n"
      "      <SolidColor color=\"#FF0000\"/>\n"
      "    </Fill>\n"
      "  </Layer>\n"
      "</pagx>\n";
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Hit-test before edit: childLayer covers (0,0)-(50,50) inside the 200x200 scene.
  auto hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "childLayer");

  // Edit the slot layer and notify the parent document.
  slotLayer->width = 80;
  slotLayer->height = 80;
  doc->notifyChange({slotLayer}, /*layoutChanged=*/true);

  // Verify the scene still has the expected structure after the edit.
  auto& rootChildren = scene->rootComposition()->children;
  ASSERT_EQ(rootChildren.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children[0]->layerType(), pagx::LayerType::Composition);

  // After edit and layout, the composition content is still hittable under the container.
  hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "childLayer");
}

/**
 * Test case: passing a content node (SolidColor) directly to notifyChange resolves it to its
 * owning Layer and refreshes the scene in place.
 */
PAGX_TEST(PAGXTest, NotifyChangeContentNode) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents = {rect, fill};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Change the SolidColor's value and notify with the SolidColor node directly.
  solid->color = {0, 1, 0, 1};
  doc->notifyChange({solid}, /*layoutChanged=*/false);

  // The scene should still have the correct structure — the SolidColor was resolved to
  // its owning Layer and refreshed.
  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
}

/**
 * Test case: dirtying a content node in an external composition and notifying the child document
 * triggers the foreign-node full-rebuild path in the parent scene.
 */
PAGX_TEST(PAGXTest, ExternalPAGXContentNodeNotify) {
  std::string mainXML =
      "<pagx width=\"100\" height=\"100\">\n"
      "  <Layer id=\"slot\" composition=\"child.pagx\"/>\n"
      "</pagx>\n";
  auto doc = pagx::PAGXImporter::FromXML(mainXML);
  ASSERT_TRUE(doc != nullptr);
  std::string childXML =
      "<pagx width=\"50\" height=\"50\">\n"
      "  <Layer id=\"childLayer\" name=\"childLayer\" width=\"50\" height=\"50\">\n"
      "    <Rectangle width=\"50\" height=\"50\"/>\n"
      "    <Fill>\n"
      "      <SolidColor id=\"solid\" color=\"#FF0000\"/>\n"
      "    </Fill>\n"
      "  </Layer>\n"
      "</pagx>\n";
  EXPECT_TRUE(doc->loadFileData("child.pagx", MakePAGXData(childXML)));
  auto* slotLayer = doc->findNode<pagx::Layer>("slot");
  ASSERT_TRUE(slotLayer != nullptr);
  ASSERT_TRUE(slotLayer->externalDoc != nullptr);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto* childDoc = slotLayer->externalDoc.get();
  auto* solid = childDoc->findNode<pagx::SolidColor>("solid");
  ASSERT_TRUE(solid != nullptr);

  // Edit a content node in the child document and notify the child document.
  solid->color = {0, 1, 0, 1};
  childDoc->notifyChange({solid}, /*layoutChanged=*/false);

  // The parent scene should have rebuilt from the foreign-node change.
  auto hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "childLayer");
}

/**
 * Test case: passing a ColorStop directly to notifyChange resolves it through the
 * Fill → Gradient → ColorStop chain and refreshes the owning Layer.
 */
PAGX_TEST(PAGXTest, NotifyChangeColorStop) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto gradient = doc->makeNode<pagx::LinearGradient>();
  auto stop = doc->makeNode<pagx::ColorStop>();
  stop->offset = 0;
  stop->color = {1, 0, 0, 1};
  gradient->colorStops = {stop};
  fill->color = gradient;
  layer->contents = {rect, fill};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Edit the ColorStop directly and notify.
  stop->color = {0, 1, 0, 1};
  doc->notifyChange({stop}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
}

/**
 * Test case: passing an Image node (through ImagePattern.image) directly to notifyChange
 * resolves it through Fill → ImagePattern → Image and refreshes the owning Layer.
 */
PAGX_TEST(PAGXTest, NotifyChangeImageNode) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  auto fill = doc->makeNode<pagx::Fill>();
  auto image = doc->makeNode<pagx::Image>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  fill->color = pattern;
  layer->contents = {rect, fill};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Edit the Image node and pass it directly to notifyChange.
  image->filePath = "updated.png";
  doc->notifyChange({image}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
}

/**
 * Test case: passing a nested element inside a Group resolves through Group.elements
 * and refreshes the owning Layer.
 */
PAGX_TEST(PAGXTest, NotifyChangeGroupElement) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>("rect");
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  group->elements = {rect, fill};
  layer->contents = {group};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Edit the nested Rectangle and pass it directly to notifyChange.
  rect->size.width = 80;
  doc->notifyChange({rect}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
}

/**
 * Test case: passing a LayerFilter directly to notifyChange resolves it through
 * Layer.filters and refreshes the owning Layer.
 */
PAGX_TEST(PAGXTest, NotifyChangeFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 5;
  blur->blurY = 5;
  layer->contents = {rect, fill};
  layer->filters = {blur};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Edit the filter and pass it directly to notifyChange.
  blur->blurX = 10;
  doc->notifyChange({blur}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
}

/**
 * Test case: a SolidColor shared by two Layers via their respective Fill is resolved to both
 * owning Layers when passed to notifyChange. Verifies findLayerForContentNode returns all
 * matching Layers, not just the first one found.
 */
PAGX_TEST(PAGXTest, NotifyChangeSharedColorUpdatesAllLayers) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer1 = doc->makeNode<pagx::Layer>("layer1");
  layer1->width = 100;
  layer1->height = 100;
  auto layer2 = doc->makeNode<pagx::Layer>("layer2");
  layer2->width = 100;
  layer2->height = 100;

  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->size.width = 100;
  rect1->size.height = 100;
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->size.width = 100;
  rect2->size.height = 100;

  auto fill1 = doc->makeNode<pagx::Fill>();
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill1->color = solid;
  fill2->color = solid;
  layer1->contents = {rect1, fill1};
  layer2->contents = {rect2, fill2};
  doc->layers = {layer1, layer2};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Change the shared SolidColor and notify with the SolidColor node directly.
  solid->color = {0, 1, 0, 1};
  doc->notifyChange({solid}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 2u);
}

/**
 * Test case: a content node not owned by any Layer (orphan) is silently dropped
 * by notifyChange — no crash, just a no-op.
 */
PAGX_TEST(PAGXTest, NotifyChangeOrphanNode) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("layer");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents = {rect, fill};
  doc->layers = {layer};

  // Create an orphan node not attached to any Layer tree.
  auto orphan = doc->makeNode<pagx::SolidColor>();
  orphan->color = {0, 1, 0, 1};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Notify with the orphan — should not crash, just silently drop it.
  doc->notifyChange({orphan}, /*layoutChanged=*/false);

  ASSERT_EQ(scene->rootComposition()->children.size(), 1u);
}

/**
 * Test case: a shared_ptr to a child inside a plain container stays valid after the container
 * is marked dirty and notifyChange does an incremental sync (handle preservation).
 */
PAGX_TEST(PAGXTest, NotifyChangePreserveContainerChildHandle) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto container = doc->makeNode<pagx::Layer>("container");
  container->width = 200;
  container->height = 200;
  auto leaf = doc->makeNode<pagx::Layer>("leaf");
  leaf->name = "Leaf";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 50;
  rect->size.height = 50;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  leaf->contents = {rect, fill};
  container->children = {leaf};
  doc->layers = {container};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Capture a handle to the leaf before notifyChange.
  auto& containerChild = scene->rootComposition()->children[0];
  ASSERT_EQ(containerChild->layerType(), pagx::LayerType::Layer);
  auto handle = containerChild->children[0];
  ASSERT_TRUE(handle != nullptr);
  ASSERT_EQ(handle->name(), "Leaf");

  // Edit the container (add a new child at a different position) and notify.
  auto newLeaf = doc->makeNode<pagx::Layer>("newLeaf");
  newLeaf->name = "NewLeaf";
  newLeaf->width = 50;
  newLeaf->height = 50;
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {100, 0};
  rect2->size.width = 50;
  rect2->size.height = 50;
  auto fill2 = doc->makeNode<pagx::Fill>();
  fill2->color = solid;
  newLeaf->contents = {rect2, fill2};
  container->children.push_back(newLeaf);
  doc->notifyChange({container}, /*layoutChanged=*/true);

  // The old handle must still point to the same instance and be hit-testable.
  ASSERT_EQ(handle->name(), "Leaf");
  auto hits = scene->getLayersUnderPoint(25, 25);
  ASSERT_FALSE(hits.empty());
  EXPECT_EQ(hits[0]->name(), "Leaf");
  AssertSceneConsistent(scene);
}

/**
 * Test case: animation timeline drives correctly when the composition slot is nested under a
 * plain container. advance/apply propagates through the container's children.
 */
PAGX_TEST(PAGXTest, CompositionTimelineThroughContainer) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto fx = MakeAlphaComposition(doc.get(), "comp", "anim", "child");

  auto container = doc->makeNode<pagx::Layer>("container");
  container->width = 200;
  container->height = 200;

  auto slot = doc->makeNode<pagx::Layer>("slot");
  slot->composition = fx.comp;
  slot->width = 100;
  slot->height = 100;
  auto driver = std::make_unique<pagx::AnimationTimeline>();
  driver->animationId = fx.animationId;
  driver->playing = true;
  slot->timelines.push_back(std::move(driver));
  container->children.push_back(slot);
  doc->layers.push_back(container);

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  auto& rootChildren = scene->rootComposition()->children;
  ASSERT_EQ(rootChildren.size(), 1u);
  ASSERT_EQ(rootChildren[0]->layerType(), pagx::LayerType::Layer);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children[0]->layerType(), pagx::LayerType::Composition);

  // Advance through the container: linear 0→1 over 60 frames. Just verify it doesn't crash
  // and the tree stays intact.
  scene->advanceAndApply(500'000);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
  ASSERT_EQ(rootChildren[0]->children[0]->layerType(), pagx::LayerType::Composition);

  // Advance to end.
  scene->advanceAndApply(500'000);
  ASSERT_EQ(rootChildren[0]->children.size(), 1u);
}

/**
 * Test case: changing PAGXDocument::width and notifying the document itself triggers a full
 * rebuild, reflecting the new size in runtime layers.
 */
PAGX_TEST(PAGXTest, NotifyChangeDocumentSize) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>("L");
  layer->width = 200;
  layer->height = 200;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 100;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents = {rect, fill};
  doc->layers = {layer};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);
  EXPECT_FLOAT_EQ(scene->width(), 200);
  EXPECT_FLOAT_EQ(scene->height(), 200);

  // Resize the document and notify the document node.
  doc->width = 300;
  doc->height = 300;
  doc->notifyChange({doc.get()}, /*layoutChanged=*/true);

  EXPECT_FLOAT_EQ(scene->width(), 300);
  EXPECT_FLOAT_EQ(scene->height(), 300);
  auto hits = scene->getLayersUnderPoint(50, 50);
  ASSERT_FALSE(hits.empty());
}

/**
 * Test case: after changing document size and notifying the document node, layers with constraint
 * layout (bottom/right anchored) re-layout to new positions relative to the new canvas size.
 * Baseline snapshots are taken before and after the resize for visual verification.
 */
PAGX_TEST(PAGXTest, NotifyChangeDocumentSizeConstraints) {
  auto doc = pagx::PAGXDocument::Make(400, 300);

  // Top-level layer anchored to document canvas via bottom/right constraints.
  auto child = doc->makeNode<pagx::Layer>("child");
  child->width = 100;
  child->height = 60;
  child->bottom = 25;
  child->right = 50;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size.width = 100;
  rect->size.height = 60;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  child->contents = {rect, fill};
  doc->layers = {child};

  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene != nullptr);

  // Bottom=25 on 300-high canvas → y = 300 - 60 - 25 = 215.
  // Right=50 on 400-wide canvas → x = 400 - 100 - 50 = 250.
  auto handle = scene->rootComposition()->children[0];
  auto bounds = scene->getGlobalBounds(handle);
  EXPECT_NEAR(bounds.x, 250, 1.0e-3f);
  EXPECT_NEAR(bounds.y, 215, 1.0e-3f);

  // Snapshot before resize: red rect at (250, 215) on 400×300 canvas.
  auto surface = pagx::PAGSurface::MakeOffscreen(400, 300);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NotifyChangeDocumentSize_before"));

  // Resize document to 600×500 and notify.
  doc->width = 600;
  doc->height = 500;
  doc->notifyChange({doc.get()}, /*layoutChanged=*/true);

  // After rebuild: y = 500 - 60 - 25 = 415, x = 600 - 100 - 50 = 450.
  handle = scene->rootComposition()->children[0];
  bounds = scene->getGlobalBounds(handle);
  EXPECT_NEAR(bounds.x, 450, 1.0e-3f);
  EXPECT_NEAR(bounds.y, 415, 1.0e-3f);

  // Snapshot after resize: red rect at (450, 415) on 600×500 canvas.
  surface = pagx::PAGSurface::MakeOffscreen(600, 500);
  ASSERT_TRUE(surface != nullptr);
  ASSERT_TRUE(scene->draw(surface));
  EXPECT_TRUE(Baseline::Compare(surface, "PAGXTest/NotifyChangeDocumentSize_after"));
}

}  // namespace pag
