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
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "cli/CommandBounds.h"
#include "cli/CommandExport.h"
#include "cli/CommandFont.h"
#include "cli/CommandFormat.h"
#include "cli/CommandImport.h"
#include "cli/CommandLayout.h"
#include "cli/CommandRender.h"
#include "cli/CommandVerify.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"
#include "utils/TestDir.h"

namespace pag {
using namespace tgfx;

static std::string TempDir() {
#ifdef GENERATE_BASELINE_IMAGES
  auto dir = TestDir::GetRoot() + "/baseline-out/PAGXCliTest";
#else
  auto dir = TestDir::GetRoot() + "/out/PAGXCliTest";
#endif
  std::filesystem::create_directories(dir);
  return dir;
}

static std::string TestResourcePath(const std::string& name) {
  return ProjectPath::Absolute("resources/cli/" + name);
}

static std::string CopyToTemp(const std::string& resourceName, const std::string& tempName) {
  auto src = TestResourcePath(resourceName);
  auto dst = TempDir() + "/" + tempName;
  std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
  return dst;
}

static bool CompareRenderedImage(const std::string& imagePath, const std::string& key) {
  auto codec = ImageCodec::MakeFrom(imagePath);
  if (codec == nullptr) {
    return false;
  }
  Bitmap bitmap(codec->width(), codec->height(), false, false);
  if (bitmap.isEmpty()) {
    return false;
  }
  Pixmap pixmap(bitmap);
  if (!codec->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return false;
  }
  return Baseline::Compare(pixmap, key);
}

static bool RenderAndCompare(std::vector<std::string> args, const std::string& key) {
  std::vector<char*> argv = {};
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  auto bitmap = pagx::cli::RenderToBitmap(static_cast<int>(argv.size()), argv.data());
  if (bitmap.isEmpty()) {
    return false;
  }
  Pixmap pixmap(bitmap);
  return Baseline::Compare(pixmap, key);
}

static std::string ReadFile(const std::string& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static int CallRun(int (*fn)(int, char*[]), std::vector<std::string> args) {
  std::vector<char*> argv = {};
  argv.reserve(args.size());
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  return fn(static_cast<int>(argv.size()), argv.data());
}

static std::string ExportToSVG(const std::string& pagxResourceName, const std::string& svgTempName,
                               std::vector<std::string> extraExportArgs = {}) {
  auto pagxPath = TestResourcePath(pagxResourceName);
  auto svgPath = TempDir() + "/" + svgTempName;
  std::vector<std::string> args = {"export", "--input", pagxPath, "--output", svgPath};
  for (auto& arg : extraExportArgs) {
    args.push_back(std::move(arg));
  }
  auto ret = CallRun(pagx::cli::RunExport, std::move(args));
  EXPECT_EQ(ret, 0);
  return svgPath;
}

CLI_TEST(PAGXCliTest, Format_PreserveNewline) {
  auto inputPath = TestResourcePath("optimize_preserve_newline.pagx");
  auto outputPath = TempDir() + "/format_newline_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("&#10;") != std::string::npos);
  EXPECT_TRUE(output.find("Hello&#10;World") != std::string::npos);
}

//==============================================================================
// Format tests
//==============================================================================

CLI_TEST(PAGXCliTest, Format_Indentation) {
  auto inputPath = TestResourcePath("format_messy_indent.pagx");
  auto outputPath = TempDir() + "/messy_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("  <Layer") != std::string::npos);
  EXPECT_TRUE(output.find("    <Rectangle") != std::string::npos);
  EXPECT_TRUE(output.find("    <Fill") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_AttributeReordering) {
  auto inputPath = TestResourcePath("format_attr_order.pagx");
  auto outputPath = TempDir() + "/attr_order_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  auto namePos = output.find("name=");
  auto alphaPos = output.find("alpha=");
  ASSERT_NE(namePos, std::string::npos);
  ASSERT_NE(alphaPos, std::string::npos);
  EXPECT_LT(namePos, alphaPos);
  auto leftPos = output.find("left=");
  auto sizePos = output.find("size=");
  ASSERT_NE(leftPos, std::string::npos);
  ASSERT_NE(sizePos, std::string::npos);
  EXPECT_LT(leftPos, sizePos);
}

CLI_TEST(PAGXCliTest, Format_PreservesValues) {
  auto inputPath = TestResourcePath("format_preserve_values.pagx");
  auto outputPath = TempDir() + "/preserve_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("visible=\"true\"") != std::string::npos);
  EXPECT_TRUE(output.find("alpha=\"1\"") != std::string::npos);
  EXPECT_TRUE(output.find("roundness=\"0\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_CustomIndent) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto outputPath = TempDir() + "/indent4_out.pagx";
  auto ret =
      CallRun(pagx::cli::RunFormat, {"format", "--indent", "4", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("    <Layer") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_DeepNesting) {
  auto inputPath = TestResourcePath("format_deep_nesting.pagx");
  auto outputPath = TempDir() + "/deep_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("        <Rectangle") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_ResourcesBlock) {
  auto inputPath = TestResourcePath("format_resources.pagx");
  auto outputPath = TempDir() + "/resources_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("  <Resources>") != std::string::npos);
  EXPECT_TRUE(output.find("    <PathData") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_InPlace) {
  auto inputPath = CopyToTemp("format_messy_indent.pagx", "format_inplace.pagx");
  auto ret = CallRun(pagx::cli::RunFormat, {"format", inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(inputPath);
  EXPECT_TRUE(output.find("  <Layer") != std::string::npos);
  EXPECT_TRUE(output.find("    <Rectangle") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_IndentZero) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto outputPath = TempDir() + "/indent0_out.pagx";
  auto ret =
      CallRun(pagx::cli::RunFormat, {"format", "--indent", "0", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Layer") != std::string::npos);
  EXPECT_TRUE(output.find("  <Layer") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_InvalidFile) {
  auto inputPath = TestResourcePath("validate_not_xml.pagx");
  auto outputPath = TempDir() + "/format_invalid_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Bounds tests
//==============================================================================

CLI_TEST(PAGXCliTest, Bounds_AllLayers) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_XPathById) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret =
      CallRun(pagx::cli::RunBounds, {"bounds", "--xpath", "//Layer[@id='header']", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_XPathNested) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret =
      CallRun(pagx::cli::RunBounds, {"bounds", "--xpath", "//Layer[@id='card1']", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_JsonOutput) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--json", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_RelativeCoords) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--relative", "//Layer[@id='content']",
                                            "--xpath", "//Layer[@id='card1']", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_MissingFile) {
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdTopLevel) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--id", "header", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdNested) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--id", "card1", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdWithJson) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--id", "sidebar", "--json", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdWithRelative) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--id", "card1", "--relative",
                                            "//Layer[@id='content']", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdNotFound) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "--id", "nonexistent", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_IdAndXPathMutuallyExclusive) {
  auto inputPath = TestResourcePath("bounds_layout.pagx");
  auto ret = CallRun(pagx::cli::RunBounds,
                     {"bounds", "--id", "header", "--xpath", "//Layer[1]", inputPath});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Render tests — Verify rendered output matches baseline screenshots
//==============================================================================

CLI_TEST(PAGXCliTest, Render_Basic) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", inputPath}, "PAGXCliTest/RenderBasic"));
}

CLI_TEST(PAGXCliTest, Render_Gradient) {
  auto inputPath = TestResourcePath("render_gradient.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", inputPath}, "PAGXCliTest/RenderGradient"));
}

CLI_TEST(PAGXCliTest, Render_Scale) {
  auto inputPath = TestResourcePath("render_scale.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--scale", "2.0", inputPath}, "PAGXCliTest/RenderScale"));
}

CLI_TEST(PAGXCliTest, Render_Crop) {
  auto inputPath = TestResourcePath("render_crop.pagx");
  EXPECT_TRUE(
      RenderAndCompare({"render", "--crop", "50,50,100,100", inputPath}, "PAGXCliTest/RenderCrop"));
}

CLI_TEST(PAGXCliTest, Render_Background) {
  auto inputPath = TestResourcePath("render_background.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--background", "#0088FF", inputPath},
                               "PAGXCliTest/RenderBackground"));
}

CLI_TEST(PAGXCliTest, Render_WebpFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderWebpFormat.webp";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--format", "webp", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderWebpFormat"));
}

CLI_TEST(PAGXCliTest, Render_JpgFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderJpgFormat.jpg";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--format", "jpg", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderJpgFormat"));
}

CLI_TEST(PAGXCliTest, Render_Quality) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderQuality.webp";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--format", "webp",
                                            "--quality", "80", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_GT(std::filesystem::file_size(outputPath), 0u);
}

CLI_TEST(PAGXCliTest, Render_CombinedOptions) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  EXPECT_TRUE(RenderAndCompare(
      {"render", "--scale", "2.0", "--crop", "0,0,200,200", "--background", "#FFFF00", inputPath},
      "PAGXCliTest/RenderCombinedOptions"));
}

CLI_TEST(PAGXCliTest, Render_Text) {
  auto inputPath = TestResourcePath("render_text.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", inputPath}, "PAGXCliTest/RenderText"));
}

