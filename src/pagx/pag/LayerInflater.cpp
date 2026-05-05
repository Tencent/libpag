// Copyright (C) 2026 Tencent. All rights reserved.

#include "pagx/pag/LayerInflater.h"
#include <utility>
#include <vector>
#include "pagx/TextLayout.h"
#include "pagx/pag/FontProvider.h"
#include "pagx/pag/InflaterContext.h"
#include "pagx/pag/TypefaceKey.h"
#include "renderer/GlyphRunRenderer.h"
#ifdef PAG_USE_HARFBUZZ
#include "pagx/FontConfig.h"
#include "pagx/LayoutContext.h"
#include "renderer/TextShaper.h"
#endif
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Shader.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/layers/ImageLayer.h"
#include "tgfx/layers/ShapeLayer.h"
#include "tgfx/layers/ShapeStyle.h"
#include "tgfx/layers/SolidLayer.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/vectors/Ellipse.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Gradient.h"
#include "tgfx/layers/vectors/ImagePattern.h"
#include "tgfx/layers/vectors/MergePath.h"
#include "tgfx/layers/vectors/Polystar.h"
#include "tgfx/layers/vectors/Rectangle.h"
#include "tgfx/layers/vectors/Repeater.h"
#include "tgfx/layers/vectors/RoundCorner.h"
#include "tgfx/layers/vectors/ShapePath.h"
#include "tgfx/layers/vectors/SolidColor.h"
#include "tgfx/layers/vectors/StrokeStyle.h"
#include "tgfx/layers/vectors/Text.h"
#include "tgfx/layers/vectors/TextModifier.h"
#include "tgfx/layers/vectors/TextPath.h"
#include "tgfx/layers/vectors/TextSelector.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorGroup.h"
// Filter / style headers
#include "tgfx/layers/filters/BlendFilter.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/ColorMatrixFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/InnerShadowFilter.h"
#include "tgfx/layers/layerstyles/BackgroundBlurStyle.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"

