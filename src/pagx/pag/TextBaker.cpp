// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 17 TextBaker — case A / case B branching.
//
// PAGX exposes two Text rendering modes; Phase 17 (v2.23) gives each one a
// dedicated branch in PAG v2 so the Inflater can always replay the exact
// PAGX-native geometry without re-shaping at load time.
//
//   1. Case A — author <GlyphRun> referencing embedded <Font>: glyph IDs
//      and positions are author-resolved; the referenced path-based font
//      is interned into PAGDocument.embeddedFonts and the run snapshot
//      lands in ElementTextData::glyphRuns. The Inflater rebuilds the
//      glyph paths via tgfx::PathTypefaceBuilder (see inflateTextAsPath).
//
//   2. Case B — runtime-shape Text (no <GlyphRun> children): the raw
//      UTF-8 string + fontFamily/fontStyle/fontSize is shaped by PAGX
//      TextLayout at applyLayout() time; we snapshot the resulting
//      glyphData->layoutRuns into ElementTextData::shapedGlyphs. The
//      Inflater resolves each run's typeface via FontProvider and replays
//      the glyph stream — no second-pass shaping (see
//      inflateTextAsShapedTextBlob). Glyph paths are NOT embedded; the
//      host is expected to have the family installed.
//
// Authoritative spec: docs/pagx_to_pag_v2_design.md §10.
#include "pagx/pag/TextBaker.h"
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "pagx/TextLayout.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/TypefaceKey.h"
#include "renderer/GlyphRunRenderer.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Path.h"

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

// ---- Phase 17 case A EmbeddedFont interning ----
//
// Content-hash-based intern: two PAGX <Font> nodes that carry the same
// unitsPerEm and the same glyph table collapse to one entry in
// `doc.embeddedFonts`. Hash key = unitsPerEm + per-glyph (advance,
// path verbs, path points) serialized into a byte buffer, then
// std::hash<std::string>. Collisions are tolerated because the map's
// value (uint32_t index) is never the discriminator — we resolve by
// direct content equality would be overkill for single-Bake maps.
//
// Why content hash (not pagx::Font* pointer): design decision confirmed
// Phase 17 — structurally identical fonts emitted by different authoring
// passes should dedup, and the hash cost (O(glyphCount * verbCount)) is
// paid once per font during Bake, not per GlyphRun.

std::string ComputeEmbeddedFontHash(const pagx::Font& font) {
  std::string buf;
  buf.reserve(64 + static_cast<size_t>(font.glyphs.size()) * 16);
  auto appendRaw = [&](const void* p, size_t n) { buf.append(static_cast<const char*>(p), n); };
  uint32_t unitsPerEm = static_cast<uint32_t>(font.unitsPerEm);
  appendRaw(&unitsPerEm, sizeof(unitsPerEm));
  uint32_t glyphCount = static_cast<uint32_t>(font.glyphs.size());
  appendRaw(&glyphCount, sizeof(glyphCount));
  for (const auto* g : font.glyphs) {
    if (g == nullptr || g->path == nullptr) {
      uint32_t sentinel = 0;
      appendRaw(&sentinel, sizeof(sentinel));
      continue;
    }
    appendRaw(&g->advance, sizeof(g->advance));
    const auto& verbs = g->path->verbs();
    const auto& points = g->path->points();
    uint32_t vCount = static_cast<uint32_t>(verbs.size());
    uint32_t pCount = static_cast<uint32_t>(points.size());
    appendRaw(&vCount, sizeof(vCount));
    appendRaw(&pCount, sizeof(pCount));
    if (!verbs.empty()) {
      appendRaw(verbs.data(), verbs.size() * sizeof(pagx::PathVerb));
    }
    if (!points.empty()) {
      appendRaw(points.data(), points.size() * sizeof(pagx::Point));
    }
  }
  return buf;
}

// Interns a PAGX Font into the PAG document's embeddedFonts table.
// Returns the index into doc.embeddedFonts. Safe against null font or
// null glyph entries — missing glyphs become empty EmbeddedGlyph slots
// so GlyphID indexing stays aligned with the PAGX glyph vector.
uint32_t InternEmbeddedFont(BakeContext& ctx, PAGDocument& doc, const pagx::Font& src) {
  auto key = ComputeEmbeddedFontHash(src);
  auto it = ctx.embeddedFontIndexByHash.find(key);
  if (it != ctx.embeddedFontIndexByHash.end()) {
    return it->second;
  }
  auto font = std::make_unique<EmbeddedFont>();
  font->unitsPerEm = static_cast<uint32_t>(src.unitsPerEm);
  font->glyphs.reserve(src.glyphs.size());
  for (const auto* g : src.glyphs) {
    EmbeddedGlyph out;
    if (g != nullptr) {
      out.advance = g->advance;
      if (g->path != nullptr) {
        out.path = pagx::ToTGFX(*g->path);
      }
    }
    font->glyphs.push_back(std::move(out));
  }
  uint32_t index = static_cast<uint32_t>(doc.embeddedFonts.size());
  doc.embeddedFonts.push_back(std::move(font));
  ctx.embeddedFontIndexByHash.emplace(std::move(key), index);
  return index;
}

