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

#ifdef PAG_BUILD_PPT

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PPTExporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
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
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
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
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

static std::string PPTOutDir() {
  auto dir = ProjectPath::Absolute("test/out/PAGXPPTTest");
  if (!std::filesystem::exists(dir)) {
    std::filesystem::create_directories(dir);
  }
  return dir;
}

static bool ExportAndVerify(pagx::PAGXDocument& doc, const std::string& name,
                            const pagx::PPTExportOptions& options = {}) {
  auto path = PPTOutDir() + "/" + name + ".pptx";
  bool ok = pagx::PPTExporter::ToFile(doc, path, options);
  if (ok) {
    ok = std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
  }
  return ok;
}

/**
 * Read all SVG files from resources/svg, convert each to PAGX, then export to PPTX.
 */
PAGX_TEST(PAGXPPTTest, PPTExport_FromSVG) {
  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles;

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());
  ASSERT_FALSE(svgFiles.empty()) << "No SVG files found in resources/svg";

  auto outDir = PPTOutDir();

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      continue;
    }
    if (doc->width <= 0 || doc->height <= 0) {
      continue;
    }

    auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
    ASSERT_FALSE(pagxXml.empty()) << baseName << " PAGX export failed";

    auto pagxPath = outDir + "/" + baseName + ".pagx";
    {
      std::ofstream file(pagxPath, std::ios::binary);
      ASSERT_TRUE(file.good()) << "Failed to write " << pagxPath;
      file.write(pagxXml.data(), static_cast<std::streamsize>(pagxXml.size()));
    }

    auto reimported = pagx::PAGXImporter::FromFile(pagxPath);
    ASSERT_NE(reimported, nullptr) << baseName << " PAGX re-import failed";

    auto pptxPath = outDir + "/" + baseName + ".pptx";
    ASSERT_TRUE(pagx::PPTExporter::ToFile(*reimported, pptxPath))
        << baseName << " PPT export failed";
    EXPECT_TRUE(std::filesystem::exists(pptxPath));
    EXPECT_GT(std::filesystem::file_size(pptxPath), 0u) << baseName << " PPTX file is empty";
  }
}

PAGX_TEST(PAGXPPTTest, EmptyDocument) {
  auto doc = pagx::PAGXDocument::Make(800, 600);
  ASSERT_TRUE(ExportAndVerify(*doc, "empty_doc"));
}

PAGX_TEST(PAGXPPTTest, RectangleSolidFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "rect_solid_fill"));
}

PAGX_TEST(PAGXPPTTest, RectangleRoundCorners) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};
  rect->roundness = 20.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.5f, 1.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "rect_round_corners"));
}

PAGX_TEST(PAGXPPTTest, EllipseSolidFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {180, 120};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 1.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "ellipse_solid_fill"));
}

PAGX_TEST(PAGXPPTTest, PathWithCubicBezier) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 250);
  pathData->cubicTo(100, 50, 300, 50, 350, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.0f, 0.8f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_cubic_bezier"));
}

PAGX_TEST(PAGXPPTTest, PathWithQuadBezier) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 250);
  pathData->quadTo(200, 20, 350, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.6f, 0.3f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_quad_bezier"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddFillRule) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Outer square
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  // Inner square (will be a hole with EvenOdd)
  pathData->moveTo(150, 150);
  pathData->lineTo(250, 150);
  pathData->lineTo(250, 250);
  pathData->lineTo(150, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.4f, 0.8f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd"));
}

PAGX_TEST(PAGXPPTTest, EmptyPathSkipped) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "empty_path_skipped"));
}

PAGX_TEST(PAGXPPTTest, NullPathDataSkipped) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "null_pathdata_skipped"));
}

PAGX_TEST(PAGXPPTTest, LinearGradientFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {300, 200};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {1, 1};
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "linear_gradient"));
}

PAGX_TEST(PAGXPPTTest, RadialGradientFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {250, 250};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 1.0f, 0.0f, 1.0f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {1.0f, 0.0f, 1.0f, 0.5f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "radial_gradient"));
}

PAGX_TEST(PAGXPPTTest, GradientStopWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {300, 200};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {1, 0};
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 0.3f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 0.5f;
  stop2->color = {0.0f, 1.0f, 0.0f, 1.0f};
  auto* stop3 = doc->makeNode<pagx::ColorStop>();
  stop3->offset = 1.0f;
  stop3->color = {0.0f, 0.0f, 1.0f, 0.7f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  grad->colorStops.push_back(stop3);
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "gradient_stop_alpha"));
}

PAGX_TEST(PAGXPPTTest, SolidColorWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->alpha = 0.5f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 1.0f, 0.8f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "solid_color_alpha"));
}

PAGX_TEST(PAGXPPTTest, NoFillNoStroke) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  layer->contents.push_back(rect);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "no_fill_no_stroke"));
}

PAGX_TEST(PAGXPPTTest, FillWithNullColor) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};
  auto* fill = doc->makeNode<pagx::Fill>();

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "fill_null_color"));
}

PAGX_TEST(PAGXPPTTest, StrokeSolidColor) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_solid_color"));
}

PAGX_TEST(PAGXPPTTest, StrokeRoundCapAndJoin) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(200, 50);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 8.0f;
  stroke->cap = pagx::LineCap::Round;
  stroke->join = pagx::LineJoin::Round;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.2f, 0.2f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_round_cap_join"));
}

PAGX_TEST(PAGXPPTTest, StrokeSquareCapBevelJoin) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 250);
  pathData->lineTo(200, 50);
  pathData->lineTo(350, 250);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 6.0f;
  stroke->cap = pagx::LineCap::Square;
  stroke->join = pagx::LineJoin::Bevel;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.5f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_square_bevel"));
}

PAGX_TEST(PAGXPPTTest, StrokeMiterJoin) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 250);
  pathData->lineTo(200, 50);
  pathData->lineTo(350, 250);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 6.0f;
  stroke->join = pagx::LineJoin::Miter;
  stroke->miterLimit = 10.0f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.3f, 0.8f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_miter"));
}

PAGX_TEST(PAGXPPTTest, StrokeNoColor) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;

  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_no_color"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashSysDot) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_dash_sysdot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashLong) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {24.0f, 12.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_dash_long"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashDot4) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {12.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_dash_dot4"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashDotDot6) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {12.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_dash_dotdot6"));
}

PAGX_TEST(PAGXPPTTest, StrokeCustomDash) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {20.0f, 8.0f, 4.0f, 8.0f, 4.0f, 8.0f, 4.0f, 8.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_custom_dash"));
}

PAGX_TEST(PAGXPPTTest, StrokeOddDashCount) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 150);
  pathData->lineTo(350, 150);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {20.0f, 8.0f, 12.0f, 6.0f, 8.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_odd_dash_count"));
}

PAGX_TEST(PAGXPPTTest, StrokeWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 5.0f;
  stroke->alpha = 0.5f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_with_alpha"));
}

PAGX_TEST(PAGXPPTTest, FillAndStrokeCombined) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* fillColor = doc->makeNode<pagx::SolidColor>();
  fillColor->color = {0.9f, 0.9f, 0.2f, 1.0f};
  fill->color = fillColor;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 3.0f;
  auto* strokeColor = doc->makeNode<pagx::SolidColor>();
  strokeColor->color = {0.2f, 0.2f, 0.2f, 1.0f};
  stroke->color = strokeColor;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "fill_and_stroke"));
}

PAGX_TEST(PAGXPPTTest, NativeText) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Hello PPT";
  text->position = {100, 150};
  text->fontFamily = "Arial";
  text->fontSize = 36.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextEmpty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->position = {100, 150};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_empty", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextBoldItalic) {
  auto doc = pagx::PAGXDocument::Make(500, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Bold Italic";
  text->position = {50, 150};
  text->fontFamily = "\"Times New Roman\"";
  text->fontStyle = "Bold Italic";
  text->fontSize = 32.0f;
  text->letterSpacing = 2.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.0f, 0.5f, 0.8f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_bold_italic", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextCenterAlign) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Centered";
  text->position = {200, 150};
  text->fontSize = 28.0f;
  text->textAnchor = pagx::TextAnchor::Center;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_center", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextEndAlign) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Right Aligned";
  text->position = {350, 150};
  text->fontSize = 24.0f;
  text->textAnchor = pagx::TextAnchor::End;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_end_align", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextMultiline) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Line 1\nLine 2\nLine 3";
  text->position = {50, 100};
  text->fontSize = 24.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_multiline", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithTextBox) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "In Box";
  text->position = {50, 100};
  text->fontSize = 24.0f;

  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;
  textBox->height = 200;
  textBox->textAlign = pagx::TextAlign::Center;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(textBox);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_textbox", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithTextBoxEndAlign) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Right";
  text->position = {50, 100};
  text->fontSize = 24.0f;

  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;
  textBox->height = 200;
  textBox->textAlign = pagx::TextAlign::End;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(textBox);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_textbox_end", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextNoFontFamily) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "No Font";
  text->position = {100, 150};
  text->fontSize = 24.0f;

  layer->contents.push_back(text);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_no_font", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextNoFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "No fill";
  text->position = {100, 150};
  text->fontSize = 28.0f;
  text->fontFamily = "Arial";

  layer->contents.push_back(text);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_no_fill", options));
}

