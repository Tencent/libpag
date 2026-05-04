// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 16 TextBaker — runtime-shape mode.
//
// PAGX exposes two Text rendering modes:
//   1. Pre-shaped: `text.glyphRuns` holds pagx::GlyphRun* entries with glyph
//      IDs, positions and per-glyph xforms. Useful for author-authored
//      per-glyph animation, but tied to a specific font + typeface identity
//      that isn't portable across platforms.
//   2. Runtime shaping: `text.glyphRuns` is empty. The Text carries a UTF-8
//      string + fontFamily/fontStyle/fontSize; the renderer shapes at draw
//      time.
//
// Phase 16 (v2.20) standardises on runtime shaping for the PAG v2 pipeline.
// Pre-shaped inputs are DOWNGRADED: glyph IDs + per-glyph xforms are
// discarded, the text + fontFamily/fontStyle are preserved, and a
// TextGlyphRunsDowngraded=208 warning is emitted so the call site can decide
// whether to regenerate the PAGX asset without pre-shaping.
//
// Authoritative spec: docs/pagx_to_pag_v2_design.md §10 +
// docs/pagx_to_pag_v2_phase16_text_redesign.md §4.
#include "pagx/pag/TextBaker.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "renderer/ToTGFX.h"

namespace pagx::pag {

namespace {

tgfx::Color ToTgfxColor(const pagx::Color& c) {
  return tgfx::Color{c.red, c.green, c.blue, c.alpha};
}

// PAGX Text.textAnchor → ElementTextData.justification. PAGX uses a simpler
// three-value anchor model (Start / Center / End); the v1 TextDocument
// paragraph justification extends that with FullJustify variants Baker
// doesn't currently emit.
ParagraphJustification MapTextAnchor(pagx::TextAnchor anchor) {
  switch (anchor) {
    case pagx::TextAnchor::Start:
      return ParagraphJustification::LeftJustify;
    case pagx::TextAnchor::Center:
      return ParagraphJustification::CenterJustify;
    case pagx::TextAnchor::End:
      return ParagraphJustification::RightJustify;
  }
  return ParagraphJustification::LeftJustify;
}

}  // namespace

std::unique_ptr<VectorElement> TextBaker::BakeText(BakeContext& ctx, PAGDocument& /*doc*/,
                                                   const pagx::Text& src) {
  auto data = std::make_unique<ElementTextData>();

  // Render-space position + font size. LayerBuilder::convertText uses
  // `renderPosition()` (= layoutBounds + center-of-textBounds offset) and
  // `renderFontSize()` (= fontSize * textScale) at draw time; mirror that
  // here so Path B (Baker -> Codec -> Inflater -> TextBlob::MakeFrom) matches
  // Path A (LayerBuilder direct). Using `src.position` / `src.fontSize`
  // loses the applyLayout() contribution and the text renders at the
  // origin with the unscaled font.
  auto renderPos = src.renderPosition();

  // Baseline offset: runtime-shape produces a TextBlob whose glyphs sit at
  // y=baseline=0 (i.e. the visual top is above the drawing origin). PAGX's
  // layout engine already resolved the absolute baseline y for the first
  // glyph; carry it through so the Inflater can place the baseline where
  // LayerBuilder's convertText would have placed it. Without this the
  // Inflater would fall back to a font-metrics approximation (ascent-only)
  // that drifts ~16 px because the layout engine uses the full line-box
  // height instead of ascent.
  const float baselineY = src.firstBaselineY();

  data->position = MakeProp(tgfx::Point{renderPos.x, renderPos.y + baselineY});

  // Content — the raw UTF-8 string runtime shapers re-shape at load time.
  data->text = src.text;
  data->fontFamily = src.fontFamily;
  data->fontStyle = src.fontStyle;
  data->fontSize = src.renderFontSize();

  // Paragraph layout. PAGX Text only exposes anchor + letterSpacing; the rest
  // default to v1 TextDocument neutrals. BoxText, leading, firstBaseLine and
  // baselineShift would be driven by a wrapping TextBox — the Baker currently
  // has no access to that context, so leave them at defaults and let the
  // Inflater's TextShaper fall back to font-metrics auto-values.
  data->direction = TextDirection::Default;
  data->justification = MapTextAnchor(src.textAnchor);
  data->tracking = src.letterSpacing;

  data->fauxBold = src.fauxBold;
  data->fauxItalic = src.fauxItalic;

  // Pre-shaped downgrade. PAGX-native per-glyph xform information is lost;
  // callers that genuinely need it should keep consuming the PAGX document
  // directly rather than round-tripping through PAG v2.
  if (!src.glyphRuns.empty()) {
    ctx.warn(ErrorCode::TextGlyphRunsDowngraded,
             "Pre-shaped pagx::Text.glyphRuns dropped; runtime-shape fallback used");

    // PAGX-exclusive per-glyph anchors are salvaged: the Inflater can align
    // the TextShaper output glyph-for-glyph against this vector for
    // TextModifier / RangeSelector paths (design doc §10.3).
    std::vector<tgfx::Point> allAnchors;
    for (const auto* run : src.glyphRuns) {
      if (run == nullptr) {
        continue;
      }
      for (const auto& a : run->anchors) {
        allAnchors.push_back(tgfx::Point{a.x, a.y});
      }
    }
    if (!allAnchors.empty()) {
      data->anchors = MakeProp(std::move(allAnchors));
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
    // `path.transform(MakeTrans(renderPosition))`. Mirror both here so path
    // coordinates on the wire match LayerBuilder's direct-render output.
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

  // RangeSelector baking (structure-only — TextSelector has no concrete
  // subtypes exposed in the PAGX public header today). Emit empty vector;
  // later Phases plug in the real selector dispatch when pagx::TextRangeSelector /
  // pagx::TextExpressionSelector land.
  (void)src.selectors;

  auto el = std::make_unique<VectorElement>();
  el->type = VectorElementType::TextModifier;
  el->payload = std::move(data);
  return el;
}

}  // namespace pagx::pag
