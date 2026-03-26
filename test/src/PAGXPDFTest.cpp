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
#include <filesystem>
#include <fstream>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PDFExporter.h"
#include "pagx/SVGImporter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "utils/ProjectPath.h"
#include "utils/TestUtils.h"

namespace pag {

static std::string SavePDFFile(const std::string& content, const std::string& key) {
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

static void AssertValidPDF(const std::string& pdf, const std::string& label = "") {
  ASSERT_FALSE(pdf.empty()) << label << " PDF export failed";
  EXPECT_GE(pdf.size(), 50u) << label << " PDF too small";
  EXPECT_EQ(pdf.substr(0, 5), "%PDF-") << label << " missing PDF header";
  EXPECT_NE(pdf.find("%%EOF"), std::string::npos) << label << " missing PDF trailer";
}

/**
 * Read all SVG files from resources/svg, convert each to PAGX, then export to PDF.
 */
PAGX_TEST(PAGXPDFTest, SVGToPAGXToPDF) {
  std::string svgDir = ProjectPath::Absolute("resources/svg");
  std::vector<std::string> svgFiles = {};

  for (const auto& entry : std::filesystem::directory_iterator(svgDir)) {
    if (entry.path().extension() == ".svg") {
      svgFiles.push_back(entry.path().string());
    }
  }
  std::sort(svgFiles.begin(), svgFiles.end());
  ASSERT_FALSE(svgFiles.empty()) << "No SVG files found in resources/svg";

  for (const auto& svgPath : svgFiles) {
    std::string baseName = std::filesystem::path(svgPath).stem().string();

    auto doc = pagx::SVGImporter::Parse(svgPath);
    if (!doc) {
      ADD_FAILURE() << "Failed to parse SVG: " << svgPath;
      continue;
    }
    if (doc->width <= 0 || doc->height <= 0) {
      continue;
    }

    auto pagxXml = pagx::PAGXExporter::ToXML(*doc);
    ASSERT_FALSE(pagxXml.empty()) << baseName << " PAGX export failed";

    auto reloadedDoc = pagx::PAGXImporter::FromXML(pagxXml);
    ASSERT_NE(reloadedDoc, nullptr) << baseName << " PAGX re-import failed";

    auto pdfContent = pagx::PDFExporter::ToPDF(*reloadedDoc);
    AssertValidPDF(pdfContent, baseName);

    SavePDFFile(pdfContent, "PAGXPDFTest/" + baseName + ".pdf");
  }
}

// Covers: emitEllipsePath, emitRoundedRectPath, emitRectPath (without transform)
PAGX_TEST(PAGXPDFTest, EllipseAndRoundedRect) {
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto layer = doc->makeNode<pagx::Layer>();

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
  ellipse->size = {120, 80};

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 200};
  rect->size = {100, 60};
  rect->roundness = 15;

  auto plainRect = doc->makeNode<pagx::Rectangle>();
  plainRect->position = {50, 250};
  plainRect->size = {80, 40};
  plainRect->roundness = 0;

  auto fill = doc->makeNode<pagx::Fill>();
  auto solid = doc->makeNode<pagx::SolidColor>();
  solid->color = {0.2f, 0.5f, 0.8f, 1.0f};
  fill->color = solid;

  layer->contents = {ellipse, rect, plainRect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "EllipseAndRoundedRect");
  EXPECT_NE(pdf.find(" c\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/EllipseAndRoundedRect.pdf");
}

// Covers: applyStrokeAttrs – round/square cap, round/bevel join, miterLimit, dashes
PAGX_TEST(PAGXPDFTest, StrokeAttributes) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto path1 = doc->makeNode<pagx::Path>();
  auto pd1 = doc->makeNode<pagx::PathData>();
  pd1->moveTo(10, 50);
  pd1->lineTo(190, 50);
  path1->data = pd1;

  auto stroke1 = doc->makeNode<pagx::Stroke>();
  auto sc1 = doc->makeNode<pagx::SolidColor>();
  sc1->color = {1.0f, 0.0f, 0.0f, 1.0f};
  stroke1->color = sc1;
  stroke1->width = 4.0f;
  stroke1->cap = pagx::LineCap::Round;
  stroke1->join = pagx::LineJoin::Round;

  layer->contents = {path1, stroke1};

  auto layer2 = doc->makeNode<pagx::Layer>();
  auto path2 = doc->makeNode<pagx::Path>();
  auto pd2 = doc->makeNode<pagx::PathData>();
  pd2->moveTo(10, 100);
  pd2->lineTo(100, 60);
  pd2->lineTo(190, 100);
  path2->data = pd2;

  auto stroke2 = doc->makeNode<pagx::Stroke>();
  auto sc2 = doc->makeNode<pagx::SolidColor>();
  sc2->color = {0.0f, 0.0f, 1.0f, 1.0f};
  stroke2->color = sc2;
  stroke2->width = 3.0f;
  stroke2->cap = pagx::LineCap::Square;
  stroke2->join = pagx::LineJoin::Bevel;
  stroke2->dashes = {10.0f, 5.0f, 3.0f, 5.0f};
  stroke2->dashOffset = 2.0f;

  layer2->contents = {path2, stroke2};

  auto layer3 = doc->makeNode<pagx::Layer>();
  auto path3 = doc->makeNode<pagx::Path>();
  auto pd3 = doc->makeNode<pagx::PathData>();
  pd3->moveTo(210, 150);
  pd3->lineTo(300, 20);
  pd3->lineTo(390, 150);
  path3->data = pd3;

  auto stroke3 = doc->makeNode<pagx::Stroke>();
  auto sc3 = doc->makeNode<pagx::SolidColor>();
  sc3->color = {0.0f, 0.5f, 0.0f, 1.0f};
  stroke3->color = sc3;
  stroke3->width = 5.0f;
  stroke3->join = pagx::LineJoin::Miter;
  stroke3->miterLimit = 10.0f;

  layer3->contents = {path3, stroke3};

  doc->layers = {layer, layer2, layer3};

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "StrokeAttributes");
  EXPECT_NE(pdf.find(" J\n"), std::string::npos);
  EXPECT_NE(pdf.find(" j\n"), std::string::npos);
  EXPECT_NE(pdf.find(" d\n"), std::string::npos);
  EXPECT_NE(pdf.find(" M\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/StrokeAttributes.pdf");
}

// Covers: fillAndStroke, fillEvenOddAndStroke, fill, fillEvenOdd, stroke, endPath
PAGX_TEST(PAGXPDFTest, FillAndStrokeCombinations) {
  auto doc = pagx::PAGXDocument::Make(300, 300);

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {75, 75};
    rect->size = {100, 100};

    auto fill = doc->makeNode<pagx::Fill>();
    auto fc = doc->makeNode<pagx::SolidColor>();
    fc->color = {1.0f, 0.8f, 0.0f, 1.0f};
    fill->color = fc;
    fill->fillRule = pagx::FillRule::EvenOdd;

    auto stroke = doc->makeNode<pagx::Stroke>();
    auto stc = doc->makeNode<pagx::SolidColor>();
    stc->color = {0.0f, 0.0f, 0.0f, 1.0f};
    stroke->color = stc;
    stroke->width = 2.0f;

    layer->contents = {rect, fill, stroke};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {225, 75};
    rect->size = {100, 100};

    auto stroke = doc->makeNode<pagx::Stroke>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0.8f, 0.0f, 0.0f, 1.0f};
    stroke->color = sc;
    stroke->width = 3.0f;