PAGX_TEST(PAGXPPTTest, GroupWithTransform) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* group = doc->makeNode<pagx::Group>();
  group->position = {200, 200};
  group->rotation = 45.0f;
  group->scale = {1.5f, 1.5f};
  group->alpha = 0.8f;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {80, 60};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.7f, 0.3f, 1.0f};
  fill->color = solid;

  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "group_with_transform"));
}

PAGX_TEST(PAGXPPTTest, GroupWithAnchor) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* group = doc->makeNode<pagx::Group>();
  group->anchor = {40, 30};
  group->position = {200, 200};
  group->rotation = 30.0f;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {40, 30};
  rect->size = {80, 60};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.4f, 0.0f, 1.0f};
  fill->color = solid;

  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "group_with_anchor"));
}

PAGX_TEST(PAGXPPTTest, GroupWithSkew) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* group = doc->makeNode<pagx::Group>();
  group->position = {200, 200};
  group->skew = 20.0f;
  group->skewAxis = 45.0f;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {100, 80};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.6f, 0.2f, 0.8f, 1.0f};
  fill->color = solid;

  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "group_with_skew"));
}

PAGX_TEST(PAGXPPTTest, NestedGroups) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* outerGroup = doc->makeNode<pagx::Group>();
  outerGroup->position = {100, 100};

  auto* innerGroup = doc->makeNode<pagx::Group>();
  innerGroup->position = {50, 50};
  innerGroup->rotation = 15.0f;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {60, 40};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;

  innerGroup->elements.push_back(rect);
  innerGroup->elements.push_back(fill);
  outerGroup->elements.push_back(innerGroup);
  layer->contents.push_back(outerGroup);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "nested_groups"));
}

PAGX_TEST(PAGXPPTTest, LayerWithTranslation) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->x = 50;
  layer->y = 30;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 60};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.5f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "layer_with_translation"));
}

PAGX_TEST(PAGXPPTTest, LayerWithMatrix) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Scale(2.0f, 1.5f);

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 40};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "layer_with_matrix"));
}

PAGX_TEST(PAGXPPTTest, LayerWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->alpha = 0.5f;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "layer_with_alpha"));
}

PAGX_TEST(PAGXPPTTest, InvisibleLayerSkipped) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->visible = false;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "invisible_layer"));
}

PAGX_TEST(PAGXPPTTest, ChildLayers) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* parentLayer = doc->makeNode<pagx::Layer>();
  parentLayer->x = 10;
  parentLayer->y = 10;

  auto* childLayer = doc->makeNode<pagx::Layer>();
  childLayer->x = 50;
  childLayer->y = 50;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 60};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.0f, 0.5f, 1.0f};
  fill->color = solid;

  childLayer->contents.push_back(rect);
  childLayer->contents.push_back(fill);
  parentLayer->children.push_back(childLayer);
  doc->layers.push_back(parentLayer);

  ASSERT_TRUE(ExportAndVerify(*doc, "child_layers"));
}

PAGX_TEST(PAGXPPTTest, MultipleLayers) {
  auto doc = pagx::PAGXDocument::Make(400, 400);

  auto* layer1 = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {150, 100};
  auto* fill1 = doc->makeNode<pagx::Fill>();
  auto* solid1 = doc->makeNode<pagx::SolidColor>();
  solid1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill1->color = solid1;
  layer1->contents.push_back(rect);
  layer1->contents.push_back(fill1);

  auto* layer2 = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {300, 300};
  ellipse->size = {100, 100};
  auto* fill2 = doc->makeNode<pagx::Fill>();
  auto* solid2 = doc->makeNode<pagx::SolidColor>();
  solid2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill2->color = solid2;
  layer2->contents.push_back(ellipse);
  layer2->contents.push_back(fill2);

  doc->layers.push_back(layer1);
  doc->layers.push_back(layer2);

  ASSERT_TRUE(ExportAndVerify(*doc, "multiple_layers"));
}

PAGX_TEST(PAGXPPTTest, DropShadowFilter) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.6f, 0.9f, 1.0f};
  fill->color = solid;

  auto* shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->offsetX = 5.0f;
  shadow->offsetY = 5.0f;
  shadow->blurX = 10.0f;
  shadow->blurY = 10.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.5f};

  layer->filters.push_back(shadow);
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "drop_shadow"));
}

PAGX_TEST(PAGXPPTTest, InnerShadowFilter) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.9f, 0.2f, 1.0f};
  fill->color = solid;

  auto* shadow = doc->makeNode<pagx::InnerShadowFilter>();
  shadow->offsetX = 3.0f;
  shadow->offsetY = 3.0f;
  shadow->blurX = 8.0f;
  shadow->blurY = 8.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.4f};

  layer->filters.push_back(shadow);
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "inner_shadow"));
}

PAGX_TEST(PAGXPPTTest, BothShadowFilters) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.7f, 0.3f, 0.9f, 1.0f};
  fill->color = solid;

  auto* dropShadow = doc->makeNode<pagx::DropShadowFilter>();
  dropShadow->offsetX = 4.0f;
  dropShadow->offsetY = 4.0f;
  dropShadow->blurX = 6.0f;
  dropShadow->blurY = 6.0f;
  dropShadow->color = {0.0f, 0.0f, 0.0f, 0.6f};

  auto* innerShadow = doc->makeNode<pagx::InnerShadowFilter>();
  innerShadow->offsetX = 2.0f;
  innerShadow->offsetY = 2.0f;
  innerShadow->blurX = 4.0f;
  innerShadow->blurY = 4.0f;
  innerShadow->color = {1.0f, 1.0f, 1.0f, 0.3f};

  layer->filters.push_back(dropShadow);
  layer->filters.push_back(innerShadow);
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "both_shadows"));
}

PAGX_TEST(PAGXPPTTest, PathWithStrokePadding) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(200, 100);
  pathData->lineTo(200, 200);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 10.0f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_stroke_padding"));
}

PAGX_TEST(PAGXPPTTest, RotatedLayer) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Rotate(45.0f);

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 200};
  rect->size = {100, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.8f, 0.4f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "rotated_layer"));
}

PAGX_TEST(PAGXPPTTest, TextWithRotation) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Rotate(30.0f);

  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Rotated Text";
  text->position = {200, 200};
  text->fontSize = 24.0f;
  text->fontFamily = "Arial";

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "text_rotated", options));
}

PAGX_TEST(PAGXPPTTest, ConvertTextToPathDefault) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Path Text";
  text->position = {100, 150};
  text->fontSize = 24.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "text_to_path_default"));
}

PAGX_TEST(PAGXPPTTest, TextWithGlyphRuns) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;
  auto* glyph = doc->makeNode<pagx::Glyph>();
  glyph->advance = 500;
  glyph->path = doc->makeNode<pagx::PathData>();
  glyph->path->moveTo(100, 0);
  glyph->path->lineTo(400, 0);
  glyph->path->lineTo(400, 800);
  glyph->path->lineTo(100, 800);
  glyph->path->close();
  font->glyphs.push_back(glyph);

  auto* text = doc->makeNode<pagx::Text>();
  text->text = "A";
  text->position = {100, 200};
  text->fontSize = 48.0f;

  auto* run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 48.0f;
  run->glyphs = {1};
  text->glyphRuns.push_back(run);

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "text_glyph_runs", options));
}

PAGX_TEST(PAGXPPTTest, MultipleElementsInLayer) {
  auto doc = pagx::PAGXDocument::Make(500, 400);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {100, 80};

  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {350, 100};
  ellipse->size = {120, 80};

  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(200, 300);
  pathData->lineTo(300, 200);
  pathData->lineTo(400, 300);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.5f, 0.8f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(ellipse);
  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "multiple_elements"));
}

PAGX_TEST(PAGXPPTTest, InvalidFilePath) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  EXPECT_FALSE(pagx::PPTExporter::ToFile(*doc, "/nonexistent/dir/file.pptx"));
}

PAGX_TEST(PAGXPPTTest, LargeCanvas) {
  auto doc = pagx::PAGXDocument::Make(3840, 2160);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {1920, 1080};
  rect->size = {500, 300};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.1f, 0.1f, 0.1f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "large_canvas"));
}

