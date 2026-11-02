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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits>
#include "base/PAGTest.h"
#include "pagx/html/importer/HTMLBackgroundLayout.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLObjectLayout.h"
#include "pagx/html/importer/HTMLValueParser.h"

namespace pag {
namespace {

struct GeometryInput {
  float width = 100;
  float height = 80;
  pagx::HTMLDiagnosticSink diagnostics{false};
  pagx::HTMLValueParser parser{diagnostics, width, height};
};

}  // namespace

PAG_TEST(PAGXHTMLGeometryTest, AbsoluteUnitsAndNestedArithmeticPreserveFractionalLengths) {
  GeometryInput input;
  for (const auto* value : {"96px", "72pt", "6pc", "1in", "2.54cm", "25.4mm", "101.6q"}) {
    SCOPED_TRACE(value);
    pagx::HTMLLengthPercentage length;
    ASSERT_TRUE(pagx::ParseHTMLLengthPercentage(value, input.parser, length));
    EXPECT_NEAR(length.resolve(123), 96, 1e-4f);
  }
  pagx::HTMLLengthPercentage length;
  ASSERT_TRUE(
      pagx::ParseHTMLLengthPercentage(" CALC((50% - 1.5px) / 2 + 3px * 2) ", input.parser, length));
  EXPECT_FLOAT_EQ(length.resolve(100), 30.25f);
  EXPECT_FLOAT_EQ(length.resolve(-100), -19.75f);
  ASSERT_TRUE(pagx::ParseHTMLLengthPercentage("calc(2 - 2)", input.parser, length));
  EXPECT_FLOAT_EQ(length.resolve(100), 0);
}

PAG_TEST(PAGXHTMLGeometryTest, InvalidLengthExpressionsDoNotAcceptPrefixesOrUnboundedNesting) {
  GeometryInput input;
  for (const auto* value :
       {"", "2", "0x10px", "calc(2px+ 3px)", "calc(2px +3px)", "calc(2px -)", "calc(2px", "(2px)",
        "calc(2px)junk", "nanpx", "inf%", "1e38in", "calc(1e38% * 1e38)"}) {
    SCOPED_TRACE(value);
    pagx::HTMLLengthPercentage length;
    EXPECT_FALSE(pagx::ParseHTMLLengthPercentage(value, input.parser, length));
  }
  std::string nested = "1px";
  for (int i = 0; i < 40; ++i) nested = "calc(" + nested + ")";
  pagx::HTMLLengthPercentage length;
  EXPECT_FALSE(pagx::ParseHTMLLengthPercentage(nested, input.parser, length));
}

PAG_TEST(PAGXHTMLGeometryTest, PositionsResolveKeywordsOffsetsAndNegativeFreeSpace) {
  GeometryInput input;
  struct Case {
    const char* value;
    float x;
    float y;
  };
  // An oversized object leaves -40px horizontally and 60px vertically.
  for (const auto& row : std::vector<Case>{{"left", 0, 30},
                                           {"bottom", -20, 60},
                                           {"top right", -40, 0},
                                           {"center left", 0, 30},
                                           {"25% 75%", -10, 45},
                                           {"right 10px", -40, 10},
                                           {"right 10px bottom 25%", -50, 45},
                                           {"bottom -2.5px left calc(50% + 3px)", -17, 62.5f}}) {
    SCOPED_TRACE(row.value);
    pagx::HTMLCSSPosition position;
    ASSERT_TRUE(pagx::ParseHTMLCSSPosition(row.value, input.parser, position));
    EXPECT_FLOAT_EQ(position.x.resolve(-40), row.x);
    EXPECT_FLOAT_EQ(position.y.resolve(60), row.y);
  }
  for (const auto* value : {"center bottom 10px", "bottom 10px center"}) {
    SCOPED_TRACE(value);
    pagx::HTMLCSSPosition position;
    EXPECT_FALSE(pagx::ParseHTMLCSSPosition(value, input.parser, position));
    ASSERT_TRUE(pagx::ParseHTMLCSSPosition(value, input.parser, position, true));
    EXPECT_FLOAT_EQ(position.x.resolve(-40), -20);
    EXPECT_FLOAT_EQ(position.y.resolve(60), 50);
  }
  for (const auto* value : {"left right", "top bottom", "top 10px", "10px left",
                            "center 10px bottom 5px", "left 1px top 2px extra"}) {
    SCOPED_TRACE(value);
    pagx::HTMLCSSPosition position;
    EXPECT_FALSE(pagx::ParseHTMLCSSPosition(value, input.parser, position, true));
  }
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundListsCycleWithoutSplittingGradientArguments) {
  const std::string gradient = "linear-gradient(red, rgba(0, 0, 255, .5))";
  auto list = " none, " + gradient + ", url('a,b.png') ";
  EXPECT_EQ(pagx::BackgroundListEntry(list, 0, "auto"), "none");
  EXPECT_EQ(pagx::BackgroundListEntry(list, 4, "auto"), gradient);
  EXPECT_EQ(pagx::BackgroundListEntry(list, 5, "auto"), "url('a,b.png')");
  EXPECT_EQ(pagx::BackgroundListEntry("", 10, "padding-box"), "padding-box");
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundBoxesSubtractBorderAndAsymmetricPadding) {
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100.5f;
  box.heightPx = 80.25f;
  box.borderWidthPx = 2.5f;
  box.padding = {3, 5, 7, 11};
  auto border = pagx::BackgroundBoxRect(box, "border-box");
  EXPECT_FLOAT_EQ(border.x, 0);
  EXPECT_FLOAT_EQ(border.width, 100.5f);
  auto padding = pagx::BackgroundBoxRect(box, "padding-box");
  EXPECT_FLOAT_EQ(padding.x, 2.5f);
  EXPECT_FLOAT_EQ(padding.width, 95.5f);
  EXPECT_FLOAT_EQ(padding.height, 75.25f);
  auto content = pagx::BackgroundBoxRect(box, "content-box");
  EXPECT_FLOAT_EQ(content.x, 13.5f);
  EXPECT_FLOAT_EQ(content.y, 5.5f);
  EXPECT_FLOAT_EQ(content.width, 79.5f);
  EXPECT_FLOAT_EQ(content.height, 65.25f);
  box.widthPx = box.heightPx = 1;
  content = pagx::BackgroundBoxRect(box, "content-box");
  EXPECT_FLOAT_EQ(content.width, 0);
  EXPECT_FLOAT_EQ(content.height, 0);
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundSizeUsesIntrinsicRatioOnlyWhenAvailable) {
  GeometryInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100;
  box.heightPx = 80;
  box.backgroundRepeat = "no-repeat";
  struct Case {
    const char* size;
    float naturalWidth;
    float naturalHeight;
    float width;
    float height;
  };
  for (const auto& row : std::vector<Case>{{"auto", 40, 20, 40, 20},
                                           {"contain", 40, 20, 100, 50},
                                           {"cover", 40, 20, 160, 80},
                                           {"25% auto", 40, 20, 25, 12.5f},
                                           {"auto 25%", 40, 20, 40, 20},
                                           {"calc(50% - 2.5px) 10px", 40, 20, 47.5f, 10},
                                           {"auto", 0, 0, 100, 80},
                                           {"contain", 0, 0, 100, 80},
                                           {"cover", 0, 0, 100, 80},
                                           {"auto 20px", 0, 0, 100, 20},
                                           {"20px auto", 0, 0, 20, 80}}) {
    SCOPED_TRACE(row.size);
    SCOPED_TRACE(row.naturalWidth);
    box.backgroundSize = row.size;
    pagx::HTMLBackgroundLayout layout;
    ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, row.naturalWidth, row.naturalHeight,
                                              input.parser, layout));
    EXPECT_FLOAT_EQ(layout.x.size, row.width);
    EXPECT_FLOAT_EQ(layout.y.size, row.height);
    EXPECT_EQ(layout.x.count, 1);
    EXPECT_EQ(layout.y.count, 1);
  }
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundSpaceAndRoundFollowDifferentTilePlacementRules) {
  GeometryInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100;
  box.heightPx = 80;
  box.backgroundSize = "30px auto";
  box.backgroundRepeat = "round no-repeat";
  pagx::HTMLBackgroundLayout layout;
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  EXPECT_NEAR(layout.x.size, 100.f / 3, 1e-5f);
  EXPECT_NEAR(layout.y.size, 50.f / 3, 1e-5f);
  EXPECT_EQ(layout.x.count, 3);
  EXPECT_TRUE(layout.x.contiguous);
  box.backgroundSize = "30px 30px";
  box.backgroundRepeat = "space no-repeat";
  box.backgroundPosition = "right bottom";
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 0, 0, input.parser, layout));
  EXPECT_FLOAT_EQ(layout.x.first, 0);
  EXPECT_FLOAT_EQ(layout.x.step, 35);
  EXPECT_EQ(layout.x.count, 3);
  EXPECT_FALSE(layout.x.contiguous);
  EXPECT_FLOAT_EQ(layout.y.first, 50);
  box.backgroundSize = "60px 30px";
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 0, 0, input.parser, layout));
  EXPECT_EQ(layout.x.count, 1);
  EXPECT_FLOAT_EQ(layout.x.first, 40);
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundRepeatCoversClipBeyondPositioningBox) {
  GeometryInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPx = box.heightPx = 100;
  box.borderWidthPx = 10;
  box.backgroundOrigin = "padding-box";
  box.backgroundClip = "border-box";
  box.backgroundSize = "20px 20px";
  box.backgroundRepeat = "repeat-x";
  pagx::HTMLBackgroundLayout layout;
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 0, 0, input.parser, layout));
  EXPECT_FLOAT_EQ(layout.x.first, -10);
  EXPECT_EQ(layout.x.count, 6);
  EXPECT_FLOAT_EQ(layout.y.first, 10);
  EXPECT_EQ(layout.y.count, 1);
  box.backgroundRepeat = "repeat-y";
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 0, 0, input.parser, layout));
  EXPECT_EQ(layout.x.count, 1);
  EXPECT_FLOAT_EQ(layout.x.first, 10);
  EXPECT_EQ(layout.y.count, 6);
  EXPECT_FLOAT_EQ(layout.y.first, -10);
}

