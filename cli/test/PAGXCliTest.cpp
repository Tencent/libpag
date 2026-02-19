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
#include "gtest/gtest.h"
#include "CommandFormat.h"
#include "CommandOptimize.h"
#include "CommandValidator.h"

namespace pagx::cli {

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

TEST(PAGXCliTest, Validate_ValidFile) {
  auto path = WriteTempFile("valid.pagx", VALID_SIMPLE);
  auto ret = CallRun(RunValidate, {"validate", path});
  EXPECT_EQ(ret, 0);
}

TEST(PAGXCliTest, Validate_MissingAttribute) {
  auto xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0">
  <Layer name="test"/>
</pagx>
)";
  auto path = WriteTempFile("invalid_missing.pagx", xml);
  auto ret = CallRun(RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

TEST(PAGXCliTest, Validate_NonXmlFile) {
  auto path = WriteTempFile("notxml.pagx", "this is not xml at all");
  auto ret = CallRun(RunValidate, {"validate", path});
  EXPECT_NE(ret, 0);
}

//==============================================================================
// Optimize tests
//==============================================================================

TEST(PAGXCliTest, Optimize_RemoveEmptyElements) {
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("\"empty\"") == std::string::npos);
  EXPECT_TRUE(output.find("width=\"0\"") == std::string::npos);
  EXPECT_TRUE(output.find("\"visible\"") != std::string::npos);
}

TEST(PAGXCliTest, Optimize_DeduplicatePathData) {
  // Use a triangle path (not a rectangle/ellipse) to avoid Path→Primitive replacement.
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  size_t count = 0;
  size_t pos = 0;
  while ((pos = output.find("<PathData", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 1u);
}

TEST(PAGXCliTest, Optimize_DeduplicateGradient) {
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  size_t count = 0;
  size_t pos = 0;
  while ((pos = output.find("<LinearGradient", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 1u);
}

TEST(PAGXCliTest, Optimize_RemoveUnreferencedResource) {
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("orphan") == std::string::npos);
}

TEST(PAGXCliTest, Optimize_PathToRectangle) {
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("<Rectangle") != std::string::npos);
  EXPECT_TRUE(output.find("<Path") == std::string::npos);
}

// NOTE: Path→Ellipse detection via isOval() requires exact Bezier control points. The SVG path
// string format used by PAGX (with default float precision) introduces rounding that prevents
// isOval() from matching after an export→import round trip. The optimization still works for
// Path data constructed programmatically with full float precision (e.g., from non-PAGX sources).
// A Path→Ellipse test is omitted here due to this inherent limitation.

TEST(PAGXCliTest, Optimize_FullCanvasClipRemoval) {
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
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("mask=") == std::string::npos);
}

TEST(PAGXCliTest, Optimize_DryRun) {
  auto inputPath = WriteTempFile("dryrun.pagx", VALID_SIMPLE);
  auto originalContent = ReadFile(inputPath);
  auto ret = CallRun(RunOptimize, {"optimize", "--dry-run", inputPath});
  EXPECT_EQ(ret, 0);
  auto afterContent = ReadFile(inputPath);
  EXPECT_EQ(originalContent, afterContent);
}

TEST(PAGXCliTest, Optimize_NoOptimizationsNeeded) {
  auto inputPath = WriteTempFile("already_optimal.pagx", VALID_SIMPLE);
  auto outputPath = TempDir() + "/already_optimal_out.pagx";
  auto ret = CallRun(RunOptimize, {"optimize", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
}

//==============================================================================
// Format tests
//==============================================================================

TEST(PAGXCliTest, Format_Indentation) {
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
  auto ret = CallRun(RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("  <Layer") != std::string::npos);
  EXPECT_TRUE(output.find("    <Rectangle") != std::string::npos);
  EXPECT_TRUE(output.find("    <Fill") != std::string::npos);
}

TEST(PAGXCliTest, Format_AttributeReordering) {
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
  auto ret = CallRun(RunFormat, {"format", "-o", outputPath, inputPath});
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

TEST(PAGXCliTest, Format_PreservesValues) {
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
  auto ret = CallRun(RunFormat, {"format", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("visible=\"true\"") != std::string::npos);
  EXPECT_TRUE(output.find("alpha=\"1\"") != std::string::npos);
  EXPECT_TRUE(output.find("roundness=\"0\"") != std::string::npos);
}

TEST(PAGXCliTest, Format_CustomIndent) {
  auto inputPath = WriteTempFile("indent4.pagx", VALID_SIMPLE);
  auto outputPath = TempDir() + "/indent4_out.pagx";
  auto ret = CallRun(RunFormat, {"format", "--indent", "4", "-o", outputPath, inputPath});
  EXPECT_EQ(ret, 0);
  auto output = ReadFile(outputPath);
  EXPECT_TRUE(output.find("    <Layer") != std::string::npos);
}

}  // namespace pagx::cli

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
