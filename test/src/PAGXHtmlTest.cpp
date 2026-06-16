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

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/html/Woff2FontGenerator.h"
#include "pagx/nodes/Font.h"
#include "tgfx/core/ImageCodec.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"

namespace pag {

static std::string LoadAndConvert(const std::string& pagxPath,
                                  const pagx::HTMLExportOptions& options = {}) {
  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  if (doc == nullptr) {
    return "";
  }
  // Shared tmp asset dir for all string-assertion tests. Individual tests assert HTML text
  // content only; the PNG files written here are not validated, so cross-test filename
  // collisions (e.g. dgc0.png from two different samples) are harmless.
  auto tmpAssets = ProjectPath::Absolute("test/out/PAGXHtmlTest/tmp-assets");
  return pagx::HTMLExporter::ToHTML(*doc, tmpAssets, pagx::HTMLOutputMode::Fragment, options);
}

static std::string LoadXMLAndConvert(const std::string& xml,
                                     const pagx::HTMLExportOptions& options = {}) {
  auto doc = pagx::PAGXImporter::FromXML(xml);
  if (doc == nullptr) {
    return "";
  }
  auto tmpAssets = ProjectPath::Absolute("test/out/PAGXHtmlTest/tmp-assets");
  return pagx::HTMLExporter::ToHTML(*doc, tmpAssets, pagx::HTMLOutputMode::Fragment, options);
}

static std::vector<std::string> GetHtmlTestFiles() {
  std::vector<std::string> files = {};
  auto dir = ProjectPath::Absolute("resources/pagx_to_html");
  if (!std::filesystem::exists(dir)) {
    return files;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".pagx") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

static std::string WrapHtmlDocument(const std::string& fragment, int width, int height) {
  return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><style>"
         "body{margin:0;padding:0;background:transparent;width:" +
         std::to_string(width) + "px;height:" + std::to_string(height) +
         "px;overflow:hidden}</style></head>\n<body>\n" + fragment + "\n</body></html>";
}

// Result of exporting one PAGX sample into a wrapped HTML file on disk.
struct ExportedSample {
  bool success = false;  // false when load/export fails; inspect logs for the reason
  int width = 0;         // document width, needed by screenshot callers
  int height = 0;        // document height, same
  std::string htmlPath;  // absolute path of the wrapped HTML file written to disk
};

// Generates the wrapped HTML for one PAGX sample and writes it next to `outDir`. Rasterizes
// any Diamond / tiled-ImagePattern / PlusDarker fills into `outDir/static-img/` using a
// per-sample name prefix so multiple samples can share that single directory without id
// collisions. This helper is shared by BatchConvertAll (structural smoke test) and
// HtmlScreenshotCompare (Puppeteer pixel comparison) so the two paths stay in lockstep —
// any change to HTMLExportOptions, wrapper contents, or output paths needs to happen here
// and affect both call sites at once.
static ExportedSample ExportSampleHtmlToFile(const std::string& pagxPath,
                                             const std::string& outDir) {
  ExportedSample result;
  auto baseName = std::filesystem::path(pagxPath).stem().string();
  result.htmlPath = outDir + "/" + baseName + ".html";

  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  if (!doc) {
    std::cerr << "ExportSampleHtmlToFile: failed to load " << pagxPath << "\n";
    return result;
  }

  result.width = static_cast<int>(doc->width);
  result.height = static_cast<int>(doc->height);

  pagx::HTMLExportOptions opts;
  // Resource directory follows the new HTMLExporter convention: sibling directory named after
  // the HTML file stem. Multiple samples in the same outDir therefore get independent asset
  // sub-directories (outDir/<baseName>/), no filename-prefix gymnastics required.
  auto resourceDir = outDir + "/" + baseName;
  auto fragment =
      pagx::HTMLExporter::ToHTML(*doc, resourceDir, pagx::HTMLOutputMode::Fragment, opts);
  if (fragment.empty()) {
    std::cerr << "ExportSampleHtmlToFile: ToHTML returned empty for " << baseName << "\n";
    return result;
  }
  if (fragment.find("data-pagx-version") == std::string::npos) {
    std::cerr << "ExportSampleHtmlToFile: missing data-pagx-version in " << baseName << "\n";
    return result;
  }

  auto fullHtml = WrapHtmlDocument(fragment, result.width, result.height);
  std::ofstream htmlFile(result.htmlPath, std::ios::binary);
  if (!htmlFile) {
    std::cerr << "ExportSampleHtmlToFile: cannot open " << result.htmlPath << " for writing\n";
    return result;
  }
  htmlFile.write(fullHtml.data(), static_cast<std::streamsize>(fullHtml.size()));

  result.success = true;
  return result;
}

// =============================================================================
// Batch test: ensure every .pagx file in resources/pagx_to_html/ converts successfully
// =============================================================================

CLI_TEST(PAGXHtmlTest, BatchConvertAll) {
  auto files = GetHtmlTestFiles();
  ASSERT_FALSE(files.empty()) << "No .pagx files found in resources/pagx_to_html/";
  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);
  for (const auto& filePath : files) {
    auto result = ExportSampleHtmlToFile(filePath, outDir);
    EXPECT_TRUE(result.success) << "Failed to export: " << filePath;
  }
}

// =============================================================================
// Root document
// =============================================================================

CLI_TEST(PAGXHtmlTest, RootDocument) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("data-pagx-version"), std::string::npos);
  EXPECT_NE(html.find("width: 320px"), std::string::npos);
  EXPECT_NE(html.find("height: 220px"), std::string::npos);
  EXPECT_NE(html.find("overflow: hidden"), std::string::npos);
  EXPECT_NE(html.find("position: relative"), std::string::npos);
  EXPECT_NE(html.find("data-pagx-version"), std::string::npos);
}