PAGX_TEST(PAGXPPTTest, ComplexSceneAllShapes) {
  auto doc = pagx::PAGXDocument::Make(800, 600);

  auto* layer = doc->makeNode<pagx::Layer>();

  auto* group1 = doc->makeNode<pagx::Group>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};
  rect->roundness = 10.0f;
  auto* fillRect = doc->makeNode<pagx::Fill>();
  auto* solidRect = doc->makeNode<pagx::SolidColor>();
  solidRect->color = {0.9f, 0.3f, 0.1f, 1.0f};
  fillRect->color = solidRect;
  auto* strokeRect = doc->makeNode<pagx::Stroke>();
  strokeRect->width = 2.0f;
  auto* strokeColor = doc->makeNode<pagx::SolidColor>();
  strokeColor->color = {0.0f, 0.0f, 0.0f, 1.0f};
  strokeRect->color = strokeColor;
  group1->elements.push_back(rect);
  group1->elements.push_back(fillRect);
  group1->elements.push_back(strokeRect);

  auto* group2 = doc->makeNode<pagx::Group>();
  group2->position = {400, 0};
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {120, 120};
  auto* gradFill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  auto* gs1 = doc->makeNode<pagx::ColorStop>();
  gs1->offset = 0.0f;
  gs1->color = {1.0f, 1.0f, 0.0f, 1.0f};
  auto* gs2 = doc->makeNode<pagx::ColorStop>();
  gs2->offset = 1.0f;
  gs2->color = {1.0f, 0.0f, 0.0f, 1.0f};
  grad->colorStops.push_back(gs1);
  grad->colorStops.push_back(gs2);
  gradFill->color = grad;
  group2->elements.push_back(ellipse);
  group2->elements.push_back(gradFill);

  auto* path = doc->makeNode<pagx::Path>();
  auto* pd = doc->makeNode<pagx::PathData>();
  pd->moveTo(100, 400);
  pd->cubicTo(200, 300, 500, 300, 700, 400);
  pd->lineTo(700, 550);
  pd->lineTo(100, 550);
  pd->close();
  path->data = pd;
  auto* fillPath = doc->makeNode<pagx::Fill>();
  auto* lgFill = doc->makeNode<pagx::LinearGradient>();
  lgFill->startPoint = {0, 0};
  lgFill->endPoint = {0, 1};
  auto* lgs1 = doc->makeNode<pagx::ColorStop>();
  lgs1->offset = 0.0f;
  lgs1->color = {0.3f, 0.7f, 1.0f, 1.0f};
  auto* lgs2 = doc->makeNode<pagx::ColorStop>();
  lgs2->offset = 1.0f;
  lgs2->color = {0.0f, 0.2f, 0.5f, 1.0f};
  lgFill->colorStops.push_back(lgs1);
  lgFill->colorStops.push_back(lgs2);
  fillPath->color = lgFill;

  auto* shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->offsetX = 3.0f;
  shadow->offsetY = 3.0f;
  shadow->blurX = 5.0f;
  shadow->blurY = 5.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.4f};
  layer->filters.push_back(shadow);

  layer->contents.push_back(group1);
  layer->contents.push_back(group2);
  layer->contents.push_back(path);
  layer->contents.push_back(fillPath);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "complex_scene"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetSysDash) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_sysdot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetDot) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {4.0f, 12.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_dot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetDash) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {12.0f, 12.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_preset_dash"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetLgDash) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {24.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_lg_dash"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetLgDashDot) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {24.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_lg_dash_dot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetDashDot) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {12.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_preset_dashDot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetLgDashDotDot) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {24.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_lg_dashDotDot"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetSysDashDotDot) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  stroke->dashes = {12.0f, 4.0f, 4.0f, 4.0f, 4.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_sys_dashDotDot"));
}

PAGX_TEST(PAGXPPTTest, EllipseWithStrokeAndFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {200, 140};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* fillSolid = doc->makeNode<pagx::SolidColor>();
  fillSolid->color = {0.9f, 0.8f, 0.1f, 1.0f};
  fill->color = fillSolid;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 3.0f;
  auto* strokeSolid = doc->makeNode<pagx::SolidColor>();
  strokeSolid->color = {0.1f, 0.1f, 0.1f, 1.0f};
  stroke->color = strokeSolid;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "ellipse_fill_and_stroke"));
}

PAGX_TEST(PAGXPPTTest, PathOnlyLines) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 250);
  pathData->lineTo(50, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_only_lines"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddSingleContour) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(100, 100);
  pathData->lineTo(300, 100);
  pathData->lineTo(300, 300);
  pathData->lineTo(100, 300);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.6f, 0.3f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_single"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddThreeContours) {
  auto doc = pagx::PAGXDocument::Make(500, 500);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(10, 10);
  pathData->lineTo(490, 10);
  pathData->lineTo(490, 490);
  pathData->lineTo(10, 490);
  pathData->close();
  pathData->moveTo(80, 80);
  pathData->lineTo(420, 80);
  pathData->lineTo(420, 420);
  pathData->lineTo(80, 420);
  pathData->close();
  pathData->moveTo(160, 160);
  pathData->lineTo(340, 160);
  pathData->lineTo(340, 340);
  pathData->lineTo(160, 340);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.8f, 0.2f, 0.2f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_three"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddWithCurves) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 200);
  pathData->cubicTo(50, 50, 350, 50, 350, 200);
  pathData->cubicTo(350, 350, 50, 350, 50, 200);
  pathData->close();
  pathData->moveTo(150, 200);
  pathData->cubicTo(150, 130, 250, 130, 250, 200);
  pathData->cubicTo(250, 270, 150, 270, 150, 200);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.1f, 0.5f, 0.9f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_curves"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddWithQuads) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 200);
  pathData->quadTo(200, 20, 350, 200);
  pathData->quadTo(200, 380, 50, 200);
  pathData->close();
  pathData->moveTo(130, 200);
  pathData->quadTo(200, 100, 270, 200);
  pathData->quadTo(200, 300, 130, 200);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.4f, 0.8f, 0.2f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_quads"));
}

PAGX_TEST(PAGXPPTTest, MaskBakeDefault) {
  auto doc = pagx::PAGXDocument::Make(400, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {200, 150};
  maskRect->size = {150, 100};
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  auto* contentLayer = doc->makeNode<pagx::Layer>();
  auto* contentEllipse = doc->makeNode<pagx::Ellipse>();
  contentEllipse->position = {200, 150};
  contentEllipse->size = {250, 200};
  auto* contentFill = doc->makeNode<pagx::Fill>();
  auto* contentSolid = doc->makeNode<pagx::SolidColor>();
  contentSolid->color = {0.0f, 0.5f, 1.0f, 1.0f};
  contentFill->color = contentSolid;
  contentLayer->contents.push_back(contentEllipse);
  contentLayer->contents.push_back(contentFill);
  contentLayer->mask = maskLayer;

  doc->layers.push_back(contentLayer);

  ASSERT_TRUE(ExportAndVerify(*doc, "mask_bake_default"));
}

PAGX_TEST(PAGXPPTTest, MaskNoBake) {
  auto doc = pagx::PAGXDocument::Make(400, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {200, 150};
  maskRect->size = {150, 100};
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  auto* contentLayer = doc->makeNode<pagx::Layer>();
  auto* contentEllipse = doc->makeNode<pagx::Ellipse>();
  contentEllipse->position = {200, 150};
  contentEllipse->size = {250, 200};
  auto* contentFill = doc->makeNode<pagx::Fill>();
  auto* contentSolid = doc->makeNode<pagx::SolidColor>();
  contentSolid->color = {0.0f, 0.5f, 1.0f, 1.0f};
  contentFill->color = contentSolid;
  contentLayer->contents.push_back(contentEllipse);
  contentLayer->contents.push_back(contentFill);
  contentLayer->mask = maskLayer;

  doc->layers.push_back(contentLayer);

  pagx::PPTExportOptions options;
  options.bakeMask = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "mask_no_bake", options));
}

PAGX_TEST(PAGXPPTTest, MaskNoBakeProducesSmaller) {
  auto doc = pagx::PAGXDocument::Make(400, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {200, 150};
  maskRect->size = {150, 100};
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  auto* contentLayer = doc->makeNode<pagx::Layer>();
  auto* contentEllipse = doc->makeNode<pagx::Ellipse>();
  contentEllipse->position = {200, 150};
  contentEllipse->size = {250, 200};
  auto* contentFill = doc->makeNode<pagx::Fill>();
  auto* contentSolid = doc->makeNode<pagx::SolidColor>();
  contentSolid->color = {0.0f, 0.5f, 1.0f, 1.0f};
  contentFill->color = contentSolid;
  contentLayer->contents.push_back(contentEllipse);
  contentLayer->contents.push_back(contentFill);
  contentLayer->mask = maskLayer;

  doc->layers.push_back(contentLayer);

  auto outDir = PPTOutDir();
  auto bakedPath = outDir + "/mask_size_baked.pptx";
  auto vectorPath = outDir + "/mask_size_vector.pptx";

  pagx::PPTExportOptions bakedOpts;
  bakedOpts.bakeMask = true;
  ASSERT_TRUE(pagx::PPTExporter::ToFile(*doc, bakedPath, bakedOpts));

  pagx::PPTExportOptions vectorOpts;
  vectorOpts.bakeMask = false;
  ASSERT_TRUE(pagx::PPTExporter::ToFile(*doc, vectorPath, vectorOpts));

  auto bakedSize = std::filesystem::file_size(bakedPath);
  auto vectorSize = std::filesystem::file_size(vectorPath);
  EXPECT_GT(bakedSize, vectorSize);
}

PAGX_TEST(PAGXPPTTest, MaskNoBakeWithChildLayers) {
  auto doc = pagx::PAGXDocument::Make(400, 400);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskPath = doc->makeNode<pagx::Path>();
  auto* maskPathData = doc->makeNode<pagx::PathData>();
  maskPathData->moveTo(100, 100);
  maskPathData->lineTo(300, 100);
  maskPathData->lineTo(300, 300);
  maskPathData->lineTo(100, 300);
  maskPathData->close();
  maskPath->data = maskPathData;
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskPath);
  maskLayer->contents.push_back(maskFill);

  auto* parentLayer = doc->makeNode<pagx::Layer>();
  auto* parentRect = doc->makeNode<pagx::Rectangle>();
  parentRect->position = {200, 200};
  parentRect->size = {300, 300};
  auto* parentFill = doc->makeNode<pagx::Fill>();
  auto* parentSolid = doc->makeNode<pagx::SolidColor>();
  parentSolid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  parentFill->color = parentSolid;
  parentLayer->contents.push_back(parentRect);
  parentLayer->contents.push_back(parentFill);

  auto* childLayer = doc->makeNode<pagx::Layer>();
  childLayer->x = 50;
  childLayer->y = 50;
  auto* childEllipse = doc->makeNode<pagx::Ellipse>();
  childEllipse->position = {100, 100};
  childEllipse->size = {80, 80};
  auto* childFill = doc->makeNode<pagx::Fill>();
  auto* childSolid = doc->makeNode<pagx::SolidColor>();
  childSolid->color = {0.0f, 0.0f, 1.0f, 1.0f};
  childFill->color = childSolid;
  childLayer->contents.push_back(childEllipse);
  childLayer->contents.push_back(childFill);

  parentLayer->children.push_back(childLayer);
  parentLayer->mask = maskLayer;

  doc->layers.push_back(parentLayer);

  pagx::PPTExportOptions options;
  options.bakeMask = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "mask_no_bake_children", options));
}