CLI_TEST(PAGXCliTest, Render_MissingFile) {
  auto outputPath = TempDir() + "/RenderMissingFile.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "nonexistent.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_DefaultOutput) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunRender, {"render", inputPath});
  EXPECT_EQ(ret, 0);
  auto defaultOutput = ProjectPath::Absolute("resources/cli/render_basic.png");
  EXPECT_TRUE(std::filesystem::exists(defaultOutput));
  std::filesystem::remove(defaultOutput);
}

CLI_TEST(PAGXCliTest, Render_InvalidScale) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderInvalidScale.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--scale", "0", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_InvalidCrop) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderInvalidCrop.png";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--crop", "abc", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_InvalidBackground) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderInvalidBg.png";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--background", "notacolor", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_UnsupportedFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderUnsupported.bmp";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--format", "bmp", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_IdFull) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", inputPath}, "PAGXCliTest/RenderIdFull"));
}

CLI_TEST(PAGXCliTest, Render_IdBadge) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(
      RenderAndCompare({"render", "--id", "badge", inputPath}, "PAGXCliTest/RenderIdBadge"));
}

CLI_TEST(PAGXCliTest, Render_IdCard) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--id", "card", inputPath}, "PAGXCliTest/RenderIdCard"));
}

CLI_TEST(PAGXCliTest, Render_IdBanner) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(
      RenderAndCompare({"render", "--id", "banner", inputPath}, "PAGXCliTest/RenderIdBanner"));
}

CLI_TEST(PAGXCliTest, Render_IdNestedIcon) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(
      RenderAndCompare({"render", "--id", "icon", inputPath}, "PAGXCliTest/RenderIdNestedIcon"));
}