// =============================================================================
// Layer attributes
// =============================================================================

CLI_TEST(PAGXHtmlTest, LayerVisibility) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_visibility.pagx"));
  ASSERT_FALSE(html.empty());
  // Accept either the class form ("display: none" in a CSS rule) or the inlined form
  // ("display:none" in a style="..." attribute) — both are valid Pretty output.
  EXPECT_TRUE(html.find("display: none") != std::string::npos ||
              html.find("display:none") != std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerAlpha) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_alpha.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("opacity: 0.3"), std::string::npos);
  EXPECT_NE(html.find("opacity: 0.7"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformXY) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_xy.pagx"));
  ASSERT_FALSE(html.empty());
  // Layer x/y resolve to renderPosition and are emitted as left/top on absolute-positioned layers.
  EXPECT_NE(html.find("left: 55px"), std::string::npos);
  EXPECT_NE(html.find("left: 145px"), std::string::npos);
  EXPECT_NE(html.find("top: 50px"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformMatrix) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_matrix.pagx"));
  ASSERT_FALSE(html.empty());
  // A 45-degree rotation matrix is simplified to rotate(45deg) instead of matrix(...).
  EXPECT_NE(html.find("rotate("), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformMatrix3D) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_matrix3d.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("matrix3d("), std::string::npos);
  // Accept both Pretty class-rule ("transform-style: preserve-3d") and inlined
  // ("transform-style:preserve-3d") forms.
  EXPECT_TRUE(html.find("transform-style: preserve-3d") != std::string::npos ||
              html.find("transform-style:preserve-3d") != std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformPriority) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_priority.pagx"));
  ASSERT_FALSE(html.empty());
  // The first layer should use translate (no matrix/matrix3D)
  EXPECT_NE(html.find("translate(10px,10px)"), std::string::npos);
  // The second layer: matrix overrides x/y, should use matrix() not translate(999,999)
  EXPECT_EQ(html.find("translate(999px,999px)"), std::string::npos);
  // The third layer: matrix3D overrides both matrix and x/y
  EXPECT_NE(html.find("matrix3d("), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerBlendModes) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_blend_modes.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("mix-blend-mode: multiply"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: screen"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: overlay"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: darken"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: lighten"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: color-dodge"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: color-burn"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: hard-light"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: soft-light"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: difference"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: exclusion"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: hue"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: saturation"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: luminosity"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode: plus-lighter"), std::string::npos);
  // plusDarker has no CSS equivalent — should have some fallback
}

CLI_TEST(PAGXHtmlTest, LayerGroupOpacity) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_group_opacity.pagx"));
  ASSERT_FALSE(html.empty());
  // groupOpacity=true: opacity on the parent div
  // groupOpacity=false: opacity distributed to children
  EXPECT_NE(html.find("opacity: 0.5"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, MultipleDropShadowStylesUseOneFilterSource) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="160" height="120">
  <Layer width="100" height="80" matrix="1,0,0,1,30,20">
    <Rectangle position="50,40" size="100,80" roundness="12"/>
    <Fill color="#FFFFFF"/>
    <DropShadowStyle offsetY="5" blurX="10" blurY="10" color="#0000001A" showBehindLayer="false"/>
    <DropShadowStyle offsetY="5" blurX="5" blurY="5" color="#0000001A" showBehindLayer="false"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  size_t filterCount = 0;
  for (size_t pos = 0; (pos = html.find("<filter ", pos)) != std::string::npos; pos++) {
    filterCount++;
  }
  EXPECT_EQ(filterCount, static_cast<size_t>(1));
  EXPECT_EQ(html.find(") url(#filter"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerNesting) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_nesting.pagx"));
  ASSERT_FALSE(html.empty());
  // Nested layers produce multiple <div> elements inside the root.
  EXPECT_NE(html.find("<div"), std::string::npos);
}

// =============================================================================
// Geometry elements
// =============================================================================

CLI_TEST(PAGXHtmlTest, GeometryRectangle) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_rectangle.pagx"));
  ASSERT_FALSE(html.empty());
  // CSS path: div with border-radius
  EXPECT_NE(html.find("border-radius"), std::string::npos);
  // Check center-point conversion: center=50,50 size=80,60 → left=10px, top=20px
  EXPECT_NE(html.find("width: 80px"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, RectangleStrokeRoundnessIsClampedToBounds) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="240" height="60">
  <Layer width="224" height="36">
    <Rectangle position="112,18" size="224,36" roundness="80"/>
    <Fill color="#FFFFFF"/>
    <Stroke color="#EDEDED" width="2" align="inside" placement="foreground"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("rx=\"18\""), std::string::npos);
  EXPECT_NE(html.find("ry=\"18\""), std::string::npos);
  EXPECT_EQ(html.find("rx=\"80\""), std::string::npos);
  EXPECT_EQ(html.find("ry=\"80\""), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, GeometryEllipse) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_ellipse.pagx"));
  ASSERT_FALSE(html.empty());
  // Accept both class-rule and inlined forms.
  EXPECT_TRUE(html.find("border-radius: 50%") != std::string::npos ||
              html.find("border-radius:50%") != std::string::npos);
}

