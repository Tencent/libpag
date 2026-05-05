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
#include "pagx/TextLayout.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/TypefaceKey.h"
#include "renderer/GlyphRunRenderer.h"
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

  // Layout-space origin + font size. The correct origin depends on whether
  // this Text is a standalone element or nested in a TextBox:
  //
  //   - Standalone (textBounds.x/y == 0): use renderPosition(), which may
  //     include a centering offset when the parent Layer has stretched the
  //     Text's layoutBounds beyond its intrinsic textBounds. Without this
  //     offset the text drifts off the visible area of samples where a
  //     fixed-size parent Layer expects the text to sit in the middle.
  //
  //   - TextBox child (textBounds.x/y != 0): use layoutOrigin() ==
  //     (textBounds.x, textBounds.y). The TextBox layout engine has already
  //     placed the Text on its line-box accounting for textAlign /
  //     paragraphAlign / lineHeight / padding / writing mode, and
  //     renderPosition() would cancel that contribution by subtracting
  //     textBounds.x/y.
  //
  // This is split-path rather than unconditional textBounds because the two
  // branches answer different questions — "where does the visible glyph
  // box sit in the layoutBounds rect?" (standalone) versus "where did the
  // TextBox layout engine drop me?" (TextBox child). Merging would either
  // lose standalone centering (causing complete_example / composition PSNR
  // regressions) or lose TextBox placement (causing rich_text / text_box
  // to render everything at the origin).
  const bool insideTextBox = (src.layoutOrigin().x != 0.0f || src.layoutOrigin().y != 0.0f);
  auto layoutOrigin = insideTextBox ? src.layoutOrigin() : src.renderPosition();

  // Baseline offset: runtime-shape produces a TextBlob whose glyphs sit at
  // y=baseline=0 (i.e. the visual top is above the drawing origin). PAGX's
  // layout engine already resolved the absolute baseline y for the first
  // glyph (in the same layout coordinate system as layoutOrigin), so carry
  // it through. Without this the Inflater would fall back to a
  // font-metrics approximation (ascent-only) that drifts ~16 px because
  // the layout engine uses the full line-box height instead of ascent.
  const float baselineY = src.firstBaselineY();

  // Standalone Text: the baseline is relative to renderPosition (which
  // already includes the centering offset), so we add it directly.
  // TextBox child: the baseline is in TextBox coordinates and already
  // absolute — we use it as-is for y, with layoutOrigin.x for the x offset.
  float positionY = insideTextBox ? baselineY : (layoutOrigin.y + baselineY);
  data->position = MakeProp(tgfx::Point{layoutOrigin.x, positionY});

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

  // Pre-shaped layout hint. The runtime-shape path cannot express PAGX's
  // multi-line / justify / vertical-writing geometry from a single
  // `position`, because a single Point anchors the whole TextBlob and the
  // Inflater has no paragraph layout engine (§10.1). When PAGX TextLayout
  // produced multi-line glyphs, per-glyph RSXforms (vertical writing mode
  // with rotated Latin runs), or any other layout decision a single
  // position cannot reproduce, we snapshot `glyphData->layoutRuns`
  // directly so the Inflater can replay it.
  //
  // Trigger: `HasLayoutThatRuntimeShapeCannotReproduce(runs)` — the gate is
  // semantic ("does runtime shape lose information here?") rather than
  // structural ("is this Text inside a TextBox?"). Structural gates
  // (`insideTextBox`) miss the edge case where a TextBox child Text starts
  // at the TextBox origin (e.g. text_box.pagx Box 1: horizontal justify
  // single-child Text, `layoutOrigin = (0, 0)` because the first glyph
  // sits at the TextBox origin). Semantic gates capture exactly the cases
  // we need:
  //   - multi-line (more than one distinct baseline y among glyphs)
  //   - vertical writing / RangeSelector (any non-empty xforms)
  // Single-line Text in either coordinate convention is correctly handled
  // by the runtime-shape fallback + firstBaselineY / layoutOrigin split,
  // so those are skipped regardless of TextBox membership — saves ~10-20 B
  // per glyph on .pag size without changing the render.
  //
  // Paragraphs wider than `kMaxHintGlyphs` are skipped as well and left to
  // runtime shape (which still renders something even if geometry drifts).
  // A blanket hint on a 10 000-glyph screenplay would hurt .pag size more
  // than it helps PSNR; Phase 16.7+ is the right place to address that
  // case with a proper paragraph layout port.
  constexpr size_t kMaxHintGlyphs = 500;
  const auto& runs = GlyphRunRenderer::GetLayoutRuns(&src);
  size_t totalGlyphs = 0;
  bool hasMultipleLines = false;
  bool hasXforms = false;
  float firstBaseline = 0.0f;
  bool firstBaselineSeen = false;
  for (const auto& run : runs) {
    totalGlyphs += run.glyphs.size();
    if (!run.xforms.empty()) {
      hasXforms = true;
    }
    for (const auto& p : run.positions) {
      if (!firstBaselineSeen) {
        firstBaseline = p.y;
        firstBaselineSeen = true;
      } else if (p.y != firstBaseline) {
        hasMultipleLines = true;
      }
    }
  }
  const bool needsShapedHint =
      (hasMultipleLines || hasXforms) && totalGlyphs > 0 && totalGlyphs <= kMaxHintGlyphs;
  if (needsShapedHint) {
    std::vector<ElementTextData::ShapedRun> shapedRuns;
    shapedRuns.reserve(runs.size());
    bool allRunsUsable = true;
    for (const auto& run : runs) {
      if (run.glyphs.empty() || run.positions.size() < run.glyphs.size()) {
        continue;
      }
      auto typeface = run.font.getTypeface();
      if (typeface == nullptr) {
        // One unresolvable run inside a TextBox would leave a gap in the
        // replayed glyph stream; better to drop the whole hint and let the
        // Inflater runtime-shape everything (matches the run-level
        // fallback policy on the Inflater side).
        allRunsUsable = false;
        break;
      }
      ElementTextData::ShapedRun out;
      out.glyphs = run.glyphs;
      // Positions are stored relative to `data->position` so both the
      // hint path and the runtime-shape fallback apply `setPosition` the
      // same way on the Inflater side. This also keeps the hint
      // independent of any downstream Layer matrices that move the
      // anchor point.
      out.positions.reserve(run.positions.size());
      for (const auto& p : run.positions) {
        out.positions.push_back(tgfx::Point{p.x - layoutOrigin.x, p.y - positionY});
      }
      if (!run.xforms.empty() && run.xforms.size() >= run.glyphs.size()) {
        out.xforms.reserve(run.xforms.size());
        for (const auto& xf : run.xforms) {
          // RSXform's rotation/scale are orientation-only; only (tx, ty)
          // need translating into position-relative space.
          tgfx::RSXform adj = xf;
          adj.tx = xf.tx - layoutOrigin.x;
          adj.ty = xf.ty - positionY;
          out.xforms.push_back(adj);
        }
      }
      out.fontSize = run.font.getSize();
      out.typefaceFamily = typeface->fontFamily();
      out.typefaceStyle = typeface->fontStyle();
      out.typefaceKey = MakeTypefaceKey(*typeface);
      shapedRuns.push_back(std::move(out));
    }
    if (allRunsUsable && !shapedRuns.empty()) {
      data->shapedRuns = std::move(shapedRuns);
    }
  }

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
