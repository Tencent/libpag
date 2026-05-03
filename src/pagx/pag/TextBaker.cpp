// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 8 TextBaker.
//
// PAGX exposes two Text rendering modes:
//
//   1. Pre-shaped: `text.glyphRuns` holds pagx::GlyphRun* entries with
//      explicit glyph IDs, positions, anchors, scales, rotations, and
//      skews. Each run references a pagx::Font whose glyph outlines (path
//      or image) live inline in the PAGX document. The Baker emits one
//      ClassicGlyphRunBlob per run.
//
//   2. Runtime shaping: `text.glyphRuns` is empty. The PAGX layout engine
//      (TextLayout, driven by PAGXDocument::applyLayout() through
//      Text::onMeasure) shapes the text with the declared fontFamily /
//      fontStyle and produces layoutRuns stored inside text.glyphData.
//      The Baker reads those back via `friend class pag::TextBaker` and
//      emits one LayoutRun GlyphRunBlob per run.
//
// Font handling:
//   - Phase 8 does NOT bundle TTF bytes with FontAssets. PAGX's pagx::Font
//     carries per-glyph outlines rather than raw font bytes, and PAGX's
//     Text runtime-shaping mode relies on the host platform's system
//     fonts. We register a System-kind FontAsset per (family, style) or
//     per pagx::Font* so fontIndex slots are stable; Phase 10 PAGExporter
//     will upgrade these to Embedded FontAssets once FontEmbedder has had
//     a chance to run.
//   - Missing / null fonts → UINT32_MAX sentinel + FontSourceMissing=201
//     warning per §C.6 / §G.2.
#include "pagx/pag/TextBaker.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include "pagx/TextLayout.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TextSelector.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/ResourceBaker.h"
#include "renderer/GlyphRunRenderer.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Typeface.h"