CLI_TEST(PAGXHtmlTest, GeometryPolystar) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_polystar.pagx"));
  ASSERT_FALSE(html.empty());
  // Polystar always uses SVG path
  EXPECT_NE(html.find("<path"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, GeometryPath) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_path.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("<path"), std::string::npos);
}

// =============================================================================
// Color sources
// =============================================================================

CLI_TEST(PAGXHtmlTest, ColorFormats) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_formats.pagx"));
  ASSERT_FALSE(html.empty());
  // Should contain hex color values
  EXPECT_NE(html.find("#3B82F6"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorLinearGradient) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_linear_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("linear-gradient"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorRadialGradient) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_radial_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  // RadialGradient is emitted as CSS `radial-gradient()` on square geometries and as SVG
  // `<radialGradient>` when fitsToGeometry=true with a non-square bounding box (CSS gradients
  // cannot reproduce tgfx's objectBoundingBox non-uniform scale). Either path is valid — the
  // test only asserts that the gradient is exported, not which form it takes.
  EXPECT_TRUE(html.find("radial-gradient") != std::string::npos ||
              html.find("<radialGradient") != std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorConicGradient) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_conic_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  // ConicGradient now always rasterizes to a PNG: the new HTMLExporter contract requires a
  // resource directory, so the writer takes the rasterization path that produces stable pixels
  // across browsers (in particular headless Chromium, where SVG pattern + foreignObject
  // delivery of CSS conic-gradient is unreliable). The PNG is referenced via SVG <image>
  // (not HTML <img>) because the gradient lives inside an SVG <foreignObject>-free path that
  // pairs the rasterized image with a clipPath built from the geometry.
  EXPECT_NE(html.find("<image"), std::string::npos);
  EXPECT_NE(html.find("tmp-assets/cgc"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorDiamondGradient) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_diamond_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  // DiamondGradient rasterizes to a PNG because CSS has no native diamond shape; the HTML
  // references that PNG through an <img> tag. URL prefix is derived from the resource
  // directory's basename ("tmp-assets/"), and the id follows the "dgc" naming scheme.
  EXPECT_NE(html.find("<img"), std::string::npos);
  EXPECT_NE(html.find("tmp-assets/dgc"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorImagePattern) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_image_pattern.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("background-image"), std::string::npos);
}

// =============================================================================
// Painters
// =============================================================================

CLI_TEST(PAGXHtmlTest, PainterFill) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/painter_fill.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("background"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, PainterStroke) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/painter_stroke.pagx"));
  ASSERT_FALSE(html.empty());
  // Stroke uses SVG
  EXPECT_NE(html.find("stroke"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, PainterMultiple) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/painter_multiple.pagx"));
  ASSERT_FALSE(html.empty());
  // Multiple painters generate overlay elements
}

CLI_TEST(PAGXHtmlTest, PainterAccumulatedGeometry) {
  auto html = LoadAndConvert(
      ProjectPath::Absolute("resources/pagx_to_html/painter_accumulated_geometry.pagx"));
  ASSERT_FALSE(html.empty());
}

// =============================================================================
// Shape modifiers
// =============================================================================

CLI_TEST(PAGXHtmlTest, ModifierTrimPath) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/modifier_trim_path.pagx"));
  ASSERT_FALSE(html.empty());
  // TrimPath forces SVG rendering
  EXPECT_NE(html.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ModifierRoundCorner) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/modifier_round_corner.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("<path"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ModifierMergePath) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/modifier_merge_path.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("<path"), std::string::npos);
}

// =============================================================================
// Text system
// =============================================================================

CLI_TEST(PAGXHtmlTest, TextBasic) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_basic.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("Hello World"), std::string::npos);
  EXPECT_NE(html.find("font-family"), std::string::npos);
  EXPECT_NE(html.find("font-size"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, TextBox) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_box.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("text-align"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, TextModifier) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_modifier.pagx"));
  ASSERT_FALSE(html.empty());
}

