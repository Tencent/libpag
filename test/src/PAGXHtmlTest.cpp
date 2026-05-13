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
#include "pagx/FontConfig.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Typeface.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"

namespace pag {

// Returns a FontConfig that registers the repo-bundled Noto Sans SC Regular as a fallback
// typeface. This ensures that text shaping during HTML export (PAGXDocument::applyLayout) uses
// the same Regular font metrics as the tgfx native renderer, so overflow:hidden line-count
// decisions and other metric-dependent layout calculations agree between the two code paths
// for non-bold text. Bold text is intentionally not registered here: tgfx falls back to
// faux-bold synthesis (Regular advance with thickened outline) and the screenshot baseline
// captures whatever the browser produces with that HTML structure, accepting the visual
// divergence between faux-bold and a real Bold typeface.
static pagx::FontConfig MakeHtmlFontConfig() {
  pagx::FontConfig c;
  auto regularTypeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  if (regularTypeface) {
    c.registerTypeface(regularTypeface);
  }
  return c;
}

static std::string LoadAndConvert(const std::string& pagxPath,
                                  const pagx::HTMLExportOptions& options = {}) {
  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  if (doc == nullptr) {
    return "";
  }
  auto fontConfig = MakeHtmlFontConfig();
  doc->applyLayout(&fontConfig);
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
  // Every @font-face lists the bundled local font file first and the Google Fonts CDN URL as a
  // safety net. Browsers try sources left-to-right and silently fall back on load failure, so
  // we get: (a) fast deterministic rendering when running offline — no network round-trip — and
  // (b) graceful degradation to CDN when the local file is absent (e.g. the Bold face, which is
  // not bundled in the repo, always resolves against the CDN). Local paths resolve relative to
  // the HTML file itself, so BatchConvertAll must populate test/out/PAGXHtmlTest/fonts/ for the
  // first url() to succeed; see the fonts-copy step below.
  //
  // `font-synthesis:none` on body prevents Chromium from auto-synthesising bold/italic on
  // elements that don't ask for it. The exporter's fauxBold and fauxItalic paths emit
  // `font-weight:bold;font-synthesis-weight:auto` and `font-style:italic;font-synthesis-style:
  // auto` respectively, re-enabling synthesis only on the elements that carry the faux flag.
  std::string fontFace =
      // Noto Sans SC Regular (400) — local OTF bundled in resources/font, CDN fallback.
      "@font-face{font-family:'Noto Sans SC';font-weight:400;"
      "src:url('fonts/NotoSansSC-Regular.otf') format('opentype'),"
      "url('https://fonts.gstatic.com/s/notosanssc/v40/"
      "k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaG9_FnYw.ttf') format('truetype');"
      "font-display:block}"
      // Noto Sans SC Bold (700) — local TTF downloaded on demand by test/screenshot.js into
      // `test/out/PAGXHtmlTest/fonts/NotoSansSC-Bold.ttf` (md5 pinned to the CDN v40 asset),
      // with the CDN URL kept as a fallback. The local-first src lets the browser resolve
      // Bold without a network round-trip, which is what enables the fast capture path
      // (shared page + waitUntil:'load') in screenshot.js — on the slow path the CDN
      // fallback is still exercised (~1 s per capture), but runs remain correct.
      "@font-face{font-family:'Noto Sans SC';font-weight:bold;"
      "src:url('fonts/NotoSansSC-Bold.ttf') format('truetype'),"
      "url('https://fonts.gstatic.com/s/notosanssc/v40/"
      "k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaGzjCnYw.ttf') format('truetype');"
      "font-display:block}"
      // Noto Color Emoji — local TTF bundled, CDN fallback.
      "@font-face{font-family:'Noto Color Emoji';"
      "src:url('fonts/NotoColorEmoji.ttf') format('truetype'),"
      "url('https://fonts.gstatic.com/s/notocoloremoji/v39/"
      "Yq6P-KqIXTD0t4D9z1ESnKM3-HpFab4.ttf') format('truetype');"
      "font-display:block}"
      // Noto Sans Hebrew Regular (400) — local TTF bundled, CDN fallback.
      "@font-face{font-family:'Noto Sans Hebrew';font-weight:400;"
      "src:url('fonts/NotoSansHebrew-Regular.ttf') format('truetype'),"
      "url('https://fonts.gstatic.com/s/notosanshebrew/v50/"
      "or3HQ7v33eiDljA1IufXTtVf7V6RvEEdhQlk0LlGxCyaeNKYZC0sqk3xXGiXd4qtog.ttf') "
      "format('truetype');"
      "font-display:block}";
  return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><style>" + fontFace +
         "body{margin:0;padding:0;background:transparent;font-synthesis:none;width:" +
         std::to_string(width) + "px;height:" + std::to_string(height) +
         "px;overflow:hidden}</style></head>\n<body>\n" + fragment + "\n</body></html>";
}

// Copies the bundled fonts referenced by WrapHtmlDocument into the destination directory so
// that the `url('fonts/<name>')` entries in each @font-face rule resolve locally when the HTML
// is opened in a browser. Bold is intentionally NOT in this list — test/screenshot.js
// downloads NotoSansSC-Bold.ttf into the same `fonts/` directory at startup (with md5
// verification against the v40 CDN asset). Keeping Bold out of the repo avoids carrying a
// 10 MB TTF for one font weight while still letting the wrapper's local-first src resolve
// Bold off-network at capture time.
static void CopyBundledFontsTo(const std::string& destDir) {
  struct FontFile {
    const char* repoPath;
    const char* destName;
  };
  static constexpr FontFile kBundled[] = {
      {"resources/font/NotoSansSC-Regular.otf", "NotoSansSC-Regular.otf"},
      {"resources/font/NotoColorEmoji.ttf", "NotoColorEmoji.ttf"},
      {"resources/font/NotoSansHebrew-Regular.ttf", "NotoSansHebrew-Regular.ttf"},
  };
  std::error_code ec;
  std::filesystem::create_directories(destDir, ec);
  if (ec) {
    return;
  }
  for (const auto& font : kBundled) {
    auto src = ProjectPath::Absolute(font.repoPath);
    if (!std::filesystem::exists(src)) {
      continue;
    }
    auto dest = destDir + "/" + font.destName;
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
  }
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
//
// The caller owns the FontConfig (registering fonts is expensive) and is responsible for
// invoking CopyBundledFontsTo(outDir + "/fonts") before calling this so the emitted
// @font-face rules can resolve their local `url('fonts/...')` entries in the browser.
// Failure does not abort the caller: the returned ExportedSample has success=false and a
// std::cerr log line; callers decide whether that is a FAIL-the-test or a skip-this-sample
// event based on their severity policy.
static ExportedSample ExportSampleHtmlToFile(const std::string& pagxPath, const std::string& outDir,
                                             const pagx::FontConfig& fontConfig) {
  ExportedSample result;
  auto baseName = std::filesystem::path(pagxPath).stem().string();
  result.htmlPath = outDir + "/" + baseName + ".html";

  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  if (!doc) {
    std::cerr << "ExportSampleHtmlToFile: failed to load " << pagxPath << "\n";
    return result;
  }
  // Re-using the caller's fontConfig here keeps text metrics consistent with whatever
  // fallbacks + registered typefaces the test harness set up; without it, applyLayout
  // would fall back to system fonts and its metrics would disagree with the browser.
  doc->applyLayout(&fontConfig);

  result.width = static_cast<int>(doc->width);
  result.height = static_cast<int>(doc->height);

  pagx::HTMLExportOptions opts;
  opts.staticImgDir = outDir + "/static-img";
  opts.staticImgUrlPrefix = "static-img/";
  opts.staticImgNamePrefix = baseName + "-";
  auto fragment = pagx::HTMLExporter::ToHTML(*doc, opts);
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
  // Populate test/out/PAGXHtmlTest/fonts/ so the local `url('fonts/...')` entries in the
  // WrapHtmlDocument @font-face rules can resolve when the emitted HTML is opened in a
  // browser. The wrapper's second source (Google Fonts CDN) only kicks in when the local
  // copy is missing — without this step, the primary source would 404 on every sample and
  // the browser would fall back to CDN, defeating the "render offline, deterministic across
  // machines" guarantee the multi-source wrapper is designed to provide.
  auto outDir = ProjectPath::Absolute("test/out/PAGXHtmlTest");
  std::filesystem::create_directories(outDir);
  auto fontConfig = MakeHtmlFontConfig();
  CopyBundledFontsTo(outDir + "/fonts");
  for (const auto& filePath : files) {
    auto result = ExportSampleHtmlToFile(filePath, outDir, fontConfig);
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

CLI_TEST(PAGXHtmlTest, ShapeGlyphRun) {
  auto html = LoadAndConvert(ProjectPath::Absolute("resources/pagx_to_html/shape_glyph_run.pagx"));
  ASSERT_FALSE(html.empty());
  // Embedded geometric shapes and bitmap images rendered via GlyphRun produce SVG paths and
  // image elements (not HTML text spans, since these nodes carry no text/fontFamily).
  EXPECT_NE(html.find("<path"), std::string::npos);
  EXPECT_NE(html.find("<image"), std::string::npos);
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

  // Populate the fonts/ directory so Puppeteer's Chromium can resolve the local `url('fonts/
  // ...')` entries the wrapper emits. Without this step the local source 404s and every
  // screenshot would silently fall back to the CDN — defeating determinism and adding network
  // latency to a test that is expected to be offline-stable.
  CopyBundledFontsTo(outDir + "/fonts");
  auto fontConfig = MakeHtmlFontConfig();

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
    auto result = ExportSampleHtmlToFile(filePath, outDir, fontConfig);
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
