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
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/svg/SVGTextLayout.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
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

static std::string SaveFile(const std::string& content, const std::string& key) {
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  if (file) {
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }
  return outPath;
}

/**
 * Test SVG export of a simple rectangle with solid fill.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Rectangle) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_rectangle.svg");
}

/**
 * Test SVG export of ellipse and circle elements.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Ellipse) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto layer1 = doc->makeNode<pagx::Layer>();
  auto circle = doc->makeNode<pagx::Ellipse>();
  circle->position = {80, 100};
  circle->size = {100, 100};
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill1->color = solid1;
  layer1->contents.push_back(circle);
  layer1->contents.push_back(fill1);
  doc->layers.push_back(layer1);

  auto layer2 = doc->makeNode<pagx::Layer>();
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {220, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_ellipse.svg");
}

/**
 * Test SVG export of path element.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Path) {
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
  SaveFile(svg, "PAGXSVGTest/svg_export_path.svg");
}

/**
 * Test SVG export of text element.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Text) {
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
  SaveFile(svg, "PAGXSVGTest/svg_export_text.svg");
}

/**
 * Test SVG export with linear gradient.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_LinearGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_linear_gradient.svg");
}

/**
 * Test SVG export with radial gradient.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_RadialGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_radial_gradient.svg");
}

/**
 * Test SVG export with layer transform (translate, scale, rotate via matrix).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Transform) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Translate(50, 50);
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_transform.svg");
}

/**
 * Test SVG export with opacity.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Opacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->alpha = 0.5f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_opacity.svg");
}

/**
 * Test SVG export with stroke properties (width, cap, join, dashes).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_StrokeProperties) {
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
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_properties.svg");
}

/**
 * Test SVG export with fill rule (evenodd).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_FillRule) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  *pathData = pagx::PathDataFromSVGString("M100,10 L40,198 L190,78 L10,78 L160,198 Z");
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
  SaveFile(svg, "PAGXSVGTest/svg_export_fill_rule.svg");
}

/**
 * Test SVG export with blur filter.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_BlurFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_blur.svg");
}

/**
 * Test SVG export with drop shadow filter.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_DropShadow) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_drop_shadow.svg");
}

/**
 * Test SVG export with nested layers (group hierarchy).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_NestedLayers) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto parentLayer = doc->makeNode<pagx::Layer>();
  parentLayer->id = "parent";

  auto childLayer1 = doc->makeNode<pagx::Layer>();
  childLayer1->id = "child1";
  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->position = {75, 75};
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
  rect2->position = {75, 75};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_nested.svg");
}

/**
 * Test SVG export with semi-transparent fill color.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_FillOpacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_fill_opacity.svg");
}

/**
 * Test SVG export with XML declaration option disabled.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_NoXmlDeclaration) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
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
PAGX_TEST(PAGXSVGTest, SVGExport_ToFile) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto outPath = ProjectPath::Absolute("test/out/PAGXSVGTest/svg_export_tofile.svg");
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  EXPECT_TRUE(pagx::SVGExporter::ToFile(*doc, outPath));
  EXPECT_TRUE(std::filesystem::exists(outPath));
  EXPECT_GT(std::filesystem::file_size(outPath), 0u);
}

/**
 * Test multiple SVG <-> PAGX round-trips.
 * Each iteration feeds the previous output back as input: SVG -> PAGX -> SVG -> PAGX -> ...
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MultiRoundTrip) {
  constexpr int NumRoundTrips = 3;
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

    for (int i = 0; i < NumRoundTrips; i++) {
      auto round = std::to_string(i + 1);

      auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
      ASSERT_FALSE(pagxXml.empty()) << baseName << " round " << round << " PAGX export failed";
      auto pagxPath = SaveFile(pagxXml, "PAGXSVGTest/multi_roundtrip_" + baseName + ".pagx");

      doc = pagx::PAGXImporter::FromFile(pagxPath);
      ASSERT_NE(doc, nullptr) << baseName << " round " << round << " PAGX import failed";

      auto svgStr = pagx::SVGExporter::ToSVG(*doc);
      ASSERT_FALSE(svgStr.empty()) << baseName << " round " << round << " SVG export failed";
      SaveFile(svgStr, "PAGXSVGTest/multi_roundtrip_" + baseName + ".svg");

      doc = pagx::SVGImporter::ParseString(svgStr);
      ASSERT_NE(doc, nullptr) << baseName << " round " << round << " SVG re-import failed";
    }
  }
}

/**
 * Test SVG export: empty document produces valid SVG.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmptyDocument) {
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
PAGX_TEST(PAGXSVGTest, SVGExport_InnerShadow) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_inner_shadow.svg");
}

/**
 * Test SVG export with multiple gradient color stops.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MultiStopGradient) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 50};
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
  size_t count = 0;
  size_t pos = 0;
  while ((pos = svg.find("<stop", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 5u);
  SaveFile(svg, "PAGXSVGTest/svg_export_multi_stop_gradient.svg");
}

/**
 * Test SVG export with combined fill and stroke on same shape.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_FillAndStroke) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
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
  SaveFile(svg, "PAGXSVGTest/svg_export_fill_and_stroke.svg");
}

/**
 * Test SVG export with layer position (x, y).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_LayerPosition) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 50;
  layer->y = 50;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.7f, 0.2f, 0.9f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_layer_position.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_XmlEscaping) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->id = "layer<\"test\">";
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "A < B & C > D \"quoted\" 'apos'";
  text->position = {100, 50};
  text->fontFamily = "Arial";
  text->fontSize = 16;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("&lt;"), std::string::npos);
  EXPECT_NE(svg.find("&gt;"), std::string::npos);
  EXPECT_NE(svg.find("&amp;"), std::string::npos);
  EXPECT_NE(svg.find("&quot;"), std::string::npos);
  EXPECT_NE(svg.find("&apos;"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_xml_escaping.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_DisplayP3Color) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.2f, 0.1f, 0.9f};
  solid->color.colorSpace = pagx::ColorSpace::DisplayP3;
  fill->color = solid;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto strokeColor = doc->makeNode<pagx::SolidColor>();
  strokeColor->color = {0.0f, 0.8f, 0.3f, 0.8f};
  strokeColor->color.colorSpace = pagx::ColorSpace::DisplayP3;
  stroke->color = strokeColor;
  stroke->width = 3.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("display-p3"), std::string::npos);
  EXPECT_NE(svg.find("style="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_display_p3.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_DisplayP3GradientStop) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 0.8f};
  stop1->color.colorSpace = pagx::ColorSpace::DisplayP3;
  auto stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops = {stop1, stop2};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("display-p3"), std::string::npos);
  EXPECT_NE(svg.find("stop-color"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_display_p3_gradient.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupTransformFull) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {50, 50};
  group->position = {200, 200};
  group->scale = {1.5f, 0.8f};
  group->rotation = 30.0f;
  group->skew = 15.0f;
  group->skewAxis = 45.0f;
  group->alpha = 0.7f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.4f, 0.2f, 0.8f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
  EXPECT_NE(svg.find("opacity=\"0.7\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_group_transform_full.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ColorMatrixFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.4f, 0.2f, 1.0f};
  fill->color = solid;
  auto cm = doc->makeNode<pagx::ColorMatrixFilter>();
  cm->matrix = {0.33f, 0.33f, 0.33f, 0.0f, 0.0f, 0.33f, 0.33f, 0.33f, 0.0f, 0.0f,
                0.33f, 0.33f, 0.33f, 0.0f, 0.0f, 0.0f,  0.0f,  0.0f,  1.0f, 0.0f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(cm);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feColorMatrix"), std::string::npos);
  EXPECT_NE(svg.find("type=\"matrix\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_color_matrix_filter.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_BlendFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;
  auto blend = doc->makeNode<pagx::BlendFilter>();
  blend->color = {1.0f, 0.5f, 0.0f, 0.8f};
  blend->blendMode = pagx::BlendMode::Multiply;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(blend);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feFlood"), std::string::npos);
  EXPECT_NE(svg.find("feBlend"), std::string::npos);
  EXPECT_NE(svg.find("flood-color="), std::string::npos);
  EXPECT_NE(svg.find("flood-opacity="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_blend_filter.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_AsymmetricBlur) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 8;
  blur->blurY = 2;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(blur);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feGaussianBlur"), std::string::npos);
  EXPECT_NE(svg.find("stdDeviation=\"8 2\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_asymmetric_blur.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_AsymmetricDropShadow) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.1f, 0.1f, 1.0f};
  fill->color = solid;
  auto shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->blurX = 10;
  shadow->blurY = 3;
  shadow->offsetX = 5;
  shadow->offsetY = 5;
  shadow->color = {0, 0, 0, 0.6f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(shadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("10 3"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_asymmetric_drop_shadow.svg");
}

static std::vector<uint8_t> MakeMinimalPNG(int width, int height) {
  std::vector<uint8_t> png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
                              0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52};
  png.push_back(static_cast<uint8_t>((width >> 24) & 0xFF));
  png.push_back(static_cast<uint8_t>((width >> 16) & 0xFF));
  png.push_back(static_cast<uint8_t>((width >> 8) & 0xFF));
  png.push_back(static_cast<uint8_t>(width & 0xFF));
  png.push_back(static_cast<uint8_t>((height >> 24) & 0xFF));
  png.push_back(static_cast<uint8_t>((height >> 16) & 0xFF));
  png.push_back(static_cast<uint8_t>((height >> 8) & 0xFF));
  png.push_back(static_cast<uint8_t>(height & 0xFF));
  return png;
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFill) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  auto pngData = MakeMinimalPNG(64, 64);
  image->data = pagx::Data::MakeWithCopy(pngData.data(), pngData.size());
  pattern->image = image;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("<image"), std::string::npos);
  EXPECT_NE(svg.find("url(#pattern"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternTiling) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  auto pngData = MakeMinimalPNG(32, 32);
  image->data = pagx::Data::MakeWithCopy(pngData.data(), pngData.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = pagx::Matrix::Scale(0.5f, 0.5f);
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("patternContentUnits=\"objectBoundingBox\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_tiling.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternNoData) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};

  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  pattern->image = image;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("fill=\"none\""), std::string::npos);

  auto fill2 = doc->makeNode<pagx::Fill>();
  auto pattern2 = doc->makeNode<pagx::ImagePattern>();
  fill2->color = pattern2;
  auto layer2 = doc->makeNode<pagx::Layer>();
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {50, 50};
  rect2->size = {80, 80};
  layer2->contents.push_back(rect2);
  layer2->contents.push_back(fill2);
  doc->layers.push_back(layer2);

  svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_FALSE(svg.empty());
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFilePath) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  image->filePath = "assets/texture.png";
  pattern->image = image;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("assets/texture.png"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_filepath.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextBoxElement) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {280, 180};
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Text inside a text box with wrapping";
  text->position = {10, 30};
  text->fontFamily = "Arial";
  text->fontSize = 18;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.1f, 0.1f, 0.1f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(textBox);
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_text_box.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_EmptyGradientStops) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<linearGradient"), std::string::npos);
  EXPECT_NE(svg.find("/>"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_empty_gradient.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_LayerBlendMode) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->blendMode = pagx::BlendMode::Multiply;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.6f, 0.2f, 0.8f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("mix-blend-mode:multiply"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_blend_mode.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_CustomData) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->customData["name"] = "test-layer";
  layer->customData["index"] = "42";
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("data-name=\"test-layer\""), std::string::npos);
  EXPECT_NE(svg.find("data-index=\"42\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_custom_data.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskCircle = doc->makeNode<pagx::Ellipse>();
  maskCircle->position = {100, 100};
  maskCircle->size = {120, 120};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskCircle);
  maskLayer->contents.push_back(maskFill);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<mask"), std::string::npos);
  EXPECT_NE(svg.find("mask-type:alpha"), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_alpha.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskContour) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.3f, 0.3f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {100, 100};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(maskFill);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<clipPath"), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_contour.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskLuminance) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.4f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {100, 100};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Luminance;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<mask"), std::string::npos);
  EXPECT_EQ(svg.find("mask-type:alpha"), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_luminance.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_InvisibleLayer) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->visible = false;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeSquareBevel) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(20, 50);
  pathData->lineTo(150, 20);
  pathData->lineTo(280, 50);
  path->data = pathData;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;
  stroke->width = 8.0f;
  stroke->cap = pagx::LineCap::Square;
  stroke->join = pagx::LineJoin::Bevel;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-linecap=\"square\""), std::string::npos);
  EXPECT_NE(svg.find("stroke-linejoin=\"bevel\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_square_bevel.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeMiterLimit) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(20, 80);
  pathData->lineTo(150, 10);
  pathData->lineTo(280, 80);
  path->data = pathData;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;
  stroke->width = 6.0f;
  stroke->join = pagx::LineJoin::Miter;
  stroke->miterLimit = 10.0f;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-miterlimit=\"10\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_miter_limit.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_GradientTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  grad->matrix = pagx::Matrix::Rotate(45.0f);
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("gradientTransform="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_gradient_transform.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ShadowOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  auto shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->blurX = 5;
  shadow->blurY = 5;
  shadow->offsetX = 3;
  shadow->offsetY = 3;
  shadow->color = {0, 0, 0, 0.5f};
  shadow->shadowOnly = true;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(shadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<filter"), std::string::npos);
  EXPECT_EQ(svg.find("SourceGraphic"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_shadow_only.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MultipleShadows) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.8f, 0.8f, 1.0f};
  fill->color = solid;
  auto shadow1 = doc->makeNode<pagx::DropShadowFilter>();
  shadow1->blurX = 4;
  shadow1->blurY = 4;
  shadow1->offsetX = 3;
  shadow1->offsetY = 3;
  shadow1->color = {0, 0, 0, 0.4f};
  auto shadow2 = doc->makeNode<pagx::InnerShadowFilter>();
  shadow2->blurX = 3;
  shadow2->blurY = 3;
  shadow2->offsetX = 1;
  shadow2->offsetY = 1;
  shadow2->color = {0, 0, 0, 0.3f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(shadow1);
  layer->filters.push_back(shadow2);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<feMerge"), std::string::npos);
  size_t mergeNodeCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("feMergeNode", pos)) != std::string::npos) {
    mergeNodeCount++;
    pos++;
  }
  EXPECT_GE(mergeNodeCount, 3u);
  SaveFile(svg, "PAGXSVGTest/svg_export_multiple_shadows.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_EmptyLayerSelfClosing) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->id = "empty-layer";
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("id=\"empty-layer\""), std::string::npos);
  EXPECT_NE(svg.find("/>"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeOpacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 0.5f};
  stroke->color = solid;
  stroke->width = 4.0f;
  stroke->alpha = 0.8f;
  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-opacity="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_opacity.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextAnchorEnd) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Right aligned";
  text->position = {280, 50};
  text->fontFamily = "Arial";
  text->fontSize = 20;
  text->textAnchor = pagx::TextAnchor::End;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("text-anchor=\"end\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_text_anchor_end.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextBoldItalic) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Bold Italic Text";
  text->position = {150, 50};
  text->fontFamily = "Arial";
  text->fontSize = 20;
  text->fontStyle = "BoldItalic";
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("font-weight=\"bold\""), std::string::npos);
  EXPECT_NE(svg.find("font-style=\"italic\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_text_bold_italic.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextLetterSpacing) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Spaced Out";
  text->position = {150, 50};
  text->fontFamily = "Arial";
  text->fontSize = 20;
  text->letterSpacing = 5.0f;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("letter-spacing=\"5\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_text_letter_spacing.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_EmptyPath) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  path->data = pathData;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_EmptyText) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "";
  text->fontSize = 20;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_EQ(svg.find("<text"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_NullFillColor) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = nullptr;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("fill=\"none\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskNestedChildren) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.4f, 0.7f, 1.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->matrix = pagx::Matrix::Translate(10, 10);
  auto maskChild = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskChild->contents.push_back(maskRect);
  maskChild->contents.push_back(maskFill);
  maskLayer->children.push_back(maskChild);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<mask"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_nested.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ContourMaskNested) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.5f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->matrix = pagx::Matrix::Translate(20, 20);
  auto childLayer = doc->makeNode<pagx::Layer>();
  childLayer->matrix = pagx::Matrix::Scale(1.2f, 1.2f);
  auto maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {80, 80};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  childLayer->contents.push_back(maskEllipse);
  childLayer->contents.push_back(maskFill);
  maskLayer->children.push_back(childLayer);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<clipPath"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_contour_mask_nested.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_GradientStopOpacity) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 100};
  grad->endPoint = {190, 100};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 0.3f};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 0.7f};
  grad->colorStops = {s1, s2};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stop-opacity="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_gradient_stop_opacity.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_InnerShadowShadowOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.9f, 0.9f, 1.0f};
  fill->color = solid;
  auto innerShadow = doc->makeNode<pagx::InnerShadowFilter>();
  innerShadow->blurX = 5;
  innerShadow->blurY = 5;
  innerShadow->offsetX = 2;
  innerShadow->offsetY = 2;
  innerShadow->color = {0, 0, 0, 0.4f};
  innerShadow->shadowOnly = true;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(innerShadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feComposite"), std::string::npos);
  EXPECT_EQ(svg.find("SourceGraphic"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_inner_shadow_only.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ToFileInvalidPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  EXPECT_FALSE(pagx::SVGExporter::ToFile(*doc, "/nonexistent/dir/file.svg"));
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_DecodeUTF8_ASCII) {
  const char* data = "A";
  int32_t unichar = 0;
  auto consumed = pagx::SVGDecodeUTF8Char(data, 1, &unichar);
  EXPECT_EQ(consumed, 1u);
  EXPECT_EQ(unichar, 'A');
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_DecodeUTF8_MultiByte) {
  // U+4E2D '中' = 0xE4 0xB8 0xAD (3 bytes)
  const char data[] = {'\xE4', '\xB8', '\xAD', '\0'};
  int32_t unichar = 0;
  auto consumed = pagx::SVGDecodeUTF8Char(data, 3, &unichar);
  EXPECT_EQ(consumed, 3u);
  EXPECT_EQ(unichar, 0x4E2D);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_DecodeUTF8_Empty) {
  int32_t unichar = -1;
  auto consumed = pagx::SVGDecodeUTF8Char("", 0, &unichar);
  EXPECT_EQ(consumed, 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_DecodeUTF8_Invalid) {
  // 0xFF is not a valid UTF-8 leading byte
  const char data[] = {'\xFF'};
  int32_t unichar = 0;
  auto consumed = pagx::SVGDecodeUTF8Char(data, 1, &unichar);
  EXPECT_EQ(consumed, 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_EstimateCharAdvance) {
  float fontSize = 20.0f;
  // CJK → full em-width
  EXPECT_FLOAT_EQ(pagx::EstimateCharAdvance(0x4E2D, fontSize), fontSize);
  // Space → 0.25em
  EXPECT_FLOAT_EQ(pagx::EstimateCharAdvance(' ', fontSize), fontSize * 0.25f);
  // Tab → 4em
  EXPECT_FLOAT_EQ(pagx::EstimateCharAdvance('\t', fontSize), fontSize * 4.0f);
  // Latin → 0.6em
  EXPECT_FLOAT_EQ(pagx::EstimateCharAdvance('A', fontSize), fontSize * 0.6f);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_BreakTextIntoLines_SingleLine) {
  std::vector<pagx::SVGCharInfo> chars = {
      {'H', 0, 1, 12.0f},
      {'i', 1, 1, 12.0f},
  };
  auto lines = pagx::BreakTextIntoLines(chars, 100);
  EXPECT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].charStart, 0u);
  EXPECT_EQ(lines[0].charCount, 2u);
  EXPECT_FLOAT_EQ(lines[0].width, 24.0f);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_BreakTextIntoLines_ExplicitNewline) {
  std::vector<pagx::SVGCharInfo> chars = {
      {'A', 0, 1, 10.0f},
      {'\n', 1, 1, 0.0f},
      {'B', 2, 1, 10.0f},
  };
  auto lines = pagx::BreakTextIntoLines(chars, 100);
  EXPECT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0].charCount, 1u);
  EXPECT_FLOAT_EQ(lines[0].width, 10.0f);
  EXPECT_EQ(lines[1].charStart, 2u);
  EXPECT_EQ(lines[1].charCount, 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_BreakTextIntoLines_WordWrap) {
  // "AB CD" with each char advance=10, boxWidth=35 → break after space
  std::vector<pagx::SVGCharInfo> chars = {
      {'A', 0, 1, 10.0f}, {'B', 1, 1, 10.0f}, {' ', 2, 1, 5.0f},
      {'C', 3, 1, 10.0f}, {'D', 4, 1, 10.0f},
  };
  auto lines = pagx::BreakTextIntoLines(chars, 35);
  EXPECT_GE(lines.size(), 2u);
  EXPECT_GT(lines[0].charCount, 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_BreakTextIntoLines_ForcedBreak) {
  // Each char is 15px wide, boxWidth=20 → first char fits, second forces break
  std::vector<pagx::SVGCharInfo> chars = {
      {'X', 0, 1, 15.0f},
      {'Y', 1, 1, 15.0f},
      {'Z', 2, 1, 15.0f},
  };
  auto lines = pagx::BreakTextIntoLines(chars, 20);
  EXPECT_GE(lines.size(), 2u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_BreakTextIntoLines_BreakBetweenCJK) {
  // CJK characters can break between each other
  std::vector<pagx::SVGCharInfo> chars = {
      {0x4E2D, 0, 3, 20.0f},
      {0x6587, 3, 3, 20.0f},
      {0x5B57, 6, 3, 20.0f},
  };
  auto lines = pagx::BreakTextIntoLines(chars, 45);
  EXPECT_GE(lines.size(), 2u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ExtractLineText) {
  std::string fullText = "Hello World";
  std::vector<pagx::SVGCharInfo> chars = {
      {'H', 0, 1, 0}, {'e', 1, 1, 0}, {'l', 2, 1, 0},  {'l', 3, 1, 0},
      {'o', 4, 1, 0}, {' ', 5, 1, 0}, {'W', 6, 1, 0},  {'o', 7, 1, 0},
      {'r', 8, 1, 0}, {'l', 9, 1, 0}, {'d', 10, 1, 0},
  };
  pagx::SVGTextLine line = {6, 5, 0};
  EXPECT_EQ(pagx::ExtractLineText(fullText, chars, line), "World");

  pagx::SVGTextLine emptyLine = {0, 0, 0};
  EXPECT_EQ(pagx::ExtractLineText(fullText, chars, emptyLine), "");
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_MultiLine) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {100, 180};
  textBox->wordWrap = true;
  textBox->textAlign = pagx::TextAlign::Start;
  textBox->paragraphAlign = pagx::ParagraphAlign::Near;

  std::string content = "Hello World Test";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_TRUE(result.isMultiLine);
  EXPECT_GT(result.lines.size(), 0u);
  EXPECT_GT(result.lineHeight, 0.0f);
  EXPECT_FLOAT_EQ(result.x, 10.0f);
  EXPECT_EQ(result.anchor, pagx::TextAnchor::Start);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_TextAlignCenter) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 100};
  textBox->wordWrap = true;
  textBox->textAlign = pagx::TextAlign::Center;

  std::string content = "Center";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_FLOAT_EQ(result.x, 10.0f + 200.0f / 2.0f);
  EXPECT_EQ(result.anchor, pagx::TextAnchor::Center);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_TextAlignEnd) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 100};
  textBox->wordWrap = true;
  textBox->textAlign = pagx::TextAlign::End;

  std::string content = "End";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_FLOAT_EQ(result.x, 10.0f + 200.0f);
  EXPECT_EQ(result.anchor, pagx::TextAnchor::End);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_ParagraphAlignMiddle) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 200};
  textBox->wordWrap = true;
  textBox->paragraphAlign = pagx::ParagraphAlign::Middle;

  std::string content = "Center vertically";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_TRUE(result.isMultiLine);
  EXPECT_GT(result.firstLineY, textBox->position.y);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_ParagraphAlignFar) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 200};
  textBox->wordWrap = true;
  textBox->paragraphAlign = pagx::ParagraphAlign::Far;

  std::string content = "Bottom aligned";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_TRUE(result.isMultiLine);
  EXPECT_GT(result.firstLineY, textBox->position.y + 100);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_CustomLineHeight) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 200};
  textBox->wordWrap = true;
  textBox->lineHeight = 30.0f;

  std::string content = "Custom line height";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_FLOAT_EQ(result.lineHeight, 30.0f);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_SingleLineParagraphAlign) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 100};
  textBox->wordWrap = false;

  std::string content = "Single line";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {50, 50};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  // ParagraphAlign::Near (default)
  textBox->paragraphAlign = pagx::ParagraphAlign::Near;
  auto resultNear = pagx::ComputeTextLayout(params);
  EXPECT_FALSE(resultNear.isMultiLine);
  EXPECT_FLOAT_EQ(resultNear.y, textBox->position.y + 16.0f);

  // ParagraphAlign::Middle
  textBox->paragraphAlign = pagx::ParagraphAlign::Middle;
  auto resultMid = pagx::ComputeTextLayout(params);
  EXPECT_FLOAT_EQ(resultMid.y, textBox->position.y + textBox->size.height / 2 + 16.0f * 0.35f);

  // ParagraphAlign::Far
  textBox->paragraphAlign = pagx::ParagraphAlign::Far;
  auto resultFar = pagx::ComputeTextLayout(params);
  EXPECT_FLOAT_EQ(resultFar.y, textBox->position.y + textBox->size.height);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_NoTextBox) {
  std::string content = "No box";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 0.0f;
  params.position = {30, 40};
  params.textAnchor = pagx::TextAnchor::Center;
  params.textBox = nullptr;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_FALSE(result.isMultiLine);
  EXPECT_FLOAT_EQ(result.x, 30.0f);
  EXPECT_FLOAT_EQ(result.y, 40.0f);
  EXPECT_EQ(result.anchor, pagx::TextAnchor::Center);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeTextLayout_UTF8Content) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {0, 0};
  textBox->size = {100, 200};
  textBox->wordWrap = true;

  std::string content = "Hello\n世界";
  pagx::SVGTextLayoutParams params = {};
  params.text = &content;
  params.fontSize = 16.0f;
  params.letterSpacing = 2.0f;
  params.position = {0, 0};
  params.textAnchor = pagx::TextAnchor::Start;
  params.textBox = textBox;

  auto result = pagx::ComputeTextLayout(params);
  EXPECT_TRUE(result.isMultiLine);
  EXPECT_GE(result.lines.size(), 2u);
  EXPECT_FALSE(result.chars.empty());
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_Basic) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->lineTo(500, -700);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};  // glyphID 1 → index 0

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 10.0f, 50.0f);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].pathData, pathData);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_SkipGlyphID0) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 0);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 500.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 16.0f;
  run->glyphs = {0, 1};  // 0 = missing glyph, 1 = valid

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_OutOfRangeGlyphID) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 0);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 500.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 16.0f;
  run->glyphs = {99};  // out of range

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_EmptyPath) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto emptyPath = doc->makeNode<pagx::PathData>();  // empty path

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = emptyPath;
  glyph->advance = 500.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 16.0f;
  run->glyphs = {1};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_NullFont) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = nullptr;
  run->glyphs = {1};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_EmptyGlyphs) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->glyphs = {};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 0u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_WithPositions) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->lineTo(500, -700);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->positions = {{5.0f, 3.0f}};
  run->xOffsets = {2.0f};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 10.0f, 50.0f);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_XOffsetsOnly) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->xOffsets = {5.0f};
  // no positions

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 10.0f, 50.0f);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_WithRotation) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->lineTo(500, -700);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->rotations = {45.0f};
  run->anchors = {{10.0f, 5.0f}};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_WithScale) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->scales = {{2.0f, 0.5f}};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_WithSkew) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1};
  run->skews = {15.0f};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_CombinedTransforms) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->lineTo(500, -700);
  pathData->close();

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = pathData;
  glyph->advance = 600.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 24.0f;
  run->glyphs = {1};
  run->rotations = {30.0f};
  run->scales = {{1.5f, 0.8f}};
  run->skews = {10.0f};
  run->anchors = {{5.0f, -3.0f}};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 10.0f, 20.0f);
  EXPECT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_MultipleGlyphs) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto pathData1 = doc->makeNode<pagx::PathData>();
  pathData1->moveTo(0, 0);
  pathData1->lineTo(400, 0);
  pathData1->lineTo(400, -600);
  pathData1->close();

  auto pathData2 = doc->makeNode<pagx::PathData>();
  pathData2->moveTo(0, 0);
  pathData2->lineTo(300, 0);
  pathData2->lineTo(300, -500);
  pathData2->close();

  auto glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = pathData1;
  glyph1->advance = 500.0f;

  auto glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = pathData2;
  glyph2->advance = 400.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph1);
  font->glyphs.push_back(glyph2);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20.0f;
  run->glyphs = {1, 2};  // Both valid glyphs

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 2u);
}

PAGX_TEST(PAGXSVGTest, SVGTextLayout_ComputeGlyphPaths_NullGlyphPath) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = nullptr;
  glyph->advance = 500.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 16.0f;
  run->glyphs = {1};

  auto text = doc->makeNode<pagx::Text>();
  text->glyphRuns.push_back(run);

  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_EQ(result.size(), 0u);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextAsPathOutput) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "AB";
  text->position = {10, 50};
  text->fontSize = 24;

  auto pathData1 = doc->makeNode<pagx::PathData>();
  pathData1->moveTo(0, 0);
  pathData1->lineTo(500, 0);
  pathData1->lineTo(500, -700);
  pathData1->close();
  auto glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = pathData1;
  glyph1->advance = 600.0f;

  auto pathData2 = doc->makeNode<pagx::PathData>();
  pathData2->moveTo(0, 0);
  pathData2->lineTo(400, 0);
  pathData2->lineTo(400, -600);
  pathData2->close();
  auto glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = pathData2;
  glyph2->advance = 500.0f;

  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  font->glyphs.push_back(glyph1);
  font->glyphs.push_back(glyph2);

  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 24.0f;
  run->glyphs = {1, 2};
  text->glyphRuns.push_back(run);

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("<g"), std::string::npos);
  EXPECT_EQ(svg.find("<text"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_text_as_path_output.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextAsPathFallbackToText) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto text = doc->makeNode<pagx::Text>();
  text->text = "No glyphs";
  text->position = {10, 50};
  text->fontFamily = "Arial";
  text->fontSize = 20;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("No glyphs"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MultiLineTextSVGOutput) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {80, 180};
  textBox->wordWrap = true;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello World Testing";
  text->position = {10, 30};
  text->fontFamily = "Arial";
  text->fontSize = 16;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(textBox);
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  size_t textCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("<text", pos)) != std::string::npos) {
    textCount++;
    pos++;
  }
  EXPECT_GT(textCount, 1u);
  SaveFile(svg, "PAGXSVGTest/svg_export_multiline_text_output.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MultiLineTextNewline) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {10, 10};
  textBox->size = {200, 180};
  textBox->wordWrap = true;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Line1\nLine2\nLine3";
  text->fontFamily = "Arial";
  text->fontSize = 14;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(textBox);
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("Line1"), std::string::npos);
  EXPECT_NE(svg.find("Line2"), std::string::npos);
  EXPECT_NE(svg.find("Line3"), std::string::npos);
  size_t textCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("<text", pos)) != std::string::npos) {
    textCount++;
    pos++;
  }
  EXPECT_GE(textCount, 3u);
  SaveFile(svg, "PAGXSVGTest/svg_export_multiline_newline.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternTilingFallback) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  std::vector<uint8_t> invalidData = {0x00, 0x01, 0x02, 0x03};
  image->data = pagx::Data::MakeWithCopy(invalidData.data(), invalidData.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = pagx::Matrix::Translate(5, 10);
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
  EXPECT_NE(svg.find("width=\"100%\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"100%\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_tiling_fallback.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFallbackIdentityMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  std::vector<uint8_t> badData(30, 0x00);
  image->data = pagx::Data::MakeWithCopy(badData.data(), badData.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
  EXPECT_NE(svg.find("<image"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_fallback_identity.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFallbackWithMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  std::vector<uint8_t> badData(30, 0x00);
  image->data = pagx::Data::MakeWithCopy(badData.data(), badData.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->matrix = pagx::Matrix::Scale(2.0f, 2.0f);
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
  size_t imageTransform = svg.find("<image");
  ASSERT_NE(imageTransform, std::string::npos);
  auto afterImage = svg.substr(imageTransform, svg.find("/>", imageTransform) - imageTransform);
  EXPECT_NE(afterImage.find("transform="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_fallback_matrix.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskWithNamedId) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 1.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "custom-mask";
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {100, 100};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("id=\"custom-mask\""), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#custom-mask)\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_named_id.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ContourMaskWithNamedId) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.5f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "custom-clip";
  auto maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {80, 80};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(maskFill);

  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("id=\"custom-clip\""), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#custom-clip)\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_contour_mask_named_id.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_InvisibleLayerWithMask) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->visible = false;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<mask"), std::string::npos);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupPassThrough) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.8f, 0.3f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_EQ(svg.find("<g"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupWithAlphaOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->alpha = 0.5f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<g"), std::string::npos);
  EXPECT_NE(svg.find("opacity=\"0.5\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PNGDimensionsBadSignature) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  std::vector<uint8_t> fakePNG(30, 0x42);
  image->data = pagx::Data::MakeWithCopy(fakePNG.data(), fakePNG.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PNGDimensionsZeroWidth) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  auto pngData = MakeMinimalPNG(0, 64);
  image->data = pagx::Data::MakeWithCopy(pngData.data(), pngData.size());
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternNonTilingOBB) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  auto pngData = MakeMinimalPNG(64, 64);
  image->data = pagx::Data::MakeWithCopy(pngData.data(), pngData.size());
  pattern->image = image;
  pattern->matrix = pagx::Matrix::Translate(10, 20);
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternContentUnits=\"objectBoundingBox\""), std::string::npos);
  EXPECT_NE(svg.find("width=\"1\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"1\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_image_pattern_non_tiling_obb.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeDefaultWidth) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  stroke->color = solid;
  stroke->width = 1.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke=\"#"), std::string::npos);
  EXPECT_EQ(svg.find("stroke-width="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeMiterDefaultLimit) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(20, 80);
  pathData->lineTo(100, 10);
  pathData->lineTo(180, 80);
  path->data = pathData;
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  stroke->color = solid;
  stroke->width = 4.0f;
  stroke->join = pagx::LineJoin::Miter;
  stroke->miterLimit = 4.0f;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("stroke-miterlimit="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeNullColor) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto stroke = doc->makeNode<pagx::Stroke>();
  stroke->color = nullptr;
  stroke->width = 3.0f;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke=\"none\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 100};
  grad->endPoint = {190, 100};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  stroke->color = grad;
  stroke->width = 4.0f;
  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = nullptr;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke=\"url(#grad"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_gradient.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_RadialGradientWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {100, 100};
  grad->radius = 90;
  grad->matrix = pagx::Matrix::Scale(1.5f, 0.8f);
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 1, 1, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 0, 1};
  grad->colorStops = {s1, s2};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<radialGradient"), std::string::npos);
  EXPECT_NE(svg.find("gradientTransform="), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_radial_gradient_transform.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFilepathTiling) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto fill = doc->makeNode<pagx::Fill>();
  auto pattern = doc->makeNode<pagx::ImagePattern>();
  auto image = doc->makeNode<pagx::Image>();
  image->filePath = "textures/tile.png";
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("patternUnits=\"userSpaceOnUse\""), std::string::npos);
  EXPECT_NE(svg.find("textures/tile.png"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_IndentOption) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.indent = 4;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("    <rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_DisplayP3StrokeOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 150};
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto strokeColor = doc->makeNode<pagx::SolidColor>();
  strokeColor->color = {0.9f, 0.1f, 0.4f, 1.0f};
  strokeColor->color.colorSpace = pagx::ColorSpace::DisplayP3;
  stroke->color = strokeColor;
  stroke->width = 4.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("display-p3"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PathWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->position = {50, 50};
  auto path = doc->makeNode<pagx::Path>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(50, 0);
  pathData->lineTo(50, 50);
  pathData->close();
  path->data = pathData;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;
  group->elements.push_back(path);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_EllipseWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->position = {30, 30};
  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {50, 50};
  ellipse->size = {80, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.2f, 0.2f, 1.0f};
  fill->color = solid;
  group->elements.push_back(ellipse);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<ellipse"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RectangleWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->position = {30, 30};
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TextWithTransform) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->position = {50, 10};
  auto text = doc->makeNode<pagx::Text>();
  text->text = "Transformed";
  text->position = {0, 30};
  text->fontFamily = "Arial";
  text->fontSize = 18;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MultipleLayersNoGroup) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto layer1 = doc->makeNode<pagx::Layer>();
  auto rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->position = {60, 60};
  rect1->size = {80, 80};
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1, 0, 0, 1};
  fill1->color = solid1;
  layer1->contents.push_back(rect1);
  layer1->contents.push_back(fill1);
  doc->layers.push_back(layer1);

  auto layer2 = doc->makeNode<pagx::Layer>();
  auto rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {140, 140};
  rect2->size = {80, 80};
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0, 0, 1, 1};
  fill2->color = solid2;
  layer2->contents.push_back(rect2);
  layer2->contents.push_back(fill2);
  doc->layers.push_back(layer2);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  size_t rectCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("<rect", pos)) != std::string::npos) {
    rectCount++;
    pos++;
  }
  EXPECT_EQ(rectCount, 2u);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupScaleOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->scale = {2.0f, 2.0f};
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {40, 40};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupAnchorOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {50, 50};
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.3f, 0.8f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupRotationOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->rotation = 45.0f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.2f, 0.5f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupPositionOnly) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->position = {50, 50};
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.4f, 0.7f, 0.1f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("matrix("), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PathNullData) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto path = doc->makeNode<pagx::Path>();
  path->data = nullptr;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1, 0, 0, 1};
  fill->color = solid;
  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_DropShadowZeroOffset) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  auto shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->blurX = 5;
  shadow->blurY = 5;
  shadow->offsetX = 0;
  shadow->offsetY = 0;
  shadow->color = {0, 0, 0, 0.5f};
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(shadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feGaussianBlur"), std::string::npos);
  EXPECT_NE(svg.find("feOffset"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_BlendFilterNormal) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.5f, 1.0f};
  fill->color = solid;
  auto blend = doc->makeNode<pagx::BlendFilter>();
  blend->color = {1.0f, 0.0f, 0.0f, 1.0f};
  blend->blendMode = pagx::BlendMode::Normal;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->filters.push_back(blend);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("feFlood"), std::string::npos);
  EXPECT_NE(svg.find("feBlend"), std::string::npos);
  EXPECT_EQ(svg.find("flood-opacity="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_NestedGroupTransform) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  auto outerGroup = doc->makeNode<pagx::Group>();
  outerGroup->position = {100, 100};
  auto innerGroup = doc->makeNode<pagx::Group>();
  innerGroup->scale = {0.5f, 0.5f};
  innerGroup->alpha = 0.8f;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.6f, 1.0f};
  fill->color = solid;
  innerGroup->elements.push_back(rect);
  innerGroup->elements.push_back(fill);
  outerGroup->elements.push_back(innerGroup);
  layer->contents.push_back(outerGroup);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_NE(svg.find("opacity=\"0.8\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_nested_group_transform.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_RectangleNoRoundness) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 120};
  rect->roundness = 0;
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_EQ(svg.find("rx="), std::string::npos);
  EXPECT_EQ(svg.find("ry="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_LayerWithAllFeatures) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->id = "full-featured";
  layer->alpha = 0.9f;
  layer->matrix = pagx::Matrix::Translate(10, 10);
  layer->x = 5;
  layer->y = 5;
  layer->blendMode = pagx::BlendMode::Screen;
  layer->customData["type"] = "main";

  auto blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 2;
  blur->blurY = 2;
  layer->filters.push_back(blur);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {150, 150};
  maskRect->size = {200, 200};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1, 1, 1, 1};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.4f, 0.6f, 0.8f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("id=\"full-featured\""), std::string::npos);
  EXPECT_NE(svg.find("opacity=\"0.9\""), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
  EXPECT_NE(svg.find("data-type=\"main\""), std::string::npos);
  EXPECT_NE(svg.find("filter=\"url(#"), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#"), std::string::npos);
  EXPECT_NE(svg.find("mix-blend-mode:screen"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_all_features.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_EmptyGradientRadial) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {100, 100};
  grad->radius = 80;
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<radialGradient"), std::string::npos);
  EXPECT_NE(svg.find("/>"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_FillWithGradientAndP3NotApplied) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};
  auto fill = doc->makeNode<pagx::Fill>();
  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("url(#grad"), std::string::npos);
  EXPECT_EQ(svg.find("display-p3"), std::string::npos);
}

}  // namespace pag
