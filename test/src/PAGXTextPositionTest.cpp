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
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/TextLayout.h"
#include "pagx/utils/TextUtils.h"
#include "utils/ProjectPath.h"

namespace pag {

PAG_TEST(PAGXTextPositionTest, XMLImportReportsInvalidPositionsWithoutDiscardingReadableText) {
  for (const auto* value : {"0", "0,1,2", "0,nan", "0,1,", "0 1px"}) {
    SCOPED_TRACE(value);
    auto doc = pagx::PAGXImporter::FromXML(
        std::string(
            "<pagx width='100' height='100'><Layer><Text id='t' text='一二' inlinePositions='") +
        value + "'/></Layer></pagx>");
    ASSERT_NE(doc, nullptr);
    ASSERT_FALSE(doc->errors.empty());
    EXPECT_NE(doc->errors[0].find("Invalid inlinePositions"), std::string::npos);
    auto* node = doc->findNode("t");
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->nodeType(), pagx::NodeType::Text);
    EXPECT_EQ(static_cast<pagx::Text*>(node)->text, "一二");
    EXPECT_TRUE(static_cast<pagx::Text*>(node)->inlinePositions.empty());
  }
}

PAG_TEST(PAGXTextPositionTest, MixedPositionedAndNaturalRunsContinueFromTheMeasuredExtent) {
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0,
                     "PositionFixture", "Regular");
  pagx::LayoutContext context(&fonts);
  for (auto mode : {pagx::WritingMode::Horizontal, pagx::WritingMode::Vertical}) {
    for (float scale : {1.f, 2.f}) {
      SCOPED_TRACE(static_cast<int>(mode));
      SCOPED_TRACE(scale);
      auto doc = pagx::PAGXDocument::Make(300, 300);
      auto* before = doc->makeNode<pagx::Text>();
      auto* positioned = doc->makeNode<pagx::Text>();
      auto* after = doc->makeNode<pagx::Text>();
      for (auto* text : {before, positioned, after}) {
        text->fontFamily = "PositionFixture";
        text->fontSize = 20;
      }
      before->text = "一";
      positioned->text = "二三";
      positioned->inlinePositions = {0, 50};
      after->text = "四";
      pagx::TextLayoutParams params;
      params.writingMode = mode;
      params.textScale = scale;
      auto result = pagx::TextLayout::Layout(
          pagx::TextLayout::MakeElements({before, positioned, after}), params, &context);
      auto* a = result.getGlyphRuns(before);
      auto* b = result.getGlyphRuns(positioned);
      auto* c = result.getGlyphRuns(after);
      ASSERT_NE(a, nullptr);
      ASSERT_NE(b, nullptr);
      ASSERT_NE(c, nullptr);
      ASSERT_EQ(a->size(), 1u);
      ASSERT_EQ(b->size(), 1u);
      ASSERT_EQ(c->size(), 1u);
      ASSERT_EQ(a->front().positions.size(), 1u);
      ASSERT_EQ(b->front().positions.size(), 2u);
      ASSERT_EQ(c->front().positions.size(), 1u);
      auto coordinate = [mode](const tgfx::Point& point) {
        return mode == pagx::WritingMode::Horizontal ? point.x : point.y;
      };
      auto origin = coordinate(a->front().positions[0]);
      EXPECT_NEAR(coordinate(b->front().positions[0]) - origin, 20 * scale, 1e-3f);
      EXPECT_NEAR(coordinate(b->front().positions[1]) - origin, 70 * scale, 1e-3f);
      EXPECT_NEAR(coordinate(c->front().positions[0]) - origin, 90 * scale, 1e-3f);
      EXPECT_NEAR(
          mode == pagx::WritingMode::Horizontal ? result.bounds.width : result.bounds.height,
          110 * scale, 1e-3f);
    }
  }
}

PAG_TEST(PAGXTextPositionTest, InvalidOrStaleInlinePositionsFallBackToNaturalLayout) {
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0,
                     "PositionFixture", "Regular");
  pagx::LayoutContext context(&fonts);
  auto doc = pagx::PAGXDocument::Make(300, 300);
  auto* text = doc->makeNode<pagx::Text>();
  text->fontFamily = "PositionFixture";
  text->fontSize = 20;
  for (auto mode : {pagx::WritingMode::Horizontal, pagx::WritingMode::Vertical}) {
    pagx::TextLayoutParams params;
    params.writingMode = mode;
    for (const auto* value : {"一二", "一\t二", "一\n二", "一\r二"}) {
      SCOPED_TRACE(value);
      text->text = value;
      text->inlinePositions.clear();
      auto natural =
          pagx::TextLayout::Layout(pagx::TextLayout::MakeElements({text}), params, &context);
      auto* expected = natural.getGlyphRuns(text);
      ASSERT_NE(expected, nullptr);
      for (const auto& invalid :
           std::vector<std::vector<float>>{{0},
                                           {0, NAN},
                                           {0, std::numeric_limits<float>::infinity()},
                                           {0, 80, 160}}) {
        text->inlinePositions = invalid;
        auto result =
            pagx::TextLayout::Layout(pagx::TextLayout::MakeElements({text}), params, &context);
        EXPECT_EQ(result.bounds, natural.bounds);
        auto* actual = result.getGlyphRuns(text);
        ASSERT_NE(actual, nullptr);
        ASSERT_EQ(actual->size(), expected->size());
        for (size_t i = 0; i < actual->size(); ++i) {
          EXPECT_EQ((*actual)[i].glyphs, (*expected)[i].glyphs);
          EXPECT_EQ((*actual)[i].positions, (*expected)[i].positions);
        }
      }
    }
  }
}

PAG_TEST(PAGXTextPositionTest, InlinePositionParsingCountsUnicodeScalarsAndClearsRejectedValues) {
  std::vector<float> positions;
  ASSERT_TRUE(pagx::ParseTextInlinePositions("A😀中", " -2.5, 10  30.25 ", &positions));
  EXPECT_EQ(positions, (std::vector<float>{-2.5f, 10, 30.25f}));
  ASSERT_TRUE(pagx::ParseTextInlinePositions("á", "0,0", &positions));
  EXPECT_EQ(positions.size(), 2u);
  for (const auto* value : {"", "0", "0,1,2,3", "0,,1", "0,1,", "0 1 nan", "0 1 inf", "0 1 2px",
                            "0;1;2", "0 1 1e999"}) {
    SCOPED_TRACE(value);
    positions = {99};
    EXPECT_FALSE(pagx::ParseTextInlinePositions("A😀中", value, &positions));
    EXPECT_TRUE(positions.empty());
  }
  for (const auto& value : std::vector<std::string>{"", "a\nb", "a\tb", std::string("\xff", 1)}) {
    positions = {99};
    EXPECT_FALSE(pagx::ParseTextInlinePositions(value, "0,1,2", &positions));
    EXPECT_TRUE(positions.empty());
  }
}

}  // namespace pag
