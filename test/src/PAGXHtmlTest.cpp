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
  return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"></head>\n"
         "<body style=\"margin:0;padding:0;background:transparent;width:" +
         std::to_string(width) + "px;height:" + std::to_string(height) + "px;overflow:hidden\">\n" +
         fragment + "\n</body></html>";
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
    EXPECT_NE(html.find("pagx-root"), std::string::npos)
        << "Missing pagx-root class in: " << baseName;
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
  EXPECT_NE(html.find("pagx-root"), std::string::npos);
  EXPECT_NE(html.find("width:320px"), std::string::npos);
  EXPECT_NE(html.find("height:220px"), std::string::npos);
  EXPECT_NE(html.find("overflow:hidden"), std::string::npos);
  EXPECT_NE(html.find("position:relative"), std::string::npos);
  EXPECT_NE(html.find("data-pagx-version"), std::string::npos);
}

// =============================================================================
// Layer attributes
// =============================================================================

CLI_TEST(PAGXHtmlTest, LayerVisibility) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_visibility.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("display:none"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerAlpha) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_alpha.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("opacity:0.3"), std::string::npos);
  EXPECT_NE(html.find("opacity:0.7"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformXY) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_xy.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("translate(40px,50px)"), std::string::npos);
  EXPECT_NE(html.find("translate(130px,50px)"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformMatrix) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_matrix.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("matrix("), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerTransformMatrix3D) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_transform_matrix3d.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("matrix3d("), std::string::npos);
  EXPECT_NE(html.find("transform-style:preserve-3d"), std::string::npos);
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
  EXPECT_NE(html.find("mix-blend-mode:multiply"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:screen"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:overlay"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:darken"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:lighten"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:color-dodge"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:color-burn"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:hard-light"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:soft-light"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:difference"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:exclusion"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:hue"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:saturation"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:luminosity"), std::string::npos);
  EXPECT_NE(html.find("mix-blend-mode:plus-lighter"), std::string::npos);
  // plusDarker has no CSS equivalent — should have some fallback
}

CLI_TEST(PAGXHtmlTest, LayerGroupOpacity) {
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_group_opacity.pagx"));
  ASSERT_FALSE(html.empty());
  // groupOpacity=true: opacity on the parent div
  // groupOpacity=false: opacity distributed to children
  EXPECT_NE(html.find("opacity:0.5"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, LayerNesting) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/layer_nesting.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("pagx-layer"), std::string::npos);
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
  EXPECT_NE(html.find("width:80px"), std::string::npos);
}

CLI_TEST(PAGXHtmlTest, GeometryEllipse) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/geometry_ellipse.pagx"));
  ASSERT_FALSE(html.empty());
  EXPECT_NE(html.find("border-radius:50%"), std::string::npos);
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
  auto html =
      LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/color_diamond_gradient.pagx"));
  ASSERT_FALSE(html.empty());
  // DiamondGradient uses WebGL2 canvas rendering
  EXPECT_NE(html.find("<canvas"), std::string::npos);
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
  EXPECT_NE(html.find("pagx-root"), std::string::npos);
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
  EXPECT_NE(content.find("pagx-root"), std::string::npos);
}

// =============================================================================
// React JSX output
// =============================================================================

CLI_TEST(PAGXHtmlTest, ReactJSXOutput) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.framework = pagx::HTMLFramework::React;
  opts.componentName = "TestComponent";
  auto jsx = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(jsx.empty());

  EXPECT_NE(jsx.find("export default function TestComponent()"), std::string::npos)
      << "React output should have export default function";
  EXPECT_NE(jsx.find("return ("), std::string::npos) << "React output should have return statement";
  EXPECT_NE(jsx.find("className=\"pagx-root\""), std::string::npos)
      << "React output should use className instead of class";
  EXPECT_NE(jsx.find("style={"), std::string::npos)
      << "React output should have style object syntax";
  EXPECT_EQ(jsx.find("class=\""), std::string::npos)
      << "React output should not have class attribute";
  EXPECT_EQ(jsx.find("style=\""), std::string::npos)
      << "React output should not have style string attribute";

  SaveFile(jsx, "PAGXHtmlTest/react_output.jsx");
}

CLI_TEST(PAGXHtmlTest, ReactJSXStyleConversion) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/layer_alpha.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.framework = pagx::HTMLFramework::React;
  auto jsx = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(jsx.empty());

  EXPECT_NE(jsx.find("opacity: 0.3"), std::string::npos)
      << "Numeric values should not be quoted in React style object";

  SaveFile(jsx, "PAGXHtmlTest/react_style_conversion.jsx");
}

// =============================================================================
// Vue SFC output
// =============================================================================

