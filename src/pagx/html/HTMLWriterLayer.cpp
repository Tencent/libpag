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
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

namespace {

// Splits `text` into visible line segments based on where tgfx's text layout engine placed
// line breaks, so the emitted HTML matches PAGX line-for-line instead of relying on Chromium's
// word-wrap re-flow (which uses its own glyph advance measurements and often breaks one token
// earlier or later than tgfx when a word lands near the TextBox right edge).
//
// Only single-byte ASCII sources are handled here: HarfBuzz cluster information is not kept
// in TextLayoutGlyphRun, so we assume 1 glyph == 1 source byte and any multi-byte codepoint,
// ligature, or RTL text falls back to CSS word-wrap by returning false.
struct TgfxWrappedLine {
  size_t byteStart = 0;
  size_t byteEnd = 0;  // exclusive; trailing whitespace trimmed by tgfx is not included
};

bool TrySplitTextByTgfxLines(const std::string& text, const std::vector<size_t>& glyphsPerLine,
                             std::vector<TgfxWrappedLine>& outLines) {
  outLines.clear();
  if (text.empty() || glyphsPerLine.size() <= 1) {
    return false;
  }
  for (unsigned char c : text) {
    if (c >= 0x80) {
      return false;
    }
  }
  size_t pos = 0;
  size_t n = text.size();
  for (size_t li = 0; li < glyphsPerLine.size(); li++) {
    if (li > 0) {
      // Skip leading spaces that tgfx trimmed from the previous line end.
      while (pos < n && text[pos] == ' ') {
        pos++;
      }
    }
    size_t start = pos;
    size_t needed = glyphsPerLine[li];
    while (pos < n && needed > 0) {
      needed--;
      pos++;
    }
    if (needed > 0) {
      return false;
    }
    size_t end = pos;
    while (end > start && text[end - 1] == ' ') {
      end--;
    }
    outLines.push_back({start, end});
  }
  while (pos < n && text[pos] == ' ') {
    pos++;
  }
  if (pos != n) {
    return false;
  }
  return true;
}

}  // namespace

//==============================================================================
// HTMLWriter – element processing
//==============================================================================

