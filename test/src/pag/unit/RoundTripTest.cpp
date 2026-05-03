// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 4a RoundTrip — verifies Encode -> bytes -> Decode reproduces the
// FileHeader / CompositionList / Composition framing fields. Layer-level
// roundtrip lands in Phase 4b alongside LayerBlock decode.
#include <cstring>
#include "gtest/gtest.h"
#include "pagx/Diagnostic.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"
#include "tgfx/core/Data.h"

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

// =============================================================================
// Phase 4b: ImageAssetTable / FontAssetTable / LayerBlock / CompositionRef
// =============================================================================

TEST(RoundTrip, ImageAssetTablePreserved) {
  PAGDocument doc = MakeMinimalDoc();
  auto img = std::make_unique<ImageAsset>();
  img->width = 256;
  img->height = 128;
  img->kind = ImageAssetKind::Raster;
  // Synthetic PNG-ish bytes: enough to confirm the wire roundtrips.
  static constexpr uint8_t kSampleData[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  img->data = ::tgfx::Data::MakeWithCopy(kSampleData, sizeof(kSampleData));
  doc.images.push_back(std::move(img));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  EXPECT_TRUE(dec.errors.empty());
  ASSERT_EQ(dec.doc->images.size(), 1u);
  EXPECT_EQ(dec.doc->images[0]->width, 256);
  EXPECT_EQ(dec.doc->images[0]->height, 128);
  EXPECT_EQ(dec.doc->images[0]->kind, ImageAssetKind::Raster);
  ASSERT_NE(dec.doc->images[0]->data, nullptr);
  EXPECT_EQ(dec.doc->images[0]->data->size(), sizeof(kSampleData));
  EXPECT_EQ(0, std::memcmp(dec.doc->images[0]->data->data(), kSampleData, sizeof(kSampleData)));
}

TEST(RoundTrip, FontAssetTablePreserved) {
  PAGDocument doc = MakeMinimalDoc();
  auto sysFont = std::make_unique<FontAsset>();
  sysFont->kind = FontSourceKind::System;
  sysFont->family = "Helvetica";
  sysFont->style = "Bold";
  doc.fonts.push_back(std::move(sysFont));

  auto embeddedFont = std::make_unique<FontAsset>();
  embeddedFont->kind = FontSourceKind::Embedded;
  embeddedFont->family = "MyFont";
  embeddedFont->style = "Regular";
  static constexpr uint8_t kFontBytes[] = {'O', 'T', 'T', 'O', 0x00, 0x01};
  embeddedFont->data = ::tgfx::Data::MakeWithCopy(kFontBytes, sizeof(kFontBytes));
  embeddedFont->axes.push_back(FontAxis{0x77676874u, 400.0f, 100.0f, 900.0f});  // 'wght'
  doc.fonts.push_back(std::move(embeddedFont));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  ASSERT_EQ(dec.doc->fonts.size(), 2u);
  EXPECT_EQ(dec.doc->fonts[0]->family, "Helvetica");
  EXPECT_EQ(dec.doc->fonts[0]->style, "Bold");
  EXPECT_EQ(dec.doc->fonts[0]->kind, FontSourceKind::System);
  EXPECT_EQ(dec.doc->fonts[1]->family, "MyFont");
  EXPECT_EQ(dec.doc->fonts[1]->kind, FontSourceKind::Embedded);
  ASSERT_NE(dec.doc->fonts[1]->data, nullptr);
  EXPECT_EQ(dec.doc->fonts[1]->data->size(), sizeof(kFontBytes));
  ASSERT_EQ(dec.doc->fonts[1]->axes.size(), 1u);
  EXPECT_EQ(dec.doc->fonts[1]->axes[0].tag, 0x77676874u);
  EXPECT_FLOAT_EQ(dec.doc->fonts[1]->axes[0].defaultValue, 400.0f);
  EXPECT_FLOAT_EQ(dec.doc->fonts[1]->axes[0].minValue, 100.0f);
  EXPECT_FLOAT_EQ(dec.doc->fonts[1]->axes[0].maxValue, 900.0f);
}

TEST(RoundTrip, LayerBlockBasicFields) {
  PAGDocument doc = MakeMinimalDoc();
  auto comp = std::make_unique<Composition>();
  comp->id = "main";
  comp->width = 100;
  comp->height = 100;

  auto layer = std::make_unique<Layer>();
  layer->type = LayerType::Layer;
  layer->name = "root_layer";
  layer->startTime = 10;
  layer->duration = 60;
  layer->stretch = ::pag::Ratio{2, 1};
  layer->preserve3D = true;
  layer->allowsGroupOpacity = false;
  layer->alpha = MakeProp(0.5f);
  comp->layers.push_back(std::move(layer));

  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  EXPECT_TRUE(dec.errors.empty());
  ASSERT_EQ(dec.doc->compositions.size(), 1u);
  ASSERT_EQ(dec.doc->compositions[0]->layers.size(), 1u);
  const Layer& l = *dec.doc->compositions[0]->layers[0];
  EXPECT_EQ(l.type, LayerType::Layer);
  EXPECT_EQ(l.name, "root_layer");
  EXPECT_EQ(l.startTime, 10u);
  EXPECT_EQ(l.duration, 60u);
  EXPECT_EQ(l.stretch.numerator, 2);
  EXPECT_EQ(l.stretch.denominator, 1u);
  EXPECT_TRUE(l.preserve3D);
  EXPECT_FALSE(l.allowsGroupOpacity);
  EXPECT_FLOAT_EQ(l.alpha.value, 0.5f);
}

TEST(RoundTrip, LayerBlockChildrenRecursion) {
  PAGDocument doc = MakeMinimalDoc();
  auto comp = std::make_unique<Composition>();
  comp->id = "tree";
  comp->width = 100;
  comp->height = 100;

  auto root = std::make_unique<Layer>();
  root->type = LayerType::Layer;
  root->name = "root";
  for (int i = 0; i < 3; ++i) {
    auto child = std::make_unique<Layer>();
    child->type = LayerType::Layer;
    child->name = std::string("child_") + std::to_string(i);
    root->children.push_back(std::move(child));
  }
  comp->layers.push_back(std::move(root));
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  ASSERT_EQ(dec.doc->compositions[0]->layers.size(), 1u);
  ASSERT_EQ(dec.doc->compositions[0]->layers[0]->children.size(), 3u);
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(dec.doc->compositions[0]->layers[0]->children[i]->name,
              std::string("child_") + std::to_string(i));
  }
}