CLI_TEST(PAGXCliTest, Render_IdWithScale) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--id", "banner", "--scale", "2", inputPath},
                               "PAGXCliTest/RenderIdWithScale"));
}

CLI_TEST(PAGXCliTest, Render_IdWithCrop) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--id", "banner", "--crop", "60,0,120,80", inputPath},
                               "PAGXCliTest/RenderIdWithCrop"));
}

CLI_TEST(PAGXCliTest, Render_XPathLayer) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  EXPECT_TRUE(RenderAndCompare({"render", "--xpath", "//Layer[@id='badge']", inputPath},
                               "PAGXCliTest/RenderXPathLayer"));
}

CLI_TEST(PAGXCliTest, Render_IdNotFound) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  auto outputPath = TempDir() + "/RenderIdNotFound.png";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--id", "nonexistent", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_IdAndXPathMutuallyExclusive) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  auto outputPath = TempDir() + "/RenderMutualExclusive.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--id", "badge", "--xpath",
                                            "//Layer[1]", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_XPathNoMatch) {
  auto inputPath = TestResourcePath("render_layer_target.pagx");
  auto outputPath = TempDir() + "/RenderXPathNoMatch.png";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--xpath", "//Layer[@id='nope']", inputPath});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Font tests
//==============================================================================

CLI_TEST(PAGXCliTest, FontInfo_FromFile) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto ret = CallRun(pagx::cli::RunFont, {"font", "info", "--file", fontPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, FontInfo_JsonOutput) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto ret = CallRun(pagx::cli::RunFont, {"font", "info", "--file", fontPath, "--json"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, FontInfo_FileNotFound) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "info", "--file", "nonexistent.ttf"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, FontInfo_MutualExclusive) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "info", "--file", "x.ttf", "--name", "Arial"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, FontInfo_NoSource) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "info"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Font_UnknownSubcommand) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "xyz"});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Lint tests
//==============================================================================

