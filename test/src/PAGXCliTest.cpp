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
#include "cli/CommandBounds.h"
#include "cli/CommandFont.h"
#include "cli/CommandFormat.h"
#include "cli/CommandOptimize.h"
#include "cli/CommandRender.h"
#include "cli/CommandValidator.h"
#include "base/PAGTest.h"
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

static std::string ReadFile(const std::string& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static int CallRun(int (*fn)(int, char*[]), std::vector<std::string> args) {
  std::vector<char*> argv = {};
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  return fn(static_cast<int>(argv.size()), argv.data());
}

static size_t CountOccurrences(const std::string& text, const std::string& pattern) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(pattern, pos)) != std::string::npos) {
    count++;
    pos++;
  }
  return count;
}

//==============================================================================
// Validate tests
//==============================================================================

CLI_TEST(PAGXCliTest, Validate_ValidFile) {
  auto path = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MissingAttribute) {
  auto path = TestResourcePath("validate_missing_attr.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_NonXmlFile) {
  auto path = TestResourcePath("validate_not_xml.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MalformedXml) {
  auto path = TestResourcePath("validate_malformed.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_InvalidRootElement) {
  auto path = TestResourcePath("validate_wrong_root.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_JsonFormat) {
  auto path = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", "--format", "json", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MissingFile) {
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Optimize tests — RemoveEmptyNodes
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_RemoveEmptyElements) {
  auto inputPath = TestResourcePath("optimize_remove_empty.pagx");
  auto outputPath = TempDir() + "/empty_elements_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"empty\"") == std::string::npos);
  EXPECT_TRUE(output.find("width=\"0\"") == std::string::npos);
  EXPECT_TRUE(output.find("\"visible\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_RemoveEmptyGroup) {
  auto inputPath = TestResourcePath("optimize_remove_empty_group.pagx");
  auto outputPath = TempDir() + "/empty_group_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Group") == std::string::npos);
  EXPECT_TRUE(output.find("\"main\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepLayerWithComposition) {
  auto inputPath = TestResourcePath("optimize_keep_comp_layer.pagx");
  auto outputPath = TempDir() + "/keep_comp_layer_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"ref\"") != std::string::npos);
  EXPECT_TRUE(output.find("composition=\"@comp1\"") != std::string::npos);
}

//==============================================================================
// Optimize tests — DeduplicatePathData
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_DeduplicateMultiplePaths) {
  auto inputPath = TestResourcePath("optimize_dedup_multi_path.pagx");
  auto outputPath = TempDir() + "/dup_multi_path_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<PathData"), 1u);
}

//==============================================================================
// Optimize tests — DeduplicateColorSources
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_DeduplicateGradient) {
  auto inputPath = TestResourcePath("optimize_dedup_linear_gradient.pagx");
  auto outputPath = TempDir() + "/dup_gradient_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<LinearGradient"), 1u);
}

CLI_TEST(PAGXCliTest, Optimize_DeduplicateRadialGradient) {
  auto inputPath = TestResourcePath("optimize_dedup_radial_gradient.pagx");
  auto outputPath = TempDir() + "/dup_radial_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<RadialGradient"), 1u);
}

CLI_TEST(PAGXCliTest, Optimize_NoDeduplicateDifferentGradients) {
  auto inputPath = TestResourcePath("optimize_no_dedup_diff_gradient.pagx");
  auto outputPath = TempDir() + "/no_dup_gradient_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<LinearGradient"), 1u);
  EXPECT_EQ(CountOccurrences(output, "<RadialGradient"), 1u);
}

//==============================================================================
// Optimize tests — RemoveUnreferencedResources
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_UnreferencedResources) {
  auto inputPath = TestResourcePath("optimize_unref_resources.pagx");
  auto outputPath = TempDir() + "/unref_resources_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("orphanPath") == std::string::npos);
  EXPECT_TRUE(output.find("unusedGrad") == std::string::npos);
  EXPECT_TRUE(output.find("\"grad\"") != std::string::npos);
  EXPECT_EQ(CountOccurrences(output, "<LinearGradient"), 1u);
}

