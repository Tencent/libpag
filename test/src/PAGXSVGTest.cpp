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
#include "pagx/utils/ExporterUtils.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/types/Data.h"
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

// ===========================================================================
// ExporterUtils tests
// ===========================================================================

PAGX_TEST(PAGXSVGTest, CollectFillStroke_Empty) {
  std::vector<pagx::Element*> contents;
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, nullptr);
  EXPECT_EQ(info.stroke, nullptr);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXSVGTest, CollectFillStroke_FillOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill = doc->makeNode<pagx::Fill>();
  std::vector<pagx::Element*> contents = {fill};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill);
  EXPECT_EQ(info.stroke, nullptr);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXSVGTest, CollectFillStroke_StrokeOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto stroke = doc->makeNode<pagx::Stroke>();
  std::vector<pagx::Element*> contents = {stroke};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, nullptr);
  EXPECT_EQ(info.stroke, stroke);
  EXPECT_EQ(info.textBox, nullptr);
}

PAGX_TEST(PAGXSVGTest, CollectFillStroke_AllThree) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill = doc->makeNode<pagx::Fill>();
  auto stroke = doc->makeNode<pagx::Stroke>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  std::vector<pagx::Element*> contents = {fill, stroke, textBox};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill);
  EXPECT_EQ(info.stroke, stroke);
  EXPECT_EQ(info.textBox, textBox);
}

PAGX_TEST(PAGXSVGTest, CollectFillStroke_DuplicatesKeepsFirst) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto fill1 = doc->makeNode<pagx::Fill>();
  auto fill2 = doc->makeNode<pagx::Fill>();
  auto stroke1 = doc->makeNode<pagx::Stroke>();
  auto stroke2 = doc->makeNode<pagx::Stroke>();
  std::vector<pagx::Element*> contents = {fill1, stroke1, fill2, stroke2};
  auto info = pagx::CollectFillStroke(contents);
  EXPECT_EQ(info.fill, fill1);
  EXPECT_EQ(info.stroke, stroke1);
}

PAGX_TEST(PAGXSVGTest, BuildLayerMatrix_Identity) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_TRUE(m.isIdentity());
}

PAGX_TEST(PAGXSVGTest, BuildLayerMatrix_WithPosition) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 30.0f;
  layer->y = 50.0f;
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_FLOAT_EQ(m.tx, 30.0f);
  EXPECT_FLOAT_EQ(m.ty, 50.0f);
  EXPECT_FLOAT_EQ(m.a, 1.0f);
  EXPECT_FLOAT_EQ(m.d, 1.0f);
}

PAGX_TEST(PAGXSVGTest, BuildLayerMatrix_WithMatrix) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Scale(2.0f, 3.0f);
  auto m = pagx::BuildLayerMatrix(layer);
  EXPECT_FLOAT_EQ(m.a, 2.0f);
  EXPECT_FLOAT_EQ(m.d, 3.0f);
  EXPECT_FLOAT_EQ(m.tx, 0.0f);
  EXPECT_FLOAT_EQ(m.ty, 0.0f);
}

