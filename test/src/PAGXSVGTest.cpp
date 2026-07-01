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
#include <set>
#include "cli/CommandVerify.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/SVGExporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
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
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/Baseline.h"
#include "utils/PAGXTestUtils.h"
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
    pagx::PAGXOptimizer::Optimize(doc.get());

    for (int i = 0; i < NumRoundTrips; i++) {
      auto round = std::to_string(i + 1);

      auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
      ASSERT_FALSE(pagxXml.empty()) << baseName << " round " << round << " PAGX export failed";
      auto pagxPath = SaveFile(pagxXml, "PAGXSVGTest/multi_roundtrip_" + baseName + ".pagx");

      VerifyFile(pagxPath, baseName + " round " + round);

      doc = pagx::PAGXImporter::FromFile(pagxPath);
      ASSERT_NE(doc, nullptr) << baseName << " round " << round << " PAGX import failed";

      auto svgStr = pagx::SVGExporter::ToSVG(*doc);
      ASSERT_FALSE(svgStr.empty()) << baseName << " round " << round << " SVG export failed";
      SaveFile(svgStr, "PAGXSVGTest/multi_roundtrip_" + baseName + ".svg");

      doc = pagx::SVGImporter::ParseString(svgStr);
      ASSERT_NE(doc, nullptr) << baseName << " round " << round << " SVG re-import failed";
      pagx::PAGXOptimizer::Optimize(doc.get());
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
 * Test SVG import: a zero-size SVG that only hosts <defs> (a common hidden
 * container in snapshotted HTML) is legal and must import to a valid empty
 * document rather than failing the parse.
 */
PAGX_TEST(PAGXSVGTest, SVGImport_ZeroSizeDefsOnly) {
  std::string svg =
      "<svg width=\"0\" height=\"0\"><defs><path id=\"p0\" d=\"M0 0L10 0L5 8Z\"/>"
      "</defs></svg>";
  auto doc = pagx::SVGImporter::ParseString(svg);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->layers.empty());
}

// Recursively collect every element of the given node type from a layer tree, descending into
// Layer contents, child Layers, and Group elements. Used by the textPath import tests below.
static void CollectElementsByType(pagx::Layer* layer, pagx::NodeType type,
                                  std::vector<pagx::Element*>& out);
static void CollectElementsFromElement(pagx::Element* element, pagx::NodeType type,
                                       std::vector<pagx::Element*>& out) {
  if (element == nullptr) {
    return;
  }
  if (element->nodeType() == type) {
    out.push_back(element);
  }
  if (element->nodeType() == pagx::NodeType::Group) {
    auto group = static_cast<pagx::Group*>(element);
    for (auto child : group->elements) {
      CollectElementsFromElement(child, type, out);
    }
  }
}
static void CollectElementsByType(pagx::Layer* layer, pagx::NodeType type,
                                  std::vector<pagx::Element*>& out) {
  if (layer == nullptr) {
    return;
  }
  for (auto element : layer->contents) {
    CollectElementsFromElement(element, type, out);
  }
  for (auto child : layer->children) {
    CollectElementsByType(child, type, out);
  }
}

/**
 * Test SVG import: a <text> element with a <textPath href="#..."> child resolves the referenced
 * path into a TextPath modifier alongside a Text node, so path-following text imports instead of
 * collapsing into an empty fill. startOffset maps to firstMargin, and textLength +
 * lengthAdjust="spacing" maps to forceAlignment with a recovered lastMargin.
 */
PAGX_TEST(PAGXSVGTest, SVGImport_TextPath) {
  std::string svg =
      "<svg width=\"300\" height=\"100\" viewBox=\"0 0 300 100\">"
      "<defs><path id=\"line\" d=\"M10 50L270 50\"/></defs>"
      "<text font-family=\"Arial\" font-size=\"12\" fill=\"#10B981\" textLength=\"220\""
      " lengthAdjust=\"spacing\">"
      "<textPath href=\"#line\" startOffset=\"20\">Along The Line</textPath>"
      "</text></svg>";
  auto doc = pagx::SVGImporter::ParseString(svg);
  ASSERT_NE(doc, nullptr);
  ASSERT_FALSE(doc->layers.empty());

  std::vector<pagx::Element*> texts;
  std::vector<pagx::Element*> textPaths;
  for (auto layer : doc->layers) {
    CollectElementsByType(layer, pagx::NodeType::Text, texts);
    CollectElementsByType(layer, pagx::NodeType::TextPath, textPaths);
  }

  ASSERT_EQ(texts.size(), 1u);
  ASSERT_EQ(textPaths.size(), 1u);

  auto text = static_cast<pagx::Text*>(texts[0]);
  EXPECT_EQ(text->text, "Along The Line");

  auto textPath = static_cast<pagx::TextPath*>(textPaths[0]);
  ASSERT_NE(textPath->path, nullptr);
  EXPECT_FALSE(textPath->path->isEmpty());
  EXPECT_FLOAT_EQ(textPath->firstMargin, 20.0f);
  EXPECT_TRUE(textPath->forceAlignment);
  // Path length is 260 (straight line from x=10 to x=270). lastMargin = 260 - 20 - 220 = 20.
  EXPECT_FLOAT_EQ(textPath->lastMargin, 20.0f);
}

/**
 * Test SVG import: a <textPath> whose href does not resolve to a defined <path> drops the path
 * modifier but still keeps the text content as a plain Text node, so the run is not lost.
 */