//==============================================================================
// Optimize tests — ReplacePathsWithPrimitives
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_PathToRectangle) {
  auto inputPath = TestResourcePath("optimize_path_to_rect.pagx");
  auto outputPath = TempDir() + "/rect_path_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Rectangle") != std::string::npos);
  EXPECT_TRUE(output.find("<Path") == std::string::npos);
}

// NOTE: Path->Ellipse detection via isOval() requires exact Bezier control points. The SVG path
// string format used by PAGX (with default float precision) introduces rounding that prevents
// isOval() from matching after an export->import round trip. The optimization still works for
// Path data constructed programmatically with full float precision (e.g., from non-PAGX sources).
// A Path->Ellipse test is omitted here due to this inherent limitation.

CLI_TEST(PAGXCliTest, Optimize_PathKeepIrregular) {
  auto inputPath = TestResourcePath("optimize_path_keep_irregular.pagx");
  auto outputPath = TempDir() + "/irregular_path_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Path") != std::string::npos ||
              output.find("<PathData") != std::string::npos);
  EXPECT_TRUE(output.find("<Rectangle") == std::string::npos);
  EXPECT_TRUE(output.find("<Ellipse") == std::string::npos);
}

//==============================================================================
// Optimize tests — RemoveFullCanvasClipMasks
//==============================================================================

// NOTE: Partial clip mask retention test is omitted because mask="@id" references are not
// preserved through the PAGX import/export round trip in the current Exporter implementation.

CLI_TEST(PAGXCliTest, Optimize_RemoveFullCanvasClips) {
  auto inputPath = TestResourcePath("optimize_fullcanvas_clips.pagx");
  auto outputPath = TempDir() + "/fullcanvas_clips_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("mask=") == std::string::npos);
}

//==============================================================================
// Optimize tests — RemoveOffCanvasLayers
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_RemoveOffCanvasRight) {
  auto inputPath = TestResourcePath("optimize_offcanvas_right.pagx");
  auto outputPath = TempDir() + "/offcanvas_right_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"visible\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"offscreen\"") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepPartiallyVisible) {
  auto inputPath = TestResourcePath("optimize_partial_visible.pagx");
  auto outputPath = TempDir() + "/partial_visible_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"partial\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepInvisibleLayer) {
  auto inputPath = TestResourcePath("optimize_invisible_layer.pagx");
  auto outputPath = TempDir() + "/invisible_layer_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"hidden\"") != std::string::npos);
}

//==============================================================================
// Optimize tests — LocalizeCoordinates
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_LocalizeRectangle) {
  auto inputPath = TestResourcePath("optimize_localize_rect.pagx");
  auto outputPath = TempDir() + "/localize_rect_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"200\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"300\"") != std::string::npos);
  EXPECT_TRUE(output.find("center=") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeEllipse) {
  auto inputPath = TestResourcePath("optimize_localize_ellipse.pagx");
  auto outputPath = TempDir() + "/localize_ellipse_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"150\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"250\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizePolystar) {
  auto inputPath = TestResourcePath("optimize_localize_polystar.pagx");
  auto outputPath = TempDir() + "/localize_polystar_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"300\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"200\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeTextBox) {
  auto inputPath = TestResourcePath("optimize_localize_textbox.pagx");
  auto outputPath = TempDir() + "/localize_textbox_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"100\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"150\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeSkipMatrix) {
  auto inputPath = TestResourcePath("optimize_localize_skip_matrix.pagx");
  auto outputPath = TempDir() + "/localize_matrix_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("center=\"200,200\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeAlreadyAtOrigin) {
  auto inputPath = TestResourcePath("optimize_localize_at_origin.pagx");
  auto outputPath = TempDir() + "/localize_origin_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"100\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"100\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeNestedLayers) {
  auto inputPath = TestResourcePath("optimize_localize_nested.pagx");
  auto outputPath = TempDir() + "/localize_nested_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"parent\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"child\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeMultipleElements) {
  auto inputPath = TestResourcePath("optimize_localize_multi_elements.pagx");
  auto outputPath = TempDir() + "/localize_multi_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
}

