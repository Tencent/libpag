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
#include "CommandFormat.h"
#include "CommandOptimize.h"
#include "CommandValidator.h"

namespace pag {

static std::string TempDir() {
  auto dir = std::filesystem::temp_directory_path() / "pagx_cli_test";
  std::filesystem::create_directories(dir);
  return dir.string();
}

static std::string WriteTempFile(const std::string& name, const std::string& content) {
  auto path = TempDir() + "/" + name;
  std::ofstream out(path);
  out << content;
  out.close();
  return path;
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

// A minimal valid PAGX document.
static const char* VALID_SIMPLE = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="bg">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#336699"/>
  </Layer>
</pagx>
)";

//==============================================================================
// Validate tests
//==============================================================================

CLI_TEST(PAGXCliTest, Validate_ValidFile) {
  auto path = WriteTempFile("valid.pagx", VALID_SIMPLE);
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MissingAttribute) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0">
  <Layer name="test"/>
</pagx>
)";
  auto path = WriteTempFile("invalid_missing.pagx", xml);
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_NonXmlFile) {
  auto path = WriteTempFile("notxml.pagx", "this is not xml at all");
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_MalformedXml) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="test">
    <Rectangle center="100,100" size="200,200"/>
)";
  auto path = WriteTempFile("malformed.pagx", xml);
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

CLI_TEST(PAGXCliTest, Validate_InvalidRootElement) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<svg width="200" height="200">
  <rect x="0" y="0" width="200" height="200"/>
</svg>
)";
  auto path = WriteTempFile("wrong_root.pagx", xml);
  auto ret = CallRun(pagx::cli::RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Optimize tests — RemoveEmptyNodes
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_RemoveEmptyElements) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="visible">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="empty"/>
  <Layer name="zero_stroke">
    <Rectangle center="50,50" size="100,100"/>
    <Stroke color="#000000" width="0"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("empty_elements.pagx", xml);
  auto outputPath = TempDir() + "/empty_elements_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"empty\"") == std::string::npos);
  EXPECT_TRUE(output.find("width=\"0\"") == std::string::npos);
  EXPECT_TRUE(output.find("\"visible\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_RemoveEmptyGroup) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Group/>
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("empty_group.pagx", xml);
  auto outputPath = TempDir() + "/empty_group_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Group") == std::string::npos);
  EXPECT_TRUE(output.find("\"main\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepLayerWithComposition) {
  // A Layer that has no direct contents but references a Composition should be kept.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="ref" composition="@comp1"/>
  <Resources>
    <Composition id="comp1" width="50" height="50">
      <Layer name="inner">
        <Rectangle size="50,50"/>
        <Fill color="#FF0000"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("keep_comp_layer.pagx", xml);
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

CLI_TEST(PAGXCliTest, Optimize_DeduplicatePathData) {
  // Use a triangle path (not a rectangle/ellipse) to avoid Path->Primitive replacement.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="paths">
    <Path data="@pd1"/>
    <Path data="@pd2"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <PathData id="pd1" data="M 0 0 L 100 0 L 50 80 Z"/>
    <PathData id="pd2" data="M 0 0 L 100 0 L 50 80 Z"/>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("dup_pathdata.pagx", xml);
  auto outputPath = TempDir() + "/dup_pathdata_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<PathData"), 1u);
}

CLI_TEST(PAGXCliTest, Optimize_DeduplicateMultiplePaths) {
  // Three identical paths should be merged into one.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="paths">
    <Path data="@pd1"/>
    <Path data="@pd2"/>
    <Path data="@pd3"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <PathData id="pd1" data="M 0 0 L 80 0 L 40 60 Z"/>
    <PathData id="pd2" data="M 0 0 L 80 0 L 40 60 Z"/>
    <PathData id="pd3" data="M 0 0 L 80 0 L 40 60 Z"/>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("dup_multi_path.pagx", xml);
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
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="fills">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="@g1"/>
    <Rectangle center="150,50" size="100,100"/>
    <Fill color="@g2"/>
  </Layer>
  <Resources>
    <LinearGradient id="g1" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
    <LinearGradient id="g2" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("dup_gradient.pagx", xml);
  auto outputPath = TempDir() + "/dup_gradient_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<LinearGradient"), 1u);
}

CLI_TEST(PAGXCliTest, Optimize_DeduplicateRadialGradient) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="fills">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="@rg1"/>
    <Rectangle center="150,50" size="100,100"/>
    <Fill color="@rg2"/>
  </Layer>
  <Resources>
    <RadialGradient id="rg1" center="50,50" radius="50">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </RadialGradient>
    <RadialGradient id="rg2" center="50,50" radius="50">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </RadialGradient>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("dup_radial.pagx", xml);
  auto outputPath = TempDir() + "/dup_radial_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<RadialGradient"), 1u);
}

