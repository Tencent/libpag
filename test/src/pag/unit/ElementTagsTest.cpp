// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 5a — round-trips each of the 11 supported VectorElement Tag bodies
// through Codec::Encode + Codec::Decode by wrapping them in a Vector layer,
// the only path that wires the new readers into the top-level dispatcher.
// Text / TextPath / TextModifier round-trips defer to Phase 8.
#include <utility>
#include "gtest/gtest.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"

using namespace pagx::pag;

namespace {

// Wraps an arbitrary list of VectorElements inside a single Vector layer
// inside a single Composition, then runs Encode + Decode and returns the
// roundtripped layer's vector payload (or nullptr on decode failure).
std::unique_ptr<VectorPayload> RoundTripVectorPayload(
    std::vector<std::unique_ptr<VectorElement>> elements) {
  PAGDocument doc;
  doc.header.width = 100.0f;
  doc.header.height = 100.0f;

  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 100;
  comp->height = 100;

  auto layer = std::make_unique<Layer>();
  layer->type = LayerType::Vector;
  layer->vector = std::make_unique<VectorPayload>();
  layer->vector->contents = std::move(elements);
  comp->layers.push_back(std::move(layer));

  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  if (enc.bytes == nullptr) {
    return nullptr;
  }
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  if (dec.doc == nullptr) {
    return nullptr;
  }
  if (dec.doc->compositions.empty() || dec.doc->compositions[0]->layers.empty()) {
    return nullptr;
  }
  return std::move(dec.doc->compositions[0]->layers[0]->vector);
}

template <typename Data>
std::unique_ptr<VectorElement> MakeElement(VectorElementType type, std::unique_ptr<Data> data) {
  auto el = std::make_unique<VectorElement>();
  el->type = type;
  el->payload = std::move(data);
  return el;
}

}  // namespace

TEST(ElementTags, RectangleRoundTrip) {
  auto rect = std::make_unique<ElementRectangleData>();
  rect->position = MakeProp(::tgfx::Point{10.0f, 20.0f});
  rect->size = MakeProp(::tgfx::Point{100.0f, 50.0f});
  rect->roundness = MakeProp(8.0f);
  rect->reversed = true;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::Rectangle, std::move(rect)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->contents.size(), 1u);
  ASSERT_EQ(out->contents[0]->type, VectorElementType::Rectangle);
  const auto& d = std::get<std::unique_ptr<ElementRectangleData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->position.value.x, 10.0f);
  EXPECT_FLOAT_EQ(d->position.value.y, 20.0f);
  EXPECT_FLOAT_EQ(d->size.value.x, 100.0f);
  EXPECT_FLOAT_EQ(d->size.value.y, 50.0f);
  EXPECT_FLOAT_EQ(d->roundness.value, 8.0f);
  EXPECT_TRUE(d->reversed);
}

TEST(ElementTags, EllipseRoundTrip) {
  auto e = std::make_unique<ElementEllipseData>();
  e->position = MakeProp(::tgfx::Point{5.0f, 7.0f});
  e->size = MakeProp(::tgfx::Point{30.0f, 40.0f});
  e->reversed = false;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::Ellipse, std::move(e)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementEllipseData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->position.value.x, 5.0f);
  EXPECT_FLOAT_EQ(d->size.value.y, 40.0f);
  EXPECT_FALSE(d->reversed);
}

TEST(ElementTags, PolystarRoundTrip) {
  auto p = std::make_unique<ElementPolystarData>();
  p->pointCount = MakeProp(7.0f);
  p->outerRadius = MakeProp(50.0f);
  p->innerRadius = MakeProp(25.0f);
  p->rotation = MakeProp(30.0f);
  p->polystarType = ::tgfx::PolystarType::Star;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::Polystar, std::move(p)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementPolystarData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->pointCount.value, 7.0f);
  EXPECT_FLOAT_EQ(d->outerRadius.value, 50.0f);
  EXPECT_FLOAT_EQ(d->innerRadius.value, 25.0f);
  EXPECT_FLOAT_EQ(d->rotation.value, 30.0f);
  EXPECT_EQ(d->polystarType, ::tgfx::PolystarType::Star);
}

