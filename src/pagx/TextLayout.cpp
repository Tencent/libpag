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

#include "TextLayout.h"
#include <algorithm>
#include <cmath>
#include "LayoutContext.h"
#include "ShapedText.h"
#include "TextLayoutParams.h"
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "renderer/LineBreaker.h"
#include "renderer/PunctuationSquash.h"
#include "renderer/VerticalTextUtils.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/UTF.h"
#ifdef PAG_USE_HARFBUZZ
#include "renderer/HarfBuzzShaper.h"
#endif
#ifdef PAG_BUILD_PAGX
#include "renderer/BidiResolver.h"
#endif

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyEqual;

static tgfx::Matrix ComputeGroupMatrix(const Group* group) {
  auto matrix = tgfx::Matrix::I();
  matrix.postTranslate(-group->anchor.x, -group->anchor.y);
  matrix.postScale(group->scale.x, group->scale.y);
  if (group->skew != 0.0f) {
    float skewRad = DegreesToRadians(-group->skew);
    float axisRad = DegreesToRadians(group->skewAxis);
    float u = cosf(axisRad);
    float v = sinf(axisRad);
    float w = tanf(skewRad);
    tgfx::Matrix temp = {};
    temp.setAll(u, -v, 0, v, u, 0);
    matrix.postConcat(temp);
    temp.setAll(1, w, 0, 0, 1, 0);
    matrix.postConcat(temp);
    temp.setAll(u, v, 0, -v, u, 0);
    matrix.postConcat(temp);
  }
  matrix.postRotate(group->rotation);
  matrix.postTranslate(group->position.x, group->position.y);
  return matrix;
}

static bool CompareByCluster(const ShapedGlyph& a, const ShapedGlyph& b) {
  return a.cluster < b.cluster;
}

static size_t DecodeUTF8Char(const char* data, size_t remaining, int32_t* unichar) {
  const char* ptr = data;
  const char* end = data + remaining;
  int32_t ch = tgfx::UTF::NextUTF8(&ptr, end);
  if (ch < 0) {
    return 0;
  }
  *unichar = ch;
  return static_cast<size_t>(ptr - data);
}

// Build context that maintains state during text layout
class TextLayoutContext {
 public:
  TextLayoutContext(FontConfig* fontConfig) : fontConfig_(fontConfig) {
  }

  friend class TextLayout;

 private:
  // Single glyph information for multi-line layout.
  struct GlyphInfo {
    tgfx::GlyphID glyphID = 0;
    tgfx::Font font = {};
    float advance = 0;
    float xPosition = 0;
    int32_t unichar = 0;
    float fontSize = 0;
    float ascent = 0;
    float descent = 0;
    float fontLineHeight = 0;
    Text* sourceText = nullptr;
    uint32_t cluster = 0;
    float xOffset = 0;
    float yOffset = 0;
    uint8_t bidiLevel = 0;
  };

  struct LineInfo {
    std::vector<GlyphInfo> glyphs = {};
    float width = 0;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxLineHeight = 0;
    float metricsHeight = 0;
    float roundingRatio = 1.0f;
  };

  // Vertical glyph group with layout metrics. Usually contains a single glyph, but consecutive
  // rotated-group characters (Latin letters, digits, etc.) are collected into one group and rendered
  // as a horizontal run rotated 90 degrees (tate-chu-yoko).
  struct VerticalGlyphInfo {
    std::vector<GlyphInfo> glyphs = {};
    VerticalOrientation orientation = VerticalOrientation::Upright;
    float height = 0;
    float width = 0;
    float leadingSquash = 0;
    bool canBreakBefore = false;
  };

  // Column of vertical text, containing glyphs arranged top to bottom.
  struct ColumnInfo {
    std::vector<VerticalGlyphInfo> glyphs = {};
    float height = 0;
    float maxColumnWidth = 0;
  };

  // Shaped text information for a single Text element.
  struct ShapedInfo {
    Text* text = nullptr;
    float totalWidth = 0;
    std::vector<GlyphInfo> allGlyphs = {};
    bool paragraphRTL = false;
  };

  // A segment of text split by newlines and tabs for shaping.
  struct TextSegment {
    size_t start = 0;
    size_t length = 0;
    bool isNewline = false;
    bool isTab = false;
#ifdef PAG_BUILD_PAGX
    uint8_t bidiLevel = 0;
#endif
  };

  // Splits a text range into segments by newlines and tabs, appending to the output vector.
  static void SplitTextSegments(const std::string& content, size_t rangeStart, size_t rangeLength,
                                uint8_t bidiLevel, std::vector<TextSegment>& segments) {
    size_t rangeEnd = rangeStart + rangeLength;
    size_t pos = rangeStart;
    while (pos < rangeEnd) {
      int32_t ch = 0;
      size_t charLen = DecodeUTF8Char(content.data() + pos, rangeEnd - pos, &ch);
      if (charLen == 0) {
        pos++;
        continue;
      }
      if (ch == '\n') {
        TextSegment seg = {};
        seg.start = pos;
        seg.length = charLen;
        seg.isNewline = true;
        segments.push_back(seg);
        pos += charLen;
      } else if (ch == '\t') {
        TextSegment seg = {};
        seg.start = pos;
        seg.length = charLen;
        seg.isTab = true;
        segments.push_back(seg);
        pos += charLen;
      } else {
        size_t segStart = pos;
        pos += charLen;
        while (pos < rangeEnd) {
          int32_t nextCh = 0;
          size_t nextLen = DecodeUTF8Char(content.data() + pos, rangeEnd - pos, &nextCh);
          if (nextLen == 0) {
            pos++;
            break;
          }
          if (nextCh == '\n' || nextCh == '\t') {
            break;
          }
          pos += nextLen;
        }
        TextSegment seg = {};
        seg.start = segStart;
        seg.length = pos - segStart;
#ifdef PAG_BUILD_PAGX
        seg.bidiLevel = bidiLevel;
#else
        (void)bidiLevel;
#endif
        segments.push_back(seg);
      }
    }
  }

  static GlyphInfo CreateNewlineGlyph(Text* text, const tgfx::Font& font) {
    auto metrics = font.getMetrics();
    GlyphInfo gi = {};
    gi.unichar = '\n';
    gi.fontSize = text->fontSize;
    gi.ascent = metrics.ascent;
    gi.descent = metrics.descent;
    gi.fontLineHeight = std::max(0.0f, fabsf(metrics.ascent) + metrics.descent + metrics.leading);
    gi.sourceText = text;
    return gi;
  }

  static GlyphInfo CreateTabGlyph(Text* text, const tgfx::Font& font, float tabWidth,
                                  float xPosition) {
    float nextTabStop = 0;
    if (tabWidth > 0) {
      nextTabStop = ceilf((xPosition + 1.0f) / tabWidth) * tabWidth;
    }
    float tabAdvance = nextTabStop - xPosition;
    if (tabAdvance < 0) {
      tabAdvance = 0;
    }
    auto metrics = font.getMetrics();
    GlyphInfo gi = {};
    gi.unichar = '\t';
    gi.advance = tabAdvance;
    gi.xPosition = xPosition;
    gi.fontSize = text->fontSize;
    gi.ascent = metrics.ascent;
    gi.descent = metrics.descent;
    gi.fontLineHeight = std::max(0.0f, fabsf(metrics.ascent) + metrics.descent + metrics.leading);
    gi.sourceText = text;
    return gi;
  }

  static void collectTextElementsWithMatrices(const std::vector<Element*>& elements,
                                              std::vector<Text*>& outText,
                                              std::vector<tgfx::Matrix>& outMatrices,
                                              const tgfx::Matrix& parentMatrix) {
    for (auto* element : elements) {
      if (element->nodeType() == NodeType::Text) {
        auto* text = static_cast<Text*>(element);
        auto textMatrix = parentMatrix;
        textMatrix.postTranslate(text->position.x, text->position.y);
        outText.push_back(text);
        outMatrices.push_back(textMatrix);
      } else if (element->nodeType() == NodeType::Group ||
                 element->nodeType() == NodeType::TextBox) {
        auto* group = static_cast<Group*>(element);
        auto groupMatrix = parentMatrix;
        groupMatrix.postConcat(ComputeGroupMatrix(group));
        collectTextElementsWithMatrices(group->elements, outText, outMatrices, groupMatrix);
      }
    }
  }

