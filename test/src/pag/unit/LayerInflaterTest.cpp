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
// 601 InflateFontCreateFailed — case B ElementText carries a shapedGlyphs
// run whose typeface MakeDefaultFontProvider cannot resolve on macOS
// (SystemFont::MakeFromName is a no-op there — see feedback_build_quirks.md
// entry #49). Inflater emits 601 and drops the text element. Phase 17/18
// kept this warn route intact.
// ---------------------------------------------------------------------------

TEST(LayerInflater, CaseBTextUnresolvableTypefaceWarns601) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);

  auto vec = std::make_unique<VectorPayload>();
  auto textEl = std::make_unique<VectorElement>();
  textEl->type = VectorElementType::Text;
  auto textData = std::make_unique<ElementTextData>();
  textData->fontFamily = "Arial";
  textData->fontStyle = "Regular";
  textData->fontSize = 24.0f;
  // Case B run — Inflater must ask FontProvider for a typeface.
  ElementTextData::ShapedGlyphRun run;
  run.typefaceFamily = "Arial";
  run.typefaceStyle = "Regular";
  run.typefaceKey = "Arial|Regular|2048|3000";
  run.fontSize = 24.0f;
  run.glyphs = {72, 105};
  run.positions = {{0, 0}, {12, 0}};
  textData->shapedGlyphs.push_back(std::move(run));
  textEl->payload = std::move(textData);
  vec->contents.push_back(std::move(textEl));
  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  // Default Options → Inflate substitutes MakeDefaultFontProvider(), which
  // walks pag::FontManager with no registered fonts. getTypeface() returns
  // null for "Arial/Regular" and §508-512 warns + drops the text.
  auto r = LayerInflater::Inflate(std::move(doc));
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateFontCreateFailed));
}

// ---------------------------------------------------------------------------
// 602 InflateGlyphRunBuildFailed — case A ElementText whose glyphRuns point
// at an embeddedFontIndex out of range (or at a malformed EmbeddedFont with
// unitsPerEm==0). Inflater skips the affected run and warns; the layer
// itself stays non-null so the surrounding tree is intact.
// ---------------------------------------------------------------------------

TEST(LayerInflater, CaseATextMissingEmbeddedFontWarns602) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);

  auto vec = std::make_unique<VectorPayload>();
  auto textEl = std::make_unique<VectorElement>();
  textEl->type = VectorElementType::Text;
  auto textData = std::make_unique<ElementTextData>();
  ElementTextData::GlyphRunData run;
  run.embeddedFontIndex = 999;  // no such EmbeddedFont — will skip + warn.
  run.fontSize = 24.0f;
  run.glyphs = {1, 2};
  run.positions = {{0, 0}, {12, 0}};
  textData->glyphRuns.push_back(std::move(run));
  textEl->payload = std::move(textData);
  vec->contents.push_back(std::move(textEl));
  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateGlyphRunBuildFailed));
}

TEST(LayerInflater, CaseATextZeroUnitsPerEmWarns602) {
  auto doc = MakeDoc();
  // One EmbeddedFont with unitsPerEm=0 — Inflater treats this as malformed
  // and warns (§625-627).
  auto embedded = std::make_unique<EmbeddedFont>();
  embedded->unitsPerEm = 0;
  EmbeddedGlyph g;
  g.advance = 500.0f;
  embedded->glyphs.push_back(std::move(g));
  doc->embeddedFonts.push_back(std::move(embedded));

  auto comp = MakeComp();
  auto layer = MakeLayer(LayerType::Vector);
  auto vec = std::make_unique<VectorPayload>();
  auto textEl = std::make_unique<VectorElement>();
  textEl->type = VectorElementType::Text;
  auto textData = std::make_unique<ElementTextData>();
  ElementTextData::GlyphRunData run;
  run.embeddedFontIndex = 0;
  run.fontSize = 24.0f;
  run.glyphs = {0};
  run.positions = {{0, 0}};
  textData->glyphRuns.push_back(std::move(run));
  textEl->payload = std::move(textData);
  vec->contents.push_back(std::move(textEl));
  layer->vector = std::move(vec);
  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  EXPECT_TRUE(HasWarningCode(r.warnings, ErrorCode::InflateGlyphRunBuildFailed));
}