TEST(ElementTags, ShapePathRoundTripPositionOnly) {
  // Phase 5a omits the Path field — just verify the position survives.
  auto sp = std::make_unique<ElementShapePathData>();
  sp->position = MakeProp(::tgfx::Point{1.5f, 2.5f});
  sp->reversed = true;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::ShapePath, std::move(sp)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementShapePathData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->position.value.x, 1.5f);
  EXPECT_TRUE(d->reversed);
}

TEST(ElementTags, FillStyleSolidColorRoundTrip) {
  auto fs = std::make_unique<ElementFillStyleData>();
  fs->style = std::make_unique<ShapeStyleData>();
  fs->style->sourceType = ColorSourceType::SolidColor;
  fs->style->color = MakeProp(::tgfx::Color{1.0f, 0.0f, 0.0f, 1.0f});
  fs->style->alpha = MakeProp(0.7f);
  fs->fillRule = ::tgfx::FillRule::EvenOdd;
  fs->placement = ::tgfx::LayerPlacement::Foreground;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::FillStyle, std::move(fs)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementFillStyleData>>(out->contents[0]->payload);
  ASSERT_NE(d->style, nullptr);
  EXPECT_EQ(d->style->sourceType, ColorSourceType::SolidColor);
  EXPECT_NEAR(d->style->color.value.red, 1.0f, 1.0f / 255.0f);
  EXPECT_NEAR(d->style->color.value.alpha, 1.0f, 1.0f / 255.0f);
  EXPECT_FLOAT_EQ(d->style->alpha.value, 0.7f);
  EXPECT_EQ(d->fillRule, ::tgfx::FillRule::EvenOdd);
  EXPECT_EQ(d->placement, ::tgfx::LayerPlacement::Foreground);
}

TEST(ElementTags, StrokeStyleRoundTrip) {
  auto ss = std::make_unique<ElementStrokeStyleData>();
  ss->style = std::make_unique<ShapeStyleData>();
  ss->style->sourceType = ColorSourceType::SolidColor;
  ss->style->color = MakeProp(::tgfx::Color{0.0f, 1.0f, 0.0f, 1.0f});
  ss->strokeWidth = MakeProp(3.0f);
  ss->lineCap = ::tgfx::LineCap::Round;
  ss->lineJoin = ::tgfx::LineJoin::Bevel;
  ss->miterLimit = MakeProp(8.0f);
  ss->lineDashPattern = MakeProp(std::vector<float>{4.0f, 2.0f, 1.0f});
  ss->lineDashPhase = MakeProp(1.5f);
  ss->lineDashAdaptive = true;
  ss->strokeAlign = ::tgfx::StrokeAlign::Inside;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::StrokeStyle, std::move(ss)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementStrokeStyleData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->strokeWidth.value, 3.0f);
  EXPECT_EQ(d->lineCap, ::tgfx::LineCap::Round);
  EXPECT_EQ(d->lineJoin, ::tgfx::LineJoin::Bevel);
  EXPECT_FLOAT_EQ(d->miterLimit.value, 8.0f);
  ASSERT_EQ(d->lineDashPattern.value.size(), 3u);
  EXPECT_FLOAT_EQ(d->lineDashPattern.value[0], 4.0f);
  EXPECT_FLOAT_EQ(d->lineDashPattern.value[2], 1.0f);
  EXPECT_FLOAT_EQ(d->lineDashPhase.value, 1.5f);
  EXPECT_TRUE(d->lineDashAdaptive);
  EXPECT_EQ(d->strokeAlign, ::tgfx::StrokeAlign::Inside);
}

TEST(ElementTags, TrimPathRoundTrip) {
  auto tp = std::make_unique<ElementTrimPathData>();
  tp->start = MakeProp(20.0f);
  tp->end = MakeProp(80.0f);
  tp->offset = MakeProp(5.0f);
  tp->trimType = ::tgfx::TrimPathType::Continuous;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::TrimPath, std::move(tp)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementTrimPathData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->start.value, 20.0f);
  EXPECT_FLOAT_EQ(d->end.value, 80.0f);
  EXPECT_FLOAT_EQ(d->offset.value, 5.0f);
  EXPECT_EQ(d->trimType, ::tgfx::TrimPathType::Continuous);
}

