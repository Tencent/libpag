// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 9 Parity tests: for the same PAGXDocument, verify the full
// encode-roundtrip path (Baker → Codec::Encode → Codec::Decode → Inflater)
// produces a tgfx::Layer tree structurally equivalent to the direct
// LayerBuilder::Build output. Structural comparison checks layer type
// identity, child counts, alpha, blend mode, matrix translate, and
// VectorElement counts — enough to catch any wire-format drift between
// encode and decode without depending on pixel-level baselines (that's
// Phase 15's job).
#include <memory>
#include <utility>
#include <vector>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/LayerInflater.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/VectorLayer.h"

namespace pagx::pag {

namespace {

struct Mismatch {
  std::string where;
  std::string detail;
};

// Recursive comparator. Appends every mismatch to `out` so the caller can
// log all problems at once. Matches the comparable surface of tgfx::Layer
// that LayerBuilder and Inflater both set (see
// LayerBuilder::applyLayerAttributes and LayerInflater::applyCommon).
void CompareLayerTree(const std::shared_ptr<tgfx::Layer>& a, const std::shared_ptr<tgfx::Layer>& b,
                      const std::string& path, std::vector<Mismatch>* out) {
  if (a == nullptr || b == nullptr) {
    if (a != b) {
      out->push_back({path, a ? "b is null" : "a is null"});
    }
    return;
  }
  if (a->type() != b->type()) {
    out->push_back({path, "type differs"});
  }
  if (a->alpha() != b->alpha()) {
    out->push_back({path, "alpha differs"});
  }
  if (a->blendMode() != b->blendMode()) {
    out->push_back({path, "blendMode differs"});
  }
  if (a->matrix().getTranslateX() != b->matrix().getTranslateX() ||
      a->matrix().getTranslateY() != b->matrix().getTranslateY()) {
    out->push_back({path, "matrix translate differs"});
  }

  // VectorLayer contents count.
  auto aVec = std::dynamic_pointer_cast<tgfx::VectorLayer>(a);
  auto bVec = std::dynamic_pointer_cast<tgfx::VectorLayer>(b);
  if ((aVec == nullptr) != (bVec == nullptr)) {
    out->push_back({path, "only one side is VectorLayer"});
  } else if (aVec != nullptr && bVec != nullptr) {
    if (aVec->contents().size() != bVec->contents().size()) {
      out->push_back({path, "VectorLayer contents size differs"});
    }
  }

  if (a->children().size() != b->children().size()) {
    out->push_back({path, "children count differs"});
    return;  // children sizes mismatch — stop descending to avoid confusing
             // additional diagnostics from pairwise recursion.
  }
  for (size_t i = 0; i < a->children().size(); ++i) {
    CompareLayerTree(a->children()[i], b->children()[i], path + "/" + std::to_string(i), out);
  }
}

// End-to-end roundtrip: Baker → Encode → Decode → Inflate → tgfx::Layer.
std::shared_ptr<tgfx::Layer> RoundTripToLayer(pagx::PAGXDocument* pagxDoc) {
  auto bakeResult = Bake(*pagxDoc);
  if (bakeResult.doc == nullptr) {
    return nullptr;
  }
  auto encodeResult = Codec::Encode(*bakeResult.doc);
  if (encodeResult.bytes == nullptr || encodeResult.bytes->length() == 0) {
    return nullptr;
  }
  auto decodeResult = Codec::Decode(encodeResult.bytes->data(), encodeResult.bytes->length());
  if (decodeResult.doc == nullptr) {
    return nullptr;
  }
  auto inflateResult = LayerInflater::Inflate(std::move(decodeResult.doc));
  return inflateResult.layer;
}

}  // namespace

// ---------------------------------------------------------------------------
// Scenario 1: single VectorLayer holding one Rectangle + SolidColor fill.
// Both pipelines should emit the same layer-tree shape.
// ---------------------------------------------------------------------------

TEST(LayerInflaterParity, SingleVectorLayerWithRectAndFill) {
  auto builder = pagx::test::PAGXBuilder::Make(640, 480);
  auto layerHandle = builder.AddLayer().Name("host");
  auto* pagxLayer = layerHandle.layer();

  auto* rect = builder.RawDocument()->makeNode<pagx::Rectangle>();
  rect->position = pagx::Point{10.0f, 20.0f};
  rect->size = pagx::Size{100.0f, 50.0f};
  pagxLayer->contents.push_back(rect);

  auto* solid = builder.RawDocument()->makeNode<pagx::SolidColor>();
  solid->color = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f};
  auto* fill = builder.RawDocument()->makeNode<pagx::Fill>();
  fill->color = solid;
  pagxLayer->contents.push_back(fill);

  builder.RawDocument()->applyLayout();
  auto direct = pagx::LayerBuilder::Build(builder.RawDocument().get());
  auto roundTripped = RoundTripToLayer(builder.RawDocument().get());
  ASSERT_NE(direct, nullptr);
  ASSERT_NE(roundTripped, nullptr);

  std::vector<Mismatch> mismatches;
  CompareLayerTree(direct, roundTripped, "root", &mismatches);
  for (const auto& m : mismatches) {
    ADD_FAILURE() << m.where << ": " << m.detail;
  }
}

// ---------------------------------------------------------------------------
// Scenario 2: parent Layer with two empty children. Shape check only (no
// vector contents) — exercises child count + plain-Layer path.
// ---------------------------------------------------------------------------

TEST(LayerInflaterParity, ParentWithTwoChildren) {
  auto builder = pagx::test::PAGXBuilder::Make(800, 600);
  auto parent = builder.AddLayer().Name("parent");
  parent.AddChild().Name("c0");
  parent.AddChild().Name("c1");

  builder.RawDocument()->applyLayout();
  auto direct = pagx::LayerBuilder::Build(builder.RawDocument().get());
  auto roundTripped = RoundTripToLayer(builder.RawDocument().get());
  ASSERT_NE(direct, nullptr);
  ASSERT_NE(roundTripped, nullptr);

  std::vector<Mismatch> mismatches;
  CompareLayerTree(direct, roundTripped, "root", &mismatches);
  for (const auto& m : mismatches) {
    ADD_FAILURE() << m.where << ": " << m.detail;
  }
}

// ---------------------------------------------------------------------------
// Scenario 3: Layer with custom alpha + translate — attribute roundtrip
// (matrix / alpha / blendMode are the three we compare).
// ---------------------------------------------------------------------------

TEST(LayerInflaterParity, AttributesRoundTrip) {
  auto builder = pagx::test::PAGXBuilder::Make(400, 300);
  builder.AddLayer()
      .Name("attr")
      .Alpha(0.75f)
      .Position(15.0f, 25.0f)
      .BlendMode(pagx::BlendMode::Multiply);

  builder.RawDocument()->applyLayout();
  auto direct = pagx::LayerBuilder::Build(builder.RawDocument().get());
  auto roundTripped = RoundTripToLayer(builder.RawDocument().get());
  ASSERT_NE(direct, nullptr);
  ASSERT_NE(roundTripped, nullptr);

  std::vector<Mismatch> mismatches;
  CompareLayerTree(direct, roundTripped, "root", &mismatches);
  for (const auto& m : mismatches) {
    ADD_FAILURE() << m.where << ": " << m.detail;
  }
}

}  // namespace pagx::pag