void HTMLWriter::paintGeos(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, const TextBox* textBox, float alpha, bool hasTrim,
                           const TrimPath* curTrim, bool hasMerge, MergePathMode mergeMode) {
  if (geos.empty()) {
    return;
  }
  bool hasText = false;
  for (auto& g : geos) {
    if (g.type == NodeType::Text) {
      hasText = true;
      break;
    }
  }
  if (hasText) {
    for (auto& g : geos) {
      if (g.type == NodeType::Text) {
        writeText(out, static_cast<const Text*>(g.element), fill, stroke, textBox, alpha);
      } else {
        std::vector<GeoInfo> single = {g};
        renderGeo(out, single, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
      }
    }
  } else {
    renderGeo(out, geos, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
  }
}

void HTMLWriter::writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                               bool distribute, LayerPlacement targetPlacement) {
  std::vector<GeoInfo> geos = {};
  const Fill* curFill = nullptr;
  const Stroke* curStroke = nullptr;
  const TextBox* curTextBox = nullptr;
  bool hasTrim = false;
  const TrimPath* curTrim = nullptr;
  bool hasMerge = false;
  MergePathMode mergeMode = MergePathMode::Append;
  float roundCornerRadius = 0.0f;
  const TextModifier* curTextModifier = nullptr;
  const TextPath* curTextPath = nullptr;

  bool hasUpcomingRepeater = false;
  const TextBox* preScannedTextBox = nullptr;
  struct RichTextSpan {
    const Text* text = nullptr;
    const Fill* fill = nullptr;
    const Stroke* stroke = nullptr;
  };
  std::vector<RichTextSpan> richTextSpans = {};
  int richTextGroupCount = 0;
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::Repeater) {
      hasUpcomingRepeater = true;
    } else if (e->nodeType() == NodeType::TextBox) {
      preScannedTextBox = static_cast<const TextBox*>(e);
    } else if (e->nodeType() == NodeType::Group && preScannedTextBox == nullptr) {
      auto grp = static_cast<const Group*>(e);
      const Text* spanText = nullptr;
      const Fill* spanFill = nullptr;
      const Stroke* spanStroke = nullptr;
      bool isTextSpan = true;
      for (auto* ge : grp->elements) {
        auto gt = ge->nodeType();
        if (gt == NodeType::Text) {
          spanText = static_cast<const Text*>(ge);
        } else if (gt == NodeType::Fill) {
          spanFill = static_cast<const Fill*>(ge);
        } else if (gt == NodeType::Stroke) {
          spanStroke = static_cast<const Stroke*>(ge);
        } else {
          isTextSpan = false;
          break;
        }
      }
      if (isTextSpan && spanText && (spanFill || spanStroke)) {
        richTextGroupCount++;
        richTextSpans.push_back({spanText, spanFill, spanStroke});
      }
    }
  }
  bool useRichText = preScannedTextBox != nullptr && richTextGroupCount >= 2;

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Polystar:
      case NodeType::Text:
        geos.push_back({type, element, {}});
        break;
      case NodeType::Fill: {
        auto fill = static_cast<const Fill*>(element);
        curFill = fill;
        if (fill->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          if (curTextModifier && !geos.empty()) {
            writeTextModifier(out, geos, curTextModifier, curFill, nullptr, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, curFill, nullptr, curTextBox, a);
          } else {
            paintGeos(out, geos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::Stroke: {
        auto stroke = static_cast<const Stroke*>(element);
        curStroke = stroke;
        if (stroke->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          if (curTextModifier && !geos.empty()) {
            writeTextModifier(out, geos, curTextModifier, nullptr, curStroke, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, nullptr, curStroke, curTextBox, a);
          } else {
            paintGeos(out, geos, nullptr, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::TextBox: {
        curTextBox = static_cast<const TextBox*>(element);
        if (!curTextBox->elements.empty()) {
          // TextBox with child elements: render as a positioned container with text styling.
          auto tb = curTextBox;
          auto tbBounds = tb->layoutBounds();
          auto tbPos = tb->renderPosition();
          float tbW = !std::isnan(tb->width) ? tb->width : tbBounds.width;
          float tbH = !std::isnan(tb->height) ? tb->height : tbBounds.height;
          float tbLeft = tbPos.x;
          float tbTop = tbPos.y;
          std::string style = "position:absolute;left:" + FloatToString(tbLeft) +
                              "px;top:" + FloatToString(tbTop) + "px";
          if (!std::isnan(tbW) && tbW > 0) {
            style += ";width:" + FloatToString(tbW) + "px";
          }
          if (!std::isnan(tbH) && tbH > 0) {
            style += ";height:" + FloatToString(tbH) + "px";
          }
          if (tb->textAlign == TextAlign::Center) {
            style += ";text-align:center";
          } else if (tb->textAlign == TextAlign::End) {
            style += ";text-align:end";
          } else if (tb->textAlign == TextAlign::Justify) {
            style += ";text-align:justify";
          }
          if (tb->paragraphAlign != ParagraphAlign::Near) {
            style += ";display:flex;flex-direction:column";
            if (tb->paragraphAlign == ParagraphAlign::Middle) {
              style += ";justify-content:center";
            } else if (tb->paragraphAlign == ParagraphAlign::Far) {
              style += ";justify-content:flex-end";
            }
          }
          if (tb->writingMode == WritingMode::Vertical) {
            style += ";writing-mode:vertical-rl";
          }
          if (tb->lineHeight > 0) {
            style += ";line-height:" + FloatToString(tb->lineHeight) + "px";
          }
          if (tb->wordWrap && !std::isnan(tbW) && tbW > 0) {
            style += ";word-wrap:break-word";
          } else {
            style += ";white-space:nowrap";
          }
          if (tb->overflow == Overflow::Hidden) {
            style += ";overflow:hidden";
          }
          // Collect text spans from TextBox children (Text+Fill at top level, and Group children)
          struct TBSpan {
            const Text* text = nullptr;
            const Fill* fill = nullptr;
            const Stroke* stroke = nullptr;
          };
          std::vector<TBSpan> tbSpans;
          const Fill* topFill = nullptr;
          const Stroke* topStroke = nullptr;
          for (auto* childElem : tb->elements) {
            if (childElem->nodeType() == NodeType::Fill) {
              auto fill = static_cast<const Fill*>(childElem);
              topFill = fill;
              for (auto& s : tbSpans) {
                if (!s.fill) {
                  s.fill = fill;
                }
              }
            } else if (childElem->nodeType() == NodeType::Stroke) {
              auto stroke = static_cast<const Stroke*>(childElem);
              topStroke = stroke;
              for (auto& s : tbSpans) {
                if (!s.stroke) {
                  s.stroke = stroke;
                }
              }
            } else if (childElem->nodeType() == NodeType::Text) {
              tbSpans.push_back({static_cast<const Text*>(childElem), topFill, topStroke});
            } else if (childElem->nodeType() == NodeType::Group) {
              auto grp = static_cast<const Group*>(childElem);
              const Text* grpText = nullptr;
              const Fill* grpFill = nullptr;
              const Stroke* grpStroke = nullptr;
              for (auto* ge : grp->elements) {
                if (ge->nodeType() == NodeType::Text) {
                  if (!grpText) {
                    grpText = static_cast<const Text*>(ge);
                  }
                } else if (ge->nodeType() == NodeType::Fill) {
                  grpFill = static_cast<const Fill*>(ge);
                } else if (ge->nodeType() == NodeType::Stroke) {
                  grpStroke = static_cast<const Stroke*>(ge);
                }
              }
              if (grpText) {
                tbSpans.push_back(
                    {grpText, grpFill ? grpFill : topFill, grpStroke ? grpStroke : topStroke});
              }
            }
          }
          if (!tbSpans.empty()) {
            // Pin font-size on the container so Chromium's line-box strut uses this value
            // instead of the inherited ancestor default (typically 16px body), which would
            // otherwise inflate each line-box above the declared `line-height` and push every
            // text line further down than tgfx renders it. Use the smallest span font-size so
            // the strut contribution never exceeds what any child span would produce on its own.
            float strutSize = tbSpans[0].text->renderFontSize();
            for (const auto& s : tbSpans) {
              float fs = s.text->renderFontSize();
              if (fs > 0 && fs < strutSize) {
                strutSize = fs;
              }
            }
            if (strutSize > 0) {
              style += ";font-size:" + FloatToString(strutSize) + "px";
            }
            out.openTag("div");
            out.addAttr("style", style);
            out.closeTagStart();
            bool needsInnerWrap = tb->paragraphAlign != ParagraphAlign::Near;
            if (needsInnerWrap) {
              out.openTag("div");
              out.closeTagStart();
            }
            for (auto& span : tbSpans) {
              std::string spanStyle;
              if (!span.text->fontFamily.empty()) {
                spanStyle += "font-family:'" + span.text->fontFamily + "'";
              }
              if (!spanStyle.empty()) {
                spanStyle += ";";
              }
              spanStyle += "font-size:" + FloatToString(span.text->renderFontSize()) + "px";
              if (!span.text->fontStyle.empty()) {
                if (span.text->fontStyle.find("Bold") != std::string::npos) {
                  spanStyle += ";font-weight:bold";
                }
                if (span.text->fontStyle.find("Italic") != std::string::npos) {
                  spanStyle += ";font-style:italic";
                }
              }
              if (span.text->letterSpacing != 0.0f) {
                spanStyle += ";letter-spacing:" + FloatToString(span.text->letterSpacing) + "px";
              }
              if (span.fill && span.fill->color) {
                auto ct = span.fill->color->nodeType();
                if (ct == NodeType::SolidColor) {
                  auto sc = static_cast<const SolidColor*>(span.fill->color);
                  spanStyle += ";color:" + ColorToRGBA(sc->color, span.fill->alpha);
                } else {
                  float ca = 1.0f;
                  std::string css = colorToCSS(span.fill->color, &ca);
                  if (!css.empty()) {
                    spanStyle += ";background:" + css;
                    spanStyle += ";-webkit-background-clip:text;background-clip:text";
                    spanStyle += ";-webkit-text-fill-color:transparent";
                  }
                }
              }
              if (span.text->fauxBold && !span.stroke) {
                spanStyle += ";-webkit-text-stroke:" +
                             FloatToString(FauxBoldStrokeWidth(span.text->renderFontSize())) +
                             "px currentColor";
              }
              if (span.stroke && span.stroke->color &&
                  span.stroke->color->nodeType() == NodeType::SolidColor) {
                auto sc = static_cast<const SolidColor*>(span.stroke->color);
                spanStyle += ";-webkit-text-stroke:" + FloatToString(span.stroke->width) + "px " +
                             ColorToRGBA(sc->color, span.stroke->alpha);
                spanStyle += ";paint-order:stroke fill";
              }
              if (span.text->fauxItalic) {
                // `transform` is a no-op on pure inline boxes, so promote the span to
                // inline-block. Each tbSpan is a short text run, so losing word-wrap inside
                // the span is acceptable and matches the single-run expectation of fauxItalic.
                spanStyle += ";display:inline-block;transform:skewX(-12deg)";
              }
              out.openTag("span");
              out.addAttr("style", spanStyle);
              // Respect tgfx's wrap decisions when possible. CSS word-wrap uses Chromium's own
              // glyph advance measurements, which differ from tgfx's HarfBuzz output by a few
              // subpixels and can push the break point past/before a token near the TextBox
              // edge (e.g. text_box "Text line 1..5", text_features "This text overflows...").
              // Splitting the source string along tgfx's line boundaries and emitting one
              // `<br>` per break forces Chromium to mirror tgfx's layout exactly.
              std::vector<TgfxWrappedLine> tgfxLines;
              bool canSplit = tb->wordWrap && !std::isnan(tbW) && tbW > 0 &&
                              TrySplitTextByTgfxLines(span.text->text,
                                                      span.text->wrappedGlyphCounts(), tgfxLines);
              if (canSplit) {
                out.closeTagStart();
                // For `textAlign=justify` we can't rely on `<br>`-separated inline runs: CSS
                // treats the text before each `<br>` as a line break, i.e. the "last line" of
                // that fragment, so `text-align:justify` leaves them start-aligned (matches
                // `text-align-last:auto`). tgfx justifies every line except the final one by
                // expanding the inter-word gaps, so we emit each non-final tgfx line as a
                // block-level `<span>` with `text-align-last:justify` to force Chromium to
                // distribute the leftover width, and keep the final line without the override
                // so it stays start-aligned (mirrors TextLayout.cpp's `lineIdx < size-1` check).
                bool justifyPerLine = tb->textAlign == TextAlign::Justify && tgfxLines.size() > 1;
                for (size_t li = 0; li < tgfxLines.size(); li++) {
                  auto& ln = tgfxLines[li];
                  std::string lineText =
                      span.text->text.substr(ln.byteStart, ln.byteEnd - ln.byteStart);
                  if (justifyPerLine) {
                    out.openTag("span");
                    bool isLast = li + 1 == tgfxLines.size();
                    // `white-space:nowrap` keeps Chromium from re-wrapping the per-line slice
                    // when its own glyph-advance measurement runs slightly wider than tgfx's
                    // (a single word like "justify" hitting the edge will otherwise bump to a
                    // new line and nullify the `text-align-last:justify` distribution).
                    out.addAttr("style",
                                isLast ? std::string("display:block")
                                       : std::string("display:block;white-space:nowrap;text-align-"
                                                     "last:justify"));
                    out.closeTagWithText(lineText);
                  } else {
                    if (li > 0) {
                      out.openTag("br");
                      out.closeTagSelfClosing();
                    }
                    out.addTextContent(lineText);
                  }
                }
                out.closeTag();
              } else {
                out.closeTagWithText(span.text->text);
              }
            }
            if (needsInnerWrap) {
              out.closeTag();
            }
            out.closeTag();
          }
        } else if (useRichText && !richTextSpans.empty()) {
          auto tb = curTextBox;
          auto tbPos = tb->renderPosition();
          std::string style = "position:absolute;left:" + FloatToString(tbPos.x) +
                              "px;top:" + FloatToString(tbPos.y) + "px";
          if (!std::isnan(tb->width)) {
            style += ";width:" + FloatToString(tb->width) + "px";
          }
          if (!std::isnan(tb->height)) {
            style += ";height:" + FloatToString(tb->height) + "px";
          }
          if (tb->paragraphAlign != ParagraphAlign::Near) {
            style += ";display:flex;flex-direction:column";
            if (tb->paragraphAlign == ParagraphAlign::Middle) {
              style += ";justify-content:center";
            } else if (tb->paragraphAlign == ParagraphAlign::Far) {
              style += ";justify-content:flex-end";
            }
          }
          if (tb->textAlign == TextAlign::Center) {
            style += ";text-align:center";
          } else if (tb->textAlign == TextAlign::End) {
            style += ";text-align:end";
          } else if (tb->textAlign == TextAlign::Justify) {
            style += ";text-align:justify";
          }
          if (tb->wordWrap) {
            style += ";word-wrap:break-word";
          }
          if (tb->overflow == Overflow::Hidden) {
            style += ";overflow:hidden";
          }
          out.openTag("div");
          out.addAttr("style", style);
          out.closeTagStart();
          bool needsInnerWrap = tb->paragraphAlign != ParagraphAlign::Near;
          if (needsInnerWrap) {
            out.openTag("div");
            out.closeTagStart();
          }
          for (auto& span : richTextSpans) {
            std::string spanStyle = tb->wordWrap ? "white-space:pre-wrap" : "white-space:pre";
            if (!span.text->fontFamily.empty()) {
              spanStyle += ";font-family:'" + span.text->fontFamily + "'";
            }
            spanStyle += ";font-size:" + FloatToString(span.text->renderFontSize()) + "px";
            if (!span.text->fontStyle.empty()) {
              if (span.text->fontStyle.find("Bold") != std::string::npos) {
                spanStyle += ";font-weight:bold";
              }
              if (span.text->fontStyle.find("Italic") != std::string::npos) {
                spanStyle += ";font-style:italic";
              }
            }
            if (span.text->letterSpacing != 0.0f) {
              spanStyle += ";letter-spacing:" + FloatToString(span.text->letterSpacing) + "px";
            }
            if (span.fill && span.fill->color) {
              auto ct = span.fill->color->nodeType();
              if (ct == NodeType::SolidColor) {
                auto sc = static_cast<const SolidColor*>(span.fill->color);
                spanStyle += ";color:" + ColorToRGBA(sc->color, span.fill->alpha);
              } else {
                float ca = 1.0f;
                std::string css = colorToCSS(span.fill->color, &ca);
                if (!css.empty()) {
                  spanStyle += ";background:" + css;
                  spanStyle += ";-webkit-background-clip:text;background-clip:text";
                  spanStyle += ";-webkit-text-fill-color:transparent";
                }
              }
            }
            if (span.text->fauxBold && !span.stroke) {
              spanStyle += ";-webkit-text-stroke:" +
                           FloatToString(FauxBoldStrokeWidth(span.text->renderFontSize())) +
                           "px currentColor";
            }
            if (span.stroke && span.stroke->color &&
                span.stroke->color->nodeType() == NodeType::SolidColor) {
              auto sc = static_cast<const SolidColor*>(span.stroke->color);
              spanStyle += ";-webkit-text-stroke:" + FloatToString(span.stroke->width) + "px " +
                           ColorToRGBA(sc->color, span.stroke->alpha);
              spanStyle += ";paint-order:stroke fill";
            }
            if (span.text->fauxItalic) {
              // `transform` is a no-op on pure inline boxes, so promote the span to
              // inline-block. Each rich-text span is a short text run, so losing word-wrap
              // inside the span is acceptable and matches the single-run expectation of
              // fauxItalic.
              spanStyle += ";display:inline-block;transform:skewX(-12deg)";
            }
            out.openTag("span");
            out.addAttr("style", spanStyle);
            // Mirror tgfx's line-break decisions with explicit <br> tags instead of relying on
            // Chromium's word-wrap (same rationale as the inline-TextBox path above).
            std::vector<TgfxWrappedLine> tgfxLines;
            bool canSplit =
                tb->wordWrap && TrySplitTextByTgfxLines(span.text->text,
                                                        span.text->wrappedGlyphCounts(), tgfxLines);
            if (canSplit) {
              out.closeTagStart();
              // Same justify-aware per-line emission as the inline-TextBox path: block-level
              // spans with `text-align-last:justify` on every non-final line let Chromium
              // reproduce tgfx's "stretch every line except the last" justify semantics.
              bool justifyPerLine = tb->textAlign == TextAlign::Justify && tgfxLines.size() > 1;
              for (size_t li = 0; li < tgfxLines.size(); li++) {
                auto& ln = tgfxLines[li];
                std::string lineText =
                    span.text->text.substr(ln.byteStart, ln.byteEnd - ln.byteStart);
                if (justifyPerLine) {
                  out.openTag("span");
                  bool isLast = li + 1 == tgfxLines.size();
                  out.addAttr("style", isLast ? std::string("display:block")
                                              : std::string("display:block;white-space:nowrap;"
                                                            "text-align-last:justify"));
                  out.closeTagWithText(lineText);
                } else {
                  if (li > 0) {
                    out.openTag("br");
                    out.closeTagSelfClosing();
                  }
                  out.addTextContent(lineText);
                }
              }
              out.closeTag();
            } else {
              out.closeTagWithText(span.text->text);
            }
          }
          if (needsInnerWrap) {
            out.closeTag();
          }
          out.closeTag();
        }
        break;
      }
      case NodeType::TrimPath:
        hasTrim = true;
        curTrim = static_cast<const TrimPath*>(element);
        break;
      case NodeType::RoundCorner: {
        auto rc = static_cast<const RoundCorner*>(element);
        roundCornerRadius = rc->radius;
        if (roundCornerRadius > 0) {
          for (auto& g : geos) {
            if (g.type == NodeType::Rectangle || g.type == NodeType::Ellipse ||
                g.type == NodeType::Path || g.type == NodeType::Polystar) {
              PathData pathData = PathDataFromSVGString("");
              GeoToPathData(g.element, g.type, pathData);
              PathData rounded = PathDataFromSVGString("");
              ApplyRoundCorner(pathData, roundCornerRadius, rounded);
              g.modifiedPathData = PathDataToSVGString(rounded);
            }
          }
        }
        break;
      }
      case NodeType::MergePath: {
        auto mp = static_cast<const MergePath*>(element);
        hasMerge = true;
        mergeMode = mp->mode;
        curFill = nullptr;
        curStroke = nullptr;
        break;
      }
      case NodeType::TextModifier:
        curTextModifier = static_cast<const TextModifier*>(element);
        break;
      case NodeType::TextPath:
        curTextPath = static_cast<const TextPath*>(element);
        break;
      case NodeType::Group: {
        auto group = static_cast<const Group*>(element);
        if (useRichText) {
          bool isRichSpan = false;
          for (auto& span : richTextSpans) {
            if (span.text != nullptr) {
              for (auto* ge : group->elements) {
                if (ge == span.text) {
                  isRichSpan = true;
                  break;
                }
              }
            }
            if (isRichSpan) {
              break;
            }
          }
          if (isRichSpan) {
            break;
          }
        }
        bool hasPainter = false;
        for (auto* ge : group->elements) {
          auto gt = ge->nodeType();
          if (gt == NodeType::Fill || gt == NodeType::Stroke) {
            hasPainter = true;
            break;
          }
        }
        if (hasPainter && group->alpha < 1.0f) {
          writeGroup(out, group, alpha, distribute);
        } else {
          auto savedFill = curFill;
          auto savedStroke = curStroke;
          auto savedHasTrim = hasTrim;
          auto savedTrim = curTrim;
          curFill = nullptr;
          curStroke = nullptr;
          hasTrim = false;
          curTrim = nullptr;
          auto savedGeos = std::move(geos);
          geos.clear();
          std::vector<GeoInfo> groupGeos;
          Matrix gm = BuildGroupMatrix(group);
          for (auto* ge : group->elements) {
            auto gt = ge->nodeType();
            if (gt == NodeType::Rectangle || gt == NodeType::Ellipse || gt == NodeType::Path ||
                gt == NodeType::Polystar) {
              PathData pathData = PathDataFromSVGString("");
              GeoToPathData(ge, gt, pathData);
              if (!pathData.isEmpty()) {
                std::string svgPath = gm.isIdentity() ? PathDataToSVGString(pathData)
                                                      : TransformPathDataToSVG(pathData, gm);
                GeoInfo info = {gt, ge, svgPath};
                geos.push_back(info);
                groupGeos.push_back(info);
              }
            } else if (gt == NodeType::Text) {
              GeoInfo info = {gt, ge, {}};
              geos.push_back(info);
              groupGeos.push_back(info);
            } else if (gt == NodeType::Fill) {
              auto fill = static_cast<const Fill*>(ge);
              curFill = fill;
              if (fill->placement == targetPlacement && !hasUpcomingRepeater) {
                float a = distribute ? alpha : 1.0f;
                paintGeos(out, geos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                          mergeMode);
              }
            } else if (gt == NodeType::Stroke) {
              auto stroke = static_cast<const Stroke*>(ge);
              curStroke = stroke;
              if (stroke->placement == targetPlacement && !hasUpcomingRepeater) {
                float a = distribute ? alpha : 1.0f;
                paintGeos(out, geos, curFill, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                          mergeMode);
              }
            } else if (gt == NodeType::TrimPath) {
              hasTrim = true;
              curTrim = static_cast<const TrimPath*>(ge);
            } else if (gt == NodeType::Group) {
              writeGroup(out, static_cast<const Group*>(ge), alpha, distribute);
            }
          }
          curFill = savedFill;
          curStroke = savedStroke;
          hasTrim = savedHasTrim;
          curTrim = savedTrim;
          savedGeos.insert(savedGeos.end(), groupGeos.begin(), groupGeos.end());
          geos = std::move(savedGeos);
        }
        break;
      }
      case NodeType::Repeater: {
        auto rep = static_cast<const Repeater*>(element);
        // tgfx's Repeater::apply multiplies `copyAlpha` onto the painters that already exist
        // when the repeater runs, i.e. only Fill/Stroke nodes emitted *before* the Repeater.
        // Painters added after the Repeater attach to every copy uniformly with alpha=1.0.
        // Mirror this ordering so the HTML decay (div opacity per copy) only kicks in when
        // the fill/stroke was declared before the Repeater (showcase_mandala Middle Ring has
        // the Stroke after, so every petal must render at full alpha).
        const Fill* repFillBefore = curFill;
        const Stroke* repStrokeBefore = curStroke;
        const Fill* repFillAfter = nullptr;
        const Stroke* repStrokeAfter = nullptr;
        if (!repFillBefore || !repStrokeBefore) {
          for (auto* e : elements) {
            if (e == element) {
              continue;
            }
            if (e->nodeType() == NodeType::Fill && !repFillBefore && !repFillAfter) {
              auto f = static_cast<const Fill*>(e);
              if (f->placement == targetPlacement) {
                repFillAfter = f;
              }
            } else if (e->nodeType() == NodeType::Stroke && !repStrokeBefore && !repStrokeAfter) {
              auto s = static_cast<const Stroke*>(e);
              if (s->placement == targetPlacement) {
                repStrokeAfter = s;
              }
            }
          }
        }
        const Fill* repFill = repFillBefore ? repFillBefore : repFillAfter;
        const Stroke* repStroke = repStrokeBefore ? repStrokeBefore : repStrokeAfter;
        bool applyDecay = (repFillBefore != nullptr) || (repStrokeBefore != nullptr);
        writeRepeater(out, rep, geos, repFill, repStroke, distribute ? alpha : 1.0f, curTrim,
                      applyDecay);
        geos.clear();
        curFill = nullptr;
        curStroke = nullptr;
        hasTrim = false;
        curTrim = nullptr;
        hasMerge = false;
        mergeMode = MergePathMode::Append;
        roundCornerRadius = 0.0f;
        curTextModifier = nullptr;
        curTextPath = nullptr;
        break;
      }
      default:
        break;
    }
  }
}

static std::string clipPathFromContents(const Layer* layer) {
  auto layerBounds = layer->layoutBounds();
  float containerW = layerBounds.width;
  float containerH = layerBounds.height;
  if (containerW <= 0 || containerH <= 0) {
    return {};
  }
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      float top = bounds.y;
      float left = bounds.x;
      float bottom = containerH - (bounds.y + bounds.height);
      float right = containerW - (bounds.x + bounds.width);
      if (top < 0) top = 0;
      if (left < 0) left = 0;
      if (bottom < 0) bottom = 0;
      if (right < 0) right = 0;
      std::string clip = ";clip-path:inset(" + FloatToString(top) + "px " + FloatToString(right) +
                         "px " + FloatToString(bottom) + "px " + FloatToString(left) + "px";
      if (r->roundness > 0) {
        clip += " round " + FloatToString(r->roundness) + "px";
      }
      clip += ")";
      return clip;
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      float cx = bounds.x + bounds.width / 2;
      float cy = bounds.y + bounds.height / 2;
      return ";clip-path:ellipse(" + FloatToString(bounds.width / 2) + "px " +
             FloatToString(bounds.height / 2) + "px at " + FloatToString(cx) + "px " +
             FloatToString(cy) + "px)";
    }
  }
  return {};
}

// Returns a CSS border-radius value string (without the property prefix, e.g. "20px" or "50%")
// when the layer's visual contour can be expressed by applying border-radius directly to the
// layer's own <div>. Only matches the narrow case where the first contents child is a Rectangle
// or Ellipse that fully covers the layer: this is the only shape whose box-shadow outline must
// trace a plain rounded rectangle. Returns empty string when no matching shape exists.
//
// Used exclusively by the DropShadowStyle + BackgroundBlurStyle coexistence workaround: CSS
// `filter: drop-shadow()` establishes an isolated stacking context that breaks the sibling
// `backdrop-filter` sampling path. `box-shadow` does not establish that context, but it paints
// along the element's own border-box — only correct when we can also put the matching
// border-radius on that border-box.
static std::string layerBoxShadowBorderRadius(const Layer* layer) {
  auto layerBounds = layer->layoutBounds();
  float containerW = layerBounds.width;
  float containerH = layerBounds.height;
  if (containerW <= 0 || containerH <= 0) {
    return {};
  }
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      // Rectangle must fully cover the layer (same logic as clipPathFromContents: top/left/
      // bottom/right insets are all zero). Otherwise box-shadow would trace the wrong outline.
      if (bounds.x > 0.5f || bounds.y > 0.5f || bounds.x + bounds.width < containerW - 0.5f ||
          bounds.y + bounds.height < containerH - 0.5f) {
        return {};
      }
      if (r->roundness > 0) {
        return FloatToString(r->roundness) + "px";
      }
      return "0";
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      // Ellipse must exactly match the layer bounds (centered, full size).
      if (bounds.x > 0.5f || bounds.y > 0.5f || bounds.x + bounds.width < containerW - 0.5f ||
          bounds.y + bounds.height < containerH - 0.5f) {
        return {};
      }
      return "50%";
    }
    // Non-primitive first element (Group, Path, Text, etc.) cannot be represented as a simple
    // rounded box-shadow outline, so leave the caller to fall back to filter:drop-shadow.
    return {};
  }
  return {};
}

// Describes the geometry of a layer's primary fill shape (the first Rectangle or Ellipse in
// layer->contents) in layer-local coordinates, so a sibling <div> can reproduce just that outline
// without inheriting the alpha of the layer's other descendants or filter outputs.
//
// Used by DropShadowStyle emission: rendering the shadow as a blurred sibling div whose alpha
// comes from the layer's own geometry mirrors tgfx's DropShadowStyle (source = opaque content
// silhouette), whereas applying `filter:drop-shadow` on the layer div would include every child
// and every child's filter halo in the shadow source — producing a visibly stronger/larger
// shadow, especially in nested DropShadow scenarios like drop_shadow_show_behind.pagx Row 2.
struct ShadowShape {
  float left = 0;
  float top = 0;
  float width = 0;
  float height = 0;
  // CSS border-radius value without the property name (e.g. "12px" or "50%"). Empty when the
  // shape is a square-cornered Rectangle.
  std::string radius;
  bool valid = false;
};

static ShadowShape findLayerShadowShape(const Layer* layer) {
  ShadowShape s = {};
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      s.left = bounds.x;
      s.top = bounds.y;
      s.width = bounds.width;
      s.height = bounds.height;
      if (r->roundness > 0) {
        s.radius = FloatToString(r->roundness) + "px";
      }
      s.valid = true;
      return s;
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      s.left = bounds.x;
      s.top = bounds.y;
      s.width = bounds.width;
      s.height = bounds.height;
      s.radius = "50%";
      s.valid = true;
      return s;
    }
    // Non-primitive geometry (Path, Polystar, Text fill, Group, etc.) cannot be rendered as a
    // simple CSS-styled div, so fall back to the SVG filter path.
    return s;
  }
  return s;
}