TEST(ElementTags, RoundCornerRoundTrip) {
  auto rc = std::make_unique<ElementRoundCornerData>();
  rc->radius = MakeProp(12.0f);

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::RoundCorner, std::move(rc)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementRoundCornerData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->radius.value, 12.0f);
}

TEST(ElementTags, MergePathRoundTrip) {
  auto mp = std::make_unique<ElementMergePathData>();
  mp->mode = ::tgfx::MergePathOp::Difference;

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::MergePath, std::move(mp)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementMergePathData>>(out->contents[0]->payload);
  EXPECT_EQ(d->mode, ::tgfx::MergePathOp::Difference);
}

TEST(ElementTags, RepeaterRoundTrip) {
  auto r = std::make_unique<ElementRepeaterData>();
  r->copies = MakeProp(4.0f);
  r->offset = MakeProp(0.5f);
  r->order = ::tgfx::RepeaterOrder::AboveOriginal;
  r->position = MakeProp(::tgfx::Point{10.0f, 0.0f});
  r->rotation = MakeProp(15.0f);
  r->scale = MakeProp(::tgfx::Point{1.5f, 1.5f});

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::Repeater, std::move(r)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementRepeaterData>>(out->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->copies.value, 4.0f);
  EXPECT_FLOAT_EQ(d->offset.value, 0.5f);
  EXPECT_EQ(d->order, ::tgfx::RepeaterOrder::AboveOriginal);
  EXPECT_FLOAT_EQ(d->position.value.x, 10.0f);
  EXPECT_FLOAT_EQ(d->rotation.value, 15.0f);
  EXPECT_FLOAT_EQ(d->scale.value.x, 1.5f);
}

TEST(ElementTags, VectorGroupRecursionRoundTrip) {
  // VectorGroup with two Rectangle children + group-level alpha override.
  auto g = std::make_unique<ElementVectorGroupData>();

  auto r1 = std::make_unique<ElementRectangleData>();
  r1->size = MakeProp(::tgfx::Point{10.0f, 10.0f});
  g->elements.push_back(MakeElement(VectorElementType::Rectangle, std::move(r1)));

  auto r2 = std::make_unique<ElementRectangleData>();
  r2->size = MakeProp(::tgfx::Point{20.0f, 20.0f});
  g->elements.push_back(MakeElement(VectorElementType::Rectangle, std::move(r2)));

  g->alpha = MakeProp(0.4f);
  g->rotation = MakeProp(45.0f);

  std::vector<std::unique_ptr<VectorElement>> els;
  els.push_back(MakeElement(VectorElementType::VectorGroup, std::move(g)));
  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->contents[0]->type, VectorElementType::VectorGroup);
  const auto& d = std::get<std::unique_ptr<ElementVectorGroupData>>(out->contents[0]->payload);
  ASSERT_EQ(d->elements.size(), 2u);
  EXPECT_EQ(d->elements[0]->type, VectorElementType::Rectangle);
  EXPECT_EQ(d->elements[1]->type, VectorElementType::Rectangle);
  EXPECT_FLOAT_EQ(d->alpha.value, 0.4f);
  EXPECT_FLOAT_EQ(d->rotation.value, 45.0f);
}

TEST(ElementTags, MultipleElementsPreserveOrder) {
  std::vector<std::unique_ptr<VectorElement>> els;

  auto r = std::make_unique<ElementRectangleData>();
  r->roundness = MakeProp(2.0f);
  els.push_back(MakeElement(VectorElementType::Rectangle, std::move(r)));

  auto fs = std::make_unique<ElementFillStyleData>();
  fs->style = std::make_unique<ShapeStyleData>();
  els.push_back(MakeElement(VectorElementType::FillStyle, std::move(fs)));

  auto ss = std::make_unique<ElementStrokeStyleData>();
  ss->style = std::make_unique<ShapeStyleData>();
  els.push_back(MakeElement(VectorElementType::StrokeStyle, std::move(ss)));

  auto out = RoundTripVectorPayload(std::move(els));
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->contents.size(), 3u);
  EXPECT_EQ(out->contents[0]->type, VectorElementType::Rectangle);
  EXPECT_EQ(out->contents[1]->type, VectorElementType::FillStyle);
  EXPECT_EQ(out->contents[2]->type, VectorElementType::StrokeStyle);
}