TEST(RoundTrip, CompositionRefPayload) {
  PAGDocument doc = MakeMinimalDoc();
  auto leafComp = std::make_unique<Composition>();
  leafComp->id = "leaf";
  leafComp->width = 50;
  leafComp->height = 50;
  doc.compositions.push_back(std::move(leafComp));

  auto rootComp = std::make_unique<Composition>();
  rootComp->id = "root";
  rootComp->width = 100;
  rootComp->height = 100;

  auto refLayer = std::make_unique<Layer>();
  refLayer->type = LayerType::CompositionRef;
  refLayer->name = "ref_to_leaf";
  refLayer->compositionIndex = 0;  // points to "leaf" — written before "root"
  rootComp->layers.push_back(std::move(refLayer));

  doc.compositions.push_back(std::move(rootComp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  EXPECT_TRUE(dec.errors.empty());
  ASSERT_EQ(dec.doc->compositions.size(), 2u);
  ASSERT_EQ(dec.doc->compositions[1]->layers.size(), 1u);
  EXPECT_EQ(dec.doc->compositions[1]->layers[0]->type, LayerType::CompositionRef);
  EXPECT_EQ(dec.doc->compositions[1]->layers[0]->compositionIndex, 0u);
}

// Phase 11.5 regression: the Baker depth-first walks PAGX compositions and
// interns children before the parent finishes, so the root composition
// (compositions[0]) routinely holds a CompositionRef with compositionIndex
// pointing at a *later* composition slot (1, 2, ...). Pre-fix this hit
// `InvalidCompositionIndex=306` in Decode because the range check compared
// against the count of already-decoded compositions (0 when processing the
// root) instead of the total declared count. This test specifically
// constructs a forward-reference so the regression stays caught.
TEST(RoundTrip, CompositionRefForwardReferenceSupported) {
  PAGDocument doc = MakeMinimalDoc();

  // compositions[0] = root, carrying a CompositionRef(compositionIndex=1).
  auto rootComp = std::make_unique<Composition>();
  rootComp->id = "root";
  rootComp->width = 100;
  rootComp->height = 100;

  auto refLayer = std::make_unique<Layer>();
  refLayer->type = LayerType::CompositionRef;
  refLayer->name = "ref_to_leaf_forward";
  refLayer->compositionIndex = 1;  // forward-reference: leaf hasn't been
                                   // pushed into the vector yet.
  rootComp->layers.push_back(std::move(refLayer));
  doc.compositions.push_back(std::move(rootComp));

  // compositions[1] = leaf (serialised after root).
  auto leafComp = std::make_unique<Composition>();
  leafComp->id = "leaf";
  leafComp->width = 50;
  leafComp->height = 50;
  doc.compositions.push_back(std::move(leafComp));

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);
  EXPECT_TRUE(enc.warnings.empty());

  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr) << "forward CompositionRef should not fatal";
  EXPECT_TRUE(dec.errors.empty());
  ASSERT_EQ(dec.doc->compositions.size(), 2u);
  ASSERT_EQ(dec.doc->compositions[0]->layers.size(), 1u);
  EXPECT_EQ(dec.doc->compositions[0]->layers[0]->type, LayerType::CompositionRef);
  EXPECT_EQ(dec.doc->compositions[0]->layers[0]->compositionIndex, 1u);
}

