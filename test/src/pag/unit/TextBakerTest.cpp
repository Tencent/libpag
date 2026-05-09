// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 8 TextBaker — end-to-end Bake + Encode + Decode roundtrip for
// pre-shaped Text, TextPath, and TextModifier nodes. Runtime-shaped Text
// needs a real platform font to shape against; that path is verified in
// Phase 12 RenderEquivalenceTest against the standard sample corpus, not
// here, to keep unit tests font-independent.
#include <memory>
#include <optional>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

namespace {

struct RoundTripResult {
  std::unique_ptr<PAGDocument> baked;
  std::unique_ptr<PAGDocument> decoded;
  std::vector<Diagnostic> bakeWarnings;
};

// Bake + encode + decode. `skipLayout` defaults to false — Bake requires
// `applyLayout()` to have run (otherwise LayoutNotApplied=100 is fatal);
// the Text::onMeasure side effect of shaping against the authored font is
// harmless for pre-shaped Text (pagx::Text preserves `glyphRuns` through
// the layout pass; TextBaker prefers that path).
template <typename Setup>
RoundTripResult BakeAndRoundTrip(Setup setup, bool skipLayout = false) {
  auto builder = pagx::test::PAGXBuilder::Make();
  auto layer = builder.AddLayer().Name("text_host");
  setup(builder, *layer.layer());
  if (!skipLayout) {
    builder.RawDocument()->applyLayout();
  }
  auto bake = Bake(*builder.RawDocument());
  RoundTripResult r;
  r.bakeWarnings = bake.warnings;
  if (bake.doc == nullptr) {
    return r;
  }
  auto enc = Codec::Encode(*bake.doc);
  if (enc.bytes == nullptr) {
    return r;
  }
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  r.baked = std::move(bake.doc);
  r.decoded = std::move(dec.doc);
  return r;
}

const Layer* FirstLayer(const PAGDocument& doc) {
  if (doc.compositions.empty() || doc.compositions[0]->layers.empty()) {
    return nullptr;
  }
  return doc.compositions[0]->layers[0].get();
}

}  // namespace

// -----------------------------------------------------------------------
// TextPath
// -----------------------------------------------------------------------

TEST(TextBaker, TextPathFlagsAndPathRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* tp = b.RawDocument()->makeNode<pagx::TextPath>();
    tp->baselineOrigin = pagx::Point{5.0f, 10.0f};
    tp->baselineAngle = 45.0f;
    tp->firstMargin = 3.0f;
    tp->lastMargin = 7.0f;
    tp->perpendicular = true;
    tp->reversed = false;
    tp->forceAlignment = true;
    // Path stays null — the Baker treats that as the default tgfx::Path{}.
    host.contents.push_back(tp);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->vector->contents.size(), 1u);
  ASSERT_EQ(layer->vector->contents[0]->type, VectorElementType::TextPath);
  const auto& d =
      std::get<std::unique_ptr<ElementTextPathData>>(layer->vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->baselineOrigin.value.x, 5.0f);
  EXPECT_FLOAT_EQ(d->baselineOrigin.value.y, 10.0f);
  EXPECT_FLOAT_EQ(d->baselineAngle.value, 45.0f);
  EXPECT_FLOAT_EQ(d->firstMargin.value, 3.0f);
  EXPECT_FLOAT_EQ(d->lastMargin.value, 7.0f);
  EXPECT_TRUE(d->perpendicular);
  EXPECT_FALSE(d->reversed);
  EXPECT_TRUE(d->forceAlignment);
}

// -----------------------------------------------------------------------
// TextModifier
// -----------------------------------------------------------------------

TEST(TextBaker, TextModifierTransformRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* mod = b.RawDocument()->makeNode<pagx::TextModifier>();
    mod->anchor = pagx::Point{1.0f, 2.0f};
    mod->position = pagx::Point{3.0f, 4.0f};
    mod->rotation = 15.0f;
    mod->scale = pagx::Point{1.5f, 2.5f};
    mod->skew = 5.0f;
    mod->skewAxis = 10.0f;
    mod->alpha = 0.75f;
    host.contents.push_back(mod);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->vector->contents.size(), 1u);
  ASSERT_EQ(layer->vector->contents[0]->type, VectorElementType::TextModifier);
  const auto& d =
      std::get<std::unique_ptr<ElementTextModifierData>>(layer->vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->anchor.value.x, 1.0f);
  EXPECT_FLOAT_EQ(d->position.value.x, 3.0f);
  EXPECT_FLOAT_EQ(d->rotation.value, 15.0f);
  EXPECT_FLOAT_EQ(d->scale.value.x, 1.5f);
  EXPECT_FLOAT_EQ(d->scale.value.y, 2.5f);
  EXPECT_FLOAT_EQ(d->skew.value, 5.0f);
  EXPECT_FLOAT_EQ(d->alpha.value, 0.75f);
  // Optional paint overrides stay unset.
  EXPECT_FALSE(d->hasFillColor);
  EXPECT_FALSE(d->hasStrokeColor);
  EXPECT_FALSE(d->hasStrokeWidth);
}