namespace pagx::pag {

namespace {

// ---------------------------------------------------------------------------
// Forward declarations for intra-file recursion. The dispatcher pair
// (inflateLayer / inflateComposition) calls each other through a
// CompositionRef; the subtype builders (applyPayload / inflateVectorElement)
// call back into inflateLayer for VectorGroup children.
// ---------------------------------------------------------------------------
std::shared_ptr<tgfx::Layer> inflateLayer(PAGDocument& doc, const Layer& src,
                                          std::vector<uint32_t>* pathStack, InflaterContext* ctx);

std::shared_ptr<tgfx::Layer> inflateComposition(PAGDocument& doc, uint32_t compositionIndex,
                                                std::vector<uint32_t>* pathStack,
                                                InflaterContext* ctx);

void applyCommon(const Layer& src, tgfx::Layer* layer, InflaterContext* ctx);

std::shared_ptr<tgfx::VectorElement> inflateVectorElement(PAGDocument& doc,
                                                          const VectorElement& src,
                                                          InflaterContext* ctx);

// ShapeStyleData → tgfx::ColorSource dispatcher (mirror of
// LayerBuilder::convertColorSource). Image decode failures bubble up as
// nullptr + warn 600; caller substitutes SolidColor::Make() so Fill/Stroke
// still gets a valid, non-null colorSource.
std::shared_ptr<tgfx::ColorSource> inflateColorSource(PAGDocument& doc, const ShapeStyleData& style,
                                                      InflaterContext* ctx);

// Filter / style shims — implemented in §9.3 (Task 3).
std::shared_ptr<tgfx::LayerFilter> inflateLayerFilter(const LayerFilter& src);
std::shared_ptr<tgfx::LayerStyle> inflateLayerStyle(const LayerStyle& src);

// Per-payload sub-inflaters — Task 4-6 flesh these out.
std::shared_ptr<tgfx::Layer> inflateVectorPayload(PAGDocument& doc, const VectorPayload& pay,
                                                  InflaterContext* ctx);
std::shared_ptr<tgfx::Layer> inflateShapePayload(PAGDocument& doc, const ShapePayload& pay,
                                                 InflaterContext* ctx);
std::shared_ptr<tgfx::Layer> inflateImagePayload(PAGDocument& doc, const ImagePayload& pay,
                                                 InflaterContext* ctx);
std::shared_ptr<tgfx::Layer> inflateSolidPayload(const SolidPayload& pay);
std::shared_ptr<tgfx::Layer> inflateMeshPayload();

// ---------------------------------------------------------------------------
// Inflate entry dispatcher
// ---------------------------------------------------------------------------

std::shared_ptr<tgfx::Layer> makeLayerByType(LayerType type) {
  switch (type) {
    case LayerType::Shape:
      return tgfx::ShapeLayer::Make();
    case LayerType::Image:
      return tgfx::ImageLayer::Make();
    case LayerType::Solid:
      return tgfx::SolidLayer::Make();
    case LayerType::Vector:
      return tgfx::VectorLayer::Make();
    case LayerType::Mesh:
      // MeshPayload is empty this cycle — a bare Layer stand-in keeps the
      // tree well-formed for mask / filter wiring, and future MeshLayer
      // integration can upgrade the call site with zero surrounding changes.
      return tgfx::Layer::Make();
    case LayerType::Layer:
    case LayerType::Text:
    case LayerType::CompositionRef:
      // Layer/Text/CompositionRef are handled by inflateLayer's explicit
      // branches before the type-dispatched payload path; returning a bare
      // Layer here keeps makeLayerByType total without inventing payload
      // semantics (Layer carries only children, CompositionRef is routed
      // via inflateComposition, Text is deferred to Phase 10).
      return tgfx::Layer::Make();
  }
  return tgfx::Layer::Make();
}

std::shared_ptr<tgfx::Layer> inflateLayer(PAGDocument& doc, const Layer& src,
                                          std::vector<uint32_t>* pathStack, InflaterContext* ctx) {
  // Budget first: if we're at the ceiling, the entire subtree is elided —
  // caller sees nullptr and writes a nullptr into layerByPath.
  const uint32_t myIndexForBudget = pathStack->empty() ? 0 : pathStack->back();
  if (!ctx->reserveLayerBudget(myIndexForBudget)) {
    ctx->layerByPath[PackLayerPath(*pathStack)] = nullptr;
    return nullptr;
  }

  // Route CompositionRef / payload types.
  std::shared_ptr<tgfx::Layer> layer = nullptr;
  if (src.type == LayerType::CompositionRef) {
    layer = inflateComposition(doc, src.compositionIndex, pathStack, ctx);
    // inflateComposition may have returned nullptr from cycle / over-depth;
    // caller path will substitute a placeholder so layerPath indices stay
    // connected.
  } else if (src.type == LayerType::Vector && src.vector != nullptr) {
    layer = inflateVectorPayload(doc, *src.vector, ctx);
  } else if (src.type == LayerType::Shape && src.shape != nullptr) {
    layer = inflateShapePayload(doc, *src.shape, ctx);
  } else if (src.type == LayerType::Image && src.image != nullptr) {
    layer = inflateImagePayload(doc, *src.image, ctx);
  } else if (src.type == LayerType::Solid && src.solid != nullptr) {
    layer = inflateSolidPayload(*src.solid);
  } else if (src.type == LayerType::Mesh) {
    layer = inflateMeshPayload();
  } else {
    // Plain Layer (children-only container) / Text payload not built this
    // cycle / payload pointer null (Baker elided it).
    layer = makeLayerByType(src.type);
  }

  if (layer == nullptr) {
    // Subtype inflater gave up (e.g. Vector / Shape with all elements
    // dropped). Fall back to a plain Layer so mask/filter wiring still has
    // a host — the degraded visuals are an accepted trade-off (no fatals in
    // Inflater).
    layer = tgfx::Layer::Make();
  }

  applyCommon(src, layer.get(), ctx);

  // Queue mask lookup for Pass 2. PendingMask cap guards the vector.
  if (!src.maskLayerPath.empty() && ctx->reservePendingMaskSlot()) {
    ctx->pendingMasks.push_back(
        InflaterContext::PendingMask{layer, PackLayerPath(src.maskLayerPath), src.maskType});
  }

  // Record this layer's path BEFORE recursing into children — mask targets
  // may resolve to us.
  ctx->layerByPath[PackLayerPath(*pathStack)] = layer;

  // Recurse children with pathStack pushing the child index.
  for (uint32_t i = 0; i < src.children.size(); ++i) {
    pathStack->push_back(i);
    auto childLayer = inflateLayer(doc, *src.children[i], pathStack, ctx);
    pathStack->pop_back();

    if (childLayer == nullptr) {
      // Degraded path: try to instantiate an empty placeholder so sibling
      // indices stay aligned with the Baker-side structural coordinates.
      // reserveLayerBudget is re-checked — if the budget is genuinely out,
      // the slot simply stays nullptr and addChild is skipped.
      if (ctx->reserveLayerBudget(i)) {
        childLayer = tgfx::Layer::Make();
        std::vector<uint32_t> childPath = *pathStack;
        childPath.push_back(i);
        ctx->layerByPath[PackLayerPath(childPath)] = childLayer;
      } else {
        continue;
      }
    }
    layer->addChild(childLayer);
  }

  return layer;
}

std::shared_ptr<tgfx::Layer> inflateComposition(PAGDocument& doc, uint32_t compositionIndex,
                                                std::vector<uint32_t>* pathStack,
                                                InflaterContext* ctx) {
  if (compositionIndex >= doc.compositions.size() ||
      doc.compositions[compositionIndex] == nullptr) {
    // Decoder should have caught this, but the Inflater is still defence in
    // depth — return nullptr and let the caller degrade to a placeholder.
    return nullptr;
  }
  CompositionVisitScope scope(ctx, compositionIndex);
  if (!scope) {
    return nullptr;  // Cycle / over-depth → warn already pushed by the scope.
  }
  const auto& comp = *doc.compositions[compositionIndex];
  auto container = tgfx::Layer::Make();
  for (uint32_t i = 0; i < comp.layers.size(); ++i) {
    pathStack->push_back(i);
    auto childLayer = inflateLayer(doc, *comp.layers[i], pathStack, ctx);
    pathStack->pop_back();
    if (childLayer != nullptr) {
      container->addChild(childLayer);
    }
  }
  return container;
}

// ---------------------------------------------------------------------------
// Task 3-6 stubs — filled in by later phases of this TDD pass. Keeping them
// defined but minimal so the skeleton compiles standalone; the follow-up
// edits widen each body to mirror the corresponding LayerBuilder section.
// ---------------------------------------------------------------------------

// ---------- applyCommon (mirror of LayerBuilder::applyLayerAttributes) ----------
//
// Every setter here is 1:1 with src/renderer/LayerBuilder.cpp:736-801. The
// conditional "only set when non-default" shape is preserved even though the
// setters are idempotent — if LayerBuilder ever relies on a default remaining
// un-touched (e.g. future auto-dirty tracking), Inflater should track it.

void applyCommon(const Layer& src, tgfx::Layer* layer, InflaterContext* ctx) {
  layer->setVisible(src.visible.value);
  layer->setAlpha(src.alpha.value);

  // PAGDocument's BlendMode default is SrcOver (set in `PAGDocument.h:570`).
  // LayerBuilder checks `!= Normal` against the PAGX enum; Baker converted
  // `pagx::BlendMode::Normal` → `tgfx::BlendMode::SrcOver`, so the same
  // semantic filter here is "!= SrcOver".
  if (src.blendMode.value != tgfx::BlendMode::SrcOver) {
    layer->setBlendMode(src.blendMode.value);
  }

  // Matrix: PAGX's `renderPosition()` folds translate into the matrix at
  // bake time; PAGDocument stores the already-combined matrix. Only set if
  // non-identity so downstream LayerRecorder can skip the composite.
  if (!src.matrix.value.isIdentity()) {
    layer->setMatrix(src.matrix.value);
  }

  if (!src.matrix3D.value.isIdentity()) {
    layer->setMatrix3D(src.matrix3D.value);
  }
  if (src.preserve3D) {
    layer->setPreserve3D(true);
  }

  // antiAlias default is true (PAGDocument stores `allowsEdgeAntialiasing`
  // directly). LayerBuilder guards on the false path only.
  if (!src.allowsEdgeAntialiasing) {
    layer->setAllowsEdgeAntialiasing(false);
  }
  layer->setAllowsGroupOpacity(src.allowsGroupOpacity);
  if (!src.passThroughBackground) {
    layer->setPassThroughBackground(false);
  }
  if (src.hasScrollRect) {
    layer->setScrollRect(src.scrollRect.value);
  }

  // Layer styles.
  if (!src.styles.empty()) {
    std::vector<std::shared_ptr<tgfx::LayerStyle>> tgfxStyles;
    tgfxStyles.reserve(src.styles.size());
    for (const auto& style : src.styles) {
      if (style == nullptr) {
        continue;
      }
      auto tgfxStyle = inflateLayerStyle(*style);
      if (tgfxStyle != nullptr) {
        if (style->excludeChildEffects) {
          tgfxStyle->setExcludeChildEffects(true);
        }
        tgfxStyles.push_back(std::move(tgfxStyle));
      }
    }
    if (!tgfxStyles.empty()) {
      layer->setLayerStyles(tgfxStyles);
    }
  }

  // Layer filters.
  if (!src.filters.empty()) {
    std::vector<std::shared_ptr<tgfx::LayerFilter>> tgfxFilters;
    tgfxFilters.reserve(src.filters.size());
    for (const auto& filter : src.filters) {
      if (filter == nullptr) {
        continue;
      }
      auto tgfxFilter = inflateLayerFilter(*filter);
      if (tgfxFilter != nullptr) {
        tgfxFilters.push_back(std::move(tgfxFilter));
      }
    }
    if (!tgfxFilters.empty()) {
      layer->setFilters(tgfxFilters);
    }
  }

  (void)ctx;  // ctx threaded in case future hooks need diagnostics here.
}

// ---------- LayerFilter dispatch (§D.12 / LayerBuilder::convertLayerFilter) ----------

std::shared_ptr<tgfx::LayerFilter> inflateLayerFilter(const LayerFilter& src) {
  switch (src.type) {
    case LayerFilterType::Blur:
      return tgfx::BlurFilter::Make(src.blurX.value, src.blurY.value, src.tileMode);
    case LayerFilterType::DropShadow:
      return tgfx::DropShadowFilter::Make(src.offsetX.value, src.offsetY.value, src.blurX.value,
                                          src.blurY.value, src.color.value, src.shadowOnly);
    case LayerFilterType::InnerShadow:
      return tgfx::InnerShadowFilter::Make(src.offsetX.value, src.offsetY.value, src.blurX.value,
                                           src.blurY.value, src.color.value, src.shadowOnly);
    case LayerFilterType::ColorMatrix:
      return tgfx::ColorMatrixFilter::Make(src.colorMatrix.value);
    case LayerFilterType::Blend:
      return tgfx::BlendFilter::Make(src.blendColor.value, src.blendFilterMode);
  }
  return nullptr;
}

// ---------- LayerStyle dispatch (§D.12 / LayerBuilder::convertLayerStyle) ----------

std::shared_ptr<tgfx::LayerStyle> inflateLayerStyle(const LayerStyle& src) {
  std::shared_ptr<tgfx::LayerStyle> style;
  switch (src.type) {
    case LayerStyleType::DropShadow:
      style = tgfx::DropShadowStyle::Make(src.offsetX.value, src.offsetY.value, src.blurX.value,
                                          src.blurY.value, src.color.value, src.showBehindLayer);
      break;
    case LayerStyleType::InnerShadow:
      style = tgfx::InnerShadowStyle::Make(src.offsetX.value, src.offsetY.value, src.blurX.value,
                                           src.blurY.value, src.color.value);
      break;
    case LayerStyleType::BackgroundBlur:
      style = tgfx::BackgroundBlurStyle::Make(src.blurX.value, src.blurY.value, src.tileMode);
      break;
  }
  if (style != nullptr && src.blendMode != tgfx::BlendMode::SrcOver) {
    style->setBlendMode(src.blendMode);
  }
  return style;
}

// ---------- ShapeStyleData → tgfx::ColorSource (LayerBuilder::convertColorSource) ----------

std::shared_ptr<tgfx::ColorSource> inflateColorSource(PAGDocument& doc, const ShapeStyleData& style,
                                                      InflaterContext* ctx) {
  switch (style.sourceType) {
    case ColorSourceType::SolidColor:
      return tgfx::SolidColor::Make(style.color.value);

    case ColorSourceType::LinearGradient: {
      auto gradient = tgfx::Gradient::MakeLinear(style.startPoint.value, style.endPoint.value,
                                                 style.stopColors.value, style.stopPositions.value);
      if (gradient != nullptr) {
        gradient->setFitsToGeometry(style.fitsToGeometry);
        if (!style.gradientMatrix.value.isIdentity()) {
          gradient->setMatrix(style.gradientMatrix.value);
        }
      }
      return gradient;
    }
    case ColorSourceType::RadialGradient: {
      auto gradient = tgfx::Gradient::MakeRadial(style.center.value, style.radius.value,
                                                 style.stopColors.value, style.stopPositions.value);
      if (gradient != nullptr) {
        gradient->setFitsToGeometry(style.fitsToGeometry);
        if (!style.gradientMatrix.value.isIdentity()) {
          gradient->setMatrix(style.gradientMatrix.value);
        }
      }
      return gradient;
    }
    case ColorSourceType::ConicGradient: {
      auto gradient = tgfx::Gradient::MakeConic(style.center.value, style.startAngle.value,
                                                style.endAngle.value, style.stopColors.value,
                                                style.stopPositions.value);
      if (gradient != nullptr) {
        gradient->setFitsToGeometry(style.fitsToGeometry);
        if (!style.gradientMatrix.value.isIdentity()) {
          gradient->setMatrix(style.gradientMatrix.value);
        }
      }
      return gradient;
    }
    case ColorSourceType::DiamondGradient: {
      auto gradient =
          tgfx::Gradient::MakeDiamond(style.center.value, style.radius.value,
                                      style.stopColors.value, style.stopPositions.value);
      if (gradient != nullptr) {
        gradient->setFitsToGeometry(style.fitsToGeometry);
        if (!style.gradientMatrix.value.isIdentity()) {
          gradient->setMatrix(style.gradientMatrix.value);
        }
      }
      return gradient;
    }

    case ColorSourceType::ImagePattern: {
      const uint32_t idx = style.patternImageIndex;
      if (idx == UINT32_MAX || idx >= doc.images.size() || doc.images[idx] == nullptr ||
          doc.images[idx]->data == nullptr) {
        // Sentinel / missing image — Baker already warned at 200 (§G.2). We
        // don't double-warn here, just fall back to a null ColorSource so
        // the caller drops to SolidColor::Make() default.
        return nullptr;
      }
      auto image =
          tgfx::Image::MakeFromEncoded(std::const_pointer_cast<tgfx::Data>(doc.images[idx]->data));
      if (image == nullptr) {
        ctx->warn(ErrorCode::InflateImageDecodeFailed,
                  "tgfx::Image::MakeFromEncoded returned null for patternImageIndex", idx);
        return nullptr;
      }
      // tgfx::Image now owns the decoded payload (or a ref to the encoded
      // bytes). Release the PAGDocument's reference so downstream peak
      // memory stays bounded (see §11.1 "Inflater 生命周期纪律" + Phase
      // 10.5 `ImageBytesReleasedAfterInflate` contract).
      doc.images[idx]->data.reset();
      tgfx::SamplingOptions sampling(style.filterMode, style.mipmapMode);
      auto pattern = tgfx::ImagePattern::Make(image, style.tileModeX, style.tileModeY, sampling);
      if (pattern != nullptr) {
        // `pagx::pag::ScaleMode` and `tgfx::ScaleMode` share enum values
        // (None=0/Stretch=1/LetterBox=2/Zoom=3; see Phase 6 quirk #21), so
        // the cast is a no-op at the bit level.
        pattern->setScaleMode(static_cast<tgfx::ScaleMode>(style.scaleMode));
        if (!style.patternMatrix.value.isIdentity()) {
          pattern->setMatrix(style.patternMatrix.value);
        }
      }
      return pattern;
    }
  }
  return nullptr;
}

// ---------- Font resolution (Phase 16 runtime-shape) --------------------
//
// Returns a non-null tgfx::Font on success. Lookup order:
//   1. FontProvider.getTypeface(fontFamily, fontStyle) — exact-match path.
//   2. FontProvider.getFallbackTypefaces() — first non-null entry wins.
//   3. All failed → push InflateFontCreateFailed=601 and return an empty
//      Font. The caller will propagate through TextBlob::MakeFrom which
//      itself returns null when the font carries no typeface, and the
//      outer inflateElementText path then surfaces 602.
//
// contextIndex currently defaults to UINT32_MAX because the Baker no longer
// threads a fontIndex — PAGDocument::fonts[] is gone. If future consumers
// need a per-layer cross-reference we will repurpose contextIndex to the
// enclosing layer index.
tgfx::Font resolveFont(const ElementTextData& pay, InflaterContext* ctx) {
  if (ctx->fontProvider == nullptr) {
    // Should never happen — LayerInflater::Inflate substitutes
    // MakeDefaultFontProvider() when the caller passes nullptr. Defensive
    // programming only.
    ctx->warn(ErrorCode::InflateFontCreateFailed, "FontProvider is null");
    return tgfx::Font{};
  }

  auto typeface = ctx->fontProvider->getTypeface(pay.fontFamily, pay.fontStyle);
  if (typeface == nullptr) {
    for (auto& candidate : ctx->fontProvider->getFallbackTypefaces()) {
      if (candidate != nullptr) {
        typeface = std::move(candidate);
        break;
      }
    }
  }
  if (typeface == nullptr) {
    ctx->warn(ErrorCode::InflateFontCreateFailed, "FontProvider exhausted; no typeface matched " +
                                                      pay.fontFamily + "/" + pay.fontStyle);
    return tgfx::Font{};
  }

  tgfx::Font font(std::move(typeface), pay.fontSize);
  if (pay.fauxBold) {
    font.setFauxBold(true);
  }
  if (pay.fauxItalic) {
    font.setFauxItalic(true);
  }
  return font;
}

// ---------- ElementText inflation (Phase 16 runtime-shape MVP) ----------
//
// The Inflater feeds the raw UTF-8 string + the resolved tgfx::Font into
// tgfx::TextBlob::MakeFrom which performs shaping under the hood. Paragraph
// layout (justification / leading / firstBaseLine / BoxText) is deferred to
// Phase 16.7 — the current MVP path uses a single line. PAGX-exclusive
// anchors are forwarded to tgfx::Text::Make for TextModifier / RangeSelector
// consumption.

std::shared_ptr<tgfx::VectorElement> inflateElementText(PAGDocument& /*doc*/,
                                                        const ElementTextData& pay,
                                                        InflaterContext* ctx) {
  if (pay.text.empty()) {
    // Not necessarily an error — an empty Text is legal but produces no
    // glyphs. Skip silently; returning nullptr drops the element so the
    // enclosing VectorGroup still renders its other children.
    return nullptr;
  }

  auto font = resolveFont(pay, ctx);
  if (font.getTypeface() == nullptr) {
    // resolveFont already warned. Drop the element.
    return nullptr;
  }

  std::shared_ptr<tgfx::TextBlob> textBlob;

  // Pre-shaped hint path (§10.5 Phase 16.6): when the Baker recognized a
  // TextBox-child Text, it snapshots PAGX TextLayout's already-resolved
  // glyphs + positions + optional RSXforms into pay.shapedRuns. Replaying
  // them directly via GlyphRunRenderer::BuildTextBlobFromLayoutRuns
  // reproduces every layout decision runtime shape can't express — multi-
  // line x-offsets, justify spacing, vertical writing mode per-glyph
  // (x,y)+rotation — with no paragraph-layout port required on this side.
  //
  // We only trust the hint when every run's typefaceKey matches the
  // typeface re-resolved by our FontProvider. Any mismatch falls back to
  // runtime shape (the host presumably substituted a different font; we
  // want the render to match that substitution, not the export-time font).
  if (!pay.shapedRuns.empty() && ctx->fontProvider != nullptr) {
    std::vector<pagx::TextLayoutGlyphRun> runs;
    runs.reserve(pay.shapedRuns.size());
    bool allKeysMatch = true;
    for (const auto& sr : pay.shapedRuns) {
      auto tf = ctx->fontProvider->getTypeface(sr.typefaceFamily, sr.typefaceStyle);
      if (tf == nullptr || MakeTypefaceKey(*tf) != sr.typefaceKey) {
        allKeysMatch = false;
        break;
      }
      pagx::TextLayoutGlyphRun r;
      r.font = tgfx::Font(tf, sr.fontSize);
      r.glyphs = sr.glyphs;
      r.positions = sr.positions;
      r.xforms = sr.xforms;
      runs.push_back(std::move(r));
    }
    if (allKeysMatch) {
      textBlob = pagx::GlyphRunRenderer::BuildTextBlobFromLayoutRuns(runs, tgfx::Matrix::I());
    } else {
      ctx->warn(ErrorCode::TextShapingHintMiss,
                "ElementText.shapedRuns typefaceKey mismatch; falling back to runtime shape");
    }
  }

#ifdef PAG_USE_HARFBUZZ
  // Preferred path: if the FontProvider exposes its pagx::FontConfig we can
  // shape through the same HarfBuzz-backed pagx::TextShaper the PAGX layout
  // engine uses. That matches Path A (LayerBuilder) byte-for-byte on the
  // per-glyph xAdvance, so CrossCheck PSNR is no longer limited by the
  // HarfBuzz-vs-CoreText advance drift (Arial Bold 84pt: HB says 49.79,
  // CoreText says 56 — 11% per glyph that the primitive shaper can't fix).
  if (textBlob == nullptr && ctx->fontProvider != nullptr) {
    if (auto* config = ctx->fontProvider->getFontConfig()) {
      pagx::LayoutContext shapingContext(config);
      auto shapedGlyphs = pagx::TextShaper::Shape(pay.text, font, shapingContext,
                                                  /*vertical=*/false, /*rtl=*/false);
      std::vector<tgfx::GlyphID> glyphIDs;
      std::vector<tgfx::Point> positions;
      glyphIDs.reserve(shapedGlyphs.size());
      positions.reserve(shapedGlyphs.size());
      float x = 0.0f;
      for (const auto& sg : shapedGlyphs) {
        if (sg.glyphID == 0) {
          continue;
        }
        glyphIDs.push_back(sg.glyphID);
        positions.push_back({x + sg.xOffset, sg.yOffset});
        x += sg.xAdvance + pay.tracking;
      }
      if (!glyphIDs.empty()) {
        textBlob =
            tgfx::TextBlob::MakeFrom(glyphIDs.data(), positions.data(), glyphIDs.size(), font);
      }
    }
  }
#endif

  if (textBlob == nullptr) {
    // Fall back to the tgfx primitive shaper. No kerning / ligatures / bidi,
    // and the xAdvance per glyph will come from font.getAdvance() instead of
    // HarfBuzz — that's fine on hosts that didn't wire a FontConfig-backed
    // provider (e.g. production PAGLoader callers using pag::FontManager).
    textBlob = tgfx::TextBlob::MakeFrom(pay.text, font);
  }

  if (textBlob == nullptr) {
    ctx->warn(ErrorCode::InflateGlyphRunBuildFailed,
              "TextBlob construction failed for ElementText; dropping element");
    return nullptr;
  }

  auto text = tgfx::Text::Make(std::move(textBlob), pay.anchors.value);
  if (text != nullptr) {
    text->setPosition(pay.position.value);
  }
  return text;
}

// ---------- VectorElement dispatcher (LayerBuilder::convertVectorElement) ----------

std::shared_ptr<tgfx::VectorElement> inflateVectorElement(PAGDocument& doc,
                                                          const VectorElement& src,
                                                          InflaterContext* ctx) {
  switch (src.type) {
    case VectorElementType::Rectangle: {
      const auto& pay = std::get<std::unique_ptr<ElementRectangleData>>(src.payload);
      auto rect = tgfx::Rectangle::Make();
      rect->setPosition(pay->position.value);
      rect->setSize({pay->size.value.x, pay->size.value.y});
      rect->setRoundness(pay->roundness.value);
      rect->setReversed(pay->reversed);
      return rect;
    }
    case VectorElementType::Ellipse: {
      const auto& pay = std::get<std::unique_ptr<ElementEllipseData>>(src.payload);
      auto ellipse = tgfx::Ellipse::Make();
      ellipse->setPosition(pay->position.value);
      ellipse->setSize({pay->size.value.x, pay->size.value.y});
      ellipse->setReversed(pay->reversed);
      return ellipse;
    }
    case VectorElementType::Polystar: {
      const auto& pay = std::get<std::unique_ptr<ElementPolystarData>>(src.payload);
      auto polystar = tgfx::Polystar::Make();
      polystar->setPosition(pay->position.value);
      polystar->setPolystarType(pay->polystarType);
      polystar->setPointCount(pay->pointCount.value);
      polystar->setRotation(pay->rotation.value);
      polystar->setOuterRadius(pay->outerRadius.value);
      polystar->setOuterRoundness(pay->outerRoundness.value);
      polystar->setInnerRadius(pay->innerRadius.value);
      polystar->setInnerRoundness(pay->innerRoundness.value);
      polystar->setReversed(pay->reversed);
      return polystar;
    }
    case VectorElementType::ShapePath: {
      const auto& pay = std::get<std::unique_ptr<ElementShapePathData>>(src.payload);
      auto shape = tgfx::ShapePath::Make();
      shape->setPosition(pay->position.value);
      shape->setPath(pay->path.value);
      shape->setReversed(pay->reversed);
      return shape;
    }
    case VectorElementType::FillStyle: {
      const auto& pay = std::get<std::unique_ptr<ElementFillStyleData>>(src.payload);
      std::shared_ptr<tgfx::ColorSource> colorSource;
      if (pay->style != nullptr) {
        colorSource = inflateColorSource(doc, *pay->style, ctx);
      }
      if (colorSource == nullptr) {
        // LayerBuilder fallback: ensure FillStyle always has a ColorSource.
        colorSource = tgfx::SolidColor::Make();
      }
      auto fill = tgfx::FillStyle::Make(colorSource);
      if (fill != nullptr && pay->style != nullptr) {
        fill->setAlpha(pay->style->alpha.value);
        if (pay->style->blendMode != tgfx::BlendMode::SrcOver) {
          fill->setBlendMode(pay->style->blendMode);
        }
        if (pay->fillRule != tgfx::FillRule::Winding) {
          fill->setFillRule(pay->fillRule);
        }
        if (pay->placement != tgfx::LayerPlacement::Background) {
          fill->setPlacement(pay->placement);
        }
      }
      return fill;
    }
    case VectorElementType::StrokeStyle: {
      const auto& pay = std::get<std::unique_ptr<ElementStrokeStyleData>>(src.payload);
      std::shared_ptr<tgfx::ColorSource> colorSource;
      if (pay->style != nullptr) {
        colorSource = inflateColorSource(doc, *pay->style, ctx);
      }
      if (colorSource == nullptr) {
        colorSource = tgfx::SolidColor::Make();
      }
      auto stroke = tgfx::StrokeStyle::Make(colorSource);
      if (stroke == nullptr) {
        return nullptr;
      }
      if (pay->style != nullptr) {
        stroke->setAlpha(pay->style->alpha.value);
        if (pay->style->blendMode != tgfx::BlendMode::SrcOver) {
          stroke->setBlendMode(pay->style->blendMode);
        }
      }
      stroke->setStrokeWidth(pay->strokeWidth.value);
      stroke->setLineCap(pay->lineCap);
      stroke->setLineJoin(pay->lineJoin);
      stroke->setMiterLimit(pay->miterLimit.value);
      if (!pay->lineDashPattern.value.empty()) {
        stroke->setDashes(pay->lineDashPattern.value);
        stroke->setDashOffset(pay->lineDashPhase.value);
        stroke->setDashAdaptive(pay->lineDashAdaptive);
      }
      if (pay->strokeAlign != tgfx::StrokeAlign::Center) {
        stroke->setStrokeAlign(pay->strokeAlign);
      }
      if (pay->placement != tgfx::LayerPlacement::Background) {
        stroke->setPlacement(pay->placement);
      }
      return stroke;
    }
    case VectorElementType::TrimPath: {
      const auto& pay = std::get<std::unique_ptr<ElementTrimPathData>>(src.payload);
      auto trim = tgfx::TrimPath::Make();
      trim->setStart(pay->start.value);
      trim->setEnd(pay->end.value);
      trim->setOffset(pay->offset.value);
      if (pay->trimType != tgfx::TrimPathType::Separate) {
        trim->setTrimType(pay->trimType);
      }
      return trim;
    }
    case VectorElementType::RoundCorner: {
      const auto& pay = std::get<std::unique_ptr<ElementRoundCornerData>>(src.payload);
      auto round = tgfx::RoundCorner::Make();
      round->setRadius(pay->radius.value);
      return round;
    }
    case VectorElementType::MergePath: {
      const auto& pay = std::get<std::unique_ptr<ElementMergePathData>>(src.payload);
      auto merge = tgfx::MergePath::Make();
      if (pay->mode != tgfx::MergePathOp::Append) {
        merge->setMode(pay->mode);
      }
      return merge;
    }
    case VectorElementType::Repeater: {
      const auto& pay = std::get<std::unique_ptr<ElementRepeaterData>>(src.payload);
      auto rep = tgfx::Repeater::Make();
      rep->setCopies(pay->copies.value);
      rep->setOffset(pay->offset.value);
      rep->setOrder(pay->order);
      rep->setAnchor(pay->anchor.value);
      rep->setPosition(pay->position.value);
      rep->setRotation(pay->rotation.value);
      rep->setScale(pay->scale.value);
      rep->setStartAlpha(pay->startAlpha.value);
      rep->setEndAlpha(pay->endAlpha.value);
      return rep;
    }
    case VectorElementType::Text: {
      const auto& pay = std::get<std::unique_ptr<ElementTextData>>(src.payload);
      return inflateElementText(doc, *pay, ctx);
    }
    case VectorElementType::TextPath: {
      const auto& pay = std::get<std::unique_ptr<ElementTextPathData>>(src.payload);
      auto textPath = tgfx::TextPath::Make();
      textPath->setPath(pay->path.value);
      textPath->setBaselineOrigin(pay->baselineOrigin.value);
      textPath->setBaselineAngle(pay->baselineAngle.value);
      textPath->setFirstMargin(pay->firstMargin.value);
      textPath->setLastMargin(pay->lastMargin.value);
      textPath->setPerpendicular(pay->perpendicular);
      textPath->setReversed(pay->reversed);
      textPath->setForceAlignment(pay->forceAlignment);
      return textPath;
    }
    case VectorElementType::TextModifier: {
      const auto& pay = std::get<std::unique_ptr<ElementTextModifierData>>(src.payload);
      auto mod = tgfx::TextModifier::Make();
      mod->setAnchor(pay->anchor.value);
      mod->setPosition(pay->position.value);
      mod->setRotation(pay->rotation.value);
      mod->setScale(pay->scale.value);
      mod->setSkew(pay->skew.value);
      mod->setSkewAxis(pay->skewAxis.value);
      mod->setAlpha(pay->alpha.value);
      if (pay->hasFillColor) {
        mod->setFillColor(pay->fillColor.value);
      }
      if (pay->hasStrokeColor) {
        mod->setStrokeColor(pay->strokeColor.value);
      }
      if (pay->hasStrokeWidth) {
        mod->setStrokeWidth(pay->strokeWidth.value);
      }
      std::vector<std::shared_ptr<tgfx::TextSelector>> tgfxSelectors;
      tgfxSelectors.reserve(pay->rangeSelectors.size());
      for (const auto& sel : pay->rangeSelectors) {
        if (sel == nullptr) {
          continue;
        }
        auto tgfxSel = std::make_shared<tgfx::RangeSelector>();
        tgfxSel->setStart(sel->start.value);
        tgfxSel->setEnd(sel->end.value);
        tgfxSel->setOffset(sel->offset.value);
        tgfxSel->setUnit(sel->unit);
        tgfxSel->setShape(sel->shape);
        tgfxSel->setEaseIn(sel->easeIn.value);
        tgfxSel->setEaseOut(sel->easeOut.value);
        tgfxSel->setMode(sel->mode);
        tgfxSel->setWeight(sel->weight.value);
        tgfxSel->setRandomOrder(sel->randomOrder);
        tgfxSel->setRandomSeed(sel->randomSeed);
        tgfxSelectors.push_back(std::move(tgfxSel));
      }
      mod->setSelectors(std::move(tgfxSelectors));
      return mod;
    }
    case VectorElementType::VectorGroup: {
      const auto& pay = std::get<std::unique_ptr<ElementVectorGroupData>>(src.payload);
      auto group = tgfx::VectorGroup::Make();
      std::vector<std::shared_ptr<tgfx::VectorElement>> children;
      children.reserve(pay->elements.size());
      for (const auto& childEl : pay->elements) {
        if (childEl == nullptr) {
          continue;
        }
        auto tgfxChild = inflateVectorElement(doc, *childEl, ctx);
        if (tgfxChild != nullptr) {
          children.push_back(std::move(tgfxChild));
        }
      }
      group->setElements(std::move(children));
      group->setAnchor(pay->anchor.value);
      group->setPosition(pay->position.value);
      group->setScale(pay->scale.value);
      group->setRotation(pay->rotation.value);
      group->setAlpha(pay->alpha.value);
      group->setSkew(pay->skew.value);
      group->setSkewAxis(pay->skewAxis.value);
      return group;
    }
  }
  return nullptr;
}

std::shared_ptr<tgfx::Layer> inflateVectorPayload(PAGDocument& doc, const VectorPayload& pay,
                                                  InflaterContext* ctx) {
  auto layer = tgfx::VectorLayer::Make();
  std::vector<std::shared_ptr<tgfx::VectorElement>> elements;
  elements.reserve(pay.contents.size());
  for (const auto& element : pay.contents) {
    if (element == nullptr) {
      continue;
    }
    auto tgfxEl = inflateVectorElement(doc, *element, ctx);
    if (tgfxEl != nullptr) {
      elements.push_back(std::move(tgfxEl));
    }
  }
  layer->setContents(std::move(elements));
  return layer;
}

// ---------- ShapeStyleData → tgfx::ShapeStyle (ShapeLayer-only) ----------
//
// PAGDocument's ShapeStyleData carries a VectorLayer-oriented ColorSource
// graph. tgfx::ShapeStyle (used by tgfx::ShapeLayer) wants a Color OR a
// Shader; gradient / image sources route through tgfx::Shader factories
// instead of the VectorLayer ColorSource path. Phase 9 keeps the two
// conversions side-by-side so ShapePayload can land without waiting for a
// future ShapeStyle ↔ ColorSource unification.

std::shared_ptr<tgfx::ShapeStyle> inflateShapeStyle(PAGDocument& doc, const ShapeStyleData& style,
                                                    InflaterContext* ctx) {
  const float styleAlpha = style.alpha.value;
  const tgfx::BlendMode blendMode = style.blendMode;

  switch (style.sourceType) {
    case ColorSourceType::SolidColor: {
      tgfx::Color color = style.color.value;
      color.alpha *= styleAlpha;  // ShapeStyle::Make(color) packs alpha into color.
      return tgfx::ShapeStyle::Make(color, blendMode);
    }
    case ColorSourceType::LinearGradient: {
      auto shader =
          tgfx::Shader::MakeLinearGradient(style.startPoint.value, style.endPoint.value,
                                           style.stopColors.value, style.stopPositions.value);
      if (shader == nullptr) {
        return nullptr;
      }
      if (!style.gradientMatrix.value.isIdentity()) {
        shader = shader->makeWithMatrix(style.gradientMatrix.value);
      }
      return tgfx::ShapeStyle::Make(shader, styleAlpha, blendMode);
    }
    case ColorSourceType::RadialGradient: {
      auto shader =
          tgfx::Shader::MakeRadialGradient(style.center.value, style.radius.value,
                                           style.stopColors.value, style.stopPositions.value);
      if (shader == nullptr) {
        return nullptr;
      }
      if (!style.gradientMatrix.value.isIdentity()) {
        shader = shader->makeWithMatrix(style.gradientMatrix.value);
      }
      return tgfx::ShapeStyle::Make(shader, styleAlpha, blendMode);
    }
    case ColorSourceType::ConicGradient: {
      auto shader = tgfx::Shader::MakeConicGradient(style.center.value, style.startAngle.value,
                                                    style.endAngle.value, style.stopColors.value,
                                                    style.stopPositions.value);
      if (shader == nullptr) {
        return nullptr;
      }
      if (!style.gradientMatrix.value.isIdentity()) {
        shader = shader->makeWithMatrix(style.gradientMatrix.value);
      }
      return tgfx::ShapeStyle::Make(shader, styleAlpha, blendMode);
    }
    case ColorSourceType::DiamondGradient: {
      auto shader =
          tgfx::Shader::MakeDiamondGradient(style.center.value, style.radius.value,
                                            style.stopColors.value, style.stopPositions.value);
      if (shader == nullptr) {
        return nullptr;
      }
      if (!style.gradientMatrix.value.isIdentity()) {
        shader = shader->makeWithMatrix(style.gradientMatrix.value);
      }
      return tgfx::ShapeStyle::Make(shader, styleAlpha, blendMode);
    }
    case ColorSourceType::ImagePattern: {
      const uint32_t idx = style.patternImageIndex;
      if (idx == UINT32_MAX || idx >= doc.images.size() || doc.images[idx] == nullptr ||
          doc.images[idx]->data == nullptr) {
        return nullptr;
      }
      auto image =
          tgfx::Image::MakeFromEncoded(std::const_pointer_cast<tgfx::Data>(doc.images[idx]->data));
      if (image == nullptr) {
        ctx->warn(ErrorCode::InflateImageDecodeFailed,
                  "tgfx::Image::MakeFromEncoded returned null (ShapePayload)", idx);
        return nullptr;
      }
      doc.images[idx]->data.reset();
      tgfx::SamplingOptions sampling(style.filterMode, style.mipmapMode);
      auto shader =
          tgfx::Shader::MakeImageShader(image, style.tileModeX, style.tileModeY, sampling);
      if (shader == nullptr) {
        return nullptr;
      }
      if (!style.patternMatrix.value.isIdentity()) {
        shader = shader->makeWithMatrix(style.patternMatrix.value);
      }
      return tgfx::ShapeStyle::Make(shader, styleAlpha, blendMode);
    }
  }
  return nullptr;
}

std::shared_ptr<tgfx::Layer> inflateShapePayload(PAGDocument& doc, const ShapePayload& pay,
                                                 InflaterContext* ctx) {
  auto layer = tgfx::ShapeLayer::Make();
  layer->setPath(pay.path.value);

  // Fill styles.
  std::vector<std::shared_ptr<tgfx::ShapeStyle>> fills;
  fills.reserve(pay.fillStyles.size());
  for (const auto& s : pay.fillStyles) {
    if (s == nullptr) {
      continue;
    }
    auto fill = inflateShapeStyle(doc, *s, ctx);
    if (fill != nullptr) {
      fills.push_back(std::move(fill));
    }
  }
  if (!fills.empty()) {
    layer->setFillStyles(std::move(fills));
  }

  // Stroke styles.
  std::vector<std::shared_ptr<tgfx::ShapeStyle>> strokes;
  strokes.reserve(pay.strokeStyles.size());
  for (const auto& s : pay.strokeStyles) {
    if (s == nullptr) {
      continue;
    }
    auto stroke = inflateShapeStyle(doc, *s, ctx);
    if (stroke != nullptr) {
      strokes.push_back(std::move(stroke));
    }
  }
  if (!strokes.empty()) {
    layer->setStrokeStyles(std::move(strokes));
  }

  // Stroke parameters shared by all strokes.
  layer->setLineWidth(pay.strokeWidth.value);
  layer->setLineCap(pay.lineCap);
  layer->setLineJoin(pay.lineJoin);
  layer->setMiterLimit(pay.miterLimit.value);
  if (!pay.lineDashPattern.value.empty()) {
    layer->setLineDashPattern(pay.lineDashPattern.value);
    layer->setLineDashPhase(pay.lineDashPhase.value);
    layer->setLineDashAdaptive(pay.lineDashAdaptive);
  }
  if (pay.strokeAlign != tgfx::StrokeAlign::Center) {
    layer->setStrokeAlign(pay.strokeAlign);
  }
  if (pay.strokeOnTop) {
    layer->setStrokeOnTop(true);
  }
  return layer;
}

std::shared_ptr<tgfx::Layer> inflateImagePayload(PAGDocument& doc, const ImagePayload& pay,
                                                 InflaterContext* ctx) {
  auto layer = tgfx::ImageLayer::Make();
  const uint32_t idx = pay.imageIndex;
  if (idx == UINT32_MAX || idx >= doc.images.size() || doc.images[idx] == nullptr ||
      doc.images[idx]->data == nullptr) {
    // Missing asset — Baker already warned at 200; the ImageLayer stays
    // imageless so it renders blank but the surrounding tree is intact.
    return layer;
  }
  auto image =
      tgfx::Image::MakeFromEncoded(std::const_pointer_cast<tgfx::Data>(doc.images[idx]->data));
  if (image == nullptr) {
    ctx->warn(ErrorCode::InflateImageDecodeFailed,
              "tgfx::Image::MakeFromEncoded returned null (ImagePayload)", idx);
    return layer;
  }
  doc.images[idx]->data.reset();
  layer->setImage(std::move(image));
  layer->setSampling(pay.sampling);
  return layer;
}

std::shared_ptr<tgfx::Layer> inflateSolidPayload(const SolidPayload& pay) {
  auto layer = tgfx::SolidLayer::Make();
  layer->setWidth(pay.width.value);
  layer->setHeight(pay.height.value);
  layer->setRadiusX(pay.radiusX.value);
  layer->setRadiusY(pay.radiusY.value);
  layer->setColor(pay.color.value);
  return layer;
}

std::shared_ptr<tgfx::Layer> inflateMeshPayload() {
  return tgfx::Layer::Make();
}

}  // namespace

// ---------------------------------------------------------------------------
// Public Inflate entry
// ---------------------------------------------------------------------------

LayerInflater::Result LayerInflater::Inflate(std::unique_ptr<PAGDocument> doc, Options opts) {
  Result result;
  if (doc == nullptr) {
    // Null document is treated as an empty-document case. Returning the 604
    // here keeps the callsite symmetric with the "compositions empty" path.
    InflaterContext ctx;
    ctx.warn(ErrorCode::InflaterEmptyDocument, "PAGDocument is null");
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  InflaterContext ctx;
  ctx.fontProvider = opts.fontProvider ? std::move(opts.fontProvider) : MakeDefaultFontProvider();
  ctx.visitingComposition.assign(doc->compositions.size(), false);

  // Empty-document gate (§9.1): compositions empty OR compositions[0] empty.
  if (doc->compositions.empty() || doc->compositions[0] == nullptr ||
      doc->compositions[0]->layers.empty()) {
    ctx.warn(ErrorCode::InflaterEmptyDocument,
             "compositions empty or compositions[0] has no layers");
    result.warnings = std::move(ctx.warnings);
    return result;
  }

  // Pass 1: walk compositions[0] into a tgfx::Layer tree.
  auto rootLayer = tgfx::Layer::Make();
  std::vector<uint32_t> pathStack;
  pathStack.reserve(16);
  {
    CompositionVisitScope scope(&ctx, 0);
    if (!scope) {
      // compositions[0] self-loop was already reported by the scope ctor.
      result.warnings = std::move(ctx.warnings);
      return result;
    }
    const auto& comp0 = *doc->compositions[0];
    for (uint32_t i = 0; i < comp0.layers.size(); ++i) {
      pathStack.push_back(i);
      auto childLayer = inflateLayer(*doc, *comp0.layers[i], &pathStack, &ctx);
      pathStack.pop_back();
      if (childLayer != nullptr) {
        rootLayer->addChild(childLayer);
      }
    }
  }

  // Pass 2: apply masks.
  for (const auto& pending : ctx.pendingMasks) {
    auto it = ctx.layerByPath.find(pending.targetPath);
    if (it == ctx.layerByPath.end() || it->second == nullptr) {
      ctx.warn(ErrorCode::InflateMaskResolveFailed,
               "maskLayerPath did not resolve to an inflated layer");
      continue;
    }
    auto maskLayer = it->second;
    maskLayer->setVisible(true);
    pending.host->setMask(maskLayer);
    pending.host->setMaskType(pending.maskType);
  }

  // Finalize: transfer warnings to the Result.
  result.layer = std::move(rootLayer);
  result.warnings = std::move(ctx.warnings);
  return result;
}

}  // namespace pagx::pag
