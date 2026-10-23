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

#include "base/PAGTest.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Rectangle.h"

namespace pag {

PAG_TEST(PAGXLayoutRoundingTest, NestedPoliciesRestoreTheEnclosingPolicy) {
  pagx::LayoutContext context(nullptr);
  EXPECT_FLOAT_EQ(context.roundLayoutPosition(-1.25f), -1);
  EXPECT_FLOAT_EQ(context.roundLayoutSize(1.25f), 2);
  {
    pagx::ScopedLayoutRounding fractional(&context, pagx::LayoutRounding::None);
    EXPECT_FLOAT_EQ(context.roundLayoutPosition(-1.25f), -1.25f);
    EXPECT_FLOAT_EQ(context.roundLayoutSize(1.25f), 1.25f);
    {
      pagx::ScopedLayoutRounding inherit(&context, pagx::LayoutRounding::Inherit);
      EXPECT_EQ(context.layoutRounding, pagx::LayoutRounding::None);
      {
        pagx::ScopedLayoutRounding pixel(&context, pagx::LayoutRounding::Pixel);
        EXPECT_FLOAT_EQ(context.roundLayoutPosition(-1.5f), -2);
        EXPECT_FLOAT_EQ(context.roundLayoutSize(1.25f), 2);
      }
      EXPECT_EQ(context.layoutRounding, pagx::LayoutRounding::None);
    }
    EXPECT_EQ(context.layoutRounding, pagx::LayoutRounding::None);
  }
  EXPECT_EQ(context.layoutRounding, pagx::LayoutRounding::Pixel);
  pagx::ScopedLayoutRounding noContext(nullptr, pagx::LayoutRounding::None);
  EXPECT_EQ(context.layoutRounding, pagx::LayoutRounding::Pixel);
}

PAG_TEST(PAGXLayoutRoundingTest,
         ReparentingRecomputesInheritedRoundingWithoutChangingAuthoredValues) {
  auto doc = pagx::PAGXImporter::FromXML(R"(<pagx width="200" height="100">
    <Layer id="fractional" width="80.25" height="40.125" layoutRounding="none">
      <Layer id="moving" width="50%" height="50%" left=".375" top=".625">
        <Rectangle id="shape" width="100%" height="100%"/>
      </Layer>
    </Layer>
    <Layer id="pixel" width="80.25" height="40.125" layoutRounding="pixel"/>
  </pagx>)");
  ASSERT_NE(doc, nullptr);
  auto* fractional = static_cast<pagx::Layer*>(doc->findNode("fractional"));
  auto* pixel = static_cast<pagx::Layer*>(doc->findNode("pixel"));
  auto* moving = static_cast<pagx::Layer*>(doc->findNode("moving"));
  auto* shape = static_cast<pagx::Rectangle*>(doc->findNode("shape"));
  ASSERT_NE(fractional, nullptr);
  ASSERT_NE(pixel, nullptr);
  ASSERT_NE(moving, nullptr);
  ASSERT_NE(shape, nullptr);
  for (int iteration = 0; iteration < 3; ++iteration) {
    doc->applyLayout();
    EXPECT_FLOAT_EQ(moving->layoutX, .375f);
    EXPECT_FLOAT_EQ(moving->layoutY, .625f);
    EXPECT_FLOAT_EQ(shape->layoutWidth, 40.125f);
    EXPECT_FLOAT_EQ(shape->layoutHeight, 20.0625f);
    fractional->children.clear();
    pixel->children = {moving};
    doc->applyLayout();
    EXPECT_FLOAT_EQ(moving->layoutX, 0);
    EXPECT_FLOAT_EQ(moving->layoutY, 1);
    EXPECT_FLOAT_EQ(shape->layoutWidth, 41);
    EXPECT_FLOAT_EQ(shape->layoutHeight, 21);
    EXPECT_FLOAT_EQ(moving->left, .375f);
    EXPECT_FLOAT_EQ(moving->top, .625f);
    EXPECT_FLOAT_EQ(moving->percentWidth, 50);
    EXPECT_EQ(moving->layoutRounding, pagx::LayoutRounding::Inherit);
    pixel->children.clear();
    fractional->children = {moving};
  }
}

}  // namespace pag
