/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/ppt/PPTFeatureProbe.h"
#include "pagx/ppt/PPTWriter.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyEqual;
using pag::FloatNearlyZero;
using pag::RadiansToDegrees;

namespace {

// Emits the run-level typeface triplet shared by writeParagraphRun and
// writeLineBreak. a:latin covers Latin scripts, a:ea handles East Asian
// (Chinese/Japanese/Korean) ranges, and a:cs covers complex scripts (Arabic,
// Hebrew, Thai, etc.) — without any of them PowerPoint silently falls back to
// the theme font for that script range even when the run carries the right
// typeface on a sibling element.
void WriteRunTypeface(XMLBuilder& out, const std::string& typeface) {
  out.openElement("a:latin").addRequiredAttribute("typeface", typeface).closeElementSelfClosing();
  out.openElement("a:ea").addRequiredAttribute("typeface", typeface).closeElementSelfClosing();
  out.openElement("a:cs").addRequiredAttribute("typeface", typeface).closeElementSelfClosing();
}

}  // namespace

void PPTWriter::writeTextAsPath(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& /*filters*/,
                                const std::vector<LayerStyle*>& /*styles*/) {
  auto renderPos = text->renderPosition();
  std::vector<GlyphPath> glyphPaths;
  std::vector<GlyphImage> glyphImages;
  ComputeGlyphPathsAndImages(*text, renderPos.x, renderPos.y, &glyphPaths, &glyphImages);
  if (glyphPaths.empty() && glyphImages.empty()) {
    return;
  }

  // ── Bitmap glyphs ─────────────────────────────────────────────────────────
  // Each bitmap glyph is emitted as its own p:pic; the glyph's image lives in
  // pixel coords, so we compose the per-glyph transform with the outer matrix
  // and let DecomposeXform turn the image rect into <a:xfrm>. Per-glyph skew
  // can't be expressed in OOXML and is silently dropped (matches the behaviour
  // for other unsupported transform components elsewhere in this exporter).
  for (const auto& gi : glyphImages) {
    if (!gi.image) {
      continue;
    }
    int imgW = 0;
    int imgH = 0;
    if (!GetImageDimensions(gi.image, &imgW, &imgH) || imgW <= 0 || imgH <= 0) {
      continue;
    }
    std::string relId = _ctx->addImage(gi.image);
    if (relId.empty()) {
      continue;
    }
    Matrix combined = m * gi.transform;
    auto xf =
        DecomposeXform(0.0f, 0.0f, static_cast<float>(imgW), static_cast<float>(imgH), combined);
    beginPicture(out, "GlyphImage");
    out.openElement("p:blipFill").closeElementStart();
    WriteBlip(out, relId, alpha);
    WriteDefaultStretch(out);
    out.closeElement();  // p:blipFill
    endPicture(out, xf);
  }

  if (glyphPaths.empty()) {
    return;
  }

  // Merge all glyph paths into a single compound shape to guarantee baseline
  // alignment.  Per-glyph shapes suffer from independent EMU rounding of offY
  // and extCY, which causes visible baseline jitter on some mobile renderers.
  std::vector<PathContour> allContours;
  for (const auto& gp : glyphPaths) {
    if (!gp.pathData || gp.pathData->isEmpty()) {
      continue;
    }
    AppendTransformedContours(allContours, gp.pathData, gp.transform);
  }

  if (allContours.empty()) {
    return;
  }

  // Compute bounding box using segment endpoints only (polygon approximation).
  // For cubic bezier segments the actual curve may exceed the endpoint hull, but
  // glyph outlines are dense enough that the deviation is negligible in practice.
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = std::numeric_limits<float>::lowest();
  for (const auto& contour : allContours) {
    ExpandBounds(minX, minY, maxX, maxY, contour.start);
    for (const auto& seg : contour.segs) {
      int n = PathData::PointsPerVerb(seg.verb);
      for (int i = 0; i < n; i++) {
        ExpandBounds(minX, minY, maxX, maxY, seg.pts[i]);
      }
    }
  }

  if (maxX <= minX || maxY <= minY) {
    return;
  }

  float boundsW = maxX - minX;
  float boundsH = maxY - minY;

  float sx = 0;
  float sy = 0;
  DecomposeScale(m, &sx, &sy);
  if (sx <= 0) sx = 1.0f;
  if (sy <= 0) sy = 1.0f;

  auto xf = DecomposeXform(minX, minY, boundsW, boundsH, m);
  int64_t pw = std::max(int64_t(1), PxToEMU(boundsW * sx));
  int64_t ph = std::max(int64_t(1), PxToEMU(boundsH * sy));
  float scaledOfsX = minX * sx;
  float scaledOfsY = minY * sy;

  // Always group glyph contours through PrepareContourGroups so that
  // AdjustWindingForEvenOdd normalises every glyph's outer/inner ring
  // orientation (depth-0 outers share the same sign, holes are reversed).
  // Skipping this step — as a previous fast-path did when _bridgeContours was
  // false — let two same-direction outers from neighbouring glyphs cancel
  // under even-odd whenever skew/rotation made them overlap, producing the
  // hollowed-out 'G' seen in the skews row of glyph_run_advanced.
  //
  // Each group (typically one per glyph) becomes its own <p:sp>: that gives
  // every glyph an isolated even-odd fill domain, so overlap between adjacent
  // glyphs cannot collapse one into a hole of another. _bridgeContours still
  // controls whether outer+inner rings inside a single group are stitched via
  // zero-width bridges or emitted as separate sub-paths in the same <a:path>.
  // InlineSegments is safe here because glyph shapes never stroke.
  auto groups = PrepareContourGroups(allContours, FillRule::EvenOdd);
  for (const auto& group : groups) {
    beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    EmitGroupCustGeom(out, allContours, group, pw, ph, sx, sy, scaledOfsX, scaledOfsY,
                      BoundsMarkerStyle::InlineSegments, _bridgeContours);
    writeGlyphShape(out, fs.fill, alpha);
  }
}