PAGX_TEST(PAGXPPTTest, MaskNoBakeWithTransformAndAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 400);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {200, 200};
  maskRect->size = {200, 200};
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskRect);
  maskLayer->contents.push_back(maskFill);

  auto* contentLayer = doc->makeNode<pagx::Layer>();
  contentLayer->x = 20;
  contentLayer->y = 20;
  contentLayer->alpha = 0.8f;
  contentLayer->matrix = pagx::Matrix::Rotate(15.0f);

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 200};
  rect->size = {300, 300};
  rect->roundness = 15.0f;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {1, 1};
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;
  contentLayer->contents.push_back(rect);
  contentLayer->contents.push_back(fill);
  contentLayer->mask = maskLayer;

  doc->layers.push_back(contentLayer);

  pagx::PPTExportOptions options;
  options.bakeMask = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "mask_no_bake_transform_alpha", options));
}

PAGX_TEST(PAGXPPTTest, StrokeDashPresetSysDash2) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  // dr = 12/4 = 3.0 (> 1.5 and <= 4.5), sr = 4/4 = 1.0 (<= 2.0) → sysDash
  stroke->dashes = {12.0f, 4.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_preset_sysDash2"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashEmptyDashes) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4.0f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "dash_empty"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithTextBoxZeroHeight) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Zero Height Box";
  text->position = {50, 100};
  text->fontSize = 24.0f;

  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(textBox);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_textbox_zero_height", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextNoLetterSpacing) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "No Spacing";
  text->position = {50, 150};
  text->fontSize = 24.0f;
  text->fontFamily = "Arial";
  text->letterSpacing = 0.0f;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.2f, 0.2f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_no_spacing", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithFillAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Alpha Text";
  text->position = {50, 150};
  text->fontSize = 28.0f;
  text->fontFamily = "Arial";

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->alpha = 0.6f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 1.0f, 0.5f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_fill_alpha", options));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddOpenInnerContour) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Outer square
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  // Inner open contour (not closed) - should be kept as-is
  pathData->moveTo(150, 150);
  pathData->lineTo(250, 150);
  pathData->lineTo(250, 250);
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.6f, 0.2f, 0.4f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_open_inner"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddOppositeWinding) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Outer square (CW)
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  // Inner square (CCW) - opposite winding, should not be reversed
  pathData->moveTo(150, 150);
  pathData->lineTo(150, 250);
  pathData->lineTo(250, 250);
  pathData->lineTo(250, 150);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.7f, 0.3f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_opposite_winding"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithShadow) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Shadow Text";
  text->position = {50, 150};
  text->fontSize = 30.0f;
  text->fontFamily = "Arial";

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  auto* shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->offsetX = 3.0f;
  shadow->offsetY = 3.0f;
  shadow->blurX = 5.0f;
  shadow->blurY = 5.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->filters.push_back(shadow);

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "native_text_shadow", options));
}

PAGX_TEST(PAGXPPTTest, PathZeroBoundsSkipped) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Single point → zero width and height
  pathData->moveTo(100, 100);
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_zero_bounds"));
}

PAGX_TEST(PAGXPPTTest, BlurFilterIgnored) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.9f, 1.0f};
  fill->color = solid;

  auto* blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 5.0f;
  blur->blurY = 5.0f;
  layer->filters.push_back(blur);

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "blur_filter_ignored"));
}

static pagx::Image* MakeTestPNGImage(pagx::PAGXDocument* doc) {
  // Minimal valid 2x2 RGBA PNG (8-bit, non-interlaced)
  static const uint8_t kMinimalPNG[] = {
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
  image->data = pagx::Data::MakeWithCopy(kMinimalPNG, sizeof(kMinimalPNG));
  return image;
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_Stretch) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_stretch"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_TileRepeat) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
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

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_tile_repeat"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_TileMirror) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
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

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_tile_mirror"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_MirrorXOnly) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
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

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_mirror_x"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_MirrorYOnly) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Mirror;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_mirror_y"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_NullImage) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* pattern = doc->makeNode<pagx::ImagePattern>();

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_null_image"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_WithTransform) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {100.0f, 0.0f, 0.0f, 75.0f, 100.0f, 75.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_transform"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_WithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {100.0f, 0.0f, 0.0f, 75.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->alpha = 0.5f;
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_alpha"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternAsSmallPicture) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  // Scale image to be smaller than the shape (1px wide, 1px tall at position 150,100)
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 150.0f, 100.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_small_picture"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternOnEllipse) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {100.0f, 0.0f, 0.0f, 75.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_ellipse"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternOnPath) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(100, 50);
  pathData->lineTo(300, 50);
  pathData->lineTo(300, 250);
  pathData->lineTo(100, 250);
  pathData->close();
  path->data = pathData;

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {100.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_path"));
}

PAGX_TEST(PAGXPPTTest, MaskNoBakeContourType) {
  auto doc = pagx::PAGXDocument::Make(400, 300);

  auto* maskLayer = doc->makeNode<pagx::Layer>();
  auto* maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {200, 150};
  maskEllipse->size = {180, 120};
  auto* maskFill = doc->makeNode<pagx::Fill>();
  auto* maskSolid = doc->makeNode<pagx::SolidColor>();
  maskSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskSolid;
  maskLayer->contents.push_back(maskEllipse);
  maskLayer->contents.push_back(maskFill);

  auto* contentLayer = doc->makeNode<pagx::Layer>();
  contentLayer->maskType = pagx::MaskType::Contour;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {300, 200};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.8f, 0.4f, 1.0f};
  fill->color = solid;
  contentLayer->contents.push_back(rect);
  contentLayer->contents.push_back(fill);
  contentLayer->mask = maskLayer;

  doc->layers.push_back(contentLayer);

  pagx::PPTExportOptions options;
  options.bakeMask = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "mask_no_bake_contour", options));
}

PAGX_TEST(PAGXPPTTest, NativeTextTextAnchorCenter) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Centered";
  text->fontFamily = "Arial";
  text->fontSize = 24;
  text->position = {200, 150};
  text->textAnchor = pagx::TextAnchor::Center;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_anchor_center"));
}

PAGX_TEST(PAGXPPTTest, NativeTextTextAnchorEnd) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Right";
  text->fontFamily = "Arial";
  text->fontSize = 24;
  text->position = {200, 150};
  text->textAnchor = pagx::TextAnchor::End;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_anchor_end"));
}

PAGX_TEST(PAGXPPTTest, NativeTextTextBoxCenterAlign) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "TextBox Center";
  text->fontFamily = "Arial";
  text->fontSize = 20;
  text->position = {100, 100};
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 80};
  textBox->width = 300;
  textBox->height = 60;
  textBox->textAlign = pagx::TextAlign::Center;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(text);
  layer->contents.push_back(textBox);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_textbox_center"));
}

PAGX_TEST(PAGXPPTTest, NativeTextTextBoxEndAlign) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "TextBox End";
  text->fontFamily = "Arial";
  text->fontSize = 20;
  text->position = {100, 100};
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 80};
  textBox->width = 300;
  textBox->height = 60;
  textBox->textAlign = pagx::TextAlign::End;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(text);
  layer->contents.push_back(textBox);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_textbox_end"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithInnerShadow) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Inner Shadow Text";
  text->fontFamily = "Arial";
  text->fontSize = 28;
  text->position = {200, 150};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.1f, 0.1f, 0.1f, 1.0f};
  fill->color = solid;
  auto* innerShadow = doc->makeNode<pagx::InnerShadowFilter>();
  innerShadow->blurX = 3.0f;
  innerShadow->blurY = 3.0f;
  innerShadow->offsetX = 1.0f;
  innerShadow->offsetY = 1.0f;
  innerShadow->color = {0.0f, 0.0f, 0.0f, 0.6f};
  layer->filters.push_back(innerShadow);
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_inner_shadow"));
}