    layer->contents = {rect, stroke};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {75, 225};
    rect->size = {100, 100};

    auto fill = doc->makeNode<pagx::Fill>();
    auto fc = doc->makeNode<pagx::SolidColor>();
    fc->color = {0.0f, 0.6f, 0.0f, 1.0f};
    fill->color = fc;
    fill->fillRule = pagx::FillRule::Winding;

    auto stroke = doc->makeNode<pagx::Stroke>();
    auto stc = doc->makeNode<pagx::SolidColor>();
    stc->color = {0.0f, 0.0f, 0.5f, 1.0f};
    stroke->color = stc;
    stroke->width = 2.0f;

    layer->contents = {rect, fill, stroke};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {225, 225};
    rect->size = {100, 100};
    layer->contents = {rect};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "FillAndStrokeCombinations");
  EXPECT_NE(pdf.find("B*\n"), std::string::npos);
  EXPECT_NE(pdf.find("S\n"), std::string::npos);
  EXPECT_NE(pdf.find("n\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/FillAndStrokeCombinations.pdf");
}

// Covers: LinearGradient with alpha stops → addLinearGradientAlphaMask, HasNonOpaqueStops
PAGX_TEST(PAGXPDFTest, LinearGradientWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {180, 180};

  auto stop0 = doc->makeNode<pagx::ColorStop>();
  stop0->offset = 0.0f;
  stop0->color = {1.0f, 0.0f, 0.0f, 1.0f};
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 1.0f;
  stop1->color = {0.0f, 0.0f, 1.0f, 0.3f};
  grad->colorStops = {stop0, stop1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LinearGradientWithAlpha");
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
  EXPECT_NE(pdf.find("ShadingType 2"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/LinearGradientWithAlpha.pdf");
}

// Covers: RadialGradient with alpha stops → addRadialGradientAlphaMask
PAGX_TEST(PAGXPDFTest, RadialGradientWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
  ellipse->size = {180, 180};

  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {90, 90};
  grad->radius = 90;

  auto stop0 = doc->makeNode<pagx::ColorStop>();
  stop0->offset = 0.0f;
  stop0->color = {1.0f, 1.0f, 0.0f, 0.8f};
  auto stop1 = doc->makeNode<pagx::ColorStop>();
  stop1->offset = 1.0f;
  stop1->color = {0.5f, 0.0f, 0.5f, 0.2f};
  grad->colorStops = {stop0, stop1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {ellipse, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "RadialGradientWithAlpha");
  EXPECT_NE(pdf.find("ShadingType 3"), std::string::npos);
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/RadialGradientWithAlpha.pdf");
}

// Covers: multi-stop gradient → buildStitchingFunction (FunctionType 3)
PAGX_TEST(PAGXPDFTest, MultiStopGradient) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 50};
  rect->size = {280, 80};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {280, 0};

  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0.0f;
  s0->color = {1, 0, 0, 1};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.25f;
  s1->color = {1, 1, 0, 1};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 0.5f;
  s2->color = {0, 1, 0, 1};
  auto s3 = doc->makeNode<pagx::ColorStop>();
  s3->offset = 0.75f;
  s3->color = {0, 1, 1, 1};
  auto s4 = doc->makeNode<pagx::ColorStop>();
  s4->offset = 1.0f;
  s4->color = {0, 0, 1, 1};
  grad->colorStops = {s0, s1, s2, s3, s4};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "MultiStopGradient");
  EXPECT_NE(pdf.find("FunctionType 3"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/MultiStopGradient.pdf");
}

// Covers: writeTextAsPDFText (ASCII path), ensureDefaultFont, EscapePDFString
PAGX_TEST(PAGXPDFTest, TextAsPDFText_ASCII) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Hello (world) \\ test";
  text->position = {20, 50};
  text->fontSize = 24.0f;

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  fill->color = sc;

  layer->contents = {text, fill};
  doc->layers.push_back(layer);

  pagx::PDFExporter::Options opts;
  opts.convertTextToPath = false;
  auto pdf = pagx::PDFExporter::ToPDF(*doc, opts);
  AssertValidPDF(pdf, "TextAsPDFText_ASCII");
  EXPECT_NE(pdf.find("BT\n"), std::string::npos);
  EXPECT_NE(pdf.find("Tf\n"), std::string::npos);
  EXPECT_NE(pdf.find("Helvetica"), std::string::npos);
  EXPECT_NE(pdf.find("\\("), std::string::npos);
  EXPECT_NE(pdf.find("\\)"), std::string::npos);
  EXPECT_NE(pdf.find("\\\\"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/TextAsPDFText_ASCII.pdf");
}

// Covers: writeTextAsPDFText (non-ASCII CID font path), ensureCIDFont, showTextHex
PAGX_TEST(PAGXPDFTest, TextAsPDFText_NonASCII) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();

  auto text = doc->makeNode<pagx::Text>();
  text->text = "\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95";
  text->position = {20, 60};
  text->fontSize = 20.0f;

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  fill->color = sc;

  layer->contents = {text, fill};
  doc->layers.push_back(layer);

  pagx::PDFExporter::Options opts;
  opts.convertTextToPath = false;
  auto pdf = pagx::PDFExporter::ToPDF(*doc, opts);
  AssertValidPDF(pdf, "TextAsPDFText_NonASCII");
  EXPECT_NE(pdf.find("UniGB-UCS2-H"), std::string::npos);
  EXPECT_NE(pdf.find("> Tj\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/TextAsPDFText_NonASCII.pdf");
}

// Covers: writeText with convertTextToPath=true but no glyphRuns → falls back to PDF text
PAGX_TEST(PAGXPDFTest, TextFallbackToNativeText) {
  auto doc = pagx::PAGXDocument::Make(200, 80);
  auto layer = doc->makeNode<pagx::Layer>();

  auto text = doc->makeNode<pagx::Text>();
  text->text = "Fallback";
  text->position = {10, 40};
  text->fontSize = 16.0f;

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 0, 1};
  fill->color = sc;

  layer->contents = {text, fill};
  doc->layers.push_back(layer);

  pagx::PDFExporter::Options opts;
  opts.convertTextToPath = true;
  auto pdf = pagx::PDFExporter::ToPDF(*doc, opts);
  AssertValidPDF(pdf, "TextFallbackToNativeText");
  EXPECT_NE(pdf.find("BT\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/TextFallbackToNativeText.pdf");
}

// Covers: empty text skips (writeText, writeTextAsPDFText early returns)
PAGX_TEST(PAGXPDFTest, EmptyTextAndEmptyPath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto text = doc->makeNode<pagx::Text>();
    text->text = "";
    text->position = {10, 50};

    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0, 0, 0, 1};
    fill->color = sc;

    layer->contents = {text, fill};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto path = doc->makeNode<pagx::Path>();
    path->data = nullptr;

    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0, 0, 0, 1};
    fill->color = sc;

    layer->contents = {path, fill};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    auto path = doc->makeNode<pagx::Path>();
    auto pd = doc->makeNode<pagx::PathData>();
    path->data = pd;

    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0, 0, 0, 1};
    fill->color = sc;

    layer->contents = {path, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "EmptyTextAndEmptyPath");
}

// Covers: layer visible=false (writeLayer early return), layer x/y offset, layer alpha
PAGX_TEST(PAGXPDFTest, LayerVisibilityAlphaOffset) {
  auto doc = pagx::PAGXDocument::Make(300, 200);

  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->visible = false;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {50, 50};
    rect->size = {80, 80};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {1, 0, 0, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->alpha = 0.5f;
    layer->x = 20.0f;
    layer->y = 30.0f;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {100, 60};
    rect->size = {80, 60};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0, 0.5f, 1, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerVisibilityAlphaOffset");
  EXPECT_NE(pdf.find("/ca"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/LayerVisibilityAlphaOffset.pdf");
}

// Covers: layer blendMode → getBlendModeExtGState, BlendModeToPDFName
PAGX_TEST(PAGXPDFTest, LayerBlendModes) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  pagx::BlendMode modes[] = {
      pagx::BlendMode::Multiply,   pagx::BlendMode::Screen,    pagx::BlendMode::Overlay,
      pagx::BlendMode::Darken,     pagx::BlendMode::Lighten,   pagx::BlendMode::ColorDodge,
      pagx::BlendMode::ColorBurn,  pagx::BlendMode::HardLight, pagx::BlendMode::SoftLight,
      pagx::BlendMode::Difference, pagx::BlendMode::Exclusion, pagx::BlendMode::Hue,
      pagx::BlendMode::Saturation, pagx::BlendMode::Color,     pagx::BlendMode::Luminosity,
  };

  for (auto mode : modes) {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->blendMode = mode;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {100, 100};
    rect->size = {50, 50};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0.5f, 0.5f, 0.5f, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerBlendModes");
  EXPECT_NE(pdf.find("/BM /Multiply"), std::string::npos);
  EXPECT_NE(pdf.find("/BM /Screen"), std::string::npos);
  EXPECT_NE(pdf.find("/BM /Luminosity"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/LayerBlendModes.pdf");
}

// Covers: layer with unsupported blend mode (PlusLighter → returns nullptr from BlendModeToPDFName)
PAGX_TEST(PAGXPDFTest, LayerUnsupportedBlendMode) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->blendMode = pagx::BlendMode::PlusLighter;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.5f, 0.5f, 0.5f, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerUnsupportedBlendMode");
}

// Covers: contour mask → writeClipPath, clip, clipEvenOdd, endPath
PAGX_TEST(PAGXPDFTest, ContourMask) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskEllipse = doc->makeNode<pagx::Ellipse>();
  maskEllipse->position = {100, 100};
  maskEllipse->size = {160, 160};
  maskLayer->contents = {maskEllipse};

  auto layer = doc->makeNode<pagx::Layer>();
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0.7f, 0, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "ContourMask");
  EXPECT_NE(pdf.find("W\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/ContourMask.pdf");
}

// Covers: contour mask with EvenOdd fill rule → clipEvenOdd
PAGX_TEST(PAGXPDFTest, ContourMaskEvenOdd) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {180, 180};
  auto maskFill = doc->makeNode<pagx::Fill>();
  maskFill->fillRule = pagx::FillRule::EvenOdd;
  maskLayer->contents = {maskRect, maskFill};

  auto layer = doc->makeNode<pagx::Layer>();
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.8f, 0.2f, 0, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "ContourMaskEvenOdd");
  EXPECT_NE(pdf.find("W*\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/ContourMaskEvenOdd.pdf");
}

// Covers: soft mask (Alpha + Luminance) → writeSoftMask, renderMaskLayerContent, addSoftMaskExtGState
PAGX_TEST(PAGXPDFTest, SoftMask) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  {
    auto maskLayer = doc->makeNode<pagx::Layer>();
    auto maskRect = doc->makeNode<pagx::Rectangle>();
    maskRect->position = {100, 100};
    maskRect->size = {120, 120};
    auto maskFill = doc->makeNode<pagx::Fill>();
    auto mc = doc->makeNode<pagx::SolidColor>();
    mc->color = {1, 1, 1, 0.5f};
    maskFill->color = mc;
    maskLayer->contents = {maskRect, maskFill};

    auto layer = doc->makeNode<pagx::Layer>();
    layer->mask = maskLayer;
    layer->maskType = pagx::MaskType::Alpha;

    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {100, 100};
    rect->size = {160, 160};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0, 0, 1, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  {
    auto maskLayer = doc->makeNode<pagx::Layer>();
    maskLayer->alpha = 0.7f;
    maskLayer->matrix = pagx::Matrix::Translate(10, 10);
    auto maskEllipse = doc->makeNode<pagx::Ellipse>();
    maskEllipse->position = {100, 100};
    maskEllipse->size = {100, 100};
    auto maskFill = doc->makeNode<pagx::Fill>();
    auto mc = doc->makeNode<pagx::SolidColor>();
    mc->color = {1, 1, 1, 1};
    maskFill->color = mc;
    maskLayer->contents = {maskEllipse, maskFill};

    auto layer = doc->makeNode<pagx::Layer>();
    layer->mask = maskLayer;
    layer->maskType = pagx::MaskType::Luminance;

    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {100, 100};
    rect->size = {180, 180};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {1, 0, 0, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "SoftMask");
  EXPECT_NE(pdf.find("/Alpha"), std::string::npos);
  EXPECT_NE(pdf.find("/Luminosity"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/SoftMask.pdf");
}

// Covers: soft mask with nested children → writeClipPath recursion with children
PAGX_TEST(PAGXPDFTest, ContourMaskWithChildren) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {180, 180};
  maskLayer->contents = {maskRect};

  auto childMask = doc->makeNode<pagx::Layer>();
  auto childEllipse = doc->makeNode<pagx::Ellipse>();
  childEllipse->position = {100, 100};
  childEllipse->size = {100, 100};
  childMask->contents = {childEllipse};
  maskLayer->children.push_back(childMask);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Contour;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.3f, 0.3f, 0.8f, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "ContourMaskWithChildren");
  SavePDFFile(pdf, "PAGXPDFTest/ContourMaskWithChildren.pdf");
}

// Covers: soft mask with nested children → renderMaskLayerContent recursion
PAGX_TEST(PAGXPDFTest, SoftMaskWithChildren) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  auto maskLayer = doc->makeNode<pagx::Layer>();
  auto maskRect = doc->makeNode<pagx::Rectangle>();
  maskRect->position = {100, 100};
  maskRect->size = {180, 180};
  auto maskFill = doc->makeNode<pagx::Fill>();
  auto mc = doc->makeNode<pagx::SolidColor>();
  mc->color = {1, 1, 1, 1};
  maskFill->color = mc;
  maskLayer->contents = {maskRect, maskFill};

  auto childMask = doc->makeNode<pagx::Layer>();
  auto childEllipse = doc->makeNode<pagx::Ellipse>();
  childEllipse->position = {100, 100};
  childEllipse->size = {80, 80};
  auto childFill = doc->makeNode<pagx::Fill>();
  auto cfc = doc->makeNode<pagx::SolidColor>();
  cfc->color = {0.5f, 0.5f, 0.5f, 1};
  childFill->color = cfc;
  childMask->contents = {childEllipse, childFill};
  maskLayer->children.push_back(childMask);

  auto layer = doc->makeNode<pagx::Layer>();
  layer->mask = maskLayer;
  layer->maskType = pagx::MaskType::Alpha;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {200, 200};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0.8f, 0.2f, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "SoftMaskWithChildren");
  SavePDFFile(pdf, "PAGXPDFTest/SoftMaskWithChildren.pdf");
}

// Covers: Group with alpha and non-identity transform → writeElements Group branch
PAGX_TEST(PAGXPDFTest, GroupWithAlphaAndTransform) {
  auto doc = pagx::PAGXDocument::Make(300, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto group = doc->makeNode<pagx::Group>();
  group->position = {50, 30};
  group->alpha = 0.6f;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {60, 40};
  rect->size = {100, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.9f, 0.1f, 0.3f, 1};
  fill->color = sc;
  group->elements = {rect, fill};

  auto fill2 = doc->makeNode<pagx::Fill>();
  auto sc2 = doc->makeNode<pagx::SolidColor>();
  sc2->color = {0, 0, 0, 1};
  fill2->color = sc2;
  layer->contents = {group, fill2};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GroupWithAlphaAndTransform");
  SavePDFFile(pdf, "PAGXPDFTest/GroupWithAlphaAndTransform.pdf");
}

// Covers: Group without alpha and with identity transform (else branch in writeElements)
PAGX_TEST(PAGXPDFTest, GroupNoAlphaIdentityTransform) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto group = doc->makeNode<pagx::Group>();
  group->alpha = 1.0f;

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.1f, 0.1f, 0.9f, 1};
  fill->color = sc;
  group->elements = {rect, fill};

  auto fill2 = doc->makeNode<pagx::Fill>();
  auto sc2 = doc->makeNode<pagx::SolidColor>();
  sc2->color = {0, 0, 0, 1};
  fill2->color = sc2;
  layer->contents = {group, fill2};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GroupNoAlphaIdentityTransform");
}

// Covers: path with quad bezier (PathVerb::Quad → cubic conversion)
PAGX_TEST(PAGXPDFTest, PathWithQuadBezier) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto path = doc->makeNode<pagx::Path>();
  auto pd = doc->makeNode<pagx::PathData>();
  pd->moveTo(20, 180);
  pd->quadTo(100, 10, 180, 180);
  pd->close();
  path->data = pd;

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.6f, 0, 0.6f, 1};
  fill->color = sc;

  layer->contents = {path, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "PathWithQuadBezier");
  EXPECT_NE(pdf.find(" c\n"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/PathWithQuadBezier.pdf");
}

// Covers: path with cubic bezier and close
PAGX_TEST(PAGXPDFTest, PathWithCubicBezier) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto path = doc->makeNode<pagx::Path>();
  auto pd = doc->makeNode<pagx::PathData>();
  pd->moveTo(20, 100);
  pd->cubicTo(60, 10, 140, 190, 180, 100);
  pd->close();
  path->data = pd;

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0.4f, 0.8f, 1};
  fill->color = sc;

  layer->contents = {path, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "PathWithCubicBezier");
  EXPECT_NE(pdf.find(" c\n"), std::string::npos);
  EXPECT_NE(pdf.find("h\n"), std::string::npos);
}

