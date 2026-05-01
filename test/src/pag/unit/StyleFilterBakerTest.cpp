// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 7 StyleFilterBaker — end-to-end Bake + Encode + Decode roundtrip
// for 5 filter subtypes and 3 style subtypes, plus multi-entry containers
// and the layerFlags::HasFilters / HasStyles presence-bit semantics. Each
// TEST constructs a minimal PAGX doc, bakes it, byte-wise encodes via
// Codec::Encode, decodes, and asserts the relevant field survived. This
// tight spiral catches drift between StyleFilterBaker, StyleFilterTags, and
// the LayerBlock sub-Tag dispatch in CodecTagsLayer.
#include <memory>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::pag {

namespace {

struct RoundTripResult {
  std::unique_ptr<PAGDocument> baked;
  std::unique_ptr<PAGDocument> decoded;
};

// Build a PAGX document with one layer where `setup` can attach filters /
// styles, bake it through Phase 3-7, then encode/decode. Reflects what the
// downstream test asserts on.
template <typename Setup>
RoundTripResult BakeAndRoundTrip(Setup setup) {
  auto builder = pagx::test::PAGXBuilder::Make();
  auto layer = builder.AddLayer().Name("filter_host");
  setup(builder, *layer.layer());
  builder.RawDocument()->applyLayout();
  auto bake = Bake(*builder.RawDocument());
  RoundTripResult r;
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
// Filter roundtrips — one per subtype
// -----------------------------------------------------------------------

TEST(StyleFilterBaker, BlurFilterRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* f = b.RawDocument()->makeNode<pagx::BlurFilter>();
    f->blurX = 8.0f;
    f->blurY = 12.0f;
    f->tileMode = pagx::TileMode::Repeat;
    host.filters.push_back(f);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  const auto& f = *layer->filters[0];
  EXPECT_EQ(f.type, LayerFilterType::Blur);
  EXPECT_FLOAT_EQ(f.blurX.value, 8.0f);
  EXPECT_FLOAT_EQ(f.blurY.value, 12.0f);
  EXPECT_EQ(f.tileMode, tgfx::TileMode::Repeat);
}

TEST(StyleFilterBaker, DropShadowFilterRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* f = b.RawDocument()->makeNode<pagx::DropShadowFilter>();
    f->offsetX = 4.0f;
    f->offsetY = -3.0f;
    f->blurX = 2.0f;
    f->blurY = 2.0f;
    f->color = pagx::Color{0.25f, 0.5f, 0.75f, 1.0f};
    f->shadowOnly = true;
    host.filters.push_back(f);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  const auto& f = *layer->filters[0];
  EXPECT_EQ(f.type, LayerFilterType::DropShadow);
  EXPECT_FLOAT_EQ(f.offsetX.value, 4.0f);
  EXPECT_FLOAT_EQ(f.offsetY.value, -3.0f);
  // Color values traverse 8-bit quantisation.
  EXPECT_NEAR(f.color.value.red, 0.25f, 1.0f / 255.0f);
  EXPECT_NEAR(f.color.value.blue, 0.75f, 1.0f / 255.0f);
  EXPECT_TRUE(f.shadowOnly);
}

TEST(StyleFilterBaker, InnerShadowFilterRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* f = b.RawDocument()->makeNode<pagx::InnerShadowFilter>();
    f->offsetX = 1.0f;
    f->offsetY = 2.0f;
    f->blurX = 3.0f;
    f->blurY = 4.0f;
    f->color = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f};
    host.filters.push_back(f);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  EXPECT_EQ(layer->filters[0]->type, LayerFilterType::InnerShadow);
  EXPECT_FLOAT_EQ(layer->filters[0]->offsetY.value, 2.0f);
  EXPECT_FLOAT_EQ(layer->filters[0]->blurX.value, 3.0f);
}

TEST(StyleFilterBaker, ColorMatrixFilterRoundTrip) {
  // Non-identity matrix so we can assert individual cell values survive.
  std::array<float, 20> m = {
      0.5f, 0.1f, 0.0f, 0.0f, 0.1f,  0.0f, 0.7f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.9f, 0.0f, 0.05f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };
  auto r = BakeAndRoundTrip([&](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* f = b.RawDocument()->makeNode<pagx::ColorMatrixFilter>();
    f->matrix = m;
    host.filters.push_back(f);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  const auto& f = *layer->filters[0];
  EXPECT_EQ(f.type, LayerFilterType::ColorMatrix);
  for (size_t i = 0; i < 20; ++i) {
    EXPECT_FLOAT_EQ(f.colorMatrix.value[i], m[i]) << "cell " << i;
  }
}

TEST(StyleFilterBaker, BlendFilterRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* f = b.RawDocument()->makeNode<pagx::BlendFilter>();
    f->color = pagx::Color{0.2f, 0.4f, 0.6f, 1.0f};
    f->blendMode = pagx::BlendMode::Multiply;
    host.filters.push_back(f);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  const auto& f = *layer->filters[0];
  EXPECT_EQ(f.type, LayerFilterType::Blend);
  EXPECT_NEAR(f.blendColor.value.green, 0.4f, 1.0f / 255.0f);
  EXPECT_EQ(f.blendFilterMode, tgfx::BlendMode::Multiply);
}

// -----------------------------------------------------------------------
// Style roundtrips — one per subtype
// -----------------------------------------------------------------------

TEST(StyleFilterBaker, DropShadowStyleRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* s = b.RawDocument()->makeNode<pagx::DropShadowStyle>();
    s->offsetX = 2.0f;
    s->offsetY = 3.0f;
    s->blurX = 4.0f;
    s->blurY = 5.0f;
    s->color = pagx::Color{0.1f, 0.2f, 0.3f, 1.0f};
    s->showBehindLayer = false;  // non-default
    s->blendMode = pagx::BlendMode::Screen;
    s->excludeChildEffects = true;
    host.styles.push_back(s);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->styles.size(), 1u);
  const auto& s = *layer->styles[0];
  EXPECT_EQ(s.type, LayerStyleType::DropShadow);
  EXPECT_FLOAT_EQ(s.offsetX.value, 2.0f);
  EXPECT_FLOAT_EQ(s.blurY.value, 5.0f);
  EXPECT_FALSE(s.showBehindLayer);
  EXPECT_EQ(s.blendMode, tgfx::BlendMode::Screen);
  EXPECT_TRUE(s.excludeChildEffects);
}

TEST(StyleFilterBaker, InnerShadowStyleRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* s = b.RawDocument()->makeNode<pagx::InnerShadowStyle>();
    s->offsetX = 1.5f;
    s->offsetY = 0.5f;
    s->blurX = 2.0f;
    s->blurY = 2.0f;
    s->color = pagx::Color{0.0f, 1.0f, 0.0f, 1.0f};
    host.styles.push_back(s);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->styles.size(), 1u);
  const auto& s = *layer->styles[0];
  EXPECT_EQ(s.type, LayerStyleType::InnerShadow);
  EXPECT_FLOAT_EQ(s.offsetX.value, 1.5f);
  EXPECT_NEAR(s.color.value.green, 1.0f, 1.0f / 255.0f);
}

TEST(StyleFilterBaker, BackgroundBlurStyleRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* s = b.RawDocument()->makeNode<pagx::BackgroundBlurStyle>();
    s->blurX = 10.0f;
    s->blurY = 10.0f;
    s->tileMode = pagx::TileMode::Clamp;
    host.styles.push_back(s);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->styles.size(), 1u);
  const auto& s = *layer->styles[0];
  EXPECT_EQ(s.type, LayerStyleType::BackgroundBlur);
  EXPECT_FLOAT_EQ(s.blurX.value, 10.0f);
  EXPECT_EQ(s.tileMode, tgfx::TileMode::Clamp);
}