  TextLayoutResult processTextWithLayout(std::vector<Text*>& textElements,
                                         const TextLayoutParams& params) {
    // Shape each Text and concatenate all glyphs for unified layout within the TextBox.
    std::vector<GlyphInfo> allGlyphs = {};
    size_t estimatedGlyphCount = 0;
    for (auto* text : textElements) {
      estimatedGlyphCount += text->text.size();
    }
    allGlyphs.reserve(estimatedGlyphCount);
    float totalWidth = 0;
    bool paragraphRTL = false;
    bool directionResolved = false;
    for (auto* text : textElements) {
      ShapedInfo info = {};
      info.text = text;
      if (!text->text.empty()) {
        shapeText(text, info, params.writingMode == WritingMode::Vertical);
        if (!directionResolved) {
          paragraphRTL = info.paragraphRTL;
          directionResolved = true;
        }
      }
      for (auto& g : info.allGlyphs) {
        if (g.unichar != '\n') {
          g.xPosition += totalWidth;
        }
      }
      allGlyphs.insert(allGlyphs.end(), info.allGlyphs.begin(), info.allGlyphs.end());
      totalWidth += info.totalWidth;
    }
    TextLayoutResult result = {};
    if (allGlyphs.empty()) {
      return result;
    }
    if (params.writingMode == WritingMode::Vertical) {
      auto columns = layoutColumns(allGlyphs, params);
      buildTextBlobVertical(params, columns, result);
      float totalColumnWidth = 0;
      float maxColumnHeight = 0;
      for (auto& col : columns) {
        totalColumnWidth += col.maxColumnWidth;
        maxColumnHeight = std::max(maxColumnHeight, col.height);
      }
      // Compute anchor offset for standalone Text in vertical mode (no explicit box).
      float anchorY = 0;
      if (std::isnan(params.boxHeight)) {
        switch (params.textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            anchorY = -maxColumnHeight / 2;
            break;
          case TextAlign::End:
            anchorY = -maxColumnHeight;
            break;
          default:
            break;
        }
      }
      result.bounds = Rect::MakeXYWH(0, anchorY, totalColumnWidth, maxColumnHeight);
    } else {
      auto lines = layoutLines(allGlyphs, params);
      buildTextBlobWithLayout(params, lines, result, paragraphRTL);
      float maxLineWidth = 0;
      float totalHeight = 0;
      for (auto& line : lines) {
        maxLineWidth = std::max(maxLineWidth, line.width);
        totalHeight += line.maxLineHeight;
      }
      // Compute anchor offset so bounds reflect the text origin based on textAlign.
      // Compute anchor offset for standalone Text (no explicit box) so bounds reflect
      // the text origin based on textAlign. When boxWidth is set, bounds start at (0,0).
      float anchorX = 0;
      if (std::isnan(params.boxWidth)) {
        switch (params.textAlign) {
          case TextAlign::Start:
            break;
          case TextAlign::Center:
            anchorX = -maxLineWidth / 2;
            break;
          case TextAlign::End:
            anchorX = -maxLineWidth;
            break;
          default:
            break;
        }
      }
      result.bounds = Rect::MakeXYWH(anchorX, 0, maxLineWidth, totalHeight);
    }
    // Build layoutGlyphRuns from positioned glyph data, grouping consecutive same-font glyphs.
    BuildLayoutGlyphRuns(result);
    return result;
  }

  void shapeText(Text* text, ShapedInfo& info, bool vertical = false) {
    auto primaryTypeface = findTypeface(text->fontFamily, text->fontStyle);
    if (primaryTypeface == nullptr) {
      return;
    }

    tgfx::Font primaryFont(primaryTypeface, text->fontSize);
    primaryFont.setFauxBold(text->fauxBold);
    primaryFont.setFauxItalic(text->fauxItalic);
    float currentX = 0;
    const std::string& content = text->text;
    bool hasLetterSpacing = !FloatNearlyEqual(text->letterSpacing, 0.0f);

#ifdef PAG_USE_HARFBUZZ
    // Build fallback fonts list for HarfBuzz shaping.
    std::vector<tgfx::Font> fallbackFonts = {};
    auto* fp = fontConfig_;
    if (fp != nullptr) {
      auto& holders = fp->fallbackTypefaces;
      fallbackFonts.reserve(holders.size());
      for (auto& holder : holders) {
        auto fallback = holder.getTypeface();
        if (fallback != nullptr && fallback != primaryTypeface) {
          tgfx::Font fallbackFont(fallback, text->fontSize);
          fallbackFont.setFauxBold(text->fauxBold);
          fallbackFont.setFauxItalic(text->fauxItalic);
          fallbackFonts.push_back(std::move(fallbackFont));
        }
      }
    }

    // Collect newline and tab positions, then shape non-special segments.
    std::vector<TextSegment> segments = {};

#ifdef PAG_BUILD_PAGX
    // Use BiDi resolver to split text into directional runs, then further split by newlines/tabs.
    auto bidiResult = BidiResolver::Resolve(content);
    info.paragraphRTL = bidiResult.isRTL;
    for (auto& run : bidiResult.runs) {
      SplitTextSegments(content, run.start, run.length, run.level, segments);
    }
#else
    SplitTextSegments(content, 0, content.size(), 0, segments);
#endif

    std::shared_ptr<tgfx::Typeface> currentTypeface = nullptr;
    float tabWidth = text->fontSize * 4;

    for (auto& seg : segments) {
      if (seg.isNewline) {
        info.allGlyphs.push_back(CreateNewlineGlyph(text, primaryFont));
        currentX = 0;
        currentTypeface = nullptr;
        continue;
      }

      if (seg.isTab) {
        auto tabGlyph = CreateTabGlyph(text, primaryFont, tabWidth, currentX);
        currentX += tabGlyph.advance;
        info.allGlyphs.push_back(std::move(tabGlyph));
        currentTypeface = nullptr;
        continue;
      }

      // Shape this text segment with HarfBuzz.
      auto substring = content.substr(seg.start, seg.length);
      bool rtl = false;
#ifdef PAG_BUILD_PAGX
      rtl = seg.bidiLevel & 1;
#endif
      auto shapedGlyphs =
          HarfBuzzShaper::Shape(substring, primaryFont, fallbackFonts, vertical, rtl);

#ifdef PAG_BUILD_PAGX
      // HarfBuzz returns RTL glyphs in visual order (left-to-right). Sort them by cluster
      // to restore logical order so that allGlyphs is always in logical order. Simple reverse
      // is insufficient because HarfBuzz places neutral characters (spaces, punctuation) at
      // visual positions that don't correspond to a simple reversal of the logical sequence.
      // Visual reordering is done later via the UAX#9 L2 algorithm when building the TextBlob.
      if (rtl) {
        std::sort(shapedGlyphs.begin(), shapedGlyphs.end(), CompareByCluster);
      }
#endif

      for (auto& sg : shapedGlyphs) {
        if (sg.glyphID == 0) {
          continue;
        }

        auto glyphTypeface = sg.font.getTypeface();
        if (currentTypeface == nullptr || glyphTypeface != currentTypeface) {
          currentTypeface = glyphTypeface;
        }

        auto metrics = sg.font.getMetrics();
        // Decode the unichar at this cluster position for line breaking and other logic.
        int32_t unichar = 0;
        size_t clusterByteOffset = seg.start + sg.cluster;
        if (clusterByteOffset < content.size()) {
          DecodeUTF8Char(content.data() + clusterByteOffset, content.size() - clusterByteOffset,
                         &unichar);
        }

        GlyphInfo gi = {};
        gi.glyphID = sg.glyphID;
        gi.font = sg.font;
        gi.advance = sg.xAdvance;
        gi.xPosition = currentX;
        gi.unichar = unichar;
        gi.fontSize = text->fontSize;
        gi.ascent = metrics.ascent;
        gi.descent = metrics.descent;
        gi.fontLineHeight =
            std::max(0.0f, fabsf(metrics.ascent) + metrics.descent + metrics.leading);
        gi.sourceText = text;
        gi.cluster = static_cast<uint32_t>(seg.start) + sg.cluster;
        gi.xOffset = sg.xOffset;
        gi.yOffset = sg.yOffset;
#ifdef PAG_BUILD_PAGX
        gi.bidiLevel = seg.bidiLevel;
#endif
        info.allGlyphs.push_back(gi);

        currentX += sg.xAdvance + text->letterSpacing;
      }
    }

#else
    // Non-HarfBuzz path: original per-character glyph lookup.

    // Fallback font cache: built on demand as fallback typefaces are loaded during glyph lookup.
    std::unordered_map<tgfx::Typeface*, tgfx::Font> fallbackFontCache = {};

    std::shared_ptr<tgfx::Typeface> currentTypeface = nullptr;

    size_t i = 0;
    while (i < content.size()) {
      int32_t unichar = 0;
      size_t charLen = DecodeUTF8Char(content.data() + i, content.size() - i, &unichar);
      if (charLen == 0) {
        i++;
        continue;
      }
      i += charLen;

      // Handle newline: store font metrics so \n participates in line metrics calculation
      // when it becomes the leading cluster of the next line.
      if (unichar == '\n') {
        info.allGlyphs.push_back(CreateNewlineGlyph(text, primaryFont));
        currentX = 0;
        currentTypeface = nullptr;
        continue;
      }

      // Handle tab character.
      if (unichar == '\t') {
        float tabWidth = text->fontSize * 4;
        auto tabGlyph = CreateTabGlyph(text, primaryFont, tabWidth, currentX);
        currentX += tabGlyph.advance;
        info.allGlyphs.push_back(std::move(tabGlyph));
        currentTypeface = nullptr;
        continue;
      }

      // Try to find glyph in primary font or fallbacks
      tgfx::GlyphID glyphID = primaryFont.getGlyphID(unichar);
      tgfx::Font glyphFont = primaryFont;
      std::shared_ptr<tgfx::Typeface> glyphTypeface = primaryTypeface;

      if (glyphID == 0) {
        auto& fbHolders = fontConfig_->fallbackTypefaces;
        for (auto& holder : fbHolders) {
          auto fallback = holder.getTypeface();
          if (fallback == nullptr || fallback == primaryTypeface) {
            continue;
          }
          auto it = fallbackFontCache.find(fallback.get());
          if (it == fallbackFontCache.end()) {
            tgfx::Font f(fallback, text->fontSize);
            f.setFauxBold(text->fauxBold);
            f.setFauxItalic(text->fauxItalic);
            it = fallbackFontCache.emplace(fallback.get(), std::move(f)).first;
          }
          glyphID = it->second.getGlyphID(unichar);
          if (glyphID != 0) {
            glyphFont = it->second;
            glyphTypeface = fallback;
            break;
          }
        }
      }

      if (glyphID == 0) {
        continue;
      }

      float advance = glyphFont.getAdvance(glyphID);

      // Start new run if typeface changed
      if (currentTypeface != glyphTypeface) {
        currentTypeface = glyphTypeface;
      }

      auto metrics = glyphFont.getMetrics();
      GlyphInfo gi = {};
      gi.glyphID = glyphID;
      gi.font = glyphFont;
      gi.advance = advance;
      gi.xPosition = currentX;
      gi.unichar = unichar;
      gi.fontSize = text->fontSize;
      gi.ascent = metrics.ascent;
      gi.descent = metrics.descent;
      gi.fontLineHeight = std::max(0.0f, fabsf(metrics.ascent) + metrics.descent + metrics.leading);
      gi.sourceText = text;
      info.allGlyphs.push_back(gi);

      currentX += advance + text->letterSpacing;
    }
#endif

    // Remove the extra letterSpacing after the last glyph.
    if (hasLetterSpacing && currentX > 0) {
      currentX -= text->letterSpacing;
    }
    info.totalWidth = currentX;
  }