PAGX_TEST(PAGXSVGTest, SVGImport_TextPathUnresolvedHref) {
  std::string svg =
      "<svg width=\"300\" height=\"100\" viewBox=\"0 0 300 100\">"
      "<text font-family=\"Arial\" font-size=\"12\">"
      "<textPath href=\"#missing\">Orphan Text</textPath>"
      "</text></svg>";
  auto doc = pagx::SVGImporter::ParseString(svg);
  ASSERT_NE(doc, nullptr);
  ASSERT_FALSE(doc->layers.empty());

  std::vector<pagx::Element*> texts;
  std::vector<pagx::Element*> textPaths;
  for (auto layer : doc->layers) {
    CollectElementsByType(layer, pagx::NodeType::Text, texts);
    CollectElementsByType(layer, pagx::NodeType::TextPath, textPaths);
  }

  ASSERT_EQ(texts.size(), 1u);
  EXPECT_EQ(static_cast<pagx::Text*>(texts[0])->text, "Orphan Text");
  EXPECT_TRUE(textPaths.empty());
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

/**
 * Test SVG export of a Group with skew transform produces correct matrix direction.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_GroupSkew) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto group = doc->makeNode<pagx::Group>();
  group->skew = 30;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {80, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.6f, 0.8f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The SVG matrix for a pure skewX(30°) is matrix(1, 0, -tan(30°), 1, 0, 0) under
  // PAGX's skew convention (matching tgfx native rendering, which shears with
  // DegreesToRadians(-skew)). tan(30°) ≈ 0.5774, so the third component (skewX)
  // must be negative for a positive skew angle.
  auto matrixPos = svg.find("matrix(");
  ASSERT_NE(matrixPos, std::string::npos);
  auto matrixEnd = svg.find(")", matrixPos);
  auto matrixStr = svg.substr(matrixPos + 7, matrixEnd - matrixPos - 7);
  // Parse matrix(a,b,c,d,e,f) — c is the third value.
  float a = 0, b = 0, c = 0, d = 0;
  sscanf(matrixStr.c_str(), "%f,%f,%f,%f", &a, &b, &c, &d);
  // skewX component (c) is the negation of tan(skew) under PAGX's convention.
  EXPECT_NEAR(c, -std::tan(30 * static_cast<float>(M_PI) / 180), 0.001f);
  SaveFile(svg, "PAGXSVGTest/svg_export_group_skew.svg");
}

/**
 * Multi-fill / multi-stroke: two Fills and two Strokes around one Rectangle
 * should emit four separate <rect> elements (one per painter in source order).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MultiFillMultiStroke) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 60};

  auto fill1 = doc->makeNode<pagx::Fill>();
  auto solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill1->color = solid1;

  auto stroke1 = doc->makeNode<pagx::Stroke>();
  auto solidS1 = doc->makeNode<pagx::SolidColor>();
  solidS1->color = {0.0f, 1.0f, 0.0f, 1.0f};
  stroke1->color = solidS1;
  stroke1->width = 2;

  auto fill2 = doc->makeNode<pagx::Fill>();
  auto solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0.0f, 0.0f, 1.0f, 0.5f};
  fill2->color = solid2;

  auto stroke2 = doc->makeNode<pagx::Stroke>();
  auto solidS2 = doc->makeNode<pagx::SolidColor>();
  solidS2->color = {1.0f, 1.0f, 0.0f, 1.0f};
  stroke2->color = solidS2;
  stroke2->width = 4;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill1);
  layer->contents.push_back(stroke1);
  layer->contents.push_back(fill2);
  layer->contents.push_back(stroke2);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  size_t pos = 0;
  int rectCount = 0;
  while ((pos = svg.find("<rect", pos)) != std::string::npos) {
    ++rectCount;
    ++pos;
  }
  EXPECT_EQ(rectCount, 4);
  SaveFile(svg, "PAGXSVGTest/svg_export_multi_fill_stroke.svg");
}

/**
 * StrokeAlign::Inside: the stroke geometry is shrunk by half the stroke width
 * so the visible rect width equals the authored width minus the stroke width.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_StrokeAlignInside) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};

  auto stroke = doc->makeNode<pagx::Stroke>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;
  stroke->width = 20;
  stroke->align = pagx::StrokeAlign::Inside;

  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Inside stroke shrinks the rect geometry by stroke->width total (half per side).
  EXPECT_NE(svg.find("width=\"80\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"80\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_stroke_align_inside.svg");
}

/**
 * Polystar: a 5-point star is resolved into a Path with a cubic / line contour.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Polystar) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  auto star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {150, 150};
  star->pointCount = 5;
  star->outerRadius = 80;
  star->innerRadius = 40;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.8f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(star);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("d=\"M"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_polystar.svg");
}

/**
 * Repeater: 3 copies of a Rectangle should produce three <rect> elements.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_Repeater) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 100};
  rect->size = {40, 40};

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.7f, 0.5f, 1.0f};
  fill->color = solid;

  auto rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  rep->position = {80, 0};

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  size_t pos = 0;
  int rectCount = 0;
  while ((pos = svg.find("<rect", pos)) != std::string::npos) {
    ++rectCount;
    ++pos;
  }
  EXPECT_EQ(rectCount, 3);
  SaveFile(svg, "PAGXSVGTest/svg_export_repeater.svg");
}

/**
 * DropShadowStyle emits feOffset + feGaussianBlur + feMerge chain, just like
 * DropShadowFilter. The style path goes through layer->styles, not filters.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_DropShadowStyle) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {100, 100};

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;

  auto ds = doc->makeNode<pagx::DropShadowStyle>();
  ds->offsetX = 5;
  ds->offsetY = 7;
  ds->blurX = 4;
  ds->blurY = 4;
  ds->color = {0.0f, 0.0f, 0.0f, 0.5f};

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->styles.push_back(ds);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<feGaussianBlur"), std::string::npos);
  EXPECT_NE(svg.find("<feOffset"), std::string::npos);
  EXPECT_NE(svg.find("<feMerge"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_dropshadow_style.svg");
}

/**
 * WritingMode::Vertical emits writing-mode="vertical-rl" on the <text> element
 * so CJK vertical layout appears correctly. The modern CSS Writing Modes value
 * (rather than the legacy SVG 1.1 "tb") is used because text-anchor handling
 * in the legacy keyword path is inconsistent across renderers.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_WritingModeVertical) {
  auto doc = pagx::PAGXDocument::Make(200, 300);
  auto layer = doc->makeNode<pagx::Layer>();
  auto textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 100;
  textBox->height = 200;
  textBox->writingMode = pagx::WritingMode::Vertical;

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Vert";
  text->fontFamily = "Arial";
  text->fontSize = 24;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  textBox->elements.push_back(text);
  textBox->elements.push_back(fill);
  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);

  pagx::SVGExportOptions options;
  options.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, options);
  EXPECT_NE(svg.find("writing-mode=\"vertical-rl\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_writing_mode_vertical.svg");
}

// Coverage for the vertical justify stretch path: when a column is shorter than the box's inner
// height the exporter writes textLength + lengthAdjust="spacing" so SVG distributes the surplus
// spacing across the glyphs instead of leaving them at the start of the column. The last column
// degrades to Start (no textLength) to match the renderer.
PAGX_TEST(PAGXSVGTest, SVGExport_WritingModeVerticalJustifyEmitsTextLength) {
  auto doc = pagx::PAGXDocument::Make(200, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {20, 20};
  textBox->width = 120;
  textBox->height = 320;
  textBox->writingMode = pagx::WritingMode::Vertical;
  textBox->textAlign = pagx::TextAlign::Justify;
  textBox->overflow = pagx::Overflow::Hidden;

  // Text long enough that vertical layout creates several columns shorter than 320.
  auto* text = doc->makeNode<pagx::Text>();
  text->text =
      "AAAAAAAAAA BBBBBBBBBB CCCCCCCCCC DDDDDDDDDD EEEEEEEEEE FFFFFFFFFF GGGGGGGGGG HHHHHHHHHH";
  text->fontFamily = "Arial";
  text->fontSize = 24;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;

  textBox->elements.push_back(text);
  textBox->elements.push_back(fill);
  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);

  pagx::SVGExportOptions options;
  options.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, options);
  EXPECT_NE(svg.find("writing-mode=\"vertical-rl\""), std::string::npos);
  EXPECT_NE(svg.find("textLength="), std::string::npos);
  EXPECT_NE(svg.find("lengthAdjust=\"spacing\""), std::string::npos);
}

/**
 * Composition recursion: a Layer referencing a Composition emits the
 * composition's inner layers into the same <g>, so the nested Rectangle
 * appears in the output.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_CompositionChildLayer) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto comp = doc->makeNode<pagx::Composition>();
  comp->width = 300;
  comp->height = 300;
  auto inner = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {120, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.6f, 0.3f, 1.0f};
  fill->color = solid;
  inner->contents.push_back(rect);
  inner->contents.push_back(fill);
  comp->layers.push_back(inner);

  auto outer = doc->makeNode<pagx::Layer>();
  outer->composition = comp;
  doc->layers.push_back(outer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The inner rect must surface in the SVG even though it lives under a
  // composition rather than a direct contents/children relationship.
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_NE(svg.find("width=\"120\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"80\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_composition_child.svg");
}

/**
 * renderPosition() / renderSize(): a Rectangle with layout-override width and
 * height on the LayoutNode base (not the intrinsic `size`) should surface the
 * override in the emitted SVG. This proves the writer now reads through
 * renderSize() rather than the raw `size` field.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_RenderPositionFromConstraint) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  // Intrinsic size the legacy writer used to emit verbatim.
  rect->size = {10, 10};
  // LayoutNode overrides that renderSize() must honour.
  rect->width = 150;
  rect->height = 80;
  rect->position = {200, 200};

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("width=\"150\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"80\""), std::string::npos);
  EXPECT_EQ(svg.find("width=\"10\""), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_render_position.svg");
}

// Helper function to create a minimal PNG image
static pagx::Image* MakeTestPNGImage(pagx::PAGXDocument* doc) {
  // Minimal valid 2x2 RGBA PNG (8-bit, non-interlaced)
  static const uint8_t MINIMAL_PNG[] = {
      0x89,
      0x50,
      0x4E,
      0x47,
      0x0D,
      0x0A,
      0x1A,
      0x0A,  // PNG signature
      // IHDR
      0x00,
      0x00,
      0x00,
      0x0D,
      0x49,
      0x48,
      0x44,
      0x52,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x02,
      0x08,
      0x02,
      0x00,
      0x00,
      0x00,
      0xFD,
      0xD4,
      0x9A,
      0x73,
      // IDAT (compressed pixel data)
      0x00,
      0x00,
      0x00,
      0x14,
      0x49,
      0x44,
      0x41,
      0x54,
      0x78,
      0x9C,
      0x62,
      0xF8,
      0xCF,
      0xC0,
      0xF0,
      0x1F,
      0x01,
      0x18,
      0x18,
      0x18,
      0x00,
      0x09,
      0x04,
      0x01,
      0x01,
      0xE2,
      0x2D,
      0x42,
      0xA3,
      // IEND
      0x00,
      0x00,
      0x00,
      0x00,
      0x49,
      0x45,
      0x4E,
      0x44,
      0xAE,
      0x42,
      0x60,
      0x82,
  };
  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(MINIMAL_PNG, sizeof(MINIMAL_PNG));
  return image;
}

/**
 * Test SVG export of ImagePattern with Repeat tile mode (default SVG support).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternRepeat) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Should contain pattern definition
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("url(#pattern"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_imagepattern_repeat.svg");
}

/**
 * Test SVG export of ImagePattern with Mirror tile mode (requires baking).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternMirror) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Mirror;
  pattern->tileModeY = pagx::TileMode::Mirror;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Should contain baked pattern with data URI
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_imagepattern_mirror.svg");
}

/**
 * Test SVG export of ImagePattern with Clamp tile mode (requires baking).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternClamp) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Clamp;
  pattern->tileModeY = pagx::TileMode::Clamp;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Should contain baked pattern with data URI
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_imagepattern_clamp.svg");
}

/**
 * Test SVG export of ImagePattern with Decal tile mode (requires baking).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternDecal) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Decal;
  pattern->tileModeY = pagx::TileMode::Decal;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Should contain baked pattern with data URI
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_imagepattern_decal.svg");
}

/**
 * Test SVG export of ImagePattern with mixed tile modes (X=Mirror, Y=Repeat).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternMixedModes) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Mirror;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Should contain baked pattern due to Mirror in X
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_imagepattern_mixed.svg");
}

// ===========================================================================
// ModifierResolver coverage — exercised through SVG export so geometry walks
// hit the real resolver implementation.
// ===========================================================================

static pagx::Fill* MakeSolidFillSVG(pagx::PAGXDocument* doc, float r, float g, float b,
                                    float a = 1.0f) {
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {r, g, b, a};
  fill->color = color;
  return fill;
}

/**
 * Polygon with fractional pointCount should still emit a path; the resolver's
 * StarToTGFXPath / PolygonToTGFXPath helpers truncate non-integer pointCount.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_PolygonShape) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {150, 150};
  poly->pointCount = 6;
  poly->outerRadius = 80;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("d=\"M"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_polygon.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_PolygonWithRoundness) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {150, 150};
  poly->pointCount = 5;
  poly->outerRadius = 70;
  poly->outerRoundness = 0.5f;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.5f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  // Roundness path emits cubic curves (C in the path data).
  EXPECT_NE(svg.find("C"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StarWithRoundnessReversed) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {150, 150};
  star->pointCount = 5;
  star->outerRadius = 80;
  star->innerRadius = 40;
  star->outerRoundness = 0.3f;
  star->innerRoundness = 0.7f;
  star->reversed = true;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.8f, 0.5f, 0.0f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StarFractionalPoints) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {150, 150};
  star->pointCount = 5.5f;
  star->outerRadius = 80;
  star->innerRadius = 40;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 1.0f, 0.8f, 0.0f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StarDegenerateZeroPointCount) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {100, 100};
  star->pointCount = 0.0f;
  star->outerRadius = 50;
  star->innerRadius = 25;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.5f));
  doc->layers.push_back(layer);

  // Zero-point star should result in empty path data; export must still succeed.
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PolygonTooFewPoints) {
  // pointCount < 3 → PolygonToTGFXPath returns empty path.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {100, 100};
  poly->pointCount = 2.0f;
  poly->outerRadius = 50;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.5f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

/**
 * TrimPath::Separate should bake into a fresh <path>.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathSeparate) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 80};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Separate;
  trim->start = 0.0f;
  trim->end = 0.5f;
  trim->offset = 90.0f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0, 0, 0, 1};
  stroke->color = color;
  stroke->width = 4;

  layer->contents.push_back(rect);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathContinuous) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->position = {80, 100};
  rect1->size = {60, 60};
  auto* rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {220, 100};
  rect2->size = {60, 60};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Continuous;
  trim->start = 0.0f;
  trim->end = 0.75f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.2f, 0.6f, 0.2f, 1};
  stroke->color = color;
  stroke->width = 3;

  layer->contents.push_back(rect1);
  layer->contents.push_back(rect2);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_trim_continuous.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathWithoutShape) {
  // TrimPath with no preceding shape should be a no-op.
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* trim = doc->makeNode<pagx::TrimPath>();
  layer->contents.push_back(trim);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

/**
 * RoundCorner modifier rounds rectangle corners and bakes them into a Path.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_RoundCorner) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 80};
  auto* rc = doc->makeNode<pagx::RoundCorner>();
  rc->radius = 15.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(rc);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.7f, 0.4f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RoundCornerNonPositiveRadius) {
  // radius <= 0 is a no-op; the original rectangle should still emit.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 80};
  auto* rc = doc->makeNode<pagx::RoundCorner>();
  rc->radius = 0.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(rc);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 1.0f, 0.0f, 0.0f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
}

/**
 * MergePath modes exercise MergeModeToPathOp's switch.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MergePathUnion) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {120, 100};
  r1->size = {80, 80};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {180, 100};
  r2->size = {80, 80};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Union;
  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.4f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathDifference) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {150, 100};
  r1->size = {100, 100};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {180, 100};
  r2->size = {60, 60};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Difference;
  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.8f, 0.2f, 0.2f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathIntersect) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {130, 100};
  r1->size = {100, 100};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {170, 100};
  r2->size = {100, 100};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Intersect;
  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.8f, 0.2f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathXor) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {130, 100};
  r1->size = {100, 100};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {170, 100};
  r2->size = {100, 100};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Xor;
  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.7f, 0.3f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathWithoutShape) {
  // MergePath without any preceding shape should be a no-op.
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* mp = doc->makeNode<pagx::MergePath>();
  layer->contents.push_back(mp);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterAboveOriginal) {
  // Order = AboveOriginal triggers the std::reverse path.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 100};
  rect->size = {40, 40};
  auto* fill = MakeSolidFillSVG(doc.get(), 0.4f, 0.2f, 0.7f);
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 4;
  rep->position = {70, 0};
  rep->order = pagx::RepeaterOrder::AboveOriginal;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  size_t pos = 0;
  int rectCount = 0;
  while ((pos = svg.find("<rect", pos)) != std::string::npos) {
    ++rectCount;
    ++pos;
  }
  EXPECT_EQ(rectCount, 4);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterZeroCopiesErasesScope) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {50, 50};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 0;
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.7f, 0.3f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Zero copies clears the scope — no <rect> should remain.
  EXPECT_EQ(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterNegativeCopiesNoOp) {
  // Negative copies break out of the case without changing the output.
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {50, 50};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = -3;
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.4f, 0.4f, 0.9f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterEmptyScope) {
  // Repeater with no preceding output should be a no-op.
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterFractionalCopies) {
  // copies=2.5 → 3 copies with fractional alpha on the last.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 100};
  rect->size = {40, 40};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 2.5f;
  rep->position = {80, 0};
  rep->endAlpha = 0.0f;
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // 3 generated copies => at least 3 <rect>.
  size_t pos = 0;
  int rectCount = 0;
  while ((pos = svg.find("<rect", pos)) != std::string::npos) {
    ++rectCount;
    ++pos;
  }
  EXPECT_GE(rectCount, 3);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterNegativeScaleFractionalOffset) {
  // Triggers SignedPow's sign-preservation on a negative base with non-integer exponent.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 100};
  rect->size = {40, 40};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  rep->offset = 0.5f;
  rep->position = {60, 0};
  rep->scale = {-1.2f, -1.0f};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.9f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PathWithRenderScale) {
  // renderScale != 1 must be baked into the emitted path data (not exposed as a
  // transform="scale(...)") so that userSpaceOnUse gradients and patterns resolve
  // in the same parent-container coordinate space the PAGX renderer uses. Here
  // the 100x100 triangle is laid out into a 200x200 box (renderScale = 2.0), so
  // the d attribute must carry coordinates up to 200 — not the authored 100.
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  *data = pagx::PathDataFromSVGString("M0 0 L100 0 L100 100 Z");
  path->data = data;
  path->width = 200.0f;
  path->height = 200.0f;

  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.9f, 0.1f, 0.5f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_EQ(svg.find("scale("), std::string::npos);
  EXPECT_NE(svg.find("L200"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PathReversedTrim) {
  // Path with reversed=true exercises PrimitiveToTGFXPath's reverse branch via
  // a trim modifier that re-enters the helper.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  *data = pagx::PathDataFromSVGString("M40 40 L160 40 L160 160 L40 160 Z");
  path->data = data;
  path->reversed = true;
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.6f;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  stroke->color = sc;
  stroke->width = 3;

  layer->contents.push_back(path);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimEmptyPath) {
  // Trim on an empty Path is a no-op (PrimitiveToTGFXPath returns empty).
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  // No path data.
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  layer->contents.push_back(path);
  layer->contents.push_back(trim);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0, 0, 0));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_EllipseWithTrim) {
  // Trim on an ellipse exercises PrimitiveToTGFXPath's NodeType::Ellipse branch.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* el = doc->makeNode<pagx::Ellipse>();
  el->position = {100, 100};
  el->size = {120, 80};
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.1f, 0.7f, 0.2f, 1};
  stroke->color = sc;
  stroke->width = 3;
  layer->contents.push_back(el);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RectangleNegativeRoundnessTrim) {
  // Rectangle with negative roundness triggers PrimitiveToTGFXPath's clamp.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 60};
  rect->roundness = -5.0f;
  rect->reversed = true;
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  stroke->color = sc;
  stroke->width = 4;

  layer->contents.push_back(rect);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PolystarThroughTrim) {
  // Polystar is resolved to a Path before Trim re-enters PrimitiveToTGFXPath.
  // Drives the wrapper Path's renderPosition()=(0,0) / renderScale()=1 fix.
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {150, 150};
  star->pointCount = 5;
  star->outerRadius = 80;
  star->innerRadius = 40;
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  stroke->color = sc;
  stroke->width = 3;
  layer->contents.push_back(star);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

// ===========================================================================
// SVGExporter coverage — conic / diamond gradient fallback, P3 color, blend
// filter, color matrix filter, scrollRect, mask types.
// ===========================================================================

PAGX_TEST(PAGXSVGTest, SVGExport_ConicGradientDegradesToRadial) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {100, 100};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.bakeUnsupported = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<radialGradient"), std::string::npos);
  EXPECT_NE(svg.find("conic gradient degraded"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_DiamondGradientDegradesToRadial) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::DiamondGradient>();
  grad->center = {100, 100};
  grad->radius = 90.0f;
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 1, 0, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.bakeUnsupported = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<radialGradient"), std::string::npos);
  EXPECT_NE(svg.find("diamond gradient degraded"), std::string::npos);
}

// Coverage for bakeUnsupported=true (the default): conic / diamond gradient trips the SVG
// feature probe and is baked to an embedded PNG patch via <image> instead of degrading to a
// radial gradient.
PAGX_TEST(PAGXSVGTest, SVGExport_BakeUnsupportedRastersConicGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->id = "conicLayer";
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {100, 100};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  // Default options: bakeUnsupported=true. Expect an <image> with a PNG data URI to replace the
  // vector emission, and no <radialGradient> degrade fallback to slip through.
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<image"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  EXPECT_EQ(svg.find("<radialGradient"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_BakeUnsupportedRastersDiamondGradient) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::DiamondGradient>();
  grad->center = {100, 100};
  grad->radius = 90.0f;
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 1, 0, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<image"), std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
}

// Regression: when a rasterize-path layer (an unsupported feature triggers PNG bake) also has a
// scrollRect, the baked PNG and the placed <image> must both honour the scrollRect window. The
// previous code used `getBounds(coordinateSpace, true)` for the <image> placement and skipped the
// scrollRect clip in MaskedLayerDrawerInSpace, so the <image> covered the full unclipped bounds
// while leaking out-of-window pixels into the export. After the fix the <image>'s width/height
// match the scrollRect dimensions exactly.
PAGX_TEST(PAGXSVGTest, SVGExport_BakeUnsupportedHonoursScrollRect) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->id = "scrolledRaster";
  layer->hasScrollRect = true;
  layer->scrollRect = {0, 0, 100, 80};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {300, 300};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {150, 150};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0, 0, 1, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The bake produced an <image>, not a vector fall-through.
  auto imageStart = svg.find("<image");
  ASSERT_NE(imageStart, std::string::npos);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
  // Inspect the <image> element specifically (root <svg> also has width="300", so we cannot
  // assert on the global string). The <image>'s width / height must equal the scrollRect window
  // (100 × 80), proving the rasterize bounds were intersected with the scrollRect.
  auto imageEnd = svg.find("/>", imageStart);
  ASSERT_NE(imageEnd, std::string::npos);
  std::string imageTag = svg.substr(imageStart, imageEnd - imageStart);
  EXPECT_NE(imageTag.find("width=\"100\""), std::string::npos);
  EXPECT_NE(imageTag.find("height=\"80\""), std::string::npos);
  // The unclipped layer bounds (300 × 300) must NOT leak into the <image> placement.
  EXPECT_EQ(imageTag.find("width=\"300\""), std::string::npos);
  EXPECT_EQ(imageTag.find("height=\"300\""), std::string::npos);
}

// Coverage for the rasterScale clamp: 0 / negative / huge values must be clamped to a usable
// range so bakeUnsupported=true still produces a non-empty <image>. The placed <image> always
// keeps logical coordinates regardless of the chosen scale, but the PNG payload size scales.
PAGX_TEST(PAGXSVGTest, SVGExport_RasterScaleDefaultProducesImage) {
  auto doc = pagx::PAGXDocument::Make(120, 120);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {60, 60};
  rect->size = {100, 100};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {60, 60};
  auto* s = doc->makeNode<pagx::ColorStop>();
  s->offset = 0.0f;
  s->color = {1, 0, 0, 1};
  grad->colorStops = {s};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.rasterScale = 2.0f;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RasterScaleClampsZeroAndNegative) {
  auto doc = pagx::PAGXDocument::Make(120, 120);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {60, 60};
  rect->size = {100, 100};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {60, 60};
  auto* s = doc->makeNode<pagx::ColorStop>();
  s->offset = 0.0f;
  s->color = {1, 0, 0, 1};
  grad->colorStops = {s};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  // rasterScale=0 / negative would silently produce a zero-pixel surface and the bake would
  // fall through to the vector path, contradicting bakeUnsupported=true. The clamp at the
  // entry point keeps the bake alive.
  pagx::SVGExporter::Options opts;
  opts.rasterScale = 0.0f;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);

  opts.rasterScale = -1.0f;
  auto svg2 = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg2.find("data:image/png;base64,"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RasterScaleClampsHugeValue) {
  auto doc = pagx::PAGXDocument::Make(60, 60);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {30, 30};
  rect->size = {40, 40};
  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {30, 30};
  auto* s = doc->makeNode<pagx::ColorStop>();
  s->offset = 0.0f;
  s->color = {1, 0, 0, 1};
  grad->colorStops = {s};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  // Huge rasterScale is clamped at the entry point so float→int cast in the bake stays well
  // within int range. The exported SVG keeps logical pixel dimensions.
  pagx::SVGExporter::Options opts;
  opts.rasterScale = 1000.0f;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_DisplayP3FillEmitsColorStyle) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.3f, 0.2f, 1.0f};
  solid->color.colorSpace = pagx::ColorSpace::DisplayP3;
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("color(display-p3"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_p3.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_BlendModeOnFill) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* fill = MakeSolidFillSVG(doc.get(), 1.0f, 0.4f, 0.0f);
  fill->blendMode = pagx::BlendMode::Multiply;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("mix-blend-mode:multiply"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_LayerBlendModeEmitsStyle) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->blendMode = pagx::BlendMode::Screen;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.4f, 0.6f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("mix-blend-mode:screen"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_BlendFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* blend = doc->makeNode<pagx::BlendFilter>();
  blend->color = {0.0f, 1.0f, 0.0f, 0.7f};
  blend->blendMode = pagx::BlendMode::Multiply;
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.9f, 0.2f, 0.2f));
  layer->filters.push_back(blend);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<feFlood"), std::string::npos);
  EXPECT_NE(svg.find("<feBlend"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_ColorMatrixFilter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* cm = doc->makeNode<pagx::ColorMatrixFilter>();
  // Sepia-like matrix to confirm value emission.
  cm->matrix = {0.393f, 0.769f, 0.189f, 0.0f, 0.0f, 0.349f, 0.686f, 0.168f, 0.0f, 0.0f,
                0.272f, 0.534f, 0.131f, 0.0f, 0.0f, 0.0f,   0.0f,   0.0f,   1.0f, 0.0f};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.6f, 0.4f, 0.2f));
  layer->filters.push_back(cm);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<feColorMatrix"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_ScrollRect) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->hasScrollRect = true;
  layer->scrollRect = {10, 20, 200, 150};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {180, 100};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.5f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<clipPath"), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#scrollClip"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskContour) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {120, 120};
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* user = doc->makeNode<pagx::Layer>();
  user->mask = maskLayer;
  user->maskType = pagx::MaskType::Contour;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  user->contents.push_back(rect);
  user->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(user);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<clipPath"), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#clip"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_contour.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskContourWithScrollRectCoexist) {
  // Regression: when a layer carries both a contour mask and a scrollRect, both must remain
  // effective. Prior code wrote `clip-path="url(#contour)"` and then `clip-path="url(#scroll)"`
  // onto the same outer <g>, where the second overwrites the first and silently disables the
  // contour mask. Check that the emitted SVG references both clipPath ids.
  auto doc = pagx::PAGXDocument::Make(300, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {120, 120};
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* user = doc->makeNode<pagx::Layer>();
  user->mask = maskLayer;
  user->maskType = pagx::MaskType::Contour;
  user->hasScrollRect = true;
  user->scrollRect = {10, 20, 200, 150};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {180, 100};
  user->contents.push_back(rect);
  user->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(user);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Both clipPath defs must be present — the contour clip and the scrollRect clip.
  EXPECT_NE(svg.find("id=\"clip"), std::string::npos);
  EXPECT_NE(svg.find("id=\"scrollClip"), std::string::npos);
  // Both clip-path attributes must reference their respective ids in the output. With the
  // collision-fix the contour clip is on the outer <g> and the scrollRect clip moves to a
  // middle <g> wrapper, so both references survive end-to-end.
  EXPECT_NE(svg.find("clip-path=\"url(#clip"), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#scrollClip"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_contour_with_scrollrect.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_MaskAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  *data = pagx::PathDataFromSVGString("M40 40 L160 40 L160 160 L40 160 Z");
  path->data = data;
  maskLayer->contents.push_back(path);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* user = doc->makeNode<pagx::Layer>();
  user->mask = maskLayer;
  user->maskType = pagx::MaskType::Alpha;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  user->contents.push_back(rect);
  user->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.1f));
  doc->layers.push_back(user);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<mask"), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#mask"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_mask_alpha.svg");
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeAlignOutside) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  stroke->color = sc;
  stroke->width = 10;
  stroke->align = pagx::StrokeAlign::Outside;
  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Outside stroke grows the rect by stroke->width (5 per side).
  EXPECT_NE(svg.find("width=\"110\""), std::string::npos);
  EXPECT_NE(svg.find("height=\"110\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeMiterLimitAndBevel) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  data->moveTo(20, 80);
  data->lineTo(80, 20);
  data->lineTo(140, 80);
  path->data = data;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.0f, 0.0f, 0.0f, 1};
  stroke->color = sc;
  stroke->width = 4;
  stroke->join = pagx::LineJoin::Bevel;
  stroke->cap = pagx::LineCap::Square;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-linejoin=\"bevel\""), std::string::npos);
  EXPECT_NE(svg.find("stroke-linecap=\"square\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeMiterCustomLimit) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  data->moveTo(40, 100);
  data->lineTo(100, 40);
  data->lineTo(160, 100);
  path->data = data;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  stroke->color = sc;
  stroke->width = 2;
  stroke->join = pagx::LineJoin::Miter;
  stroke->miterLimit = 8.0f;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke-miterlimit=\"8\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_FillDefaultColorWhenNoSource) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* fill = doc->makeNode<pagx::Fill>();
  // fill->color stays nullptr.
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Defaults to opaque black.
  EXPECT_NE(svg.find("fill=\"#000000\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StrokeDefaultColorWhenNoSource) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  auto* stroke = doc->makeNode<pagx::Stroke>();
  // stroke->color stays nullptr.
  stroke->width = 2;
  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stroke=\"#000000\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_FilterCombinationDropShadowPlusBlur) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {120, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.4f, 0.7f));
  auto* blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 4;
  blur->blurY = 8;  // exercise non-square blur stdDeviation
  layer->filters.push_back(blur);
  auto* shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->offsetX = 3;
  shadow->offsetY = 5;
  shadow->blurX = 2;
  shadow->blurY = 2;
  shadow->color = {0, 0, 0, 0.6f};
  shadow->shadowOnly = true;
  layer->filters.push_back(shadow);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<feGaussianBlur"), std::string::npos);
  // Non-square blur emits "x y" stdDeviation.
  EXPECT_NE(svg.find("stdDeviation=\"4 8\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_ConvertTextToPathDisabled) {
  // Exercises native <text> emission path (writeText) when convertTextToPath is off.
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Native";
  text->position = {150, 50};
  text->fontFamily = "Arial";
  text->fontStyle = "Bold";
  text->fontSize = 24;
  text->textAnchor = pagx::TextAnchor::Center;
  text->letterSpacing = 2.0f;
  text->fauxItalic = true;
  auto* fill = MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f);

  auto* group = doc->makeNode<pagx::Group>();
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("Native"), std::string::npos);
  EXPECT_NE(svg.find("font-style=\"italic\""), std::string::npos);
  EXPECT_NE(svg.find("letter-spacing="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_InvisibleLayerSkipped) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->visible = false;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.5f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("<rect"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GroupOpacityPropagatesToChildren) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->alpha = 0.4f;
  layer->groupOpacity = false;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 80};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // With groupOpacity=false, alpha is folded into fill-opacity, not the <g> opacity.
  EXPECT_NE(svg.find("fill-opacity="), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_CustomData) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->customData["foo"] = "bar";
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {40, 40};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 1.0f, 0.0f, 0.0f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("data-foo=\"bar\""), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GradientColorStopAlpha) {
  // Exercises stop-opacity emission in writeGradientStops.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {10, 10};
  grad->endPoint = {190, 190};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {1.0f, 0.0f, 0.0f, 0.3f};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {0.0f, 0.0f, 1.0f, 0.6f};
  s2->color.colorSpace = pagx::ColorSpace::DisplayP3;
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("stop-opacity="), std::string::npos);
  EXPECT_NE(svg.find("color(display-p3"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_GradientFitsToGeometry) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {1, 1};
  grad->fitsToGeometry = true;
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.0f;
  s1->color = {0, 0, 0, 1};
  auto* s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1.0f;
  s2->color = {1, 1, 1, 1};
  grad->colorStops = {s1, s2};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("gradientUnits=\"objectBoundingBox\""), std::string::npos);
}

/**
 * ImagePattern with a rotated matrix that has skew/rotation triggers the
 * fallback path that emits transform on the inner <image>. This exercises
 * the !ComputePlacedImageRectSVG branch in writeImagePatternDef.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternRotatedMatrix) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Decal;
  pattern->tileModeY = pagx::TileMode::Decal;
  pattern->scaleMode = pagx::ScaleMode::None;
  // Matrix carrying skew so b/c are non-zero -> ComputePlacedImageRectSVG returns false.
  pattern->matrix = {1.0f, 0.5f, 0.5f, 1.0f, 10.0f, 20.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<pattern"), std::string::npos);
}

/**
 * Exercises Image filePath fallback (no inline data). When the on-disk read fails the exporter
 * drops the asset and surfaces a warning rather than embedding the host-local filesystem path,
 * which would never resolve in a different environment and would leak directory layout.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_ImagePatternFilePathFallback) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {20, 20};
  rect->size = {160, 160};

  auto* image = doc->makeNode<pagx::Image>();
  image->filePath = "nonexistent/test_image.png";
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  std::vector<std::string> warnings;
  auto svg = pagx::SVGExporter::ToSVG(*doc, {}, &warnings);
  EXPECT_EQ(svg.find("nonexistent/test_image.png"), std::string::npos);
  EXPECT_FALSE(warnings.empty());
  bool sawWarning = false;
  for (const auto& w : warnings) {
    if (w.find("nonexistent/test_image.png") != std::string::npos) {
      sawWarning = true;
      break;
    }
  }
  EXPECT_TRUE(sawWarning);
}

/**
 * JPEG magic bytes should be detected and produce a data:image/jpeg URI.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_JpegMimeDetection) {
  static constexpr uint8_t FAKE_JPEG[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J',  'F',
                                          'I',  'F',  0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
                                          0x00, 0x01, 0x00, 0x00, 0xFF, 0xD9};
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {10, 10};
  rect->size = {80, 80};
  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(FAKE_JPEG, sizeof(FAKE_JPEG));
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Decal;
  pattern->tileModeY = pagx::TileMode::Decal;
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("data:image/jpeg;base64,"), std::string::npos);
}

/**
 * WebP magic bytes should be detected and produce a data:image/webp URI.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_WebpMimeDetection) {
  static constexpr uint8_t FAKE_WEBP[] = {'R', 'I', 'F', 'F', 0x10, 0x00, 0x00, 0x00,
                                          'W', 'E', 'B', 'P', 'V',  'P',  '8',  ' '};
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {10, 10};
  rect->size = {80, 80};
  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(FAKE_WEBP, sizeof(FAKE_WEBP));
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Decal;
  pattern->tileModeY = pagx::TileMode::Decal;
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("data:image/webp;base64,"), std::string::npos);
}

/**
 * Ellipse with the same rx/ry inside a rotated transform should emit a
 * <circle> with a transform attribute (covers the circle+transform branch).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EllipseAsCircleWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->x = 100;
  layer->y = 100;
  layer->matrix = {0.7071f, 0.7071f, -0.7071f, 0.7071f, 0.0f, 0.0f};
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {0, 0};
  ellipse->size = {50, 50};
  layer->contents.push_back(ellipse);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.7f, 0.2f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<circle"), std::string::npos);
}

/**
 * Ellipse with different rx and ry inside a rotated transform should emit
 * an <ellipse> with a transform attribute.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_TrueEllipseWithTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->x = 100;
  layer->y = 100;
  layer->matrix = {0.866f, 0.5f, -0.5f, 0.866f, 0.0f, 0.0f};
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {0, 0};
  ellipse->size = {80, 40};
  layer->contents.push_back(ellipse);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.5f, 0.5f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<ellipse"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

/**
 * Path emitted with renderPosition / renderScale offsets should produce a
 * transform="translate(...) scale(...)" string when the layer also has its
 * own matrix transform.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_PathWithLocalAndLayerTransform) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->x = 50;
  layer->y = 50;
  layer->matrix = {0.966f, 0.259f, -0.259f, 0.966f, 0.0f, 0.0f};
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(0, 0);
  pathData->lineTo(40, 0);
  pathData->lineTo(40, 40);
  pathData->close();
  path->data = pathData;
  path->position = {10, 10};
  layer->contents.push_back(path);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.2f, 0.2f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

/**
 * Test that text element with multi-line content emits multiple <tspan>
 * elements (covers the multi-line text rendering path in writeText).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_TextMultiLine) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Line 1\nLine 2\nLine 3";
  text->position = {20, 50};
  text->fontFamily = "Arial";
  text->fontSize = 18;
  auto* fill = MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f);
  auto* group = doc->makeNode<pagx::Group>();
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<text"), std::string::npos);
}

/**
 * Text with explicit textBox container exercises the writeTextWithLayout
 * code path that emits container-aware <tspan> positioning.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_TextInContainer) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* box = doc->makeNode<pagx::TextBox>();
  box->position = {20, 20};
  box->width = 200;
  box->height = 80;
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Wrapped text inside a box that should wrap across multiple lines";
  text->fontFamily = "Arial";
  text->fontSize = 14;
  auto* fill = MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f);
  box->elements.push_back(text);
  box->elements.push_back(fill);
  layer->contents.push_back(box);
  doc->layers.push_back(layer);

  pagx::SVGExporter::Options opts;
  opts.convertTextToPath = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_NE(svg.find("<text"), std::string::npos);
}

/**
 * Layer with 3D matrix should still produce a 2D affine transform via
 * BuildLayerMatrix's 3D->2D fallback.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_LayerMatrix3D) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->x = 50;
  layer->y = 50;
  pagx::Matrix3D m3d{};
  float vals[16] = {2.0f, 0.0f, 0.0f, 0.0f, 0.0f,  2.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 5.0f, 0.0f, 1.0f};
  for (int i = 0; i < 16; ++i) {
    m3d.values[i] = vals[i];
  }
  layer->matrix3D = m3d;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {30, 30};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.4f, 0.4f, 0.4f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<rect"), std::string::npos);
  EXPECT_NE(svg.find("transform="), std::string::npos);
}

// =====================================================================
// embedFontsAsWoff2 — vector Font + GlyphRun rendering paths
// =====================================================================
//
// These tests cover SVGExporter's WOFF2 / @font-face emission path introduced together with
// SVGExportOptions::embedFontsAsWoff2. The pre-pass in ToSVG generates a WOFF2 byte stream from
// each embedded vector Font and base64-embeds it as a `data:font/woff2;base64,...` URI inside a
// <style>@font-face{...}</style> declaration in <defs>. Text nodes whose GlyphRun references a
// participating Font are emitted as a real <text> element with PUA Unicode characters mapped
// 1:1 to glyph IDs (0xE000 + (gid-1)). The path output is reserved for cases the <text> element
// cannot losslessly express: convertTextToPath=true, embedFontsAsWoff2=false, bitmap fonts, runs
// with per-glyph scales/skews, and gradient fills.

// Builds a minimal vector Font with two square-ish glyph paths plus a Text containing a single
// GlyphRun that references it. Returns the doc so each test can tweak options independently.
static std::shared_ptr<pagx::PAGXDocument> MakeVectorGlyphRunDocSVG() {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto* font = doc->makeNode<pagx::Font>();
  font->id = "vectorFont";
  font->unitsPerEm = 1000;

  auto* glyph1 = doc->makeNode<pagx::Glyph>();
  glyph1->path = doc->makeNode<pagx::PathData>();
  *glyph1->path = pagx::PathDataFromSVGString("M 0,0 L 700,0 L 700,-800 L 0,-800 Z");
  glyph1->advance = 700;
  font->glyphs.push_back(glyph1);

  auto* glyph2 = doc->makeNode<pagx::Glyph>();
  glyph2->path = doc->makeNode<pagx::PathData>();
  *glyph2->path = pagx::PathDataFromSVGString("M 350,0 L 700,-800 L 0,-800 Z");
  glyph2->advance = 700;
  font->glyphs.push_back(glyph2);

  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  auto* text = doc->makeNode<pagx::Text>();
  text->fontSize = 50;

  auto* run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 50;
  run->glyphs = {1, 2};
  run->y = 60;
  run->xOffsets = {10, 60};
  text->glyphRuns.push_back(run);

  group->elements.push_back(text);
  group->elements.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f));
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  return doc;
}

/**
 * Default SVGExportOptions enables embedFontsAsWoff2. A vector Font referenced by a GlyphRun
 * should become a base64-encoded WOFF2 @font-face declaration, and the Text should emit a
 * <text> element pointing at the generated `pagx-font-*` family. No <path> should be emitted
 * for the run — the @font-face renders the glyph silhouettes via the embedded font.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_Default) {
  auto doc = MakeVectorGlyphRunDocSVG();

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("@font-face"), std::string::npos);
  EXPECT_NE(svg.find("data:font/woff2;base64,"), std::string::npos);
  EXPECT_NE(svg.find("pagx-font-"), std::string::npos);
  EXPECT_NE(svg.find("<text"), std::string::npos);
  // Generator-controlled family names follow the "pagx-font-<id>" pattern, which is a valid
  // unquoted CSS <custom-ident>. SVGExporter emits the bare identifier so the XMLBuilder does
  // not turn `'` into `&apos;` (a common source of cross-renderer quirks).
  EXPECT_NE(svg.find("font-family=\"pagx-font-"), std::string::npos);
  EXPECT_EQ(svg.find("&apos;"), std::string::npos);
  EXPECT_NE(svg.find("format('woff2')"), std::string::npos);
  EXPECT_EQ(svg.find("<path"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_default.svg");
}

/**
 * Disabling embedFontsAsWoff2 should suppress the @font-face emission entirely and route the
 * GlyphRun through writeTextAsPath, producing a <path> with the glyph outline geometry.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_Disabled) {
  auto doc = MakeVectorGlyphRunDocSVG();

  pagx::SVGExporter::Options opts;
  opts.embedFontsAsWoff2 = false;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_EQ(svg.find("@font-face"), std::string::npos);
  EXPECT_EQ(svg.find("data:font/woff2"), std::string::npos);
  EXPECT_EQ(svg.find("pagx-font-"), std::string::npos);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_disabled.svg");
}

/**
 * convertTextToPath=true is the user explicitly asking for outline geometry. The pre-pass must
 * skip WOFF2 generation regardless of embedFontsAsWoff2's value, and the GlyphRun must render
 * as a <path>.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_SuppressedByConvertTextToPath) {
  auto doc = MakeVectorGlyphRunDocSVG();

  pagx::SVGExporter::Options opts;
  opts.embedFontsAsWoff2 = true;
  opts.convertTextToPath = true;
  auto svg = pagx::SVGExporter::ToSVG(*doc, opts);
  EXPECT_EQ(svg.find("@font-face"), std::string::npos);
  EXPECT_EQ(svg.find("data:font/woff2"), std::string::npos);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_convert_to_path.svg");
}

/**
 * Bitmap (CBDT) fonts must be skipped by the WOFF2 pre-pass because plain SVG <text> cannot
 * express the per-glyph image transforms a bitmap glyph carries. The Text should fall back to
 * the writeTextAsPath / <image> rendering path and no @font-face declaration should appear.
 *
 * The test avoids relying on a real PNG decoder — Image.data is left empty, which is enough for
 * SVGExporter's pre-pass to inspect Glyph.image vs Glyph.path and choose its branch.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_BitmapFontFallback) {
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto* font = doc->makeNode<pagx::Font>();
  font->id = "bitmapFont";
  font->unitsPerEm = 100;

  auto* bitmapGlyph = doc->makeNode<pagx::Glyph>();
  bitmapGlyph->image = doc->makeNode<pagx::Image>();
  bitmapGlyph->advance = 60;
  font->glyphs.push_back(bitmapGlyph);

  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  auto* text = doc->makeNode<pagx::Text>();
  auto* run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 60;
  run->glyphs = {1};
  run->y = 60;
  text->glyphRuns.push_back(run);
  group->elements.push_back(text);
  group->elements.push_back(MakeSolidFillSVG(doc.get(), 0.0f, 0.0f, 0.0f));
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_EQ(svg.find("@font-face"), std::string::npos);
  EXPECT_EQ(svg.find("data:font/woff2"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_bitmap_fallback.svg");
}

/**
 * GlyphRuns carrying per-glyph scales (or skews) cannot be rendered through plain SVG <text>
 * because <text> has no per-character scale/skew attribute. writeTextAsFont returns false and
 * the writer falls back to writeTextAsPath. The @font-face declaration is still emitted (a
 * different Text in the same document might still use it), but this run renders as <path>.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_PerGlyphScaleFallback) {
  auto doc = MakeVectorGlyphRunDocSVG();
  // The doc has one GlyphRun; attach scales to force the fallback branch.
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() == pagx::NodeType::GlyphRun) {
      auto* run = static_cast<pagx::GlyphRun*>(nodePtr.get());
      run->scales = {{1.2f, 1.2f}, {0.8f, 0.8f}};
      break;
    }
  }
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // @font-face is still produced — the WOFF2 pre-pass runs per Font, not per GlyphRun.
  EXPECT_NE(svg.find("@font-face"), std::string::npos);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  // The fallback's defining contract is "no <text> element". Without this, a regression where
  // writeTextAsFont starts ignoring `scales` and emits a (visually wrong) <text> would still
  // pass the @font-face + <path> assertions above because both can appear via other paths.
  EXPECT_EQ(svg.find("<text"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_per_glyph_scale_fallback.svg");
}

/**
 * Gradient fills must fall back to writeTextAsPath because SVG <text> cannot losslessly clip a
 * gradient to glyph silhouettes — only individual <path> glyph elements can carry their own
 * userSpaceOnUse gradient transform. The @font-face is still emitted, but the GlyphRun renders
 * as <path>.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_GradientFillFallback) {
  auto doc = MakeVectorGlyphRunDocSVG();
  // Replace the Group's solid Fill with a linear gradient Fill.
  for (auto& nodePtr : doc->nodes) {
    if (nodePtr->nodeType() != pagx::NodeType::Group) {
      continue;
    }
    auto* group = static_cast<pagx::Group*>(nodePtr.get());
    for (auto& element : group->elements) {
      if (element->nodeType() != pagx::NodeType::Fill) {
        continue;
      }
      auto* fill = static_cast<pagx::Fill*>(element);
      auto* grad = doc->makeNode<pagx::LinearGradient>();
      grad->startPoint = {0, 0};
      grad->endPoint = {200, 0};
      auto* stop1 = doc->makeNode<pagx::ColorStop>();
      stop1->offset = 0.0f;
      stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
      auto* stop2 = doc->makeNode<pagx::ColorStop>();
      stop2->offset = 1.0f;
      stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
      grad->colorStops.push_back(stop1);
      grad->colorStops.push_back(stop2);
      fill->color = grad;
      break;
    }
    break;
  }

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("@font-face"), std::string::npos);
  EXPECT_NE(svg.find("<linearGradient"), std::string::npos);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  // Same as PerGlyphScaleFallback: assert <text> is absent so a regression that lets a gradient-
  // filled GlyphRun escape as <text> (and silently lose the glyph-clipped gradient) is caught.
  EXPECT_EQ(svg.find("<text"), std::string::npos);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_gradient_fallback.svg");
}

/**
 * Multiple distinct vector Fonts must each produce a unique @font-face declaration with a
 * unique family-name. The pre-pass assigns IDs sequentially ("f0", "f1", ...) and stores the
 * mapping by Font*; a regression that switched to a hash-keyed dictionary or that reused a
 * single counter across documents would either collapse two fonts onto one family or leak the
 * second font's family back to the first run, both of which break <text> rendering.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_MultipleFontsUnique) {
  auto doc = pagx::PAGXDocument::Make(400, 100);

  auto buildFont = [&](float advance) {
    auto* font = doc->makeNode<pagx::Font>();
    font->unitsPerEm = 1000;
    auto* glyph = doc->makeNode<pagx::Glyph>();
    glyph->path = doc->makeNode<pagx::PathData>();
    *glyph->path = pagx::PathDataFromSVGString("M 0,0 L 700,0 L 700,-800 L 0,-800 Z");
    glyph->advance = advance;
    font->glyphs.push_back(glyph);
    return font;
  };

  auto* fontA = buildFont(700);
  auto* fontB = buildFont(800);

  auto* layer = doc->makeNode<pagx::Layer>();
  auto buildText = [&](pagx::Font* font, float y) {
    auto* text = doc->makeNode<pagx::Text>();
    text->fontSize = 50;
    auto* run = doc->makeNode<pagx::GlyphRun>();
    run->font = font;
    run->fontSize = 50;
    run->glyphs = {1};
    run->y = y;
    run->xOffsets = {0};
    text->glyphRuns.push_back(run);
    layer->contents.push_back(text);
  };
  buildText(fontA, 50);
  buildText(fontB, 50);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Two @font-face rules must appear, with distinct generated family-names. fontId is assigned
  // by `"f" + std::to_string(woff2Fonts.size())`, so the first registered font yields
  // `pagx-font-f0` and the second yields `pagx-font-f1`. With deterministic emission order in
  // place we could check positions too, but the assertion below only requires both ids to
  // appear at least once.
  size_t fontFaceCount = 0;
  for (size_t pos = svg.find("@font-face"); pos != std::string::npos;
       pos = svg.find("@font-face", pos + 1)) {
    ++fontFaceCount;
  }
  EXPECT_EQ(fontFaceCount, 2u);
  EXPECT_NE(svg.find("pagx-font-f0"), std::string::npos);
  EXPECT_NE(svg.find("pagx-font-f1"), std::string::npos);
  // Each generated <text> must carry one of the two family-names so glyph-to-font wiring is
  // intact. A regression that emitted both <text> blocks under the same family would leave one
  // of them visually empty (the wrong cmap returns GID 0 for the PUA codepoints).
  size_t textCount = 0;
  std::set<std::string> textFamilies;
  for (size_t pos = svg.find("<text"); pos != std::string::npos; pos = svg.find("<text", pos + 1)) {
    ++textCount;
    // Locate the font-family attribute that belongs to THIS <text> opening tag: the search must
    // stop at the tag's closing `>` so a later element's font-family is not mistakenly picked
    // up. <text> tags here are always written with attributes and self-close inside the tag
    // header, so scanning for `>` is sufficient.
    auto tagEnd = svg.find('>', pos);
    if (tagEnd == std::string::npos) {
      continue;
    }
    auto attrPos = svg.find("font-family=\"", pos);
    if (attrPos == std::string::npos || attrPos > tagEnd) {
      continue;
    }
    auto valStart = attrPos + std::strlen("font-family=\"");
    auto valEnd = svg.find('"', valStart);
    if (valEnd == std::string::npos || valEnd > tagEnd) {
      continue;
    }
    textFamilies.insert(svg.substr(valStart, valEnd - valStart));
  }
  EXPECT_EQ(textCount, 2u);
  // Both <text> elements must reference distinct family names. Without this, a regression that
  // shadowed the per-Font lookup with `woff2Fonts.begin()` (or any single-font default) would
  // emit `<text font-family="pagx-font-f0">` twice while the f1 @font-face declaration sat
  // unused — the textCount + fontFaceCount + substring presence checks above would all still
  // pass.
  EXPECT_EQ(textFamilies.size(), 2u);
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_multiple_unique.svg");
}

/**
 * writeTextAsFont must apply TextBox.padding to renderPosition before computing absolute glyph
 * x/y. The padding offset reaches the run via `fs.textBox->padding.left/.top`. A regression that
 * dropped that branch would emit the <text x="..." y="..."> at the un-padded origin, putting
 * the glyphs on top of the box edge instead of inside the padded content area.
 *
 * The TextBox here participates as a *modifier* (empty `elements`) sharing the parent group's
 * scope. processVectorScope's `FindModifierTextBox` picks it up and threads it onto the
 * accumulated geometry so the padding reaches writeTextAsFont via FillStrokeInfo.textBox.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_EmbedFontsAsWoff2_TextBoxPaddingOffset) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto* glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M 0,0 L 700,0 L 700,-800 L 0,-800 Z");
  glyph->advance = 700;
  font->glyphs.push_back(glyph);

  auto* layer = doc->makeNode<pagx::Layer>();

  auto* text = doc->makeNode<pagx::Text>();
  text->fontSize = 50;
  auto* run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 50;
  run->glyphs = {1};
  // Choose y inside the box so the glyph sits at a position that is provably shifted by padding.
  // The glyph's authored x is 0 (no xOffsets / positions / run.x).
  run->y = 30;
  run->xOffsets = {0};
  text->glyphRuns.push_back(run);

  // Modifier-only TextBox: empty `elements`, lives next to the Text in the same container scope.
  auto* textBox = doc->makeNode<pagx::TextBox>();
  // Non-zero padding on every side. Only `left` and `top` participate in writeTextAsFont's
  // renderPos translation (the right/bottom edges only constrain layout, not paint position).
  textBox->padding.left = 12;
  textBox->padding.top = 17;
  textBox->padding.right = 5;
  textBox->padding.bottom = 5;

  layer->contents.push_back(textBox);
  layer->contents.push_back(text);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // Save first so the artifact is available for inspection regardless of assertion outcomes.
  SaveFile(svg, "PAGXSVGTest/svg_export_embed_fonts_woff2_textbox_padding.svg");
  // Ensure the <text> took the WOFF2 path (otherwise padding would land inside the per-glyph
  // path matrices instead of the <text x/y> attributes and the assertion would not be probing
  // the intended branch).
  ASSERT_NE(svg.find("@font-face"), std::string::npos);
  ASSERT_NE(svg.find("<text"), std::string::npos);
  // Our authored padding contributes 12 to the absolute x and 17 + 30 = 47 to the absolute y.
  // The serializer omits the trailing `.0` on whole numbers; assert on the integer literals.
  EXPECT_NE(svg.find(" x=\"12\""), std::string::npos);
  EXPECT_NE(svg.find(" y=\"47\""), std::string::npos);
}

// ===========================================================================
// ModifierResolver — additional edge branches reachable through SVG export.
// ===========================================================================

PAGX_TEST(PAGXSVGTest, SVGExport_ReversedPath) {
  // Path with reversed=true exercises the PrimitiveToTGFXPath reverse branch.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  *data = pagx::PathDataFromSVGString("M40 40 L160 40 L160 160 L40 160 Z");
  path->data = data;
  path->reversed = true;

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Separate;
  trim->start = 0.0f;
  trim->end = 0.5f;

  layer->contents.push_back(path);
  layer->contents.push_back(trim);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.4f, 0.5f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PathWithNullDataStaysEmpty) {
  // Path with data == nullptr should resolve to an empty tgfx::Path; trim
  // applied on top still emits a no-op (the original Path passes through).
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  path->data = nullptr;

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Separate;
  trim->start = 0.1f;
  trim->end = 0.6f;

  layer->contents.push_back(path);
  layer->contents.push_back(trim);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.4f, 0.7f, 0.2f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathContinuousReversed) {
  // start > end triggers the reversed branch in applyContinuousTrim.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect1 = doc->makeNode<pagx::Rectangle>();
  rect1->position = {80, 100};
  rect1->size = {60, 60};
  auto* rect2 = doc->makeNode<pagx::Rectangle>();
  rect2->position = {220, 100};
  rect2->size = {60, 60};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Continuous;
  trim->start = 0.8f;
  trim->end = 0.2f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.5f, 0.5f, 0.0f, 1};
  stroke->color = color;
  stroke->width = 3;

  layer->contents.push_back(rect1);
  layer->contents.push_back(rect2);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  // Reversed start/end (start=0.8, end=0.2) keeps the interval outside [start,end]: roughly
  // 60% of the combined path length stays painted, distributed across both shapes. Assert at
  // least two <path> elements survive so a regression that drops one shape is caught.
  size_t pathCount = 0;
  for (size_t pos = svg.find("<path"); pos != std::string::npos; pos = svg.find("<path", pos + 1)) {
    ++pathCount;
  }
  EXPECT_GE(pathCount, 2u);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathContinuousFullRange) {
  // start=0, end=1 → effect == nullptr in applyContinuousTrim.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 100};
  rect->size = {80, 80};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Continuous;
  trim->start = 0.0f;
  trim->end = 1.0f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.0f, 0.0f, 0.0f, 1};
  stroke->color = color;
  stroke->width = 2;

  layer->contents.push_back(rect);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
  // Full-range trim (PathEffect::MakeTrim returns nullptr) should leave the rectangle intact:
  // exactly one <path> with full extent. Without these checks, a regression that silently
  // dropped the geometry or split it into multiple sub-paths would still pass the loose
  // "<path exists" assertion.
  size_t pathCount = 0;
  for (size_t pos = svg.find("<path"); pos != std::string::npos; pos = svg.find("<path", pos + 1)) {
    ++pathCount;
  }
  EXPECT_EQ(pathCount, 1u);
  // The rectangle author width is 80, centred at (200, 100) → corners at x=160 and x=240. The
  // serializer emits absolute LineTo segments (`L<x> <y>`), so look for either x corner — the
  // far x in particular catches a regression that silently truncated the rectangle's extent.
  EXPECT_TRUE(svg.find("L240") != std::string::npos || svg.find("L 240") != std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathContinuousWrapAround) {
  // end > 1 forces the wrap-around branch (trimEnd > totalLength).
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {80, 100};
  r1->size = {60, 60};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {220, 100};
  r2->size = {60, 60};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Continuous;
  trim->start = 0.7f;
  trim->end = 1.2f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.0f, 0.5f, 0.5f, 1};
  stroke->color = color;
  stroke->width = 2;

  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  // Wrap-around (end > 1) keeps two segments: the tail [0.7, 1.0] and the head [0.0, 0.2]. Both
  // should land on the same shape (r1) — but the implementation distributes per shape so the
  // segments may be split into two <path> elements. Either way, at least one <path> must remain;
  // verifying ≥1 distinguishes "trim fired" from a regression that drops every shape.
  size_t pathCount = 0;
  for (size_t pos = svg.find("<path"); pos != std::string::npos; pos = svg.find("<path", pos + 1)) {
    ++pathCount;
  }
  EXPECT_GE(pathCount, 1u);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterClampsExcessiveCopies) {
  // copies above MAX_REPEATER_COPIES (10000) clamp; emitted output should be
  // bounded but not crash.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {2, 2};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 1000000.0f;
  rep->position = {0, 0};
  rep->scale = {1, 1};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RepeaterNegativeScale) {
  // Negative scale exercises SignedPow's negative branch.
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {80, 100};
  rect->size = {30, 30};
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  rep->position = {60, 0};
  rep->scale = {-1.0f, 1.0f};
  rep->offset = 0.5f;

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.5f, 0.2f, 0.5f));
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<svg"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathDifferenceForcesEvenOddFill) {
  // Difference produces an even-odd-oriented combined path; the resolver
  // should re-stamp the Fill's fillRule to EvenOdd so downstream renderers
  // punch out the inner contour. Verify by querying the exported SVG.
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* outer = doc->makeNode<pagx::Rectangle>();
  outer->position = {150, 150};
  outer->size = {200, 200};
  auto* inner = doc->makeNode<pagx::Rectangle>();
  inner->position = {150, 150};
  inner->size = {80, 80};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Difference;
  layer->contents.push_back(outer);
  layer->contents.push_back(inner);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.4f, 0.8f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("evenodd"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_MergePathAppendDefaultMode) {
  // Default-constructed MergePath uses Append mode, mapping to PathOp::Union.
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {120, 100};
  r1->size = {80, 80};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {210, 100};
  r2->size = {80, 80};
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Append;
  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.6f, 0.4f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_PolygonReversed) {
  // Reversed polygon exercises the direction = -1 branch in PolygonToTGFXPath.
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {150, 150};
  poly->pointCount = 5;
  poly->outerRadius = 70;
  poly->reversed = true;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.6f, 0.2f, 0.4f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_StarFractionalPointsReversed) {
  // Fractional points + reversed exercises the decimalIndex = numPoints - 3
  // branch in StarToTGFXPath.
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {150, 150};
  star->pointCount = 4.5f;
  star->outerRadius = 80;
  star->innerRadius = 40;
  star->reversed = true;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.3f, 0.7f, 0.6f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_RoundCornerOnPathPrimitive) {
  // Apply RoundCorner to a Path (not Rectangle) so applyRoundCornerToElement
  // routes through PrimitiveToTGFXPath's NodeType::Path branch.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* data = doc->makeNode<pagx::PathData>();
  *data = pagx::PathDataFromSVGString("M40 40 L160 40 L160 160 L40 160 Z");
  path->data = data;
  auto* rc = doc->makeNode<pagx::RoundCorner>();
  rc->radius = 12.0f;
  layer->contents.push_back(path);
  layer->contents.push_back(rc);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.8f, 0.4f, 0.2f));
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

PAGX_TEST(PAGXSVGTest, SVGExport_TrimPathOnEllipse) {
  // Apply TrimPath to an Ellipse to hit PrimitiveToTGFXPath's Ellipse branch.
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
  ellipse->size = {120, 80};
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->type = pagx::TrimType::Separate;
  trim->start = 0.0f;
  trim->end = 0.5f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  auto* color = doc->makeNode<pagx::SolidColor>();
  color->color = {0.6f, 0.0f, 0.6f, 1};
  stroke->color = color;
  stroke->width = 3;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  auto svg = pagx::SVGExporter::ToSVG(*doc);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

// ===========================================================================
// Regression tests for PR-feedback fixes: lock down the behaviours so future
// edits cannot silently undo them.
// ===========================================================================

/**
 * SVG painter alpha must come from the Painter's enclosing scope, NOT from the geometry's
 * accumulated alpha at the moment it propagated upward. Earlier code attached `entry.alpha`
 * (the alpha in effect when the geometry was collected) to each emit, so a Painter sitting
 * OUTSIDE a 50%-alpha Group would multiply 0.5 onto the geometry that bubbled up from inside —
 * making the outer red Fill render at half opacity even though the Painter's own scope has
 * alpha=1. The fix: emitGeometryWithFs receives the Painter's scope alpha as an explicit
 * argument, and applies that. Regression: if entry.alpha sneaks back in, the outer fill would
 * be emitted with `fill-opacity="0.5"` or equivalent.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_PaintersOutsideGroupIgnoreGroupAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  group->alpha = 0.5f;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 80};
  group->elements.push_back(rect);
  layer->contents.push_back(group);
  // Painter lives in the layer scope, OUTSIDE the group. With the fix, its alpha is the layer's
  // alpha (1.0). Without the fix, the rect's `entry.alpha = 0.5` (captured inside the group)
  // gets multiplied in.
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 1.0f, 0.0f, 0.0f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The fill emits `fill="rgb(255,0,0)"`. Any fill-opacity attribute at all would indicate the
  // painter took on the group's 0.5 alpha. Both writeRectangle's `fill-opacity` and the inline
  // `;opacity:0.5` style spellings would trigger the regression, so check both shapes.
  EXPECT_EQ(svg.find("fill-opacity"), std::string::npos);
  EXPECT_EQ(svg.find("opacity:0.5"), std::string::npos);
  EXPECT_EQ(svg.find("opacity=\"0.5\""), std::string::npos);
}

/**
 * Layers with a mask and a non-identity transform split into an outer <g> carrying the mask
 * attribute and an inner <g> carrying the transform. SVG resolves `mask` in the referencing
 * element's user coordinate system, so a layer-level transform on the same <g> as `mask=` would
 * drag the mask along with it — visibly drifting from where the renderer places it (which is
 * the owner's PARENT space). The fix: outer wraps mask+opacity+filter only, inner takes the
 * transform. Regression: if the split collapses back, the mask attribute would appear on the
 * same <g> as `translate(...)`.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MaskOuterTransformIsolation) {
  auto doc = pagx::PAGXDocument::Make(300, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* user = doc->makeNode<pagx::Layer>();
  user->mask = maskLayer;
  user->maskType = pagx::MaskType::Alpha;
  // Non-identity transform: position the layer 50 pixels right and down.
  user->matrix = pagx::Matrix::Translate(50, 50);
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {100, 100};
  user->contents.push_back(rect);
  user->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(user);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The outer <g> must carry the mask attribute and NOT the transform; an inner <g> takes the
  // transform. Locate the mask attribute and assert the same opening tag has no `transform=`.
  auto maskAttr = svg.find("mask=\"url(#mask");
  ASSERT_NE(maskAttr, std::string::npos);
  // Find the surrounding <g opener and its closing `>`. The transform attribute, if present,
  // must NOT belong to that opener.
  auto tagStart = svg.rfind("<g", maskAttr);
  auto tagEnd = svg.find('>', maskAttr);
  ASSERT_NE(tagStart, std::string::npos);
  ASSERT_NE(tagEnd, std::string::npos);
  auto outerTag = svg.substr(tagStart, tagEnd - tagStart);
  EXPECT_EQ(outerTag.find("transform="), std::string::npos);
  // And the inner wrapper must exist with the layer's matrix. SVG serializes affine transforms
  // as `matrix(a,b,c,d,tx,ty)`; a pure translation by (50,50) becomes `matrix(1,0,0,1,50,50)`.
  EXPECT_NE(svg.find("matrix(1,0,0,1,50,50)"), std::string::npos);
}

/**
 * A Foreground-placement Painter buried inside a nested Group must still trigger the layer's
 * second paint pass. The fix moved `hasForeground` from a shallow contents-list scan to a
 * deep traversal (HasForegroundPainter in ExporterUtils) that walks Group / TextBox children.
 * Regression: if the recursion is dropped, `hasForeground` returns false, the second pass is
 * skipped, and the inner Foreground Fill applies to nothing — the rect renders un-filled
 * (default black) instead of red.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_NestedGroupForegroundPainter) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 100};
  group->elements.push_back(rect);
  auto* fgFill = MakeSolidFillSVG(doc.get(), 1.0f, 0.0f, 0.0f);
  fgFill->placement = pagx::LayerPlacement::Foreground;
  group->elements.push_back(fgFill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // The rect must be filled red. Colors are serialized as `#RRGGBB` (sRGB hex). Without the
  // deep-foreground detection, the second pass is skipped and the rect either emits with default
  // fill (`#000000`) or not at all.
  EXPECT_NE(svg.find("fill=\"#FF0000\""), std::string::npos);
}

/**
 * A Text whose GlyphRuns disagree on `fontSize` cannot be expressed by a single <text> element
 * (SVG <text> assigns one font-size to all glyphs). The fix: writeTextAsFont returns false on
 * mismatched sizes so the caller falls back to writeTextAsPath, which emits one <path> per
 * glyph at the correct authored scale. Regression: if the fontSize-uniformity check is
 * removed, both runs render at one size — the larger run is squashed onto the smaller's
 * font-size attribute, producing visibly mis-scaled glyphs.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_MultipleGlyphRunFontSizesFallToPath) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto* font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto* glyph = doc->makeNode<pagx::Glyph>();
  glyph->path = doc->makeNode<pagx::PathData>();
  *glyph->path = pagx::PathDataFromSVGString("M 0,0 L 700,0 L 700,-800 L 0,-800 Z");
  glyph->advance = 700;
  font->glyphs.push_back(glyph);

  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->fontSize = 24;
  auto* run1 = doc->makeNode<pagx::GlyphRun>();
  run1->font = font;
  run1->fontSize = 12;
  run1->glyphs = {1};
  run1->xOffsets = {0};
  run1->y = 50;
  text->glyphRuns.push_back(run1);
  auto* run2 = doc->makeNode<pagx::GlyphRun>();
  run2->font = font;
  run2->fontSize = 24;
  run2->glyphs = {1};
  run2->xOffsets = {30};
  run2->y = 50;
  text->glyphRuns.push_back(run2);
  layer->contents.push_back(text);
  layer->contents.push_back(MakeSolidFillSVG(doc.get(), 0.1f, 0.1f, 0.1f));
  doc->layers.push_back(layer);

  auto svg = pagx::SVGExporter::ToSVG(*doc);
  // No <text> element — falling back to <path> per glyph at the authored size.
  EXPECT_EQ(svg.find("<text"), std::string::npos);
  EXPECT_NE(svg.find("<path"), std::string::npos);
}

/**
 * A single mask Layer referenced by two owners under different MaskType values must produce
 * two distinct <mask> defs with the correct mask-type attribute on each. Sharing one cache
 * slot across MaskTypes silently routes the second owner to the wrong mask channel (alpha
 * vs luminance).
 */
PAGX_TEST(PAGXSVGTest, SVGExport_SharedMaskLayerDifferentMaskTypesEmitDistinctDefs) {
  auto doc = pagx::PAGXDocument::Make(300, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "sharedMask";
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* alphaOwner = doc->makeNode<pagx::Layer>();
  alphaOwner->mask = maskLayer;
  alphaOwner->maskType = pagx::MaskType::Alpha;
  auto* alphaRect = doc->makeNode<pagx::Rectangle>();
  alphaRect->position = {50, 50};
  alphaRect->size = {100, 100};
  alphaOwner->contents.push_back(alphaRect);
  alphaOwner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(alphaOwner);

  auto* lumOwner = doc->makeNode<pagx::Layer>();
  lumOwner->mask = maskLayer;
  lumOwner->maskType = pagx::MaskType::Luminance;
  auto* lumRect = doc->makeNode<pagx::Rectangle>();
  lumRect->position = {160, 50};
  lumRect->size = {100, 100};
  lumOwner->contents.push_back(lumRect);
  lumOwner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.9f, 0.6f, 0.2f));
  doc->layers.push_back(lumOwner);

  auto svg = pagx::SVGExporter::ToSVG(*doc);

  // Exactly two <mask> elements emitted, one per MaskType.
  size_t maskOpenCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("<mask ", pos)) != std::string::npos) {
    ++maskOpenCount;
    pos += 6;
  }
  EXPECT_EQ(maskOpenCount, 2u);

  // Both ids derive from the same layer id but carry MaskType-specific suffixes so the def ids
  // don't collide in <defs>.
  auto alphaIdPos = svg.find("id=\"sharedMask_a\"");
  auto lumIdPos = svg.find("id=\"sharedMask_l\"");
  ASSERT_NE(alphaIdPos, std::string::npos);
  ASSERT_NE(lumIdPos, std::string::npos);

  // The alpha def carries mask-type:alpha; the luminance def does not (luminance is the SVG
  // default for <mask> and no style attribute is emitted).
  auto alphaTagEnd = svg.find('>', alphaIdPos);
  ASSERT_NE(alphaTagEnd, std::string::npos);
  auto alphaTag = svg.substr(alphaIdPos, alphaTagEnd - alphaIdPos);
  EXPECT_NE(alphaTag.find("mask-type:alpha"), std::string::npos);

  auto lumTagEnd = svg.find('>', lumIdPos);
  ASSERT_NE(lumTagEnd, std::string::npos);
  auto lumTag = svg.substr(lumIdPos, lumTagEnd - lumIdPos);
  EXPECT_EQ(lumTag.find("mask-type:alpha"), std::string::npos);

  // Each owner references its own def id.
  EXPECT_NE(svg.find("mask=\"url(#sharedMask_a)\""), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#sharedMask_l)\""), std::string::npos);
}

