// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 6 PaintBaker — exercises the six ColorSource branches (SolidColor /
// LinearGradient / RadialGradient / ConicGradient / DiamondGradient /
// ImagePattern) plus Fill / Stroke element bakers and the ShapeStyleData
// wire codec. Each "*RoundTrip" test bakes a minimal PAGX doc, encodes to
// bytes via Codec::Encode, decodes, and asserts the relevant field survived.
// This is a tight spiral through Baker + Codec that catches drift in either
// direction.
#include <cstring>
#include <memory>
#include "gtest/gtest.h"
#include "pag/support/CorruptBuilder.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/pag/Baker.h"
#include "pagx/pag/Codec.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/types/Data.h"

namespace pagx::pag {

namespace {

// Bake then encode-decode roundtrip. Returns the decoded doc, or nullptr if
// anything along the Bake/Encode/Decode pipeline went fatal. We deliberately
// couple Baker+Codec here so Phase 6 tests fail loudly on any drift between
// the two sides of the ShapeStyleData wire contract.
struct RoundTripResult {
  std::unique_ptr<PAGDocument> baked;
  std::unique_ptr<PAGDocument> decoded;
  std::vector<Diagnostic> bakeWarnings;
};

template <typename Setup>
RoundTripResult BakeAndRoundTrip(Setup setup) {
  auto builder = pagx::test::PAGXBuilder::Make();
  auto layer = builder.AddLayer().Name("paint_host");
  setup(builder, *layer.layer());
  builder.RawDocument()->applyLayout();
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

// Pulls the ShapeStyleData out of an ElementFillStyleData held by the first
// VectorElement on the host layer. Returns nullptr when the structure is not
// the expected shape (test callers assert instead of crashing).
const ShapeStyleData* FirstFillStyle(const PAGDocument& doc) {
  if (doc.compositions.empty() || doc.compositions[0]->layers.empty()) {
    return nullptr;
  }
  const auto& layer = *doc.compositions[0]->layers[0];
  if (layer.vector == nullptr || layer.vector->contents.empty()) {
    return nullptr;
  }
  const auto& el = *layer.vector->contents[0];
  if (el.type != VectorElementType::FillStyle) {
    return nullptr;
  }
  const auto& data = std::get<std::unique_ptr<ElementFillStyleData>>(el.payload);
  return data->style.get();
}

}  // namespace

// -----------------------------------------------------------------------
// SolidColor
// -----------------------------------------------------------------------

TEST(PaintBaker, SolidColorFillRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* solid = b.RawDocument()->makeNode<pagx::SolidColor>();
    solid->color = pagx::Color{0.25f, 0.5f, 0.75f, 1.0f};
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = solid;
    fill->alpha = 0.5f;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::SolidColor);
  EXPECT_FLOAT_EQ(style->alpha.value, 0.5f);
  // Colors go through 8-bit quantisation in WriteColor / ReadColor.
  EXPECT_NEAR(style->color.value.red, 0.25f, 1.0f / 255.0f);
  EXPECT_NEAR(style->color.value.green, 0.5f, 1.0f / 255.0f);
  EXPECT_NEAR(style->color.value.blue, 0.75f, 1.0f / 255.0f);
}

// -----------------------------------------------------------------------
// LinearGradient — stops, start/end points, fitsToGeometry
// -----------------------------------------------------------------------

TEST(PaintBaker, LinearGradientFillRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* s0 = b.RawDocument()->makeNode<pagx::ColorStop>();
    s0->offset = 0.0f;
    s0->color = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f};
    auto* s1 = b.RawDocument()->makeNode<pagx::ColorStop>();
    s1->offset = 1.0f;
    s1->color = pagx::Color{0.0f, 0.0f, 1.0f, 1.0f};
    auto* grad = b.RawDocument()->makeNode<pagx::LinearGradient>();
    grad->colorStops = {s0, s1};
    grad->startPoint = pagx::Point{0.0f, 0.0f};
    grad->endPoint = pagx::Point{50.0f, 100.0f};
    grad->fitsToGeometry = false;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = grad;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::LinearGradient);
  ASSERT_EQ(style->stopColors.value.size(), 2u);
  ASSERT_EQ(style->stopPositions.value.size(), 2u);
  EXPECT_NEAR(style->stopColors.value[0].red, 1.0f, 1.0f / 255.0f);
  EXPECT_NEAR(style->stopColors.value[1].blue, 1.0f, 1.0f / 255.0f);
  EXPECT_FLOAT_EQ(style->stopPositions.value[0], 0.0f);
  EXPECT_FLOAT_EQ(style->stopPositions.value[1], 1.0f);
  EXPECT_FLOAT_EQ(style->endPoint.value.x, 50.0f);
  EXPECT_FLOAT_EQ(style->endPoint.value.y, 100.0f);
  EXPECT_FALSE(style->fitsToGeometry);
}