CLI_TEST(PAGXHtmlTest, VueSFCOutput) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/root_document.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.framework = pagx::HTMLFramework::Vue;
  auto vue = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(vue.empty());

  EXPECT_NE(vue.find("<template>"), std::string::npos) << "Vue output should have <template> tag";
  EXPECT_NE(vue.find("</template>"), std::string::npos)
      << "Vue output should have closing </template> tag";
  EXPECT_NE(vue.find("<script setup>"), std::string::npos)
      << "Vue output should have <script setup> tag";
  EXPECT_NE(vue.find("class=\"pagx-root\""), std::string::npos)
      << "Vue output should keep class attribute";
  EXPECT_NE(vue.find(":style=\"{"), std::string::npos)
      << "Vue output should have :style binding syntax";
  EXPECT_EQ(vue.find(" style=\""), std::string::npos)
      << "Vue output should not have static style attribute";

  SaveFile(vue, "PAGXHtmlTest/vue_output.vue");
}

CLI_TEST(PAGXHtmlTest, VueSFCStyleConversion) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/layer_alpha.pagx"));
  ASSERT_TRUE(doc != nullptr);
  doc->applyLayout();

  pagx::HTMLExportOptions opts = {};
  opts.framework = pagx::HTMLFramework::Vue;
  auto vue = pagx::HTMLExporter::ToHTML(*doc, opts);
  ASSERT_FALSE(vue.empty());

  EXPECT_NE(vue.find("opacity: 0.3"), std::string::npos)
      << "Numeric values should not be quoted in Vue style object";

  SaveFile(vue, "PAGXHtmlTest/vue_style_conversion.vue");
}

// =============================================================================
// Screenshot comparison: HTML rendering vs PAGX native rendering
// =============================================================================

static bool NodeAvailable() {
  return std::system("node --version > /dev/null 2>&1") == 0;
}

static bool TakeHtmlScreenshot(const std::string& htmlPath, const std::string& pngPath, int width,
                               int height) {
  auto scriptPath = ProjectPath::Absolute("test/screenshot.js");
  auto cmd = "node " + scriptPath + " " + htmlPath + " " + pngPath + " " + std::to_string(width) +
             " " + std::to_string(height) + " 2>&1";
  return std::system(cmd.c_str()) == 0;
}

CLI_TEST(PAGXHtmlTest, HtmlScreenshotCompare) {
  if (!NodeAvailable()) {
    GTEST_SKIP() << "Node.js not available, skipping HTML screenshot comparison";
  }

  auto files = GetHtmlTestFiles();
  ASSERT_FALSE(files.empty()) << "No .pagx files found in resources/pagx_to_html/";

  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);

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

    auto html = pagx::HTMLExporter::ToHTML(*doc);
    if (html.empty()) {
      ADD_FAILURE() << "Failed to export HTML: " << baseName;
      continue;
    }

    auto fullHtml = WrapHtmlDocument(html, width, height);
    auto htmlPath = outDir + "/" + baseName + ".html";
    {
      std::ofstream htmlFile(htmlPath, std::ios::binary);
      htmlFile.write(fullHtml.data(), static_cast<std::streamsize>(fullHtml.size()));
    }

    auto pngPath = outDir + "/" + baseName + ".png";
    if (!TakeHtmlScreenshot(htmlPath, pngPath, width, height)) {
      ADD_FAILURE() << "Screenshot failed: " << baseName;
      continue;
    }

    auto codec = tgfx::ImageCodec::MakeFrom(pngPath);
    if (!codec) {
      ADD_FAILURE() << "Failed to load screenshot PNG: " << baseName;
      continue;
    }

    tgfx::Bitmap bitmap(codec->width(), codec->height(), false, false);
    tgfx::Pixmap pixmap(bitmap);
    if (!codec->readPixels(pixmap.info(), pixmap.writablePixels())) {
      ADD_FAILURE() << "Failed to decode screenshot: " << baseName;
      continue;
    }

    EXPECT_TRUE(Baseline::Compare(pixmap, "PAGXTest/html/" + baseName)) << baseName;
  }
}

// =============================================================================
// Framework semantic consistency: verify Native/React/Vue produce equivalent output
// =============================================================================