//==============================================================================
// Optimize tests — ExtractCompositions
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_ExtractFourPlusLayers) {
  auto inputPath = TestResourcePath("optimize_extract_four_layers.pagx");
  auto outputPath = TempDir() + "/extract_four_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<Composition"), 1u);
  EXPECT_EQ(CountOccurrences(output, "composition=\"@"), 4u);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractDifferentContent) {
  auto inputPath = TestResourcePath("optimize_no_extract_diff_content.pagx");
  auto outputPath = TempDir() + "/no_extract_diff_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractMatrixLayer) {
  auto inputPath = TestResourcePath("optimize_no_extract_matrix.pagx");
  auto outputPath = TempDir() + "/no_extract_matrix_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractSingleLayer) {
  auto inputPath = TestResourcePath("optimize_no_extract_single.pagx");
  auto outputPath = TempDir() + "/no_extract_single_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

//==============================================================================
// Optimize tests — DryRun, general, and error handling
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_DryRun) {
  auto inputPath = CopyToTemp("validate_simple.pagx", "dryrun.pagx");
  auto originalContent = ReadFile(inputPath);
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  EXPECT_EQ(ret, 0);
  auto afterContent = ReadFile(inputPath);
  EXPECT_EQ(originalContent, afterContent);
}

CLI_TEST(PAGXCliTest, Optimize_DryRunWithChanges) {
  auto inputPath = CopyToTemp("optimize_remove_empty.pagx", "dryrun_changes.pagx");
  auto originalContent = ReadFile(inputPath);
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  EXPECT_EQ(ret, 0);
  auto afterContent = ReadFile(inputPath);
  EXPECT_EQ(originalContent, afterContent);
}

CLI_TEST(PAGXCliTest, Optimize_NoOptimizationsNeeded) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto outputPath = TempDir() + "/already_optimal_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Optimize_InPlaceOverwrite) {
  auto inputPath = CopyToTemp("optimize_inplace.pagx", "inplace.pagx");
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(inputPath);
  EXPECT_TRUE(output.find("\"empty\"") == std::string::npos);
  EXPECT_TRUE(output.find("\"main\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_InvalidPagx) {
  auto inputPath = TestResourcePath("validate_not_xml.pagx");
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Optimize_UnknownOption) {
  auto inputPath = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--unknown", inputPath});
  EXPECT_NE(ret, 0);
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
  auto centerPos = output.find("center=");
  auto sizePos = output.find("size=");
  ASSERT_NE(centerPos, std::string::npos);
  ASSERT_NE(sizePos, std::string::npos);
  EXPECT_LT(centerPos, sizePos);
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
  auto ret = CallRun(pagx::cli::RunBounds,
                     {"bounds", "--relative", "//Layer[@id='content']", "--xpath",
                      "//Layer[@id='card1']", inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Bounds_MissingFile) {
  auto ret = CallRun(pagx::cli::RunBounds, {"bounds", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Render tests — Verify rendered output matches baseline screenshots
//==============================================================================

CLI_TEST(PAGXCliTest, Render_Basic) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderBasic.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderBasic"));
}

CLI_TEST(PAGXCliTest, Render_Gradient) {
  auto inputPath = TestResourcePath("render_gradient.pagx");
  auto outputPath = TempDir() + "/RenderGradient.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderGradient"));
}

CLI_TEST(PAGXCliTest, Render_Scale) {
  auto inputPath = TestResourcePath("render_scale.pagx");
  auto outputPath = TempDir() + "/RenderScale.png";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--scale", "2.0", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderScale"));
}

CLI_TEST(PAGXCliTest, Render_Crop) {
  auto inputPath = TestResourcePath("render_crop.pagx");
  auto outputPath = TempDir() + "/RenderCrop.png";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--crop", "50,50,100,100", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderCrop"));
}

CLI_TEST(PAGXCliTest, Render_Background) {
  auto inputPath = TestResourcePath("render_background.pagx");
  auto outputPath = TempDir() + "/RenderBackground.png";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--background", "#0088FF", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderBackground"));
}

CLI_TEST(PAGXCliTest, Render_WebpFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderWebpFormat.webp";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--format", "webp", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderWebpFormat"));
}