PAG_TEST(PAGXHTMLGeometryTest, BackgroundRejectsInvalidGeometryBeforeExpandingTiles) {
  GeometryInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100;
  box.heightPx = 80;
  pagx::HTMLBackgroundLayout layout;
  for (const auto* size :
       {"-1px 20px", "10px -1px", "1px 2px 3px", "banana", "auto 1em", "0.00001px 1px"}) {
    SCOPED_TRACE(size);
    box.backgroundSize = size;
    EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  }
  box.backgroundSize = "10px";
  box.backgroundPosition = "left right";
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  box.backgroundPosition.clear();
  box.backgroundRepeat = "repeat no-repeat space";
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  box.backgroundRepeat.clear();
  box.backgroundOrigin = "text";
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  box.backgroundOrigin.clear();
  box.backgroundClip = "text";
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
  box.backgroundClip.clear();
  box.widthPx = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.parser, layout));
}

PAG_TEST(PAGXHTMLGeometryTest, ObjectFitRespectsMissingIntrinsicDimensionsAndScaleDown) {
  struct Case {
    pagx::HTMLImageIntrinsicSize intrinsic;
    const char* fit;
    float width;
    float height;
  };
  for (const auto& row : std::vector<Case>{{{40, 20}, "fill", 100, 80},
                                           {{40, 20}, "contain", 100, 50},
                                           {{40, 20}, "cover", 160, 80},
                                           {{20, 40}, "cover", 100, 200},
                                           {{40, 20}, "none", 40, 20},
                                           {{40, 20}, "scale-down", 40, 20},
                                           {{400, 200}, "scale-down", 100, 50},
                                           {{}, "none", 100, 80},
                                           {{NAN, NAN, 2}, "none", 100, 50},
                                           {{40, NAN, 2}, "none", 40, 20},
                                           {{NAN, 20, 2}, "none", 40, 20},
                                           {{40, NAN}, "none", 40, 80},
                                           {{NAN, 20}, "none", 100, 20},
                                           {{}, "cover", 100, 80},
                                           {{}, "contain", 100, 80}}) {
    SCOPED_TRACE(row.fit);
    SCOPED_TRACE(row.intrinsic.width);
    SCOPED_TRACE(row.intrinsic.height);
    float width = -1, height = -1;
    ASSERT_TRUE(pagx::ResolveHTMLObjectSize(row.intrinsic, row.fit, 100, 80, width, height));
    EXPECT_FLOAT_EQ(width, row.width);
    EXPECT_FLOAT_EQ(height, row.height);
  }
}