CLI_TEST(PAGXHtmlTest, FrameworkSemanticConsistency) {
  std::vector<std::string> testFiles = {
      "root_document.pagx",         "layer_alpha.pagx",   "geometry_rectangle.pagx",
      "color_linear_gradient.pagx", "style_shadows.pagx", "text_modifier.pagx",
  };

  for (const auto& fileName : testFiles) {
    auto filePath = ProjectPath::Absolute("resources/pagx_to_html/" + fileName);
    auto baseName = std::filesystem::path(fileName).stem().string();

    auto doc = pagx::PAGXImporter::FromFile(filePath);
    ASSERT_TRUE(doc != nullptr) << "Failed to load: " << baseName;
    doc->applyLayout();

    // Generate all three formats
    auto nativeHtml = pagx::HTMLExporter::ToHTML(*doc);
    ASSERT_FALSE(nativeHtml.empty()) << baseName;

    pagx::HTMLExportOptions reactOpts = {};
    reactOpts.framework = pagx::HTMLFramework::React;
    auto reactJsx = pagx::HTMLExporter::ToHTML(*doc, reactOpts);
    ASSERT_FALSE(reactJsx.empty()) << baseName;

    pagx::HTMLExportOptions vueOpts = {};
    vueOpts.framework = pagx::HTMLFramework::Vue;
    auto vueSfc = pagx::HTMLExporter::ToHTML(*doc, vueOpts);
    ASSERT_FALSE(vueSfc.empty()) << baseName;

    // React: className used, no raw class= or style=
    EXPECT_EQ(reactJsx.find("class=\""), std::string::npos)
        << baseName << ": React should not have class=";
    EXPECT_NE(reactJsx.find("className=\""), std::string::npos)
        << baseName << ": React should use className";
    EXPECT_EQ(reactJsx.find("style=\""), std::string::npos)
        << baseName << ": React should not have style string";
    EXPECT_NE(reactJsx.find("style={{"), std::string::npos)
        << baseName << ": React should have style object";

    // Vue: class kept, style uses :style binding
    EXPECT_EQ(vueSfc.find("className=\""), std::string::npos)
        << baseName << ": Vue should not have className";
    EXPECT_EQ(vueSfc.find(" style=\""), std::string::npos)
        << baseName << ": Vue should not have static style";
    EXPECT_NE(vueSfc.find(":style=\"{"), std::string::npos)
        << baseName << ": Vue should have :style binding";

    // fontFamily not truncated by HTML entity (the critical bug we fixed)
    if (nativeHtml.find("font-family") != std::string::npos) {
      EXPECT_NE(reactJsx.find("fontFamily: \"'"), std::string::npos)
          << baseName << ": React fontFamily should have quoted font name";
      EXPECT_NE(vueSfc.find("fontFamily: '"), std::string::npos)
          << baseName << ": Vue fontFamily should have quoted font name";
      EXPECT_EQ(reactJsx.find("fontFamily: \"&#39"), std::string::npos)
          << baseName << ": React fontFamily must not contain HTML entities";
      EXPECT_EQ(vueSfc.find("fontFamily: '&#39"), std::string::npos)
          << baseName << ": Vue fontFamily must not contain HTML entities";
    }

    // Numeric values are unquoted
    if (nativeHtml.find("opacity:") != std::string::npos) {
      EXPECT_EQ(reactJsx.find("opacity: \""), std::string::npos)
          << baseName << ": React opacity should be unquoted number";
      EXPECT_EQ(vueSfc.find("opacity: '"), std::string::npos)
          << baseName << ": Vue opacity should be unquoted number";
    }

    // Vendor prefixes correctly camelCased
    if (nativeHtml.find("-webkit-backdrop-filter") != std::string::npos) {
      EXPECT_NE(reactJsx.find("WebkitBackdropFilter"), std::string::npos)
          << baseName << ": React should camelCase -webkit- prefix";
      EXPECT_NE(vueSfc.find("WebkitBackdropFilter"), std::string::npos)
          << baseName << ": Vue should camelCase -webkit- prefix";
    }

    // CSS functions survive conversion
    if (nativeHtml.find("linear-gradient") != std::string::npos) {
      EXPECT_NE(reactJsx.find("linear-gradient"), std::string::npos)
          << baseName << ": React should preserve linear-gradient";
      EXPECT_NE(vueSfc.find("linear-gradient"), std::string::npos)
          << baseName << ": Vue should preserve linear-gradient";
    }
    if (nativeHtml.find("drop-shadow") != std::string::npos) {
      EXPECT_NE(reactJsx.find("drop-shadow"), std::string::npos)
          << baseName << ": React should preserve drop-shadow";
      EXPECT_NE(vueSfc.find("drop-shadow"), std::string::npos)
          << baseName << ": Vue should preserve drop-shadow";
    }

    // SVG element count identical across all three formats
    size_t nativeSvg = 0, reactSvg = 0, vueSvg = 0;
    for (size_t p = 0; (p = nativeHtml.find("<svg ", p)) != std::string::npos; p++) {
      nativeSvg++;
    }
    for (size_t p = 0; (p = reactJsx.find("<svg ", p)) != std::string::npos; p++) {
      reactSvg++;
    }
    for (size_t p = 0; (p = vueSfc.find("<svg ", p)) != std::string::npos; p++) {
      vueSvg++;
    }
    EXPECT_EQ(nativeSvg, reactSvg) << baseName << ": SVG count Native vs React";
    EXPECT_EQ(nativeSvg, vueSvg) << baseName << ": SVG count Native vs Vue";
  }
}

}  // namespace pag