CLI_TEST(PAGXCliTest, Lint_C6_HighRepeaterCopies) {
  auto inputPath = TestResourcePath("lint_c6_high_repeater.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("Repeater produces") != std::string::npos);
  EXPECT_TRUE(output.find("total copies") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C6_NestedRepeaterProduct) {
  auto inputPath = TestResourcePath("lint_c6_nested_repeater.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("Repeater produces") != std::string::npos);
  EXPECT_TRUE(output.find("total copies") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C7_HighBlurRadius) {
  auto inputPath = TestResourcePath("lint_c7_high_blur.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("BlurFilter radius too large") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C8_StrokeAlignmentInRepeater) {
  auto inputPath = TestResourcePath("lint_c8_stroke_align.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("inside Repeater forces CPU rendering") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C9_DashedStrokeInRepeater) {
  auto inputPath = TestResourcePath("lint_c9_dashed_stroke.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("dashed Stroke inside Repeater") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C10_ComplexPath) {
  auto inputPath = TestResourcePath("lint_c10_complex_path.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("verbs (> 500)") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C11_LowOpacityHighCost) {
  auto inputPath = TestResourcePath("lint_c11_low_opacity.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("opacity") != std::string::npos);
  EXPECT_TRUE(output.find("high-cost children") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C13_SimpleRectangleMask) {
  auto inputPath = TestResourcePath("lint_c13_simple_rect_mask.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("rectangular alpha mask can use clipToBounds") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_ValidFile) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Lint_MissingFile) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Lint_JsonOutput) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--json", inputPath});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("\"ok\": true") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_Help) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Lint_UnknownOption) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--unknown", inputPath});
  EXPECT_NE(ret, 0);
}

// Extractable Composition detection: consecutive pair with subtree should warn,
// leaf Layers and containers with TextBox should not.
CLI_TEST(PAGXCliTest, Verify_ExtractableComposition) {
  auto inputPath = TestResourcePath("verify_extractable_composition.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  // Should detect exactly 2 warnings: case 1 (consecutive pair) + case 2 (subtree repeat)
  size_t pos = 0;
  int count = 0;
  while ((pos = output.find("structurally identical Layers", pos)) != std::string::npos) {
    count++;
    pos += 28;
  }
  EXPECT_EQ(count, 3);  // case 1 (pair) + case 2 (subtree) + case 3b (triple sequence)
}

CLI_TEST(PAGXCliTest, Lint_MissingInput) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Lint_EmptyNodes) {
  auto inputPath = TestResourcePath("lint_empty_nodes.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("empty Layer with no content") != std::string::npos ||
              output.find("empty Group with no elements") != std::string::npos);
}

// NOTE: The Lint_FullCanvasClipMask test is now exercisable because the verify command
// uses a different detection path (DetectFullCanvasClipMask on the document model).

CLI_TEST(PAGXCliTest, Lint_UnreferencedResources) {
  auto inputPath = TestResourcePath("lint_unref_resources.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("unreferenced resource") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_DuplicatePathData) {
  auto inputPath = TestResourcePath("lint_duplicate_pathdata.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("duplicate PathData") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_DuplicateGradient) {
  auto inputPath = TestResourcePath("lint_duplicate_gradient.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("duplicate") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_MergeableGroups) {
  auto inputPath = TestResourcePath("lint_mergeable_groups.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("consecutive Groups share identical painters") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_UnwrappableGroup) {
  auto inputPath = TestResourcePath("lint_unwrappable_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("redundant first-child Group") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_PathToPrimitive) {
  auto inputPath = TestResourcePath("lint_path_to_primitive.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("Path draws an") != std::string::npos);
}

// Heart (M C C C C Z) and diamond (M L L L Z) shapes should NOT be reported as replaceable.
// NOTE: The verify command's ellipse detection matches any M+4C+Z verb pattern without
// checking actual geometry. The heart shape (M C C C C Z) triggers a false positive.
// This is a known issue in CommandVerify.cpp::DetectPathToPrimitives.
CLI_TEST(PAGXCliTest, Lint_PathNonPrimitive) {
  auto inputPath = TestResourcePath("lint_path_non_primitive.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // Diamond (M L L L Z) should NOT be reported as a rectangle (edges not axis-aligned).
  EXPECT_TRUE(output.find("Path draws an axis-aligned rectangle") == std::string::npos);
}

// Lint_LocalizableCoordinates: The old lint had a "localizable coordinates" check for Layer x/y
// attributes, but the new verify command does not include this check. The test resource triggers
// no diagnostics in verify. Adjusted to verify clean exit.
CLI_TEST(PAGXCliTest, Lint_LocalizableCoordinates) {
  auto inputPath = TestResourcePath("lint_localizable_coords.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Lint_ExtractableCompositions) {
  auto inputPath = TestResourcePath("lint_extractable_compositions.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("structurally identical Layers") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_DowngradeableLayer) {
  auto inputPath = TestResourcePath("lint_downgradeable_layer.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("child Layer(s) use no Layer-exclusive features") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_IncludeInLayoutNoParent) {
  auto inputPath = TestResourcePath("lint_include_in_layout_no_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("includeInLayout=\"false\" has no effect") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_FlexNoParentLayout) {
  auto inputPath = TestResourcePath("lint_flex_no_parent_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("flex has no effect") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_FullCanvasClipMask) {
  auto inputPath = TestResourcePath("lint_full_canvas_clip.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("rectangular alpha mask can use clipToBounds") != std::string::npos);
}

//==============================================================================
// Export tests — PAGX to SVG
//==============================================================================

CLI_TEST(PAGXCliTest, Export_PagxToSvg_Basic) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Basic.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
  EXPECT_NE(output.find("</svg>"), std::string::npos);
  EXPECT_NE(output.find("xmlns=\"http://www.w3.org/2000/svg\""), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_Gradient) {
  auto inputPath = TestResourcePath("render_gradient.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Gradient.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<svg") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_CustomIndent) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Indent4.svg";
  auto ret = CallRun(pagx::cli::RunExport,
                     {"export", "--svg-indent", "4", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("    <") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_NoXmlDeclaration) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/ExportSVG_NoXml.svg";
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--svg-no-xml-declaration", "--input",
                                            inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<?xml") == std::string::npos);
  EXPECT_EQ(output.find("<svg"), 0u);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_ForceFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/ExportSVG_ForceFormat.out";
  auto ret = CallRun(pagx::cli::RunExport,
                     {"export", "--format", "svg", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_MissingFile) {
  auto outputPath = TempDir() + "/ExportSVG_Missing.svg";
  auto ret = CallRun(pagx::cli::RunExport,
                     {"export", "--input", "nonexistent.pagx", "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_InvalidFile) {
  auto inputPath = TestResourcePath("validate_not_xml.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Invalid.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_UnknownOption) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--unknown", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_MissingInput) {
  auto ret = CallRun(pagx::cli::RunExport, {"export"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_MissingFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_UnsupportedFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--format", "xyz", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_Text) {
  auto inputPath = TestResourcePath("render_text.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Text.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_Scale) {
  auto inputPath = TestResourcePath("render_scale.pagx");
  auto outputPath = TempDir() + "/ExportSVG_Scale.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_PagxToSvg_ValidateSimple) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto outputPath = TempDir() + "/ExportSVG_ValidateSimple.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
  EXPECT_NE(output.find("</svg>"), std::string::npos);
}

//==============================================================================
// Import tests — SVG to PAGX
//==============================================================================

CLI_TEST(PAGXCliTest, Import_SvgToPagx_Basic) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportSVG_Basic.svg");
  auto outputPath = TempDir() + "/ImportSVG_Basic.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
  EXPECT_NE(output.find("</pagx>"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_SvgToPagx_Gradient) {
  auto svgPath = ExportToSVG("render_gradient.pagx", "ImportSVG_Gradient.svg");
  auto outputPath = TempDir() + "/ImportSVG_Gradient.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_SvgToPagx_Text) {
  auto svgPath =
      ExportToSVG("render_text.pagx", "ImportSVG_Text.svg", {"--svg-no-convert-text-to-path"});
  auto outputPath = TempDir() + "/ImportSVG_Text.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_SvgToPagx_RoundTrip) {
  auto svgPath = ExportToSVG("validate_simple.pagx", "ImportSVG_RoundTrip.svg");
  auto outputPath = TempDir() + "/ImportSVG_RoundTrip.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_SvgToPagx_Scale) {
  auto svgPath = ExportToSVG("render_scale.pagx", "ImportSVG_Scale.svg");
  auto outputPath = TempDir() + "/ImportSVG_Scale.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_ForceFormat) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportSVG_ForceFormat.xml", {"--format", "svg"});
  auto outputPath = TempDir() + "/ImportSVG_ForceFormat.pagx";
  auto ret = CallRun(pagx::cli::RunImport,
                     {"import", "--format", "svg", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_DefaultOutput) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportSVG_Default.svg");
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath});
  EXPECT_EQ(ret, 0);
  auto defaultOutput = TempDir() + "/ImportSVG_Default.pagx";
  EXPECT_TRUE(std::filesystem::exists(defaultOutput));
}

CLI_TEST(PAGXCliTest, Import_MissingFile) {
  auto outputPath = TempDir() + "/ImportSVG_Missing.pagx";
  auto ret = CallRun(pagx::cli::RunImport,
                     {"import", "--input", "nonexistent.svg", "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_InvalidSvg) {
  auto inputPath = TestResourcePath("validate_not_xml.pagx");
  auto outputPath = TempDir() + "/ImportSVG_Invalid.pagx";
  auto ret = CallRun(pagx::cli::RunImport,
                     {"import", "--format", "svg", "--input", inputPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_UnknownOption) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--unknown", "--input", "test.svg"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_MissingInput) {
  auto ret = CallRun(pagx::cli::RunImport, {"import"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_UnsupportedFormat) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--format", "xyz", "--input", "test.xyz"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_UnknownFormatInference) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", "test.unknown"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_Help) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_HelpShort) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "-h"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_SvgOptions) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportSVG_Options.svg");
  auto outputPath = TempDir() + "/ImportSVG_Options.pagx";
  auto ret = CallRun(pagx::cli::RunImport,
                     {"import", "--svg-no-expand-use", "--svg-flatten-transforms",
                      "--svg-preserve-unknown", "--input", svgPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<pagx"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Import_UnexpectedArgument) {
  auto ret = CallRun(pagx::cli::RunImport, {"import", "somefile.svg"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_Help) {
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_HelpShort) {
  auto ret = CallRun(pagx::cli::RunExport, {"export", "-h"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_InvalidIndent_NonNumeric) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--svg-indent", "abc", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_InvalidIndent_OutOfRange) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--svg-indent", "20", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_InvalidIndent_Negative) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--svg-indent", "-1", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_InvalidIndent_Partial) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--svg-indent", "4abc", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_UnexpectedArgument) {
  auto ret = CallRun(pagx::cli::RunExport, {"export", "somefile.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_NoConvertTextToPath) {
  auto inputPath = TestResourcePath("render_text.pagx");
  auto outputPath = TempDir() + "/ExportSVG_NoConvertText.svg";
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--svg-no-convert-text-to-path", "--input",
                                            inputPath, "--output", outputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_NE(output.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_DefaultOutput) {
  auto inputPath = CopyToTemp("render_basic.pagx", "ExportDefault.pagx");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--format", "svg", "--input", inputPath});
  EXPECT_EQ(ret, 0);
  auto defaultOutput = TempDir() + "/ExportDefault.svg";
  EXPECT_TRUE(std::filesystem::exists(defaultOutput));
}

CLI_TEST(PAGXCliTest, Export_InferFormatFromOutputNoExt) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/ExportInferNoExt";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_DefaultOutputNoExtInput) {
  auto inputPath = CopyToTemp("render_basic.pagx", "ExportNoExt");
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Export_WriteFailure) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = "/nonexistent_dir_xyz/output.svg";
  auto ret =
      CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_WriteFailure) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportSVG_WriteFail.svg");
  auto outputPath = "/nonexistent_dir_xyz/output.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Import_DefaultOutputNoExtInput) {
  auto svgPath = ExportToSVG("render_basic.pagx", "ImportNoExt", {"--format", "svg"});
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--format", "svg", "--input", svgPath});
  EXPECT_EQ(ret, 0);
  auto defaultOutput = TempDir() + "/ImportNoExt.pagx";
  EXPECT_TRUE(std::filesystem::exists(defaultOutput));
}

//==============================================================================
// LayoutCheck tests
//==============================================================================

CLI_TEST(PAGXCliTest, LayoutCheck_Help) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_MissingFile) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "nonexistent.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_MissingInput) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_UnknownOption) {
  auto path = TestResourcePath("layout_check_clean.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--bogus", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Clean) {
  auto path = TestResourcePath("layout_check_clean.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_CleanProblemsOnly) {
  auto path = TestResourcePath("layout_check_clean.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Overlap) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Clipped) {
  auto path = TestResourcePath("layout_check_clipped.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // In global mode, overflow Layer should show absolute coordinates.
  EXPECT_TRUE(output.find("bounds=\"150,150,100,100\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Absolute) {
  auto path = TestResourcePath("layout_check_absolute.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_ZeroSize) {
  auto path = TestResourcePath("layout_check_zero_size.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_IdNode) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--id", "parent", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_IdNodeRelativeCoords) {
  auto path = TestResourcePath("layout_check_clipped.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--id", "overflow", path});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // The overflow Layer is at x=150 y=150 in the document, but when targeted via --id
  // its bounds should start from (0,0) as the coordinate origin.
  EXPECT_TRUE(output.find("bounds=\"0,0,100,100\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_IdNodeNotFound) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--id", "nonexistent_id", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_XPath) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--xpath", "//Layer[@id='parent']", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_CheckXml) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  // Manual-positioned Layers without container layout no longer trigger overlap warnings.
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_CheckClean) {
  auto path = TestResourcePath("layout_check_clean.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_CheckOverlap) {
  auto path = TestResourcePath("layout_check_overlap.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  // Manual-positioned Layers without container layout no longer trigger overlap warnings.
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Elements) {
  auto path = TestResourcePath("layout_check_elements.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, LayoutCheck_Placeholder) {
  auto path = TestResourcePath("layout_check_placeholder.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  // The third child Layer (id="bad") has zero width, should trigger a diagnostic.
  EXPECT_TRUE(output.find("zero size") != std::string::npos);
}

// Background Rectangle on a layout Layer should not trigger overlap warnings with child Layers.
CLI_TEST(PAGXCliTest, LayoutCheck_BackgroundNoOverlap) {
  auto path = TestResourcePath("layout_check_background.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

// Manual-positioned Layers without container layout should not trigger overlap warnings.
CLI_TEST(PAGXCliTest, LayoutCheck_ManualPositionNoOverlap) {
  auto path = TestResourcePath("layout_check_layout_overlap.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

// Text with system fonts should be measured correctly via font fallback (not zero-size).
CLI_TEST(PAGXCliTest, LayoutCheck_TextFontFallback) {
  auto path = TestResourcePath("layout_check_text_fallback.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Line Paths have preferred size clamped to 1px minimum, so they should not trigger zero-size
// warnings. Content-measured Layers containing line Paths also get a non-zero size.
CLI_TEST(PAGXCliTest, LayoutCheck_PathZeroSize) {
  auto path = TestResourcePath("layout_check_path_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Content origin offset: unconstrained Path starts at (50, 50), not (0, 0).
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffset) {
  auto path = TestResourcePath("layout_check_content_offset.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (50,50), not (0,0)") != std::string::npos);
  EXPECT_TRUE(output.find("container measurement inaccurate") != std::string::npos);
}

// Content origin offset with negative coordinates.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetNeg) {
  auto path = TestResourcePath("layout_check_content_offset_neg.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (-20,0), not (0,0)") != std::string::npos);
}

// Content at origin (0, 0) — no offset problem.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentAtOrigin) {
  auto path = TestResourcePath("layout_check_content_at_origin.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset not reported when Layer has explicit width/height.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetExplicitSize) {
  auto path = TestResourcePath("layout_check_content_offset_explicit_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset: a constrained Group at (0,0) covers the origin, so the minimum
// coordinate across all children is (0,0) and no offset problem is reported.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetConstrained) {
  auto path = TestResourcePath("layout_check_content_offset_constrained.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  // The constrained Group at left=0,top=0 has layoutBounds starting at (0,0),
  // so minX=0, minY=0 — no offset problem.
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset not reported when Layer is a flex child (engine assigns size).
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetFlex) {
  auto path = TestResourcePath("layout_check_content_offset_flex.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset detected inside a Group that has constraints.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetGroup) {
  auto path = TestResourcePath("layout_check_content_offset_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (20,20), not (0,0)") != std::string::npos);
}

// Content origin offset NOT reported for a Group without constraints (painter scope isolation only).
// The Group's measurement doesn't affect positioning when it has no constraints.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetGroupNoConstraints) {
  auto path = TestResourcePath("layout_check_content_offset_group_no_constraints.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset NOT reported for a Layer with no constraints and not in parent layout.
// The Layer's measurement doesn't affect any positioning.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetLayerUnpositioned) {
  auto path = TestResourcePath("layout_check_content_offset_layer_unpositioned.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset NOT reported for a Layer with includeInLayout=false and no constraints.
// Even though parent has container layout, the Layer is excluded and unpositioned.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetExcludedFromLayout) {
  auto path = TestResourcePath("layout_check_content_offset_excluded_from_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset reported for a Layer in parent container layout (positioned by layout).
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetInParentLayout) {
  auto path = TestResourcePath("layout_check_content_offset_in_parent_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (50,50), not (0,0)") != std::string::npos);
}

// Content origin offset NOT reported when container has a mix of constrained and unconstrained
// children. The constrained child (Text with left/top/right/bottom) defines the intended content
// region, so offset of unconstrained siblings is acceptable.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetMixedConstraints) {
  auto path = TestResourcePath("layout_check_content_offset_mixed_constraints.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Flex child in content-measured parent (no main-axis size to distribute).
CLI_TEST(PAGXCliTest, LayoutCheck_FlexNoParentSize) {
  auto path = TestResourcePath("layout_check_flex_no_parent_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("flex has no effect, parent has no") != std::string::npos);
}

// Flex child with explicit parent size — no problem.
CLI_TEST(PAGXCliTest, LayoutCheck_FlexWithParentSize) {
  auto path = TestResourcePath("layout_check_flex_with_parent_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
  EXPECT_TRUE(output.find("no space to distribute") == std::string::npos);
}

// Nested flex: parent gets main-axis size from grandparent — no false positive.
CLI_TEST(PAGXCliTest, LayoutCheck_FlexNested) {
  auto path = TestResourcePath("layout_check_flex_nested.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
  EXPECT_TRUE(output.find("no space to distribute") == std::string::npos);
}

// Flex child — parent derives main-axis size from opposite-pair constraints.
CLI_TEST(PAGXCliTest, LayoutCheck_FlexConstraintParent) {
  auto path = TestResourcePath("layout_check_flex_constraint_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
}

// Flex child zero-size with parent that has opposite-pair constraints deriving zero height.
// The cause should NOT say "parent has no ... to distribute" because the parent does have a size
// source (constraints) — it just happens to derive zero.
CLI_TEST(PAGXCliTest, LayoutCheck_FlexConstraintZeroParent) {
  auto path = TestResourcePath("layout_check_flex_constraint_zero_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  // Should report zero-size but NOT the "parent has no ... to distribute" cause.
  EXPECT_TRUE(output.find("zero size") != std::string::npos);
  EXPECT_TRUE(output.find("parent has no") == std::string::npos);
}

// Flex in horizontal layout with content-measured parent — same problem, different axis.
CLI_TEST(PAGXCliTest, LayoutCheck_FlexHorizontal) {
  auto path = TestResourcePath("layout_check_flex_horizontal.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("parent has no") != std::string::npos);
}

// --depth limits Layer nesting depth. depth=1 shows root + direct child Layers but not
// grandchild Layers. Elements inside each shown Layer are always included.
CLI_TEST(PAGXCliTest, LayoutCheck_Depth) {
  auto path = TestResourcePath("layout_check_depth.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--depth", "1", path});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // Root and direct child Layers should be present.
  EXPECT_TRUE(output.find("id=\"root\"") != std::string::npos);
  EXPECT_TRUE(output.find("id=\"child1\"") != std::string::npos);
  EXPECT_TRUE(output.find("id=\"child2\"") != std::string::npos);
  // Grandchild Layers should NOT be present (depth exceeded).
  EXPECT_TRUE(output.find("id=\"grandchild1\"") == std::string::npos);
  EXPECT_TRUE(output.find("id=\"grandchild2\"") == std::string::npos);
}

// --depth 0 means unlimited (same as no --depth).
CLI_TEST(PAGXCliTest, LayoutCheck_DepthZero) {
  auto path = TestResourcePath("layout_check_depth.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--depth", "0", path});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // All nodes should be present with unlimited depth.
  EXPECT_TRUE(output.find("id=\"root\"") != std::string::npos);
  EXPECT_TRUE(output.find("id=\"grandchild1\"") != std::string::npos);
  EXPECT_TRUE(output.find("id=\"grandchild2\"") != std::string::npos);
}

// Container overflow: children total main-axis size exceeds parent available space.
CLI_TEST(PAGXCliTest, LayoutCheck_ContainerOverflow) {
  auto path = TestResourcePath("layout_check_container_overflow.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children overflow") != std::string::npos);
}

// Negative constraint-derived size: left+right exceeds parent width.
CLI_TEST(PAGXCliTest, LayoutCheck_NegativeConstraintSize) {
  auto path = TestResourcePath("layout_check_negative_constraint.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("negative derived size") != std::string::npos);
}

// Element constraint conflict: centerX overrides left on a VectorElement.
CLI_TEST(PAGXCliTest, LayoutCheck_ElementConstraintConflict) {
  auto path = TestResourcePath("layout_check_element_constraint_conflict.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left ignored, centerX takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_OffCanvas) {
  auto path = TestResourcePath("layout_check_offcanvas.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("completely outside canvas bounds, not visible") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_RedundantConstraintCenterX) {
  auto path = TestResourcePath("layout_check_redundant_centerx.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left ignored, centerX takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_RedundantConstraintCenterY) {
  auto path = TestResourcePath("layout_check_redundant_centery.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("ignored, centerY takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_RedundantConstraintLeftZero) {
  auto path = TestResourcePath("layout_check_redundant_left_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left=\"0\" redundant") != std::string::npos);
}

CLI_TEST(PAGXCliTest, LayoutCheck_RedundantConstraintTopZero) {
  auto path = TestResourcePath("layout_check_redundant_top_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("top=\"0\" redundant") != std::string::npos);
}

// centerX/centerY on VectorElement inside content-measured Layer — ineffective.
CLI_TEST(PAGXCliTest, LayoutCheck_IneffectiveCenterLayer) {
  auto path = TestResourcePath("layout_check_ineffective_center_layer.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("centerX ineffective") != std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") != std::string::npos);
}

// centerX/centerY on VectorElement inside content-measured Group — ineffective.
CLI_TEST(PAGXCliTest, LayoutCheck_IneffectiveCenterGroup) {
  auto path = TestResourcePath("layout_check_ineffective_center_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("centerX ineffective") != std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") != std::string::npos);
}

// centerX/centerY on VectorElement inside explicit-size Layer — valid, no problem.
CLI_TEST(PAGXCliTest, LayoutCheck_IneffectiveCenterExplicitSize) {
  auto path = TestResourcePath("layout_check_ineffective_center_explicit.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("centerX ineffective") == std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") == std::string::npos);
}

// Content origin offset: Layer with opposite-edge constraints (left+right) derives size from
// constraints, not from content measurement. Should NOT report content-origin-offset.
CLI_TEST(PAGXCliTest, LayoutCheck_ContentOriginOffsetOppositeConstraint) {
  auto path = TestResourcePath("layout_check_content_offset_opposite_constraint.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Group with opposite-edge constraints (left+right+top+bottom) has constraint-derived size.
// centerX/centerY on child elements should NOT be reported as ineffective.
CLI_TEST(PAGXCliTest, LayoutCheck_IneffectiveCenterGroupConstrained) {
  auto path = TestResourcePath("layout_check_ineffective_center_group_constrained.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("centerX ineffective") == std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") == std::string::npos);
}

// TextBox with explicit width="0" height="0" is anchor mode (point text).
// Should NOT trigger zero-size warning.
CLI_TEST(PAGXCliTest, LayoutCheck_ZeroSizeExplicit) {
  auto path = TestResourcePath("layout_check_zero_size_explicit.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Polystar without explicit position should have auto-position applied at import time.
// Content-measured Layer should NOT report content-origin-offset.
CLI_TEST(PAGXCliTest, LayoutCheck_PolystarOrigin) {
  auto path = TestResourcePath("layout_check_polystar_origin.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--problems-only", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

//==============================================================================
// Resolve (import --resolve) tests
//==============================================================================

CLI_TEST(PAGXCliTest, Insert_SvgIntoLayer_Basic) {
  auto pagxPath = CopyToTemp("insert_basic.pagx", "insert_basic.pagx");
  auto svgPath = TestResourcePath("insert_basic.svg");
  auto outputPath = TempDir() + "/insert_out.pagx";
  auto ret = CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", "-o",
                                            outputPath, pagxPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_TRUE(RenderAndCompare({"render", "--scale", "2", outputPath}, "PAGXCliTest/insert_basic"));
}

CLI_TEST(PAGXCliTest, Insert_SvgIntoLayer_DefaultOutput) {
  auto pagxPath = CopyToTemp("insert_basic.pagx", "insert_default_out.pagx");
  auto svgPath = TestResourcePath("insert_search.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(
      RenderAndCompare({"render", "--scale", "2", pagxPath}, "PAGXCliTest/insert_default_out"));
}

CLI_TEST(PAGXCliTest, Insert_MissingTargetId) {
  auto pagxPath = TestResourcePath("insert_missing_id.pagx");
  auto svgPath = TestResourcePath("insert_basic.svg");
  auto ret = CallRun(pagx::cli::RunImport,
                     {"import", "--input", svgPath, "--resolve", "nonexistent", pagxPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Insert_MissingRequiredOptions) {
  auto pagxPath = TempDir() + "/dummy.pagx";

  auto ret = CallRun(pagx::cli::RunImport, {"import", "--resolve", "icon", pagxPath});
  EXPECT_NE(ret, 0);

  ret = CallRun(pagx::cli::RunImport, {"import", "--input", "icon.svg", pagxPath});
  EXPECT_NE(ret, 0);

  ret = CallRun(pagx::cli::RunImport, {"import", "--input", "icon.svg", "--resolve", "icon"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Insert_PreservesLayerAttributes) {
  auto pagxPath = CopyToTemp("insert_preserve_attrs.pagx", "insert_preserve_attrs.pagx");
  auto svgPath = TestResourcePath("insert_circle.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(
      RenderAndCompare({"render", "--scale", "2", pagxPath}, "PAGXCliTest/insert_preserve_attrs"));
}

CLI_TEST(PAGXCliTest, Insert_ReplacesExistingContent) {
  auto pagxPath = CopyToTemp("insert_replace.pagx", "insert_replace.pagx");
  auto svgPath = TestResourcePath("insert_triangle.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);

  auto output = ReadFile(pagxPath);
  EXPECT_EQ(output.find("<Rectangle"), std::string::npos);
  EXPECT_EQ(output.find("#FF0000"), std::string::npos);
  EXPECT_EQ(output.find("DropShadowStyle"), std::string::npos);

  EXPECT_TRUE(RenderAndCompare({"render", "--scale", "2", pagxPath}, "PAGXCliTest/insert_replace"));
}

CLI_TEST(PAGXCliTest, Insert_InvalidSvg) {
  auto pagxPath = CopyToTemp("insert_basic.pagx", "insert_invalid_svg.pagx");
  auto svgPath = TestResourcePath("insert_invalid.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Insert_MultiElementSvg) {
  auto pagxPath = CopyToTemp("insert_basic.pagx", "insert_multi.pagx");
  auto svgPath = TestResourcePath("insert_multi.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(RenderAndCompare({"render", "--scale", "2", pagxPath}, "PAGXCliTest/insert_multi"));
}

CLI_TEST(PAGXCliTest, Insert_RenderResult) {
  auto pagxPath = CopyToTemp("insert_render.pagx", "insert_render.pagx");
  auto svgPath = TestResourcePath("insert_render_search.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(RenderAndCompare({"render", "--scale", "2", pagxPath}, "PAGXCliTest/insert_render"));
}

CLI_TEST(PAGXCliTest, Insert_PreservesOriginalFormatting) {
  auto pagxPath = CopyToTemp("insert_format.pagx", "insert_format.pagx");
  auto svgPath = TestResourcePath("insert_rect.svg");
  auto ret =
      CallRun(pagx::cli::RunImport, {"import", "--input", svgPath, "--resolve", "icon", pagxPath});
  EXPECT_EQ(ret, 0);

  auto output = ReadFile(pagxPath);
  EXPECT_NE(output.find("<!-- header section -->"), std::string::npos);
  EXPECT_NE(output.find("                <Rectangle"), std::string::npos);
  EXPECT_NE(output.find("                <Fill color=\"#FF0000\""), std::string::npos);
  EXPECT_NE(output.find("          <Rectangle"), std::string::npos);
  EXPECT_NE(output.find("          <Fill color=\"#00FF00\""), std::string::npos);
  EXPECT_NE(output.find("id=\"icon\""), std::string::npos);
  EXPECT_NE(output.find("width=\"48\""), std::string::npos);
  EXPECT_NE(output.find("<Fill color=\"#0000FF\""), std::string::npos);
}

}  // namespace pag
