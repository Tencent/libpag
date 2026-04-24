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
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "tgfx/core/ImageCodec.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"

namespace pag {

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

static std::string LoadAndConvert(const std::string& pagxPath,
                                  const pagx::HTMLExportOptions& options = {}) {
  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  if (doc == nullptr) {
    return "";
  }
  doc->applyLayout();
  return pagx::HTMLExporter::ToHTML(*doc, options);
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
  return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><style>body{margin:0;padding:0;"
         "background:transparent;width:" +
         std::to_string(width) + "px;height:" + std::to_string(height) +
         "px;overflow:hidden}</style></head>\n<body>\n" + fragment + "\n</body></html>";
}

// =============================================================================
// Batch test: ensure every .pagx file in resources/pagx_to_html/ converts successfully
// =============================================================================

CLI_TEST(PAGXHtmlTest, BatchConvertAll) {
  auto files = GetHtmlTestFiles();
  ASSERT_FALSE(files.empty()) << "No .pagx files found in resources/pagx_to_html/";
  for (const auto& filePath : files) {
    auto baseName = std::filesystem::path(filePath).stem().string();
    auto doc = pagx::PAGXImporter::FromFile(filePath);
    ASSERT_NE(doc, nullptr) << "Failed to load: " << baseName;
    doc->applyLayout();
    auto html = pagx::HTMLExporter::ToHTML(*doc);
    EXPECT_FALSE(html.empty()) << "Failed to convert: " << baseName;
    EXPECT_NE(html.find("data-pagx-version"), std::string::npos)
        << "Missing data-pagx-version attribute in: " << baseName;
    auto fullHtml =
        WrapHtmlDocument(html, static_cast<int>(doc->width), static_cast<int>(doc->height));
    SaveFile(fullHtml, "PAGXHtmlTest/" + baseName + ".html");
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
  EXPECT_NE(html.find("display: none"), std::string::npos);
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
  EXPECT_NE(html.find("transform-style: preserve-3d"), std::string::npos);
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

CLI_TEST(PAGXHtmlTest, GeometryEllipse) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_ellipse.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("border-radius: 50%"), std::string::npos);
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
  EXPECT_NE(html.find("radial-gradient"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorConicGradient) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_conic_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("conic-gradient"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ColorDiamondGradient) {
  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);
  pagx::HTMLExportOptions opts;
  opts.staticImgDir = outDir + "/static-img";
  opts.staticImgUrlPrefix = "static-img/";
  opts.staticImgNamePrefix = "color_diamond_gradient-";
  auto html = LoadAndConvert(
      ProjectPath::Absolute("resources/pagx_to_html/color_diamond_gradient.pagx"), opts);
  ASSERT_FALSE(html.empty());
  // DiamondGradient rasterizes to a PNG because CSS has no native diamond shape; the HTML
  // references that PNG through an <img> tag.
  EXPECT_NE(html.find("<img"), std::string::npos);
  EXPECT_NE(html.find("static-img/color_diamond_gradient-"), std::string::npos);
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

CLI_TEST(PAGXHtmlTest, TextGlyphRun) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/text_glyph_run.pagx"));
  ASSERT_FALSE(html.empty());
  // GlyphRun produces SVG paths per glyph
  EXPECT_NE(html.find("<path"), std::string::npos);
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
  EXPECT_NE(html.find("pagx-group"), std::string::npos);
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

CLI_TEST(PAGXHtmlTest, ExportOptions_Indent) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.indent = 4;
  auto html = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(html.empty());
  // With 4-space indent, first child should be indented by 4 spaces
  EXPECT_NE(html.find("    "), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ExportOptions_FormatPretty) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.format = pagx::HTMLFormat::Pretty;
  auto html = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(html.empty());
  // Pretty CSS: selector on its own line, declarations indented.
  EXPECT_NE(html.find(".root0 {\n"), std::string::npos);
  EXPECT_NE(html.find("\n  position: relative;\n"), std::string::npos);
  // Generated comment is still present in non-minify modes.
  EXPECT_NE(html.find("Generated by PAGX HTMLExporter"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, ExportOptions_FormatMinify) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions compactOpts = {};
  auto compact = pagx::HTMLExporter::ToHTML(*doc, compactOpts);

  pagx::HTMLExportOptions minifyOpts = {};
  minifyOpts.format = pagx::HTMLFormat::Minify;
  auto minified = pagx::HTMLExporter::ToHTML(*doc, minifyOpts);

  ASSERT_FALSE(minified.empty());
  // No generated comment in minify mode.
  EXPECT_EQ(minified.find("Generated by PAGX HTMLExporter"), std::string::npos);
  // No newlines anywhere in the minified output.
  EXPECT_EQ(minified.find('\n'), std::string::npos);
  // Style block is single-line with rules concatenated.
  EXPECT_NE(minified.find("<style>.root0{"), std::string::npos);
  // Minify output is strictly smaller than compact output.
  EXPECT_LT(minified.size(), compact.size());
}

CLI_TEST(PAGXHtmlTest, ExportOptions_FormatPrettyIsDefault) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions defaulted = {};
  auto defaultedHtml = pagx::HTMLExporter::ToHTML(*doc, defaulted);

  pagx::HTMLExportOptions explicitPretty = {};
  explicitPretty.format = pagx::HTMLFormat::Pretty;
  auto prettyHtml = pagx::HTMLExporter::ToHTML(*doc, explicitPretty);

  // Default and explicit Pretty must produce byte-identical output.
  EXPECT_EQ(defaultedHtml, prettyHtml);
}

CLI_TEST(PAGXHtmlTest, ExportToFile) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

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
// per-sample Puppeteer launches. Reusing one browser mirrors what
// PAGXTest.GenerateComparisonPage already does for the comparison page.
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
    auto baseName = std::filesystem::path(filePath).stem().string();

    auto doc = pagx::PAGXImporter::FromFile(filePath);
    if (!doc) {
      ADD_FAILURE() << "Failed to load: " << baseName;
      continue;
    }
    doc->applyLayout();

    int width = static_cast<int>(doc->width);
    int height = static_cast<int>(doc->height);
    if (width <= 0 || height <= 0) {
      continue;
    }

    auto htmlPath = outDir + "/" + baseName + ".html";
    pagx::HTMLExportOptions opts;
    // Rasterize Diamond/tiled-ImagePattern fills into PNG next to the HTML so that browser
    // screenshots pick up the exact pixels tgfx produced, instead of a WebGL approximation.
    opts.staticImgDir = outDir + "/static-img";
    opts.staticImgUrlPrefix = "static-img/";
    opts.staticImgNamePrefix = baseName + "-";
    auto html = pagx::HTMLExporter::ToHTML(*doc, opts);
    if (html.empty()) {
      ADD_FAILURE() << "Failed to export HTML: " << baseName;
      continue;
    }

    auto fullHtml = WrapHtmlDocument(html, width, height);
    {
      std::ofstream htmlFile(htmlPath, std::ios::binary);
      htmlFile.write(fullHtml.data(), static_cast<std::streamsize>(fullHtml.size()));
    }
    auto pngPath = outDir + "/" + baseName + ".png";
    screenshotTasks.emplace_back(htmlPath, pngPath, width, height);
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