CLI_TEST(PAGXHtmlTest, TextPath) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_path.pagx"));
  ASSERT_FALSE(html.empty());
  // Verify circular path text renders (text_path_advanced has reversed circular path)
  auto htmlAdv =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_path_advanced.pagx"));
  ASSERT_FALSE(htmlAdv.empty());
  // Text path colors should be present in the output
  EXPECT_NE(htmlAdv.find("#3B82F6"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ShapeGlyphRun) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/shape_glyph_run.pagx"));
  ASSERT_FALSE(html.empty());
  // All embedded font glyphs (vector and bitmap) render via WOFF2 @font-face with PUA characters.
  // No SVG <path> or <image> elements should be present for glyph rendering.
  EXPECT_NE(html.find("@font-face"), std::string::npos);
  EXPECT_NE(html.find("pagx-font-"), std::string::npos);
  EXPECT_EQ(html.find("<path"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, EmbeddedVectorFontNormalizesLowUnitsPerEm) {
  const std::string xml = R"(
<pagx width="64" height="32">
  <Resources>
    <Font id="bad" unitsPerEm="1">
      <Glyph advance="1" path="M 0,0 L 1,0 L 1,-1 L 0,-1 Z"/>
    </Font>
  </Resources>
  <Layer width="64" height="32">
    <Text>
      <GlyphRun font="@bad" fontSize="16" glyphs="1" x="8" y="24"/>
    </Text>
    <Fill color="#111111"/>
  </Layer>
</pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  const pagx::Font* font = nullptr;
  for (const auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Font) {
      font = static_cast<const pagx::Font*>(node.get());
      break;
    }
  }
  ASSERT_NE(font, nullptr);
  auto fontResult = pagx::BuildWoff2FromFont(font, "f0");
  ASSERT_FALSE(fontResult.woff2Data.empty());
  EXPECT_EQ(fontResult.unitsPerEm, static_cast<uint16_t>(16));
  EXPECT_NEAR(fontResult.designScale, 16.0f, 0.001f);

  auto tmpAssets = ProjectPath::Absolute("test/out/PAGXHtmlTest/low-upem-assets");
  auto html = pagx::HTMLExporter::ToHTML(*doc, tmpAssets, pagx::HTMLOutputMode::Fragment);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("@font-face"), std::string::npos);
  EXPECT_NE(html.find("pagx-font-"), std::string::npos);
  EXPECT_NE(html.find("font_f0.woff2"), std::string::npos);
  EXPECT_NE(html.find("\xEE\x80\x80"), std::string::npos);
  auto fontPath = tmpAssets + "/fonts/font_f0.woff2";
  ASSERT_TRUE(std::filesystem::exists(fontPath));
  EXPECT_GT(std::filesystem::file_size(fontPath), static_cast<uintmax_t>(0));
}

CLI_TEST(PAGXHtmlTest, EmbeddedVectorFontUsesPathBoundsForTinyAdvance) {
  const std::string xml = R"(
<pagx width="120" height="40">
  <Resources>
    <Font id="tiny" unitsPerEm="1">
      <Glyph advance="0.011236" path="M 0,0 L 1,0 L 1,-1 L 0,-1 Z"/>
    </Font>
  </Resources>
  <Layer width="120" height="40">
    <Text>
      <GlyphRun font="@tiny" fontSize="89" glyphs="1" x="8" y="24"/>
    </Text>
    <Fill color="#111111"/>
  </Layer>
</pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  const pagx::Font* font = nullptr;
  for (const auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Font) {
      font = static_cast<const pagx::Font*>(node.get());
      break;
    }
  }
  ASSERT_NE(font, nullptr);
  auto fontResult = pagx::BuildWoff2FromFont(font, "f0");
  ASSERT_FALSE(fontResult.woff2Data.empty());
  EXPECT_EQ(fontResult.unitsPerEm, static_cast<uint16_t>(16));
  EXPECT_GT(fontResult.minVisibleGlyphAdvance, static_cast<uint16_t>(0));
}