TEST(TextBaker, TextModifierOptionalPaintOverrides) {
  // modifierFlags branches — each optional ends up in the wire flags byte
  // and its Property only serialises when the flag is on.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* mod = b.RawDocument()->makeNode<pagx::TextModifier>();
    mod->fillColor = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f};
    mod->strokeColor = pagx::Color{0.0f, 1.0f, 0.0f, 1.0f};
    mod->strokeWidth = 2.5f;
    host.contents.push_back(mod);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  const auto& d =
      std::get<std::unique_ptr<ElementTextModifierData>>(layer->vector->contents[0]->payload);
  EXPECT_TRUE(d->hasFillColor);
  EXPECT_TRUE(d->hasStrokeColor);
  EXPECT_TRUE(d->hasStrokeWidth);
  // Colors go through 8-bit quantisation.
  EXPECT_NEAR(d->fillColor.value.red, 1.0f, 1.0f / 255.0f);
  EXPECT_NEAR(d->strokeColor.value.green, 1.0f, 1.0f / 255.0f);
  EXPECT_FLOAT_EQ(d->strokeWidth.value, 2.5f);
}

// -----------------------------------------------------------------------
// Phase 17 v2.23 — case A (author-authored GlyphRun + embedded <Font>)
// -----------------------------------------------------------------------
//
// Case A writes data->glyphRuns verbatim from pagx::Text.glyphRuns and
// interns each referenced pagx::Font into PAGDocument.embeddedFonts via
// content hash. The tests below exercise:
//   1. Basic fill — glyphRuns populated, text stays empty-position, the
//      referenced font materialises in embeddedFonts with matching data.
//   2. Intern dedup — two GlyphRuns referencing distinct-but-content-equal
//      PAGX Font nodes collapse to a single embeddedFonts entry.

TEST(TextBaker, CaseAAuthorGlyphRunInternsFont) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* font = b.RawDocument()->makeNode<pagx::Font>();
    font->id = "caseAFont";
    font->unitsPerEm = 1024;
    auto* glyph1 = b.RawDocument()->makeNode<pagx::Glyph>();
    glyph1->path = b.RawDocument()->makeNode<pagx::PathData>();
    // Triangle in PAGX y-up source coordinates; the y-flip is an
    // Inflater concern, so the Baker preserves negative-y verbs verbatim.
    glyph1->path->moveTo(0.0f, 0.0f);
    glyph1->path->lineTo(40.0f, 0.0f);
    glyph1->path->lineTo(40.0f, -50.0f);
    glyph1->path->close();
    glyph1->advance = 45.0f;
    font->glyphs.push_back(glyph1);
    auto* glyph2 = b.RawDocument()->makeNode<pagx::Glyph>();
    glyph2->path = b.RawDocument()->makeNode<pagx::PathData>();
    glyph2->path->moveTo(0.0f, 0.0f);
    glyph2->path->lineTo(60.0f, -80.0f);
    glyph2->path->close();
    glyph2->advance = 65.0f;
    font->glyphs.push_back(glyph2);

    auto* text = b.RawDocument()->makeNode<pagx::Text>();
    text->fontSize = 50.0f;
    auto* run = b.RawDocument()->makeNode<pagx::GlyphRun>();
    run->font = font;
    run->fontSize = 50.0f;
    run->glyphs = {1, 2, 1};
    run->x = 12.0f;
    run->y = 34.0f;
    run->xOffsets = {0.0f, 50.0f, 120.0f};
    text->glyphRuns.push_back(run);
    host.contents.push_back(text);
  });
  ASSERT_NE(r.decoded, nullptr);
  // One EmbeddedFont materialised with matching unitsPerEm + glyph table.
  ASSERT_EQ(r.decoded->embeddedFonts.size(), 1u);
  const auto& font = r.decoded->embeddedFonts[0];
  EXPECT_EQ(font->unitsPerEm, 1024u);
  ASSERT_EQ(font->glyphs.size(), 2u);
  EXPECT_FLOAT_EQ(font->glyphs[0].advance, 45.0f);
  EXPECT_FLOAT_EQ(font->glyphs[1].advance, 65.0f);

  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->vector->contents.size(), 1u);
  ASSERT_EQ(layer->vector->contents[0]->type, VectorElementType::Text);
  const auto& d = std::get<std::unique_ptr<ElementTextData>>(layer->vector->contents[0]->payload);
  // Case A writes position = (0, 0); GlyphRun carries the real coordinates.
  EXPECT_FLOAT_EQ(d->position.value.x, 0.0f);
  EXPECT_FLOAT_EQ(d->position.value.y, 0.0f);
  ASSERT_EQ(d->glyphRuns.size(), 1u);
  const auto& gr = d->glyphRuns[0];
  EXPECT_EQ(gr.embeddedFontIndex, 0u);
  EXPECT_FLOAT_EQ(gr.fontSize, 50.0f);
  EXPECT_EQ(gr.glyphs, (std::vector<tgfx::GlyphID>{1, 2, 1}));
  EXPECT_FLOAT_EQ(gr.x, 12.0f);
  EXPECT_FLOAT_EQ(gr.y, 34.0f);
  EXPECT_EQ(gr.xOffsets, (std::vector<float>{0.0f, 50.0f, 120.0f}));
  // Case B fields stay empty so Inflater never confuses branches.
  EXPECT_TRUE(d->shapedGlyphs.empty());
}