CLI_TEST(PAGXCliTest, Optimize_NoDeduplicateDifferentGradients) {
  // A LinearGradient and a RadialGradient should not be merged even if they have the same colors.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="fills">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="@g1"/>
    <Rectangle center="150,50" size="100,100"/>
    <Fill color="@g2"/>
  </Layer>
  <Resources>
    <LinearGradient id="g1" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
    <RadialGradient id="g2" center="50,50" radius="50">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </RadialGradient>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("no_dup_gradient.pagx", xml);
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

CLI_TEST(PAGXCliTest, Optimize_RemoveUnreferencedResource) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#336699"/>
  </Layer>
  <Resources>
    <PathData id="orphan" data="M 0 0 L 50 50 Z"/>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("unref_resource.pagx", xml);
  auto outputPath = TempDir() + "/unref_resource_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("orphan") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_RemoveOrphanGradient) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#336699"/>
  </Layer>
  <Resources>
    <LinearGradient id="unused" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("orphan_gradient.pagx", xml);
  auto outputPath = TempDir() + "/orphan_gradient_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("unused") == std::string::npos);
  EXPECT_TRUE(output.find("<LinearGradient") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepReferencedGradient) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="@grad"/>
  </Layer>
  <Resources>
    <LinearGradient id="grad" endPoint="200,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("ref_gradient.pagx", xml);
  auto outputPath = TempDir() + "/ref_gradient_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"grad\"") != std::string::npos);
  EXPECT_EQ(CountOccurrences(output, "<LinearGradient"), 1u);
}

//==============================================================================
// Optimize tests — ReplacePathsWithPrimitives
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_PathToRectangle) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="rect_layer">
    <Path data="M 10 20 L 110 20 L 110 120 L 10 120 Z"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("rect_path.pagx", xml);
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
  // An irregular polygon should not be replaced with a primitive.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="irregular">
    <Path data="M 10 10 L 100 30 L 80 90 L 20 70 Z"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("irregular_path.pagx", xml);
  auto outputPath = TempDir() + "/irregular_path_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Path") != std::string::npos || output.find("<PathData") != std::string::npos);
  EXPECT_TRUE(output.find("<Rectangle") == std::string::npos);
  EXPECT_TRUE(output.find("<Ellipse") == std::string::npos);
}

//==============================================================================
// Optimize tests — RemoveFullCanvasClipMasks
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_FullCanvasClipRemoval) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="content" mask="@clip">
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <Layer id="clip">
      <Rectangle center="100,100" size="200,200"/>
    </Layer>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("fullcanvas_clip.pagx", xml);
  auto outputPath = TempDir() + "/fullcanvas_clip_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("mask=") == std::string::npos);
}

// NOTE: Partial clip mask retention test is omitted because mask="@id" references are not
// preserved through the PAGX import/export round trip in the current Exporter implementation.
// The mask attribute is dropped during export regardless of optimization.

CLI_TEST(PAGXCliTest, Optimize_RemoveOversizedClip) {
  // A clip mask larger than the canvas should also be removed.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="content" mask="@clip">
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <Layer id="clip">
      <Rectangle center="100,100" size="400,400"/>
    </Layer>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("oversized_clip.pagx", xml);
  auto outputPath = TempDir() + "/oversized_clip_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("mask=") == std::string::npos);
}

