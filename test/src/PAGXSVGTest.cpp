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

}  // namespace pag