PPTWriter::NativeTextGeometry PPTWriter::computeNativeTextGeometry(
    const Text* text, Text* mutableText, const FillStrokeInfo& fs,
    const TextLayoutResult* precomputed) {
  NativeTextGeometry geom;
  float boxWidth = fs.textBox ? EffectiveTextBoxWidth(fs.textBox) : NAN;
  float boxHeight = fs.textBox ? EffectiveTextBoxHeight(fs.textBox) : NAN;
  geom.hasTextBox = fs.textBox && !std::isnan(boxWidth) && boxWidth > 0;

  if (geom.hasTextBox) {
    geom.posX = fs.textBox->renderPosition().x;
    geom.posY = fs.textBox->renderPosition().y;
    geom.estWidth = boxWidth;
    geom.estHeight =
        (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : text->renderFontSize() * 1.4f;
    return geom;
  }

  auto textBounds = precomputed->getTextBounds(mutableText);
  if (textBounds.width > 0 && textBounds.height > 0) {
    geom.posX = text->renderPosition().x + textBounds.x;
    geom.posY = text->renderPosition().y + textBounds.y;
    geom.estWidth = textBounds.width;
    geom.estHeight = textBounds.height;
    return geom;
  }

  // Fallback when layout produced no usable bounds (e.g. font metrics missing):
  // estimate from font size and adjust for the textAnchor that the missing
  // layout would otherwise apply. Use renderFontSize() so that a text shrunk
  // via textScale to fit dual-axis constraints still estimates correctly.
  float effectiveFontSize = text->renderFontSize();
  geom.estWidth = static_cast<float>(CountUTF8Characters(text->text)) * effectiveFontSize * 0.6f;
  geom.estHeight = effectiveFontSize * 1.4f;
  geom.posX = text->renderPosition().x;
  geom.posY = text->renderPosition().y - effectiveFontSize * 0.85f;
  if (text->textAnchor == TextAnchor::Center) {
    geom.posX -= geom.estWidth / 2.0f;
  } else if (text->textAnchor == TextAnchor::End) {
    geom.posX -= geom.estWidth;
  }
  return geom;
}

void PPTWriter::emitTextShapeEnvelope(XMLBuilder& out, const Xform& xf, const TextBox* textBox,
                                      const char* wrap) {
  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();
  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", "TextBox")
      .closeElementSelfClosing();
  out.openElement("p:cNvSpPr").addRequiredAttribute("txBox", "1").closeElementSelfClosing();
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvSpPr

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, xf);
  out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.openElement("a:ln").closeElementStart();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.closeElement();
  // Shadow effects on the surrounding p:spPr would apply to the text-box rectangle
  // rather than the text itself; callers emit them inside a:rPr below so the
  // shadow follows the actual glyph outlines.
  out.closeElement();  // p:spPr

  out.openElement("p:txBody").closeElementStart();
  // Map TextBox->padding onto <a:bodyPr> insets so PowerPoint lays out the
  // text inside the same inner rectangle that PAGX used during layout
  // (MakeTextBoxParams already shrinks the layout width/height by padding).
  // OOXML's default insets are non-zero, so we always emit explicit values
  // -- zeros for standalone text (no TextBox parent) and for TextBoxes with
  // zero padding, to preserve the pre-padding-support behavior.
  int64_t lIns = 0, tIns = 0, rIns = 0, bIns = 0;
  if (textBox) {
    lIns = PxToEMU(textBox->padding.left);
    tIns = PxToEMU(textBox->padding.top);
    rIns = PxToEMU(textBox->padding.right);
    bIns = PxToEMU(textBox->padding.bottom);
  }
  out.openElement("a:bodyPr")
      .addRequiredAttribute("wrap", wrap)
      .addRequiredAttribute("lIns", lIns)
      .addRequiredAttribute("tIns", tIns)
      .addRequiredAttribute("rIns", rIns)
      .addRequiredAttribute("bIns", bIns);
  AddBodyPrAttrsForTextBox(out, textBox);
  out.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
}