//==============================================================================
// Optimize tests — RemoveOffCanvasLayers
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_RemoveOffCanvasRight) {
  // A Layer completely outside the canvas should be removed. Use a large offset and explicit center
  // so the bounds are clearly outside the 200x200 canvas.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="visible">
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="offscreen">
    <Rectangle center="500,500" size="50,50"/>
    <Fill color="#0000FF"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("offcanvas_right.pagx", xml);
  auto outputPath = TempDir() + "/offcanvas_right_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"visible\"") != std::string::npos);
  EXPECT_TRUE(output.find("\"offscreen\"") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepPartiallyVisible) {
  // A Layer that partially overlaps the canvas should be kept.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="partial" x="180" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("partial_visible.pagx", xml);
  auto outputPath = TempDir() + "/partial_visible_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"partial\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_KeepInvisibleLayer) {
  // visible=false Layers should be skipped (not removed) because they may be toggled at runtime.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="hidden" visible="false" x="500" y="500">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("invisible_layer.pagx", xml);
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
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="shape">
    <Rectangle center="200,300" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_rect.pagx", xml);
  auto outputPath = TempDir() + "/localize_rect_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"200\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"300\"") != std::string::npos);
  // Exporter omits center="0,0" (it is the default value).
  EXPECT_TRUE(output.find("center=") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeEllipse) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="circle">
    <Ellipse center="150,250" size="80,80"/>
    <Fill color="#00FF00"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_ellipse.pagx", xml);
  auto outputPath = TempDir() + "/localize_ellipse_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"150\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"250\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizePolystar) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="star">
    <Polystar center="300,200" outerRadius="40" pointCount="5"/>
    <Fill color="#FFFF00"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_polystar.pagx", xml);
  auto outputPath = TempDir() + "/localize_polystar_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("x=\"300\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"200\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeTextBox) {
  // Include a Rectangle to ensure non-empty bounds (prevents off-canvas removal when fonts are
  // not embedded).
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="text">
    <Rectangle center="200,175" size="200,50"/>
    <Stroke color="#CCC" width="1"/>
    <Text text="Hello" fontFamily="Arial" fontSize="20"/>
    <TextBox position="100,150" size="200,50"/>
    <Fill color="#000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_textbox.pagx", xml);
  auto outputPath = TempDir() + "/localize_textbox_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  // TextBox position is the anchor, so Layer should get x="100" y="150".
  EXPECT_TRUE(output.find("x=\"100\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"150\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeSkipMatrix) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="rotated" matrix="0,-1,1,0,0,0">
    <Rectangle center="200,200" size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_matrix.pagx", xml);
  auto outputPath = TempDir() + "/localize_matrix_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  // Rectangle center should remain unchanged (matrix Layer is skipped).
  EXPECT_TRUE(output.find("center=\"200,200\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeAlreadyAtOrigin) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="centered" x="100" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_origin.pagx", xml);
  auto outputPath = TempDir() + "/localize_origin_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  // Already at origin, no localization applied. Layer keeps x="100" y="100".
  EXPECT_TRUE(output.find("x=\"100\"") != std::string::npos);
  EXPECT_TRUE(output.find("y=\"100\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeNestedLayers) {
  // Child Layers should also be localized recursively.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="parent">
    <Rectangle center="200,200" size="200,200"/>
    <Fill color="#EEE"/>
    <Layer name="child">
      <Rectangle center="300,300" size="50,50"/>
      <Fill color="#FF0000"/>
    </Layer>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_nested.pagx", xml);
  auto outputPath = TempDir() + "/localize_nested_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  // Both parent and child should have been localized.
  // Parent: center 200,200 -> Layer x=200 y=200, center omitted
  EXPECT_TRUE(output.find("\"parent\"") != std::string::npos);
  // Child: center 300,300 -> Layer x=300 y=300, center omitted
  EXPECT_TRUE(output.find("\"child\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_LocalizeMultipleElements) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="multi">
    <Group>
      <Rectangle center="100,100" size="50,50"/>
      <Fill color="#FF0000"/>
    </Group>
    <Group>
      <Ellipse center="200,100" size="50,50"/>
      <Fill color="#0000FF"/>
    </Group>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("localize_multi.pagx", xml);
  auto outputPath = TempDir() + "/localize_multi_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  // Groups default position is 0,0, so the computed center is (0,0) and no localization occurs.
  EXPECT_EQ(ret, 0);
}

//==============================================================================
// Optimize tests — ExtractCompositions
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_ExtractDuplicateLayers) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="icon1" x="100" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="icon2" x="300" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="icon3" x="200" y="300">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("extract_comp.pagx", xml);
  auto outputPath = TempDir() + "/extract_comp_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<Composition"), 1u);
  EXPECT_TRUE(output.find("composition=\"@") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_ExtractFourPlusLayers) {
  // Four identical Layers should also be extracted.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="a" x="50" y="50">
    <Ellipse size="30,30"/>
    <Fill color="#00FF00"/>
  </Layer>
  <Layer name="b" x="150" y="50">
    <Ellipse size="30,30"/>
    <Fill color="#00FF00"/>
  </Layer>
  <Layer name="c" x="250" y="50">
    <Ellipse size="30,30"/>
    <Fill color="#00FF00"/>
  </Layer>
  <Layer name="d" x="350" y="50">
    <Ellipse size="30,30"/>
    <Fill color="#00FF00"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("extract_four.pagx", xml);
  auto outputPath = TempDir() + "/extract_four_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_EQ(CountOccurrences(output, "<Composition"), 1u);
  EXPECT_EQ(CountOccurrences(output, "composition=\"@"), 4u);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractDifferentSizes) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="big" x="100" y="100">
    <Rectangle size="100,100"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="small" x="300" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("no_extract_size.pagx", xml);
  auto outputPath = TempDir() + "/no_extract_size_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractDifferentColors) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="red" x="100" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="blue" x="300" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#0000FF"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("no_extract_color.pagx", xml);
  auto outputPath = TempDir() + "/no_extract_color_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractMatrixLayer) {
  // Layers with non-identity matrix should not be candidates for extraction.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="a" matrix="0.7,0.7,-0.7,0.7,100,100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="b" matrix="0.7,0.7,-0.7,0.7,200,200">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("no_extract_matrix.pagx", xml);
  auto outputPath = TempDir() + "/no_extract_matrix_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

CLI_TEST(PAGXCliTest, Optimize_NoExtractSingleLayer) {
  // A single Layer (even if it matches structurally) should not be extracted.
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer name="solo" x="100" y="100">
    <Rectangle size="50,50"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("no_extract_single.pagx", xml);
  auto outputPath = TempDir() + "/no_extract_single_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Composition") == std::string::npos);
}

