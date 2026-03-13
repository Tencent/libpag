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

#include <gtest/gtest.h>
#include <memory>
#include "pagx/AutoLayout.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/LayoutDirection.h"

using namespace pagx;

class AutoLayoutTest : public testing::Test {};

TEST_F(AutoLayoutTest, ContainerLayout_HorizontalEqualWidth) {
  // Test three equal-width cards arranged horizontally
  auto doc = PAGXDocument::Make(920, 200);
  auto parent = doc->makeNode<Layer>();
  doc->layers.push_back(parent);

  parent->width = 920;
  parent->height = 200;
  parent->layout = LayoutDirection::Horizontal;
  parent->gap = 14;
  parent->padding = Padding{20, 20, 20, 20};

  auto card1 = doc->makeNode<Layer>();
  auto card2 = doc->makeNode<Layer>();
  auto card3 = doc->makeNode<Layer>();

  parent->children = {card1, card2, card3};

  // Create content for each card
  for (auto* card : {card1, card2, card3}) {
    auto rect = doc->makeNode<Rectangle>();
    rect->size = {100, 100};
    rect->position = {0, 0};
    card->contents.push_back(rect);
  }

  AutoLayout::Apply(doc.get());

  // Expected: width = (920 - 40) - 28 = 852 / 3 = 284
  float expectedChildWidth = 284.0f;

  EXPECT_EQ(card1->width.value_or(0), expectedChildWidth);
  EXPECT_EQ(card2->width.value_or(0), expectedChildWidth);
  EXPECT_EQ(card3->width.value_or(0), expectedChildWidth);

  // Position checks
  EXPECT_EQ(card1->x, 20.0f);
  EXPECT_EQ(card2->x, 20 + 284 + 14);
  EXPECT_EQ(card3->x, 20 + 284 * 2 + 14 * 2);
}

TEST_F(AutoLayoutTest, ContainerLayout_VerticalStart) {
  auto doc = PAGXDocument::Make(400, 600);
  auto parent = doc->makeNode<Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 600;
  parent->layout = LayoutDirection::Vertical;
  parent->gap = 10;

  auto child1 = doc->makeNode<Layer>();
  auto child2 = doc->makeNode<Layer>();

  parent->children = {child1, child2};

  // Set fixed heights
  child1->height = 100;
  child2->height = 150;

  AutoLayout::Apply(doc.get());

  EXPECT_EQ(child1->y, 0.0f);
  EXPECT_EQ(child2->y, 110.0f);
}

TEST_F(AutoLayoutTest, ContainerLayout_Arrangement_Center) {
  auto doc = PAGXDocument::Make(400, 200);
  auto parent = doc->makeNode<Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 200;
  parent->layout = LayoutDirection::Horizontal;
  parent->arrangement = Arrangement::Center;
  parent->gap = 0;

  auto child = doc->makeNode<Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 100;

  AutoLayout::Apply(doc.get());

  // Centered: (400 - 100) / 2 = 150
  EXPECT_EQ(child->x, 150.0f);
}

TEST_F(AutoLayoutTest, ContainerLayout_Arrangement_SpaceBetween) {
  auto doc = PAGXDocument::Make(400, 100);
  auto parent = doc->makeNode<Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 100;
  parent->layout = LayoutDirection::Horizontal;
  parent->arrangement = Arrangement::SpaceBetween;
  parent->gap = 0;

  auto child1 = doc->makeNode<Layer>();
  auto child2 = doc->makeNode<Layer>();

  parent->children = {child1, child2};

  child1->width = 50;
  child1->height = 50;
  child2->width = 50;
  child2->height = 50;

  AutoLayout::Apply(doc.get());

  // SpaceBetween: first at 0, second at 350
  EXPECT_EQ(child1->x, 0.0f);
  EXPECT_EQ(child2->x, 350.0f);
}

TEST_F(AutoLayoutTest, ContainerLayout_Alignment_Center) {
  auto doc = PAGXDocument::Make(400, 300);
  auto parent = doc->makeNode<Layer>();
  doc->layers.push_back(parent);

  parent->width = 400;
  parent->height = 300;
  parent->layout = LayoutDirection::Horizontal;
  parent->alignment = Alignment::Center;

  auto child = doc->makeNode<Layer>();
  parent->children = {child};

  child->width = 100;
  child->height = 50;

  AutoLayout::Apply(doc.get());

  // Centered on cross-axis: (300 - 50) / 2 = 125
  EXPECT_EQ(child->y, 125.0f);
}

TEST_F(AutoLayoutTest, ConstraintLayout_Left) {
  auto doc = PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->constraints.left = 20;

  layer->contents.push_back(rect);

  AutoLayout::Apply(doc.get());

  // Rectangle should be translated so its left edge is at 20
  // Original bounds: left = -50, so tx = 20 - (-50) = 70
  EXPECT_FLOAT_EQ(rect->position.x, 70.0f);
}

TEST_F(AutoLayoutTest, ConstraintLayout_LeftRight_Rectangle) {
  auto doc = PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<Rectangle>();
  rect->size = {100, 50};
  rect->position = {0, 0};
  rect->constraints.left = 10;
  rect->constraints.right = 10;

  layer->contents.push_back(rect);

  AutoLayout::Apply(doc.get());

  // Rectangle should stretch: width = 400 - 10 - 10 = 380
  // Center = 10 + 380/2 = 200
  EXPECT_FLOAT_EQ(rect->size.width, 380.0f);
  EXPECT_FLOAT_EQ(rect->position.x, 200.0f);
}

TEST_F(AutoLayoutTest, ConstraintLayout_CenterX) {
  auto doc = PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto rect = doc->makeNode<Rectangle>();
  rect->size = {100, 50};
  rect->position = {100, 100};
  rect->constraints.centerX = 0;

  layer->contents.push_back(rect);

  AutoLayout::Apply(doc.get());

  // Centered: container center = 200, offset = 0, so position.x = 200
  EXPECT_FLOAT_EQ(rect->position.x, 200.0f);
}

TEST_F(AutoLayoutTest, ConstraintLayout_TopBottom_TextBox) {
  auto doc = PAGXDocument::Make(400, 200);
  auto layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 200;

  auto textBox = doc->makeNode<TextBox>();
  textBox->size = {100, 50};
  textBox->position = {0, 0};
  textBox->constraints.top = 20;
  textBox->constraints.bottom = 20;

  layer->contents.push_back(textBox);

  AutoLayout::Apply(doc.get());

  // TextBox should stretch: height = 200 - 20 - 20 = 160
  // Position: y = 20 (since textBox anchors at top-left)
  EXPECT_FLOAT_EQ(textBox->size.height, 160.0f);
  EXPECT_FLOAT_EQ(textBox->position.y, 20.0f);
}

TEST_F(AutoLayoutTest, Group_DerivedLayoutSize) {
  auto doc = PAGXDocument::Make(400, 300);
  auto layer = doc->makeNode<Layer>();
  doc->layers.push_back(layer);

  layer->width = 400;
  layer->height = 300;

  auto group = doc->makeNode<Group>();
  group->position = {0, 0};
  group->constraints.left = 20;
  group->constraints.right = 20;
  group->constraints.top = 30;
  group->constraints.bottom = 30;

  layer->contents.push_back(group);

  AutoLayout::Apply(doc.get());

  // Group should derive layout size from constraints
  // width = 400 - 20 - 20 = 360
  // height = 300 - 30 - 30 = 240
  EXPECT_EQ(group->width.value_or(0), 360.0f);
  EXPECT_EQ(group->height.value_or(0), 240.0f);
}
