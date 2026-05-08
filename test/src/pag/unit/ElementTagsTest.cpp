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

// =============================================================================
// Phase 17 v2.23 — EmbeddedFont + ElementText case A / case B round-trips.
//
// Case A: ElementText.glyphRuns references PAGDocument.embeddedFonts[i] by
// index — the two structures only round-trip together when the document
// carries the matching embedded font resource. Cases below exercise both
// halves end-to-end through Codec::Encode + Codec::Decode.
// =============================================================================

namespace {

// Doc-level round-trip: keeps the original Composition + Layer + Vector
// scaffolding from RoundTripVectorPayload but additionally seeds and
// preserves PAGDocument.embeddedFonts so case A glyphRuns can validate
// their indexed resource lookup.
std::unique_ptr<PAGDocument> RoundTripDocument(std::unique_ptr<PAGDocument> doc) {
  auto enc = Codec::Encode(*doc);
  if (enc.bytes == nullptr) {
    return nullptr;
  }
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  return std::move(dec.doc);
}

std::unique_ptr<PAGDocument> MakeMinimalDocWithText(std::unique_ptr<ElementTextData> textData) {
  auto doc = std::make_unique<PAGDocument>();
  doc->header.width = 100.0f;
  doc->header.height = 100.0f;

  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 100;
  comp->height = 100;

  auto layer = std::make_unique<Layer>();
  layer->type = LayerType::Vector;
  layer->vector = std::make_unique<VectorPayload>();
  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Text;
  el->payload = std::move(textData);
  layer->vector->contents.push_back(std::move(el));
  comp->layers.push_back(std::move(layer));

  doc->compositions.push_back(std::move(comp));
  return doc;
}

}  // namespace

TEST(ElementTags, EmbeddedFontTableRoundTrip) {
  auto doc = std::make_unique<PAGDocument>();
  doc->header.width = 100.0f;
  doc->header.height = 100.0f;

  // Two distinct embedded fonts so the table count and per-entry data
  // both get exercised.
  auto font0 = std::make_unique<EmbeddedFont>();
  font0->unitsPerEm = 1000;
  font0->glyphs.resize(2);
  font0->glyphs[0].advance = 500.0f;
  font0->glyphs[0].path.moveTo(0, 0);
  font0->glyphs[0].path.lineTo(10, 0);
  font0->glyphs[0].path.lineTo(10, -20);
  font0->glyphs[0].path.close();
  font0->glyphs[1].advance = 625.0f;
  font0->glyphs[1].path.moveTo(0, 0);
  font0->glyphs[1].path.lineTo(15, -25);
  font0->glyphs[1].path.close();

  auto font1 = std::make_unique<EmbeddedFont>();
  font1->unitsPerEm = 2048;
  font1->glyphs.resize(1);
  font1->glyphs[0].advance = 800.5f;
  font1->glyphs[0].path.moveTo(2, 3);
  font1->glyphs[0].path.lineTo(4, 5);

  doc->embeddedFonts.push_back(std::move(font0));
  doc->embeddedFonts.push_back(std::move(font1));

  // A composition is mandatory at the top level even when no Text consumes
  // these embedded fonts; the codec serialises the table independently.
  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 100;
  comp->height = 100;
  doc->compositions.push_back(std::move(comp));

  auto out = RoundTripDocument(std::move(doc));
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->embeddedFonts.size(), 2u);

  const auto& f0 = *out->embeddedFonts[0];
  EXPECT_EQ(f0.unitsPerEm, 1000u);
  ASSERT_EQ(f0.glyphs.size(), 2u);
  EXPECT_FLOAT_EQ(f0.glyphs[0].advance, 500.0f);
  EXPECT_FLOAT_EQ(f0.glyphs[1].advance, 625.0f);
  EXPECT_FALSE(f0.glyphs[0].path.isEmpty());
  EXPECT_FALSE(f0.glyphs[1].path.isEmpty());

  const auto& f1 = *out->embeddedFonts[1];
  EXPECT_EQ(f1.unitsPerEm, 2048u);
  ASSERT_EQ(f1.glyphs.size(), 1u);
  EXPECT_FLOAT_EQ(f1.glyphs[0].advance, 800.5f);
  EXPECT_FALSE(f1.glyphs[0].path.isEmpty());
}