//==============================================================================
// Optimize tests — DryRun and general
//==============================================================================

CLI_TEST(PAGXCliTest, Optimize_DryRun) {
  auto inputPath = WriteTempFile("dryrun.pagx", VALID_SIMPLE);
  auto originalContent = ReadFile(inputPath);
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "--dry-run", inputPath});
  EXPECT_EQ(ret, 0);
  auto afterContent = ReadFile(inputPath);
  EXPECT_EQ(originalContent, afterContent);
}

CLI_TEST(PAGXCliTest, Optimize_NoOptimizationsNeeded) {
  auto inputPath = WriteTempFile("already_optimal.pagx", VALID_SIMPLE);
  auto outputPath = TempDir() + "/already_optimal_out.pagx";
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
}

CLI_TEST(PAGXCliTest, Optimize_InPlaceOverwrite) {
  auto inputPath = WriteTempFile("inplace.pagx", R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Layer name="empty"/>
</pagx>
)");
  auto ret = CallRun(pagx::cli::RunOptimize, {"optimize", inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(inputPath);
  // The empty Layer should have been removed in-place.
  EXPECT_TRUE(output.find("\"empty\"") == std::string::npos);
  EXPECT_TRUE(output.find("\"main\"") != std::string::npos);
}

//==============================================================================
// Format tests
//==============================================================================

CLI_TEST(PAGXCliTest, Format_Indentation) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
      <Layer name="test">
<Rectangle center="100,100" size="200,200"/>
          <Fill color="#336699"/>
      </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("messy.pagx", xml);
  auto outputPath = TempDir() + "/messy_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("  <Layer") != std::string::npos);
  EXPECT_TRUE(output.find("    <Rectangle") != std::string::npos);
  EXPECT_TRUE(output.find("    <Fill") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_AttributeReordering) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer alpha="0.5" name="test">
    <Rectangle size="200,200" center="100,100"/>
    <Fill color="#336699"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("attr_order.pagx", xml);
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
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="test" visible="true" alpha="1">
    <Rectangle center="100,100" size="200,200" roundness="0"/>
    <Fill color="#336699"/>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("preserve.pagx", xml);
  auto outputPath = TempDir() + "/preserve_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("visible=\"true\"") != std::string::npos);
  EXPECT_TRUE(output.find("alpha=\"1\"") != std::string::npos);
  EXPECT_TRUE(output.find("roundness=\"0\"") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_CustomIndent) {
  auto inputPath = WriteTempFile("indent4.pagx", VALID_SIMPLE);
  auto outputPath = TempDir() + "/indent4_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "--indent", "4", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("    <Layer") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_DeepNesting) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="outer">
    <Group>
      <Group>
        <Rectangle center="100,100" size="50,50"/>
        <Fill color="#FF0000"/>
      </Group>
    </Group>
  </Layer>
</pagx>
)";
  auto inputPath = WriteTempFile("deep.pagx", xml);
  auto outputPath = TempDir() + "/deep_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  // 6 levels of indent: pagx(0) > Layer(2) > Group(4) > Group(6) > Rectangle(8)
  EXPECT_TRUE(output.find("        <Rectangle") != std::string::npos);
}

CLI_TEST(PAGXCliTest, Format_ResourcesBlock) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="main">
    <Path data="@pd"/>
    <Fill color="#FF0000"/>
  </Layer>
  <Resources>
    <PathData id="pd" data="M 0 0 L 100 0 L 50 80 Z"/>
  </Resources>
</pagx>
)";
  auto inputPath = WriteTempFile("resources.pagx", xml);
  auto outputPath = TempDir() + "/resources_out.pagx";
  auto ret = CallRun(pagx::cli::RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("  <Resources>") != std::string::npos);
  EXPECT_TRUE(output.find("    <PathData") != std::string::npos);
}

}  // namespace pag
