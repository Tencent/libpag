// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 9 unit tests for LayerInflater — PAGDocument → tgfx::Layer tree.
// Tests drive the Inflater directly with hand-built PAGDocuments so every
// degraded path (600 InflateImageDecodeFailed, 601 InflateFontCreateFailed,
// 602 InflateGlyphRunBuildFailed, 603 InflateMaskResolveFailed, 604
// InflaterEmptyDocument, 605 InflateCompositionCycle, 606
// InflaterLayerBudgetExceeded) is reachable without going through the
// Baker/Codec round-trip.
#include <memory>
#include <utility>
#include <vector>
#include "gtest/gtest.h"
#include "pagx/pag/LayerInflater.h"
#include "pagx/pag/PAGDocument.h"
#include "tgfx/core/Data.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/SolidLayer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/vectors/VectorGroup.h"

namespace pagx::pag {

namespace {

std::unique_ptr<PAGDocument> MakeDoc() {
  auto doc = std::make_unique<PAGDocument>();
  doc->header.width = 640.0f;
  doc->header.height = 480.0f;
  return doc;
}

std::unique_ptr<Composition> MakeComp(uint32_t w = 640, uint32_t h = 480) {
  auto c = std::make_unique<Composition>();
  c->width = w;
  c->height = h;
  return c;
}

std::unique_ptr<Layer> MakeLayer(LayerType type = LayerType::Layer) {
  auto l = std::make_unique<Layer>();
  l->type = type;
  return l;
}

bool HasWarningCode(const std::vector<Diagnostic>& warnings, ErrorCode code) {
  for (const auto& w : warnings) {
    if (w.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// 604 InflaterEmptyDocument — three null-input shapes all collapse to the
// same warn path, layer remains nullptr.
// ---------------------------------------------------------------------------

TEST(LayerInflater, NullDocumentWarnsEmpty) {
  auto r = LayerInflater::Inflate(nullptr);
  EXPECT_EQ(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflaterEmptyDocument));
}

TEST(LayerInflater, EmptyCompositionsWarnsEmpty) {
  auto doc = MakeDoc();
  auto r = LayerInflater::Inflate(std::move(doc));
  EXPECT_EQ(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflaterEmptyDocument));
}

TEST(LayerInflater, FirstCompositionEmptyLayersWarnsEmpty) {
  auto doc = MakeDoc();
  doc->compositions.push_back(MakeComp());
  auto r = LayerInflater::Inflate(std::move(doc));
  EXPECT_EQ(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflaterEmptyDocument));
}

// ---------------------------------------------------------------------------
// Basic happy path: single plain Layer → tgfx::Layer tree with 1 child.
// ---------------------------------------------------------------------------

TEST(LayerInflater, SingleLayerProducesOneChild) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  comp->layers.push_back(MakeLayer());
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  EXPECT_TRUE(r.warnings.empty());
  EXPECT_EQ(r.layer->children().size(), 1u);
}

TEST(LayerInflater, LayerAttributesApply) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer();
  layer->visible = MakeProp(true);
  layer->alpha = MakeProp(0.5f);
  layer->matrix = MakeProp(tgfx::Matrix::MakeTrans(10.0f, 20.0f));
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto child = r.layer->children()[0];
  EXPECT_FLOAT_EQ(child->alpha(), 0.5f);
  EXPECT_FLOAT_EQ(child->matrix().getTranslateX(), 10.0f);
  EXPECT_FLOAT_EQ(child->matrix().getTranslateY(), 20.0f);
}

// ---------------------------------------------------------------------------
// Vector payload — 3 VectorElements feed through, wrap inside a VectorLayer.
// ---------------------------------------------------------------------------