// Covers: PDFExporter::ToFile
PAGX_TEST(PAGXPDFTest, ToFile) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.2f, 0.8f, 0.2f, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto outDir = ProjectPath::Absolute("test/out/PAGXPDFTest");
  std::filesystem::create_directories(outDir);
  auto outPath = outDir + "/ToFile.pdf";
  ASSERT_TRUE(pagx::PDFExporter::ToFile(*doc, outPath));
  ASSERT_TRUE(std::filesystem::exists(outPath));
  auto fileSize = std::filesystem::file_size(outPath);
  EXPECT_GT(fileSize, 50u);

  EXPECT_FALSE(pagx::PDFExporter::ToFile(*doc, "/nonexistent_dir/impossible.pdf"));
}

// Covers: fill alpha < 1, stroke alpha < 1 → getExtGState
PAGX_TEST(PAGXPDFTest, FillStrokeAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};

  auto fill = doc->makeNode<pagx::Fill>();
  auto fc = doc->makeNode<pagx::SolidColor>();
  fc->color = {1, 0, 0, 1};
  fill->color = fc;
  fill->alpha = 0.5f;

  auto stroke = doc->makeNode<pagx::Stroke>();
  auto stc = doc->makeNode<pagx::SolidColor>();
  stc->color = {0, 0, 1, 1};
  stroke->color = stc;
  stroke->width = 3.0f;
  stroke->alpha = 0.7f;

  layer->contents = {rect, fill, stroke};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "FillStrokeAlpha");
  EXPECT_NE(pdf.find("/ca"), std::string::npos);
  EXPECT_NE(pdf.find("/CA"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/FillStrokeAlpha.pdf");
}

// Covers: gradient on stroke → setStrokePattern
PAGX_TEST(PAGXPDFTest, GradientStroke) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {160, 0};
  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 0, 0, 1};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0, 0, 1, 1};
  grad->colorStops = {s0, s1};

  auto stroke = doc->makeNode<pagx::Stroke>();
  stroke->color = grad;
  stroke->width = 4.0f;

  layer->contents = {rect, stroke};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GradientStroke");
  EXPECT_NE(pdf.find("/Pattern CS"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/GradientStroke.pdf");
}

// Covers: gradient fill → setFillPattern
PAGX_TEST(PAGXPDFTest, GradientFillPattern) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {180, 0};
  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {0, 1, 0, 1};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {1, 0, 1, 1};
  grad->colorStops = {s0, s1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GradientFillPattern");
  EXPECT_NE(pdf.find("/Pattern cs"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/GradientFillPattern.pdf");
}

// Covers: layer with non-identity matrix → concatMatrix in writeLayer
PAGX_TEST(PAGXPDFTest, LayerWithMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();
  layer->matrix = pagx::Matrix::Rotate(15);

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.8f, 0.4f, 0, 1};
  fill->color = sc;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerWithMatrix");
  EXPECT_NE(pdf.find(" cm\n"), std::string::npos);
}

// Covers: layer with child layers (recursion in writeLayer)
PAGX_TEST(PAGXPDFTest, LayerWithChildren) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto parent = doc->makeNode<pagx::Layer>();

  auto child = doc->makeNode<pagx::Layer>();
  child->x = 10;
  child->y = 10;
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {60, 60};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0, 0, 1, 1};
  fill->color = sc;
  child->contents = {rect, fill};
  parent->children.push_back(child);

  auto parentRect = doc->makeNode<pagx::Rectangle>();
  parentRect->position = {100, 100};
  parentRect->size = {180, 180};
  auto parentFill = doc->makeNode<pagx::Fill>();
  auto psc = doc->makeNode<pagx::SolidColor>();
  psc->color = {1, 1, 0, 0.3f};
  parentFill->color = psc;
  parent->contents = {parentRect, parentFill};

  doc->layers.push_back(parent);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerWithChildren");
  SavePDFFile(pdf, "PAGXPDFTest/LayerWithChildren.pdf");
}

