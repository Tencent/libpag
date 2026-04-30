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
  // and let decomposeXform turn the image rect into <a:xfrm>. Per-glyph skew
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
        decomposeXform(0.0f, 0.0f, static_cast<float>(imgW), static_cast<float>(imgH), combined);
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

  auto xf = decomposeXform(minX, minY, boundsW, boundsH, m);
  int64_t pw = std::max(int64_t(1), PxToEMU(boundsW * sx));
  int64_t ph = std::max(int64_t(1), PxToEMU(boundsH * sy));
  float scaledOfsX = minX * sx;
  float scaledOfsY = minY * sy;

  if (!_bridgeContours || allContours.size() <= 1) {
    beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    WriteContourGeom(out, allContours, pw, ph, sx, sy, scaledOfsX, scaledOfsY, FillRule::EvenOdd);
    writeGlyphShape(out, fs.fill, alpha);
    return;
  }

  // One <p:sp> per bridged group so each carries its own inline bounds marker.
  // InlineSegments is safe here because glyph shapes never stroke.
  auto groups = PrepareContourGroups(allContours, FillRule::EvenOdd);
  for (const auto& group : groups) {
    beginShape(out, "Glyph", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    EmitGroupCustGeom(out, allContours, group, pw, ph, sx, sy, scaledOfsX, scaledOfsY,
                      BoundsMarkerStyle::InlineSegments);
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
    geom.estHeight = (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : text->fontSize * 1.4f;
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
  // layout would otherwise apply.
  geom.estWidth = static_cast<float>(CountUTF8Characters(text->text)) * text->fontSize * 0.6f;
  geom.estHeight = text->fontSize * 1.4f;
  geom.posX = text->renderPosition().x;
  geom.posY = text->renderPosition().y - text->fontSize * 0.85f;
  if (text->textAnchor == TextAnchor::Center) {
    geom.posX -= geom.estWidth / 2.0f;
  } else if (text->textAnchor == TextAnchor::End) {
    geom.posX -= geom.estWidth;
  }
  return geom;
}

void PPTWriter::emitNativeTextShapeFrame(XMLBuilder& out, const Matrix& m,
                                         const NativeTextGeometry& geom, const TextBox* textBox,
                                         bool useLineLayout) {
  auto xf = decomposeXform(geom.posX, geom.posY, geom.estWidth, geom.estHeight, m);
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
  // rather than the text itself; emit them inside a:rPr below so the shadow follows
  // the actual glyph outlines.
  out.closeElement();  // p:spPr

  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area (our PAGX-determined visual lines should fit, so PPT
  // shouldn't introduce additional wraps).
  bool justifyAlign = textBox && textBox->textAlign == TextAlign::Justify;
  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr")
      .addRequiredAttribute("wrap", useLineLayout ? (justifyAlign ? "square" : "none")
                                                  : (geom.hasTextBox ? "square" : "none"))
      .addRequiredAttribute("lIns", "0")
      .addRequiredAttribute("tIns", "0")
      .addRequiredAttribute("rIns", "0")
      .addRequiredAttribute("bIns", "0");
  AddBodyPrAttrsForTextBox(out, textBox);
  out.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
}

void PPTWriter::emitNativeTextBody(XMLBuilder& out, const Text* text,
                                   const std::vector<TextLayoutLineInfo>* lines,
                                   const PPTRunStyle& style, int64_t lnSpcPts, bool useLineLayout,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles) {
  if (useLineLayout) {
    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks. OOXML's algn="just" never justifies the last line of a
    // paragraph, so isolating each line in its own paragraph would disable
    // justify entirely; soft breaks within one paragraph make all lines
    // except the semantically last participate in justify.
    bool wroteAny = false;
    out.openElement("a:p").closeElementStart();
    WriteParagraphProperties(out, style.algn, lnSpcPts);
    for (const auto& lineInfo : *lines) {
      if (lineInfo.byteStart >= lineInfo.byteEnd) {
        continue;
      }
      std::string line =
          text->text.substr(lineInfo.byteStart, lineInfo.byteEnd - lineInfo.byteStart);
      if (line.empty()) {
        continue;
      }
      if (wroteAny) {
        writeLineBreak(out, style);
      }
      writeParagraphRun(out, line, style, filters, styles);
      wroteAny = true;
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
    writeParagraph(out, line, style, filters, styles, lnSpcPts);
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

  PPTRunStyle style = BuildRunStyle(text, fs.fill, fs.stroke, alpha);
  if (text->textAnchor == TextAnchor::Center) {
    style.algn = "ctr";
  } else if (text->textAnchor == TextAnchor::End) {
    style.algn = "r";
  }
  if (fs.textBox) {
    if (fs.textBox->textAlign == TextAlign::Center) {
      style.algn = "ctr";
    } else if (fs.textBox->textAlign == TextAlign::End) {
      style.algn = "r";
    } else if (fs.textBox->textAlign == TextAlign::Justify) {
      style.algn = "just";
    }
  }
  // In vertical writing mode, PAGX lineHeight controls column width (the
  // block-axis dimension), not character-to-character spacing. PPT's lnSpc
  // always controls the inline-axis spacing regardless of vert orientation, so
  // emitting it for vertical text would incorrectly stretch the character pitch.
  bool isVertical = fs.textBox && fs.textBox->writingMode == WritingMode::Vertical;
  int64_t lnSpcPts = (!isVertical && fs.textBox) ? LineHeightToSpcPts(fs.textBox->lineHeight) : 0;

  emitNativeTextBody(out, text, lines, style, lnSpcPts, useLineLayout, filters, styles);

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
    out.openElement("a:latin")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.openElement("a:ea")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    // a:cs covers complex scripts (Arabic, Hebrew, Thai, etc.). Without it
    // those glyphs fall back to the theme font even when the run ships with
    // the correct typeface on a:latin/a:ea.
    out.openElement("a:cs")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
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
    out.openElement("a:latin")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.openElement("a:ea")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.openElement("a:cs")
        .addRequiredAttribute("typeface", style.typeface)
        .closeElementSelfClosing();
    out.closeElement();  // a:rPr
  } else {
    out.closeElementSelfClosing();
  }
  out.closeElement();  // a:br
}

void PPTWriter::writeParagraph(XMLBuilder& out, const std::string& lineText,
                               const PPTRunStyle& style, const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles, int64_t lnSpcPts) {
  out.openElement("a:p").closeElementStart();
  WriteParagraphProperties(out, style.algn, lnSpcPts);
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
  WriteParagraphProperties(out, algn, lnSpcPts);
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
  auto xf = decomposeXform(0.0f, 0.0f, estWidth, estHeight, transform);

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
  out.closeElement();  // p:spPr

  // Justify alignment requires PowerPoint to know a target line width; with
  // wrap="none" the text is unbounded so PPT silently falls back to start
  // alignment. Use wrap="square" in that case so PPT can justify within the
  // shape's text area. Our PAGX-determined visual lines should already fit
  // within the shape, so PPT shouldn't introduce additional wraps.
  bool justifyAlign = box->textAlign == TextAlign::Justify;
  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr")
      .addRequiredAttribute("wrap", useLineLayout ? (justifyAlign ? "square" : "none")
                                                  : (hasBoxWidth ? "square" : "none"))
      .addRequiredAttribute("lIns", "0")
      .addRequiredAttribute("tIns", "0")
      .addRequiredAttribute("rIns", "0")
      .addRequiredAttribute("bIns", "0");
  AddBodyPrAttrsForTextBox(out, box);
  out.closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
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
    // Emit ALL visual lines inside a single <a:p> separated by soft <a:br/>
    // breaks (rather than one <a:p> per line). This is required for justify
    // alignment to behave: OOXML's algn="just" never justifies the last line
    // of a paragraph, so isolating each visual line in its own paragraph
    // turns every line into a "last line" and disables justify entirely.
    // Soft breaks within a single paragraph make all lines except the
    // semantically last one participate in justify.
    emitter.openParagraph();
    bool firstLine = true;
    size_t i = 0;
    while (i < lineEntries.size()) {
      float baseline = lineEntries[i].baselineY;
      if (!firstLine) {
        emitter.emitLineBreak(runStyles[lineEntries[i].runIndex]);
      }
      firstLine = false;
      while (i < lineEntries.size() &&
             std::fabs(lineEntries[i].baselineY - baseline) < baselineEpsilon) {
        const auto& entry = lineEntries[i];
        const std::string& src = runs[entry.runIndex].text->text;
        emitter.emitRun(src.substr(entry.byteStart, entry.byteEnd - entry.byteStart),
                        runStyles[entry.runIndex]);
        ++i;
      }
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
               runs.front().text->fontSize * 0.6f;
  }
  float estHeight =
      (!std::isnan(boxHeight) && boxHeight > 0) ? boxHeight : layoutResult.bounds.height;
  if (estHeight <= 0) {
    estHeight = runs.front().text->fontSize * 1.4f;
  }

  // Per-Text line metadata produced by PAGX's own layout. Use these line
  // breaks as authoritative paragraph boundaries (rather than letting
  // PowerPoint re-wrap with its own font metrics, which doesn't match the
  // PAGX renderer) so the PPT output matches the PAGX rendering exactly.
  // `getTextLines` returns nullptr for layouts where line info isn't tracked
  // (notably vertical writing mode); those fall back to legacy '\n'-splitting
  // and PowerPoint-driven wrapping.
  std::vector<LineEntry> lineEntries;
  bool useLineLayout = box->writingMode != WritingMode::Vertical;
  if (useLineLayout) {
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
  }

  emitTextBoxShapeFrame(out, box, transform, estWidth, estHeight, useLineLayout, hasBoxWidth);

  const char* algn = nullptr;
  if (box->textAlign == TextAlign::Center) {
    algn = "ctr";
  } else if (box->textAlign == TextAlign::End) {
    algn = "r";
  } else if (box->textAlign == TextAlign::Justify) {
    algn = "just";
  }
  // In vertical writing mode, PAGX lineHeight controls column width, not
  // character-to-character spacing -- skip lnSpc emission (same logic as the
  // writeNativeText path).
  bool isVertical = box->writingMode == WritingMode::Vertical;
  int64_t lnSpcPts = isVertical ? 0 : LineHeightToSpcPts(box->lineHeight);

  // Build per-run styles up-front (font/size/bold/italic/color/typeface).
  // Alignment lives on a:pPr (not a:rPr) so we leave style.algn at nullptr here.
  std::vector<PPTRunStyle> runStyles;
  runStyles.reserve(runs.size());
  for (const auto& run : runs) {
    runStyles.push_back(BuildRunStyle(run.text, run.fill, run.stroke, alpha));
  }

  ParagraphEmitter emitter{this, out, algn, lnSpcPts, filters, styles};
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