// -----------------------------------------------------------------------
// Container semantics
// -----------------------------------------------------------------------

TEST(StyleFilterBaker, MultipleFiltersPreserveOrder) {
  // Baker + codec must preserve the declared order so Inflater later applies
  // filters in the source-document order.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* blur = b.RawDocument()->makeNode<pagx::BlurFilter>();
    blur->blurX = 5.0f;
    auto* drop = b.RawDocument()->makeNode<pagx::DropShadowFilter>();
    drop->offsetX = 10.0f;
    auto* inner = b.RawDocument()->makeNode<pagx::InnerShadowFilter>();
    inner->offsetY = 20.0f;
    host.filters.push_back(blur);
    host.filters.push_back(drop);
    host.filters.push_back(inner);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 3u);
  EXPECT_EQ(layer->filters[0]->type, LayerFilterType::Blur);
  EXPECT_EQ(layer->filters[1]->type, LayerFilterType::DropShadow);
  EXPECT_EQ(layer->filters[2]->type, LayerFilterType::InnerShadow);
  EXPECT_FLOAT_EQ(layer->filters[0]->blurX.value, 5.0f);
  EXPECT_FLOAT_EQ(layer->filters[1]->offsetX.value, 10.0f);
  EXPECT_FLOAT_EQ(layer->filters[2]->offsetY.value, 20.0f);
}

TEST(StyleFilterBaker, FiltersAndStylesCoexistOnSameLayer) {
  // Layers can carry both filters and styles at once — the LayerBlock
  // layerFlags::HasFilters and HasStyles bits gate each sub-Tag independently.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* blur = b.RawDocument()->makeNode<pagx::BlurFilter>();
    blur->blurX = 6.0f;
    host.filters.push_back(blur);

    auto* drop = b.RawDocument()->makeNode<pagx::DropShadowStyle>();
    drop->offsetX = 7.0f;
    host.styles.push_back(drop);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->filters.size(), 1u);
  ASSERT_EQ(layer->styles.size(), 1u);
  EXPECT_EQ(layer->filters[0]->type, LayerFilterType::Blur);
  EXPECT_EQ(layer->styles[0]->type, LayerStyleType::DropShadow);
  EXPECT_FLOAT_EQ(layer->filters[0]->blurX.value, 6.0f);
  EXPECT_FLOAT_EQ(layer->styles[0]->offsetX.value, 7.0f);
}

TEST(StyleFilterBaker, EmptyContainersSkipSubTagEntirely) {
  // A layer with no filters / styles must decode identically — the
  // HasFilters / HasStyles bits stay off and the Reader takes neither
  // LayerFilters nor LayerStyles branch.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& /*b*/, pagx::Layer& /*host*/) {
    // intentionally leave filters / styles empty
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* layer = FirstLayer(*r.decoded);
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->filters.empty());
  EXPECT_TRUE(layer->styles.empty());
}

}  // namespace pagx::pag