void PPTWriter::emitNativeTextShapeFrame(XMLBuilder& out, const Matrix& m,
                                         const NativeTextGeometry& geom, const TextBox* textBox,
                                         bool useLineLayout) {
  auto xf = DecomposeXform(geom.posX, geom.posY, geom.estWidth, geom.estHeight, m);
  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area (our PAGX-determined visual lines should fit, so PPT
  // shouldn't introduce additional wraps).
  bool justifyAlign = textBox && textBox->textAlign == TextAlign::Justify;
  const char* wrap =
      useLineLayout ? (justifyAlign ? "square" : "none") : (geom.hasTextBox ? "square" : "none");
  emitTextShapeEnvelope(out, xf, textBox, wrap);
}

void PPTWriter::emitNativeTextBody(XMLBuilder& out, const Text* text,
                                   const std::vector<TextLayoutLineInfo>* lines,
                                   const PPTRunStyle& style, int64_t lnSpcPts, bool rtl,
                                   bool useLineLayout, int64_t defTabSzEMU,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles) {
  if (useLineLayout) {
    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks. OOXML's algn="just" never justifies the last line of a
    // paragraph, so isolating each line in its own paragraph would disable
    // justify entirely; soft breaks within one paragraph make all lines
    // except the semantically last participate in justify.
    out.openElement("a:p").closeElementStart();
    WriteParagraphProperties(out, style.algn, lnSpcPts, rtl, defTabSzEMU);

    // TextLayout's per-Text line metadata only records lines that contributed
    // glyphs, so empty lines (created by consecutive '\n') don't appear in
    // `*lines`. Reconstruct them here by counting '\n' characters in the
    // source text between consecutive non-empty line entries: each '\n' is a
    // separate <a:br/>, so `Hg\n\nAB` produces two breaks (one empty middle
    // line) instead of collapsing into "Hg<br/>AB" (single break, no empty
    // line) which is what the previous filter-and-skip path emitted.
    bool wroteAny = false;
    size_t prevByteEnd = 0;
    for (const auto& lineInfo : *lines) {
      if (lineInfo.byteStart >= lineInfo.byteEnd) {
        continue;
      }
      if (wroteAny) {
        size_t emitted =
            writeNewlineBreaksInRange(out, text->text, prevByteEnd, lineInfo.byteStart, style);
        if (emitted == 0) {
          // Auto-wrap (no source '\n' between entries): still need one break
          // to move PowerPoint onto the next visual line.
          writeLineBreak(out, style);
        }
      } else {
        // Leading '\n's before the first non-empty line.
        writeNewlineBreaksInRange(out, text->text, 0, lineInfo.byteStart, style);
      }
      std::string line =
          text->text.substr(lineInfo.byteStart, lineInfo.byteEnd - lineInfo.byteStart);
      if (!line.empty()) {
        writeParagraphRun(out, line, style, filters, styles);
      }
      prevByteEnd = lineInfo.byteEnd;
      wroteAny = true;
    }
    if (wroteAny) {
      // Trailing '\n's after the last non-empty line: each becomes an empty
      // bottom line, matching the source text shape.
      writeNewlineBreaksInRange(out, text->text, prevByteEnd, text->text.size(), style);
    } else {
      // Text consists solely of '\n's (no non-empty lines were emitted): walk
      // the entire string so every '\n' still produces an empty line in the
      // output paragraph.
      writeNewlineBreaksInRange(out, text->text, 0, text->text.size(), style);
    }
    out.closeElement();  // a:p
    return;
  }

  const std::string& remaining = text->text;
  size_t pos = 0;
  while (pos <= remaining.size()) {
    size_t nl = remaining.find('\n', pos);
    std::string line =
        (nl == std::string::npos) ? remaining.substr(pos) : remaining.substr(pos, nl - pos);
    writeParagraph(out, line, style, filters, styles, lnSpcPts, rtl, defTabSzEMU);
    if (nl == std::string::npos) {
      break;
    }
    pos = nl + 1;
  }
}

void PPTWriter::writeNativeText(XMLBuilder& out, const Text* text, const FillStrokeInfo& fs,
                                const Matrix& m, float alpha,
                                const std::vector<LayerFilter*>& filters,
                                const std::vector<LayerStyle*>& styles,
                                const TextLayoutResult* precomputed) {
  if (text->text.empty()) {
    return;
  }

  auto* mutableText = const_cast<Text*>(text);
  float boxWidth = fs.textBox ? EffectiveTextBoxWidth(fs.textBox) : NAN;
  bool hasTextBox = fs.textBox && !std::isnan(boxWidth) && boxWidth > 0;

  TextLayoutResult localResult;
  if (!precomputed) {
    auto params = hasTextBox ? MakeTextBoxParams(fs.textBox) : MakeStandaloneParams(text);
    localResult = TextLayout::Layout({mutableText}, params, _layoutContext);
    precomputed = &localResult;
  }
  auto* lines = precomputed->getTextLines(mutableText);

  auto geom = computeNativeTextGeometry(text, mutableText, fs, precomputed);

  // When PAGX layout produced explicit per-line byte ranges we emit them as
  // soft <a:br/> breaks within a single paragraph ourselves, so PowerPoint
  // shouldn't perform additional line-wrapping on top (its font metrics differ
  // slightly from PAGX's, which would shift the break points). Vertical layout
  // has no line info; fall back to PowerPoint-driven wrapping in that case.
  bool useLineLayout = (lines != nullptr) && !lines->empty();

  emitNativeTextShapeFrame(out, m, geom, fs.textBox, useLineLayout);

  // Paragraph base direction by UBA P2/P3. Emitted via pPr@rtl so PowerPoint
  // runs BiDi with the correct base direction and reproduces the same visual
  // ordering as the PAGX renderer (which uses ICU with the same rule). Vertical
  // writing mode ignores rtl per OOXML, so there's no harm in computing it in
  // all cases.
  bool rtl = HasRTLParagraphBase(text->text);

  PPTRunStyle style = BuildRunStyle(text, fs.fill, fs.stroke, alpha);
  // TextAnchor / TextAlign use logical "Start"/"End" names; resolve them to
  // physical left/right based on the paragraph base direction so that an RTL
  // paragraph aligns to the right edge of its frame for Start (the default) and
  // to the left for End. Center / Justify are direction-symmetric.
  if (text->textAnchor == TextAnchor::Center) {
    style.algn = "ctr";
  } else if (text->textAnchor == TextAnchor::Start) {
    style.algn = rtl ? "r" : nullptr;
  } else if (text->textAnchor == TextAnchor::End) {
    style.algn = rtl ? "l" : "r";
  }
  if (fs.textBox) {
    if (fs.textBox->textAlign == TextAlign::Center) {
      style.algn = "ctr";
    } else if (fs.textBox->textAlign == TextAlign::Start) {
      style.algn = rtl ? "r" : nullptr;
    } else if (fs.textBox->textAlign == TextAlign::End) {
      style.algn = rtl ? "l" : "r";
    } else if (fs.textBox->textAlign == TextAlign::Justify) {
      style.algn = "just";
    }
  }
  // PAGX lineHeight maps onto OOXML <a:lnSpc> in both writing modes: in horizontal mode each
  // "line" is a row so lnSpc is the row pitch (vertical advance), in eaVert each "line" is a
  // column so lnSpc is the column pitch (horizontal advance). The earlier code skipped this in
  // vertical mode under the assumption that lnSpc only ever drives the inline axis, which left
  // PowerPoint to fall back to its default ~1.2x font-size column width regardless of the
  // PAGX-authored lineHeight.
  bool isVertical = fs.textBox && fs.textBox->writingMode == WritingMode::Vertical;
  int64_t lnSpcPts = fs.textBox ? LineHeightToSpcPts(fs.textBox->lineHeight) : 0;

  // Vertical writing mode ignores BiDi base direction; suppress the rtl
  // attribute so PowerPoint doesn't emit a spurious warning for eaVert text.
  bool emitRtl = rtl && !isVertical;

  // PAGX advances `\t` to the next multiple of `4 * fontSize` pixels; emit
  // the matching <a:pPr defTabSz="..."> so PowerPoint reproduces the same tab
  // stops instead of the master's 1-inch fallback. Skip the override entirely
  // when the source has no tab characters: the default is already correct
  // and the extra attribute would just add XML noise.
  int64_t defTabSzEMU =
      (text->text.find('\t') != std::string::npos) ? PagxTabSizeEMU(text->renderFontSize()) : 0;

  emitNativeTextBody(out, text, lines, style, lnSpcPts, emitRtl, useLineLayout, defTabSzEMU,
                     filters, styles);

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

void PPTWriter::writeParagraphRun(XMLBuilder& out, const std::string& runText,
                                  const PPTRunStyle& style,
                                  const std::vector<LayerFilter*>& filters,
                                  const std::vector<LayerStyle*>& styles) {
  out.openElement("a:r").closeElementStart();
  out.openElement("a:rPr")
      .addRequiredAttribute("lang", DetectTextLang(runText))
      .addRequiredAttribute("sz", style.fontSize);
  if (style.hasBold) {
    out.addRequiredAttribute("b", "1");
  }
  if (style.hasItalic) {
    out.addRequiredAttribute("i", "1");
  }
  if (style.hasLetterSpacing) {
    out.addRequiredAttribute("spc", style.letterSpc);
  }
  out.closeElementStart();
  // OOXML rPr child order is: a:ln, then EG_FillProperties, then a:effectLst,
  // then a:latin/a:ea. Emit the stroke first so the run carries both an
  // outline and a fill in a single text shape — this matches the PAGX
  // painter sequence (Fill then Stroke) without requiring a stacked second
  // shape that would otherwise cover the gradient text in renderers that do
  // not honour <a:noFill/> inside an rPr (notably LibreOffice).
  if (style.stroke) {
    writeStroke(out, style.stroke, style.strokeAlpha);
  }
  if (style.hasFillColor) {
    // For gradient fills, shape bounds are unknown at the run level; pass an empty
    // rect so writeColorSource falls back to the gradient's own coordinate space.
    writeColorSource(out, style.fillColor, style.fillAlpha);
  } else if (style.stroke) {
    // Stroke-only run: emit a fully transparent solid fill so the underlying
    // text from a previous Fill painter (rendered in a separate p:sp at the
    // same coordinates) shows through. <a:noFill/> would be the spec-correct
    // way to express "no glyph fill", but LibreOffice renders it as the
    // default theme colour (black) which hides the gradient text below.
    writeSolidColorFill(out, Color{0.0f, 0.0f, 0.0f, 0.0f}, 1.0f);
  }
  writeEffects(out, filters, styles);
  if (!style.typeface.empty()) {
    WriteRunTypeface(out, style.typeface);
  }
  out.closeElement();  // a:rPr
  out.openElement("a:t").closeElementStart();
  out.addTextContent(runText);
  out.closeElement();  // a:t
  out.closeElement();  // a:r
}

void PPTWriter::writeLineBreak(XMLBuilder& out, const PPTRunStyle& style) {
  // <a:br><a:rPr sz="..."/></a:br> — the rPr controls the line-height of the
  // break so it matches the surrounding text size (PowerPoint otherwise uses
  // its default body font size, producing visibly mismatched line spacing).
  out.openElement("a:br").closeElementStart();
  out.openElement("a:rPr")
      .addRequiredAttribute("lang", "en-US")
      .addRequiredAttribute("sz", style.fontSize);
  if (style.hasBold) {
    out.addRequiredAttribute("b", "1");
  }
  if (style.hasItalic) {
    out.addRequiredAttribute("i", "1");
  }
  if (!style.typeface.empty()) {
    out.closeElementStart();
    WriteRunTypeface(out, style.typeface);
    out.closeElement();  // a:rPr
  } else {
    out.closeElementSelfClosing();
  }
  out.closeElement();  // a:br
}

size_t PPTWriter::writeNewlineBreaksInRange(XMLBuilder& out, const std::string& source,
                                            size_t start, size_t end, const PPTRunStyle& style) {
  size_t emitted = 0;
  if (end > source.size()) {
    end = source.size();
  }
  for (size_t pos = start; pos < end; ++pos) {
    if (source[pos] == '\n') {
      writeLineBreak(out, style);
      ++emitted;
    }
  }
  return emitted;
}

size_t PPTWriter::writeNewlineBreaksAcrossRuns(ParagraphEmitter& emitter,
                                               const std::vector<RichTextRun>& runs,
                                               const std::vector<PPTRunStyle>& runStyles,
                                               size_t startRun, size_t startByte, size_t endRun,
                                               size_t endByte) {
  size_t emitted = 0;
  for (size_t r = startRun; r <= endRun && r < runs.size(); ++r) {
    const std::string& text = runs[r].text->text;
    size_t s = (r == startRun) ? startByte : 0;
    size_t e = (r == endRun) ? endByte : text.size();
    if (e > text.size()) {
      e = text.size();
    }
    for (size_t pos = s; pos < e; ++pos) {
      if (text[pos] == '\n') {
        emitter.emitLineBreak(runStyles[r]);
        ++emitted;
      }
    }
  }
  return emitted;
}

void PPTWriter::writeParagraph(XMLBuilder& out, const std::string& lineText,
                               const PPTRunStyle& style, const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles, int64_t lnSpcPts, bool rtl,
                               int64_t defTabSzEMU) {
  out.openElement("a:p").closeElementStart();
  WriteParagraphProperties(out, style.algn, lnSpcPts, rtl, defTabSzEMU);
  writeParagraphRun(out, lineText, style, filters, styles);
  out.closeElement();  // a:p
}

// ── TextBox group ──────────────────────────────────────────────────────────

namespace {

// Stable-sort comparator for PPTWriter::LineEntry. Defined as a named function
// (not a lambda) per project convention.
bool CompareLineEntryByBaselineY(const PPTWriter::LineEntry& a, const PPTWriter::LineEntry& b) {
  return a.baselineY < b.baselineY;
}

// Walks the TextBox children in source order, pairing every Text with its
// nearest enclosing Fill/Stroke. Direct Text children inherit `parentFill`
// and `parentStroke`; Texts nested inside a Group use that Group's
// locally-collected Fill/Stroke (falling back to the parent's when the Group
// supplies none).
void CollectRichTextRuns(const std::vector<Element*>& elements, const Fill* parentFill,
                         const Stroke* parentStroke, std::vector<PPTWriter::RichTextRun>& outRuns) {
  for (auto* element : elements) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto* t = static_cast<const Text*>(element);
      if (!t->text.empty()) {
        outRuns.push_back({t, parentFill, parentStroke});
      }
    } else if (type == NodeType::Group) {
      auto* g = static_cast<const Group*>(element);
      auto groupFs = CollectFillStroke(g->elements);
      const Fill* effectiveFill = groupFs.fill ? groupFs.fill : parentFill;
      const Stroke* effectiveStroke = groupFs.stroke ? groupFs.stroke : parentStroke;
      CollectRichTextRuns(g->elements, effectiveFill, effectiveStroke, outRuns);
    }
  }
}

}  // namespace