TEST(LayerInflater, VectorPayloadInflates) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);

  auto vec = std::make_unique<VectorPayload>();
  // Rectangle
  auto rect = std::make_unique<VectorElement>();
  rect->type = VectorElementType::Rectangle;
  auto rectData = std::make_unique<ElementRectangleData>();
  rectData->size = MakeProp(tgfx::Point{100.0f, 50.0f});
  rect->payload = std::move(rectData);
  vec->contents.push_back(std::move(rect));
  // Ellipse
  auto ellipse = std::make_unique<VectorElement>();
  ellipse->type = VectorElementType::Ellipse;
  auto ellipseData = std::make_unique<ElementEllipseData>();
  ellipseData->size = MakeProp(tgfx::Point{30.0f, 30.0f});
  ellipse->payload = std::move(ellipseData);
  vec->contents.push_back(std::move(ellipse));
  // Fill (SolidColor)
  auto fill = std::make_unique<VectorElement>();
  fill->type = VectorElementType::FillStyle;
  auto fillData = std::make_unique<ElementFillStyleData>();
  fillData->style = std::make_unique<ShapeStyleData>();
  fillData->style->sourceType = ColorSourceType::SolidColor;
  fillData->style->color = MakeProp(tgfx::Color{1.0f, 0.0f, 0.0f, 1.0f});
  fill->payload = std::move(fillData);
  vec->contents.push_back(std::move(fill));

  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  // The child should be a VectorLayer with 3 contents.
  auto vectorLayer = std::dynamic_pointer_cast<tgfx::VectorLayer>(r.layer->children()[0]);
  ASSERT_NE(vectorLayer, nullptr);
  EXPECT_EQ(vectorLayer->contents().size(), 3u);
}

// ---------------------------------------------------------------------------
// ShapeStyleData → ColorSource: 5 gradient + 1 SolidColor → all produce a
// non-null ColorSource that flows into FillStyle::Make.
// ---------------------------------------------------------------------------

namespace {

std::unique_ptr<VectorElement> MakeFillWithSourceType(ColorSourceType type) {
  auto fill = std::make_unique<VectorElement>();
  fill->type = VectorElementType::FillStyle;
  auto fillData = std::make_unique<ElementFillStyleData>();
  fillData->style = std::make_unique<ShapeStyleData>();
  fillData->style->sourceType = type;
  fillData->style->stopColors =
      MakeProp<std::vector<tgfx::Color>>({tgfx::Color::Black(), tgfx::Color::White()});
  fillData->style->stopPositions = MakeProp<std::vector<float>>({0.0f, 1.0f});
  fill->payload = std::move(fillData);
  return fill;
}

}  // namespace

TEST(LayerInflater, ColorSourceAllBranchesInflate) {
  for (auto type : {ColorSourceType::SolidColor, ColorSourceType::LinearGradient,
                    ColorSourceType::RadialGradient, ColorSourceType::ConicGradient,
                    ColorSourceType::DiamondGradient}) {
    auto doc = MakeDoc();
    auto comp = MakeComp();
    auto layer = MakeLayer(LayerType::Vector);
    auto vec = std::make_unique<VectorPayload>();
    vec->contents.push_back(MakeFillWithSourceType(type));
    layer->vector = std::move(vec);
    comp->layers.push_back(std::move(layer));
    doc->compositions.push_back(std::move(comp));

    auto r = LayerInflater::Inflate(std::move(doc));
    ASSERT_NE(r.layer, nullptr) << "type=" << static_cast<int>(type);
    EXPECT_TRUE(r.warnings.empty()) << "type=" << static_cast<int>(type);
  }
}

// ---------------------------------------------------------------------------
// 600 InflateImageDecodeFailed — ImagePattern with garbage bytes fails to
// decode; Fill falls back to SolidColor so the VectorLayer still holds 1
// element and the tree stays well-formed.
// ---------------------------------------------------------------------------

TEST(LayerInflater, ImagePatternDecodeFailureWarns600) {
  auto doc = MakeDoc();
  auto img = std::make_unique<ImageAsset>();
  const uint8_t garbage[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  img->data = tgfx::Data::MakeWithCopy(garbage, sizeof(garbage));
  doc->images.push_back(std::move(img));

  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);
  auto vec = std::make_unique<VectorPayload>();
  auto fill = std::make_unique<VectorElement>();
  fill->type = VectorElementType::FillStyle;
  auto fillData = std::make_unique<ElementFillStyleData>();
  fillData->style = std::make_unique<ShapeStyleData>();
  fillData->style->sourceType = ColorSourceType::ImagePattern;
  fillData->style->patternImageIndex = 0;
  fill->payload = std::move(fillData);
  vec->contents.push_back(std::move(fill));
  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateImageDecodeFailed));
}