PAGX_TEST(PAGXPPTTest, StrokeWithGradient) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 6.0f;
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  grad->startPoint = {0.0f, 0.0f};
  grad->endPoint = {1.0f, 0.0f};
  stroke->color = grad;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_gradient"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternOnStroke) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;
  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 10.0f;
  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
  stroke->color = pattern;
  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_stroke"));
}

PAGX_TEST(PAGXPPTTest, ImageWithJPEGData) {
  // Minimal but fully decodable 1x1 grayscale baseline JPEG with a JFIF APP0 header
  // at 96 DPI, so the hasJPEG() / JPEG DPI probe paths are exercised and the PPT
  // bake path can also decode the pixels without hitting libjpeg error_exit.
  static const uint8_t kMinimalJPEG[] = {
      0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00,
      0x60, 0x00, 0x60, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF,
      0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00,
      0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05,
      0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
      0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
      0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A,
      0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37,
      0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
      0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
      0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93,
      0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9,
      0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
      0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
      0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
      0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x00, 0xAF,
      0xFF, 0xD9,
  };

  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(kMinimalJPEG, sizeof(kMinimalJPEG));
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {100.0f, 0.0f, 0.0f, 75.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_jpeg"));
}

PAGX_TEST(PAGXPPTTest, GroupWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  group->alpha = 0.5f;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  group->elements.push_back(rect);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "group_alpha"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithLetterSpacing) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Spaced";
  text->fontFamily = "Arial";
  text->fontSize = 24;
  text->position = {200, 150};
  text->letterSpacing = 5.0f;
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_letter_spacing"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_ImageNoData) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = doc->makeNode<pagx::Image>();
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_no_data"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternFill_TileWithDPI) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  // Use a PNG with pHYs chunk encoding 144 DPI (5669 pixels/meter)
  static const uint8_t kPNG144DPI[] = {
      0x89,
      0x50,
      0x4E,
      0x47,
      0x0D,
      0x0A,
      0x1A,
      0x0A,  // PNG signature
      // IHDR: 4x4 RGB
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
      0x04,
      0x00,
      0x00,
      0x00,
      0x04,
      0x08,
      0x02,
      0x00,
      0x00,
      0x00,
      0x26,
      0x93,
      0x09,
      0x29,
      // pHYs: 5669 pixels/meter (~144 DPI), unit = 1
      0x00,
      0x00,
      0x00,
      0x09,
      0x70,
      0x48,
      0x59,
      0x73,
      0x00,
      0x00,
      0x16,
      0x25,
      0x00,
      0x00,
      0x16,
      0x25,
      0x01,
      0x49,
      0x52,
      0x24,
      0xF0,
      // IDAT
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
  image->data = pagx::Data::MakeWithCopy(kPNG144DPI, sizeof(kPNG144DPI));
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = {0.5f, 0.0f, 0.0f, 0.5f, 10.0f, 20.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_tile_dpi"));
}

PAGX_TEST(PAGXPPTTest, RectangleWithImagePatternAndStroke) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 150.0f, 100.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 3.0f;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "rect_imagepattern_stroke"));
}

PAGX_TEST(PAGXPPTTest, EllipseWithImagePatternAndShadow) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {200, 150};
  ellipse->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 150.0f, 100.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  auto* shadow = doc->makeNode<pagx::DropShadowFilter>();
  shadow->blurX = 4.0f;
  shadow->blurY = 4.0f;
  shadow->offsetX = 3.0f;
  shadow->offsetY = 3.0f;
  shadow->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->filters.push_back(shadow);

  layer->contents.push_back(ellipse);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "ellipse_imagepattern_shadow"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithFillNoSolidColor) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Gradient Fill Text";
  text->fontFamily = "Arial";
  text->fontSize = 24;
  text->position = {200, 150};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* grad = doc->makeNode<pagx::LinearGradient>();
  auto* stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 0.0f;
  stop1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto* stop2 = doc->makeNode<pagx::ColorStop>();
  stop2->offset = 1.0f;
  stop2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(stop1);
  grad->colorStops.push_back(stop2);
  fill->color = grad;
  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_gradient_fill"));
}

PAGX_TEST(PAGXPPTTest, LayerWithScaleMatrix) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->matrix = {2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 75};
  rect->size = {100, 75};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 1.0f, 0.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_scale"));
}

PAGX_TEST(PAGXPPTTest, LayerWithRotationMatrix) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  float rad = 0.7853981f;  // 45 degrees
  layer->matrix = {std::cos(rad), std::sin(rad), -std::sin(rad), std::cos(rad), 100.0f, 75.0f};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 75};
  rect->size = {100, 75};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 1.0f, 1.0f};
  fill->color = solid;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_rotation"));
}

PAGX_TEST(PAGXPPTTest, StrokeDashWithZeroWidth) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 100);
  pathData->lineTo(350, 100);
  path->data = pathData;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 0.0f;
  stroke->dashes = {10.0f, 5.0f};
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "stroke_dash_zero_width"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternSmallPictureRotated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  float rad = 0.5236f;  // 30 degrees
  layer->matrix = {std::cos(rad), std::sin(rad), -std::sin(rad), std::cos(rad), 0.0f, 0.0f};

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 150.0f, 100.0f};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_small_rotated"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddQuadSameWinding) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Outer square (CW in screen coords, positive signed area)
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  // Inner contour using quadTo with SAME winding direction as outer
  pathData->moveTo(150, 150);
  pathData->quadTo(200, 100, 250, 150);
  pathData->lineTo(250, 250);
  pathData->lineTo(150, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.3f, 0.7f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  ASSERT_TRUE(ExportAndVerify(*doc, "path_even_odd_quad_same_winding"));
}

PAGX_TEST(PAGXPPTTest, TextToPathEmptyGlyphRuns) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();

  auto* font = doc->makeNode<pagx::Font>();
  font->unitsPerEm = 1000;

  auto* text = doc->makeNode<pagx::Text>();
  text->text = "A";
  text->position = {100, 200};
  text->fontSize = 48.0f;

  auto* run = doc->makeNode<pagx::GlyphRun>();
  run->font = font;
  run->fontSize = 48.0f;
  run->glyphs = {0};
  text->glyphRuns.push_back(run);

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.convertTextToPath = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "text_to_path_empty_glyphs", options));
}

//==============================================================================
// Coverage boost: PPTModifierResolver paths (Polystar / Repeater / TrimPath /
// RoundCorner / MergePath) plus PPTFeatureProbe escalation paths.
//==============================================================================

static pagx::Fill* MakeSolidFill(pagx::PAGXDocument* doc, pagx::Color color) {
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  return fill;
}

PAGX_TEST(PAGXPPTTest, PolystarStarShape) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {200, 150};
  star->pointCount = 5;
  star->outerRadius = 80;
  star->innerRadius = 40;
  star->rotation = 15;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFill(doc.get(), {1.0f, 0.8f, 0.0f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_star"));
}

PAGX_TEST(PAGXPPTTest, PolystarPolygonShape) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {200, 150};
  poly->pointCount = 6;
  poly->outerRadius = 80;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.6f, 0.9f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_polygon"));
}

PAGX_TEST(PAGXPPTTest, PolystarStarWithRoundness) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {200, 150};
  star->pointCount = 6;
  star->outerRadius = 90;
  star->innerRadius = 45;
  star->outerRoundness = 0.6f;
  star->innerRoundness = 0.3f;
  star->reversed = true;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFill(doc.get(), {1.0f, 0.2f, 0.5f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_star_round"));
}

PAGX_TEST(PAGXPPTTest, PolystarPolygonWithRoundness) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* poly = doc->makeNode<pagx::Polystar>();
  poly->type = pagx::PolystarType::Polygon;
  poly->position = {200, 150};
  poly->pointCount = 5;
  poly->outerRadius = 80;
  poly->outerRoundness = 0.5f;
  poly->reversed = true;
  layer->contents.push_back(poly);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.2f, 0.8f, 0.2f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_polygon_round"));
}

PAGX_TEST(PAGXPPTTest, PolystarFractionalPoints) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->position = {200, 150};
  star->pointCount = 5.5f;
  star->outerRadius = 80;
  star->innerRadius = 40;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.9f, 0.4f, 0.1f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_fractional"));
}

PAGX_TEST(PAGXPPTTest, PolystarZeroPoints) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* star = doc->makeNode<pagx::Polystar>();
  star->type = pagx::PolystarType::Star;
  star->pointCount = 0;
  layer->contents.push_back(star);
  layer->contents.push_back(MakeSolidFill(doc.get(), {1.0f, 1.0f, 1.0f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "polystar_zero_points"));
}

PAGX_TEST(PAGXPPTTest, TrimPathSeparate) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 120};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.1f;
  trim->end = 0.7f;
  trim->offset = 45;
  trim->type = pagx::TrimType::Separate;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "trim_path_separate"));
}

PAGX_TEST(PAGXPPTTest, TrimPathContinuous) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {120, 150};
  rect->size = {80, 80};

  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {280, 150};
  ellipse->size = {80, 80};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  trim->type = pagx::TrimType::Continuous;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 4;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(ellipse);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "trim_path_continuous"));
}