PAG_TEST(PAGXHTMLGeometryTest, ObjectPositionOffsetsAreRelativeToSignedFreeSpace) {
  GeometryInput input;
  pagx::HTMLCSSPosition position;
  ASSERT_TRUE(pagx::ParseHTMLCSSPosition("right 5px bottom 25%", input.parser, position));
  pagx::Rect object;
  ASSERT_TRUE(pagx::ResolveHTMLObjectRect({40, 20}, "cover", position,
                                          pagx::Rect::MakeXYWH(10, 20, 100, 80), object));
  EXPECT_FLOAT_EQ(object.x, -55);
  EXPECT_FLOAT_EQ(object.y, 20);
  EXPECT_FLOAT_EQ(object.width, 160);
  EXPECT_FLOAT_EQ(object.height, 80);
  position.x.pixels = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(pagx::ResolveHTMLObjectRect({40, 20}, "cover", position,
                                           pagx::Rect::MakeXYWH(0, 0, 100, 80), object));
}

PAG_TEST(PAGXHTMLGeometryTest, ObjectSizingHandlesZeroAndRejectsInvalidContainers) {
  float width = -1, height = -1;
  for (const auto& intrinsic : std::vector<pagx::HTMLImageIntrinsicSize>{{0, 20}, {20, 0}}) {
    ASSERT_TRUE(pagx::ResolveHTMLObjectSize(intrinsic, "cover", 100, 80, width, height));
    EXPECT_FLOAT_EQ(width, 0);
    EXPECT_FLOAT_EQ(height, 0);
  }
  ASSERT_TRUE(pagx::ResolveHTMLObjectSize({40, 20}, "contain", 0, 80, width, height));
  EXPECT_FLOAT_EQ(width, 0);
  EXPECT_FLOAT_EQ(height, 0);
  for (float invalid : {-1.f, NAN, std::numeric_limits<float>::infinity()}) {
    EXPECT_FALSE(pagx::ResolveHTMLObjectSize({40, 20}, "fill", invalid, 80, width, height));
    EXPECT_FALSE(pagx::ResolveHTMLObjectSize({40, 20}, "fill", 100, invalid, width, height));
  }
}

}  // namespace pag
