// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 3 unit tests for LayerBaker — PAGX → PAGDocument layer-tree shape and
// generic-field round-trip.
#include <memory>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Layer.h"
#include "pagx/pag/Baker.h"

namespace pagx::pag {

namespace {

pagx::test::PAGXBuilder MakeBuilderWithSingleLayer() {
  auto builder = pagx::test::PAGXBuilder::Make(800, 600);
  builder.AddLayer().Name("solo");
  return builder;
}

}  // namespace

TEST(LayerBaker, EmptyDocumentProducesWarning) {
  // No top-level layers → BakeResult.doc non-null, warning EmptyDocument=207.
  auto pagxDoc = pagx::test::PAGXBuilder::Make().Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  EXPECT_TRUE(result.errors.empty());
  ASSERT_EQ(result.warnings.size(), 1u);
  EXPECT_EQ(result.warnings[0].code, DiagnosticCode::EmptyDocument);
}

TEST(LayerBaker, SingleLayerYieldsOneCompositionAndOneLayer) {
  auto pagxDoc = MakeBuilderWithSingleLayer().Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  ASSERT_EQ(result.doc->compositions.size(), 1u);
  ASSERT_EQ(result.doc->compositions[0]->layers.size(), 1u);
  EXPECT_EQ(result.doc->compositions[0]->layers[0]->name, "solo");
  EXPECT_EQ(result.doc->compositions[0]->layers[0]->type, LayerType::Layer);
}

TEST(LayerBaker, CanvasSizeCarriesIntoHeader) {
  auto builder = pagx::test::PAGXBuilder::Make(1280, 720);
  builder.AddLayer().Name("a");
  auto pagxDoc = builder.Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  EXPECT_FLOAT_EQ(result.doc->header.width, 1280.0f);
  EXPECT_FLOAT_EQ(result.doc->header.height, 720.0f);
  EXPECT_EQ(result.doc->compositions[0]->width, 1280u);
  EXPECT_EQ(result.doc->compositions[0]->height, 720u);
}

TEST(LayerBaker, GenericFieldsRoundTrip) {
  auto builder = pagx::test::PAGXBuilder::Make();
  builder.AddLayer()
      .Name("box")
      .Visible(false)
      .Alpha(0.5f)
      .BlendMode(pagx::BlendMode::Multiply)
      .Position(20.0f, 40.0f)
      .Preserve3D(true)
      .AntiAlias(false)
      .GroupOpacity(false)
      .PassThroughBackground(false)
      .ScrollRect(1.0f, 2.0f, 3.0f, 4.0f);
  auto pagxDoc = builder.Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  const auto& baked = *result.doc->compositions[0]->layers[0];
  EXPECT_EQ(baked.name, "box");
  EXPECT_FALSE(baked.visible.value);
  EXPECT_FLOAT_EQ(baked.alpha.value, 0.5f);
  EXPECT_EQ(baked.blendMode.value, BlendMode::Multiply);
  EXPECT_FLOAT_EQ(baked.matrix.value.getTranslateX(), 20.0f);
  EXPECT_FLOAT_EQ(baked.matrix.value.getTranslateY(), 40.0f);
  EXPECT_TRUE(baked.preserve3D);
  EXPECT_FALSE(baked.allowsEdgeAntialiasing);
  EXPECT_FALSE(baked.allowsGroupOpacity);
  EXPECT_FALSE(baked.passThroughBackground);
  EXPECT_TRUE(baked.hasScrollRect);
  EXPECT_FLOAT_EQ(baked.scrollRect.value.left, 1.0f);
  EXPECT_FLOAT_EQ(baked.scrollRect.value.bottom, 4.0f);
}

TEST(LayerBaker, NestedChildrenPreserveStructure) {
  auto builder = pagx::test::PAGXBuilder::Make();
  builder.AddLayer().Name("root").AddChild().Name("c0").AddChild().Name("c1");
  auto pagxDoc = builder.Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  auto& root = *result.doc->compositions[0]->layers[0];
  ASSERT_EQ(root.children.size(), 1u);
  EXPECT_EQ(root.children[0]->name, "c0");
  ASSERT_EQ(root.children[0]->children.size(), 1u);
  EXPECT_EQ(root.children[0]->children[0]->name, "c1");
}

TEST(LayerBaker, CompositionRefBakesIntoSecondComposition) {
  auto builder = pagx::test::PAGXBuilder::Make();
  pagx::Composition* comp = builder.AddComposition(640, 480);
  // PAGX Composition needs at least one layer for its layers vector — empty
  // is acceptable for Phase 3 (the inner composition is just an empty shell).
  builder.AddCompositionRefLayer(comp).Name("ref");
  auto pagxDoc = builder.Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  // doc.compositions[0] is the synthesized root, doc.compositions[1] is the
  // referenced composition. Their order is implementation-defined but the
  // reference must point at index 1 (root is registered first).
  ASSERT_EQ(result.doc->compositions.size(), 2u);
  const auto& refLayer = *result.doc->compositions[0]->layers[0];
  EXPECT_EQ(refLayer.type, LayerType::CompositionRef);
  EXPECT_EQ(refLayer.compositionIndex, 1u);
  EXPECT_EQ(result.doc->compositions[1]->width, 640u);
  EXPECT_EQ(result.doc->compositions[1]->height, 480u);
}

TEST(LayerBaker, MaskSelfReferenceProducesWarning) {
  // Build a PAGX layer whose mask points at itself; Baker should warn 203.
  auto builder = pagx::test::PAGXBuilder::Make();
  pagx::Layer* self = builder.MakeRawLayer();
  self->name = "self";
  self->mask = self;
  builder.RawDocument()->layers.push_back(self);
  // Need to apply layout manually since we bypassed AddLayer.
  builder.RawDocument()->applyLayout();
  auto result = Bake(*builder.RawDocument());
  ASSERT_NE(result.doc, nullptr);
  bool found = false;
  for (const auto& w : result.warnings) {
    if (w.code == DiagnosticCode::MaskSelfReference) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(LayerBaker, MaskTargetInDifferentCompositionMissing) {
  // mask points at a layer inside a different composition — Pass 2 cannot
  // resolve it (paths are per-composition) and emits MaskTargetMissing=202.
  auto builder = pagx::test::PAGXBuilder::Make();
  auto* otherComp = builder.AddComposition(100, 100);
  auto* outsider = builder.MakeRawLayer();
  outsider->name = "outsider";
  otherComp->layers.push_back(outsider);

  auto host = builder.AddLayer().Name("host");
  host.Mask(outsider);
  builder.RawDocument()->applyLayout();

  auto result = Bake(*builder.RawDocument());
  ASSERT_NE(result.doc, nullptr);
  bool found = false;
  for (const auto& w : result.warnings) {
    if (w.code == DiagnosticCode::MaskTargetMissing) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(LayerBaker, MaskResolvedFillsMaskLayerPath) {
  // host.mask = sibling at root index 0 → host (root index 1) should bake
  // with maskLayerPath == [0].
  auto builder = pagx::test::PAGXBuilder::Make();
  auto target = builder.AddLayer().Name("target");
  pagx::Layer* targetPtr = target.layer();

  auto host = builder.AddLayer().Name("host");
  host.Mask(targetPtr);

  auto pagxDoc = builder.Done();
  auto result = Bake(*pagxDoc);
  ASSERT_NE(result.doc, nullptr);
  EXPECT_TRUE(result.errors.empty());

  // No mask warnings — successful resolution stays silent.
  for (const auto& w : result.warnings) {
    EXPECT_NE(w.code, DiagnosticCode::MaskTargetMissing);
    EXPECT_NE(w.code, DiagnosticCode::MaskSelfReference);
  }
  ASSERT_EQ(result.doc->compositions[0]->layers.size(), 2u);
  const auto& hostLayer = result.doc->compositions[0]->layers[1];
  ASSERT_EQ(hostLayer->maskLayerPath.size(), 1u);
  EXPECT_EQ(hostLayer->maskLayerPath[0], 0u);
}

}  // namespace pagx::pag