TEST(PaintBaker, GradientEmptyStopsFallbackInjected) {
  // §F.3: a Gradient with no stops must be padded with [Black@0, White@1] so
  // Inflater always sees a valid stops table.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* grad = b.RawDocument()->makeNode<pagx::LinearGradient>();
    // colorStops deliberately left empty
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = grad;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  ASSERT_EQ(style->stopColors.value.size(), 2u);
  EXPECT_FLOAT_EQ(style->stopPositions.value[0], 0.0f);
  EXPECT_FLOAT_EQ(style->stopPositions.value[1], 1.0f);
  // Black@0, White@1
  EXPECT_NEAR(style->stopColors.value[0].red, 0.0f, 1.0f / 255.0f);
  EXPECT_NEAR(style->stopColors.value[1].red, 1.0f, 1.0f / 255.0f);
}

// -----------------------------------------------------------------------
// RadialGradient — center / radius
// -----------------------------------------------------------------------

TEST(PaintBaker, RadialGradientFillRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* grad = b.RawDocument()->makeNode<pagx::RadialGradient>();
    grad->center = pagx::Point{25.0f, 25.0f};
    grad->radius = 40.0f;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = grad;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::RadialGradient);
  EXPECT_FLOAT_EQ(style->center.value.x, 25.0f);
  EXPECT_FLOAT_EQ(style->radius.value, 40.0f);
}

// -----------------------------------------------------------------------
// ConicGradient — startAngle / endAngle
// -----------------------------------------------------------------------

TEST(PaintBaker, ConicGradientFillRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* grad = b.RawDocument()->makeNode<pagx::ConicGradient>();
    grad->center = pagx::Point{10.0f, 10.0f};
    grad->startAngle = 45.0f;
    grad->endAngle = 270.0f;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = grad;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::ConicGradient);
  EXPECT_FLOAT_EQ(style->startAngle.value, 45.0f);
  EXPECT_FLOAT_EQ(style->endAngle.value, 270.0f);
}

// -----------------------------------------------------------------------
// DiamondGradient
// -----------------------------------------------------------------------

TEST(PaintBaker, DiamondGradientFillRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* grad = b.RawDocument()->makeNode<pagx::DiamondGradient>();
    grad->center = pagx::Point{7.0f, 8.0f};
    grad->radius = 9.0f;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = grad;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::DiamondGradient);
  EXPECT_FLOAT_EQ(style->center.value.x, 7.0f);
  EXPECT_FLOAT_EQ(style->radius.value, 9.0f);
}

// -----------------------------------------------------------------------
// ImagePattern — inline image interns into PAGDocument::images
// -----------------------------------------------------------------------