  std::vector<LineInfo> layoutLines(const std::vector<GlyphInfo>& allGlyphs,
                                    const TextLayoutParams& params) {
    std::vector<LineInfo> lines = {};
    lines.emplace_back();
    auto* currentLine = &lines.back();
    float currentLineWidth = 0;
    int lastBreakIndex = -1;
    bool doWrap = params.wordWrap && !std::isnan(params.boxWidth) && params.boxWidth > 0;
    // Tracks the fontLineHeight of the \n that created the current line.
    // Used as the fallback height for empty lines (e.g. consecutive \n\n).
    float pendingNewlineFontLineHeight = 0.0f;

    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];

      if (glyph.unichar == '\n') {
        FinishLine(currentLine, params.lineHeight, pendingNewlineFontLineHeight);
        lines.emplace_back();
        currentLine = &lines.back();
        currentLineWidth = 0;
        lastBreakIndex = -1;
        pendingNewlineFontLineHeight = glyph.fontLineHeight;
        continue;
      }

      float letterSpacing = (glyph.sourceText != nullptr) ? glyph.sourceText->letterSpacing : 0;
      float glyphEndX = currentLineWidth + glyph.advance;

      // Auto-wrap check
      if (doWrap && !currentLine->glyphs.empty() && glyphEndX > params.boxWidth) {
        if (lastBreakIndex >= 0) {
          // Split at break point: move glyphs after lastBreakIndex to new line
          std::vector<GlyphInfo> overflow(currentLine->glyphs.begin() + lastBreakIndex + 1,
                                          currentLine->glyphs.end());
          currentLine->glyphs.resize(lastBreakIndex + 1);
          // Trim trailing whitespace from current line
          while (!currentLine->glyphs.empty() &&
                 LineBreaker::IsWhitespace(currentLine->glyphs.back().unichar)) {
            currentLine->glyphs.pop_back();
          }
          FinishLine(currentLine, params.lineHeight, 0.0f);
          lines.emplace_back();
          currentLine = &lines.back();
          // Skip leading whitespace in overflow
          size_t skipCount = 0;
          while (skipCount < overflow.size() &&
                 LineBreaker::IsWhitespace(overflow[skipCount].unichar)) {
            skipCount++;
          }
          // Recalculate positions for overflow glyphs
          currentLineWidth = 0;
          for (size_t j = skipCount; j < overflow.size(); j++) {
            overflow[j].xPosition = currentLineWidth;
            float ls =
                (overflow[j].sourceText != nullptr) ? overflow[j].sourceText->letterSpacing : 0;
            currentLineWidth += overflow[j].advance + ls;
            currentLine->glyphs.push_back(overflow[j]);
          }
          // Add current glyph
          GlyphInfo adjusted = glyph;
          adjusted.xPosition = currentLineWidth;
          currentLineWidth += glyph.advance + letterSpacing;
          currentLine->glyphs.push_back(adjusted);
          // Re-scan break opportunities among glyphs on the new line.
          lastBreakIndex = -1;
          for (size_t j = 0; j + 1 < currentLine->glyphs.size(); j++) {
            auto& prev = currentLine->glyphs[j];
            auto& next = currentLine->glyphs[j + 1];
#ifdef PAG_USE_HARFBUZZ
            bool sameCluster =
                (prev.cluster != 0 || next.cluster != 0) && prev.cluster == next.cluster;
            if (!sameCluster && LineBreaker::CanBreakBetween(prev.unichar, next.unichar)) {
              lastBreakIndex = static_cast<int>(j);
            }
#else
            if (LineBreaker::CanBreakBetween(prev.unichar, next.unichar)) {
              lastBreakIndex = static_cast<int>(j);
            }
#endif
          }
          // Also check break between the last glyph and the next unprocessed glyph.
          if (i + 1 < allGlyphs.size() && allGlyphs[i + 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
            bool sameCluster = (glyph.cluster != 0 || allGlyphs[i + 1].cluster != 0) &&
                               glyph.cluster == allGlyphs[i + 1].cluster;
            if (!sameCluster &&
                LineBreaker::CanBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
              lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
            }
#else
            if (LineBreaker::CanBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
              lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
            }
#endif
          }
          continue;
        } else {
          // No break point found - force break before current glyph
          if (!currentLine->glyphs.empty()) {
            FinishLine(currentLine, params.lineHeight, 0.0f);
            lines.emplace_back();
            currentLine = &lines.back();
          }
          GlyphInfo adjusted = glyph;
          adjusted.xPosition = 0;
          currentLine->glyphs.push_back(adjusted);
          currentLineWidth = glyph.advance + letterSpacing;
        }
        lastBreakIndex = -1;
        continue;
      }

      GlyphInfo adjusted = glyph;
      adjusted.xPosition = currentLineWidth;
      currentLine->glyphs.push_back(adjusted);
      currentLineWidth += glyph.advance + letterSpacing;

      // Update break opportunity
      if (i + 1 < allGlyphs.size() && allGlyphs[i + 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
        // Cluster-aware breaking: never break within the same HarfBuzz cluster.
        bool sameCluster = (glyph.cluster != 0 || allGlyphs[i + 1].cluster != 0) &&
                           glyph.cluster == allGlyphs[i + 1].cluster;
        if (!sameCluster && LineBreaker::CanBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
        }
#else
        if (LineBreaker::CanBreakBetween(glyph.unichar, allGlyphs[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(currentLine->glyphs.size()) - 1;
        }
#endif
      }
    }

    FinishLine(currentLine, params.lineHeight, pendingNewlineFontLineHeight);

#ifdef PAG_BUILD_PAGX
    // Apply punctuation squash to all lines.
    ApplyPunctuationSquashToLines(lines);
#endif

    return lines;
  }

#ifdef PAG_BUILD_PAGX
  static void ApplyPunctuationSquashToLines(std::vector<LineInfo>& lines) {
    for (auto& line : lines) {
      if (line.glyphs.empty()) {
        continue;
      }

      auto glyphCount = line.glyphs.size();
      // Leading squash removes whitespace before the glyph face (Opening punctuation).
      // Trailing squash removes whitespace after the glyph face (Closing punctuation).
      std::vector<float> leadingSquash(glyphCount, 0);
      std::vector<float> trailingSquash(glyphCount, 0);

      // Line-start squash: remove leading whitespace of the first non-whitespace glyph.
      // Only Opening punctuation has leading whitespace.
      for (size_t i = 0; i < glyphCount; i++) {
        if (LineBreaker::IsWhitespace(line.glyphs[i].unichar)) {
          continue;
        }
        float fraction = PunctuationSquash::GetLineStartSquash(line.glyphs[i].unichar);
        leadingSquash[i] = line.glyphs[i].advance * fraction;
        break;
      }

      // Line-end squash: remove trailing whitespace of the last non-whitespace glyph.
      // Only Closing punctuation has trailing whitespace.
      for (int i = static_cast<int>(glyphCount) - 1; i >= 0; i--) {
        if (LineBreaker::IsWhitespace(line.glyphs[i].unichar)) {
          continue;
        }
        float fraction = PunctuationSquash::GetLineEndSquash(line.glyphs[i].unichar);
        trailingSquash[i] = line.glyphs[i].advance * fraction;
        break;
      }

      // Adjacent punctuation squash. The squash type depends on where each character's internal
      // whitespace is:
      //   - Opening punctuation: whitespace on the leading (left) side
      //   - Closing punctuation: whitespace on the trailing (right) side
      //   - MiddleDot: whitespace on both sides
      // prevSquash removes the trailing whitespace of prevChar.
      // nextSquash removes the leading whitespace of nextChar.
      for (size_t i = 0; i + 1 < glyphCount; i++) {
        auto result = PunctuationSquash::GetAdjacentSquash(line.glyphs[i].unichar,
                                                           line.glyphs[i + 1].unichar);
        if (result.prevSquash > 0) {
          float amount = line.glyphs[i].advance * result.prevSquash;
          // Both Opening (leading whitespace mapped to trailing squash) and Closing/MiddleDot
          // (trailing whitespace) reduce the effective advance via trailingSquash.
          if (amount > trailingSquash[i]) {
            trailingSquash[i] = amount;
          }
        }
        if (result.nextSquash > 0) {
          float amount = line.glyphs[i + 1].advance * result.nextSquash;
          // All categories (Opening, Closing, MiddleDot) remove leading whitespace of nextChar.
          if (amount > leadingSquash[i + 1]) {
            leadingSquash[i + 1] = amount;
          }
        }
      }

      // Recalculate positions with squash applied.
      // Leading squash shifts the glyph's xPosition backward (removes space before the glyph).
      // Trailing squash reduces the effective advance (removes space after the glyph).
      float xPos = 0;
      for (size_t i = 0; i < glyphCount; i++) {
        line.glyphs[i].xPosition = xPos - leadingSquash[i];
        float effectiveAdvance = line.glyphs[i].advance - leadingSquash[i] - trailingSquash[i];
        float ls =
            (line.glyphs[i].sourceText != nullptr) ? line.glyphs[i].sourceText->letterSpacing : 0;
        xPos += effectiveAdvance + ls;
      }

      // Recalculate line width.
      if (!line.glyphs.empty()) {
        auto& lastGlyph = line.glyphs.back();
        float lastEffectiveAdvance =
            lastGlyph.advance - leadingSquash[glyphCount - 1] - trailingSquash[glyphCount - 1];
        line.width = lastGlyph.xPosition + leadingSquash[glyphCount - 1] + lastEffectiveAdvance;
      }
    }
  }
#endif

  static void FinishLine(LineInfo* line, float lineHeight, float newlineFontLineHeight) {
    if (line->glyphs.empty()) {
      // Empty line from line breaks: use the newline font metrics for height.
      if (lineHeight > 0) {
        line->maxLineHeight = lineHeight;
        line->roundingRatio = 1.0f;
      } else if (newlineFontLineHeight > 0) {
        line->maxLineHeight = roundf(newlineFontLineHeight);
        line->roundingRatio = line->maxLineHeight / newlineFontLineHeight;
      }
      return;
    }
    auto& lastGlyph = line->glyphs.back();
    line->width = lastGlyph.xPosition + lastGlyph.advance;
    float maxAscent = 0;
    float maxDescent = 0;
    float maxFontLineHeight = 0;
    for (auto& g : line->glyphs) {
      float absAscent = fabsf(g.ascent);
      if (absAscent > maxAscent) {
        maxAscent = absAscent;
      }
      if (g.descent > maxDescent) {
        maxDescent = g.descent;
      }
      if (g.fontLineHeight > maxFontLineHeight) {
        maxFontLineHeight = g.fontLineHeight;
      }
    }
    line->maxAscent = maxAscent;
    line->maxDescent = maxDescent;
    line->metricsHeight = maxFontLineHeight;
    if (lineHeight > 0) {
      line->maxLineHeight = lineHeight;
      line->roundingRatio = 1.0f;
    } else if (line->metricsHeight > 0) {
      line->maxLineHeight = roundf(line->metricsHeight);
      line->roundingRatio = line->maxLineHeight / line->metricsHeight;
    }
  }

  // Build TextLayoutGlyphRun lists from positioned glyph data, grouping consecutive same-font
  // glyphs into runs. This produces the canonical layout output format used by FontEmbedder and
  // GlyphRunRenderer.
  static void BuildLayoutGlyphRuns(TextLayoutResult& result) {
    // Horizontal glyphs: group by (sourceText, font) runs.
    for (auto& [text, glyphs] : result.horizontalGlyphs) {
      if (glyphs.empty()) {
        continue;
      }
      auto& runs = result.layoutGlyphRuns[text];
      size_t runStart = 0;
      while (runStart < glyphs.size()) {
        auto& runFont = glyphs[runStart].font;
        size_t runEnd = runStart + 1;
        while (runEnd < glyphs.size() && glyphs[runEnd].font == runFont) {
          runEnd++;
        }
        TextLayoutGlyphRun run = {};
        run.font = runFont;
        size_t count = runEnd - runStart;
        run.glyphs.reserve(count);
        run.positions.reserve(count);
        for (size_t i = runStart; i < runEnd; i++) {
          run.glyphs.push_back(glyphs[i].glyphID);
          run.positions.push_back(tgfx::Point::Make(glyphs[i].x, glyphs[i].y));
        }
        runs.push_back(std::move(run));
        runStart = runEnd;
      }
    }
    // Vertical glyphs: group by (sourceText, font, useRSXform) runs.
    for (auto& [text, glyphs] : result.verticalGlyphs) {
      if (glyphs.empty()) {
        continue;
      }
      auto& runs = result.layoutGlyphRuns[text];
      size_t runStart = 0;
      while (runStart < glyphs.size()) {
        auto& runFont = glyphs[runStart].font;
        bool runUseRSXform = glyphs[runStart].useRSXform;
        size_t runEnd = runStart + 1;
        while (runEnd < glyphs.size() && glyphs[runEnd].font == runFont &&
               glyphs[runEnd].useRSXform == runUseRSXform) {
          runEnd++;
        }
        TextLayoutGlyphRun run = {};
        run.font = runFont;
        size_t count = runEnd - runStart;
        run.glyphs.reserve(count);
        run.positions.reserve(count);
        if (runUseRSXform) {
          run.xforms.reserve(count);
        }
        for (size_t i = runStart; i < runEnd; i++) {
          run.glyphs.push_back(glyphs[i].glyphID);
          if (runUseRSXform) {
            run.positions.push_back(tgfx::Point::Make(glyphs[i].xform.tx, glyphs[i].xform.ty));
            run.xforms.push_back(glyphs[i].xform);
          } else {
            run.positions.push_back(glyphs[i].position);
          }
        }
        runs.push_back(std::move(run));
        runStart = runEnd;
      }
    }
  }

  void buildTextBlobWithLayout(const TextLayoutParams& params, const std::vector<LineInfo>& lines,
                               TextLayoutResult& result, bool paragraphRTL = false) {
    if (lines.empty()) {
      return;
    }

    // Calculate total height using line-box model: each line contributes its full lineHeight.
    float totalHeight = 0;
    for (size_t i = 0; i < lines.size(); i++) {
      totalHeight += lines[i].maxLineHeight;
    }

    // Vertical alignment offset.
    float yOffset = 0;
    if (!std::isnan(params.boxHeight)) {
      switch (params.paragraphAlign) {
        case ParagraphAlign::Near:
          yOffset = 0;
          break;
        case ParagraphAlign::Middle:
          yOffset = (params.boxHeight - totalHeight) / 2;
          break;
        case ParagraphAlign::Far:
          yOffset = params.boxHeight - totalHeight;
          break;
      }
    } else {
      switch (params.paragraphAlign) {
        case ParagraphAlign::Near:
          yOffset = 0;
          break;
        case ParagraphAlign::Middle:
          yOffset = -totalHeight / 2;
          break;
        case ParagraphAlign::Far:
          yOffset = -totalHeight;
          break;
      }
    }

    // Alphabetic baseline: shift so position.y = alphabetic baseline (first line).
    // In the linebox model, the baseline is at halfLeading + ascent from the linebox top.
    // Alphabetic mode subtracts this offset so position.y directly represents the baseline.
    if (params.baseline == TextBaseline::Alphabetic && !lines.empty()) {
      auto& firstLine = lines[0];
      if (firstLine.metricsHeight > 0) {
        float halfLeading = (firstLine.maxLineHeight - firstLine.metricsHeight) / 2;
        yOffset -= (halfLeading + firstLine.maxAscent) * firstLine.roundingRatio;
      }
    }

    // Use result.horizontalGlyphs to collect positioned glyphs grouped by source Text element.

    bool overflowHidden = params.overflow == Overflow::Hidden;
    float boxBottom = params.boxHeight;
    // Use relative coordinates for baseline calculation, then add params position at the end.
    float relativeTop = 0;
    float baselineY = 0;
    // Track the previous line's relative baseline for the equal-spacing model used by subsequent
    // lines with fixed line height: baseline(n) = baseline(n-1) + lineHeight(n-1).
    float prevRelativeBaseline = 0;
    bool hasPrevBaseline = false;

    // For Bottom and Center alignment, pre-compute baselines from the last line upward so that the
    // last line is anchored at the box bottom and preceding lines are spaced by lineHeight. This
    // ensures correct baseline positions when lines have different heights (mixed font sizes).
    std::vector<float> precomputedBaselines = {};
    if ((params.paragraphAlign == ParagraphAlign::Far ||
         params.paragraphAlign == ParagraphAlign::Middle) &&
        lines.size() > 1) {
      precomputedBaselines.resize(lines.size(), 0);
      // Start from the last content line.
      auto lastIdx = lines.size() - 1;
      auto& lastLine = lines[lastIdx];
      float lastRelBaseline = 0;
      if (!lastLine.glyphs.empty()) {
        float halfLeading = (lastLine.maxLineHeight - lastLine.metricsHeight) / 2;
        lastRelBaseline =
            (totalHeight - lastLine.maxLineHeight + halfLeading + lastLine.maxAscent) *
            lastLine.roundingRatio;
      } else {
        lastRelBaseline = (totalHeight - lastLine.maxLineHeight + lastLine.maxLineHeight) *
                          lastLine.roundingRatio;
      }
      precomputedBaselines[lastIdx] = lastRelBaseline;
      // Walk upward: each preceding line's baseline = next line's baseline - next line's lineHeight
      for (int i = static_cast<int>(lastIdx) - 1; i >= 0; i--) {
        precomputedBaselines[i] = precomputedBaselines[i + 1] - lines[i + 1].maxLineHeight;
      }
    }

    for (size_t lineIdx = 0; lineIdx < lines.size(); lineIdx++) {
      auto& line = lines[lineIdx];
      float relativeBaseline = 0;

      if (line.glyphs.empty()) {
        relativeBaseline = (relativeTop + line.maxLineHeight) * line.roundingRatio;
        baselineY = roundf(relativeBaseline + yOffset);
      } else if (!precomputedBaselines.empty()) {
        // Bottom/Center alignment: use pre-computed baselines anchored from the last line.
        relativeBaseline = precomputedBaselines[lineIdx];
        baselineY = roundf(relativeBaseline + yOffset);
      } else if (hasPrevBaseline && params.lineHeight > 0) {
        // Fixed line height, subsequent lines: baseline = prevBaseline + lineHeight.
        // This produces equal baseline-to-baseline spacing matching Figma's behavior where
        // subsequent lines have their leading added above rather than split above and below.
        relativeBaseline = prevRelativeBaseline + lines[lineIdx - 1].maxLineHeight;
        baselineY = roundf(relativeBaseline + yOffset);
      } else {
        // Auto line height or first content line: use half-leading model.
        float halfLeading = (line.maxLineHeight - line.metricsHeight) / 2;
        relativeBaseline = (relativeTop + halfLeading + line.maxAscent) * line.roundingRatio;
        baselineY = roundf(relativeBaseline + yOffset);
      }
      prevRelativeBaseline = relativeBaseline;
      hasPrevBaseline = !line.glyphs.empty();
      relativeTop += line.maxLineHeight;

      // Skip lines that overflow below the box bottom.
      if (overflowHidden && !std::isnan(params.boxHeight)) {
        float lineBottom = baselineY + line.maxDescent;
        if (lineBottom > boxBottom) {
          break;
        }
      }

      // Horizontal alignment. In RTL paragraphs, Start means right-aligned and End means
      // left-aligned.
      float xOffset = 0;
      float justifyExtraPerGap = 0;
      bool isStartAligned = (params.textAlign == TextAlign::Start && !paragraphRTL) ||
                            (params.textAlign == TextAlign::End && paragraphRTL);
      bool isEndAligned = (params.textAlign == TextAlign::End && !paragraphRTL) ||
                          (params.textAlign == TextAlign::Start && paragraphRTL);
      float effectiveBoxWidth = std::isnan(params.boxWidth) ? 0.0f : params.boxWidth;
      if (isStartAligned) {
        // Left-aligned (LTR Start or RTL End): no offset.
      } else if (params.textAlign == TextAlign::Center) {
        xOffset += (effectiveBoxWidth - line.width) / 2;
      } else if (isEndAligned) {
        xOffset += effectiveBoxWidth - line.width;
      } else if (params.textAlign == TextAlign::Justify && !std::isnan(params.boxWidth)) {
        // Justify: distribute extra space at word boundaries. Last line uses Start alignment.
        // Note: under PAG_BUILD_PAGX, gap counting is deferred until after L2 BiDi reorder
        // so that it operates on the same visual glyph order used during gap application.
        if (lineIdx < lines.size() - 1 && line.glyphs.size() > 1) {
#ifndef PAG_BUILD_PAGX
          int gapCount = 0;
          for (size_t i = 0; i + 1 < line.glyphs.size(); i++) {
            if (LineBreaker::CanBreakBetween(line.glyphs[i].unichar, line.glyphs[i + 1].unichar)) {
              gapCount++;
            }
          }
          if (gapCount > 0) {
            justifyExtraPerGap = (params.boxWidth - line.width) / static_cast<float>(gapCount);
          }
#endif
        } else if (paragraphRTL) {
          xOffset += params.boxWidth - line.width;
        }
      }

      float justifyOffset = 0;
#ifdef PAG_BUILD_PAGX
      // UAX#9 L2: reorder glyphs from logical order to visual order for rendering.
      // Reverse contiguous runs of glyphs whose bidi level >= current level, starting from the
      // maximum level down to 1. After reordering, recalculate xPosition for each glyph.
      auto visualGlyphs = line.glyphs;
      {
        uint8_t maxLevel = 0;
        for (auto& g : visualGlyphs) {
          if (g.bidiLevel > maxLevel) {
            maxLevel = g.bidiLevel;
          }
        }
        for (uint8_t level = maxLevel; level > 0; level--) {
          size_t idx = 0;
          while (idx < visualGlyphs.size()) {
            if (visualGlyphs[idx].bidiLevel >= level) {
              size_t start = idx;
              while (idx < visualGlyphs.size() && visualGlyphs[idx].bidiLevel >= level) {
                idx++;
              }
              std::reverse(visualGlyphs.begin() + static_cast<ptrdiff_t>(start),
                           visualGlyphs.begin() + static_cast<ptrdiff_t>(idx));
            } else {
              idx++;
            }
          }
        }
        // Recalculate xPosition after visual reordering.
        float xPos = 0;
        for (auto& g : visualGlyphs) {
          g.xPosition = xPos;
          float letterSpacing = (g.sourceText != nullptr) ? g.sourceText->letterSpacing : 0;
          xPos += g.advance + letterSpacing;
        }
      }
      // Compute justify gap count on visual-order glyphs so that counting and application use
      // the same adjacency pairs. This avoids mismatch when BiDi L2 reorder changes neighbors.
      if (params.textAlign == TextAlign::Justify && lineIdx < lines.size() - 1 &&
          line.glyphs.size() > 1) {
        int gapCount = 0;
        for (size_t i = 0; i + 1 < visualGlyphs.size(); i++) {
          if (LineBreaker::CanBreakBetween(visualGlyphs[i].unichar, visualGlyphs[i + 1].unichar)) {
            gapCount++;
          }
        }
        if (gapCount > 0) {
          justifyExtraPerGap = (params.boxWidth - line.width) / static_cast<float>(gapCount);
        }
      }
      for (size_t gi = 0; gi < visualGlyphs.size(); gi++) {
        auto& g = visualGlyphs[gi];
#else
      for (size_t gi = 0; gi < line.glyphs.size(); gi++) {
        auto& g = line.glyphs[gi];
#endif
        // Skip newline and tab glyphs: they only participate in metrics/spacing, not rendering.
        if (g.unichar == '\n' || g.unichar == '\t') {
          continue;
        }
#ifdef PAG_BUILD_PAGX
        if (gi > 0 && LineBreaker::CanBreakBetween(visualGlyphs[gi - 1].unichar, g.unichar)) {
#else
        if (gi > 0 && LineBreaker::CanBreakBetween(line.glyphs[gi - 1].unichar, g.unichar)) {
#endif
          justifyOffset += justifyExtraPerGap;
        }
        PositionedGlyph pg = {};
        pg.glyphID = g.glyphID;
        pg.font = g.font;
        pg.x = g.xPosition + xOffset + justifyOffset + g.xOffset;
        pg.y = baselineY - g.yOffset;
        result.horizontalGlyphs[g.sourceText].push_back(pg);
        // Update per-Text linebox bounds in layout coordinate system.
        float glyphRight = pg.x + g.advance;
        float lineTop = relativeTop - line.maxLineHeight + yOffset;
        float lineBottom = relativeTop + yOffset;
        auto it = result.perTextBounds.find(g.sourceText);
        if (it == result.perTextBounds.end()) {
          result.perTextBounds[g.sourceText] =
              Rect::MakeXYWH(pg.x, lineTop, g.advance, lineBottom - lineTop);
        } else {
          auto& tb = it->second;
          float left = std::min(tb.x, pg.x);
          float top = std::min(tb.y, lineTop);
          float right = std::max(tb.x + tb.width, glyphRight);
          float bottom = std::max(tb.y + tb.height, lineBottom);
          tb = Rect::MakeXYWH(left, top, right - left, bottom - top);
        }
      }
    }
  }

  static void RemoveTrailingLetterSpacing(std::vector<VerticalGlyphInfo>& glyphs) {
    if (glyphs.empty()) {
      return;
    }
    auto& lastVG = glyphs.back();
    if (lastVG.glyphs.empty()) {
      return;
    }
    float ls = (lastVG.glyphs.front().sourceText != nullptr)
                   ? lastVG.glyphs.front().sourceText->letterSpacing
                   : 0;
    if (ls != 0) {
      lastVG.height -= ls;
    }
  }

  static void FinishColumn(ColumnInfo* column, float lineHeight, float newlineFontLineHeight) {
    if (column->glyphs.empty()) {
      if (lineHeight > 0) {
        column->maxColumnWidth = lineHeight;
      } else if (newlineFontLineHeight > 0) {
        column->maxColumnWidth = roundf(newlineFontLineHeight);
      }
      return;
    }
    float height = 0;
    float maxFontLineHeight = 0;
    for (auto& vg : column->glyphs) {
      height += vg.height;
      if (!vg.glyphs.empty() && vg.glyphs.front().fontLineHeight > maxFontLineHeight) {
        maxFontLineHeight = vg.glyphs.front().fontLineHeight;
      }
    }
    column->height = height;
    // Auto column width uses font metrics height (ascent + descent + leading) to match
    // horizontal layout behavior where auto lineHeight = metricsHeight. This ensures rotated
    // Latin glyphs fit within the column width.
    column->maxColumnWidth = (lineHeight > 0) ? lineHeight : roundf(maxFontLineHeight);
  }

  std::vector<ColumnInfo> layoutColumns(const std::vector<GlyphInfo>& allGlyphs,
                                        const TextLayoutParams& params) {
    // Phase 1: Build VerticalGlyphInfo list with orientation, metrics, and break marks.
    // Consecutive rotated-group characters are collected into a single VerticalGlyphInfo.
    std::vector<VerticalGlyphInfo> vgList = {};
    vgList.reserve(allGlyphs.size());
    for (size_t i = 0; i < allGlyphs.size(); i++) {
      auto& glyph = allGlyphs[i];
      if (glyph.unichar == '\n') {
        VerticalGlyphInfo vg = {};
        vg.glyphs.push_back(glyph);
        vgList.push_back(std::move(vg));
        continue;
      }

      float letterSpacing = (glyph.sourceText != nullptr) ? glyph.sourceText->letterSpacing : 0;
      auto orientation = VerticalTextUtils::GetOrientation(glyph.unichar);

      {
        // Each glyph gets its own VerticalGlyphInfo entry.
        VerticalGlyphInfo vg = {};
        vg.glyphs.push_back(glyph);
        vg.orientation = orientation;
        if (orientation == VerticalOrientation::Rotated) {
          // Use horizontal advance from font metrics; HarfBuzz TTB xAdvance may be 0.
          float hAdvance = glyph.font.getAdvance(glyph.glyphID);
          vg.height = hAdvance + letterSpacing;
          vg.width = glyph.fontSize;
        } else {
          // For upright characters, use the vertical advance from font metrics.
          float vertAdvance = glyph.font.getAdvance(glyph.glyphID, true);
          vg.height = vertAdvance + letterSpacing;
          vg.width = glyph.fontSize;
        }

        if (i > 0 && allGlyphs[i - 1].unichar != '\n') {
#ifdef PAG_USE_HARFBUZZ
          bool sameCluster = (glyph.cluster != 0 || allGlyphs[i - 1].cluster != 0) &&
                             glyph.cluster == allGlyphs[i - 1].cluster;
          vg.canBreakBefore =
              !sameCluster && LineBreaker::CanBreakBetween(allGlyphs[i - 1].unichar, glyph.unichar);
#else
          vg.canBreakBefore = LineBreaker::CanBreakBetween(allGlyphs[i - 1].unichar, glyph.unichar);
#endif
        }
        vgList.push_back(std::move(vg));
      }
    }

    // Phase 2: Split into columns using break marks.
    std::vector<ColumnInfo> columns = {};
    columns.emplace_back();
    auto* currentColumn = &columns.back();
    float currentColumnHeight = 0;
    bool doWrap = params.wordWrap && !std::isnan(params.boxHeight) && params.boxHeight > 0;
    int lastBreakIndex = -1;
    // Tracks the fontLineHeight of the \n that created the current column.
    // Used as the fallback width for empty columns (e.g. consecutive \n\n).
    float pendingNewlineFontLineHeight = 0.0f;

    for (size_t i = 0; i < vgList.size(); i++) {
      auto& vg = vgList[i];

      if (vg.glyphs.front().unichar == '\n') {
        RemoveTrailingLetterSpacing(currentColumn->glyphs);
        // Trim trailing whitespace from current column.
        while (!currentColumn->glyphs.empty() &&
               LineBreaker::IsWhitespace(currentColumn->glyphs.back().glyphs.front().unichar)) {
          currentColumn->glyphs.pop_back();
        }
        FinishColumn(currentColumn, params.lineHeight, pendingNewlineFontLineHeight);
        columns.emplace_back();
        currentColumn = &columns.back();
        currentColumnHeight = 0;
        lastBreakIndex = -1;
        pendingNewlineFontLineHeight = vg.glyphs.front().fontLineHeight;
        continue;
      }

      if (vg.canBreakBefore) {
        lastBreakIndex = static_cast<int>(currentColumn->glyphs.size());
      }

      if (doWrap && !currentColumn->glyphs.empty() &&
          currentColumnHeight + vg.height > params.boxHeight) {
        if (lastBreakIndex >= 0) {
          std::vector<VerticalGlyphInfo> overflow(currentColumn->glyphs.begin() + lastBreakIndex,
                                                  currentColumn->glyphs.end());
          currentColumn->glyphs.resize(static_cast<size_t>(lastBreakIndex));
          RemoveTrailingLetterSpacing(currentColumn->glyphs);
          // Trim trailing whitespace from current column.
          while (!currentColumn->glyphs.empty() &&
                 LineBreaker::IsWhitespace(currentColumn->glyphs.back().glyphs.front().unichar)) {
            currentColumn->glyphs.pop_back();
          }
          FinishColumn(currentColumn, params.lineHeight, 0.0f);
          columns.emplace_back();
          currentColumn = &columns.back();
          // Skip leading whitespace in overflow.
          size_t skipCount = 0;
          while (skipCount < overflow.size() &&
                 LineBreaker::IsWhitespace(overflow[skipCount].glyphs.front().unichar)) {
            skipCount++;
          }
          for (size_t j = skipCount; j < overflow.size(); j++) {
            if (j == skipCount) {
              overflow[j].canBreakBefore = false;
            }
            currentColumn->glyphs.push_back(std::move(overflow[j]));
          }
          currentColumnHeight = 0;
          for (auto& g : currentColumn->glyphs) {
            currentColumnHeight += g.height;
          }
          // Re-scan break opportunities among glyphs on the new column.
          lastBreakIndex = -1;
          for (size_t j = 1; j < currentColumn->glyphs.size(); j++) {
            if (currentColumn->glyphs[j].canBreakBefore) {
              lastBreakIndex = static_cast<int>(j);
            }
          }
        } else {
          RemoveTrailingLetterSpacing(currentColumn->glyphs);
          FinishColumn(currentColumn, params.lineHeight, 0.0f);
          columns.emplace_back();
          currentColumn = &columns.back();
          currentColumnHeight = 0;
          lastBreakIndex = -1;
        }
      }

      if (vg.canBreakBefore) {
        lastBreakIndex = static_cast<int>(currentColumn->glyphs.size());
      }
      currentColumnHeight += vg.height;
      currentColumn->glyphs.push_back(std::move(vg));
    }

    RemoveTrailingLetterSpacing(currentColumn->glyphs);

    FinishColumn(currentColumn, params.lineHeight, pendingNewlineFontLineHeight);

#ifdef PAG_BUILD_PAGX
    // Apply punctuation squash to all columns.
    ApplyPunctuationSquashToColumns(columns);
#endif

    return columns;
  }

#ifdef PAG_BUILD_PAGX
  static bool IsSquashable(const VerticalGlyphInfo& vg) {
    return vg.orientation == VerticalOrientation::Upright;
  }

  static void ApplyPunctuationSquashToColumns(std::vector<ColumnInfo>& columns) {
    for (auto& column : columns) {
      if (column.glyphs.empty()) {
        continue;
      }

      auto glyphCount = column.glyphs.size();
      // In vertical layout, "leading" = top (before glyph face), "trailing" = bottom (after glyph
      // face). Opening punctuation has leading (top) whitespace, Closing has trailing (bottom).
      // Only apply squash to upright glyphs. Rotated glyphs have their whitespace axis changed after
      // rotation, so horizontal punctuation squash rules do not apply.
      std::vector<float> leadingSquash(glyphCount, 0);
      std::vector<float> trailingSquash(glyphCount, 0);

      // Column-start squash: remove leading whitespace of the first non-whitespace glyph.
      for (size_t i = 0; i < glyphCount; i++) {
        if (LineBreaker::IsWhitespace(column.glyphs[i].glyphs.front().unichar)) {
          continue;
        }
        if (!IsSquashable(column.glyphs[i])) {
          break;
        }
        float fraction =
            PunctuationSquash::GetLineStartSquash(column.glyphs[i].glyphs.front().unichar);
        leadingSquash[i] = column.glyphs[i].height * fraction;
        break;
      }

      // Column-end squash: remove trailing whitespace of the last non-whitespace glyph.
      for (int i = static_cast<int>(glyphCount) - 1; i >= 0; i--) {
        if (LineBreaker::IsWhitespace(column.glyphs[i].glyphs.front().unichar)) {
          continue;
        }
        if (!IsSquashable(column.glyphs[i])) {
          break;
        }
        float fraction =
            PunctuationSquash::GetLineEndSquash(column.glyphs[i].glyphs.front().unichar);
        trailingSquash[i] = column.glyphs[i].height * fraction;
        break;
      }

      // Adjacent punctuation squash (only between upright glyphs).
      for (size_t i = 0; i + 1 < glyphCount; i++) {
        if (!IsSquashable(column.glyphs[i]) || !IsSquashable(column.glyphs[i + 1])) {
          continue;
        }
        auto result = PunctuationSquash::GetAdjacentSquash(
            column.glyphs[i].glyphs.front().unichar, column.glyphs[i + 1].glyphs.front().unichar);
        if (result.prevSquash > 0) {
          float amount = column.glyphs[i].height * result.prevSquash;
          if (amount > trailingSquash[i]) {
            trailingSquash[i] = amount;
          }
        }
        if (result.nextSquash > 0) {
          float amount = column.glyphs[i + 1].height * result.nextSquash;
          if (amount > leadingSquash[i + 1]) {
            leadingSquash[i + 1] = amount;
          }
        }
      }

      // Apply squash to glyph heights and record leadingSquash for position adjustment.
      float totalHeight = 0;
      for (size_t i = 0; i < glyphCount; i++) {
        column.glyphs[i].leadingSquash = leadingSquash[i];
        column.glyphs[i].height -= leadingSquash[i] + trailingSquash[i];
        totalHeight += column.glyphs[i].height;
      }
      column.height = totalHeight;
    }
  }
#endif

  void buildTextBlobVertical(const TextLayoutParams& params, const std::vector<ColumnInfo>& columns,
                             TextLayoutResult& result) {
    if (columns.empty()) {
      return;
    }

    // Calculate total width: all columns use maxColumnWidth (line-box model).
    float totalWidth = 0;
    for (size_t i = 0; i < columns.size(); i++) {
      totalWidth += columns[i].maxColumnWidth;
    }

    // ParagraphAlign controls the block-flow direction (horizontal in vertical mode).
    // Columns go right-to-left, so Near = right-aligned, Far = left-aligned.
    // xStart is where the right edge of the first column starts.
    float xStart = 0;
    if (!std::isnan(params.boxWidth)) {
      switch (params.paragraphAlign) {
        case ParagraphAlign::Near:
          xStart += params.boxWidth;
          break;
        case ParagraphAlign::Middle:
          xStart += (params.boxWidth + totalWidth) / 2;
          break;
        case ParagraphAlign::Far:
          xStart += totalWidth;
          break;
      }
    } else {
      // boxWidth is NaN: no boundary, align relative to position.
      switch (params.paragraphAlign) {
        case ParagraphAlign::Near:
          xStart += totalWidth;
          break;
        case ParagraphAlign::Middle:
          xStart += totalWidth / 2;
          break;
        case ParagraphAlign::Far:
          break;
      }
    }

    // Use result.verticalGlyphs to collect positioned glyphs grouped by source Text element.

    bool overflowHidden = params.overflow == Overflow::Hidden;
    float boxLeft = 0;
    float columnX = xStart;

    for (size_t colIdx = 0; colIdx < columns.size(); colIdx++) {
      auto& column = columns[colIdx];
      // Move left by this column's allocated width.
      float allocatedWidth = column.maxColumnWidth;
      columnX -= allocatedWidth;

      // Skip columns that overflow beyond the left edge of the box.
      if (overflowHidden && !std::isnan(params.boxWidth) && columnX < boxLeft) {
        break;
      }

      // Center of this column for centering upright glyphs.
      float columnCenterX = columnX + allocatedWidth / 2;

      // TextAlign controls the inline direction (vertical in vertical mode).
      // Compute per-column vertical offset based on TextAlign.
      float inlineOffset = 0;
      float justifyGap = 0;
      float effectiveBoxHeight = std::isnan(params.boxHeight) ? 0.0f : params.boxHeight;
      switch (params.textAlign) {
        case TextAlign::Start:
          break;
        case TextAlign::Center:
          inlineOffset = (effectiveBoxHeight - column.height) / 2;
          break;
        case TextAlign::End:
          inlineOffset = effectiveBoxHeight - column.height;
          break;
        case TextAlign::Justify: {
          // Justify: distribute extra space evenly at word boundaries. Last column uses Start.
          if (!std::isnan(params.boxHeight) && colIdx < columns.size() - 1) {
            size_t breakCount = 0;
            for (size_t gi = 0; gi < column.glyphs.size(); gi++) {
              if (column.glyphs[gi].canBreakBefore) {
                breakCount++;
              }
            }
            if (breakCount > 0) {
              float extraSpace = params.boxHeight - column.height;
              justifyGap = extraSpace / static_cast<float>(breakCount);
            }
          }
          break;
        }
      }

      float currentY = inlineOffset;

      for (size_t glyphIdx = 0; glyphIdx < column.glyphs.size(); glyphIdx++) {
        auto& vg = column.glyphs[glyphIdx];

        // Skip tab glyphs: they only occupy space, not rendered.
        if (vg.glyphs.front().unichar == '\t') {
          currentY += vg.height;
          continue;
        }

        if (vg.orientation == VerticalOrientation::Rotated) {
          // Single rotated glyph: rotate 90 degrees CW using RSXform (scos=0, ssin=1).
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = true;
          float absAscent = fabsf(g.ascent);
          float tx = columnCenterX - (absAscent - g.descent) / 2;
          float ty = currentY - vg.leadingSquash;
          vpg.xform = tgfx::RSXform::Make(0, 1, tx, ty);
          result.verticalGlyphs[g.sourceText].push_back(vpg);
#ifdef PAG_USE_HARFBUZZ
        } else {
          // With HarfBuzz vert shaping, use getVerticalOffset() for all upright glyphs.
          // If the font has vert/vrt2 features, HarfBuzz already substituted the glyph.
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          auto offset = g.font.getVerticalOffset(g.glyphID);
          float glyphX = columnCenterX + offset.x;
          float glyphY = currentY - vg.leadingSquash + offset.y;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          result.verticalGlyphs[g.sourceText].push_back(vpg);
        }
#else
        } else if (VerticalTextUtils::NeedsPunctuationOffset(vg.glyphs.front().unichar)) {
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          float dx = 0;
          float dy = 0;
          VerticalTextUtils::GetPunctuationOffset(g.fontSize, &dx, &dy);
          float glyphX = columnCenterX - g.advance / 2 + dx;
          float glyphY = currentY + fabsf(g.ascent) + dy;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          result.verticalGlyphs[g.sourceText].push_back(vpg);
        } else {
          auto& g = vg.glyphs.front();
          VerticalPositionedGlyph vpg = {};
          vpg.glyphID = g.glyphID;
          vpg.font = g.font;
          vpg.useRSXform = false;
          auto offset = g.font.getVerticalOffset(g.glyphID);
          float glyphX = columnCenterX + offset.x;
          float glyphY = currentY + offset.y;
          vpg.position = tgfx::Point::Make(glyphX, glyphY);
          result.verticalGlyphs[g.sourceText].push_back(vpg);
        }
#endif

        currentY += vg.height;
        // Update per-Text linebox bounds in layout coordinate system for vertical layout.
        auto* sourceText = vg.glyphs.front().sourceText;
        if (sourceText != nullptr) {
          float glyphTop = currentY - vg.height;
          auto it = result.perTextBounds.find(sourceText);
          if (it == result.perTextBounds.end()) {
            result.perTextBounds[sourceText] =
                Rect::MakeXYWH(columnX, glyphTop, allocatedWidth, vg.height);
          } else {
            auto& tb = it->second;
            float left = std::min(tb.x, columnX);
            float top = std::min(tb.y, glyphTop);
            float right = std::max(tb.x + tb.width, columnX + allocatedWidth);
            float bottom = std::max(tb.y + tb.height, currentY);
            tb = Rect::MakeXYWH(left, top, right - left, bottom - top);
          }
        }
        if (justifyGap != 0 && glyphIdx + 1 < column.glyphs.size() &&
            column.glyphs[glyphIdx + 1].canBreakBefore) {
          currentY += justifyGap;
        }
      }
    }
  }

  std::shared_ptr<tgfx::Typeface> findTypeface(const std::string& fontFamily,
                                               const std::string& fontStyle) {
    return TextLayout::FindTypeface(fontFamily, fontStyle, fontConfig_);
  }

  FontConfig* fontConfig_ = nullptr;
};

Rect TextLayoutResult::getTextBounds(Text* text) const {
  auto it = perTextBounds.find(text);
  if (it != perTextBounds.end()) {
    return it->second;
  }
  return {};
}

const std::vector<TextLayoutGlyphRun>* TextLayoutResult::getGlyphRuns(Text* text) const {
  auto it = layoutGlyphRuns.find(text);
  if (it != layoutGlyphRuns.end()) {
    return &it->second;
  }
  return nullptr;
}

std::shared_ptr<tgfx::Typeface> TextLayout::FindTypeface(const std::string& fontFamily,
                                                         const std::string& fontStyle,
                                                         FontConfig* fontConfig) {
  if (fontConfig != nullptr) {
    return fontConfig->findTypeface(fontFamily, fontStyle);
  }
  return tgfx::Typeface::MakeFromName(fontFamily, fontStyle);
}

void TextLayout::StoreShapedText(Text* text, ShapedText&& shapedText) {
  if (text == nullptr || shapedText.textBlob == nullptr) {
    return;
  }
  text->textBlob = std::move(shapedText.textBlob);
  text->anchors = std::move(shapedText.anchors);
}

void TextLayout::StoreTextBounds(Text* text, const Rect& bounds) {
  if (text == nullptr) {
    return;
  }
  text->textBounds = bounds;
}

void TextLayout::StoreLayoutRuns(Text* text, std::vector<TextLayoutGlyphRun>&& runs) {
  if (text == nullptr) {
    return;
  }
  text->layoutRuns = std::move(runs);
}

void TextLayout::CollectTextElements(const std::vector<Element*>& elements,
                                     std::vector<Text*>& outText) {
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Text) {
      outText.push_back(static_cast<Text*>(element));
    } else if (element->nodeType() == NodeType::Group) {
      CollectTextElements(static_cast<Group*>(element)->elements, outText);
    } else if (element->nodeType() == NodeType::TextBox) {
      CollectTextElements(static_cast<TextBox*>(element)->elements, outText);
    }
  }
}

void TextLayout::CollectTextElements(const std::vector<Element*>& elements,
                                     std::vector<Text*>& outText,
                                     std::vector<tgfx::Matrix>& outMatrices) {
  TextLayoutContext::collectTextElementsWithMatrices(elements, outText, outMatrices,
                                                     tgfx::Matrix::I());
}

static bool AllHaveEmbeddedGlyphRuns(const std::vector<Text*>& textElements) {
  for (auto* text : textElements) {
    if (text->glyphRuns.empty()) {
      return false;
    }
  }
  return true;
}

static Rect ComputeEmbeddedTextBounds(const Text* text) {
  // The linebox bounds are stored on the first GlyphRun (written by FontEmbedder from layout
  // results). Only the first GlyphRun carries the bounds to avoid double-counting when a Text
  // has multiple GlyphRuns (e.g. vector + bitmap font splits).
  for (auto* glyphRun : text->glyphRuns) {
    if (glyphRun->bounds.width > 0 || glyphRun->bounds.height > 0) {
      return glyphRun->bounds;
    }
  }
  // Fallback: try to compute bounds from the TextBlob if available.
  if (text->getTextBlob()) {
    auto tight = text->getTextBlob()->getTightBounds();
    return Rect::MakeXYWH(tight.left, tight.top, tight.width(), tight.height());
  }
  return {};
}

static Rect RectUnion(const Rect& a, const Rect& b) {
  float left = std::min(a.x, b.x);
  float top = std::min(a.y, b.y);
  float right = std::max(a.x + a.width, b.x + b.width);
  float bottom = std::max(a.y + a.height, b.y + b.height);
  return Rect::MakeXYWH(left, top, right - left, bottom - top);
}

static Rect MergeEmbeddedBounds(const std::vector<Text*>& textElements) {
  Rect totalBounds = {};
  bool hasAny = false;
  for (auto* text : textElements) {
    auto textBounds = ComputeEmbeddedTextBounds(text);
    if (textBounds.width <= 0 && textBounds.height <= 0) {
      continue;
    }
    if (!hasAny) {
      totalBounds = textBounds;
      hasAny = true;
    } else {
      totalBounds = RectUnion(totalBounds, textBounds);
    }
  }
  return totalBounds;
}

Rect TextLayout::Measure(const std::vector<Text*>& textElements, const TextLayoutParams& params,
                         FontConfig* fontConfig) {
  if (textElements.empty()) {
    return {};
  }
  if (AllHaveEmbeddedGlyphRuns(textElements)) {
    return MergeEmbeddedBounds(textElements);
  }
  TextLayoutContext context(fontConfig);
  auto mutableElements = textElements;
  auto result = context.processTextWithLayout(mutableElements, params);
  return result.bounds;
}

Rect TextLayout::Measure(const std::vector<Text*>& textElements, const TextLayoutParams& params,
                         const LayoutContext& context) {
  return Measure(textElements, params, context.getFontConfig());
}

TextLayoutResult TextLayout::Layout(const std::vector<Text*>& textElements,
                                    const TextLayoutParams& params, const LayoutContext& context) {
  if (textElements.empty()) {
    return {};
  }
  TextLayoutContext layoutContext(context.getFontConfig());
  if (AllHaveEmbeddedGlyphRuns(textElements)) {
    // Embedded path: only compute bounds. TextBlob generation is deferred to the caller
    // (TextBox::updateLayout or LayerBuilder) which applies the inverse matrix.
    TextLayoutResult result = {};
    result.bounds = MergeEmbeddedBounds(textElements);
    return result;
  }
  // Cast away const for internal processing (processTextWithLayout takes non-const vector).
  auto mutableElements = textElements;
  return layoutContext.processTextWithLayout(mutableElements, params);
}

}  // namespace pagx