namespace pagx::pag {

namespace {

tgfx::Color ToTgfxColor(const pagx::Color& c) {
  return tgfx::Color{c.red, c.green, c.blue, c.alpha};
}

}  // namespace

uint32_t TextBaker::InternFont(BakeContext& ctx, PAGDocument& doc, const pagx::Font* font) {
  if (font == nullptr) {
    ctx.warn(ErrorCode::FontSourceMissing, "Text references a null pagx::Font");
    return UINT32_MAX;
  }
  // Synthesize a stable semantic key so the same pagx::Font* reused across
  // multiple GlyphRuns collapses to a single FontAsset slot. Using the
  // node pointer keeps this deterministic within one bake run; cross-run
  // deduplication isn't needed until Phase 10 bundles actual TTF bytes.
  // Semantic key format: "embedded\0<hex pointer>" so the key string
  // sorts alongside System fonts which use "system\0family\0style".
  char ptrBuf[32];
  std::snprintf(ptrBuf, sizeof(ptrBuf), "%p", static_cast<const void*>(font));
  std::string semanticKey = "embedded";
  semanticKey.push_back('\0');
  semanticKey.append(ptrBuf);

  auto asset = std::make_unique<FontAsset>();
  asset->kind = FontSourceKind::System;
  asset->family = "#embedded";
  asset->style = "";
  return RegisterFont(ctx, doc, std::move(asset), font, std::move(semanticKey));
}

uint32_t TextBaker::InternSystemFont(BakeContext& ctx, PAGDocument& doc, const std::string& family,
                                     const std::string& style) {
  if (family.empty()) {
    ctx.warn(ErrorCode::FontSourceMissing, "Text runtime shaping produced an empty font family");
    return UINT32_MAX;
  }
  // Semantic key: "system\0<family>\0<style>" per ResourceBaker convention.
  std::string semanticKey;
  semanticKey.reserve(7 + family.size() + 1 + style.size() + 1);
  semanticKey.append("system");
  semanticKey.push_back('\0');
  semanticKey.append(family);
  semanticKey.push_back('\0');
  semanticKey.append(style);

  auto asset = std::make_unique<FontAsset>();
  asset->kind = FontSourceKind::System;
  asset->family = family;
  asset->style = style;
  return RegisterFont(ctx, doc, std::move(asset), /*nodePtr=*/nullptr, std::move(semanticKey));
}

namespace {

// Builds a ClassicGlyphRunBlob from a single pagx::GlyphRun. PAGX stores
// per-glyph attribute vectors independently — if one is shorter than
// glyphs.size(), we pad with the field's default so the wire format's
// fixed-length quantised lists stay well-formed.
GlyphRunBlob BakeClassicRun(BakeContext& ctx, PAGDocument& doc, const pagx::GlyphRun& src) {
  GlyphRunBlob blob;
  blob.kind = GlyphRunKind::ClassicGlyphRun;
  blob.fontIndex = TextBaker::InternFont(ctx, doc, src.font);
  blob.fontSize = src.fontSize;
  blob.inverseMatrix = tgfx::Matrix::I();
  blob.baseX = src.x;
  blob.baseY = src.y;

  const auto glyphCount = src.glyphs.size();
  blob.classicGlyphs.resize(glyphCount);
  for (size_t i = 0; i < glyphCount; ++i) {
    auto& g = blob.classicGlyphs[i];
    g.glyphId = src.glyphs[i];
    g.xOffset = i < src.xOffsets.size() ? src.xOffsets[i] : 0.0f;
    if (i < src.positions.size()) {
      g.position = tgfx::Point{src.positions[i].x, src.positions[i].y};
    }
    if (i < src.anchors.size()) {
      g.anchor = tgfx::Point{src.anchors[i].x, src.anchors[i].y};
    }
    if (i < src.scales.size()) {
      g.scale = tgfx::Point{src.scales[i].x, src.scales[i].y};
    } else {
      g.scale = tgfx::Point{1.0f, 1.0f};
    }
    g.rotation = i < src.rotations.size() ? src.rotations[i] : 0.0f;
    g.skew = i < src.skews.size() ? src.skews[i] : 0.0f;
  }
  return blob;
}

// Builds a LayoutRun GlyphRunBlob from a runtime-shaped TextLayoutGlyphRun.
// xforms is either empty (→ hasXform=false for every glyph) or has length
// equal to glyphs.size() (one RSXform per glyph). Entries whose RSXform is
// the identity (scos=1, ssin=0, tx=ty=0) collapse to hasXform=false to
// keep the hasXformBits bitstream maximally sparse.
GlyphRunBlob BakeLayoutRun(BakeContext& ctx, PAGDocument& doc, const TextLayoutGlyphRun& src,
                           const std::string& runFamily, const std::string& runStyle) {
  GlyphRunBlob blob;
  blob.kind = GlyphRunKind::LayoutRun;
  blob.fontIndex = TextBaker::InternSystemFont(ctx, doc, runFamily, runStyle);
  blob.fontSize = src.font.getSize();
  blob.inverseMatrix = tgfx::Matrix::I();

  const auto glyphCount = src.glyphs.size();
  blob.layoutGlyphs.resize(glyphCount);
  const bool hasXforms = (src.xforms.size() == glyphCount);
  for (size_t i = 0; i < glyphCount; ++i) {
    auto& g = blob.layoutGlyphs[i];
    g.glyphId = src.glyphs[i];
    if (i < src.positions.size()) {
      g.position = tgfx::Point{src.positions[i].x, src.positions[i].y};
    }
    if (hasXforms) {
      const auto& x = src.xforms[i];
      const bool isIdentity = x.scos == 1.0f && x.ssin == 0.0f && x.tx == 0.0f && x.ty == 0.0f;
      g.hasXform = !isIdentity;
      g.scos = x.scos;
      g.ssin = x.ssin;
      g.tx = x.tx;
      g.ty = x.ty;
    }
  }
  return blob;
}

}  // namespace

std::unique_ptr<VectorElement> TextBaker::BakeText(BakeContext& ctx, PAGDocument& doc,
                                                   const pagx::Text& src) {
  auto data = std::make_unique<ElementTextData>();
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});

  if (!src.glyphRuns.empty()) {
    // Pre-shaped mode. Concatenate per-glyph anchors from all runs so the
    // ElementTextData.anchors mirrors the total glyph count.
    std::vector<tgfx::Point> allAnchors;
    for (const auto* run : src.glyphRuns) {
      if (run == nullptr) continue;
      for (const auto& a : run->anchors) {
        allAnchors.push_back(tgfx::Point{a.x, a.y});
      }
      data->glyphRuns.push_back(BakeClassicRun(ctx, doc, *run));
    }
    data->anchors = MakeProp(std::move(allAnchors));
  } else {
    // Runtime shaping mode — read layoutRuns populated by Text::onMeasure
    // via GlyphRunRenderer::GetLayoutRuns (the only access path that stays
    // inside the renderer's existing friend grant without widening Text's
    // public header surface for v2).
    const auto& layoutRuns = GlyphRunRenderer::GetLayoutRuns(&src);
    if (!layoutRuns.empty()) {
      for (const auto& run : layoutRuns) {
        // The family/style we emit into the FontAsset is the *authored*
        // string, not whatever tgfx resolved through fallback: the
        // Inflater redoes font matching at load time, and re-resolving
        // against the authored string gives the closest approximation.
        data->glyphRuns.push_back(BakeLayoutRun(ctx, doc, run, src.fontFamily, src.fontStyle));
      }
      // LayoutRun anchors are implied (not baked per-glyph here); leave the
      // Property at its default empty vector.
    } else {
      // Text carries no pre-shaped runs and hasn't been laid out — emit a
      // warning so callers know the Baker produced an empty ElementText.
      ctx.warn(ErrorCode::FontSourceMissing,
               "Text has no pre-shaped glyphRuns and no layoutRuns (applyLayout not run?)");
    }
  }

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::Text;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> TextBaker::BakeTextPath(BakeContext& /*ctx*/,
                                                       const pagx::TextPath& src) {
  auto data = std::make_unique<ElementTextPathData>();
  if (src.path != nullptr) {
    // LayerBuilder (renderer/LayerBuilder.cpp `convertTextPath`) applies
    // `getScaledPath(src.path, renderScale())` then
    // `path.transform(MakeTrans(renderPosition))`. Mirror both here so
    // path coordinates on the wire match LayerBuilder's direct-render
    // output. Phase 11.6 fix.
    auto path = pagx::ToTGFX(*src.path);
    const float scale = src.renderScale();
    if (scale != 1.0f) {
      path.transform(tgfx::Matrix::MakeScale(scale));
    }
    auto pos = src.renderPosition();
    if (pos.x != 0.0f || pos.y != 0.0f) {
      path.transform(tgfx::Matrix::MakeTrans(pos.x, pos.y));
    }
    data->path = MakeProp(std::move(path));
  }
  // `baselineOrigin` is also subject to layout; LayerBuilder uses the
  // renderBaselineOrigin() variant.
  auto baseline = src.renderBaselineOrigin();
  data->baselineOrigin = MakeProp(tgfx::Point{baseline.x, baseline.y});
  data->baselineAngle = MakeProp(src.baselineAngle);
  data->firstMargin = MakeProp(src.firstMargin);
  data->lastMargin = MakeProp(src.lastMargin);
  data->perpendicular = src.perpendicular;
  data->reversed = src.reversed;
  data->forceAlignment = src.forceAlignment;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::TextPath;
  el->payload = std::move(data);
  return el;
}

