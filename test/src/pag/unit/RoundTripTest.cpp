// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 4a RoundTrip — verifies Encode -> bytes -> Decode reproduces the
// FileHeader / CompositionList / Composition framing fields. Layer-level
// roundtrip lands in Phase 4b alongside LayerBlock decode.
#include "gtest/gtest.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"

using namespace pagx::pag;

namespace {

PAGDocument MakeMinimalDoc() {
  PAGDocument doc;
  doc.header.width = 1920.0f;
  doc.header.height = 1080.0f;
  doc.header.backgroundColor = ::tgfx::Color{0.25f, 0.5f, 0.75f, 1.0f};
  doc.header.frameRate = ::pag::Ratio{30, 1};
  doc.header.duration = 90;
  return doc;
}

}  // namespace

TEST(RoundTrip, EmptyDocFramingPreserved) {
  PAGDocument doc = MakeMinimalDoc();

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);
  EXPECT_TRUE(enc.warnings.empty());

  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr) << "Decode failed";
  EXPECT_TRUE(dec.errors.empty());

  EXPECT_FLOAT_EQ(dec.doc->header.width, 1920.0f);
  EXPECT_FLOAT_EQ(dec.doc->header.height, 1080.0f);
  // Color components are u8-quantised on the wire (§D.5 background uses
  // ReadColor / WriteColor); accept up to 1/255 loss.
  EXPECT_NEAR(dec.doc->header.backgroundColor.red, 0.25f, 1.0f / 255.0f);
  EXPECT_NEAR(dec.doc->header.backgroundColor.green, 0.5f, 1.0f / 255.0f);
  EXPECT_NEAR(dec.doc->header.backgroundColor.blue, 0.75f, 1.0f / 255.0f);
  EXPECT_NEAR(dec.doc->header.backgroundColor.alpha, 1.0f, 1.0f / 255.0f);
  EXPECT_EQ(dec.doc->header.frameRate.numerator, 30);
  EXPECT_EQ(dec.doc->header.frameRate.denominator, 1u);
  EXPECT_EQ(dec.doc->header.duration, 90u);
  EXPECT_EQ(dec.doc->compositions.size(), 0u);
}

TEST(RoundTrip, SingleEmptyCompositionPreserved) {
  PAGDocument doc = MakeMinimalDoc();
  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 1920;
  comp->height = 1080;
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);

  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  EXPECT_TRUE(dec.errors.empty());
  ASSERT_EQ(dec.doc->compositions.size(), 1u);
  EXPECT_EQ(dec.doc->compositions[0]->id, "main");
  EXPECT_EQ(dec.doc->compositions[0]->width, 1920u);
  EXPECT_EQ(dec.doc->compositions[0]->height, 1080u);
  EXPECT_EQ(dec.doc->compositions[0]->layers.size(), 0u);
}

TEST(RoundTrip, MultipleCompositionsPreserveOrder) {
  PAGDocument doc = MakeMinimalDoc();
  for (int i = 0; i < 3; ++i) {
    auto comp = std::make_unique<Composition>();
    comp->id = std::string("comp_") + std::to_string(i);
    comp->width = static_cast<uint32_t>(100 * (i + 1));
    comp->height = static_cast<uint32_t>(50 * (i + 1));
    doc.compositions.push_back(std::move(comp));
  }

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  ASSERT_EQ(dec.doc->compositions.size(), 3u);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(dec.doc->compositions[i]->id, std::string("comp_") + std::to_string(i));
    EXPECT_EQ(dec.doc->compositions[i]->width, static_cast<uint32_t>(100 * (i + 1)));
    EXPECT_EQ(dec.doc->compositions[i]->height, static_cast<uint32_t>(50 * (i + 1)));
  }
}

TEST(RoundTrip, ZeroDimensionCompositionWarnedAndClamped) {
  PAGDocument doc = MakeMinimalDoc();
  auto comp = std::make_unique<Composition>();
  comp->id = "zero";
  comp->width = 0;
  comp->height = 0;
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  EXPECT_TRUE(dec.errors.empty());
  EXPECT_GE(dec.warnings.size(), 2u);  // one for width, one for height
  EXPECT_EQ(dec.doc->compositions[0]->width, 1u);
  EXPECT_EQ(dec.doc->compositions[0]->height, 1u);
}