PAGX_TEST(PAGXPPTTest, TrimPathWithoutShape) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.5f;
  layer->contents.push_back(trim);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "trim_path_no_shape"));
}

PAGX_TEST(PAGXPPTTest, RoundCornerModifier) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 250);
  pathData->lineTo(50, 250);
  pathData->close();
  path->data = pathData;

  auto* rc = doc->makeNode<pagx::RoundCorner>();
  rc->radius = 30;

  layer->contents.push_back(path);
  layer->contents.push_back(rc);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.5f, 0.5f, 0.9f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "round_corner_mod"));
}

PAGX_TEST(PAGXPPTTest, RoundCornerZeroRadius) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 120};

  auto* rc = doc->makeNode<pagx::RoundCorner>();
  rc->radius = 0;

  layer->contents.push_back(rect);
  layer->contents.push_back(rc);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.6f, 0.3f, 0.7f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "round_corner_zero"));
}

PAGX_TEST(PAGXPPTTest, MergePathUnion) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {160, 150};
  r1->size = {120, 120};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {240, 150};
  r2->size = {120, 120};

  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Union;

  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.2f, 0.7f, 0.3f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_union"));
}

PAGX_TEST(PAGXPPTTest, MergePathDifference) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {200, 150};
  r1->size = {200, 200};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {200, 150};
  r2->size = {100, 100};

  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Difference;

  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.8f, 0.3f, 0.3f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_difference"));
}

PAGX_TEST(PAGXPPTTest, MergePathIntersect) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {170, 150};
  r1->size = {160, 160};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {230, 150};
  r2->size = {160, 160};

  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Intersect;

  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.4f, 0.4f, 0.9f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_intersect"));
}

PAGX_TEST(PAGXPPTTest, MergePathXor) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* r1 = doc->makeNode<pagx::Rectangle>();
  r1->position = {170, 150};
  r1->size = {140, 140};
  auto* r2 = doc->makeNode<pagx::Rectangle>();
  r2->position = {230, 150};
  r2->size = {140, 140};

  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Xor;

  layer->contents.push_back(r1);
  layer->contents.push_back(r2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.9f, 0.7f, 0.1f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_xor"));
}

PAGX_TEST(PAGXPPTTest, MergePathAppend) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* e1 = doc->makeNode<pagx::Ellipse>();
  e1->position = {150, 150};
  e1->size = {100, 100};
  auto* e2 = doc->makeNode<pagx::Ellipse>();
  e2->position = {270, 150};
  e2->size = {100, 100};

  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Append;

  layer->contents.push_back(e1);
  layer->contents.push_back(e2);
  layer->contents.push_back(mp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.1f, 0.5f, 0.5f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_append"));
}

PAGX_TEST(PAGXPPTTest, MergePathNoShape) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* mp = doc->makeNode<pagx::MergePath>();
  mp->mode = pagx::MergePathMode::Union;
  layer->contents.push_back(mp);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "merge_no_shape"));
}

PAGX_TEST(PAGXPPTTest, RepeaterBasic) {
  auto doc = pagx::PAGXDocument::Make(500, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {60, 150};
  rect->size = {40, 40};

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.7f, 0.5f, 1.0f}));

  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 4;
  rep->position = {80, 0};
  rep->rotation = 10;
  rep->scale = {0.9f, 0.9f};
  rep->order = pagx::RepeaterOrder::BelowOriginal;
  rep->startAlpha = 1.0f;
  rep->endAlpha = 0.3f;
  layer->contents.push_back(rep);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_basic"));
}

PAGX_TEST(PAGXPPTTest, RepeaterAboveOrder) {
  auto doc = pagx::PAGXDocument::Make(500, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {60, 150};
  ellipse->size = {50, 50};

  layer->contents.push_back(ellipse);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.8f, 0.3f, 0.3f, 1.0f}));

  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  rep->position = {100, 30};
  rep->order = pagx::RepeaterOrder::AboveOriginal;
  layer->contents.push_back(rep);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_above"));
}

PAGX_TEST(PAGXPPTTest, RepeaterFractionalCopies) {
  auto doc = pagx::PAGXDocument::Make(500, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {60, 150};
  rect->size = {40, 40};

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.2f, 0.5f, 0.8f, 1.0f}));

  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3.5f;
  rep->offset = 0.2f;
  rep->position = {60, 0};
  rep->anchor = {20, 20};
  rep->startAlpha = 1.0f;
  rep->endAlpha = 0.5f;
  layer->contents.push_back(rep);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_fractional"));
}

PAGX_TEST(PAGXPPTTest, RepeaterZeroCopies) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 150};
  rect->size = {60, 60};

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.5f, 0.5f, 0.5f, 1.0f}));

  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 0;
  layer->contents.push_back(rep);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_zero"));
}

PAGX_TEST(PAGXPPTTest, RepeaterNegativeCopies) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 150};
  rect->size = {60, 60};

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.5f, 0.5f, 0.5f, 1.0f}));

  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = -2;
  layer->contents.push_back(rep);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_negative"));
}

PAGX_TEST(PAGXPPTTest, RepeaterNoPrecedingElements) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rep = doc->makeNode<pagx::Repeater>();
  rep->copies = 3;
  rep->position = {50, 0};
  layer->contents.push_back(rep);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "repeater_empty"));
}

PAGX_TEST(PAGXPPTTest, ResolveModifiersDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 120};

  auto* trim = doc->makeNode<pagx::TrimPath>();
  trim->start = 0.0f;
  trim->end = 0.6f;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 3;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(trim);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.resolveModifiers = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "modifiers_disabled", options));
}

//==============================================================================
// Feature probe escalation tests: these layers contain features OOXML can't
// express natively, triggering rasterization (or the clamp path when disabled).
//==============================================================================

PAGX_TEST(PAGXPPTTest, ConicGradientFillEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 200};

  auto* grad = doc->makeNode<pagx::ConicGradient>();
  grad->center = {200, 150};
  grad->startAngle = 0;
  grad->endAngle = 360;
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.0f, 0.0f, 1.0f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "conic_gradient_raster"));
}

PAGX_TEST(PAGXPPTTest, DiamondGradientFillEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 200};

  auto* grad = doc->makeNode<pagx::DiamondGradient>();
  grad->center = {200, 150};
  grad->radius = 100;
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.8f, 0.0f, 1.0f};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.0f, 0.2f, 0.6f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "diamond_gradient_raster"));
}

PAGX_TEST(PAGXPPTTest, ColorMatrixFilterEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.5f, 0.5f, 0.5f, 1.0f}));

  auto* cm = doc->makeNode<pagx::ColorMatrixFilter>();
  cm->matrix = {0.3f, 0.6f, 0.1f, 0.0f, 0.0f, 0.3f, 0.6f, 0.1f, 0.0f, 0.0f,
                0.3f, 0.6f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
  layer->filters.push_back(cm);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "colormatrix_raster"));
}

PAGX_TEST(PAGXPPTTest, LayerNonNormalBlendModeEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* bgLayer = doc->makeNode<pagx::Layer>();
  auto* bgRect = doc->makeNode<pagx::Rectangle>();
  bgRect->position = {200, 150};
  bgRect->size = {300, 200};
  bgLayer->contents.push_back(bgRect);
  bgLayer->contents.push_back(MakeSolidFill(doc.get(), {0.2f, 0.8f, 0.3f, 1.0f}));
  doc->layers.push_back(bgLayer);

  auto* fgLayer = doc->makeNode<pagx::Layer>();
  fgLayer->blendMode = pagx::BlendMode::Overlay;
  auto* fgRect = doc->makeNode<pagx::Rectangle>();
  fgRect->position = {200, 150};
  fgRect->size = {200, 150};
  fgLayer->contents.push_back(fgRect);
  fgLayer->contents.push_back(MakeSolidFill(doc.get(), {0.9f, 0.1f, 0.5f, 1.0f}));
  doc->layers.push_back(fgLayer);

  pagx::PPTExportOptions options;
  options.rasterizeUnsupportedBlend = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_blend_overlay", options));
}

PAGX_TEST(PAGXPPTTest, LayerNonNormalBlendWithBackdrop) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* bg = doc->makeNode<pagx::Layer>();
  auto* bgRect = doc->makeNode<pagx::Rectangle>();
  bgRect->position = {200, 150};
  bgRect->size = {400, 300};
  bg->contents.push_back(bgRect);
  bg->contents.push_back(MakeSolidFill(doc.get(), {0.6f, 0.2f, 0.8f, 1.0f}));
  doc->layers.push_back(bg);

  auto* fg = doc->makeNode<pagx::Layer>();
  fg->blendMode = pagx::BlendMode::ColorDodge;
  auto* fgEllipse = doc->makeNode<pagx::Ellipse>();
  fgEllipse->position = {200, 150};
  fgEllipse->size = {200, 200};
  fg->contents.push_back(fgEllipse);
  fg->contents.push_back(MakeSolidFill(doc.get(), {1.0f, 0.8f, 0.1f, 1.0f}));
  doc->layers.push_back(fg);

  pagx::PPTExportOptions options;
  options.rasterizeUnsupportedBlend = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_blend_with_backdrop", options));
}