/**
 * A mask Layer referenced by multiple owners under the same MaskType must produce exactly one
 * <mask> def. The cache reuses the existing def id rather than re-emitting a duplicate-id
 * element into <defs>.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_SharedMaskLayerSameMaskTypeReusesSingleDef) {
  auto doc = pagx::PAGXDocument::Make(300, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "alphaMask";
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  for (int i = 0; i < 3; ++i) {
    auto* owner = doc->makeNode<pagx::Layer>();
    owner->mask = maskLayer;
    owner->maskType = pagx::MaskType::Alpha;
    auto* rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {static_cast<float>(50 + i * 60), 50};
    rect->size = {80, 80};
    owner->contents.push_back(rect);
    owner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
    doc->layers.push_back(owner);
  }

  auto svg = pagx::SVGExporter::ToSVG(*doc);

  size_t maskOpenCount = 0;
  size_t pos = 0;
  while ((pos = svg.find("<mask ", pos)) != std::string::npos) {
    ++maskOpenCount;
    pos += 6;
  }
  EXPECT_EQ(maskOpenCount, 1u);

  // All three owners reference the same def id.
  size_t refCount = 0;
  pos = 0;
  while ((pos = svg.find("mask=\"url(#alphaMask_a)\"", pos)) != std::string::npos) {
    ++refCount;
    pos += 1;
  }
  EXPECT_EQ(refCount, 3u);
}

/**
 * A mask Layer used as both Alpha mask and Contour clipPath must produce one <mask> def AND
 * one <clipPath> def. Sharing the cache across roles would have suppressed the second def or
 * routed the contour owner to the wrong element type.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_SharedMaskLayerMaskAndContourEmitDistinctDefs) {
  auto doc = pagx::PAGXDocument::Make(300, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "dualMask";
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {80, 80};
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));

  auto* alphaOwner = doc->makeNode<pagx::Layer>();
  alphaOwner->mask = maskLayer;
  alphaOwner->maskType = pagx::MaskType::Alpha;
  auto* alphaRect = doc->makeNode<pagx::Rectangle>();
  alphaRect->position = {50, 50};
  alphaRect->size = {100, 100};
  alphaOwner->contents.push_back(alphaRect);
  alphaOwner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  doc->layers.push_back(alphaOwner);

  auto* contourOwner = doc->makeNode<pagx::Layer>();
  contourOwner->mask = maskLayer;
  contourOwner->maskType = pagx::MaskType::Contour;
  auto* contourRect = doc->makeNode<pagx::Rectangle>();
  contourRect->position = {160, 50};
  contourRect->size = {100, 100};
  contourOwner->contents.push_back(contourRect);
  contourOwner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.9f, 0.6f, 0.2f));
  doc->layers.push_back(contourOwner);

  auto svg = pagx::SVGExporter::ToSVG(*doc);

  EXPECT_NE(svg.find("id=\"dualMask_a\""), std::string::npos);
  EXPECT_NE(svg.find("id=\"dualMask_c\""), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#dualMask_a)\""), std::string::npos);
  EXPECT_NE(svg.find("clip-path=\"url(#dualMask_c)\""), std::string::npos);
}

/**
 * When a mask Layer and its owner live under different parent layers, the SVG output places
 * the mask using mask->matrix only — diverging from the PAGX renderer's
 * mask->getRelativeMatrix3D(owner) chain walk. The exporter must surface a warning so the
 * divergence is visible to CI / log readers, while still emitting the <mask> def so the
 * owner is not silently dropped from the SVG.
 */