void PPTWriter::ParagraphEmitter::writePPr() {
  WriteParagraphProperties(out, algn, lnSpcPts, rtl, defTabSzEMU);
}

void PPTWriter::ParagraphEmitter::openParagraph() {
  out.openElement("a:p").closeElementStart();
  writePPr();
  paragraphOpen = true;
}

void PPTWriter::ParagraphEmitter::closeParagraph() {
  if (paragraphOpen) {
    out.closeElement();  // a:p
    paragraphOpen = false;
  }
}

void PPTWriter::ParagraphEmitter::emitRun(const std::string& fragment, const PPTRunStyle& style) {
  if (fragment.empty()) {
    return;
  }
  if (!paragraphOpen) {
    openParagraph();
  }
  writer->writeParagraphRun(out, fragment, style, filters, styles);
}

void PPTWriter::ParagraphEmitter::emitLineBreak(const PPTRunStyle& style) {
  writer->writeLineBreak(out, style);
}

void PPTWriter::emitTextBoxShapeFrame(XMLBuilder& out, const TextBox* box, const Matrix& transform,
                                      float estWidth, float estHeight, bool useLineLayout,
                                      bool hasBoxWidth) {
  // `transform` already incorporates BuildGroupMatrix(box) (applied by the
  // caller in writeElements), so the local origin here is (0, 0). Adding
  // box->position again would double-offset the shape, pushing the text-box
  // away from the centered position the layout assigned to it.
  //
  // Anchor mode: when the box is authored with width="0" (or height="0"), the
  // box position marks an *anchor point* and textAlign / paragraphAlign select
  // which side of the rendered text sits on it. The shape envelope is sized
  // to the actual text bounds (estWidth/estHeight), so to honour the anchor
  // we shift the shape's local origin instead of relying on PPT's in-shape
  // alignment (which would be a no-op when the shape is exactly text-sized).
  // EffectiveTextBoxWidth returns 0 only for the explicit "0" case (NaN means
  // "auto-resolve from layout bounds"), so testing the raw box->width keeps
  // the auto-measured-one-axis path (Box Q / Box R) on the default branch.
  bool widthAnchor = !std::isnan(box->width) && box->width == 0.0f;
  bool heightAnchor = !std::isnan(box->height) && box->height == 0.0f;
  bool isVertical = box->writingMode == WritingMode::Vertical;
  // OOXML's bodyPr@anchor (paragraphAlign) describes block-axis placement,
  // bodyPr child algn (textAlign) inline-axis. In horizontal writing the
  // inline axis is X and block axis is Y; in eaVert they swap.
  float anchorOffsetX = 0.0f;
  float anchorOffsetY = 0.0f;
  // Inline-axis anchor: textAlign decides which side of the text sits on the
  // anchor. In horizontal writing this drives X; in vertical writing it
  // drives the column's vertical flow (Y) instead.
  bool inlineAnchor = isVertical ? heightAnchor : widthAnchor;
  if (inlineAnchor) {
    if (box->textAlign == TextAlign::Center) {
      (isVertical ? anchorOffsetY : anchorOffsetX) = -(isVertical ? estHeight : estWidth) / 2.0f;
    } else if (box->textAlign == TextAlign::End) {
      (isVertical ? anchorOffsetY : anchorOffsetX) = -(isVertical ? estHeight : estWidth);
    }
    // Justify on a zero-extent box has no target width to distribute against
    // and falls back to Start, matching the PAGX renderer.
  }
  // Block-axis anchor: paragraphAlign decides top/centre/bottom (horizontal)
  // or right/centre/left of the column block (eaVert).
  bool blockAnchor = isVertical ? widthAnchor : heightAnchor;
  if (blockAnchor) {
    if (box->paragraphAlign == ParagraphAlign::Middle) {
      (isVertical ? anchorOffsetX : anchorOffsetY) = -(isVertical ? estWidth : estHeight) / 2.0f;
    } else if (box->paragraphAlign == ParagraphAlign::Far) {
      (isVertical ? anchorOffsetX : anchorOffsetY) = -(isVertical ? estWidth : estHeight);
    }
  }
  auto xf = DecomposeXform(anchorOffsetX, anchorOffsetY, estWidth, estHeight, transform);
  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area. Our PAGX-determined visual lines should already fit
  // within the shape, so PPT shouldn't introduce additional wraps.
  bool justifyAlign = box->textAlign == TextAlign::Justify;
  const char* wrap =
      useLineLayout ? (justifyAlign ? "square" : "none") : (hasBoxWidth ? "square" : "none");
  emitTextShapeEnvelope(out, xf, box, wrap);
}