std::unique_ptr<VectorElement> TextBaker::BakeTextModifier(BakeContext& /*ctx*/,
                                                           const pagx::TextModifier& src) {
  auto data = std::make_unique<ElementTextModifierData>();
  data->anchor = MakeProp(tgfx::Point{src.anchor.x, src.anchor.y});
  data->position = MakeProp(tgfx::Point{src.position.x, src.position.y});
  data->rotation = MakeProp(src.rotation);
  data->scale = MakeProp(tgfx::Point{src.scale.x, src.scale.y});
  data->skew = MakeProp(src.skew);
  data->skewAxis = MakeProp(src.skewAxis);
  data->alpha = MakeProp(src.alpha);

  if (src.fillColor.has_value()) {
    data->hasFillColor = true;
    data->fillColor = MakeProp(ToTgfxColor(*src.fillColor));
  }
  if (src.strokeColor.has_value()) {
    data->hasStrokeColor = true;
    data->strokeColor = MakeProp(ToTgfxColor(*src.strokeColor));
  }
  if (src.strokeWidth.has_value()) {
    data->hasStrokeWidth = true;
    data->strokeWidth = MakeProp(*src.strokeWidth);
  }

  // RangeSelector baking (structure-only for Phase 8 — TextSelector has no
  // concrete subtypes exposed in the PAGX public header today). We emit an
  // empty rangeSelectors vector; Phase 10+ will plug in the real selector
  // dispatch once pagx::TextRangeSelector / TextExpressionSelector land.
  (void)src.selectors;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::TextModifier;
  el->payload = std::move(data);
  return el;
}

}  // namespace pagx::pag
