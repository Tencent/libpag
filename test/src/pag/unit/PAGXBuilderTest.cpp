// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 3 smoke tests for the fluent PAGXBuilder helper. Per the design doc
// §19 Phase 2 deliverable note (≥13 fluent assertions), every chainable
// setter exercised at least once.
#include <memory>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"

namespace pagx::test {

TEST(PAGXBuilder, MakeAppliesLayoutOnDone) {
  auto doc = PAGXBuilder::Make(800, 600).Done();
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(doc->isLayoutApplied());
  EXPECT_EQ(doc->width, 800.0f);
  EXPECT_EQ(doc->height, 600.0f);
}

TEST(PAGXBuilder, RawDocumentSkipsLayout) {
  auto builder = PAGXBuilder::Make();
  auto doc = builder.RawDocument();
  EXPECT_FALSE(doc->isLayoutApplied());
}

TEST(PAGXBuilder, AddLayerNameAndPosition) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().Name("hero").Position(120.0f, 240.0f);
  auto doc = builder.Done();
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0]->name, "hero");
  EXPECT_FLOAT_EQ(doc->layers[0]->x, 120.0f);
  EXPECT_FLOAT_EQ(doc->layers[0]->y, 240.0f);
}

TEST(PAGXBuilder, AddLayerVisibleAndAlpha) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().Visible(false).Alpha(0.25f);
  auto doc = builder.Done();
  EXPECT_FALSE(doc->layers[0]->visible);
  EXPECT_FLOAT_EQ(doc->layers[0]->alpha, 0.25f);
}

TEST(PAGXBuilder, AddLayerBlendMode) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().BlendMode(pagx::BlendMode::Multiply);
  auto doc = builder.Done();
  EXPECT_EQ(doc->layers[0]->blendMode, pagx::BlendMode::Multiply);
}

TEST(PAGXBuilder, AddLayerThreeDFlags) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().Preserve3D(true).AntiAlias(false).GroupOpacity(false).PassThroughBackground(
      false);
  auto doc = builder.Done();
  EXPECT_TRUE(doc->layers[0]->preserve3D);
  EXPECT_FALSE(doc->layers[0]->antiAlias);
  EXPECT_FALSE(doc->layers[0]->groupOpacity);
  EXPECT_FALSE(doc->layers[0]->passThroughBackground);
}

TEST(PAGXBuilder, AddLayerScrollRect) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().ScrollRect(10.0f, 20.0f, 30.0f, 40.0f);
  auto doc = builder.Done();
  EXPECT_TRUE(doc->layers[0]->hasScrollRect);
  // pagx::Rect uses x/y/width/height storage with MakeLTRB derived from
  // (l, t, r, b) → x=l, y=t, width=r-l, height=b-t.
  EXPECT_FLOAT_EQ(doc->layers[0]->scrollRect.x, 10.0f);
  EXPECT_FLOAT_EQ(doc->layers[0]->scrollRect.y, 20.0f);
  EXPECT_FLOAT_EQ(doc->layers[0]->scrollRect.width, 20.0f);
  EXPECT_FLOAT_EQ(doc->layers[0]->scrollRect.height, 20.0f);
}

TEST(PAGXBuilder, AddLayerMask) {
  auto builder = PAGXBuilder::Make();
  pagx::Layer* target = builder.MakeRawLayer();
  builder.AddLayer().Mask(target, pagx::MaskType::Luminance);
  auto doc = builder.Done();
  EXPECT_EQ(doc->layers[0]->mask, target);
  EXPECT_EQ(doc->layers[0]->maskType, pagx::MaskType::Luminance);
}

TEST(PAGXBuilder, NestedChildren) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().Name("root").AddChild().Name("child0").AddChild().Name("grandchild");
  auto doc = builder.Done();
  ASSERT_EQ(doc->layers.size(), 1u);
  ASSERT_EQ(doc->layers[0]->children.size(), 1u);
  EXPECT_EQ(doc->layers[0]->children[0]->name, "child0");
  ASSERT_EQ(doc->layers[0]->children[0]->children.size(), 1u);
  EXPECT_EQ(doc->layers[0]->children[0]->children[0]->name, "grandchild");
}

TEST(PAGXBuilder, AddComposition) {
  auto builder = PAGXBuilder::Make();
  pagx::Composition* comp = builder.AddComposition(640.0f, 480.0f);
  ASSERT_NE(comp, nullptr);
  EXPECT_FLOAT_EQ(comp->width, 640.0f);
  EXPECT_FLOAT_EQ(comp->height, 480.0f);
}

TEST(PAGXBuilder, CompositionRefLayer) {
  auto builder = PAGXBuilder::Make();
  pagx::Composition* comp = builder.AddComposition(320.0f, 240.0f);
  builder.AddCompositionRefLayer(comp).Name("ref");
  auto doc = builder.Done();
  ASSERT_EQ(doc->layers.size(), 1u);
  EXPECT_EQ(doc->layers[0]->composition, comp);
  EXPECT_EQ(doc->layers[0]->name, "ref");
}

TEST(PAGXBuilder, MultipleSiblingLayers) {
  auto builder = PAGXBuilder::Make();
  builder.AddLayer().Name("a");
  builder.AddLayer().Name("b");
  builder.AddLayer().Name("c");
  auto doc = builder.Done();
  ASSERT_EQ(doc->layers.size(), 3u);
  EXPECT_EQ(doc->layers[0]->name, "a");
  EXPECT_EQ(doc->layers[1]->name, "b");
  EXPECT_EQ(doc->layers[2]->name, "c");
}

TEST(PAGXBuilder, ChainAllSettersOneCall) {
  // Smoke chain hitting every fluent setter at once — guards against future
  // accidental method removals.
  pagx::Layer* maskTarget = nullptr;
  auto builder = PAGXBuilder::Make();
  maskTarget = builder.MakeRawLayer();
  builder.AddLayer()
      .Name("kitchen-sink")
      .Visible(true)
      .Alpha(0.75f)
      .BlendMode(pagx::BlendMode::Screen)
      .Position(5.0f, 10.0f)
      .Preserve3D(true)
      .AntiAlias(true)
      .GroupOpacity(true)
      .PassThroughBackground(false)
      .ScrollRect(0.0f, 0.0f, 100.0f, 100.0f)
      .Mask(maskTarget, pagx::MaskType::Alpha);
  auto doc = builder.Done();
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.size(), 1u);
}

}  // namespace pagx::test