CLI_TEST(PAGXHtmlTest, EmbeddedVectorFontSupportsCustomCFFCharsetStrings) {
  std::ostringstream glyphs;
  std::ostringstream runGlyphs;
  for (int i = 0; i < 391; i++) {
    glyphs << "      <Glyph advance=\"1\" path=\"M 0,0 L 1,0 L 1,-1 L 0,-1 Z\"/>\n";
    if (i > 0) {
      runGlyphs << ',';
    }
    runGlyphs << i + 1;
  }
  auto xml = std::string(R"(
<pagx width="64" height="32">
  <Resources>
    <Font id="large" unitsPerEm="1">
)") + glyphs.str() +
             R"(    </Font>
  </Resources>
  <Layer width="64" height="32">
    <Text>
      <GlyphRun font="@large" fontSize="16" glyphs=")" +
             runGlyphs.str() + R"(" x="8" y="24"/>
    </Text>
    <Fill color="#111111"/>
  </Layer>
</pagx>)";
  auto doc = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc, nullptr);
  const pagx::Font* font = nullptr;
  for (const auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Font) {
      font = static_cast<const pagx::Font*>(node.get());
      break;
    }
  }
  ASSERT_NE(font, nullptr);
  auto fontResult = pagx::BuildWoff2FromFont(font, "f0");
  ASSERT_FALSE(fontResult.woff2Data.empty());

  auto tmpAssets = ProjectPath::Absolute("test/out/PAGXHtmlTest/custom-charset-assets");
  auto html = pagx::HTMLExporter::ToHTML(*doc, tmpAssets, pagx::HTMLOutputMode::Fragment);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("@font-face"), std::string::npos);
  EXPECT_NE(html.find("font_f0.woff2"), std::string::npos);
  auto fontPath = tmpAssets + "/fonts/font_f0.woff2";
  ASSERT_TRUE(std::filesystem::exists(fontPath));
  EXPECT_GT(std::filesystem::file_size(fontPath), static_cast<uintmax_t>(0));
}

CLI_TEST(PAGXHtmlTest, RealTextWithGlyphRunUsesEmbeddedFont) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="160" height="48">
  <Resources>
    <Font id="shapes" unitsPerEm="1000">
      <Glyph advance="700" path="M 0,0 L 700,0 L 700,-700 L 0,-700 Z"/>
    </Font>
  </Resources>
  <Layer width="160" height="48">
    <Text text="搜索" fontFamily="PingFang SC" fontSize="16">
      <GlyphRun font="@shapes" fontSize="16" glyphs="1,1" x="10" y="24"
                xOffsets="0,18" bounds="0,0,32,22"/>
    </Text>
    <Fill color="#111111"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("@font-face"), std::string::npos);
  EXPECT_NE(html.find("pagx-font-"), std::string::npos);
  EXPECT_NE(html.find("\xEE\x80\x80"), std::string::npos);
  EXPECT_EQ(html.find("搜索"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, MixedGlyphRunFontsUseTheirOwnEmbeddedFonts) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="120" height="40">
  <Resources>
    <Font id="bullet" unitsPerEm="1000">
      <Glyph advance="500" path="M 0,0 L 500,0 L 500,-500 L 0,-500 Z"/>
    </Font>
    <Font id="text" unitsPerEm="1000">
      <Glyph advance="500" path="M 0,0 L 500,0 L 500,-500 L 0,-500 Z"/>
      <Glyph advance="500" path="M 0,0 L 500,0 L 500,-500 L 0,-500 Z"/>
    </Font>
  </Resources>
  <Layer width="120" height="40">
    <Text text="•AB" fontFamily="PingFang SC" fontSize="16">
      <GlyphRun font="@bullet" fontSize="16" glyphs="1" x="0" y="24" bounds="0,0,8,16"/>
      <GlyphRun font="@text" fontSize="16" glyphs="1,2" x="16" y="24"
                xOffsets="0,16" bounds="16,0,32,16"/>
    </Text>
    <Fill color="#111111"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  size_t bulletFontCount = 0;
  for (size_t pos = 0; (pos = html.find("pagx-font-f0", pos)) != std::string::npos; pos++) {
    bulletFontCount++;
  }
  size_t textFontCount = 0;
  for (size_t pos = 0; (pos = html.find("pagx-font-f1", pos)) != std::string::npos; pos++) {
    textFontCount++;
  }
  EXPECT_GT(bulletFontCount, static_cast<size_t>(1));
  EXPECT_GT(textFontCount, static_cast<size_t>(1));
  EXPECT_NE(html.find("\xEE\x80\x81"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, SingleGroupTextBoxUsesTextBoxLayout) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="120" height="80">
  <Resources>
    <Font id="shapes" unitsPerEm="1000">
      <Glyph advance="500" path="M 0,0 L 500,0 L 500,-500 L 0,-500 Z"/>
    </Font>
  </Resources>
  <Layer width="120" height="80">
    <Group>
      <Text text="abcdefgh" fontFamily="PingFang SC" fontSize="14">
        <GlyphRun font="@shapes" fontSize="14" glyphs="1,1,1,1" y="17"
                  xOffsets="0,14,28,42"/>
        <GlyphRun font="@shapes" fontSize="14" glyphs="1,1,1,1" y="37"
                  xOffsets="0,14,28,42"/>
      </Text>
      <Fill color="#111111"/>
    </Group>
    <TextBox width="56" lineHeight="20"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("word-wrap:break-word"), std::string::npos);
  EXPECT_NE(html.find("line-height:20px"), std::string::npos);
  EXPECT_EQ(html.find("line-height:1;white-space:pre\">abcdefgh"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, SingleAutoSizeGlyphRunTextKeepsGlyphPosition) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="96" height="32">
  <Resources>
    <Font id="shapes" unitsPerEm="1000">
      <Glyph advance="500" path="M 0,0 L 500,0 L 500,-500 L 0,-500 Z"/>
    </Font>
  </Resources>
  <Layer width="48" height="20" data-text-auto-resize="width-and-height">
    <Group>
      <Text text="智能模式" fontFamily="PingFang SC" fontSize="12">
        <GlyphRun font="@shapes" fontSize="12" glyphs="1,1,1,1" y="14.32"
                  xOffsets="0,12,24,36" bounds="0,0,48,20"/>
      </Text>
      <Fill color="#111111"/>
    </Group>
    <TextBox textAlign="center" paragraphAlign="middle" lineHeight="20" data-max-lines="1"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("\xEE\x80\x80"), std::string::npos);
  EXPECT_EQ(html.find("智能模式"), std::string::npos);
  EXPECT_NE(html.find("top:2.32px"), std::string::npos);
  EXPECT_NE(html.find("line-height:1"), std::string::npos);
  EXPECT_EQ(html.find("justify-content:center"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, RichTextNewlineGroupEmitsSingleBreak) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="240" height="80">
  <Layer width="240" height="80">
    <Group>
      <Text text="Title" fontFamily="PingFang SC" fontSize="14"/>
      <Fill color="#111111"/>
    </Group>
    <Group>
      <Text text="&#10;" fontFamily="PingFang SC" fontSize="14"/>
      <Fill color="#111111"/>
    </Group>
    <Group>
      <Text text="Option" fontFamily="PingFang SC" fontSize="14"/>
      <Fill color="#111111"/>
    </Group>
    <TextBox width="200" lineHeight="24"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());

  auto titlePos = html.find("Title");
  auto optionPos = html.find("Option");
  ASSERT_NE(titlePos, std::string::npos);
  ASSERT_NE(optionPos, std::string::npos);
  ASSERT_LT(titlePos, optionPos);
  auto firstBreak = html.find("<br>", titlePos);
  ASSERT_NE(firstBreak, std::string::npos);
  ASSERT_LT(firstBreak, optionPos);
  auto secondBreak = html.find("<br>", firstBreak + 4);
  EXPECT_TRUE(secondBreak == std::string::npos || secondBreak > optionPos);
}