// Covers: gradient with non-identity matrix → ctm * grad->matrix in addLinearGradient
PAGX_TEST(PAGXPDFTest, GradientWithMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {180, 180};
  grad->matrix = pagx::Matrix::Rotate(45);

  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 0.5f, 0, 1};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0, 0.5f, 1, 1};
  grad->colorStops = {s0, s1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GradientWithMatrix");
  EXPECT_NE(pdf.find("/Matrix"), std::string::npos);
}

// Covers: radial gradient (without alpha) → addRadialGradient, ShadingType 3
PAGX_TEST(PAGXPDFTest, RadialGradientOpaque) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
  ellipse->size = {160, 160};

  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {80, 80};
  grad->radius = 80;
  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 1, 1, 1};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0, 0, 0, 1};
  grad->colorStops = {s0, s1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;

  layer->contents = {ellipse, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "RadialGradientOpaque");
  EXPECT_NE(pdf.find("ShadingType 3"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/RadialGradientOpaque.pdf");
}

// Covers: multi-stop gradient with alpha → stitching function + alpha mask for >2 stops
PAGX_TEST(PAGXPDFTest, MultiStopGradientWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(300, 100);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {150, 50};
  rect->size = {280, 80};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {280, 0};

  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 0, 0, 0.9f};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.5f;
  s1->color = {0, 1, 0, 0.4f};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1;
  s2->color = {0, 0, 1, 0.1f};
  grad->colorStops = {s0, s1, s2};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "MultiStopGradientWithAlpha");
  EXPECT_NE(pdf.find("FunctionType 3"), std::string::npos);
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
  SavePDFFile(pdf, "PAGXPDFTest/MultiStopGradientWithAlpha.pdf");
}

