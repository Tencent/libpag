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
#include "cli/CommandEmbed.h"
#include "cli/CommandExport.h"
#include "cli/CommandFont.h"
#include "cli/CommandFormat.h"
#include "cli/CommandImport.h"
#include "cli/CommandLayout.h"
#include "cli/CommandRender.h"
#include "cli/CommandResolve.h"
#include "cli/CommandVerify.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Image.h"
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

static std::string CopyResourceToTemp(const std::string& resourceRelPath,
                                      const std::string& tempName) {
  auto src = ProjectPath::Absolute(resourceRelPath);
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
  auto inputPath = TestResourcePath("format_preserve_newline.pagx");
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
  auto rectPos = output.find("<Rectangle");
  ASSERT_NE(rectPos, std::string::npos);
  auto leftPos = output.find("left=", rectPos);
  auto widthPos = output.find("width=", rectPos);
  ASSERT_NE(leftPos, std::string::npos);
  ASSERT_NE(widthPos, std::string::npos);
  EXPECT_LT(leftPos, widthPos);
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
  auto inputPath = TestResourcePath("verify_simple.pagx");
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
  auto inputPath = TestResourcePath("verify_simple.pagx");
  auto outputPath = TempDir() + "/indent0_out.pagx";
  auto ret =
      CallRun(pagx::cli::RunFormat, {"format", "--indent", "0", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Layer") != std::string::npos);
  EXPECT_TRUE(output.find("  <Layer") == std::string::npos);
}

// Formatting preserves XML comments, blank lines, CDATA sections, and does not insert an XML
// declaration when the original file has none.
CLI_TEST(PAGXCliTest, Format_PreserveCommentsAndBlankLines) {
  auto inputPath = TestResourcePath("format_comments.pagx");
  auto outputPath = TempDir() + "/format_comments_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<!-- Top-level comment describing the file -->") != std::string::npos);
  EXPECT_TRUE(output.find("<!-- Section A -->") != std::string::npos);
  EXPECT_TRUE(output.find("<!-- Section B -->") != std::string::npos);
  EXPECT_TRUE(output.find("</Layer>\n\n  <!-- Section B -->") != std::string::npos);
  EXPECT_TRUE(output.find("<![CDATA[Hello & <World>]]>") != std::string::npos);
  EXPECT_TRUE(output.find("<?xml") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_InvalidFile) {
  auto inputPath = TestResourcePath("verify_not_xml.pagx");
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

CLI_TEST(PAGXCliTest, Font_FromFile) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--file", fontPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Font_JsonOutput) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--file", fontPath, "--json"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Font_FileNotFound) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--file", "nonexistent.ttf"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Font_MutualExclusive) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--file", "x.ttf", "--name", "Arial"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Font_NoSource) {
  auto ret = CallRun(pagx::cli::RunFont, {"font"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, FontInfo_Retired_PrintsRedirectError) {
  const std::string expected =
      "pagx font: 'info' subcommand has been removed, use 'pagx font' instead";

  // Variant 1: with a positional argument
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "info", "--file", "x.otf"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }

  // Variant 2: no extra arguments
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "info"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }
}

CLI_TEST(PAGXCliTest, Font_HelpShowsCurrentFlags) {
  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--help"});
  std::cout.rdbuf(oldCout);

  EXPECT_EQ(ret, 0);
  auto help = capturedOut.str();
  EXPECT_NE(help.find("--list"), std::string::npos);
  EXPECT_NE(help.find("--file"), std::string::npos);
  EXPECT_NE(help.find("--name"), std::string::npos);
  EXPECT_NE(help.find("--size"), std::string::npos);
  EXPECT_NE(help.find("--json"), std::string::npos);
  EXPECT_EQ(help.find("embed"), std::string::npos);
  EXPECT_EQ(help.find("info"), std::string::npos);
}

CLI_TEST(PAGXCliTest, FontList_TextOutput) {
  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--list"});
  std::cout.rdbuf(oldCout);

  EXPECT_EQ(ret, 0);
  auto out = capturedOut.str();
  EXPECT_FALSE(out.empty());
  EXPECT_NE(out.find('\n'), std::string::npos);
}

CLI_TEST(PAGXCliTest, FontList_JsonOutput) {
  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto ret = CallRun(pagx::cli::RunFont, {"font", "--list", "--json"});
  std::cout.rdbuf(oldCout);

  EXPECT_EQ(ret, 0);
  auto out = capturedOut.str();
  EXPECT_FALSE(out.empty());
  auto trimEnd = out.find_last_not_of(" \t\r\n");
  ASSERT_NE(trimEnd, std::string::npos);
  auto trimStart = out.find_first_not_of(" \t\r\n");
  ASSERT_NE(trimStart, std::string::npos);
  EXPECT_EQ(out[trimStart], '[');
  EXPECT_EQ(out[trimEnd], ']');
  EXPECT_NE(out.find("{\"family\":"), std::string::npos);
}

CLI_TEST(PAGXCliTest, FontList_MutualExclusive) {
  const std::string expected = "pagx font: --list cannot be combined with --file or --name";

  // Variant 1: --list + --file
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "--list", "--file", "x.otf"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }

  // Variant 2: --list + --name
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "--list", "--name", "Arial"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }
}

