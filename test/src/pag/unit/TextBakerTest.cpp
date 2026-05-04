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
// Pre-shaped Text
// -----------------------------------------------------------------------

TEST(TextBaker, DISABLED_PreShapedTextSingleRunRoundTrip) {
  // Phase 16 (v2.20): runtime-shape mode drops pre-shaped GlyphRun/Font
  // assets. Rewrite in Phase 16.2+ against the new ElementTextData fields
  // (text / fontFamily / fontStyle / fontSize / direction / ...).
  GTEST_SKIP() << "Disabled during Phase 16 rework of pre-shaped text pipeline.";
}

TEST(TextBaker, DISABLED_PreShapedTextMultipleRunsShareFont) {
  // Phase 16 (v2.20): font interning disappears along with the fonts[]
  // asset table; runtime-shape mode stores family/style strings inline.
  GTEST_SKIP() << "Disabled during Phase 16 rework of font asset interning.";
}

TEST(TextBaker, DISABLED_TextWithNullFontEmitsWarning) {
  // Phase 16 (v2.20): FontSourceMissing semantics move with the runtime
  // shaper; rewrite once the new warning contract is defined.
  GTEST_SKIP() << "Disabled during Phase 16 rework of font-missing diagnostics.";
}

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

}  // namespace pagx::pag