// Covers: multi-stop radial gradient with alpha → radial stitching + radial alpha mask
PAGX_TEST(PAGXPDFTest, MultiStopRadialGradientWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto ellipse = doc->makeNode<pagx::Ellipse>();
  ellipse->position = {100, 100};
  ellipse->size = {180, 180};

  auto grad = doc->makeNode<pagx::RadialGradient>();
  grad->center = {90, 90};
  grad->radius = 90;
  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 1, 0, 1.0f};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 0.5f;
  s1->color = {1, 0, 1, 0.5f};
  auto s2 = doc->makeNode<pagx::ColorStop>();
  s2->offset = 1;
  s2->color = {0, 1, 1, 0.0f};
  grad->colorStops = {s0, s1, s2};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents = {ellipse, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "MultiStopRadialGradientWithAlpha");
  EXPECT_NE(pdf.find("ShadingType 3"), std::string::npos);
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
}

// Covers: gradient softMaskGSName on stroke → strokeRef.softMaskGSName branch
PAGX_TEST(PAGXPDFTest, GradientAlphaOnStroke) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {160, 160};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {160, 0};
  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1, 0, 0, 1.0f};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0, 1, 0, 0.2f};
  grad->colorStops = {s0, s1};

  auto stroke = doc->makeNode<pagx::Stroke>();
  stroke->color = grad;
  stroke->width = 6.0f;

  layer->contents = {rect, stroke};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GradientAlphaOnStroke");
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
  EXPECT_NE(pdf.find("/Pattern CS"), std::string::npos);
}