// Phase 11.5 negative: forward refs within the declared total must pass,
// but refs *beyond* the total (idx == declared count) still fatal with
// `InvalidCompositionIndex=306` since the Inflater would deref a slot that
// will never exist on the wire.
TEST(RoundTrip, CompositionRefBeyondDeclaredCountStillFatal) {
  PAGDocument doc = MakeMinimalDoc();
  auto rootComp = std::make_unique<Composition>();
  rootComp->width = 100;
  rootComp->height = 100;
  auto refLayer = std::make_unique<Layer>();
  refLayer->type = LayerType::CompositionRef;
  refLayer->compositionIndex = 5;  // only 1 composition declared; 5 is OOB.
  rootComp->layers.push_back(std::move(refLayer));
  doc.compositions.push_back(std::move(rootComp));

  auto enc = Codec::Encode(doc);
  ASSERT_NE(enc.bytes, nullptr);

  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  EXPECT_EQ(dec.doc, nullptr);
  ASSERT_FALSE(dec.errors.empty());
  bool sawInvalidCompositionIndex = false;
  for (const auto& e : dec.errors) {
    if (e.code == pagx::DiagnosticCode::InvalidCompositionIndex) {
      sawInvalidCompositionIndex = true;
      break;
    }
  }
  EXPECT_TRUE(sawInvalidCompositionIndex);
}

TEST(RoundTrip, LayerTransformDefaultsCollapsed) {
  // A layer with all-default LayerTransform Properties should still encode +
  // decode losslessly. The isDefault bit path is the most-trodden branch in
  // production exports, so failing here would cascade through every doc.
  PAGDocument doc = MakeMinimalDoc();
  auto comp = std::make_unique<Composition>();
  comp->width = 100;
  comp->height = 100;
  auto layer = std::make_unique<Layer>();
  layer->type = LayerType::Layer;
  comp->layers.push_back(std::move(layer));
  doc.compositions.push_back(std::move(comp));

  auto enc = Codec::Encode(doc);
  auto dec = Codec::Decode(enc.bytes->data(), enc.bytes->length());
  ASSERT_NE(dec.doc, nullptr);
  ASSERT_EQ(dec.doc->compositions[0]->layers.size(), 1u);
  const Layer& l = *dec.doc->compositions[0]->layers[0];
  EXPECT_TRUE(l.visible.value);
  EXPECT_FLOAT_EQ(l.alpha.value, 1.0f);
  EXPECT_EQ(l.blendMode.value, ::tgfx::BlendMode::SrcOver);
}