TEST(ElementTags, ElementTextCaseAGlyphRunsRoundTrip) {
  auto textData = std::make_unique<ElementTextData>();
  textData->position = MakeProp(::tgfx::Point{34.0f, 145.0f});
  textData->text = "GlyphRun";
  textData->fontFamily = "Arial";
  textData->fontStyle = "Bold";
  textData->fontSize = 72.0f;

  // Case A: one author-authored GlyphRun with explicit per-glyph data.
  ElementTextData::GlyphRunData run;
  run.embeddedFontIndex = 0;
  run.fontSize = 72.0f;
  run.glyphs = {14, 15, 16, 17, 18, 19, 20, 21};
  run.x = 34.0f;
  run.y = 145.0f;
  // xOffsets and positions are both optional — exercise both being non-empty.
  run.xOffsets = {0.0f, 50.0f, 100.0f, 150.0f, 200.0f, 250.0f, 300.0f, 350.0f};
  run.positions = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
  // Phase 18 reserved vectors stay empty in Phase 17.
  textData->glyphRuns.push_back(std::move(run));

  auto doc = MakeMinimalDocWithText(std::move(textData));
  auto out = RoundTripDocument(std::move(doc));
  ASSERT_NE(out, nullptr);
  ASSERT_EQ(out->compositions.size(), 1u);
  ASSERT_EQ(out->compositions[0]->layers.size(), 1u);
  const auto& vec = out->compositions[0]->layers[0]->vector;
  ASSERT_NE(vec, nullptr);
  ASSERT_EQ(vec->contents.size(), 1u);
  ASSERT_EQ(vec->contents[0]->type, VectorElementType::Text);

  const auto& d = std::get<std::unique_ptr<ElementTextData>>(vec->contents[0]->payload);
  ASSERT_EQ(d->glyphRuns.size(), 1u);
  EXPECT_TRUE(d->shapedGlyphs.empty());
  EXPECT_TRUE(d->shapedRuns.empty());

  const auto& r = d->glyphRuns[0];
  EXPECT_EQ(r.embeddedFontIndex, 0u);
  EXPECT_FLOAT_EQ(r.fontSize, 72.0f);
  ASSERT_EQ(r.glyphs.size(), 8u);
  EXPECT_EQ(r.glyphs[0], 14);
  EXPECT_EQ(r.glyphs[7], 21);
  EXPECT_FLOAT_EQ(r.x, 34.0f);
  EXPECT_FLOAT_EQ(r.y, 145.0f);
  ASSERT_EQ(r.xOffsets.size(), 8u);
  EXPECT_FLOAT_EQ(r.xOffsets[3], 150.0f);
  ASSERT_EQ(r.positions.size(), 8u);
  EXPECT_TRUE(r.anchors.empty());
  EXPECT_TRUE(r.scales.empty());
  EXPECT_TRUE(r.rotations.empty());
  EXPECT_TRUE(r.skews.empty());

  // Source-attribute metadata — also expected to round-trip.
  EXPECT_EQ(d->text, "GlyphRun");
  EXPECT_EQ(d->fontFamily, "Arial");
  EXPECT_EQ(d->fontStyle, "Bold");
  EXPECT_FLOAT_EQ(d->fontSize, 72.0f);
}

