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
  if (LineBreaker::IsCJK(ch)) {
    return fontSize;
  }
  if (ch == ' ') {
    return fontSize * 0.25f;
  }
  if (ch == '\t') {
    return fontSize * 4.0f;
  }
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

}  // namespace pagx