PAGX_TEST(PAGXSVGTest, BuildLayerMatrix_PositionAndMatrix) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->x = 10.0f;
  layer->y = 20.0f;
  layer->matrix = pagx::Matrix::Scale(2.0f, 2.0f);
  auto m = pagx::BuildLayerMatrix(layer);
  // Translate(10,20) * Scale(2,2) => a=2, d=2, tx=10, ty=20
  EXPECT_FLOAT_EQ(m.a, 2.0f);
  EXPECT_FLOAT_EQ(m.d, 2.0f);
  EXPECT_FLOAT_EQ(m.tx, 10.0f);
  EXPECT_FLOAT_EQ(m.ty, 20.0f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_Identity) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_TRUE(m.isIdentity());
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_PositionOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->position = {50.0f, 100.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.tx, 50.0f);
  EXPECT_FLOAT_EQ(m.ty, 100.0f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_AnchorOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {25.0f, 30.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.tx, -25.0f);
  EXPECT_FLOAT_EQ(m.ty, -30.0f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_ScaleOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->scale = {2.0f, 3.0f};
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_FLOAT_EQ(m.a, 2.0f);
  EXPECT_FLOAT_EQ(m.d, 3.0f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_RotationOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->rotation = 90.0f;
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_NEAR(m.a, 0.0f, 1e-5f);
  EXPECT_NEAR(m.b, 1.0f, 1e-5f);
  EXPECT_NEAR(m.c, -1.0f, 1e-5f);
  EXPECT_NEAR(m.d, 0.0f, 1e-5f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_SkewOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->skew = 45.0f;
  auto m = pagx::BuildGroupMatrix(group);
  EXPECT_NEAR(m.c, std::tan(45.0f * 3.14159265358979323846f / 180.0f), 1e-5f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_AnchorAndPosition) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {10.0f, 20.0f};
  group->position = {50.0f, 60.0f};
  auto m = pagx::BuildGroupMatrix(group);
  // Translate(50,60) * Translate(-10,-20) => tx=40, ty=40
  EXPECT_FLOAT_EQ(m.tx, 40.0f);
  EXPECT_FLOAT_EQ(m.ty, 40.0f);
}

PAGX_TEST(PAGXSVGTest, BuildGroupMatrix_ScaleAndAnchor) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto group = doc->makeNode<pagx::Group>();
  group->anchor = {50.0f, 50.0f};
  group->scale = {2.0f, 2.0f};
  auto m = pagx::BuildGroupMatrix(group);
  // Scale(2,2) * Translate(-50,-50) => a=2, d=2, tx=-100, ty=-100
  EXPECT_FLOAT_EQ(m.a, 2.0f);
  EXPECT_FLOAT_EQ(m.d, 2.0f);
  EXPECT_FLOAT_EQ(m.tx, -100.0f);
  EXPECT_FLOAT_EQ(m.ty, -100.0f);
}

PAGX_TEST(PAGXSVGTest, DetectMaskFillRule_WindingDefault) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::Winding;
  layer->contents.push_back(fill);
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::Winding);
}

PAGX_TEST(PAGXSVGTest, DetectMaskFillRule_EvenOdd) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  layer->contents.push_back(fill);
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::EvenOdd);
}

PAGX_TEST(PAGXSVGTest, DetectMaskFillRule_Empty) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  EXPECT_EQ(pagx::DetectMaskFillRule(layer), pagx::FillRule::Winding);
}

PAGX_TEST(PAGXSVGTest, DetectMaskFillRule_NestedEvenOdd) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto parent = doc->makeNode<pagx::Layer>();
  auto child = doc->makeNode<pagx::Layer>();
  auto fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  child->contents.push_back(fill);
  parent->children.push_back(child);
  EXPECT_EQ(pagx::DetectMaskFillRule(parent), pagx::FillRule::EvenOdd);
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensions_Valid) {
  // Construct a minimal valid PNG header: 8-byte signature + IHDR chunk (4 len + 4 type + 8 data)
  uint8_t header[24] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  // PNG signature
      0x00, 0x00, 0x00, 0x0D,                            // IHDR length (13)
      0x49, 0x48, 0x44, 0x52,                            // "IHDR"
      0x00, 0x00, 0x01, 0x00,                            // width = 256
      0x00, 0x00, 0x00, 0x80                             // height = 128
  };
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetPNGDimensions(header, 24, &w, &h));
  EXPECT_EQ(w, 256);
  EXPECT_EQ(h, 128);
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensions_TooSmall) {
  uint8_t data[10] = {};
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(data, 10, &w, &h));
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensions_BadSignature) {
  uint8_t data[24] = {};
  data[0] = 0x00;
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(data, 24, &w, &h));
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensions_ZeroDimension) {
  uint8_t header[24] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
      0x00, 0x00, 0x00, 0x0D,
      0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x00,  // width = 0
      0x00, 0x00, 0x00, 0x80   // height = 128
  };
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensions(header, 24, &w, &h));
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensionsFromPath_ValidFile) {
  auto pngPath = ProjectPath::Absolute("resources/apitest/imageReplacement.png");
  int w = 0, h = 0;
  if (std::filesystem::exists(pngPath)) {
    EXPECT_TRUE(pagx::GetPNGDimensionsFromPath(pngPath, &w, &h));
    EXPECT_GT(w, 0);
    EXPECT_GT(h, 0);
  }
}

PAGX_TEST(PAGXSVGTest, GetPNGDimensionsFromPath_InvalidFile) {
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetPNGDimensionsFromPath("/nonexistent/path.png", &w, &h));
}

PAGX_TEST(PAGXSVGTest, GetImagePNGDimensions_WithData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t header[24] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
      0x00, 0x00, 0x00, 0x0D,
      0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x64,  // width = 100
      0x00, 0x00, 0x00, 0xC8   // height = 200
  };
  image->data = pagx::Data::MakeWithCopy(header, 24);
  int w = 0, h = 0;
  EXPECT_TRUE(pagx::GetImagePNGDimensions(image, &w, &h));
  EXPECT_EQ(w, 100);
  EXPECT_EQ(h, 200);
}

