// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 5c VectorBaker — verifies the PAGX vector element subtree gets
// translated into PAGDocument::VectorPayload with the right type tags and
// per-field values. Phase 5c covers nine geometry / modifier elements; Fill
// / Stroke land in Phase 6 and Text-family in Phase 8.
#include <memory>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

namespace {

// Build a PAGXDocument that holds a single layer whose `contents` is the
// caller-provided list. Returns the baked PAGDocument or null on fatal.
template <typename Setup>
std::unique_ptr<PAGDocument> BakeWithContents(Setup setup) {
  auto builder = pagx::test::PAGXBuilder::Make();
  auto layer = builder.AddLayer().Name("vector_host");
  setup(builder, *layer.layer());
  builder.RawDocument()->applyLayout();
  auto result = Bake(*builder.RawDocument());
  if (result.doc == nullptr) {
    return nullptr;
  }
  return std::move(result.doc);
}

}  // namespace

TEST(VectorBaker, RectangleProducesVectorElement) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* rect = b.RawDocument()->makeNode<pagx::Rectangle>();
    rect->position = pagx::Point{10.0f, 20.0f};
    rect->size = pagx::Size{100.0f, 50.0f};
    rect->roundness = 8.0f;
    rect->reversed = true;
    host.contents.push_back(rect);
  });
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->compositions[0]->layers.size(), 1u);
  const auto& layer = *doc->compositions[0]->layers[0];
  EXPECT_EQ(layer.type, LayerType::Vector);
  ASSERT_NE(layer.vector, nullptr);
  ASSERT_EQ(layer.vector->contents.size(), 1u);
  ASSERT_EQ(layer.vector->contents[0]->type, VectorElementType::Rectangle);
  const auto& d =
      std::get<std::unique_ptr<ElementRectangleData>>(layer.vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->position.value.x, 10.0f);
  EXPECT_FLOAT_EQ(d->size.value.y, 50.0f);
  EXPECT_FLOAT_EQ(d->roundness.value, 8.0f);
  EXPECT_TRUE(d->reversed);
}

TEST(VectorBaker, EllipseRoundTrip) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* e = b.RawDocument()->makeNode<pagx::Ellipse>();
    e->position = pagx::Point{0.0f, 0.0f};
    e->size = pagx::Size{60.0f, 40.0f};
    host.contents.push_back(e);
  });
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->compositions[0]->layers[0]->vector->contents.size(), 1u);
  EXPECT_EQ(doc->compositions[0]->layers[0]->vector->contents[0]->type, VectorElementType::Ellipse);
}

TEST(VectorBaker, PolystarTypeOrderMappedCorrectly) {
  // pagx::PolystarType is { Polygon=0, Star=1 }; tgfx::PolystarType is
  // { Star=0, Polygon=1 }. The baker must map by name not by ordinal.
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* p = b.RawDocument()->makeNode<pagx::Polystar>();
    p->type = pagx::PolystarType::Polygon;
    p->pointCount = 6.0f;
    host.contents.push_back(p);
  });
  ASSERT_NE(doc, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementPolystarData>>(
      doc->compositions[0]->layers[0]->vector->contents[0]->payload);
  EXPECT_EQ(d->polystarType, ::tgfx::PolystarType::Polygon);
  EXPECT_FLOAT_EQ(d->pointCount.value, 6.0f);
}

TEST(VectorBaker, TrimPathPreserved) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* tp = b.RawDocument()->makeNode<pagx::TrimPath>();
    tp->start = 0.25f;
    tp->end = 0.75f;
    tp->offset = 30.0f;
    tp->type = pagx::TrimType::Continuous;
    host.contents.push_back(tp);
  });
  ASSERT_NE(doc, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementTrimPathData>>(
      doc->compositions[0]->layers[0]->vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->start.value, 0.25f);
  EXPECT_FLOAT_EQ(d->end.value, 0.75f);
  EXPECT_FLOAT_EQ(d->offset.value, 30.0f);
  EXPECT_EQ(d->trimType, ::tgfx::TrimPathType::Continuous);
}

TEST(VectorBaker, RoundCornerPreserved) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* rc = b.RawDocument()->makeNode<pagx::RoundCorner>();
    rc->radius = 12.0f;
    host.contents.push_back(rc);
  });
  ASSERT_NE(doc, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementRoundCornerData>>(
      doc->compositions[0]->layers[0]->vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->radius.value, 12.0f);
}

TEST(VectorBaker, MergePathPreserved) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* mp = b.RawDocument()->makeNode<pagx::MergePath>();
    mp->mode = pagx::MergePathMode::Difference;
    host.contents.push_back(mp);
  });
  ASSERT_NE(doc, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementMergePathData>>(
      doc->compositions[0]->layers[0]->vector->contents[0]->payload);
  EXPECT_EQ(d->mode, ::tgfx::MergePathOp::Difference);
}

TEST(VectorBaker, RepeaterPreserved) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* r = b.RawDocument()->makeNode<pagx::Repeater>();
    r->copies = 4.0f;
    r->offset = 0.5f;
    r->rotation = 15.0f;
    host.contents.push_back(r);
  });
  ASSERT_NE(doc, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementRepeaterData>>(
      doc->compositions[0]->layers[0]->vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->copies.value, 4.0f);
  EXPECT_FLOAT_EQ(d->offset.value, 0.5f);
  EXPECT_FLOAT_EQ(d->rotation.value, 15.0f);
}

TEST(VectorBaker, GroupRecursivelyBakesChildren) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* g = b.RawDocument()->makeNode<pagx::Group>();
    g->alpha = 0.4f;

    auto* r1 = b.RawDocument()->makeNode<pagx::Rectangle>();
    r1->size = pagx::Size{10.0f, 10.0f};
    g->elements.push_back(r1);

    auto* r2 = b.RawDocument()->makeNode<pagx::Rectangle>();
    r2->size = pagx::Size{20.0f, 20.0f};
    g->elements.push_back(r2);

    host.contents.push_back(g);
  });
  ASSERT_NE(doc, nullptr);
  const auto& root = doc->compositions[0]->layers[0]->vector->contents[0];
  ASSERT_EQ(root->type, VectorElementType::VectorGroup);
  const auto& d = std::get<std::unique_ptr<ElementVectorGroupData>>(root->payload);
  ASSERT_EQ(d->elements.size(), 2u);
  EXPECT_EQ(d->elements[0]->type, VectorElementType::Rectangle);
  EXPECT_EQ(d->elements[1]->type, VectorElementType::Rectangle);
  EXPECT_FLOAT_EQ(d->alpha.value, 0.4f);
}

TEST(VectorBaker, MultipleElementsPreserveOrderAndTriggerVectorLayer) {
  auto doc = BakeWithContents([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* rect = b.RawDocument()->makeNode<pagx::Rectangle>();
    auto* tp = b.RawDocument()->makeNode<pagx::TrimPath>();
    auto* rc = b.RawDocument()->makeNode<pagx::RoundCorner>();
    host.contents.push_back(rect);
    host.contents.push_back(tp);
    host.contents.push_back(rc);
  });
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->compositions[0]->layers[0]->type, LayerType::Vector);
  const auto& contents = doc->compositions[0]->layers[0]->vector->contents;
  ASSERT_EQ(contents.size(), 3u);
  EXPECT_EQ(contents[0]->type, VectorElementType::Rectangle);
  EXPECT_EQ(contents[1]->type, VectorElementType::TrimPath);
  EXPECT_EQ(contents[2]->type, VectorElementType::RoundCorner);
}

}  // namespace pagx::pag