PAGX_TEST(PAGXSVGTest, SVGExport_CrossParentMaskEmitsWarningButKeepsDef) {
  auto doc = pagx::PAGXDocument::Make(400, 400);

  auto* topA = doc->makeNode<pagx::Layer>();
  topA->id = "topA";
  auto* maskLayer = doc->makeNode<pagx::Layer>();
  maskLayer->id = "crossParentMask";
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {50, 50};
  maskRect->size = {80, 80};
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(MakeSolidFillSVG(doc.get(), 1, 1, 1));
  topA->children.push_back(maskLayer);
  doc->layers.push_back(topA);

  auto* topB = doc->makeNode<pagx::Layer>();
  topB->id = "topB";
  auto* owner = doc->makeNode<pagx::Layer>();
  owner->id = "crossParentOwner";
  owner->mask = maskLayer;
  owner->maskType = pagx::MaskType::Alpha;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {120, 120};
  rect->size = {100, 100};
  owner->contents.push_back(rect);
  owner->contents.push_back(MakeSolidFillSVG(doc.get(), 0.2f, 0.6f, 0.9f));
  topB->children.push_back(owner);
  doc->layers.push_back(topB);

  std::vector<std::string> warnings;
  auto svg = pagx::SVGExporter::ToSVG(*doc, {}, &warnings);

  // The cross-parent reference must be flagged for CI / log visibility.
  bool sawCrossParentWarning = false;
  for (const auto& w : warnings) {
    if (w.find("crossParentMask") != std::string::npos &&
        w.find("crossParentOwner") != std::string::npos &&
        w.find("across parent boundaries") != std::string::npos) {
      sawCrossParentWarning = true;
      break;
    }
  }
  EXPECT_TRUE(sawCrossParentWarning);

  // Emission is preserved (warning-only, not skip): the <mask> def appears in <defs> and the
  // owner still carries the mask attribute. A regression that silently drops the def would
  // leave the SVG without `<mask id=...>` and the owner without its `mask=` attribute.
  EXPECT_NE(svg.find("id=\"crossParentMask_a\""), std::string::npos);
  EXPECT_NE(svg.find("mask=\"url(#crossParentMask_a)\""), std::string::npos);
}

}  // namespace pag