// ---------------------------------------------------------------------------
// ShapePayload + gradient fills — PAGX itself routes shapes through
// VectorLayer (ShapePayload stays empty in Baker output), so these
// codepaths are only reachable when a payload lands via a future producer
// or a hand-built document. Covers all four Shader gradient flavors plus
// ImagePattern-fallback (missing image index → nullptr stroke/fill is
// dropped, ShapeLayer still instantiated).
// ---------------------------------------------------------------------------

TEST(LayerInflater, ShapePayloadGradientsInflate) {
  auto doc = MakeDoc();
  auto comp = MakeComp();

  struct GradientSample {
    ColorSourceType type;
  };
  const GradientSample samples[] = {
      {ColorSourceType::LinearGradient},
      {ColorSourceType::RadialGradient},
      {ColorSourceType::ConicGradient},
      {ColorSourceType::DiamondGradient},
  };

  for (const auto& sample : samples) {
    auto layer = MakeLayer(LayerType::Shape);
    auto payload = std::make_unique<ShapePayload>();

    auto style = std::make_unique<ShapeStyleData>();
    style->sourceType = sample.type;
    // Two valid color stops so the Shader factory succeeds.
    style->stopColors = MakeProp<std::vector<tgfx::Color>>(
        {tgfx::Color{1.0f, 0.0f, 0.0f, 1.0f}, tgfx::Color{0.0f, 0.0f, 1.0f, 1.0f}});
    style->stopPositions = MakeProp<std::vector<float>>({0.0f, 1.0f});
    // Non-identity matrix → makeWithMatrix branch.
    style->gradientMatrix = MakeProp(tgfx::Matrix::MakeTrans(5.0f, 5.0f));
    // Linear endpoints / Radial-Conic-Diamond center-radius all need valid
    // values to avoid the shader returning null.
    style->startPoint = MakeProp(tgfx::Point{0.0f, 0.0f});
    style->endPoint = MakeProp(tgfx::Point{100.0f, 0.0f});
    style->center = MakeProp(tgfx::Point{50.0f, 50.0f});
    style->radius = MakeProp(50.0f);
    payload->fillStyles.push_back(std::move(style));

    layer->shape = std::move(payload);
    comp->layers.push_back(std::move(layer));
  }

  // Stroke path with ImagePattern pointing at a missing image (returns
  // nullptr early — exercises §1053-1054 guard). Layer still instantiates.
  {
    auto layer = MakeLayer(LayerType::Shape);
    auto payload = std::make_unique<ShapePayload>();
    auto stroke = std::make_unique<ShapeStyleData>();
    stroke->sourceType = ColorSourceType::ImagePattern;
    stroke->patternImageIndex = 999;  // out of range.
    payload->strokeStyles.push_back(std::move(stroke));
    payload->strokeWidth = MakeProp(2.0f);
    // Non-default line dash + strokeOnTop to walk §1130-1140.
    payload->lineDashPattern = MakeProp<std::vector<float>>({4.0f, 2.0f});
    payload->lineDashPhase = MakeProp(1.0f);
    payload->lineDashAdaptive = true;
    payload->strokeAlign = tgfx::StrokeAlign::Outside;
    payload->strokeOnTop = true;
    layer->shape = std::move(payload);
    comp->layers.push_back(std::move(layer));
  }

  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  // 4 gradient shape layers + 1 image-stroke shape layer = 5 children.
  EXPECT_EQ(r.layer->children().size(), 5u);
  for (const auto& child : r.layer->children()) {
    auto sl = std::dynamic_pointer_cast<tgfx::ShapeLayer>(child);
    EXPECT_NE(sl, nullptr);
  }
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

// ---------------------------------------------------------------------------
// makeLayerByType fallback — the inflater only calls it when a typed
// Layer (Shape / Image / Solid / Vector) arrives with its payload pointer
// null. Normal Baker output always fills the payload, so reach the
// fallback by hand-building Layers with type != Layer but payload left
// empty. Drives the §100-108 switch.
// ---------------------------------------------------------------------------

TEST(LayerInflater, NullPayloadTypedLayersFallbackToMakeLayerByType) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  comp->layers.push_back(MakeLayer(LayerType::Shape));
  comp->layers.push_back(MakeLayer(LayerType::Image));
  comp->layers.push_back(MakeLayer(LayerType::Solid));
  comp->layers.push_back(MakeLayer(LayerType::Vector));
  comp->layers.push_back(MakeLayer(LayerType::Mesh));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 5u);
  EXPECT_NE(std::dynamic_pointer_cast<tgfx::ShapeLayer>(r.layer->children()[0]), nullptr);
  EXPECT_NE(std::dynamic_pointer_cast<tgfx::Layer>(r.layer->children()[1]), nullptr);
  EXPECT_NE(std::dynamic_pointer_cast<tgfx::SolidLayer>(r.layer->children()[2]), nullptr);
  EXPECT_NE(std::dynamic_pointer_cast<tgfx::VectorLayer>(r.layer->children()[3]), nullptr);
  // Mesh falls back to bare tgfx::Layer (MeshPayload empty this cycle).
  EXPECT_NE(r.layer->children()[4], nullptr);
}