PAGX_TEST(PAGXPPTTest, FillBlendModeEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->blendMode = pagx::BlendMode::HardLight;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.9f, 0.3f, 0.1f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  pagx::PPTExportOptions options;
  options.rasterizeUnsupportedBlend = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "fill_blend_hardlight", options));
}

PAGX_TEST(PAGXPPTTest, RasterizeUnsupportedBlendDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->blendMode = pagx::BlendMode::Overlay;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.4f, 0.6f, 0.8f, 1.0f}));
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.rasterizeUnsupportedBlend = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "raster_blend_disabled", options));
}

PAGX_TEST(PAGXPPTTest, BlendFilterSupportedMode) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.5f, 0.7f, 1.0f}));

  auto* bf = doc->makeNode<pagx::BlendFilter>();
  bf->color = {1.0f, 0.5f, 0.0f, 0.6f};
  bf->blendMode = pagx::BlendMode::Multiply;
  layer->filters.push_back(bf);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "blend_filter_multiply"));
}

PAGX_TEST(PAGXPPTTest, BlendFilterUnsupportedMode) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.5f, 0.7f, 1.0f}));

  auto* bf = doc->makeNode<pagx::BlendFilter>();
  bf->color = {1.0f, 0.5f, 0.0f, 0.7f};
  bf->blendMode = pagx::BlendMode::ColorBurn;
  layer->filters.push_back(bf);

  doc->layers.push_back(layer);
  pagx::PPTExportOptions options;
  options.rasterizeUnsupportedBlend = true;
  ASSERT_TRUE(ExportAndVerify(*doc, "blend_filter_unsupported", options));
}

PAGX_TEST(PAGXPPTTest, WideGamutColorEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.2f, 0.3f, 1.0f};
  solid->color.colorSpace = pagx::ColorSpace::DisplayP3;
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "wide_gamut_raster"));
}

PAGX_TEST(PAGXPPTTest, WideGamutGradientEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {100, 100};
  grad->endPoint = {300, 200};
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.2f, 0.1f, 1.0f};
  s0->color.colorSpace = pagx::ColorSpace::DisplayP3;
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.1f, 0.4f, 0.9f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "wide_gamut_grad_raster"));
}

PAGX_TEST(PAGXPPTTest, WideGamutDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 0.2f, 0.3f, 1.0f};
  solid->color.colorSpace = pagx::ColorSpace::DisplayP3;
  fill->color = solid;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.rasterizeWideGamut = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "wide_gamut_disabled", options));
}

PAGX_TEST(PAGXPPTTest, GroupShearTransformEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* group = doc->makeNode<pagx::Group>();
  group->position = {200, 150};
  group->skew = 20;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {0, 0};
  rect->size = {160, 100};
  group->elements.push_back(rect);
  group->elements.push_back(MakeSolidFill(doc.get(), {0.5f, 0.3f, 0.7f, 1.0f}));

  layer->contents.push_back(group);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "group_skew_raster"));
}

PAGX_TEST(PAGXPPTTest, LayerMatrixShearEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  // Shear matrix: non-zero off-diagonal with different signs so a·c + b·d != 0.
  layer->matrix = {1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 150};
  rect->size = {150, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.1f, 0.5f, 0.9f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_shear_raster"));
}

PAGX_TEST(PAGXPPTTest, TextPathFeatureEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Curve";
  text->position = {50, 150};
  text->fontFamily = "Arial";
  text->fontSize = 24;

  auto* tp = doc->makeNode<pagx::TextPath>();
  auto* tpData = doc->makeNode<pagx::PathData>();
  tpData->moveTo(50, 150);
  tpData->cubicTo(150, 50, 250, 250, 350, 150);
  tp->path = tpData;

  layer->contents.push_back(text);
  layer->contents.push_back(tp);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.0f, 0.0f, 0.0f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textpath_raster"));
}

PAGX_TEST(PAGXPPTTest, TextModifierFeatureEscalated) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Anim";
  text->position = {100, 150};
  text->fontFamily = "Arial";
  text->fontSize = 32;

  auto* tm = doc->makeNode<pagx::TextModifier>();
  tm->rotation = 15;
  auto* sel = doc->makeNode<pagx::RangeSelector>();
  sel->start = 0;
  sel->end = 1;
  tm->selectors.push_back(sel);

  layer->contents.push_back(text);
  layer->contents.push_back(tm);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.0f, 0.0f, 0.0f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textmodifier_raster"));
}

//==============================================================================
// Layer style tests: DropShadowStyle / InnerShadowStyle / BackgroundBlurStyle
// exercise writeEffects's style-collection path, which is separate from the
// filter-based DropShadowFilter / InnerShadowFilter / BlurFilter.
//==============================================================================

PAGX_TEST(PAGXPPTTest, DropShadowStyleEmitted) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {160, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.9f, 0.2f, 0.4f, 1.0f}));

  auto* ds = doc->makeNode<pagx::DropShadowStyle>();
  ds->offsetX = 4;
  ds->offsetY = 5;
  ds->blurX = 3;
  ds->blurY = 3;
  ds->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->styles.push_back(ds);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "dropshadow_style"));
}

PAGX_TEST(PAGXPPTTest, InnerShadowStyleEmitted) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {160, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.8f, 0.6f, 0.2f, 1.0f}));

  auto* is = doc->makeNode<pagx::InnerShadowStyle>();
  is->offsetX = 3;
  is->offsetY = 3;
  is->blurX = 4;
  is->blurY = 4;
  is->color = {0.0f, 0.0f, 0.0f, 0.6f};
  layer->styles.push_back(is);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "innershadow_style"));
}

PAGX_TEST(PAGXPPTTest, BackgroundBlurStyleEmitted) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {160, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.5f, 0.7f, 0.5f, 0.7f}));

  auto* bb = doc->makeNode<pagx::BackgroundBlurStyle>();
  bb->blurX = 8;
  bb->blurY = 8;
  layer->styles.push_back(bb);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "bg_blur_style"));
}

PAGX_TEST(PAGXPPTTest, MultipleShadowStylesAndFilters) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {160, 120};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.7f, 0.9f, 1.0f}));

  auto* ds1 = doc->makeNode<pagx::DropShadowStyle>();
  ds1->offsetX = 5;
  ds1->offsetY = 5;
  ds1->blurX = 3;
  ds1->blurY = 3;
  ds1->color = {0.0f, 0.0f, 0.0f, 0.5f};
  layer->styles.push_back(ds1);

  auto* ds2 = doc->makeNode<pagx::DropShadowStyle>();
  ds2->offsetX = 10;
  ds2->offsetY = 10;
  ds2->blurX = 6;
  ds2->blurY = 6;
  ds2->color = {0.2f, 0.0f, 0.4f, 0.4f};
  layer->styles.push_back(ds2);

  auto* blur = doc->makeNode<pagx::BlurFilter>();
  blur->blurX = 2;
  blur->blurY = 2;
  layer->filters.push_back(blur);

  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "multi_shadow"));
}

//==============================================================================
// TextBox container (writeTextBoxGroup) — exercised by nesting Text inside a
// TextBox with explicit size so the textbox shape frame is emitted.
//==============================================================================

PAGX_TEST(PAGXPPTTest, TextBoxContainerSingleText) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;
  textBox->height = 200;
  textBox->textAlign = pagx::TextAlign::Center;
  textBox->lineHeight = 28;

  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Hello World";
  text->fontFamily = "Arial";
  text->fontSize = 24;

  auto* fill = MakeSolidFill(doc.get(), {0.0f, 0.0f, 0.0f, 1.0f});

  textBox->elements.push_back(text);
  textBox->elements.push_back(fill);

  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_container_single"));
}

PAGX_TEST(PAGXPPTTest, TextBoxContainerMultipleTexts) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;
  textBox->height = 200;
  textBox->textAlign = pagx::TextAlign::Justify;

  auto* t1 = doc->makeNode<pagx::Text>();
  t1->text = "Line one ";
  t1->fontFamily = "Arial";
  t1->fontSize = 20;

  auto* t2 = doc->makeNode<pagx::Text>();
  t2->text = "and line two";
  t2->fontFamily = "Arial";
  t2->fontSize = 20;

  textBox->elements.push_back(t1);
  textBox->elements.push_back(t2);
  textBox->elements.push_back(MakeSolidFill(doc.get(), {0.2f, 0.2f, 0.2f, 1.0f}));

  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_container_multi"));
}

PAGX_TEST(PAGXPPTTest, TextBoxContainerEmpty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 200;
  textBox->height = 100;
  // No Text children — CollectRichTextRuns returns empty, writeTextBoxGroup
  // should early-return without emitting a shape.
  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_container_empty"));
}

PAGX_TEST(PAGXPPTTest, TextBoxContainerWithNestedGroupFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 300;
  textBox->height = 200;

  auto* outerText = doc->makeNode<pagx::Text>();
  outerText->text = "Outer ";
  outerText->fontFamily = "Arial";
  outerText->fontSize = 22;

  auto* innerGroup = doc->makeNode<pagx::Group>();
  auto* innerText = doc->makeNode<pagx::Text>();
  innerText->text = "Inner";
  innerText->fontFamily = "Arial";
  innerText->fontSize = 22;
  innerGroup->elements.push_back(innerText);
  innerGroup->elements.push_back(MakeSolidFill(doc.get(), {1.0f, 0.0f, 0.0f, 1.0f}));

  textBox->elements.push_back(outerText);
  textBox->elements.push_back(innerGroup);
  textBox->elements.push_back(MakeSolidFill(doc.get(), {0.0f, 0.0f, 1.0f, 1.0f}));

  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_nested_group_fill"));
}