PAGX_TEST(PAGXSVGTest, GetImagePNGDimensions_NoDataNoPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  int w = 0, h = 0;
  EXPECT_FALSE(pagx::GetImagePNGDimensions(image, &w, &h));
}

PAGX_TEST(PAGXSVGTest, GetImagePNGDimensions_WithFilePath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  auto pngPath = ProjectPath::Absolute("resources/apitest/imageReplacement.png");
  if (std::filesystem::exists(pngPath)) {
    image->filePath = pngPath;
    int w = 0, h = 0;
    EXPECT_TRUE(pagx::GetImagePNGDimensions(image, &w, &h));
    EXPECT_GT(w, 0);
    EXPECT_GT(h, 0);
  }
}

PAGX_TEST(PAGXSVGTest, IsJPEG_Valid) {
  uint8_t data[4] = {0xFF, 0xD8, 0xFF, 0xE0};
  EXPECT_TRUE(pagx::IsJPEG(data, 4));
}

PAGX_TEST(PAGXSVGTest, IsJPEG_TooSmall) {
  uint8_t data[1] = {0xFF};
  EXPECT_FALSE(pagx::IsJPEG(data, 1));
}

PAGX_TEST(PAGXSVGTest, IsJPEG_WrongSignature) {
  uint8_t data[2] = {0x89, 0x50};
  EXPECT_FALSE(pagx::IsJPEG(data, 2));
}

PAGX_TEST(PAGXSVGTest, GetImageData_WithInlineData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  uint8_t bytes[4] = {1, 2, 3, 4};
  image->data = pagx::Data::MakeWithCopy(bytes, 4);
  auto result = pagx::GetImageData(image);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->size(), 4u);
}

PAGX_TEST(PAGXSVGTest, GetImageData_WithFilePath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  auto pngPath = ProjectPath::Absolute("resources/apitest/imageReplacement.png");
  if (std::filesystem::exists(pngPath)) {
    image->filePath = pngPath;
    auto result = pagx::GetImageData(image);
    EXPECT_NE(result, nullptr);
  }
}

PAGX_TEST(PAGXSVGTest, GetImageData_Empty) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto image = doc->makeNode<pagx::Image>();
  auto result = pagx::GetImageData(image);
  EXPECT_EQ(result, nullptr);
}

PAGX_TEST(PAGXSVGTest, HasNonASCII_PureASCII) {
  EXPECT_FALSE(pagx::HasNonASCII("Hello, World!"));
}

PAGX_TEST(PAGXSVGTest, HasNonASCII_Empty) {
  EXPECT_FALSE(pagx::HasNonASCII(""));
}

PAGX_TEST(PAGXSVGTest, HasNonASCII_WithNonASCII) {
  EXPECT_TRUE(pagx::HasNonASCII("Hello, 世界"));
}

PAGX_TEST(PAGXSVGTest, HasNonASCII_Latin1) {
  EXPECT_TRUE(pagx::HasNonASCII("caf\xC3\xA9"));
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_ASCII) {
  auto hex = pagx::UTF8ToUTF16BEHex("A");
  EXPECT_EQ(hex, "0041");
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_Empty) {
  auto hex = pagx::UTF8ToUTF16BEHex("");
  EXPECT_EQ(hex, "");
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_MultipleASCII) {
  auto hex = pagx::UTF8ToUTF16BEHex("Hi");
  EXPECT_EQ(hex, "00480069");
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_TwoByte) {
  // "é" = U+00E9, UTF-8: C3 A9
  auto hex = pagx::UTF8ToUTF16BEHex("\xC3\xA9");
  EXPECT_EQ(hex, "00E9");
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_ThreeByte) {
  // "中" = U+4E2D, UTF-8: E4 B8 AD
  auto hex = pagx::UTF8ToUTF16BEHex("\xE4\xB8\xAD");
  EXPECT_EQ(hex, "4E2D");
}