TEST(ElementTags, ElementTextCaseBShapedGlyphsRoundTrip) {
  auto textData = std::make_unique<ElementTextData>();
  textData->position = MakeProp(::tgfx::Point{0.0f, 24.0f});
  textData->text = "Hi";
  textData->fontFamily = "Arial";
  textData->fontStyle = "Regular";
  textData->fontSize = 24.0f;

  // Case B: PAGX TextLayout-resolved snapshot. Two runs, second carrying
  // RSXform to exercise the optional vertical-writing branch.
  ElementTextData::ShapedGlyphRun run0;
  run0.typefaceFamily = "Arial";
  run0.typefaceStyle = "Regular";
  run0.typefaceKey = "Arial|Regular|2048|3000";
  run0.fontSize = 24.0f;
  run0.glyphs = {72, 105};  // 'H', 'i'
  run0.positions = {{0, 0}, {12, 0}};
  textData->shapedGlyphs.push_back(std::move(run0));

  ElementTextData::ShapedGlyphRun run1;
  run1.typefaceFamily = "Noto Sans";
  run1.typefaceStyle = "Bold";
  run1.typefaceKey = "Noto Sans|Bold|2048|7000";
  run1.fontSize = 18.0f;
  run1.glyphs = {200, 201};
  run1.positions = {{0, 24}, {0, 48}};
  // Vertical-writing rotated glyph — scos / ssin = sin(90°), cos(90°).
  run1.xforms = {{0.0f, 1.0f, 0.0f, 24.0f}, {0.0f, 1.0f, 0.0f, 48.0f}};
  textData->shapedGlyphs.push_back(std::move(run1));

  auto doc = MakeMinimalDocWithText(std::move(textData));
  auto out = RoundTripDocument(std::move(doc));
  ASSERT_NE(out, nullptr);

  const auto& vec = out->compositions[0]->layers[0]->vector;
  const auto& d = std::get<std::unique_ptr<ElementTextData>>(vec->contents[0]->payload);
  ASSERT_EQ(d->shapedGlyphs.size(), 2u);
  EXPECT_TRUE(d->glyphRuns.empty());
  EXPECT_TRUE(d->shapedRuns.empty());

  const auto& r0 = d->shapedGlyphs[0];
  EXPECT_EQ(r0.typefaceFamily, "Arial");
  EXPECT_EQ(r0.typefaceStyle, "Regular");
  EXPECT_EQ(r0.typefaceKey, "Arial|Regular|2048|3000");
  EXPECT_FLOAT_EQ(r0.fontSize, 24.0f);
  ASSERT_EQ(r0.glyphs.size(), 2u);
  EXPECT_EQ(r0.glyphs[0], 72);
  ASSERT_EQ(r0.positions.size(), 2u);
  EXPECT_FLOAT_EQ(r0.positions[1].x, 12.0f);
  EXPECT_TRUE(r0.xforms.empty());

  const auto& r1 = d->shapedGlyphs[1];
  EXPECT_EQ(r1.typefaceFamily, "Noto Sans");
  EXPECT_EQ(r1.typefaceKey, "Noto Sans|Bold|2048|7000");
  ASSERT_EQ(r1.xforms.size(), 2u);
  EXPECT_FLOAT_EQ(r1.xforms[0].scos, 0.0f);
  EXPECT_FLOAT_EQ(r1.xforms[0].ssin, 1.0f);
  EXPECT_FLOAT_EQ(r1.xforms[1].ty, 48.0f);
}

TEST(ElementTags, ElementTextEmptyBranchesRoundTrip) {
  // Sanity case: ElementText with neither glyphRuns nor shapedGlyphs nor
  // shapedRuns — Commit 1 visual-no-regression baseline. The boxFlags
  // bits 0x40 / 0x80 / 0x100 should all be off; round-trip must preserve
  // the base source-attribute fields without crashing on the empty branches.
  auto textData = std::make_unique<ElementTextData>();
  textData->position = MakeProp(::tgfx::Point{1.0f, 2.0f});
  textData->text = "empty";
  textData->fontFamily = "Sans";
  textData->fontSize = 14.0f;
  textData->fauxBold = true;
  textData->fillColor = ::tgfx::Color{0.5f, 0.5f, 0.5f, 1.0f};

  auto doc = MakeMinimalDocWithText(std::move(textData));
  auto out = RoundTripDocument(std::move(doc));
  ASSERT_NE(out, nullptr);

  const auto& vec = out->compositions[0]->layers[0]->vector;
  const auto& d = std::get<std::unique_ptr<ElementTextData>>(vec->contents[0]->payload);
  EXPECT_TRUE(d->glyphRuns.empty());
  EXPECT_TRUE(d->shapedGlyphs.empty());
  EXPECT_TRUE(d->shapedRuns.empty());
  EXPECT_EQ(d->text, "empty");
  EXPECT_EQ(d->fontFamily, "Sans");
  EXPECT_FLOAT_EQ(d->fontSize, 14.0f);
  EXPECT_TRUE(d->fauxBold);
}