CLI_TEST(PAGXHtmlTest, UniformGradientTextFallsBackToSolidColor) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="160" height="48">
  <Layer width="160" height="48">
    <Group>
      <Text text="Done" fontFamily="PingFang SC" fontSize="14"/>
      <Fill>
        <LinearGradient matrix="2,0,0,2,0,0">
          <ColorStop offset="0" color="#999999"/>
          <ColorStop offset="1" color="#999999"/>
        </LinearGradient>
      </Fill>
    </Group>
    <TextBox lineHeight="24"/>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("Done"), std::string::npos);
  EXPECT_NE(html.find("#999999"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, TextRich) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_rich.pagx"));
  ASSERT_FALSE(html.empty());
}

// =============================================================================
// Repeater
// =============================================================================

CLI_TEST(PAGXHtmlTest, Repeater) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/repeater.pagx"));
  ASSERT_FALSE(html.empty());
}

// =============================================================================
// Group
// =============================================================================

CLI_TEST(PAGXHtmlTest, Group) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/group.pagx"));
  ASSERT_FALSE(html.empty());
  // Shape-only Groups (no Text child) are flattened: their transform is baked into the SVG
  // path data via TransformPathDataToSVG. Verify expected fill colors are present.
  EXPECT_NE(html.find("#3B82F6"), std::string::npos) << "Rotate+Scale fill color missing";
  // Scope Isolation: outer Fill(green) must only paint the outer Rectangle, not the Group ellipse.
  EXPECT_NE(html.find("#10B981"), std::string::npos) << "Scope Isolation outer fill missing";
  // Propagation: Group geometry propagates upward so the outer Fill can paint it.
  EXPECT_NE(html.find("#F59E0B"), std::string::npos) << "Propagation fill missing";
}

// =============================================================================
// Clipping and masking
// =============================================================================

CLI_TEST(PAGXHtmlTest, ClipAndMask) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/clip_and_mask.pagx"));
  ASSERT_FALSE(html.empty());
  // scrollRect → clip-path or clip
  // mask → CSS mask or SVG mask/clipPath
  // Mask SVG inside data: URLs must include xmlns — the browser parses it as a standalone SVG
  // document, not as part of the HTML5 DOM where xmlns would be redundant.
  EXPECT_NE(html.find("mask-image"), std::string::npos) << "clip_and_mask should use mask-image";
  EXPECT_NE(html.find("xmlns="), std::string::npos)
      << "mask-image data URL SVG must include xmlns for standalone parsing";
  // Inline SVG elements in HTML5 should NOT have xmlns.
  EXPECT_EQ(html.find("<svg xmlns="), std::string::npos)
      << "Inline SVG elements should not have xmlns in HTML5";
}