TEST(TextBaker, CaseATwoRunsSameFontContentInternDedup) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    // Two PAGX Font nodes with identical unitsPerEm + glyph tables.
    // Content-hash intern should recognise them as the same font and
    // collapse to one embeddedFonts entry.
    auto makeFont = [&](const char* id) {
      auto* f = b.RawDocument()->makeNode<pagx::Font>();
      f->id = id;
      f->unitsPerEm = 1000;
      auto* g = b.RawDocument()->makeNode<pagx::Glyph>();
      g->path = b.RawDocument()->makeNode<pagx::PathData>();
      g->path->moveTo(0.0f, 0.0f);
      g->path->lineTo(30.0f, 0.0f);
      g->path->lineTo(30.0f, -40.0f);
      g->path->close();
      g->advance = 32.0f;
      f->glyphs.push_back(g);
      return f;
    };
    auto* fontA = makeFont("fontA");
    auto* fontB = makeFont("fontB");

    auto makeRun = [&](pagx::Font* font, float xpos) {
      auto* run = b.RawDocument()->makeNode<pagx::GlyphRun>();
      run->font = font;
      run->fontSize = 40.0f;
      run->glyphs = {1};
      run->x = xpos;
      return run;
    };
    auto* text = b.RawDocument()->makeNode<pagx::Text>();
    text->fontSize = 40.0f;
    text->glyphRuns.push_back(makeRun(fontA, 0.0f));
    text->glyphRuns.push_back(makeRun(fontB, 50.0f));
    host.contents.push_back(text);
  });
  ASSERT_NE(r.decoded, nullptr);
  // Content hash dedup → single EmbeddedFont entry despite two PAGX Fonts.
  EXPECT_EQ(r.decoded->embeddedFonts.size(), 1u);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  const auto& d = std::get<std::unique_ptr<ElementTextData>>(layer->vector->contents[0]->payload);
  ASSERT_EQ(d->glyphRuns.size(), 2u);
  EXPECT_EQ(d->glyphRuns[0].embeddedFontIndex, 0u);
  EXPECT_EQ(d->glyphRuns[1].embeddedFontIndex, 0u);
}

// -----------------------------------------------------------------------
// Phase 18 v2.24 — TextModifier RangeSelector serialization
// -----------------------------------------------------------------------
//
// The Phase 17 BakeTextModifier dropped TextModifier.selectors entirely
// (`(void)src.selectors;`); Phase 18 wires the selector vector through to
// PAG so the Inflater (and tgfx::TextModifier::apply at render time) can
// drive the per-glyph factor curve. This test exercises the round-trip:
// build a PAGX TextModifier with one RangeSelector, Bake → encode →
// decode, and assert all 11 RangeSelector fields survived.

TEST(TextBaker, TextModifierRangeSelectorRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* mod = b.RawDocument()->makeNode<pagx::TextModifier>();
    mod->position = pagx::Point{0.0f, -30.0f};
    auto* sel = b.RawDocument()->makeNode<pagx::RangeSelector>();
    sel->start = 0.25f;
    sel->end = 0.75f;
    sel->offset = 0.1f;
    sel->unit = pagx::SelectorUnit::Index;
    sel->shape = pagx::SelectorShape::Triangle;
    sel->easeIn = 0.4f;
    sel->easeOut = 0.6f;
    sel->mode = pagx::SelectorMode::Intersect;
    sel->weight = 0.8f;
    sel->randomOrder = true;
    sel->randomSeed = 4242;
    mod->selectors.push_back(sel);
    host.contents.push_back(mod);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->vector->contents.size(), 1u);
  ASSERT_EQ(layer->vector->contents[0]->type, VectorElementType::TextModifier);
  const auto& d =
      std::get<std::unique_ptr<ElementTextModifierData>>(layer->vector->contents[0]->payload);
  // Transform survives (sanity).
  EXPECT_FLOAT_EQ(d->position.value.y, -30.0f);
  // Selector materialised.
  ASSERT_EQ(d->rangeSelectors.size(), 1u);
  const auto& s = *d->rangeSelectors[0];
  EXPECT_FLOAT_EQ(s.start.value, 0.25f);
  EXPECT_FLOAT_EQ(s.end.value, 0.75f);
  EXPECT_FLOAT_EQ(s.offset.value, 0.1f);
  EXPECT_EQ(s.unit, tgfx::SelectorUnit::Index);
  EXPECT_EQ(s.shape, tgfx::SelectorShape::Triangle);
  EXPECT_FLOAT_EQ(s.easeIn.value, 0.4f);
  EXPECT_FLOAT_EQ(s.easeOut.value, 0.6f);
  EXPECT_EQ(s.mode, tgfx::SelectorMode::Intersect);
  EXPECT_FLOAT_EQ(s.weight.value, 0.8f);
  EXPECT_TRUE(s.randomOrder);
  EXPECT_EQ(s.randomSeed, 4242);
}

}  // namespace pagx::pag