// Fills `out` from a PAGX GlyphRun, verbatim (case A stores author
// coordinates as Text-local absolute positions; see design doc §10.8).
// Phase 18-reserved per-glyph fields (anchors / scales / rotations /
// skews) are carried through so a future Phase 18 payload reader can
// consume them without a schema bump — Baker writes them now, Inflater
// ignores them in Phase 17.
void FillGlyphRunData(const pagx::GlyphRun& src, uint32_t embeddedFontIndex,
                      ElementTextData::GlyphRunData* out) {
  out->embeddedFontIndex = embeddedFontIndex;
  out->fontSize = src.fontSize;
  out->glyphs = src.glyphs;
  out->x = src.x;
  out->y = src.y;
  out->xOffsets = src.xOffsets;
  out->positions.reserve(src.positions.size());
  for (const auto& p : src.positions) {
    out->positions.push_back(tgfx::Point{p.x, p.y});
  }
  out->anchors.reserve(src.anchors.size());
  for (const auto& p : src.anchors) {
    out->anchors.push_back(tgfx::Point{p.x, p.y});
  }
  out->scales.reserve(src.scales.size());
  for (const auto& p : src.scales) {
    out->scales.push_back(tgfx::Point{p.x, p.y});
  }
  out->rotations = src.rotations;
  out->skews = src.skews;
}

}  // namespace