// ---------------------------------------------------------------------------
// 601 InflateFontCreateFailed — ElementText with out-of-range fontIndex.
// Empty tgfx::Font propagates through BuildTextBlob, which returns null →
// 602 also fires.
// ---------------------------------------------------------------------------

TEST(LayerInflater, OutOfRangeFontIndexWarns601) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);
  auto vec = std::make_unique<VectorPayload>();

  auto textEl = std::make_unique<VectorElement>();
  textEl->type = VectorElementType::Text;
  auto textData = std::make_unique<ElementTextData>();
  GlyphRunBlob blob;
  blob.kind = GlyphRunKind::LayoutRun;
  blob.fontIndex = 999;  // Out of range — doc->fonts is empty.
  blob.fontSize = 16.0f;
  GlyphRunBlob::LayoutGlyph g;
  g.glyphId = 1;
  g.position = {0.0f, 0.0f};
  blob.layoutGlyphs.push_back(g);
  textData->glyphRuns.push_back(std::move(blob));
  textEl->payload = std::move(textData);
  vec->contents.push_back(std::move(textEl));

  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateFontCreateFailed));
  // Builder returns a null TextBlob when every run has no typeface → 602.
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateGlyphRunBuildFailed));
}

// ---------------------------------------------------------------------------
// 603 InflateMaskResolveFailed — mask points at a path that is never
// populated by Pass 1.
// ---------------------------------------------------------------------------

TEST(LayerInflater, MaskTargetNotFoundWarns603) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer();
  // Mask points to child index 42 inside us — no such child exists.
  layer->maskLayerPath = {0, 42};
  layer->maskType = tgfx::LayerMaskType::Alpha;
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateMaskResolveFailed));
}

// ---------------------------------------------------------------------------
// 605 InflateCompositionCycle — compositions[0].layers[0] = CompositionRef(0).
// ---------------------------------------------------------------------------

TEST(LayerInflater, CompositionSelfReferenceWarns605) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::CompositionRef);
  layer->compositionIndex = 0;  // Self-reference.
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  // Inflater survives the cycle (doesn't crash) and pushes 605.
  ASSERT_NE(r.layer, nullptr);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateCompositionCycle));
}

// ---------------------------------------------------------------------------
// ImagePayload end-to-end: Layer with LayerType::Image and a valid
// ImageAsset (decode will still fail for garbage bytes, but the ImageLayer
// is instantiated regardless so 600 coexists with a non-null tree).
// ---------------------------------------------------------------------------

TEST(LayerInflater, ImagePayloadInstantiatesLayer) {
  auto doc = MakeDoc();
  auto img = std::make_unique<ImageAsset>();
  const uint8_t bogus[4] = {0, 0, 0, 0};
  img->data = tgfx::Data::MakeWithCopy(bogus, sizeof(bogus));
  doc->images.push_back(std::move(img));

  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Image);
  layer->image = std::make_unique<ImagePayload>();
  layer->image->imageIndex = 0;
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateImageDecodeFailed));
}

// ---------------------------------------------------------------------------
// SolidPayload end-to-end: straight pass-through of 5 setters.
// ---------------------------------------------------------------------------

TEST(LayerInflater, SolidPayloadProducesSolidLayer) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Solid);
  layer->solid = std::make_unique<SolidPayload>();
  layer->solid->width = MakeProp(100.0f);
  layer->solid->height = MakeProp(50.0f);
  layer->solid->color = MakeProp(tgfx::Color{0.0f, 1.0f, 0.0f, 1.0f});
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto solid = std::dynamic_pointer_cast<tgfx::SolidLayer>(r.layer->children()[0]);
  ASSERT_NE(solid, nullptr);
}

// ---------------------------------------------------------------------------
// Filter + Style: Blur filter + DropShadow style both plumbed onto the
// produced tgfx::Layer.
// ---------------------------------------------------------------------------