// Covers: SolidColor with alpha < 1 in color itself → resolveColorSource alpha path
PAGX_TEST(PAGXPDFTest, SolidColorWithAlpha) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};

  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {1, 0, 0, 0.4f};
  fill->color = sc;

  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "SolidColorWithAlpha");
  EXPECT_NE(pdf.find("/ca"), std::string::npos);
}

// Covers: gradient alpha mask with non-identity gradient matrix
PAGX_TEST(PAGXPDFTest, GradientAlphaMaskWithGradientMatrix) {
  auto doc = pagx::PAGXDocument::Make(200, 200);
  auto layer = doc->makeNode<pagx::Layer>();

  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {100, 100};
  rect->size = {180, 180};

  auto grad = doc->makeNode<pagx::LinearGradient>();
  grad->startPoint = {0, 0};
  grad->endPoint = {180, 180};
  grad->matrix = pagx::Matrix::Scale(0.5f, 0.5f);

  auto s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {0, 0, 1, 1.0f};
  auto s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {1, 0, 0, 0.1f};
  grad->colorStops = {s0, s1};

  auto fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents = {rect, fill};
  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "GradientAlphaMaskWithGradientMatrix");
  EXPECT_NE(pdf.find("/SMask"), std::string::npos);
  EXPECT_NE(pdf.find(" cm\n"), std::string::npos);
}