CLI_TEST(PAGXCliTest, Font_UnknownSubcommand) {
  auto ret = CallRun(pagx::cli::RunFont, {"font", "xyz"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, FontEmbed_Retired_PrintsRedirectError) {
  const std::string expected =
      "pagx font: 'embed' subcommand has been removed, use 'pagx embed' instead";

  // Variant 1: with a positional argument
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "embed", "some.pagx"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }

  // Variant 2: no extra arguments
  {
    std::streambuf* oldCerr = std::cerr.rdbuf();
    std::ostringstream capturedErr;
    std::cerr.rdbuf(capturedErr.rdbuf());
    auto ret = CallRun(pagx::cli::RunFont, {"font", "embed"});
    std::cerr.rdbuf(oldCerr);
    EXPECT_EQ(ret, 1);
    EXPECT_NE(capturedErr.str().find(expected), std::string::npos);
  }
}

//==============================================================================
// Lint tests
//==============================================================================

CLI_TEST(PAGXCliTest, Verify_C6_HighRepeaterCopies) {
  auto inputPath = TestResourcePath("verify_c6_high_repeater.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("Repeater produces") != std::string::npos);
  EXPECT_TRUE(output.find("total copies") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C6_NestedRepeaterProduct) {
  auto inputPath = TestResourcePath("verify_c6_nested_repeater.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("Repeater produces") != std::string::npos);
  EXPECT_TRUE(output.find("total copies") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C7_HighBlurRadius) {
  auto inputPath = TestResourcePath("verify_c7_high_blur.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("BlurFilter radius too large") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C8_StrokeAlignmentInRepeater) {
  auto inputPath = TestResourcePath("verify_c8_stroke_align.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("inside Repeater forces CPU rendering") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C9_DashedStrokeInRepeater) {
  auto inputPath = TestResourcePath("verify_c9_dashed_stroke.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("dashed Stroke inside Repeater") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C10_ComplexPath) {
  auto inputPath = TestResourcePath("verify_c10_complex_path.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("verbs (> 500)") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C11_LowOpacityHighCost) {
  auto inputPath = TestResourcePath("verify_c11_low_opacity.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("opacity") != std::string::npos);
  EXPECT_TRUE(output.find("high-cost children") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_C13_SimpleRectangleMask) {
  auto inputPath = TestResourcePath("verify_c13_simple_rect_mask.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("rectangular alpha mask can use clipToBounds") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_ValidFile) {
  auto inputPath = TestResourcePath("verify_simple.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Verify_MissingFile) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Verify_JsonOutput) {
  auto inputPath = TestResourcePath("verify_simple.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify,
                     {"verify", "--json", "--skip-render", "--skip-layout", inputPath});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("\"ok\": true") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_Help) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Verify_UnknownOption) {
  auto inputPath = TestResourcePath("verify_simple.pagx");
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
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
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

CLI_TEST(PAGXCliTest, Verify_MissingInput) {
  auto ret = CallRun(pagx::cli::RunVerify, {"verify"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Verify_EmptyNodes) {
  auto inputPath = TestResourcePath("verify_empty_nodes.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  // Pure empty Layer and empty Group should be detected.
  EXPECT_NE(output.find("empty Layer with no content"), std::string::npos);
  EXPECT_NE(output.find("empty Group with no elements"), std::string::npos);
  // Empty nodes with explicit size, size-dependent constraints, or in parent layout should
  // NOT be detected. Verify that only 2 diagnostics are reported (emptyLayer + empty Group).
  int count = 0;
  std::string marker = "empty ";
  size_t pos = 0;
  while ((pos = output.find(marker, pos)) != std::string::npos) {
    count++;
    pos += marker.size();
  }
  EXPECT_EQ(count, 2);
}

// NOTE: The Verify_FullCanvasClipMask test is now exercisable because the verify command
// uses a different detection path (DetectFullCanvasClipMask on the document model).

CLI_TEST(PAGXCliTest, Verify_UnreferencedResources) {
  auto inputPath = TestResourcePath("verify_unref_resources.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("unreferenced resource") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_DuplicatePathData) {
  auto inputPath = TestResourcePath("verify_duplicate_pathdata.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("duplicate PathData") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_DuplicateGradient) {
  auto inputPath = TestResourcePath("verify_duplicate_gradient.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("duplicate") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_MergeableGroups) {
  auto inputPath = TestResourcePath("verify_mergeable_groups.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("consecutive Groups share identical painters") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_UnwrappableGroup) {
  auto inputPath = TestResourcePath("verify_unwrappable_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("redundant first-child Group") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_PathToPrimitive) {
  auto inputPath = TestResourcePath("verify_path_to_primitive.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("Path draws an") != std::string::npos);
}

// Heart (M C C C C Z) and diamond (M L L L Z) shapes should NOT be reported as replaceable.
// The ellipse detection validates cardinal positions and kappa-ratio control points,
// correctly rejecting non-elliptical curves like the heart shape.
CLI_TEST(PAGXCliTest, Verify_PathNonPrimitive) {
  auto inputPath = TestResourcePath("verify_path_non_primitive.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  // Diamond (M L L L Z) should NOT be reported as a rectangle (edges not axis-aligned).
  EXPECT_TRUE(output.find("Path draws an axis-aligned rectangle") == std::string::npos);
}

// Verify_LocalizableCoordinates: The old lint had a "localizable coordinates" check for Layer x/y
// attributes, but the new verify command does not include this check. The test resource triggers
// no diagnostics in verify. Adjusted to verify clean exit.
CLI_TEST(PAGXCliTest, Verify_LocalizableCoordinates) {
  auto inputPath = TestResourcePath("verify_localizable_coords.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Verify_ExtractableCompositions) {
  auto inputPath = TestResourcePath("verify_extractable_compositions.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("structurally identical Layers") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_DowngradeableLayer) {
  auto inputPath = TestResourcePath("verify_downgradeable_layer.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);  // warning returns non-zero
  EXPECT_TRUE(output.find("child Layer(s) use no Layer-exclusive features") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_MergeableAdjacentLayers) {
  auto inputPath = TestResourcePath("verify_mergeable_adjacent_layers.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("adjacent Layer(s) can be merged into one") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_UnknownAttribute) {
  auto inputPath = TestResourcePath("verify_unknown_attribute.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("attribute 'rotation' is not allowed") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_IncludeInLayoutNoParent) {
  auto inputPath = TestResourcePath("verify_include_in_layout_no_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("includeInLayout=\"false\" has no effect") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_FlexNoParentLayout) {
  auto inputPath = TestResourcePath("verify_flex_no_parent_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("flex has no effect") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_FullCanvasClipMask) {
  auto inputPath = TestResourcePath("verify_full_canvas_clip.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
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
  auto inputPath = TestResourcePath("verify_not_xml.pagx");
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
  auto inputPath = TestResourcePath("verify_simple.pagx");
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
  auto svgPath = ExportToSVG("verify_simple.pagx", "ImportSVG_RoundTrip.svg");
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
  auto inputPath = TestResourcePath("verify_not_xml.pagx");
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

CLI_TEST(PAGXCliTest, Layout_Help) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--help"});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_MissingFile) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "nonexistent.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_MissingInput) {
  auto ret = CallRun(pagx::cli::RunLayout, {"layout"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_UnknownOption) {
  auto path = TestResourcePath("layout_clean.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--bogus", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_Clean) {
  auto path = TestResourcePath("layout_clean.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_CleanProblemsOnly) {
  auto path = TestResourcePath("layout_clean.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_Overlap) {
  auto path = TestResourcePath("layout_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_Clipped) {
  auto path = TestResourcePath("layout_clipped.pagx");
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

CLI_TEST(PAGXCliTest, Layout_Absolute) {
  auto path = TestResourcePath("layout_absolute.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_ZeroSize) {
  auto path = TestResourcePath("layout_zero_size.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_IdNode) {
  auto path = TestResourcePath("layout_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--id", "parent", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_IdNodeRelativeCoords) {
  auto path = TestResourcePath("layout_clipped.pagx");
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

CLI_TEST(PAGXCliTest, Layout_IdNodeNotFound) {
  auto path = TestResourcePath("layout_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--id", "nonexistent_id", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_XPath) {
  auto path = TestResourcePath("layout_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", "--xpath", "//Layer[@id='parent']", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_CheckXml) {
  auto path = TestResourcePath("layout_overlap.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  // Manual-positioned Layers without container layout no longer trigger overlap warnings.
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_CheckClean) {
  auto path = TestResourcePath("layout_clean.pagx");
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_CheckOverlap) {
  auto path = TestResourcePath("layout_overlap.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  // Manual-positioned Layers without container layout no longer trigger overlap warnings.
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_Elements) {
  auto path = TestResourcePath("layout_elements.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Layout_BoundsOnly) {
  auto path = TestResourcePath("layout_margins.pagx");
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", path});
  std::cout.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  // margins attribute should no longer appear in layout output.
  EXPECT_TRUE(output.find("margins=") == std::string::npos);
  // bounds should still be present.
  EXPECT_TRUE(output.find("bounds=\"0,0,400,300\"") != std::string::npos);
  EXPECT_TRUE(output.find("bounds=\"20,30,200,100\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_Placeholder) {
  auto path = TestResourcePath("verify_placeholder.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  // The third child Layer (id="bad") has zero width, should trigger a diagnostic.
  EXPECT_TRUE(output.find("zero size") != std::string::npos);
}

// Background Rectangle on a layout Layer should not trigger overlap warnings with child Layers.
CLI_TEST(PAGXCliTest, Layout_BackgroundNoOverlap) {
  auto path = TestResourcePath("verify_background.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

// Manual-positioned Layers without container layout should not trigger overlap warnings.
CLI_TEST(PAGXCliTest, Layout_ManualPositionNoOverlap) {
  auto path = TestResourcePath("verify_layout_overlap.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("overlapping siblings") == std::string::npos);
}

// Text with system fonts should be measured correctly via font fallback (not zero-size).
CLI_TEST(PAGXCliTest, Layout_TextFontFallback) {
  auto path = TestResourcePath("verify_text_fallback.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Line Paths have preferred size clamped to 1px minimum, so they should not trigger zero-size
// warnings. Content-measured Layers containing line Paths also get a non-zero size.
CLI_TEST(PAGXCliTest, Layout_PathZeroSize) {
  auto path = TestResourcePath("verify_path_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Content origin offset: unconstrained Path starts at (50, 50), not (0, 0).
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffset) {
  auto path = TestResourcePath("verify_content_offset.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (50,50), not (0,0)") != std::string::npos);
  EXPECT_TRUE(output.find("container measurement inaccurate") != std::string::npos);
}

// Content origin offset with negative coordinates.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetNeg) {
  auto path = TestResourcePath("verify_content_offset_neg.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (-20,0), not (0,0)") != std::string::npos);
}

// Content at origin (0, 0) — no offset problem.
CLI_TEST(PAGXCliTest, Layout_ContentAtOrigin) {
  auto path = TestResourcePath("verify_content_at_origin.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset not reported when Layer has explicit width/height.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetExplicitSize) {
  auto path = TestResourcePath("verify_content_offset_explicit_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset: a constrained Group at (0,0) covers the origin, so the minimum
// coordinate across all children is (0,0) and no offset problem is reported.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetConstrained) {
  auto path = TestResourcePath("verify_content_offset_constrained.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  // The constrained Group at left=0,top=0 has layoutBounds starting at (0,0),
  // so minX=0, minY=0 — no offset problem.
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset not reported when Layer is a flex child (engine assigns size).
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetFlex) {
  auto path = TestResourcePath("verify_content_offset_flex.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset detected inside a Group that has constraints.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetGroup) {
  auto path = TestResourcePath("verify_content_offset_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (20,20), not (0,0)") != std::string::npos);
}

// Content origin offset NOT reported for a Group without constraints (painter scope isolation only).
// The Group's measurement doesn't affect positioning when it has no constraints.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetGroupNoConstraints) {
  auto path = TestResourcePath("verify_content_offset_group_no_constraints.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset NOT reported for a Layer with no constraints and not in parent layout.
// The Layer's measurement doesn't affect any positioning.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetLayerUnpositioned) {
  auto path = TestResourcePath("verify_content_offset_layer_unpositioned.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset NOT reported for a Layer with includeInLayout=false and no constraints.
// Even though parent has container layout, the Layer is excluded and unpositioned.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetExcludedFromLayout) {
  auto path = TestResourcePath("verify_content_offset_excluded_from_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Content origin offset reported for a Layer in parent container layout (positioned by layout).
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetInParentLayout) {
  auto path = TestResourcePath("verify_content_offset_in_parent_layout.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children start at (50,50), not (0,0)") != std::string::npos);
}

// Content origin offset NOT reported when container has a mix of constrained and unconstrained
// children. The constrained child (Text with left/top/right/bottom) defines the intended content
// region, so offset of unconstrained siblings is acceptable.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetMixedConstraints) {
  auto path = TestResourcePath("verify_content_offset_mixed_constraints.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Flex child in content-measured parent (no main-axis size to distribute).
CLI_TEST(PAGXCliTest, Layout_FlexNoParentSize) {
  auto path = TestResourcePath("verify_flex_no_parent_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("flex has no effect, parent") != std::string::npos);
}

// Flex child with explicit parent size — no problem.
CLI_TEST(PAGXCliTest, Layout_FlexWithParentSize) {
  auto path = TestResourcePath("verify_flex_with_parent_size.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
  EXPECT_TRUE(output.find("no space to distribute") == std::string::npos);
}

// Nested flex: parent gets main-axis size from grandparent — no false positive.
CLI_TEST(PAGXCliTest, Layout_FlexNested) {
  auto path = TestResourcePath("verify_flex_nested.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
  EXPECT_TRUE(output.find("no space to distribute") == std::string::npos);
}

// Flex child — parent derives main-axis size from opposite-pair constraints.
CLI_TEST(PAGXCliTest, Layout_FlexConstraintParent) {
  auto path = TestResourcePath("verify_flex_constraint_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("flex child") == std::string::npos);
}

// Flex child zero-size with parent that has opposite-pair constraints deriving zero height.
// The diagnostic should report zero-size and indicate the parent height is 0.
CLI_TEST(PAGXCliTest, Layout_FlexConstraintZeroParent) {
  auto path = TestResourcePath("verify_flex_constraint_zero_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("zero size") != std::string::npos);
  EXPECT_TRUE(output.find("parent height is 0") != std::string::npos);
}

// Flex in horizontal layout with content-measured parent — same problem, different axis.
CLI_TEST(PAGXCliTest, Layout_FlexHorizontal) {
  auto path = TestResourcePath("verify_flex_horizontal.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("parent") != std::string::npos &&
              output.find("is 0") != std::string::npos);
}

// --depth limits Layer nesting depth. depth=1 shows root + direct child Layers but not
// grandchild Layers. Elements inside each shown Layer are always included.
CLI_TEST(PAGXCliTest, Layout_Depth) {
  auto path = TestResourcePath("layout_depth.pagx");
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
CLI_TEST(PAGXCliTest, Layout_DepthZero) {
  auto path = TestResourcePath("layout_depth.pagx");
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
CLI_TEST(PAGXCliTest, Layout_ContainerOverflow) {
  auto path = TestResourcePath("verify_container_overflow.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("children overflow") != std::string::npos);
}

// Negative constraint-derived size: left+right exceeds parent width.
CLI_TEST(PAGXCliTest, Layout_NegativeConstraintSize) {
  auto path = TestResourcePath("verify_negative_constraint.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("negative derived size") != std::string::npos);
}

// Element constraint conflict: centerX overrides left on a VectorElement.
CLI_TEST(PAGXCliTest, Layout_ElementConstraintConflict) {
  auto path = TestResourcePath("verify_element_constraint_conflict.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left ignored, centerX takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_OffCanvas) {
  auto path = TestResourcePath("verify_offcanvas.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("completely outside visible region, not visible") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_ChildExceedsParent) {
  auto path = TestResourcePath("verify_child_exceeds_parent.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  // Both cases (wider child and negative-coord child) should trigger.
  auto count = 0;
  std::string::size_type pos = 0;
  while ((pos = output.find("child extends beyond parent bounds", pos)) != std::string::npos) {
    count++;
    pos += 34;
  }
  EXPECT_EQ(count, 2);
}

CLI_TEST(PAGXCliTest, Layout_ChildExceedsClippedParent) {
  // clipToBounds=true should NOT trigger DetectChildExceedingParent
  auto path = TestResourcePath("verify_child_exceeds_clipped.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("child extends beyond parent bounds") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_InvisibleInClippedContainer) {
  auto path = TestResourcePath("verify_invisible.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  // Cases 1-3,5 trigger invisible detection; Case 4 (partial overlap) should NOT trigger.
  auto count = 0;
  std::string::size_type pos = 0;
  while ((pos = output.find("completely outside visible region", pos)) != std::string::npos) {
    count++;
    pos += 32;
  }
  EXPECT_EQ(count, 4);
}

CLI_TEST(PAGXCliTest, Layout_RedundantConstraintCenterX) {
  auto path = TestResourcePath("verify_redundant_centerx.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left ignored, centerX takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_RedundantConstraintCenterY) {
  auto path = TestResourcePath("verify_redundant_centery.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("ignored, centerY takes priority") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_RedundantConstraintLeftZero) {
  auto path = TestResourcePath("verify_redundant_left_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left=\"0\" redundant") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Layout_RedundantConstraintTopZero) {
  auto path = TestResourcePath("verify_redundant_top_zero.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("top=\"0\" redundant") != std::string::npos);
}

// centerX/centerY on VectorElement inside content-measured Layer — ineffective.
CLI_TEST(PAGXCliTest, Layout_IneffectiveCenterLayer) {
  auto path = TestResourcePath("verify_ineffective_center_layer.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("centerX ineffective") != std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") != std::string::npos);
}

// centerX/centerY on VectorElement inside content-measured Group — ineffective.
CLI_TEST(PAGXCliTest, Layout_IneffectiveCenterGroup) {
  auto path = TestResourcePath("verify_ineffective_center_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("centerX ineffective") != std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") != std::string::npos);
}

// centerX/centerY on VectorElement inside explicit-size Layer — valid, no problem.
CLI_TEST(PAGXCliTest, Layout_IneffectiveCenterExplicitSize) {
  auto path = TestResourcePath("verify_ineffective_center_explicit.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("centerX ineffective") == std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") == std::string::npos);
}

// Content origin offset: Layer with opposite-edge constraints (left+right) derives size from
// constraints, not from content measurement. Should NOT report content-origin-offset.
CLI_TEST(PAGXCliTest, Layout_ContentOriginOffsetOppositeConstraint) {
  auto path = TestResourcePath("verify_content_offset_opposite_constraint.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// Group with opposite-edge constraints (left+right+top+bottom) has constraint-derived size.
// centerX/centerY on child elements should NOT be reported as ineffective.
CLI_TEST(PAGXCliTest, Layout_IneffectiveCenterGroupConstrained) {
  auto path = TestResourcePath("verify_ineffective_center_group_constrained.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("centerX ineffective") == std::string::npos);
  EXPECT_TRUE(output.find("centerY ineffective") == std::string::npos);
}

// TextBox with explicit width="0" height="0" is anchor mode (point text).
// Should NOT trigger zero-size warning.
CLI_TEST(PAGXCliTest, Layout_ZeroSizeExplicit) {
  auto path = TestResourcePath("verify_zero_size_explicit.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Empty layer (no contents, no children) in a layout should NOT trigger zero-size diagnostic,
// while a sibling with content and zero width should still trigger it.
CLI_TEST(PAGXCliTest, Layout_ZeroSizeEmpty) {
  auto path = TestResourcePath("verify_zero_size_empty.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  // The non-empty Layer with width="0" should trigger zero-size.
  EXPECT_TRUE(output.find("zero size (0x50)") != std::string::npos);
  // The empty leaf Layer (line 5) should NOT trigger zero-size.
  EXPECT_TRUE(output.find(":5: zero size") == std::string::npos);
}

// Skeleton-phase containers: nested Layers with layout attributes and painters (Fill/Stroke) but
// no leaf content (Rectangle/Ellipse/Path/Text/etc.) anywhere in the subtree. Zero size is
// expected and should NOT trigger a diagnostic.
CLI_TEST(PAGXCliTest, Layout_ZeroSizeNoLeaf) {
  auto path = TestResourcePath("verify_zero_size_no_leaf.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("zero size") == std::string::npos);
}

// Container with explicit zero width but leaf content (Rectangle) deep in the subtree.
// Zero size IS a real problem and should be reported.
CLI_TEST(PAGXCliTest, Layout_ZeroSizeDeepLeaf) {
  auto path = TestResourcePath("verify_zero_size_deep_leaf.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("zero size (0x100)") != std::string::npos);
}

// Polystar without explicit position should have auto-position applied at import time.
// Content-measured Layer should NOT report content-origin-offset.
CLI_TEST(PAGXCliTest, Layout_PolystarOrigin) {
  auto path = TestResourcePath("verify_polystar_origin.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("container measurement inaccurate") == std::string::npos);
}

// stretch-fill Rectangle inside a padded Layer should be detected.
CLI_TEST(PAGXCliTest, Layout_PaddingStretch) {
  auto path = TestResourcePath("verify_padding_stretch.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("stretch-fill element affected by padding") != std::string::npos);
}

// stretch-fill divider (left="0" right="0") inside a padded Layer should be detected.
CLI_TEST(PAGXCliTest, Layout_PaddingStretchDivider) {
  auto path = TestResourcePath("verify_padding_stretch_divider.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("stretch-fill element affected by padding") != std::string::npos);
}

// Correct nested container (background in outer, padding in inner) should NOT trigger.
CLI_TEST(PAGXCliTest, Layout_PaddingStretchNested) {
  auto path = TestResourcePath("verify_padding_stretch_nested.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stretch-fill element affected by padding") == std::string::npos);
}

// Layers at different positions but identical structure should be detected as extractable.
CLI_TEST(PAGXCliTest, Verify_CompositionPositionDiff) {
  auto inputPath = TestResourcePath("verify_composition_position_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different blendMode should NOT be detected as extractable.
CLI_TEST(PAGXCliTest, Verify_CompositionBlendModeDiff) {
  auto inputPath = TestResourcePath("verify_composition_blendmode_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different Fill fillRule should NOT be detected as extractable.
CLI_TEST(PAGXCliTest, Verify_CompositionFillRuleDiff) {
  auto inputPath = TestResourcePath("verify_composition_fillrule_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("structurally identical Layers"), std::string::npos);
}

// Groups with different Stroke cap should NOT be detected as mergeable.
CLI_TEST(PAGXCliTest, Verify_GroupsDifferentCap) {
  auto inputPath = TestResourcePath("verify_groups_different_cap.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("consecutive Groups share identical painters"), std::string::npos);
}

// Gradients differing only in matrix should be detected as duplicates.
CLI_TEST(PAGXCliTest, Verify_GradientMatrixDiff) {
  auto inputPath = TestResourcePath("verify_gradient_matrix_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("duplicate"), std::string::npos);
}

// Layers with different matrix but identical structure should be detected (matrix is stripped).
CLI_TEST(PAGXCliTest, Verify_CompositionMatrixDiff) {
  auto inputPath = TestResourcePath("verify_composition_matrix_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different constraints but identical structure should be detected (constraints are
// stripped from root).
CLI_TEST(PAGXCliTest, Verify_CompositionConstraintDiff) {
  auto inputPath = TestResourcePath("verify_composition_constraint_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different flex/includeInLayout but identical structure should be detected.
CLI_TEST(PAGXCliTest, Verify_CompositionFlexDiff) {
  auto inputPath = TestResourcePath("verify_composition_flex_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different id/name but identical structure should be detected (id/name are stripped).
CLI_TEST(PAGXCliTest, Verify_CompositionIdNameDiff) {
  auto inputPath = TestResourcePath("verify_composition_id_name_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("structurally identical Layers"), std::string::npos);
}

// Layers with different layout mode should NOT be detected as identical.
CLI_TEST(PAGXCliTest, Verify_CompositionLayoutDiff) {
  auto inputPath = TestResourcePath("verify_composition_layout_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("structurally identical Layers"), std::string::npos);
}

// Groups with identical Fill+Stroke should be detected as mergeable.
CLI_TEST(PAGXCliTest, Verify_GroupsMergeableFillStroke) {
  auto inputPath = TestResourcePath("verify_groups_mergeable_fillstroke.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_NE(output.find("consecutive Groups share identical painters"), std::string::npos);
}

// Groups with different Fill color should NOT be detected as mergeable.
CLI_TEST(PAGXCliTest, Verify_GroupsDifferentFillColor) {
  auto inputPath = TestResourcePath("verify_groups_different_fill_color.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("consecutive Groups share identical painters"), std::string::npos);
}

// Gradients with different ColorStop values should NOT be detected as duplicates.
CLI_TEST(PAGXCliTest, Verify_GradientColorStopDiff) {
  auto inputPath = TestResourcePath("verify_gradient_colorstop_diff.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("duplicate"), std::string::npos);
}

// Two different PathData should NOT be detected as duplicates.
CLI_TEST(PAGXCliTest, Verify_PathDataDifferent) {
  auto inputPath = TestResourcePath("verify_pathdata_different.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(output.find("duplicate PathData"), std::string::npos);
}

//==============================================================================
// Import directive tests (inline <svg> and import attribute)
//==============================================================================

CLI_TEST(PAGXCliTest, ImportDirective_ParseExternalSource) {
  auto inputPath = TestResourcePath("import_node_basic.pagx");
  auto doc = pagx::PAGXImporter::FromFile(inputPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->errors.empty());

  ASSERT_GE(doc->layers.size(), 1u);
  auto* layer = doc->layers[0];
  EXPECT_EQ(layer->importDirective.source, "import_external.svg");
  EXPECT_TRUE(layer->importDirective.content.empty());
}

CLI_TEST(PAGXCliTest, ImportDirective_ParseInlineContent) {
  auto inputPath = TestResourcePath("import_node_basic.pagx");
  auto doc = pagx::PAGXImporter::FromFile(inputPath);
  ASSERT_NE(doc, nullptr);
  ASSERT_GE(doc->layers.size(), 2u);
  auto* layer = doc->layers[1];
  EXPECT_TRUE(layer->importDirective.source.empty());
  EXPECT_FALSE(layer->importDirective.content.empty());
  EXPECT_NE(layer->importDirective.content.find("<svg"), std::string::npos);
  EXPECT_NE(layer->importDirective.content.find("<circle"), std::string::npos);
}

CLI_TEST(PAGXCliTest, ImportDirective_ExportRoundTrip) {
  auto inputPath = TestResourcePath("import_node_basic.pagx");
  auto doc = pagx::PAGXImporter::FromFile(inputPath);
  ASSERT_NE(doc, nullptr);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_NE(xml.find("import=\"import_external.svg\""), std::string::npos);
  EXPECT_NE(xml.find("<svg"), std::string::npos);

  auto doc2 = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(doc2, nullptr);
  ASSERT_GE(doc2->layers.size(), 2u);
  EXPECT_EQ(doc2->layers[0]->importDirective.source, "import_external.svg");
  EXPECT_NE(doc2->layers[1]->importDirective.content.find("<svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, ImportDirective_HasUnresolvedImports) {
  auto withImport = TestResourcePath("import_node_basic.pagx");
  auto doc1 = pagx::PAGXImporter::FromFile(withImport);
  ASSERT_NE(doc1, nullptr);
  EXPECT_TRUE(doc1->hasUnresolvedImports());

  auto withoutImport = TestResourcePath("import_node_none.pagx");
  auto doc2 = pagx::PAGXImporter::FromFile(withoutImport);
  ASSERT_NE(doc2, nullptr);
  EXPECT_FALSE(doc2->hasUnresolvedImports());
}

//==============================================================================
// Resolve command tests
//==============================================================================

CLI_TEST(PAGXCliTest, Resolve_ExpandExternalSource) {
  auto pagxPath = CopyToTemp("import_node_basic.pagx", "resolve_external.pagx");
  CopyToTemp("import_external.svg", "import_external.svg");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  EXPECT_EQ(ret, 0);

  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(doc->hasUnresolvedImports());
}

CLI_TEST(PAGXCliTest, Resolve_NoImportNodes) {
  auto pagxPath = CopyToTemp("import_node_none.pagx", "resolve_none.pagx");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Resolve_OutputToNewFile) {
  auto pagxPath = TestResourcePath("import_node_basic.pagx");
  auto outputPath = TempDir() + "/resolve_output.pagx";
  CopyToTemp("import_external.svg", "import_external.svg");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath, "-o", outputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));

  auto doc = pagx::PAGXImporter::FromFile(outputPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(doc->hasUnresolvedImports());
}

CLI_TEST(PAGXCliTest, Resolve_MissingFile) {
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", "nonexistent.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Resolve_MultiLayerPreservesIsolation) {
  // Verifies that resolving an inline SVG with multiple elements preserves each SVG element
  // in a separate painter scope, preventing painter accumulation bugs.
  auto pagxPath = CopyToTemp("import_resolve_multi_layer.pagx", "resolve_multi_layer.pagx");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  EXPECT_EQ(ret, 0);

  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(doc->hasUnresolvedImports());

  ASSERT_EQ(doc->layers.size(), 1u);
  auto* hostLayer = doc->layers[0];
  EXPECT_FALSE(hostLayer->contents.empty());
  size_t groupCount = 0;
  for (auto* element : hostLayer->contents) {
    if (element->nodeType() == pagx::NodeType::Group) {
      groupCount++;
    }
  }
  EXPECT_GE(groupCount, 1u);

  // Screenshot test: render the resolved file and compare against baseline.
  EXPECT_TRUE(RenderAndCompare({"render", pagxPath}, "PAGXCliTest/ImportResolve_MultiLayer"));
}

CLI_TEST(PAGXCliTest, Resolve_AddsResolvedFromComment) {
  auto pagxPath = CopyToTemp("import_node_basic.pagx", "resolve_comment.pagx");
  CopyToTemp("import_external.svg", "import_external.svg");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  EXPECT_EQ(ret, 0);

  // Read the output file and check for resolved-from comments.
  std::ifstream file(pagxPath);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("Resolved from: import_external.svg"), std::string::npos);
  EXPECT_NE(content.find("Resolved from: inline svg"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Resolve_LayerWithExplicitSizeScalesContent) {
  auto pagxPath = CopyToTemp("resolve_scaled_layer.pagx", "resolve_scaled_layer.pagx");
  CopyToTemp("import_external.svg", "import_external.svg");
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  EXPECT_EQ(ret, 0);

  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(doc->hasUnresolvedImports());

  // External SVG (48x48 viewBox) into 96x96 Layer → scale factor 2.
  ASSERT_GE(doc->layers.size(), 2u);
  auto* scaledExternal = doc->layers[0];
  EXPECT_EQ(scaledExternal->width, 96);
  EXPECT_EQ(scaledExternal->height, 96);

  // Inline SVG (24x24 viewBox) into 48x48 Layer → scale factor 2.
  auto* scaledInline = doc->layers[1];
  EXPECT_EQ(scaledInline->width, 48);
  EXPECT_EQ(scaledInline->height, 48);

  // Both layers should have content (resolved successfully).
  EXPECT_FALSE(scaledExternal->contents.empty() && scaledExternal->children.empty());
  EXPECT_FALSE(scaledInline->contents.empty() && scaledInline->children.empty());
}

CLI_TEST(PAGXCliTest, Resolve_LayerWithContentsSkipsResolve) {
  auto pagxPath = CopyToTemp("resolve_conflict_contents.pagx", "resolve_conflict_contents.pagx");
  CopyToTemp("import_external.svg", "import_external.svg");

  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  std::cerr.rdbuf(old);

  // Conflicting Layer is counted as an error, so resolve returns non-zero.
  EXPECT_NE(ret, 0);
  auto warning = oss.str();
  EXPECT_NE(warning.find("warning"), std::string::npos);
  EXPECT_NE(warning.find("skipping resolve"), std::string::npos);

  // The import should remain unresolved because the Layer was skipped.
  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->hasUnresolvedImports());
}

CLI_TEST(PAGXCliTest, Resolve_LayerWithChildrenSkipsResolve) {
  auto pagxPath = CopyToTemp("resolve_conflict_children.pagx", "resolve_conflict_children.pagx");
  CopyToTemp("import_external.svg", "import_external.svg");

  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunResolve, {"resolve", pagxPath});
  std::cerr.rdbuf(old);

  // Conflicting Layer is counted as an error, so resolve returns non-zero.
  EXPECT_NE(ret, 0);
  auto warning = oss.str();
  EXPECT_NE(warning.find("warning"), std::string::npos);
  EXPECT_NE(warning.find("skipping resolve"), std::string::npos);

  // The import should remain unresolved because the Layer was skipped.
  auto doc = pagx::PAGXImporter::FromFile(pagxPath);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->hasUnresolvedImports());
}

//==============================================================================
// CLI unresolved import checks
//==============================================================================

CLI_TEST(PAGXCliTest, UnresolvedImport_LayoutRejects) {
  auto pagxPath = TestResourcePath("import_node_basic.pagx");
  auto ret = CallRun(pagx::cli::RunLayout, {"layout", pagxPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, UnresolvedImport_RenderRejects) {
  auto pagxPath = TestResourcePath("import_node_basic.pagx");
  auto outputPath = TempDir() + "/unresolved_render.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, pagxPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, UnresolvedImport_BoundsRejects) {
  auto pagxPath = TestResourcePath("import_node_basic.pagx");
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", pagxPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, UnresolvedImport_ExportRejects) {
  auto pagxPath = TestResourcePath("import_node_basic.pagx");
  auto outputPath = TempDir() + "/unresolved_export.svg";
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--input", pagxPath, "--output", outputPath});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Verify screenshot output
//==============================================================================

CLI_TEST(PAGXCliTest, Verify_WritesScreenshot) {
  auto inputPath = TestResourcePath("import_node_none.pagx");
  auto pngPath = TempDir() + "/import_node_none.png";
  auto layoutPath = TempDir() + "/import_node_none.layout.xml";
  std::filesystem::remove(pngPath);
  std::filesystem::remove(layoutPath);
  auto tempPagx = CopyToTemp("import_node_none.pagx", "import_node_none.pagx");

  auto oldCwd = std::filesystem::current_path();
  std::filesystem::current_path(TempDir());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", tempPagx});
  std::filesystem::current_path(oldCwd);
  EXPECT_EQ(ret, 0);

  auto expectedPng = TempDir() + "/import_node_none.png";
  auto expectedLayout = TempDir() + "/import_node_none.layout.xml";
  EXPECT_TRUE(std::filesystem::exists(expectedPng));
  EXPECT_TRUE(std::filesystem::exists(expectedLayout));
  EXPECT_GT(std::filesystem::file_size(expectedPng), 0u);
}

// Content not centered inside a fixed-size Layer with centerX/centerY — should report.
CLI_TEST(PAGXCliTest, Layout_CenteringAsymmetryLayer) {
  auto path = TestResourcePath("verify_centering_asymmetry_layer.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left margin") != std::string::npos);
  EXPECT_TRUE(output.find("top margin") != std::string::npos);
}

// Content not centered inside a fixed-size Group with centerX/centerY — should report.
CLI_TEST(PAGXCliTest, Layout_CenteringAsymmetryGroup) {
  auto path = TestResourcePath("verify_centering_asymmetry_group.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  EXPECT_TRUE(output.find("left margin") != std::string::npos);
  EXPECT_TRUE(output.find("top margin") != std::string::npos);
}

// Content fills the centered container — no asymmetry, should pass clean.
CLI_TEST(PAGXCliTest, Layout_CenteringAsymmetryClean) {
  auto path = TestResourcePath("verify_centering_asymmetry_clean.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("visual bounds of all contents not centered") == std::string::npos);
}

// Content-measured container with centerX/centerY — skip check, should pass clean.
CLI_TEST(PAGXCliTest, Layout_CenteringAsymmetryContentMeasured) {
  auto path = TestResourcePath("verify_centering_asymmetry_content_measured.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("visual bounds of all contents not centered") == std::string::npos);
}

// Nested: outer Layer centered, inner child Layer contains off-center Path — recursive detection.
CLI_TEST(PAGXCliTest, Layout_CenteringAsymmetryNested) {
  auto path = TestResourcePath("verify_centering_asymmetry_nested.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", path});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_EQ(ret, 1);
  // Both outer and inner layers should report asymmetry.
  auto first = output.find("visual bounds of all contents not centered");
  EXPECT_TRUE(first != std::string::npos);
  auto second = output.find("visual bounds of all contents not centered", first + 1);
  EXPECT_TRUE(second != std::string::npos);
}

CLI_TEST(PAGXCliTest, Verify_PainterLeak) {
  auto inputPath = TestResourcePath("verify_painter_leak.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  auto count = 0;
  std::string::size_type pos = 0;
  while ((pos = output.find("painter leaks geometry", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 4);
}

CLI_TEST(PAGXCliTest, Verify_PainterLeakClean) {
  auto inputPath = TestResourcePath("verify_painter_leak_clean.pagx");
  std::streambuf* old = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunVerify, {"verify", "--skip-render", "--skip-layout", inputPath});
  std::cerr.rdbuf(old);
  auto output = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_EQ(output.find("painter leaks geometry"), std::string::npos);
}

//==============================================================================
// Embed tests
//==============================================================================

CLI_TEST(PAGXCliTest, Embed_BothDefault_EmbedsFontsAndImages) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto outPagx = TempDir() + "/embed_both_out.pagx";
  // EMBED-09 implicitly covered: embed_sample.pagx references image_as_mask.png by relative path;
  // resolution happens at PAGXImporter::FromFile load time per D1.2.
  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", tempPagx, "-o", outPagx});
  std::cout.rdbuf(oldCout);
  EXPECT_EQ(ret, 0);
  EXPECT_NE(capturedOut.str().find("pagx embed: wrote"), std::string::npos);
  auto document = pagx::PAGXImporter::FromFile(outPagx);
  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(document->getExternalFilePaths().empty());
  bool hasImageData = false;
  bool hasFontNode = false;
  for (auto& node : document->nodes) {
    if (node->nodeType() == pagx::NodeType::Image) {
      auto* image = static_cast<pagx::Image*>(node.get());
      if (image->data != nullptr) {
        hasImageData = true;
      }
    }
    if (node->nodeType() == pagx::NodeType::Font) {
      hasFontNode = true;
    }
  }
  EXPECT_TRUE(hasImageData);
  EXPECT_TRUE(hasFontNode);
}

CLI_TEST(PAGXCliTest, Embed_SkipFonts_ImagesOnly) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto outPagx = TempDir() + "/embed_skipfonts_out.pagx";
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", "--skip-fonts", tempPagx, "-o", outPagx});
  EXPECT_EQ(ret, 0);
  auto document = pagx::PAGXImporter::FromFile(outPagx);
  ASSERT_NE(document, nullptr);
  bool hasImageData = false;
  bool hasFontNode = false;
  for (auto& node : document->nodes) {
    if (node->nodeType() == pagx::NodeType::Image) {
      auto* image = static_cast<pagx::Image*>(node.get());
      if (image->data != nullptr) {
        hasImageData = true;
      }
    }
    if (node->nodeType() == pagx::NodeType::Font) {
      hasFontNode = true;
    }
  }
  EXPECT_TRUE(hasImageData);
  EXPECT_FALSE(hasFontNode);
}

CLI_TEST(PAGXCliTest, Embed_SkipImages_FontsOnly) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto outPagx = TempDir() + "/embed_skipimgs_out.pagx";
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", "--skip-images", tempPagx, "-o", outPagx});
  EXPECT_EQ(ret, 0);
  auto document = pagx::PAGXImporter::FromFile(outPagx);
  ASSERT_NE(document, nullptr);
  bool hasFilePath = false;
  bool hasNoImageData = true;
  bool hasFontNode = false;
  for (auto& node : document->nodes) {
    if (node->nodeType() == pagx::NodeType::Image) {
      auto* image = static_cast<pagx::Image*>(node.get());
      if (!image->filePath.empty()) {
        hasFilePath = true;
      }
      if (image->data != nullptr) {
        hasNoImageData = false;
      }
    }
    if (node->nodeType() == pagx::NodeType::Font) {
      hasFontNode = true;
    }
  }
  EXPECT_TRUE(hasFilePath);
  EXPECT_TRUE(hasNoImageData);
  EXPECT_TRUE(hasFontNode);
}

CLI_TEST(PAGXCliTest, Embed_BothSkipFlags_ExitsWithError) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto contentBefore = ReadFile(tempPagx);
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream capturedErr;
  std::cerr.rdbuf(capturedErr.rdbuf());
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", "--skip-fonts", "--skip-images", tempPagx});
  std::cerr.rdbuf(oldCerr);
  EXPECT_EQ(ret, 1);
  EXPECT_NE(capturedErr.str().find("pagx embed: --skip-fonts and --skip-images cannot both be set"),
            std::string::npos);
  auto contentAfter = ReadFile(tempPagx);
  EXPECT_EQ(contentBefore, contentAfter);
}

CLI_TEST(PAGXCliTest, Embed_FontFlags_AcceptedLikeOldSubcommand) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto outPagx = TempDir() + "/embed_fontflags_out.pagx";
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", "--file", fontPath, "-o", outPagx, tempPagx});
  EXPECT_EQ(ret, 0);
  auto document = pagx::PAGXImporter::FromFile(outPagx);
  ASSERT_NE(document, nullptr);
  bool hasFontNode = false;
  for (auto& node : document->nodes) {
    if (node->nodeType() == pagx::NodeType::Font) {
      hasFontNode = true;
    }
  }
  EXPECT_TRUE(hasFontNode);
}

CLI_TEST(PAGXCliTest, Embed_AlreadyEmbeddedImage_IsNoOp) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto out1 = TempDir() + "/embed_idempot_pass1.pagx";
  auto out2 = TempDir() + "/embed_idempot_pass2.pagx";
  auto ret1 = CallRun(pagx::cli::RunEmbed, {"embed", tempPagx, "-o", out1});
  EXPECT_EQ(ret1, 0);
  auto ret2 = CallRun(pagx::cli::RunEmbed, {"embed", out1, "-o", out2});
  EXPECT_EQ(ret2, 0);
  EXPECT_EQ(ReadFile(out1), ReadFile(out2));
}

CLI_TEST(PAGXCliTest, Embed_MissingImage_FailsLoud) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_missing.pagx");
  auto content = ReadFile(tempPagx);
  auto pos = content.find("image_as_mask.png");
  ASSERT_NE(pos, std::string::npos);
  content.replace(pos, strlen("image_as_mask.png"), "missing.png");
  std::ofstream out(tempPagx);
  out << content;
  out.close();
  auto outPagx = TempDir() + "/embed_missing_out.pagx";
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream capturedErr;
  std::cerr.rdbuf(capturedErr.rdbuf());
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", tempPagx, "-o", outPagx});
  std::cerr.rdbuf(oldCerr);
  EXPECT_EQ(ret, 1);
  EXPECT_NE(capturedErr.str().find("pagx embed: failed to load image '"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outPagx));
}

CLI_TEST(PAGXCliTest, Embed_Success_PrintsWroteAndExitsZero) {
  auto tempPagx = CopyToTemp("embed_sample.pagx", "embed_sample.pagx");
  auto tempPng = CopyResourceToTemp("resources/apitest/image_as_mask.png", "image_as_mask.png");
  auto outPagx = TempDir() + "/embed_success_out.pagx";
  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed", tempPagx, "-o", outPagx});
  std::cout.rdbuf(oldCout);
  EXPECT_EQ(ret, 0);
  auto output = capturedOut.str();
  EXPECT_NE(output.find("pagx embed: wrote"), std::string::npos);
  EXPECT_NE(output.find(outPagx), std::string::npos);
}

CLI_TEST(PAGXCliTest, Embed_Usage_NoInputErrors_HelpPrints) {
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream capturedErr;
  std::cerr.rdbuf(capturedErr.rdbuf());
  auto ret = CallRun(pagx::cli::RunEmbed, {"embed"});
  std::cerr.rdbuf(oldCerr);
  EXPECT_EQ(ret, 1);
  EXPECT_NE(capturedErr.str().find("pagx embed: missing input file"), std::string::npos);

  std::streambuf* oldCout = std::cout.rdbuf();
  std::ostringstream capturedOut;
  std::cout.rdbuf(capturedOut.rdbuf());
  auto helpRet = CallRun(pagx::cli::RunEmbed, {"embed", "--help"});
  std::cout.rdbuf(oldCout);
  EXPECT_EQ(helpRet, 0);
  auto helpOutput = capturedOut.str();
  EXPECT_NE(helpOutput.find("Usage: pagx embed"), std::string::npos);
  EXPECT_NE(helpOutput.find("--skip-fonts"), std::string::npos);
  EXPECT_NE(helpOutput.find("--skip-images"), std::string::npos);
}

}  // namespace pag