PAGX_TEST(PAGXPPTTest, TextBoxContainerVerticalWriting) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 50};
  textBox->width = 100;
  textBox->height = 300;
  textBox->writingMode = pagx::WritingMode::Vertical;

  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Vertical";
  text->fontFamily = "Arial";
  text->fontSize = 24;

  textBox->elements.push_back(text);
  textBox->elements.push_back(MakeSolidFill(doc.get(), {0.0f, 0.0f, 0.0f, 1.0f}));

  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_vertical"));
}

PAGX_TEST(PAGXPPTTest, TextBoxContainerAutoSize) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* textBox = doc->makeNode<pagx::TextBox>();
  textBox->position = {50, 100};
  // width/height unset (NaN default) so the writer falls back to the
  // character-count heuristic inside writeTextBoxGroup.
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Auto";
  text->fontFamily = "Arial";
  text->fontSize = 24;

  textBox->elements.push_back(text);
  textBox->elements.push_back(MakeSolidFill(doc.get(), {0.1f, 0.1f, 0.1f, 1.0f}));

  layer->contents.push_back(textBox);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "textbox_auto_size"));
}

//==============================================================================
// Native text feature coverage: multiline, run stroke, run gradient.
//==============================================================================

PAGX_TEST(PAGXPPTTest, NativeTextMultilineWrittenAsParagraphs) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "a\nb\n\nc";
  text->position = {100, 100};
  text->fontFamily = "Arial";
  text->fontSize = 20;

  layer->contents.push_back(text);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.0f, 0.0f, 0.0f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_multiline_blank"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithStrokeOnRun) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Outline";
  text->position = {100, 150};
  text->fontFamily = "Arial";
  text->fontSize = 36;

  auto* fill = doc->makeNode<pagx::Fill>();
  auto* fSolid = doc->makeNode<pagx::SolidColor>();
  fSolid->color = {1.0f, 1.0f, 1.0f, 1.0f};
  fill->color = fSolid;

  auto* stroke = doc->makeNode<pagx::Stroke>();
  stroke->width = 2;
  auto* sSolid = doc->makeNode<pagx::SolidColor>();
  sSolid->color = {0.0f, 0.0f, 0.0f, 1.0f};
  stroke->color = sSolid;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  layer->contents.push_back(stroke);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_stroke_on_run"));
}

PAGX_TEST(PAGXPPTTest, NativeTextWithRadialGradientFill) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* text = doc->makeNode<pagx::Text>();
  text->text = "Radial";
  text->position = {100, 150};
  text->fontFamily = "Arial";
  text->fontSize = 32;

  auto* grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {200, 150};
  grad->radius = 100;
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.9f, 0.2f, 1.0f};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.8f, 0.2f, 0.1f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(text);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "text_radial_fill"));
}

//==============================================================================
// Bake-path disable toggles (bakeMask covered already; add scrollRect, tiled).
//==============================================================================

PAGX_TEST(PAGXPPTTest, ScrollRectBakeDefault) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->hasScrollRect = true;
  layer->scrollRect = {50, 50, 200, 150};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {300, 200};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.6f, 0.8f, 0.4f, 1.0f}));
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "scrollrect_bake_default"));
}

PAGX_TEST(PAGXPPTTest, ScrollRectBakeDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->hasScrollRect = true;
  layer->scrollRect = {50, 50, 200, 150};
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {300, 200};
  layer->contents.push_back(rect);
  layer->contents.push_back(MakeSolidFill(doc.get(), {0.6f, 0.8f, 0.4f, 1.0f}));
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.bakeScrollRect = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "scrollrect_bake_disabled", options));
}

PAGX_TEST(PAGXPPTTest, TiledPatternBakeDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Repeat;
  pattern->tileModeY = pagx::TileMode::Repeat;
  pattern->matrix = {2, 0, 0, 2, 0, 0};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.bakeTiledPattern = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "tiled_bake_disabled", options));
}

PAGX_TEST(PAGXPPTTest, BridgeContoursDisabled) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 50);
  pathData->lineTo(350, 50);
  pathData->lineTo(350, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  pathData->moveTo(150, 150);
  pathData->lineTo(250, 150);
  pathData->lineTo(250, 250);
  pathData->lineTo(150, 250);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.5f, 0.5f, 0.7f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  pagx::PPTExportOptions options;
  options.bridgeContours = false;
  ASSERT_TRUE(ExportAndVerify(*doc, "bridge_disabled", options));
}

//==============================================================================
// ImagePattern non-decal (clamp) + tile x/y only variants to exercise
// writeImagePatternAsPicture early-return branches.
//==============================================================================

PAGX_TEST(PAGXPPTTest, ImagePatternClampX) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = pagx::TileMode::Clamp;
  pattern->tileModeY = pagx::TileMode::Decal;
  pattern->matrix = {20, 0, 0, 20, 140, 100};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_clamp_x"));
}

PAGX_TEST(PAGXPPTTest, ImagePatternIdentityMatrix) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  auto* image = MakeTestPNGImage(doc.get());
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  // Identity matrix triggers the hasTransform=false branch in
  // writeImagePatternFill so WriteDefaultStretch is used.
  pattern->matrix = {1, 0, 0, 1, 0, 0};

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "imagepattern_identity_matrix"));
}

//==============================================================================
// Composition instance layer: a Layer referencing a Composition propagates
// children through writeLayer's composition loop (separate from layer->children).
//==============================================================================

PAGX_TEST(PAGXPPTTest, LayerWithCompositionInstance) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* comp = doc->makeNode<pagx::Composition>();
  comp->width = 400;
  comp->height = 300;
  auto* inner = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {150, 100};
  inner->contents.push_back(rect);
  inner->contents.push_back(MakeSolidFill(doc.get(), {0.3f, 0.6f, 0.3f, 1.0f}));
  comp->layers.push_back(inner);

  auto* outer = doc->makeNode<pagx::Layer>();
  outer->composition = comp;
  doc->layers.push_back(outer);
  ASSERT_TRUE(ExportAndVerify(*doc, "layer_composition"));
}

//==============================================================================
// Probe-friendly content that stays on the vector (non-raster) path so the
// feature probe returns cleanly with no unsupported features — covers the
// "no escalation" branch of ProbeLayerFeatures / needsRasterization.
//==============================================================================

PAGX_TEST(PAGXPPTTest, AllSupportedFeaturesNoEscalation) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->blendMode = pagx::BlendMode::Multiply;

  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};

  auto* grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {100, 100};
  grad->endPoint = {300, 200};
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.2f, 0.1f, 1.0f};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.1f, 0.4f, 0.9f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "all_supported"));
}

//==============================================================================
// Edge cases for PPTContourUtils: reversed opposite-winding contour groups and
// grouping multiple outer/inner pairs.
//==============================================================================

PAGX_TEST(PAGXPPTTest, PathEvenOddMultipleNestedHoles) {
  auto doc = pagx::PAGXDocument::Make(600, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  // Outer A
  pathData->moveTo(50, 50);
  pathData->lineTo(250, 50);
  pathData->lineTo(250, 350);
  pathData->lineTo(50, 350);
  pathData->close();
  // Hole in A
  pathData->moveTo(100, 100);
  pathData->lineTo(200, 100);
  pathData->lineTo(200, 300);
  pathData->lineTo(100, 300);
  pathData->close();
  // Outer B
  pathData->moveTo(350, 50);
  pathData->lineTo(550, 50);
  pathData->lineTo(550, 350);
  pathData->lineTo(350, 350);
  pathData->close();
  // Hole in B
  pathData->moveTo(400, 100);
  pathData->lineTo(500, 100);
  pathData->lineTo(500, 300);
  pathData->lineTo(400, 300);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.4f, 0.6f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "path_multi_nested"));
}

PAGX_TEST(PAGXPPTTest, PathEvenOddCubicContour) {
  auto doc = pagx::PAGXDocument::Make(400, 400);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* path = doc->makeNode<pagx::Path>();
  auto* pathData = doc->makeNode<pagx::PathData>();
  pathData->moveTo(50, 200);
  pathData->cubicTo(50, 50, 350, 50, 350, 200);
  pathData->cubicTo(350, 350, 50, 350, 50, 200);
  pathData->close();
  pathData->moveTo(150, 200);
  pathData->cubicTo(150, 150, 250, 150, 250, 200);
  pathData->cubicTo(250, 250, 150, 250, 150, 200);
  pathData->close();
  path->data = pathData;

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->fillRule = pagx::FillRule::EvenOdd;
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.3f, 0.7f, 0.2f, 1.0f};
  fill->color = solid;

  layer->contents.push_back(path);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
  ASSERT_TRUE(ExportAndVerify(*doc, "path_cubic_evenodd"));
}

}  // namespace pag

#endif