std::unique_ptr<VectorElement> TextBaker::BakeText(BakeContext& ctx, PAGDocument& doc,
                                                   const pagx::Text& src) {
  auto data = std::make_unique<ElementTextData>();

  // ---- Phase 17 case A: author-authored GlyphRun with embedded <Font> ----
  //
  // When the PAGX source explicitly authored <GlyphRun> children referencing
  // a <Font id="..."> resource, the author has already resolved glyph IDs,
  // positions, and per-glyph xforms. We snapshot them verbatim into
  // `data->glyphRuns` and intern the referenced <Font> into
  // `doc.embeddedFonts`; the Inflater synthesizes glyph paths from that
  // table (`inflateTextAsPath`) without ever touching a typeface provider.
  //
  // Coordinate semantics (TextLayout.cpp §1591 path): `applyLayout()` does
  // not run for author-GlyphRun Text, so `Text.position` and
  // `Text.layoutOrigin()` are both unused — GlyphRun.x/y/xOffsets/positions
  // are already Text-local absolute coordinates. We therefore store
  // ElementTextData.position = (0, 0) and rely on GlyphRunData.x/y/... at
  // render time.
  //
  // Gate: at least one GlyphRun with a non-null Font. Runs that are all
  // null-Font fall through to case B (runtime shape) — that preserves the
  // Phase 16 behaviour for malformed PAGX.
  if (!src.glyphRuns.empty()) {
    bool anyRunHasFont = false;
    for (const auto* run : src.glyphRuns) {
      if (run != nullptr && run->font != nullptr) {
        anyRunHasFont = true;
        break;
      }
    }
    if (anyRunHasFont) {
      data->position = MakeProp(tgfx::Point{0.0f, 0.0f});
      data->text = src.text;
      data->fontFamily = src.fontFamily;
      data->fontStyle = src.fontStyle;
      data->fontSize = src.renderFontSize();
      data->direction = TextDirection::Default;
      data->justification = MapTextAnchor(src.textAnchor);
      data->tracking = src.letterSpacing;
      data->fauxBold = src.fauxBold;
      data->fauxItalic = src.fauxItalic;

      data->glyphRuns.reserve(src.glyphRuns.size());
      for (const auto* run : src.glyphRuns) {
        if (run == nullptr || run->font == nullptr) {
          // Skip malformed runs rather than emitting a placeholder — the
          // resulting stream is shorter by one run, but glyph indexing is
          // only meaningful within a single run so nothing downstream
          // breaks. Alternative (abort case A and downgrade to case B)
          // would lose every valid sibling run; that is a worse trade.
          continue;
        }
        uint32_t fontIndex = InternEmbeddedFont(ctx, doc, *run->font);
        ElementTextData::GlyphRunData out;
        FillGlyphRunData(*run, fontIndex, &out);
        data->glyphRuns.push_back(std::move(out));
      }

      auto el = std::make_unique<VectorElement>();
      el->type = VectorElementType::Text;
      el->payload = std::move(data);
      return el;
    }
  }

  // ---- Case B (Phase 17 v2.23): runtime-shape Text. ----
  //
  // Text without author <GlyphRun> children: PAGX TextLayout has shaped the
  // raw UTF-8 text into glyph IDs + positions + (optional) RSXforms. Snap-
  // shot every layoutRun verbatim into ElementTextData::shapedGlyphs and
  // bypass runtime-shape entirely on the Inflater side (Phase 17 Commit 3
  // moves shaped data from "opt-in hint" to "sole data source"). Glyph
  // shapes are NOT embedded — the Inflater resolves typefaces by family +
  // style strings via FontProvider; this is the by-design contract for the
  // host having a font installed (see design doc §10.8).

  // Render anchor (PathA: convertText() in LayerBuilder uses
  // src.renderPosition() directly as setPosition for the tgfx::Text
  // VectorElement). Mirror that exactly here so case B replays PathA's
  // blob-positioning math; the ElementTextData.position field becomes
  // the verbatim renderPosition.
  //
  // Why not (layoutOrigin.x, layoutOrigin.y + baselineY) like Phase 16.6?
  // Phase 16.6 added baselineY to compensate for runtime-shape's blob
  // origin sitting at glyph baseline=0 (the runtime shaper produces
  // glyphs at y=0 + advance, then setPosition shifts them down to the
  // visual baseline). Phase 17 case B does NOT runtime-shape: it replays
  // PAGX's already-baselined layoutRuns positions (positions[0].y =
  // baselineY by construction), so adding baselineY again would render
  // the glyphs one baseline below where PathA places them.
  //
  // shapedGlyphs.positions remain layout-absolute (no Baker-side
  // subtraction) because PathA's GlyphRunRenderer::BuildTextBlobFromLayoutRuns
  // consumes the same layout-absolute layoutRuns; matching the Baker
  // storage to PathA's input keeps PathA-vs-PathB PSNR aligned.
  auto renderPos = src.renderPosition();
  data->position = MakeProp(tgfx::Point{renderPos.x, renderPos.y});

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

  // Snapshot PAGX TextLayout output verbatim into ElementTextData::shapedGlyphs.
  // Each run carries its resolved typeface (family + style + 4-tuple key),
  // glyphs, layout-absolute positions, and optional RSXforms. The Inflater
  // applies setPosition(pay.position.value) to anchor the blob, mirroring
  // PathA's LayerBuilder::convertText math (renderPosition → setPosition).
  //
  // Edge case: PAGX TextLayout did not produce runs (Text never had
  // applyLayout() called, or empty/all-whitespace text). We still produce
  // an ElementTextData with empty shapedGlyphs; the Inflater drops empty
  // Texts silently — equivalent to Phase 16's empty-text branch.
  //
  // Run-level typeface resolution: every layoutRun has a non-null typeface
  // by construction (PAGX TextLayout would have skipped the run otherwise).
  // Defensive check: drop the entire shapedGlyphs payload if any run is
  // unresolvable, so the Inflater sees an empty case-B payload and drops
  // the Text rather than rendering a partial glyph stream with gaps.
  const auto& runs = GlyphRunRenderer::GetLayoutRuns(&src);
  std::vector<ElementTextData::ShapedGlyphRun> shapedGlyphs;
  shapedGlyphs.reserve(runs.size());
  bool allRunsUsable = true;
  for (const auto& run : runs) {
    if (run.glyphs.empty()) {
      continue;
    }
    auto typeface = run.font.getTypeface();
    if (typeface == nullptr) {
      allRunsUsable = false;
      break;
    }
    ElementTextData::ShapedGlyphRun out;
    out.typefaceFamily = typeface->fontFamily();
    out.typefaceStyle = typeface->fontStyle();
    out.typefaceKey = MakeTypefaceKey(*typeface);
    out.fontSize = run.font.getSize();
    out.glyphs = run.glyphs;
    out.positions.reserve(run.positions.size());
    for (const auto& p : run.positions) {
      // Layout-absolute coordinates — Inflater applies setPosition(renderPos)
      // to anchor the entire blob, mirroring PathA's convertText math.
      out.positions.push_back(tgfx::Point{p.x, p.y});
    }
    if (!run.xforms.empty() && run.xforms.size() >= run.glyphs.size()) {
      out.xforms.reserve(run.xforms.size());
      for (const auto& xf : run.xforms) {
        // RSXform tx/ty are also layout-absolute; preserved verbatim.
        out.xforms.push_back(xf);
      }
    }
    shapedGlyphs.push_back(std::move(out));
  }
  if (allRunsUsable && !shapedGlyphs.empty()) {
    data->shapedGlyphs = std::move(shapedGlyphs);
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