// Covers: ExtGState cache hit path (same alpha values reused)
PAGX_TEST(PAGXPDFTest, ExtGStateCacheHit) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  for (int i = 0; i < 3; i++) {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->alpha = 0.5f;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {50.0f + static_cast<float>(i) * 50, 100};
    rect->size = {40, 40};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0.5f, 0.5f, 0.5f, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "ExtGStateCacheHit");
}

// Covers: BlendMode cache hit (same mode reused)
PAGX_TEST(PAGXPDFTest, BlendModeCacheHit) {
  auto doc = pagx::PAGXDocument::Make(200, 200);

  for (int i = 0; i < 3; i++) {
    auto layer = doc->makeNode<pagx::Layer>();
    layer->blendMode = pagx::BlendMode::Multiply;
    auto rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {50.0f + static_cast<float>(i) * 50, 100};
    rect->size = {40, 40};
    auto fill = doc->makeNode<pagx::Fill>();
    auto sc = doc->makeNode<pagx::SolidColor>();
    sc->color = {0.5f, 0.5f, 0.5f, 1};
    fill->color = sc;
    layer->contents = {rect, fill};
    doc->layers.push_back(layer);
  }

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "BlendModeCacheHit");
}

// Covers: layer needsGroup=false (no matrix, no alpha, no mask, no x/y, Normal blend)
PAGX_TEST(PAGXPDFTest, LayerNoGroup) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  auto rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {50, 50};
  rect->size = {80, 80};
  auto fill = doc->makeNode<pagx::Fill>();
  auto sc = doc->makeNode<pagx::SolidColor>();
  sc->color = {0.3f, 0.6f, 0.9f, 1};
  fill->color = sc;
  layer->contents = {rect, fill};

  auto child = doc->makeNode<pagx::Layer>();
  auto childRect = doc->makeNode<pagx::Rectangle>();
  childRect->position = {50, 50};
  childRect->size = {40, 40};
  auto childFill = doc->makeNode<pagx::Fill>();
  auto csc = doc->makeNode<pagx::SolidColor>();
  csc->color = {0.9f, 0.3f, 0.1f, 1};
  childFill->color = csc;
  child->contents = {childRect, childFill};
  layer->children.push_back(child);

  doc->layers.push_back(layer);

  auto pdf = pagx::PDFExporter::ToPDF(*doc);
  AssertValidPDF(pdf, "LayerNoGroup");
}

}  // namespace pag
