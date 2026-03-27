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

#include "SVGTextLayout.h"
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "renderer/LineBreaker.h"
#include "tgfx/core/UTF.h"

namespace pagx {

size_t SVGDecodeUTF8Char(const char* data, size_t remaining, int32_t* unichar) {
  if (remaining == 0) {
    return 0;
  }
  const char* ptr = data;
  const char* end = data + remaining;
  int32_t ch = tgfx::UTF::NextUTF8(&ptr, end);
  if (ch < 0) {
    return 0;
  }
  *unichar = ch;
  return static_cast<size_t>(ptr - data);
}

float EstimateCharAdvance(int32_t ch, float fontSize) {
  // CJK ideographs are full-width, occupying 1em per character.
  if (LineBreaker::IsCJK(ch)) {
    return fontSize;
  }
  // Space is typically ~0.25em in common proportional fonts (e.g. Arial, Helvetica).
  if (ch == ' ') {
    return fontSize * 0.25f;
  }
  // Tab is conventionally rendered as 4 spaces worth of advance (4 × 1em).
  if (ch == '\t') {
    return fontSize * 4.0f;
  }
  // Latin and other alphabetic characters average ~0.6em in common proportional fonts.
  return fontSize * 0.6f;
}

std::vector<SVGTextLine> BreakTextIntoLines(const std::vector<SVGCharInfo>& chars, float boxWidth) {
  std::vector<SVGTextLine> lines;
  lines.push_back({0, 0, 0});
  auto* currentLine = &lines.back();
  float currentLineWidth = 0;
  int lastBreakIndex = -1;
  size_t lineStartIdx = 0;

  for (size_t i = 0; i < chars.size(); i++) {
    auto& ch = chars[i];

    if (ch.unichar == '\n') {
      currentLine->width = currentLineWidth;
      lines.push_back({i + 1, 0, 0});
      currentLine = &lines.back();
      currentLineWidth = 0;
      lineStartIdx = i + 1;
      lastBreakIndex = -1;
      continue;
    }

    float glyphEndX = currentLineWidth + ch.advance;

    if (boxWidth > 0 && currentLine->charCount > 0 && glyphEndX > boxWidth) {
      if (lastBreakIndex >= 0) {
        size_t breakAt = static_cast<size_t>(lastBreakIndex);
        currentLine->charCount = breakAt - lineStartIdx + 1;
        // Recalculate width up to the break point, trimming trailing whitespace.
        float w = 0;
        size_t endIdx = lineStartIdx + currentLine->charCount;
        while (endIdx > lineStartIdx && LineBreaker::IsWhitespace(chars[endIdx - 1].unichar)) {
          endIdx--;
        }
        for (size_t j = lineStartIdx; j < endIdx; j++) {
          w += chars[j].advance;
        }
        currentLine->width = w;

        // Skip leading whitespace for next line.
        size_t nextStart = breakAt + 1;
        while (nextStart <= i && LineBreaker::IsWhitespace(chars[nextStart].unichar)) {
          nextStart++;
        }

        lineStartIdx = nextStart;
        lines.push_back({lineStartIdx, 0, 0});
        currentLine = &lines.back();
        currentLineWidth = 0;
        for (size_t j = lineStartIdx; j <= i; j++) {
          currentLineWidth += chars[j].advance;
          currentLine->charCount++;
        }
        // Re-scan break opportunities on the new line.
        lastBreakIndex = -1;
        for (size_t j = lineStartIdx; j < i; j++) {
          if (LineBreaker::CanBreakBetween(chars[j].unichar, chars[j + 1].unichar)) {
            lastBreakIndex = static_cast<int>(j);
          }
        }
      } else {
        // No break opportunity found, force break before current character.
        currentLine->width = currentLineWidth;
        lineStartIdx = i;
        lines.push_back({lineStartIdx, 1, ch.advance});
        currentLine = &lines.back();
        currentLineWidth = ch.advance;
        lastBreakIndex = -1;
      }
      if (i + 1 < chars.size() && chars[i + 1].unichar != '\n') {
        if (LineBreaker::CanBreakBetween(ch.unichar, chars[i + 1].unichar)) {
          lastBreakIndex = static_cast<int>(i);
        }
      }
      continue;
    }

    currentLine->charCount++;
    currentLineWidth += ch.advance;

    if (i + 1 < chars.size() && chars[i + 1].unichar != '\n') {
      if (LineBreaker::CanBreakBetween(ch.unichar, chars[i + 1].unichar)) {
        lastBreakIndex = static_cast<int>(i);
      }
    }
  }

  currentLine->width = currentLineWidth;
  return lines;
}

std::string ExtractLineText(const std::string& fullText, const std::vector<SVGCharInfo>& chars,
                            const SVGTextLine& line) {
  if (line.charCount == 0) {
    return "";
  }
  size_t byteStart = chars[line.charStart].byteOffset;
  auto& lastChar = chars[line.charStart + line.charCount - 1];
  size_t byteEnd = lastChar.byteOffset + lastChar.byteLen;
  return fullText.substr(byteStart, byteEnd - byteStart);
}

SVGTextLayoutResult ComputeTextLayout(const SVGTextLayoutParams& params) {
  SVGTextLayoutResult result = {};
  const auto& content = *params.text;
  auto* textBox = params.textBox;
  float fontSize = params.fontSize;

  bool needsMultiLine = textBox != nullptr && textBox->wordWrap && textBox->size.width > 0;
  result.isMultiLine = needsMultiLine;

  // ── Multi-line: parse text and break into lines ──
  float boxWidth = 0;

  if (needsMultiLine) {
    boxWidth = textBox->size.width;
    // Default line height is 1.2em, a widely used typographic convention (CSS "normal" line-height).
    result.lineHeight = textBox->lineHeight > 0 ? textBox->lineHeight : fontSize * 1.2f;

    result.chars.reserve(content.size());
    size_t pos = 0;
    while (pos < content.size()) {
      int32_t unichar = 0;
      size_t len = SVGDecodeUTF8Char(content.data() + pos, content.size() - pos, &unichar);
      if (len == 0) {
        pos++;
        continue;
      }
      float advance = 0;
      if (unichar != '\n') {
        advance = EstimateCharAdvance(unichar, fontSize) + params.letterSpacing;
      }
      result.chars.push_back({unichar, pos, len, advance});
      pos += len;
    }

    result.lines = BreakTextIntoLines(result.chars, boxWidth);

    while (result.lines.size() > 1 && result.lines.back().charCount == 0) {
      result.lines.pop_back();
    }
  }

  // ── Compute position and anchor ──
  result.x = params.position.x;
  result.y = params.position.y;
  result.anchor = params.textAnchor;
  result.firstLineY = 0;

  if (textBox) {
    switch (textBox->textAlign) {
      case TextAlign::Center:
        result.x = textBox->position.x + textBox->size.width / 2;
        result.anchor = TextAnchor::Center;
        break;
      case TextAlign::End:
        result.x = textBox->position.x + textBox->size.width;
        result.anchor = TextAnchor::End;
        break;
      default:
        result.x = textBox->position.x;
        result.anchor = TextAnchor::Start;
        break;
    }

    if (needsMultiLine) {
      float boxHeight = textBox->size.height;
      float totalHeight = static_cast<float>(result.lines.size()) * result.lineHeight;
      // Approximate ascent for common fonts: ascender ≈ 80% of em-square (e.g. Arial 0.81,
      // Helvetica 0.77, Times New Roman 0.81). Used to shift the first baseline down from the
      // top of the text box so that glyphs sit on the baseline rather than hang from the top.
      float ascent = fontSize * 0.8f;
      float yOffset = 0;
      if (boxHeight > 0) {
        switch (textBox->paragraphAlign) {
          case ParagraphAlign::Middle:
            yOffset = (boxHeight - totalHeight) / 2;
            break;
          case ParagraphAlign::Far:
            yOffset = boxHeight - totalHeight;
            break;
          default:
            break;
        }
      }
      // Half-leading: the extra space (lineHeight − fontSize) is split equally above and below
      // the em square, matching the CSS half-leading model. Without this, the text baseline sits
      // too high, causing the visual center of glyphs to appear above the vertical center.
      float halfLeading = (result.lineHeight - fontSize) / 2.0f;
      result.firstLineY = textBox->position.y + yOffset + halfLeading + ascent;
    } else {
      switch (textBox->paragraphAlign) {
        case ParagraphAlign::Middle:
          // 0.35 ≈ (ascent − descent) / 2 / em. For common fonts the ascent-to-descent midpoint
          // is roughly 0.35em above the baseline (e.g. Arial ascent 0.81, descent 0.19 →
          // (0.81 − 0.19) / 2 = 0.31; Helvetica similar). This shifts the baseline so that
          // the visual center of the glyphs aligns with the vertical center of the text box.
          result.y = textBox->position.y + textBox->size.height / 2 + fontSize * 0.35f;
          break;
        case ParagraphAlign::Far:
          result.y = textBox->position.y + textBox->size.height;
          break;
        default:
          result.y = textBox->position.y + fontSize;
          break;
      }
    }
  }

  return result;
}

std::vector<SVGGlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY) {
  std::vector<SVGGlyphPath> result;
  for (const auto* run : text.glyphRuns) {
    if (!run->font || run->glyphs.empty()) {
      continue;
    }
    float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
    float currentX = textPosX + run->x;
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (!glyph || !glyph->path || glyph->path->isEmpty()) {
        continue;
      }

      float posX = 0;
      float posY = 0;
      if (i < run->positions.size()) {
        posX = textPosX + run->x + run->positions[i].x;
        posY = textPosY + run->y + run->positions[i].y;
        if (i < run->xOffsets.size()) {
          posX += run->xOffsets[i];
        }
      } else if (i < run->xOffsets.size()) {
        posX = textPosX + run->x + run->xOffsets[i];
        posY = textPosY + run->y;
      } else {
        posX = currentX;
        posY = textPosY + run->y;
      }
      currentX += glyph->advance * scale;

      Matrix glyphMatrix = Matrix::Translate(posX, posY) * Matrix::Scale(scale, scale);

      bool hasRotation = i < run->rotations.size() && run->rotations[i] != 0;
      bool hasGlyphScale =
          i < run->scales.size() && (run->scales[i].x != 1 || run->scales[i].y != 1);
      bool hasSkew = i < run->skews.size() && run->skews[i] != 0;

      if (hasRotation || hasGlyphScale || hasSkew) {
        float anchorX = glyph->advance * 0.5f;
        float anchorY = 0;
        if (i < run->anchors.size()) {
          anchorX += run->anchors[i].x;
          anchorY += run->anchors[i].y;
        }

        Matrix perGlyph = Matrix::Translate(-anchorX, -anchorY);
        if (hasGlyphScale) {
          perGlyph = Matrix::Scale(run->scales[i].x, run->scales[i].y) * perGlyph;
        }
        if (hasSkew) {
          Matrix shear = {};
          shear.c = std::tan(pag::DegreesToRadians(run->skews[i]));
          perGlyph = shear * perGlyph;
        }
        if (hasRotation) {
          perGlyph = Matrix::Rotate(run->rotations[i]) * perGlyph;
        }
        perGlyph = Matrix::Translate(anchorX, anchorY) * perGlyph;
        glyphMatrix = glyphMatrix * perGlyph;
      }

      result.push_back({glyphMatrix, glyph->path});
    }
  }
  return result;
}

}  // namespace pagx