void HTMLWriter::writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha,
                                    bool distribute, LayerPlacement targetPlacement) {
  writeElements(out, layer->contents, alpha, distribute, targetPlacement);
}

//==============================================================================
// HTMLWriter – layer
//==============================================================================

void HTMLWriter::writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha,
                            bool distributeAlpha, bool isFlexItem) {
  if (!layer->visible) {
    out.openTag("div");
    out.addAttr("class", "pagx-layer");
    out.addAttr("style", "display:none");
    if (!layer->id.empty()) {
      out.addAttr("id", layer->id);
    }
    out.closeTagSelfClosing();
    return;
  }

  bool isFlexContainer = (layer->layout != LayoutMode::None);

  std::string style;
  style.reserve(300);

  if (isFlexItem) {
    // Flex item: positioned by parent's flexbox, no absolute positioning needed.
    if (layer->flex > 0) {
      style += "flex:" + FloatToString(layer->flex);
    }
    // When the layer has absolute-positioned contents (Text, shapes), it needs explicit size
    // so that contents' coordinates are correct. Use pagx explicit size first, then fall back
    // to layout-resolved size. Skip the main-axis dimension when flex > 0 to let flex work.
    bool needsSize = !layer->contents.empty() || !layer->styles.empty();
    auto bounds = needsSize ? layer->layoutBounds() : Rect{};
    float outputW = !std::isnan(layer->width) ? layer->width : (needsSize ? bounds.width : NAN);
    float outputH = !std::isnan(layer->height) ? layer->height : (needsSize ? bounds.height : NAN);
    if (!std::isnan(outputW) && outputW > 0) {
      if (!style.empty()) {
        style += ';';
      }
      style += "width:" + FloatToString(outputW) + "px";
    }
    if (!std::isnan(outputH) && outputH > 0) {
      if (!style.empty()) {
        style += ';';
      }
      style += "height:" + FloatToString(outputH) + "px";
    }
    // Flex item needs position:relative for absolute-positioned contents or child layers.
    if (isFlexContainer || needsSize || !layer->children.empty()) {
      if (!style.empty()) {
        style += ';';
      }
      style += "position:relative";
    }
  } else {
    style += "position:absolute";
    auto renderPos = layer->renderPosition();
    std::string transform = LayerTransformCSS(layer);
    // `positionSet` becomes true after we emit `left/top`. The Repeater branch below may need
    // to shift `renderPos` by the union-bounds offset (uL, uT) so the layer div extends into
    // the negative quadrants of the layer origin (needed for stacking-context clipping like
    // mix-blend-mode, which otherwise drops Repeater copies that live at negative x/y).
    bool positionSet = false;
    auto emitLeftTop = [&](float x, float y) {
      style += ";left:" + FloatToString(x) + "px;top:" + FloatToString(y) + "px";
      positionSet = true;
    };
    // Absolute-positioned layers need explicit size when they have contents that use inset:0,
    // or when they are flex containers that need a reference frame for child layout.
    // When contents contain a Repeater, compute the union bounds of all repeated copies
    // instead of using layoutBounds(), which only covers the original (un-repeated) geometry.
    const Repeater* repeater = nullptr;
    for (auto* e : layer->contents) {
      if (e->nodeType() == NodeType::Repeater) {
        repeater = static_cast<const Repeater*>(e);
        break;
      }
    }
    // Repeater-copy origin offset: when union bounds extend into negative quadrants we shift
    // the layer div by (uL, uT) so the layer origin sits at (-uL, -uT) inside the div; each
    // Repeater copy then needs a compensating `translate(-uL, -uT)` prepended so its content
    // still paints at the layer origin in world space. See `_ctx->repeaterOriginOffset`.
    float repeaterOffsetX = 0;
    float repeaterOffsetY = 0;
    if (repeater && repeater->copies > 0) {
      // Collect bounds of geometry elements before the Repeater.
      float geoL = 1e9f, geoT = 1e9f, geoR = -1e9f, geoB = -1e9f;
      bool hasGeo = false;
      for (auto* e : layer->contents) {
        if (e == repeater) {
          break;
        }
        auto* node = LayoutNode::AsLayoutNode(e);
        if (!node) {
          continue;
        }
        auto nb = node->layoutBounds();
        if (nb.isEmpty()) {
          continue;
        }
        geoL = std::min(geoL, nb.x);
        geoT = std::min(geoT, nb.y);
        geoR = std::max(geoR, nb.x + nb.width);
        geoB = std::max(geoB, nb.y + nb.height);
        hasGeo = true;
      }
      if (hasGeo) {
        // Transform this rect through every Repeater copy matrix and compute union bounds.
        float uL = 1e9f, uT = 1e9f, uR = -1e9f, uB = -1e9f;
        int n = static_cast<int>(std::ceil(repeater->copies));
        for (int i = 0; i < n; i++) {
          float prog = static_cast<float>(i) + repeater->offset;
          Matrix m = {};
          if (repeater->anchor.x != 0 || repeater->anchor.y != 0) {
            m = Matrix::Translate(-repeater->anchor.x, -repeater->anchor.y);
          }
          float sx = std::pow(repeater->scale.x, prog);
          float sy = std::pow(repeater->scale.y, prog);
          if (sx != 1.0f || sy != 1.0f) {
            m = Matrix::Scale(sx, sy) * m;
          }
          float rot = repeater->rotation * prog;
          if (rot != 0) {
            m = Matrix::Rotate(rot) * m;
          }
          float px = repeater->position.x * prog;
          float py = repeater->position.y * prog;
          if (px != 0 || py != 0) {
            m = Matrix::Translate(px, py) * m;
          }
          if (repeater->anchor.x != 0 || repeater->anchor.y != 0) {
            m = Matrix::Translate(repeater->anchor.x, repeater->anchor.y) * m;
          }
          // Map the 4 corners of the geometry rect.
          Point corners[4] = {{geoL, geoT}, {geoR, geoT}, {geoR, geoB}, {geoL, geoB}};
          for (auto& c : corners) {
            auto p = m.mapPoint(c);
            uL = std::min(uL, p.x);
            uT = std::min(uT, p.y);
            uR = std::max(uR, p.x);
            uB = std::max(uB, p.y);
          }
        }
        float uw = uR - uL;
        float uh = uB - uT;
        // Shift the layer div so its border-box covers all Repeater copies, including those
        // that sit in negative quadrants relative to the layer origin. Without this, the
        // stacking context created by mix-blend-mode (seen in showcase_mandala's Middle/Outer
        // Ring) clips copies whose bounds fall outside the border-box, producing the visible
        // "half the petals disappear" artefact.
        if (uL < 0 || uT < 0) {
          repeaterOffsetX = uL;
          repeaterOffsetY = uT;
        }
        emitLeftTop(renderPos.x + repeaterOffsetX, renderPos.y + repeaterOffsetY);
        if (uw > 0) {
          style += ";width:" + FloatToString(uw) + "px";
        }
        if (uh > 0) {
          style += ";height:" + FloatToString(uh) + "px";
        }
        style += ";overflow:visible";
      }
    }
    if (!positionSet) {
      if (!FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y)) {
        emitLeftTop(renderPos.x, renderPos.y);
      } else if (!transform.empty()) {
        emitLeftTop(0, 0);
      }
    }
    if (!transform.empty()) {
      style += ";transform:" + transform;
      style += ";transform-origin:0 0";
    }
    // Stash the Repeater origin offset so writeRepeater can prepend a compensating translate
    // to each copy's transform; the matrix stays in layer-local coordinates as before.
    _ctx->repeaterOriginOffsetX = repeaterOffsetX;
    _ctx->repeaterOriginOffsetY = repeaterOffsetY;
    // The legacy branch below handled non-Repeater layers and still runs when `repeater` is
    // null. When a Repeater was present and sized the div above, skip this fallback.
    if (!repeater && (isFlexContainer || !layer->contents.empty())) {
      auto bounds = layer->layoutBounds();
      if (bounds.width > 0) {
        style += ";width:" + FloatToString(bounds.width) + "px";
      }
      if (bounds.height > 0) {
        style += ";height:" + FloatToString(bounds.height) + "px";
      }
    }
  }

  // Flex container properties
  if (isFlexContainer) {
    style += ";display:flex;box-sizing:border-box";
    if (layer->layout == LayoutMode::Horizontal) {
      style += ";flex-direction:row";
    } else {
      style += ";flex-direction:column";
    }

    // Space-* arrangement handling. PAGX's layout fully absorbs the declared gap into the
    // redistribution (extraGap = (availableMain - totalChildMain) / denom). CSS flex instead
    // treats `gap` as a minimum and adds justify-content's distribution on top. To make CSS
    // match PAGX, emit `justify-content:space-*` natively but *drop* the declared gap, then
    // make sure the container has a concrete main-axis size equal to PAGX's measured layout
    // size (totalChildMain + totalGap when shrink-to-fit; the stretched parent size when
    // stretched). Without the explicit size, a shrink-to-fit flex container would collapse to
    // the children's bare total width and space-* would have no room to distribute.
    bool isSpace = layer->arrangement == Arrangement::SpaceBetween ||
                   layer->arrangement == Arrangement::SpaceEvenly ||
                   layer->arrangement == Arrangement::SpaceAround;
    if (!isSpace && layer->gap > 0) {
      style += ";gap:" + FloatToString(layer->gap) + "px";
    }
    if (!layer->padding.isZero()) {
      style += ";padding:" + PaddingToCSS(layer->padding);
    }
    if (layer->alignment != Alignment::Stretch) {
      style += ";align-items:";
      style += AlignmentToCSS(layer->alignment);
    }
    if (layer->arrangement != Arrangement::Start) {
      style += ";justify-content:";
      style += ArrangementToCSS(layer->arrangement);
    }
    // If a space-* row doesn't already carry an explicit main-axis size, pin it to the pagx
    // measured size so CSS has the same distribution budget as the tgfx layout.
    if (isSpace && isFlexItem && layer->flex <= 0) {
      bool horizontal = layer->layout == LayoutMode::Horizontal;
      auto bounds = layer->layoutBounds();
      if (horizontal && std::isnan(layer->width) && bounds.width > 0) {
        if (style.find("width:") == std::string::npos) {
          style += ";width:" + FloatToString(bounds.width) + "px";
        }
      } else if (!horizontal && std::isnan(layer->height) && bounds.height > 0) {
        if (style.find("height:") == std::string::npos) {
          style += ";height:" + FloatToString(bounds.height) + "px";
        }
      }
    }
  }

  if (layer->preserve3D) {
    style += ";transform-style:preserve-3d";
  }

  bool groupOp = layer->groupOpacity;
  float layerAlpha = layer->alpha;
  if (distributeAlpha) {
    layerAlpha *= parentAlpha;
  }
  // Tentatively set up distribution; box-shadow fallback may override further below because
  // CSS `opacity < 1` creates a stacking context that breaks the sibling `backdrop-filter`
  // sampling path, which defeats the whole point of using box-shadow in the first place.
  bool childDistribute = !groupOp && layerAlpha < 1.0f;
  bool suppressGroupOpacity = false;  // set by the box-shadow fallback path below

  if (layer->blendMode != BlendMode::Normal) {
    // PlusDarker takes a dedicated filter-based path when the pre-pass has rendered a backdrop
    // for this layer; otherwise it falls through to the mix-blend-mode:darken approximation.
    bool emittedPlusDarkerFilter = false;
    if (layer->blendMode == BlendMode::PlusDarker) {
      auto it = _ctx->plusDarkerBackdrops.find(layer);
      if (it != _ctx->plusDarkerBackdrops.end()) {
        emitPlusDarkerFilterDef(it->second);
        style += ";filter:url(#" + it->second.filterId + ")";
        emittedPlusDarkerFilter = true;
      }
    }
    if (!emittedPlusDarkerFilter) {
      auto blendStr = BlendModeToMixBlendMode(layer->blendMode);
      if (blendStr) {
        style += ";mix-blend-mode:";
        style += blendStr;
      }
    }
  }

  if (!layer->passThroughBackground) {
    style += ";isolation:isolate";
  }

  if (!layer->antiAlias) {
    style += ";shape-rendering:crispEdges;image-rendering:pixelated";
  }

  std::string filterValues;

  // Detect BlurFilter.tileMode=Mirror: we switch to a 3x3 mirror-tile DOM emission path below
  // so the layer's own `filter:` property should NOT carry the blur (it's applied on the inner
  // grid wrapper instead), and the outer div gains `overflow:hidden` to clip the mirrored tiles
  // back to the source layer's size. The tile geometry uses the layer's own size, falling back
  // to its laid-out bounds when width/height are NaN.
  bool useMirrorTile = needsMirrorTiling(layer);
  float mirrorTileWidth = 0;
  float mirrorTileHeight = 0;
  if (useMirrorTile) {
    auto bounds = layer->layoutBounds();
    mirrorTileWidth = !std::isnan(layer->width) ? layer->width : bounds.width;
    mirrorTileHeight = !std::isnan(layer->height) ? layer->height : bounds.height;
    if (mirrorTileWidth <= 0 || mirrorTileHeight <= 0) {
      // Fall back to decal (current behaviour) when size is indeterminate: the grid wrapper
      // needs a concrete W/H to position the 9 tiles.
      useMirrorTile = false;
    }
  }

  if (!layer->filters.empty() && !useMirrorTile) {
    std::string filterCSS = writeFilterDefs(layer->filters);
    if (!filterCSS.empty()) {
      filterValues += filterCSS;
    }
  }

  std::vector<std::pair<NodeType, const LayerStyle*>> belowStyles = {};
  std::vector<std::pair<NodeType, const LayerStyle*>> aboveStyles = {};

  // Chromium evaluates `backdrop-filter` against the nearest enclosing stacking context that
  // does NOT itself have a `filter`. When a layer has BOTH BackgroundBlurStyle (which renders
  // via child `backdrop-filter` divs) and DropShadowStyle (which usually renders via the
  // parent's `filter: drop-shadow`), the drop-shadow creates an isolated stacking context and
  // the child backdrop-filter samples the empty group surface instead of the page below.
  // Visual symptom: glass cards fail to blur whatever is behind them.
  //
  // Swap drop-shadow for `box-shadow` in that specific combination. `box-shadow` paints into
  // the same stacking context as the element's siblings, so the child backdrop-filter still
  // sees the pre-layer backdrop. `box-shadow` only reproduces the right outline when the
  // layer's visible contour IS the element's rounded border-box, which we detect via
  // layerBoxShadowBorderRadius.
  bool hasBackdropBlurFill = false;
  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle && ls->blendMode == BlendMode::Normal) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      if ((blur->blurX + blur->blurY) * 0.5f > 0) {
        hasBackdropBlurFill = true;
        break;
      }
    }
  }
  std::string boxShadowValue;  // non-empty when the drop-shadow branch is redirected here
  std::string
      boxShadowBorderRadius;  // border-radius to apply on the layer div when using box-shadow
  // DropShadowStyle emissions that avoid the `filter:url(...)` path. Each entry is the full
  // `style=""` value for a sibling <div> placed below the layer's children in z-order. Using a
  // sibling div (instead of the layer's own `filter`) keeps the shadow source confined to the
  // layer's primary fill shape — critical for parity with tgfx when the layer has descendants
  // that paint their own filters (otherwise those descendants leak into the shadow's alpha
  // source and darken/widen the shadow beyond the layer silhouette).
  std::vector<std::string> pendingSiblingShadows;

  for (auto* ls : layer->styles) {
    bool hasBlendMode = ls->blendMode != BlendMode::Normal;

    if (ls->nodeType() == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::DropShadowStyle, ls});
      } else if (ds->blurX == ds->blurY && ds->showBehindLayer && hasBackdropBlurFill &&
                 boxShadowValue.empty()) {
        std::string radius = layerBoxShadowBorderRadius(layer);
        if (!radius.empty()) {
          // box-shadow fallback: preserves the sibling backdrop-filter sampling path. Also
          // propagate group opacity down to children, because `opacity < 1` on the layer div
          // would re-introduce the stacking context we just eliminated.
          boxShadowValue = FloatToString(ds->offsetX) + "px " + FloatToString(ds->offsetY) + "px " +
                           FloatToString(ds->blurX) + "px " + ColorToRGBA(ds->color);
          boxShadowBorderRadius = radius;
          suppressGroupOpacity = true;
          continue;
        }
        // Fall through to the SVG filter path below; CSS `filter: drop-shadow()` is avoided
        // because it reads source alpha as-is, so semi-transparent fills produce proportionally
        // weaker shadows while PAGX's shadow shape comes from a saturated opaque silhouette.
      }
      // Sibling-div shadow path: emit one <div> that reproduces the layer's primary fill shape
      // (Rectangle/Ellipse), tinted with the shadow color and CSS-blurred. Avoids the
      // filter-cascade darkening that a `filter:url(...)` on the layer div suffers when the
      // layer has children painting their own filters. Only showBehindLayer=true is covered —
      // false requires an erase-mask that CSS has no direct equivalent for, so it continues
      // down the SVG filter path.
      if (!hasBlendMode && ds->showBehindLayer) {
        ShadowShape shape = findLayerShadowShape(layer);
        if (shape.valid) {
          std::string style = "position:absolute;left:" + FloatToString(shape.left + ds->offsetX) +
                              "px;top:" + FloatToString(shape.top + ds->offsetY) +
                              "px;width:" + FloatToString(shape.width) +
                              "px;height:" + FloatToString(shape.height) +
                              "px;background-color:" + ColorToRGBA(ds->color);
          if (!shape.radius.empty()) {
            style += ";border-radius:" + shape.radius;
          }
          // CSS filter:blur is Gaussian with the given radius; matches feGaussianBlur
          // stdDeviation numerically when the values are equal.
          float blurAvg = (ds->blurX + ds->blurY) * 0.5f;
          if (blurAvg > 0) {
            style += ";filter:blur(" + FloatToString(blurAvg) + "px)";
          }
          pendingSiblingShadows.push_back(style);
          continue;
        }
      }
      {
        std::string fid = _ctx->nextId("filter");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        // Saturate SourceAlpha into a binary silhouette so both the shadow shape and the
        // erase mask below operate on the layer contour, as required by the spec
        // (§5.3.1: "Computes shadow shape based on opaque layer content"; showBehindLayer=false
        // "use layer contour as erase mask"). Without this, semi-transparent fills would
        // produce a weaker shadow and leave partially-visible shadow inside the layer.
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 255 0");
        _defs->addAttr("result", "opaqueAlpha");
        _defs->closeTagSelfClosing();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "opaqueAlpha");
        _defs->addAttr("stdDeviation", FloatToString(ds->blurX) + " " + FloatToString(ds->blurY));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        if (!FloatNearlyZero(ds->offsetX)) {
          _defs->addAttr("dx", FloatToString(ds->offsetX));
        }
        if (!FloatNearlyZero(ds->offsetY)) {
          _defs->addAttr("dy", FloatToString(ds->offsetY));
        }
        _defs->addAttr("result", "off");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "off");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(ds->color.red) + " 0 0 0 0 " +
                                     FloatToString(ds->color.green) + " 0 0 0 0 " +
                                     FloatToString(ds->color.blue) + " 0 0 0 " +
                                     FloatToString(ds->color.alpha) + " 0");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "opaqueAlpha");
          _defs->addAttr("operator", "out");
          _defs->addAttr("result", "shadowClipped");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMerge");
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "shadowClipped");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "SourceGraphic");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
        } else {
          _defs->openTag("feMerge");
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "shadow");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "SourceGraphic");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
        }
        _defs->closeTag();
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      if (hasBlendMode) {
        aboveStyles.push_back({NodeType::InnerShadowStyle, ls});
      } else {
        std::string fid = _ctx->nextId("filter");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        // Invert source alpha -> blur -> offset -> flood -> clip twice. Matches tgfx
        // ImageFilter::InnerShadowOnly that InnerShadowStyle::onDraw uses; the old
        // arithmetic "SourceAlpha - offsetAlpha" only produced a one-sided band that
        // vanished for low-alpha shadow colours (e.g. showcase_infographic #00000010).
        _defs->openTag("feComponentTransfer");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("result", "iInv");
        _defs->closeTagStart();
        _defs->openTag("feFuncA");
        _defs->addAttr("type", "table");
        _defs->addAttr("tableValues", "1 0");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        std::string sd = FloatToString(is->blurX);
        if (is->blurX != is->blurY) {
          sd += " " + FloatToString(is->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iInv");
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur");
        _defs->closeTagSelfClosing();
        std::string blurredResult = "iBlur";
        if (!FloatNearlyZero(is->offsetX) || !FloatNearlyZero(is->offsetY)) {
          _defs->openTag("feOffset");
          _defs->addAttr("in", blurredResult);
          if (!FloatNearlyZero(is->offsetX)) {
            _defs->addAttr("dx", FloatToString(is->offsetX));
          }
          if (!FloatNearlyZero(is->offsetY)) {
            _defs->addAttr("dy", FloatToString(is->offsetY));
          }
          _defs->addAttr("result", "iOff");
          _defs->closeTagSelfClosing();
          blurredResult = "iOff";
        }
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(is->color));
        if (is->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(is->color.alpha));
        }
        _defs->addAttr("result", "iFlood");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "iFlood");
        _defs->addAttr("in2", blurredResult);
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "iShadow");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "iShadow");
        _defs->addAttr("in2", "SourceAlpha");
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "iClip");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMerge");
        _defs->closeTagStart();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "SourceGraphic");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "iClip");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        _defs->closeTag();
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::BackgroundBlurStyle) {
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::BackgroundBlurStyle, ls});
      }
    }
  }

  if (!filterValues.empty()) {
    style += ";filter:" + filterValues;
  }
  // Note: for useMirrorTile we intentionally do NOT add `overflow:hidden` on this outer layer
  // div. The clip is instead applied on an inner wrapper whose bounds are expanded outward by
  // `2 * blur-radius` on each side, so the mirror blur halo can extend beyond the layer's
  // original rect (matching tgfx's BlurFilter output bounds = layer rect outset by 2*sigma),
  // while the flex/layout-relevant outer div keeps its original W x H.
  if (!boxShadowValue.empty()) {
    style += ";border-radius:" + boxShadowBorderRadius;
    style += ";box-shadow:" + boxShadowValue;
  }
  // Now that we know whether the box-shadow fallback fired, apply group opacity. Under the
  // fallback path we switch to child alpha distribution so the layer div itself stays opaque,
  // preserving backdrop-filter semantics.
  if (groupOp && layerAlpha < 1.0f) {
    if (suppressGroupOpacity) {
      childDistribute = true;
    } else {
      style += ";opacity:" + FloatToString(layerAlpha);
    }
  }

  bool needScrollRectWrapper = layer->hasScrollRect;

  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      auto clipId = writeClipDef(layer->mask);
      style += ";clip-path:url(#" + clipId + ")";
    } else {
      style += writeMaskCSS(layer->mask, layer->maskType);
    }
  }

  out.openTag("div");
  out.addAttr("class", "pagx-layer");
  if (!layer->id.empty()) {
    out.addAttr("id", layer->id);
  }
  if (!layer->name.empty()) {
    out.addAttr("data-name", layer->name);
  }
  for (auto& [key, value] : layer->customData) {
    out.addAttr(("data-" + key).c_str(), value);
  }
  out.addAttr("style", style);

  bool hasContent = !layer->contents.empty() || !layer->children.empty();
  if (layer->composition) {
    hasContent = true;
  }
  if (!hasContent) {
    out.closeTagSelfClosing();
    return;
  }

  out.closeTagStart();

  // Emit sibling-div DropShadow shadows first so they sit beneath both belowStyles-driven
  // elements and the layer's own contents/children in painter's order.
  for (const auto& shadowStyle : pendingSiblingShadows) {
    out.openTag("div");
    out.addAttr("style", shadowStyle);
    out.closeTagSelfClosing();
  }

  for (auto& [styleType, ls] : belowStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      std::string shadowStyle = "position:absolute;inset:0";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      bool isUniformBlur = FloatNearlyZero(ds->blurX - ds->blurY);
      if (isUniformBlur && ds->showBehindLayer) {
        shadowStyle += ";filter:drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                       FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
                       ColorToRGBA(ds->color) + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      } else {
        std::string filterId = _ctx->nextId("dsblend");
        _defs->openTag("filter");
        _defs->addAttr("id", filterId);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->closeTagStart();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceGraphic");
        _defs->addAttr("stdDeviation",
                       FloatToString(ds->blurX / 2) + " " + FloatToString(ds->blurY / 2));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        _defs->addAttr("dx", FloatToString(ds->offsetX));
        _defs->addAttr("dy", FloatToString(ds->offsetY));
        _defs->addAttr("result", "offsetBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(ds->color));
        if (ds->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(ds->color.alpha));
        }
        _defs->addAttr("result", "color");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "color");
        _defs->addAttr("in2", "offsetBlur");
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "SourceGraphic");
          _defs->addAttr("operator", "out");
          _defs->addAttr("result", "shadow");
          _defs->closeTagSelfClosing();
        }
        _defs->closeTag();  // </filter>
        shadowStyle += ";filter:url(#" + filterId + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      }
    } else if (styleType == NodeType::BackgroundBlurStyle) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        std::string blurStyle = "position:absolute;inset:0;backdrop-filter:blur(" +
                                FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                FloatToString(avg) + "px)";
        blurStyle += clipPathFromContents(layer);
        if (blendStr) {
          blurStyle += ";mix-blend-mode:";
          blurStyle += blendStr;
        }
        out.openTag("div");
        out.addAttr("style", blurStyle);
        out.closeTagSelfClosing();
      }
    }
  }

  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle && ls->blendMode == BlendMode::Normal) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        std::string blurStyle = "position:absolute;inset:0;backdrop-filter:blur(" +
                                FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                FloatToString(avg) + "px)" + clipPathFromContents(layer);
        // The box-shadow fallback pushes group opacity down to siblings. The backdrop-filter
        // div is its own sibling, so it also needs the distributed alpha; otherwise the blurred
        // backdrop renders at full strength while the glass/inner content appear dimmed.
        if (suppressGroupOpacity && layerAlpha < 1.0f) {
          blurStyle += ";opacity:" + FloatToString(layerAlpha);
        }
        out.openTag("div");
        out.addAttr("style", blurStyle);
        out.closeTagSelfClosing();
      }
    }
  }

  if (needScrollRectWrapper) {
    auto& sr = layer->scrollRect;
    out.openTag("div");
    out.addAttr("style", "position:absolute;left:0px;top:0px;width:" + FloatToString(sr.width) +
                             "px;height:" + FloatToString(sr.height) + "px;overflow:hidden");
    out.closeTagStart();
    out.openTag("div");
    out.addAttr("style", "position:relative;left:" + FloatToString(-sr.x) +
                             "px;top:" + FloatToString(-sr.y) + "px");
    out.closeTagStart();
  }

  float contentAlpha = childDistribute ? layerAlpha : 1.0f;

  if (useMirrorTile) {
    // Simulate BlurFilter.tileMode=Mirror with a 3-layer DOM:
    //   <clip>      position:absolute, left:-margin, top:-margin, size=(W+2m) x (H+2m),
    //               overflow:hidden — the visible blur-halo window (matches tgfx's
    //               filterBounds = rect.makeOutset(2*sigma, 2*sigma)).
    //   <grid>        position:absolute, size=(W+2m) x (H+2m), filter:blur(radius) —
    //                 receives the 9 mirrored tiles; the blur convolves across tile seams.
    //     <tile[0..8]> 9 copies of the layer's inner content, flipped via scale(±1,±1) and
    //                  translated into a 3x3 arrangement. Center tile sits at (margin, margin),
    //                  so after the overflow:hidden clip the visible rect is exactly the
    //                  layer's original W x H plus a `margin`-wide halo on every side.
    auto* bf = static_cast<const BlurFilter*>(layer->filters[0]);
    float blurRadius =
        bf->blurX;  // needsMirrorTiling only matches uniform blur (filters.size()==1)
    // Match tgfx's GaussianBlurImageFilter::onFilterBounds which outsets by 2*sigma. Using the
    // same margin keeps HTML's visible halo bbox consistent with PAGX native rendering.
    float margin = 2.0f * blurRadius;
    float clipW = mirrorTileWidth + 2.0f * margin;
    float clipH = mirrorTileHeight + 2.0f * margin;

    out.openTag("div");
    std::string clipStyle = "position:absolute;left:" + FloatToString(-margin) +
                            "px;top:" + FloatToString(-margin) +
                            "px;width:" + FloatToString(clipW) +
                            "px;height:" + FloatToString(clipH) + "px;overflow:hidden";
    out.addAttr("style", clipStyle);
    out.closeTagStart();

    out.openTag("div");
    std::string gridStyle = "position:absolute;left:0;top:0;width:" + FloatToString(clipW) +
                            "px;height:" + FloatToString(clipH) + "px;filter:blur(" +
                            FloatToString(blurRadius) + "px)";
    out.addAttr("style", gridStyle);
    out.closeTagStart();

    // Tile positions and mirror transforms. Tile wrappers are anchored at (0,0) with size
    // W x H; `transform: translate(tx, ty) scale(sx, sy)` places them in a 3x3 grid whose
    // center tile lands at (margin, margin) inside the clip+grid wrapper. Because CSS applies
    // scale with the default origin (box center), scale(-1,1) flips pixels around the tile's
    // vertical midline without changing its bounding box, and `translate(-W + margin, margin)
    // scale(-1, 1)` places the flipped tile so its right edge (now source x=0) is adjacent to
    // the center tile's left edge (source x=0) — the mirror-repeat seam.
    struct Tile {
      float tx, ty, sx, sy;
    };
    const float W = mirrorTileWidth;
    const float H = mirrorTileHeight;
    const Tile tiles[9] = {
        {-W + margin, -H + margin, -1.0f, -1.0f},  // TL
        {margin, -H + margin, 1.0f, -1.0f},        // T
        {W + margin, -H + margin, -1.0f, -1.0f},   // TR
        {-W + margin, margin, -1.0f, 1.0f},        // L
        {margin, margin, 1.0f, 1.0f},              // C
        {W + margin, margin, -1.0f, 1.0f},         // R
        {-W + margin, H + margin, -1.0f, -1.0f},   // BL
        {margin, H + margin, 1.0f, -1.0f},         // B
        {W + margin, H + margin, -1.0f, -1.0f},    // BR
    };
    for (const auto& t : tiles) {
      out.openTag("div");
      std::string ts = "position:absolute;left:0;top:0;width:" + FloatToString(W) +
                       "px;height:" + FloatToString(H) + "px";
      if (t.tx != 0 || t.ty != 0 || t.sx != 1.0f || t.sy != 1.0f) {
        ts += ";transform:translate(" + FloatToString(t.tx) + "px," + FloatToString(t.ty) + "px)";
        if (t.sx != 1.0f || t.sy != 1.0f) {
          ts += " scale(" + FloatToString(t.sx) + "," + FloatToString(t.sy) + ")";
        }
      }
      out.addAttr("style", ts);
      out.closeTagStart();
      writeLayerInner(out, layer, contentAlpha, childDistribute, isFlexContainer);
      out.closeTag();
    }
    out.closeTag();  // grid wrapper (blur)
    out.closeTag();  // clip wrapper
  } else {
    writeLayerInner(out, layer, contentAlpha, childDistribute, isFlexContainer);
  }

  for (auto& [styleType, ls] : aboveStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      std::string fid = _ctx->nextId("isf");
      _defs->openTag("filter");
      _defs->addAttr("id", fid);
      _defs->addAttr("x", "-50%");
      _defs->addAttr("y", "-50%");
      _defs->addAttr("width", "200%");
      _defs->addAttr("height", "200%");
      _defs->addAttr("color-interpolation-filters", "sRGB");
      _defs->closeTagStart();
      _defs->openTag("feOffset");
      _defs->addAttr("in", "SourceAlpha");
      if (!FloatNearlyZero(is->offsetX)) {
        _defs->addAttr("dx", FloatToString(is->offsetX));
      }
      if (!FloatNearlyZero(is->offsetY)) {
        _defs->addAttr("dy", FloatToString(is->offsetY));
      }
      _defs->addAttr("result", "iOff");
      _defs->closeTagSelfClosing();
      _defs->openTag("feComposite");
      _defs->addAttr("in", "SourceAlpha");
      _defs->addAttr("in2", "iOff");
      _defs->addAttr("operator", "arithmetic");
      _defs->addAttr("k2", "-1");
      _defs->addAttr("k3", "1");
      _defs->addAttr("result", "iComp");
      _defs->closeTagSelfClosing();
      std::string sd = FloatToString(is->blurX);
      if (is->blurX != is->blurY) {
        sd += " " + FloatToString(is->blurY);
      }
      _defs->openTag("feGaussianBlur");
      _defs->addAttr("in", "iComp");
      _defs->addAttr("stdDeviation", sd);
      _defs->addAttr("result", "iBlur");
      _defs->closeTagSelfClosing();
      _defs->openTag("feColorMatrix");
      _defs->addAttr("in", "iBlur");
      _defs->addAttr("type", "matrix");
      _defs->addAttr("values", "0 0 0 0 " + FloatToString(is->color.red) + " 0 0 0 0 " +
                                   FloatToString(is->color.green) + " 0 0 0 0 " +
                                   FloatToString(is->color.blue) + " 0 0 0 " +
                                   FloatToString(is->color.alpha) + " 0");
      _defs->closeTagSelfClosing();
      _defs->closeTag();
      std::string shadowStyle = "position:absolute;inset:0;filter:url(#" + fid + ")";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      out.openTag("div");
      out.addAttr("style", shadowStyle);
      out.closeTagSelfClosing();
    }
  }

  if (needScrollRectWrapper) {
    out.closeTag();  // inner offset div
    out.closeTag();  // clip wrapper div
  }

  out.closeTag();
}