PAGX_TEST(PAGXSVGTest, UTF8ToUTF16BEHex_FourByte) {
  // U+1F600 (😀), UTF-8: F0 9F 98 80 → surrogate pair D83D DE00
  auto hex = pagx::UTF8ToUTF16BEHex("\xF0\x9F\x98\x80");
  EXPECT_EQ(hex, "D83DDE00");
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_EmptyRuns) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_NullFont) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = nullptr;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_EmptyGlyphs) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_GlyphIDZeroSkipped) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20;
  run->glyphs = {0};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_OutOfBoundsGlyphID) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20;
  run->glyphs = {10};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_ValidGlyph) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(500, 0);
  pathData->lineTo(500, 700);
  pathData->close();
  glyph->path = pathData;
  glyph->advance = 600;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 10, 20);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].pathData, pathData);
  float scale = 20.0f / 1000.0f;
  EXPECT_FLOAT_EQ(result[0].transform.a, scale);
  EXPECT_FLOAT_EQ(result[0].transform.d, scale);
  EXPECT_FLOAT_EQ(result[0].transform.tx, 10.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 20.0f);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_WithPositions) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->x = 5.0f;
  run->y = 3.0f;
  run->glyphs = {1};
  run->positions = {{7.0f, 9.0f}};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 100, 200);
  ASSERT_EQ(result.size(), 1u);
  // posX = textPosX + run->x + positions[0].x = 100 + 5 + 7 = 112
  // posY = textPosY + run->y + positions[0].y = 200 + 3 + 9 = 212
  float scale = 10.0f / 1000.0f;
  EXPECT_FLOAT_EQ(result[0].transform.tx, 112.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 212.0f);
  EXPECT_FLOAT_EQ(result[0].transform.a, scale);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_WithXOffsetsOnly) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->x = 5.0f;
  run->y = 3.0f;
  run->glyphs = {1};
  run->xOffsets = {15.0f};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 10, 20);
  ASSERT_EQ(result.size(), 1u);
  // no positions, has xOffsets: posX = textPosX + run->x + xOffsets[0] = 10 + 5 + 15 = 30
  // posY = textPosY + run->y = 20 + 3 = 23
  EXPECT_FLOAT_EQ(result[0].transform.tx, 30.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 23.0f);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_WithPositionsAndXOffsets) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->x = 2.0f;
  run->y = 4.0f;
  run->glyphs = {1};
  run->positions = {{10.0f, 20.0f}};
  run->xOffsets = {3.0f};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  // posX = textPosX + run->x + positions[0].x + xOffsets[0] = 0 + 2 + 10 + 3 = 15
  // posY = textPosY + run->y + positions[0].y = 0 + 4 + 20 = 24
  EXPECT_FLOAT_EQ(result[0].transform.tx, 15.0f);
  EXPECT_FLOAT_EQ(result[0].transform.ty, 24.0f);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_PerGlyphRotation) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1};
  run->rotations = {45.0f};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_FALSE(result[0].transform.isIdentity());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_PerGlyphScale) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1};
  run->scales = {{2.0f, 1.5f}};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_NE(result[0].pathData, nullptr);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_PerGlyphSkew) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1};
  run->skews = {30.0f};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_PerGlyphAllTransforms) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(100, 100);
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 20;
  run->glyphs = {1};
  run->rotations = {15.0f};
  run->scales = {{1.5f, 1.2f}};
  run->skews = {10.0f};
  run->anchors = {{5.0f, 3.0f}};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].pathData, pathData);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_MultipleGlyphs) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph1 = doc->makeNode<pagx::Glyph>();
  auto path1 = doc->makeNode<pagx::PathData>();
  path1->moveTo(0, 0);
  path1->lineTo(100, 0);
  glyph1->path = path1;
  glyph1->advance = 500;
  auto glyph2 = doc->makeNode<pagx::Glyph>();
  auto path2 = doc->makeNode<pagx::PathData>();
  path2->moveTo(0, 0);
  path2->lineTo(200, 0);
  glyph2->path = path2;
  glyph2->advance = 600;
  font->glyphs = {glyph1, glyph2};
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1, 2};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].pathData, path1);
  EXPECT_EQ(result[1].pathData, path2);
  float scale = 10.0f / 1000.0f;
  float secondX = glyph1->advance * scale;
  EXPECT_FLOAT_EQ(result[1].transform.tx, secondX);
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_NullPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = nullptr;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

PAGX_TEST(PAGXSVGTest, ComputeGlyphPaths_EmptyPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto text = doc->makeNode<pagx::Text>();
  auto font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto glyph = doc->makeNode<pagx::Glyph>();
  auto pathData = doc->makeNode<pagx::PathData>();
  glyph->path = pathData;
  glyph->advance = 500;
  font->glyphs.push_back(glyph);
  auto run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 10;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);
  auto result = pagx::ComputeGlyphPaths(*text, 0, 0);
  EXPECT_TRUE(result.empty());
}

}  // namespace pag