TEST(LayerInflater, LayerFilterAndStyleApplied) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer();

  auto filter = std::make_unique<LayerFilter>();
  filter->type = LayerFilterType::Blur;
  filter->blurX = MakeProp(4.0f);
  filter->blurY = MakeProp(4.0f);
  layer->filters.push_back(std::move(filter));

  auto style = std::make_unique<LayerStyle>();
  style->type = LayerStyleType::DropShadow;
  style->offsetX = MakeProp(2.0f);
  style->offsetY = MakeProp(2.0f);
  style->blurX = MakeProp(3.0f);
  style->blurY = MakeProp(3.0f);
  style->color = MakeProp(tgfx::Color::Black());
  layer->styles.push_back(std::move(style));

  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto child = r.layer->children()[0];
  EXPECT_EQ(child->filters().size(), 1u);
  EXPECT_EQ(child->layerStyles().size(), 1u);
}

// ---------------------------------------------------------------------------
// Mask resolution success: layer[0] binds mask to layer[1]; Pass 2 looks
// layer[1] up and calls setMask.
// ---------------------------------------------------------------------------

TEST(LayerInflater, MaskResolutionSucceeds) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto host = MakeLayer();
  host->maskLayerPath = {1};  // sibling at composition-root index 1.
  auto target = MakeLayer();
  comp->layers.push_back(std::move(host));
  comp->layers.push_back(std::move(target));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  EXPECT_FALSE(HasWarningCode(r.warnings, ErrorCode::InflateMaskResolveFailed));
  ASSERT_EQ(r.layer->children().size(), 2u);
  EXPECT_NE(r.layer->children()[0]->mask(), nullptr);
}

// ---------------------------------------------------------------------------
// ShapePayload with SolidColor fill — ShapeLayer emerges and carries 1 fill.
// ---------------------------------------------------------------------------

TEST(LayerInflater, ShapePayloadSolidFillInflates) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Shape);
  layer->shape = std::make_unique<ShapePayload>();

  auto style = std::make_unique<ShapeStyleData>();
  style->sourceType = ColorSourceType::SolidColor;
  style->color = MakeProp(tgfx::Color{0.5f, 0.5f, 0.5f, 1.0f});
  layer->shape->fillStyles.push_back(std::move(style));

  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto shapeLayer = std::dynamic_pointer_cast<tgfx::ShapeLayer>(r.layer->children()[0]);
  ASSERT_NE(shapeLayer, nullptr);
  EXPECT_EQ(shapeLayer->fillStyles().size(), 1u);
}

// ---------------------------------------------------------------------------
// Nested VectorGroup — Group inside Group; dispatch recurses.
// ---------------------------------------------------------------------------

TEST(LayerInflater, NestedVectorGroupInflates) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);
  auto vec = std::make_unique<VectorPayload>();

  auto outer = std::make_unique<VectorElement>();
  outer->type = VectorElementType::VectorGroup;
  auto outerData = std::make_unique<ElementVectorGroupData>();

  auto inner = std::make_unique<VectorElement>();
  inner->type = VectorElementType::VectorGroup;
  auto innerData = std::make_unique<ElementVectorGroupData>();

  auto rect = std::make_unique<VectorElement>();
  rect->type = VectorElementType::Rectangle;
  auto rectData = std::make_unique<ElementRectangleData>();
  rectData->size = MakeProp(tgfx::Point{10.0f, 10.0f});
  rect->payload = std::move(rectData);
  innerData->elements.push_back(std::move(rect));
  inner->payload = std::move(innerData);
  outerData->elements.push_back(std::move(inner));
  outer->payload = std::move(outerData);
  vec->contents.push_back(std::move(outer));

  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto vectorLayer = std::dynamic_pointer_cast<tgfx::VectorLayer>(r.layer->children()[0]);
  ASSERT_NE(vectorLayer, nullptr);
  EXPECT_EQ(vectorLayer->contents().size(), 1u);
  auto outerGroup = std::dynamic_pointer_cast<tgfx::VectorGroup>(vectorLayer->contents()[0]);
  ASSERT_NE(outerGroup, nullptr);
  EXPECT_EQ(outerGroup->elements().size(), 1u);
}

}  // namespace pagx::pag