void PPTWriter::emitTextBoxBody(const std::vector<RichTextRun>& runs,
                                const std::vector<PPTRunStyle>& runStyles,
                                std::vector<LineEntry>& lineEntries, bool useLineLayout,
                                ParagraphEmitter& emitter) {
  if (useLineLayout) {
    // Stable-sort entries by baselineY so that runs from different Text
    // elements sharing the same visual line stay in source order (rich text
    // flows left-to-right within a baseline). Then group consecutive entries
    // with matching baselineY into one visual line.
    std::stable_sort(lineEntries.begin(), lineEntries.end(), CompareLineEntryByBaselineY);
    constexpr float baselineEpsilon = 0.5f;

    // Group entries by baseline into visual lines. Each visual line carries
    // all runs that contribute glyphs to that baseline (for rich text on the
    // same line).
    std::vector<std::vector<size_t>> visualLines;
    {
      size_t i = 0;
      while (i < lineEntries.size()) {
        float baseline = lineEntries[i].baselineY;
        visualLines.emplace_back();
        while (i < lineEntries.size() &&
               std::fabs(lineEntries[i].baselineY - baseline) < baselineEpsilon) {
          visualLines.back().push_back(i);
          ++i;
        }
      }
    }

    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks (rather than one <a:p> per line). This is required for justify
    // alignment to behave: OOXML's algn="just" never justifies the last line
    // of a paragraph, so isolating each visual line in its own paragraph
    // turns every line into a "last line" and disables justify entirely.
    // Soft breaks within a single paragraph make all lines except the
    // semantically last one participate in justify.
    emitter.openParagraph();
    if (visualLines.empty()) {
      emitter.closeParagraph();
      return;
    }

    // Leading '\n' before the first non-empty visual line: each one becomes
    // an empty line at the top of the paragraph. The very first <a:br/>
    // moves the cursor off the implicit "line 0" of the paragraph; subsequent
    // ones add additional empty lines.
    const auto& firstLineEntries = visualLines.front();
    const auto& firstEntry = lineEntries[firstLineEntries.front()];
    writeNewlineBreaksAcrossRuns(emitter, runs, runStyles, 0, 0, firstEntry.runIndex,
                                 firstEntry.byteStart);

    for (size_t li = 0; li < visualLines.size(); ++li) {
      for (size_t entryIdx : visualLines[li]) {
        const auto& entry = lineEntries[entryIdx];
        const std::string& src = runs[entry.runIndex].text->text;
        emitter.emitRun(src.substr(entry.byteStart, entry.byteEnd - entry.byteStart),
                        runStyles[entry.runIndex]);
      }
      if (li + 1 < visualLines.size()) {
        const auto& lastEntry = lineEntries[visualLines[li].back()];
        const auto& nextFirstEntry = lineEntries[visualLines[li + 1].front()];
        size_t emitted = writeNewlineBreaksAcrossRuns(emitter, runs, runStyles, lastEntry.runIndex,
                                                      lastEntry.byteEnd, nextFirstEntry.runIndex,
                                                      nextFirstEntry.byteStart);
        if (emitted == 0) {
          // PAGX wrapped mid-text without a source '\n' (auto word-wrap): the
          // following line still needs a <a:br/> to start on a new line. Use
          // the next line's run style so the break height matches the line
          // it terminates into.
          emitter.emitLineBreak(runStyles[nextFirstEntry.runIndex]);
        }
      }
    }

    // Trailing '\n' after the last entry: each one produces an empty trailing
    // line, mirroring the leading-'\n' behaviour above.
    if (!runs.empty()) {
      const auto& lastEntry = lineEntries[visualLines.back().back()];
      writeNewlineBreaksAcrossRuns(emitter, runs, runStyles, lastEntry.runIndex, lastEntry.byteEnd,
                                   runs.size() - 1, runs.back().text->text.size());
    }

    emitter.closeParagraph();
    return;
  }

  // Fallback path: stream runs into paragraphs, splitting only on '\n'. A
  // single Text element may carry multiple newlines internally, in which
  // case it contributes to several consecutive paragraphs. PowerPoint
  // performs its own wrapping inside the shape if a paragraph exceeds the
  // box width.
  for (size_t i = 0; i < runs.size(); ++i) {
    const std::string& s = runs[i].text->text;
    const auto& rs = runStyles[i];
    size_t pos = 0;
    while (pos <= s.size()) {
      size_t nl = s.find('\n', pos);
      if (nl == std::string::npos) {
        emitter.emitRun(s.substr(pos), rs);
        break;
      }
      emitter.emitRun(s.substr(pos, nl - pos), rs);
      // Newline terminates the current paragraph; if the paragraph is still
      // empty (e.g. consecutive '\n's), open and immediately close it so the
      // blank line is preserved.
      if (!emitter.paragraphOpen) {
        emitter.openParagraph();
      }
      emitter.closeParagraph();
      pos = nl + 1;
    }
  }
  emitter.closeParagraph();
}