TEST(PaintBaker, ImagePatternInlineBytesInterned) {
  static constexpr uint8_t kBytes[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* img = b.RawDocument()->makeNode<pagx::Image>();
    img->data = pagx::Data::MakeWithCopy(kBytes, sizeof(kBytes));
    auto* pattern = b.RawDocument()->makeNode<pagx::ImagePattern>();
    pattern->image = img;
    pattern->tileModeX = pagx::TileMode::Repeat;
    pattern->scaleMode = pagx::ScaleMode::Zoom;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = pattern;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::ImagePattern);
  // patternImageIndex should resolve to an interned image (not UINT32_MAX).
  EXPECT_NE(style->patternImageIndex, UINT32_MAX);
  ASSERT_LT(style->patternImageIndex, r.decoded->images.size());
  const auto& img = *r.decoded->images[style->patternImageIndex];
  ASSERT_NE(img.data, nullptr);
  EXPECT_EQ(img.data->size(), sizeof(kBytes));
  EXPECT_EQ(0, std::memcmp(img.data->data(), kBytes, sizeof(kBytes)));
  EXPECT_EQ(style->scaleMode, ScaleMode::Zoom);
}

TEST(PaintBaker, ImagePatternFilePathOnlyWarns) {
  // Phase 6 cannot read files — a file-path-only Image degrades to
  // patternImageIndex=UINT32_MAX + ImageSourceMissing warning.
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* img = b.RawDocument()->makeNode<pagx::Image>();
    img->filePath = "/tmp/not/loaded.png";
    auto* pattern = b.RawDocument()->makeNode<pagx::ImagePattern>();
    pattern->image = img;
    auto* fill = b.RawDocument()->makeNode<pagx::Fill>();
    fill->color = pattern;
    host.contents.push_back(fill);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto* style = FirstFillStyle(*r.decoded);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::ImagePattern);
  EXPECT_EQ(style->patternImageIndex, UINT32_MAX);

  bool sawMissing = false;
  for (const auto& w : r.bakeWarnings) {
    if (w.code == pagx::DiagnosticCode::ImageSourceMissing) {
      sawMissing = true;
      break;
    }
  }
  EXPECT_TRUE(sawMissing);
}

// -----------------------------------------------------------------------
// Stroke element
// -----------------------------------------------------------------------

TEST(PaintBaker, StrokeElementRoundTrip) {
  auto r = BakeAndRoundTrip([](pagx::test::PAGXBuilder& b, pagx::Layer& host) {
    auto* solid = b.RawDocument()->makeNode<pagx::SolidColor>();
    solid->color = pagx::Color{1.0f, 1.0f, 1.0f, 1.0f};
    auto* stroke = b.RawDocument()->makeNode<pagx::Stroke>();
    stroke->color = solid;
    stroke->width = 3.5f;
    stroke->alpha = 0.8f;
    stroke->miterLimit = 2.5f;
    stroke->dashes = {4.0f, 2.0f, 1.0f, 2.0f};
    stroke->dashOffset = 0.25f;
    stroke->dashAdaptive = true;
    host.contents.push_back(stroke);
  });
  ASSERT_NE(r.decoded, nullptr);
  const auto& layer = *r.decoded->compositions[0]->layers[0];
  ASSERT_NE(layer.vector, nullptr);
  ASSERT_EQ(layer.vector->contents.size(), 1u);
  ASSERT_EQ(layer.vector->contents[0]->type, VectorElementType::StrokeStyle);
  const auto& d =
      std::get<std::unique_ptr<ElementStrokeStyleData>>(layer.vector->contents[0]->payload);
  EXPECT_FLOAT_EQ(d->strokeWidth.value, 3.5f);
  EXPECT_FLOAT_EQ(d->miterLimit.value, 2.5f);
  ASSERT_EQ(d->lineDashPattern.value.size(), 4u);
  EXPECT_FLOAT_EQ(d->lineDashPattern.value[0], 4.0f);
  EXPECT_FLOAT_EQ(d->lineDashPhase.value, 0.25f);
  EXPECT_TRUE(d->lineDashAdaptive);
  ASSERT_NE(d->style, nullptr);
  EXPECT_EQ(d->style->sourceType, ColorSourceType::SolidColor);
  EXPECT_NEAR(d->style->alpha.value, 0.8f, 1e-5f);
}