void HTMLWriter::writeLayerInner(HTMLBuilder& out, const Layer* layer, float contentAlpha,
                                 bool childDistribute, bool isFlexContainer) {
  bool hasForeground = false;
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    }
  }

  writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Background);

  if (layer->composition) {
    writeComposition(out, layer->composition, contentAlpha, childDistribute);
  }

  for (auto* child : layer->children) {
    bool childIsFlexItem = isFlexContainer && child->includeInLayout;
    writeLayer(out, child, contentAlpha, childDistribute, childIsFlexItem);
  }

  if (hasForeground) {
    writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Foreground);
  }
}

void HTMLWriter::emitPlusDarkerFilterDef(const PlusDarkerBackdrop& backdrop) {
  // filterUnits/primitiveUnits default to "objectBoundingBox" / "userSpaceOnUse" respectively in
  // SVG, which makes percentage widths refer to the bounding box while primitive widths refer to
  // user-space units (pixels) — a mix that is hard to reason about. We pin both to userSpaceOnUse
  // so all coordinates are plain CSS pixels; the filter region matches the layer's cropped bbox
  // exactly, and feImage's width/height match the backdrop PNG dimensions 1:1.
  _defs->openTag("filter");
  _defs->addAttr("id", backdrop.filterId);
  _defs->addAttr("x", "0");
  _defs->addAttr("y", "0");
  _defs->addAttr("width", FloatToString(backdrop.cropWidth));
  _defs->addAttr("height", FloatToString(backdrop.cropHeight));
  _defs->addAttr("filterUnits", "userSpaceOnUse");
  _defs->addAttr("primitiveUnits", "userSpaceOnUse");
  _defs->addAttr("color-interpolation-filters", "sRGB");
  _defs->closeTagStart();
  _defs->openTag("feImage");
  _defs->addAttr("href", backdrop.backdropDataURL);
  _defs->addAttr("x", "0");
  _defs->addAttr("y", "0");
  _defs->addAttr("width", FloatToString(backdrop.cropWidth));
  _defs->addAttr("height", FloatToString(backdrop.cropHeight));
  _defs->addAttr("preserveAspectRatio", "none");
  _defs->addAttr("result", "bg");
  _defs->closeTagSelfClosing();
  _defs->openTag("feComposite");
  _defs->addAttr("in", "SourceGraphic");
  _defs->addAttr("in2", "bg");
  _defs->addAttr("operator", "arithmetic");
  _defs->addAttr("k1", "0");
  _defs->addAttr("k2", "1");
  _defs->addAttr("k3", "1");
  _defs->addAttr("k4", "-1");
  _defs->closeTagSelfClosing();
  _defs->closeTag();
}

}  // namespace pagx