void PPTWriter::writeTextBoxGroup(XMLBuilder& out, const Group* textBox,
                                  const std::vector<Element*>& elements, const Matrix& transform,
                                  float alpha, const std::vector<LayerFilter*>& filters,
                                  const std::vector<LayerStyle*>& styles) {
  auto* box = static_cast<const TextBox*>(textBox);
  auto topLevelFs = CollectFillStroke(elements);

  std::vector<RichTextRun> runs;
  CollectRichTextRuns(elements, topLevelFs.fill, topLevelFs.stroke, runs);
  if (runs.empty()) {
    return;
  }

  // Run a layout pass to obtain the textbox's resolved bounds AND its
  // per-Text line-break decisions. We use those line breaks as paragraph
  // boundaries below so PowerPoint reproduces the same wrapping that PAGX
  // computed (PPT's font metrics differ slightly, so leaving wrapping to
  // PowerPoint produces visibly different break points).
  std::vector<Text*> mutableTexts;
  mutableTexts.reserve(runs.size());
  for (const auto& run : runs) {
    mutableTexts.push_back(const_cast<Text*>(run.text));
  }
  auto params = MakeTextBoxParams(box);
  auto layoutResult = TextLayout::Layout(mutableTexts, params, _layoutContext);

  float boxWidth = EffectiveTextBoxWidth(box);
  float boxHeight = EffectiveTextBoxHeight(box);
  bool hasBoxWidth = !std::isnan(boxWidth) && boxWidth > 0;
  float estWidth = hasBoxWidth ? boxWidth : layoutResult.bounds.width;
  if (estWidth <= 0) {
    estWidth = static_cast<float>(CountUTF8Characters(runs.front().text->text)) *
               runs.front().text->renderFontSize() * 0.6f;
  }
  float estHeight =
      (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : layoutResult.bounds.height;
  if (estHeight <= 0) {
    estHeight = runs.front().text->renderFontSize() * 1.4f;
  }

  // Per-Text line metadata produced by PAGX's own layout. Use these line breaks as authoritative
  // paragraph boundaries (rather than letting PowerPoint re-wrap with its own font metrics, which
  // doesn't match the PAGX renderer) so the PPT output matches the PAGX rendering exactly. In
  // vertical mode each entry is a column instead of a row (baselineY stores -columnX so the
  // ascending sort below maps to right-to-left source order); columns dropped by
  // overflow="hidden" are absent from the layout result and therefore omitted from the PPT output
  // automatically. `getTextLines` only returns nullptr for layouts where line info isn't tracked
  // (e.g. embedded glyph runs); those fall back to legacy '\n'-splitting.
  std::vector<LineEntry> lineEntries;
  bool useLineLayout = true;
  for (size_t i = 0; i < runs.size(); ++i) {
    auto* mt = const_cast<Text*>(runs[i].text);
    auto* lines = layoutResult.getTextLines(mt);
    if (lines == nullptr) {
      useLineLayout = false;
      lineEntries.clear();
      break;
    }
    for (const auto& li : *lines) {
      if (li.byteStart >= li.byteEnd) {
        continue;
      }
      lineEntries.push_back({i, li.baselineY, li.byteStart, li.byteEnd});
    }
  }
  if (useLineLayout && lineEntries.empty()) {
    useLineLayout = false;
  }

  emitTextBoxShapeFrame(out, box, transform, estWidth, estHeight, useLineLayout, hasBoxWidth);

  // Paragraph base direction by UBA P2/P3. A TextBox carries a single pPr so
  // all runs share one base direction; concatenating the run text in source
  // order lets HasRTLParagraphBase walk until it hits the first strong
  // directional character, matching the paragraph-level rule that PAGX's ICU
  // BiDi uses. Vertical writing mode ignores rtl per OOXML, so we suppress it
  // in that case to avoid emitting a no-op attribute.
  bool isVertical = box->writingMode == WritingMode::Vertical;
  bool rtl = false;
  if (!isVertical) {
    std::string combined;
    for (const auto& run : runs) {
      combined.append(run.text->text);
    }
    rtl = HasRTLParagraphBase(combined);
  }

  const char* algn = nullptr;
  if (box->textAlign == TextAlign::Center) {
    algn = "ctr";
  } else if (box->textAlign == TextAlign::Start) {
    // "Start" is the logical default: left in LTR, right in RTL. OOXML defaults
    // to "l" when algn is absent, so only emit an explicit "r" when rtl is on.
    algn = rtl ? "r" : nullptr;
  } else if (box->textAlign == TextAlign::End) {
    algn = rtl ? "l" : "r";
  } else if (box->textAlign == TextAlign::Justify) {
    algn = "just";
  }
  // PAGX lineHeight maps onto OOXML <a:lnSpc> in both writing modes (see writeNativeText for the
  // full rationale): horizontal lnSpc drives row pitch, eaVert lnSpc drives column pitch.
  int64_t lnSpcPts = LineHeightToSpcPts(box->lineHeight);

  // Build per-run styles up-front (font/size/bold/italic/color/typeface).
  // Alignment lives on a:pPr (not a:rPr) so we leave style.algn at nullptr here.
  std::vector<PPTRunStyle> runStyles;
  runStyles.reserve(runs.size());
  for (const auto& run : runs) {
    runStyles.push_back(BuildRunStyle(run.text, run.fill, run.stroke, alpha));
  }

  // Locate the first run whose text contains a `\t` and use its renderFontSize
  // to set the paragraph's defTabSz. PAGX shapes each Text with its own
  // `4 * fontSize` tab stop; OOXML can carry only a single defTabSz per
  // paragraph, so mixed-size rich text with tabs in multiple runs may still
  // drift slightly — but matching the first tab-bearing run keeps the common
  // single-style case exact. Skip the override entirely when no run has a
  // tab so the master's default value still applies (no XML noise).
  int64_t defTabSzEMU = 0;
  for (const auto& run : runs) {
    if (run.text->text.find('\t') != std::string::npos) {
      defTabSzEMU = PagxTabSizeEMU(run.text->renderFontSize());
      break;
    }
  }

  ParagraphEmitter emitter{this, out, algn, lnSpcPts, rtl, defTabSzEMU, filters, styles};
  emitTextBoxBody(runs, runStyles, lineEntries, useLineLayout, emitter);

  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

// ── Element / layer traversal ──────────────────────────────────────────────
//
// PAGX's vector-element model is "accumulate-render": each Layer maintains an
// implicit geometry list that grows as Rectangle / Ellipse / Path / Text
// elements are encountered in document order. A Painter (Fill or Stroke)
// renders every geometry currently in scope; geometry stays in the list, so
// subsequent painters can render it again (multi-fill / multi-stroke).
//
// Group is a *one-way* scope:
//   * Painters declared inside a Group only see geometry added inside that
//     same Group (the scope_start index in `processVectorScope` enforces this).
//   * Geometry added inside a Group propagates upward when the Group exits, so
//     painters at the outer scope can still render that geometry — this is how
//     `<Group><Rect/></Group><Fill/>` ends up filling the rect (the
//     "Propagation" case in samples/group.pagx).
//
// PowerPoint shapes only carry a single fill + single stroke, so each painter
// emits its own copy of every visible geometry (`emitGeometryWithFs`); shapes
// rendered later overlap earlier ones, matching PAGX's "later painter renders
// on top" rule. This is also what makes the multi-fill / multi-stroke samples
// render correctly.
//
// Layer is the accumulation boundary — geometry never crosses a Layer boundary,
// so the accumulator is created fresh per `writeElements` call.

}  // namespace pagx