CLI_TEST(PAGXHtmlTest, ScrollRectLayoutKeepsChildrenInFlexFlow) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="320" height="120">
  <Layer id="clip" width="300" height="100" layout="horizontal" gap="12"
         alignment="stretch" scrollRect="5,7,300,100">
    <Layer id="first" width="80" height="100">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#FF0000"/>
    </Layer>
    <Layer id="second" width="90" height="100">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#00FF00"/>
    </Layer>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());

  auto offsetStylePos = html.find("style=\"position:relative;left:-5px;top:-7px");
  ASSERT_NE(offsetStylePos, std::string::npos);
  auto offsetStyleEnd = html.find('"', offsetStylePos + 7);
  ASSERT_NE(offsetStyleEnd, std::string::npos);
  auto offsetStyle = html.substr(offsetStylePos, offsetStyleEnd - offsetStylePos);
  EXPECT_NE(offsetStyle.find("display:flex"), std::string::npos);
  EXPECT_NE(offsetStyle.find("flex-direction:row"), std::string::npos);
  EXPECT_NE(offsetStyle.find("gap:12px"), std::string::npos);

  auto firstPos = html.find("id=\"first\"", offsetStyleEnd);
  auto secondPos = html.find("id=\"second\"", offsetStyleEnd);
  ASSERT_NE(firstPos, std::string::npos);
  ASSERT_NE(secondPos, std::string::npos);
  EXPECT_LT(firstPos, secondPos);
}

CLI_TEST(PAGXHtmlTest, VerticalFlexTextBoxResolvedHeightDoesNotCollapse) {
  pagx::HTMLExportOptions options;
  options.extractStyleSheet = false;
  auto html = LoadXMLAndConvert(R"(
<pagx width="260" height="160">
  <Layer id="stack" width="220" layout="vertical" gap="8" alignment="start">
    <Layer id="title" width="220" height="20">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#EEEEEE"/>
    </Layer>
    <Layer id="description" width="220" flex="1" data-text-auto-resize="none">
      <Group>
        <Text text="Line one&#10;Line two" fontFamily="PingFang SC" fontSize="14"/>
        <Fill color="#111111"/>
      </Group>
      <TextBox width="220" height="48" lineHeight="24"/>
    </Layer>
    <Layer id="footer" width="220" height="20">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#DDDDDD"/>
    </Layer>
  </Layer>
</pagx>)",
                                options);
  ASSERT_FALSE(html.empty());

  auto idPos = html.find("id=\"description\"");
  ASSERT_NE(idPos, std::string::npos);
  auto tagStart = html.rfind('<', idPos);
  ASSERT_NE(tagStart, std::string::npos);
  auto tagEnd = html.find('>', idPos);
  ASSERT_NE(tagEnd, std::string::npos);
  auto tag = html.substr(tagStart, tagEnd - tagStart);
  EXPECT_NE(tag.find("height:48px"), std::string::npos);
  EXPECT_EQ(tag.find("flex:1"), std::string::npos);
}

// =============================================================================
// Layer styles
// =============================================================================

CLI_TEST(PAGXHtmlTest, StyleShadows) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/style_shadows.pagx"));
  ASSERT_FALSE(html.empty());
  // Drop shadow → drop-shadow / box-shadow / SVG filter
  // Inner shadow → box-shadow inset / SVG filter
  // Background blur → backdrop-filter
}

// =============================================================================
// Layer filters
// =============================================================================

CLI_TEST(PAGXHtmlTest, FilterAll) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/filter_all.pagx"));
  ASSERT_FALSE(html.empty());
  // Uniform blur → filter:blur()
  // Asymmetric blur → SVG feGaussianBlur
  // DropShadowFilter → filter:drop-shadow()
  // ColorMatrixFilter → SVG feColorMatrix
}

// =============================================================================
// Edge cases and resources
// =============================================================================

CLI_TEST(PAGXHtmlTest, EdgeDefaults) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/edge_defaults.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("data-pagx-version"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ResourceComposition) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/resource_composition.pagx"));
  ASSERT_FALSE(html.empty());
}

CLI_TEST(PAGXHtmlTest, ResourceForwardRef) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/resource_forward_ref.pagx"));
  ASSERT_FALSE(html.empty());
  // Forward reference resolved: should contain the referenced color
  EXPECT_NE(html.find("#8B5CF6"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, StrokeDashAdaptive) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/stroke_dash_adaptive.pagx"));
  ASSERT_FALSE(html.empty());
}

CLI_TEST(PAGXHtmlTest, LayerPassThrough) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_pass_through.pagx"));
  ASSERT_FALSE(html.empty());
}

// =============================================================================
// Export options
// =============================================================================