CLI_TEST(PAGXCliTest, Render_JpgFormat) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderJpgFormat.jpg";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--format", "jpg", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderJpgFormat"));
}

CLI_TEST(PAGXCliTest, Render_Quality) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderQuality.webp";
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--format", "webp", "--quality", "80", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_GT(std::filesystem::file_size(outputPath), 0u);
}

CLI_TEST(PAGXCliTest, Render_CombinedOptions) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderCombinedOptions.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--scale", "2.0", "--crop",
                                             "0,0,200,200", "--background", "#FFFF00", inputPath});
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(CompareRenderedImage(outputPath, "PAGXCliTest/RenderCombinedOptions"));
}

CLI_TEST(PAGXCliTest, Render_MissingFile) {
  auto outputPath = TempDir() + "/RenderMissingFile.png";
  auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "nonexistent.pagx"});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_MissingOutput) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto ret = CallRun(pagx::cli::RunRender, {"render", inputPath});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Render_InvalidScale) {
  auto inputPath = TestResourcePath("render_basic.pagx");
  auto outputPath = TempDir() + "/RenderInvalidScale.png";
  auto ret =
      CallRun(pagx::cli::RunRender, {"render", "-o", outputPath, "--scale", "0", inputPath});
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
  auto ret = CallRun(pagx::cli::RunRender,
                     {"render", "-o", outputPath, "--format", "bmp", inputPath});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Optimize render consistency — Verify optimization preserves visual output
//==============================================================================

static void TestOptimizeAndRender(const std::string& testName, const std::string& inputFile) {
  auto inputPath = TestResourcePath(inputFile);
  auto optimizedPath = TempDir() + "/" + testName + "_optimized.pagx";
  auto originalRenderPath = TempDir() + "/" + testName + "_original.png";
  auto optimizedRenderPath = TempDir() + "/" + testName + "_optimized.png";

  {
    auto ret = CallRun(pagx::cli::RunRender, {"render", "-o", originalRenderPath, inputPath});
    ASSERT_EQ(ret, 0) << "Failed to render original file";
  }
  {
    auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", optimizedPath, inputPath});
    ASSERT_EQ(ret, 0) << "Failed to optimize file";
  }
  {
    auto ret =
        CallRun(pagx::cli::RunRender, {"render", "-o", optimizedRenderPath, optimizedPath});
    ASSERT_EQ(ret, 0) << "Failed to render optimized file";
  }

  EXPECT_TRUE(CompareRenderedImage(originalRenderPath, "PAGXCliTest/" + testName));
  EXPECT_TRUE(CompareRenderedImage(optimizedRenderPath, "PAGXCliTest/" + testName));
}

CLI_TEST(PAGXCliTest, OptimizeRender_EmptyElements) {
  TestOptimizeAndRender("OptimizeEmptyElements", "optimize_empty_elements.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_DedupPath) {
  TestOptimizeAndRender("OptimizeDedupPath", "optimize_dedup_path.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_DedupGradient) {
  TestOptimizeAndRender("OptimizeDedupGradient", "optimize_dedup_gradient.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_PathToPrimitive) {
  TestOptimizeAndRender("OptimizePathToPrimitive", "optimize_path_to_primitive.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_FullCanvasClip) {
  TestOptimizeAndRender("OptimizeFullCanvasClip", "optimize_full_canvas_clip.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_OffCanvas) {
  TestOptimizeAndRender("OptimizeOffCanvas", "optimize_off_canvas.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_LocalizeCoords) {
  TestOptimizeAndRender("OptimizeLocalizeCoords", "optimize_localize_coords.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_Unreferenced) {
  TestOptimizeAndRender("OptimizeUnreferenced", "optimize_unreferenced.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_ExtractComposition) {
  TestOptimizeAndRender("OptimizeExtractComposition", "optimize_extract_composition.pagx");
}

CLI_TEST(PAGXCliTest, OptimizeRender_Comprehensive) {
  TestOptimizeAndRender("OptimizeComprehensive", "optimize_comprehensive.pagx");
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

}  // namespace pag