// ---------------------------------------------------------------------------
// applyCommon — optional fields wired through conditional branches. One
// Layer carrying non-default matrix3D / preserve3D / !allowsEdgeAntialiasing
// / !passThroughBackground / InnerShadow filter walks §264 / §267 / §273 /
// §277 / §334-336 in a single pass.
// ---------------------------------------------------------------------------

TEST(LayerInflater, LayerOptionalAttributesAndInnerShadowFilter) {
  auto doc = MakeDoc();
  auto comp = MakeComp();
  auto layer = MakeLayer();

  // Matrix3D non-identity → §263-264. Start from identity then bump the
  // translate-X slot (column 3, row 0). Actual value doesn't matter — only
  // isIdentity() gating does.
  auto m3d = Matrix3D::I();
  m3d.values[12] = 10.0f;
  layer->matrix3D = MakeProp(m3d);
  // preserve3D / !passThroughBackground / !allowsEdgeAntialiasing.
  layer->preserve3D = true;
  layer->passThroughBackground = false;
  layer->allowsEdgeAntialiasing = false;

  // InnerShadow filter — §334-336.
  auto inner = std::make_unique<LayerFilter>();
  inner->type = LayerFilterType::InnerShadow;
  inner->offsetX = MakeProp(3.0f);
  inner->offsetY = MakeProp(4.0f);
  inner->blurX = MakeProp(2.0f);
  inner->blurY = MakeProp(2.0f);
  inner->color = MakeProp(tgfx::Color{0.0f, 0.0f, 0.0f, 1.0f});
  inner->shadowOnly = false;
  layer->filters.push_back(std::move(inner));

  comp->layers.push_back(std::move(layer));
  doc->compositions.push_back(std::move(comp));

  auto r = LayerInflater::Inflate(std::move(doc));
  ASSERT_NE(r.layer, nullptr);
  ASSERT_EQ(r.layer->children().size(), 1u);
  auto child = r.layer->children()[0];
  EXPECT_TRUE(child->preserve3D());
  EXPECT_FALSE(child->allowsEdgeAntialiasing());
  EXPECT_FALSE(child->passThroughBackground());
  EXPECT_EQ(child->filters().size(), 1u);
}

}  // namespace pagx::pag