CLI_TEST(PAGXHtmlTest, FilterDedup_ShowcaseInfographic) {
  // showcase_infographic.pagx has 3 layers with identical DropShadowStyle parameters.
  // The signature-keyed filter cache should emit one <filter> and reuse it.
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/showcase_infographic.pagx"));
  ASSERT_FALSE(html.empty());
  size_t count = 0;
  for (size_t pos = 0; (pos = html.find("<filter ", pos)) != std::string::npos; pos++) {
    count++;
  }
  EXPECT_EQ(count, static_cast<size_t>(1))
      << "Expected exactly 1 <filter> node after dedup, got " << count;
}

CLI_TEST(PAGXHtmlTest, ExportToFile) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);

  auto outPath = ProjectPath::Absolute("test/out/PAGXHtmlTest/export_to_file.html");
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  auto result = pagx::HTMLExporter::ToFile(*doc, outPath);
  EXPECT_TRUE(result);
  EXPECT_TRUE(std::filesystem::exists(outPath));

  std::ifstream file(outPath);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("data-pagx-version"), std::string::npos);
}

// =============================================================================
// Screenshot comparison: HTML rendering vs PAGX native rendering
// =============================================================================

static bool NodeAvailable() {
  return std::system("node --version > /dev/null 2>&1") == 0;
}

// Emits a tasks.json describing every {html, png, width, height} capture and invokes
// `screenshot.js --batch` so all screenshots share a single Chromium process. macOS
// Chromium becomes unstable across repeated cold-starts (see
// `feedback_puppeteer_screenshot_hang.md`), which fails this test after just a few
// per-sample Puppeteer launches. Reusing one browser sidesteps that entirely.
static bool BatchCaptureHtmlScreenshots(
    const std::vector<std::tuple<std::string, std::string, int, int>>& tasks) {
  if (tasks.empty()) {
    return true;
  }
  auto scriptPath = ProjectPath::Absolute("test/screenshot.js");
  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  auto tasksPath = outDir + "/screenshot_tasks.json";
  {
    std::ofstream tasksFile(tasksPath, std::ios::binary);
    if (!tasksFile.is_open()) {
      return false;
    }
    tasksFile << "[";
    for (size_t i = 0; i < tasks.size(); i++) {
      if (i > 0) {
        tasksFile << ",";
      }
      const auto& [html, png, w, h] = tasks[i];
      tasksFile << "{\"html\":\"" << html << "\",\"png\":\"" << png << "\",\"width\":" << w
                << ",\"height\":" << h << ",\"scale\":1}";
    }
    tasksFile << "]";
  }
  auto cmd = "node " + scriptPath + " --batch " + tasksPath + " 2>&1";
  int rc = std::system(cmd.c_str());
  std::error_code ec;
  std::filesystem::remove(tasksPath, ec);
  return rc == 0;
}

CLI_TEST(PAGXHtmlTest, HtmlScreenshotCompare) {
  if (!NodeAvailable()) {
    GTEST_SKIP() << "Node.js not available, skipping HTML screenshot comparison";
  }

  auto files = GetHtmlTestFiles();
  ASSERT_FALSE(files.empty()) << "No .pagx files found in resources/pagx_to_html/";

  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);

  // Phase 1: export every sample's HTML and collect the screenshot task list.
  struct Entry {
    std::string baseName;
    std::string pngPath;
  };
  std::vector<Entry> entries;
  std::vector<std::tuple<std::string, std::string, int, int>> screenshotTasks;
  entries.reserve(files.size());
  screenshotTasks.reserve(files.size());
  for (const auto& filePath : files) {
    auto result = ExportSampleHtmlToFile(filePath, outDir);
    if (!result.success) {
      ADD_FAILURE() << "Failed to export HTML for " << filePath;
      continue;
    }
    if (result.width <= 0 || result.height <= 0) {
      continue;
    }
    auto baseName = std::filesystem::path(filePath).stem().string();
    auto pngPath = outDir + "/" + baseName + ".png";
    screenshotTasks.emplace_back(result.htmlPath, pngPath, result.width, result.height);
    entries.push_back({baseName, pngPath});
  }

  // Phase 2: take all screenshots in a single Chromium session.
  ASSERT_TRUE(BatchCaptureHtmlScreenshots(screenshotTasks)) << "Batch screenshot capture failed";

  // Phase 3: baseline-compare each produced PNG.
  for (const auto& entry : entries) {
    auto codec = tgfx::ImageCodec::MakeFrom(entry.pngPath);
    if (!codec) {
      ADD_FAILURE() << "Failed to load screenshot PNG: " << entry.baseName;
      continue;
    }

    tgfx::Bitmap bitmap(codec->width(), codec->height(), false, false);
    tgfx::Pixmap pixmap(bitmap);
    if (!codec->readPixels(pixmap.info(), pixmap.writablePixels())) {
      ADD_FAILURE() << "Failed to decode screenshot: " << entry.baseName;
      continue;
    }

    EXPECT_TRUE(Baseline::Compare(pixmap, "PAGXTest/html/" + entry.baseName)) << entry.baseName;
  }
}

}  // namespace pag