// -----------------------------------------------------------------------
// ShapeStyleData Codec — unknown sourceType degrades to SolidColor
// -----------------------------------------------------------------------

TEST(ShapeStyleCodec, UnknownSourceTypeDegradesWithWarning) {
  // Build a minimal Fill-bearing doc, encode, then corrupt the sourceType
  // byte inside the ShapeStyleData wrapper to 99. Re-decode and confirm:
  //   (a) doc decodes (warning, not fatal)
  //   (b) style collapses to SolidColor default
  //   (c) InvalidEnumValue=407 warning surfaces
  auto builder = pagx::test::PAGXBuilder::Make();
  auto host = builder.AddLayer().Name("paint_host");
  auto* solid = builder.RawDocument()->makeNode<pagx::SolidColor>();
  solid->color = pagx::Color{1.0f, 0.0f, 0.0f, 1.0f};
  auto* fill = builder.RawDocument()->makeNode<pagx::Fill>();
  fill->color = solid;
  host.layer()->contents.push_back(fill);
  builder.RawDocument()->applyLayout();
  auto bake = Bake(*builder.RawDocument());
  ASSERT_NE(bake.doc, nullptr);
  auto enc = Codec::Encode(*bake.doc);
  ASSERT_NE(enc.bytes, nullptr);

  // Scan for the sourceType byte. After the ElementFillStyle TagHeader and
  // the ShapeStyle wrapper's innerLength (2 bytes), sourceType == 0 (SolidColor).
  // We flip the first 0x00 that appears 2 bytes after a 0x8C tag header
  // (ElementFillStyle = 44 → (44<<6)|length; cheapest scan: look for the
  // unique raw 0x00 sitting ahead of the 0x0F (propHeader isDefault=1) for
  // alpha). Instead of byte-level fragility, search for the first byte pattern
  // that matches "00 <alpha propHeader byte that is 0x10 isDefault=1>" inside
  // the inner region — that gives us the sourceType index unambiguously.
  auto bytes =
      std::vector<uint8_t>(static_cast<const uint8_t*>(enc.bytes->data()),
                           static_cast<const uint8_t*>(enc.bytes->data()) + enc.bytes->length());
  size_t idx = static_cast<size_t>(-1);
  for (size_t i = 0; i + 1 < bytes.size(); ++i) {
    // sourceType byte (0x00 = SolidColor) directly followed by alpha
    // propHeader byte 0x10 (isDefault=1, alpha==1.0f). Since our Fill has
    // alpha=1.0 by default and PaintBaker emits MakeProp(1.0f), that is the
    // matching pattern.
    if (bytes[i] == 0x00 && bytes[i + 1] == 0x10) {
      idx = i;
      break;
    }
  }
  ASSERT_NE(idx, static_cast<size_t>(-1)) << "couldn't locate sourceType byte";

  auto corrupted = pagx::test::CorruptBuilder::FromValid(std::move(bytes))
                       .ReplaceBytes(idx, {0x63})  // 99 = unknown sourceType
                       .Build();
  auto dec = Codec::Decode(corrupted.data(), corrupted.size());
  ASSERT_NE(dec.doc, nullptr) << "unknown sourceType should warn, not fatal";
  const auto* style = FirstFillStyle(*dec.doc);
  ASSERT_NE(style, nullptr);
  EXPECT_EQ(style->sourceType, ColorSourceType::SolidColor);

  bool sawWarn = false;
  for (const auto& w : dec.warnings) {
    if (w.code == pagx::DiagnosticCode::InvalidEnumValue) {
      sawWarn = true;
      break;
    }
  }
  EXPECT_TRUE(sawWarn);
}

}  // namespace pagx::pag
