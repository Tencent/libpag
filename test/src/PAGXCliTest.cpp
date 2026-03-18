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
#include "cli/CommandFont.h"
#include "cli/CommandFormat.h"
#include "cli/CommandLint.h"
#include "cli/CommandOptimize.h"
#include "cli/CommandRender.h"
#include "cli/CommandValidator.h"
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

// Bad-case fixtures: PAGX files that violate a quality rule.
// Each bad-case test asserts that the checker DETECTS the issue (no missed detections).
static std::string BadCase(const std::string& name) {
  return ProjectPath::Absolute("resources/cli/bad_cases/" + name);
}

// Good-case fixtures: PAGX files that comply with all quality rules.
// Each good-case test asserts that the checker raises NO issue (no false positives).
static std::string GoodCase(const std::string& name) {
  return ProjectPath::Absolute("resources/cli/good_cases/" + name);
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

// Expected: validate passes (exit 0). Observable: no error output. A well-formed PAGX file
// with all required attributes present is the baseline for a passing validate run.
CLI_TEST(PAGXCliTest, Validate_ValidFile) {
  auto path = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

// Expected: validate fails (non-zero exit). Observable: error output about missing required
// attribute. A PAGX file with a mandatory attribute omitted is structurally invalid.
CLI_TEST(PAGXCliTest, Validate_MissingAttribute) {
  auto path = BadCase("validate_missing_attr.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

// Expected: validate fails (non-zero exit). Observable: XML parse error in output. A file
// with non-XML content cannot be parsed at all — this is the earliest failure gate.
CLI_TEST(PAGXCliTest, Validate_NonXmlFile) {
  auto path = TestResourcePath("validate_not_xml.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

// Expected: validate fails (non-zero exit). Observable: XML parse error in output. A file
// with broken XML structure (unclosed tags, etc.) fails before schema validation.
CLI_TEST(PAGXCliTest, Validate_MalformedXml) {
  auto path = BadCase("validate_malformed.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

// Expected: validate fails (non-zero exit). Observable: error about unexpected root element.
// A PAGX file must have a specific root tag; any other root makes the whole file invalid.
CLI_TEST(PAGXCliTest, Validate_InvalidRootElement) {
  auto path = BadCase("validate_wrong_root.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

// Expected: validate passes (exit 0). Observable: output is valid JSON. The --json flag
// switches output format; a clean file should still return 0 in JSON mode.
CLI_TEST(PAGXCliTest, Validate_JsonFormat) {
  auto path = TestResourcePath("validate_simple.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", "--json", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MissingFile) {
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", "nonexistent_file.pagx"});
  EXPECT_NE(ret, 0);
}

// Semantic rule tests (validate returns non-zero on violation)

// Expected: validate fails (non-zero exit). Observable: error message contains "Fill/Stroke before
// MergePath". A Fill inside MergePath scope is silently discarded at runtime — validate catches
// this authoring mistake before the file ships.
CLI_TEST(PAGXCliTest, Validate_MergePathClearsFill) {
  auto path = BadCase("validate_fmt_mergepath_clears_fill.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(errOutput.find("Fill/Stroke-before-MergePath") != std::string::npos);
}

// Expected: validate passes (exit 0). Observable: no error output. MergePath with no preceding
// Fill/Stroke is valid usage — verifies no false positive.
CLI_TEST(PAGXCliTest, Validate_MergePathIsolated) {
  auto path = GoodCase("validate_fmt_mergepath_isolated.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

// Expected: validate fails (non-zero exit). Observable: error message contains "will be ignored
// because TextBox". An explicit position attribute is ignored at runtime when TextBox is
// present — validate flags this silent override to prevent layout confusion.
CLI_TEST(PAGXCliTest, Validate_TextBoxOverridesPosition) {
  auto path = BadCase("validate_fmt_textbox_overrides_position.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(errOutput.find("will-be-ignored-because-TextBox") != std::string::npos);
}

// Expected: validate passes (exit 0). Observable: no error output. TextBox used without
// conflicting position/anchor attributes — verifies no false positive.
CLI_TEST(PAGXCliTest, Validate_TextBoxClean) {
  auto path = GoodCase("validate_fmt_textbox_clean.pagx");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

//==============================================================================
// Optimize tests — RemoveEmptyNodes
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
  EXPECT_TRUE(output.find("position=") == std::string::npos);
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
  EXPECT_TRUE(output.find("position=\"200,200\"") != std::string::npos);
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

CLI_TEST(PAGXCliTest, Optimize_PreserveNewline) {
  auto inputPath = TestResourcePath("optimize_preserve_newline.pagx");
  auto outputPath = TempDir() + "/preserve_newline_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("&#10;") != std::string::npos);
  EXPECT_TRUE(output.find("Hello&#10;World") != std::string::npos);
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
  auto positionPos = output.find("position=");
  auto sizePos = output.find("size=");
  ASSERT_NE(positionPos, std::string::npos);
  ASSERT_NE(sizePos, std::string::npos);
  EXPECT_LT(positionPos, sizePos);
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
// Optimize render consistency — Verify optimization preserves visual output
//==============================================================================

static void TestOptimizeAndRender(const std::string& testName, const std::string& inputFile) {
  auto inputPath = TestResourcePath(inputFile);
  auto optimizedPath = TempDir() + "/" + testName + "_optimized.pagx";

  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", optimizedPath, inputPath});
  ASSERT_EQ(ret, 0) << "Failed to optimize file";

  auto baselineKey = "PAGXCliTest/" + testName;
  EXPECT_TRUE(RenderAndCompare({"render", inputPath}, baselineKey + "_original"))
      << "Original rendering mismatch for " << testName;
  EXPECT_TRUE(RenderAndCompare({"render", optimizedPath}, baselineKey + "_optimized"))
      << "Optimized rendering mismatch for " << testName;
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

//==============================================================================
// Lint hints tests
//==============================================================================

CLI_TEST(PAGXCliTest, Lint_C6_HighRepeaterCopies) {
  auto inputPath = TestResourcePath("lint_c6_high_repeater.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("HighCopies") != std::string::npos);
  EXPECT_TRUE(output.find("Repeater with high copies count") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C6_NestedRepeaterProduct) {
  auto inputPath = TestResourcePath("lint_c6_nested_repeater.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("NestedRepeaters") != std::string::npos);
  EXPECT_TRUE(output.find("Nested Repeater with product") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C7_HighBlurRadius) {
  auto inputPath = TestResourcePath("lint_c7_high_blur.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("HighBlur") != std::string::npos);
  EXPECT_TRUE(output.find("blur radius") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C8_StrokeAlignmentInRepeater) {
  auto inputPath = TestResourcePath("lint_c8_stroke_align.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("StrokeAlign") != std::string::npos);
  EXPECT_TRUE(output.find("Stroke with non-center alignment") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C9_DashedStrokeInRepeater) {
  auto inputPath = TestResourcePath("lint_c9_dashed_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("DashedStroke") != std::string::npos);
  EXPECT_TRUE(output.find("Dashed Stroke within Repeater scope") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C10_ComplexPath) {
  auto inputPath = TestResourcePath("lint_c10_complex_path.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("ComplexPath") != std::string::npos);
  EXPECT_TRUE(output.find("Path with high complexity") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C11_LowOpacityHighCost) {
  auto inputPath = TestResourcePath("lint_c11_low_opacity.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("LowOpacity") != std::string::npos);
  EXPECT_TRUE(output.find("Low opacity") != std::string::npos);
  EXPECT_TRUE(output.find("high-cost elements") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_C13_SimpleRectangleMask) {
  auto inputPath = TestResourcePath("lint_c13_simple_rect_mask.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("lint hint") != std::string::npos);
  EXPECT_TRUE(output.find("MaskedContent") != std::string::npos);
  EXPECT_TRUE(output.find("rectangular mask") != std::string::npos);
  EXPECT_TRUE(output.find("scrollRect") != std::string::npos);
}

//==============================================================================
// Lint visual quality tests
//==============================================================================

// Pixel alignment

// Expected: lint reports "pixel-alignment" issue. Observable: output contains "pixel-alignment".
// A layer with coordinates not on the 0.5px grid (e.g., x=10.3) will appear blurry on
// non-retina screens because the renderer sub-pixel-blends adjacent pixels.
CLI_TEST(PAGXCliTest, Lint_MisalignedCoord) {
  auto inputPath = BadCase("lint_vis_misaligned_coord.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("pixel-alignment") != std::string::npos);
}

// Expected: lint reports no "pixel-alignment" issue. Observable: output does NOT contain
// "pixel-alignment". Coordinates on 0.5px grid are valid — verifies no false positive.
CLI_TEST(PAGXCliTest, Lint_AlignedCoord) {
  auto inputPath = GoodCase("lint_vis_aligned_coord.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("pixel-alignment") == std::string::npos);
}

// Stroke width — minimum

// Expected: lint reports "stroke-too-thin" issue. Observable: output contains "stroke-too-thin".
// A stroke width below 1px (e.g., 0.5px) renders as a faint ghost line on most displays;
// it was likely a design oversight rather than intentional style.
CLI_TEST(PAGXCliTest, Lint_ThinStroke) {
  auto inputPath = BadCase("lint_vis_thin_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-too-thin") != std::string::npos);
}

// Expected: lint reports no "stroke-too-thin" issue. Observable: output does NOT contain
// "stroke-too-thin". Stroke ≥1px is within the valid minimum — verifies no false positive.
CLI_TEST(PAGXCliTest, Lint_NormalStroke) {
  auto inputPath = GoodCase("lint_vis_normal_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-too-thin") == std::string::npos);
}

// Stroke width — consistency

// Expected: lint reports "stroke-inconsistent" issue. Observable: output contains
// "stroke-inconsistent". Multiple strokes with different widths in the same composition
// produce a visually unbalanced icon; this is almost always unintentional.
CLI_TEST(PAGXCliTest, Lint_InconsistentStroke) {
  auto inputPath = BadCase("lint_vis_inconsistent_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-inconsistent") != std::string::npos);
}

// Expected: lint reports no "stroke-inconsistent" issue. Observable: output does NOT contain
// "stroke-inconsistent". All strokes share the same width — verifies no false positive.
CLI_TEST(PAGXCliTest, Lint_ConsistentStroke) {
  auto inputPath = GoodCase("lint_vis_consistent_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-inconsistent") == std::string::npos);
}

// Stroke width — canvas-size range

// Expected: lint reports "stroke-out-of-range" issue. Observable: output contains
// "stroke-out-of-range". A stroke that is too thick relative to the icon canvas (e.g., 8px on a
// 24×24 canvas) fills the shape rather than outlining it, losing legibility.
CLI_TEST(PAGXCliTest, Lint_StrokeOutOfRange) {
  auto inputPath = BadCase("lint_vis_stroke_out_of_range.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-out-of-range") != std::string::npos);
}

// Stroke — corner radius to stroke width ratio

// Expected: lint reports "stroke-corner-ratio" issue. Observable: output contains
// "stroke-corner-ratio". When stroke width exceeds corner radius, the inner curve of a rounded
// corner inverts, creating a sharp pinch instead of a smooth arc.
CLI_TEST(PAGXCliTest, Lint_StrokeCornerRatio) {
  auto inputPath = BadCase("lint_vis_stroke_corner_ratio.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-corner-ratio") != std::string::npos);
}

// Canvas edge

// Expected: lint reports "touches-canvas-edge" issue. Observable: output contains
// "touches-canvas-edge". A layer whose bounding box reaches the canvas boundary provides no
// visual breathing room; in tight grid layouts the icon appears to collide with adjacent icons.
CLI_TEST(PAGXCliTest, Lint_TouchesCanvasEdge) {
  auto inputPath = BadCase("lint_vis_touches_canvas_edge.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("touches-canvas-edge") != std::string::npos);
}

// Theme color — hardcoded black

// Expected: lint reports "hardcoded-theme-color" issue. Observable: output contains
// "hardcoded-theme-color". A fill or stroke hardcoded to exact black (#000000) cannot be
// overridden by the icon system's theme engine, breaking dark-mode and tinted variants.
CLI_TEST(PAGXCliTest, Lint_HardcodedBlack) {
  auto inputPath = BadCase("lint_vis_hardcoded_black.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("hardcoded-theme-color") != std::string::npos);
}

// Expected: lint reports no "hardcoded-theme-color" issue. Observable: output does NOT contain
// "hardcoded-theme-color". A fill using a theme color reference (non-black/white) is valid —
// verifies no false positive.
CLI_TEST(PAGXCliTest, Lint_ThemeColor) {
  auto inputPath = GoodCase("lint_vis_theme_color.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("hardcoded-theme-color") == std::string::npos);
}

// Theme color — hardcoded white

// Expected: lint reports "hardcoded-theme-color" issue. Observable: output contains
// "hardcoded-theme-color". Same rationale as hardcoded black — pure white (#FFFFFF) is
// equally unthemeable and must be replaced with a theme token.
CLI_TEST(PAGXCliTest, Lint_HardcodedWhite) {
  auto inputPath = BadCase("lint_vis_hardcoded_white.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("hardcoded-theme-color") != std::string::npos);
}

// Odd stroke alignment — layer position misaligned (integer, not half-pixel)

// Expected: lint reports "odd-stroke-alignment" issue. Observable: output contains
// "odd-stroke-alignment". A stroke with odd pixel width centered on an integer coordinate
// straddles two pixels equally, causing anti-aliased blur; shifting to a half-pixel position
// aligns the stroke center to a pixel boundary and produces a crisp line.
CLI_TEST(PAGXCliTest, Lint_OddStrokeMisaligned) {
  auto inputPath = BadCase("lint_vis_odd_stroke_misaligned.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("odd-stroke-alignment") != std::string::npos);
}

// Odd stroke alignment — layer position on strict half-pixel (should pass)

// Expected: lint reports no "odd-stroke-alignment" issue. Observable: output does NOT contain
// "odd-stroke-alignment". Odd-width stroke on a half-pixel position is correct — verifies no
// false positive.
CLI_TEST(PAGXCliTest, Lint_OddStrokeAligned) {
  auto inputPath = GoodCase("lint_vis_odd_stroke_aligned.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("odd-stroke-alignment") == std::string::npos);
}

// Pixel alignment — misaligned Ellipse geometry

// Expected: lint reports "pixel-alignment" issue. Observable: output contains "pixel-alignment".
// Same rule as coordinate misalignment, but applied to Ellipse geometry attributes directly.
// Ellipses are particularly sensitive because sub-pixel offsets create asymmetric anti-aliasing.
CLI_TEST(PAGXCliTest, Lint_EllipseMisaligned) {
  auto inputPath = BadCase("lint_vis_ellipse_misaligned.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("pixel-alignment") != std::string::npos);
}

// Stroke range — canvas 16x16: stroke 2px exceeds safe range [1.0, 1.5]px

// Expected: lint reports "stroke-out-of-range" issue. Observable: output contains
// "stroke-out-of-range". On a 16×16 canvas, a 2px stroke is proportionally too thick:
// it would visually dominate the icon and leave little room for the fill shape.
CLI_TEST(PAGXCliTest, Lint_StrokeOutOfRangeSmallCanvas) {
  auto inputPath = BadCase("lint_vis_stroke_range_small_canvas.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-out-of-range") != std::string::npos);
}

// Stroke range — canvas 64x64 (>48px): no range enforced, should pass

// Expected: lint reports no "stroke-out-of-range" issue. Observable: output does NOT contain
// "stroke-out-of-range". Canvas larger than 48px has no strict stroke range — verifies the
// rule does not over-trigger on larger canvases.
CLI_TEST(PAGXCliTest, Lint_StrokeLargeCanvasNoRange) {
  auto inputPath = GoodCase("lint_vis_stroke_range_large_canvas.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-out-of-range") == std::string::npos);
}

// Corner ratio — rounded rectangle with no stroke: should not flag

// Expected: lint reports no "stroke-corner-ratio" issue. Observable: output does NOT contain
// "stroke-corner-ratio". The ratio rule only applies when a stroke is present — a fill-only
// rounded rectangle is always valid regardless of corner size.
CLI_TEST(PAGXCliTest, Lint_CornerRatioNoStroke) {
  auto inputPath = GoodCase("lint_vis_corner_no_stroke.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("stroke-corner-ratio") == std::string::npos);
}

// Canvas edge — content well within boundary: should pass

// Expected: lint reports no canvas-edge issue. Observable: output does NOT contain
// "touches-canvas-edge". Content well within the canvas boundary verifies no false positive.
CLI_TEST(PAGXCliTest, Lint_SafeZoneBoundaryPass) {
  auto inputPath = GoodCase("lint_vis_safezone_boundary_pass.pagx");
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("touches-canvas-edge") == std::string::npos);
}

// Lint CLI error handling
CLI_TEST(PAGXCliTest, Lint_MissingFileArg) {
  // No file argument provided — should print usage and return 0 (lint is always advisory).
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint"});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(errOutput.find("missing input file") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_UnknownOption) {
  // Unknown flag passed — should print error message and return 0.
  auto inputPath = GoodCase("lint_vis_aligned_coord.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", "--not-a-flag", inputPath});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(errOutput.find("unknown option") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_MultipleFiles) {
  // Two file arguments provided — should reject with error and return 0.
  auto inputPath = GoodCase("lint_vis_aligned_coord.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", inputPath, inputPath});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(errOutput.find("multiple input files") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Lint_NonexistentFile) {
  // File does not exist — LintFile returns a load-error issue.
  std::string output;
  std::streambuf* old = std::cout.rdbuf();
  std::ostringstream oss;
  std::cout.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunLint, {"lint", "nonexistent_file.pagx"});
  std::cout.rdbuf(old);
  output = oss.str();
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(output.find("Failed to load file") != std::string::npos);
}

// MergePath — multiple MergePaths: first Fill+MergePath triggers violation

// Expected: validate fails (non-zero exit). Observable: error message contains "Fill/Stroke before
// MergePath". With multiple MergePath nodes, the first one that has a preceding Fill/Stroke in
// scope triggers the violation.
CLI_TEST(PAGXCliTest, Validate_MultipleMergePaths) {
  auto path = BadCase("validate_fmt_mergepath_multiple.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(errOutput.find("Fill/Stroke-before-MergePath") != std::string::npos);
}

// TextBox layout override — non-Start textAnchor with TextBox present

// Expected: validate fails (non-zero exit). Observable: error message contains "will be ignored
// because TextBox". A non-Start textAnchor (e.g., Center/End) has no effect when TextBox is
// present — TextBox fully controls text layout, making the anchor attribute misleading.
CLI_TEST(PAGXCliTest, Validate_TextBoxOverridesAnchor) {
  auto path = BadCase("validate_fmt_textbox_overrides_anchor.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(errOutput.find("will-be-ignored-because-TextBox") != std::string::npos);
}

// TextBox layout override — explicit position with TextBox present

// Expected: validate fails (non-zero exit). Observable: error message contains "will be ignored
// because TextBox". An explicit text position attribute is silently discarded when TextBox is
// active; authors typically don't realize their position value has no effect.
CLI_TEST(PAGXCliTest, Validate_TextBoxOverridesTextPosition) {
  auto path = BadCase("validate_fmt_textbox_overrides_text_position.pagx");
  std::string errOutput;
  std::streambuf* oldCerr = std::cerr.rdbuf();
  std::ostringstream oss;
  std::cerr.rdbuf(oss.rdbuf());
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  std::cerr.rdbuf(oldCerr);
  errOutput = oss.str();
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(errOutput.find("will-be-ignored-because-TextBox") != std::string::npos);
}

}  // namespace pag
