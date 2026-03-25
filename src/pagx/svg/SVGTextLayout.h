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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "pagx/types/Matrix.h"
#include "pagx/types/Point.h"
#include "pagx/types/TextAnchor.h"
#include "pagx/utils/GlyphPathUtil.h"

namespace pagx {

class Text;
class TextBox;

struct SVGCharInfo {
  int32_t unichar = 0;
  size_t byteOffset = 0;
  size_t byteLen = 0;
  float advance = 0;
};

struct SVGTextLine {
  size_t charStart = 0;
  size_t charCount = 0;
  float width = 0;
};

/**
 * Decodes a single UTF-8 character from the byte stream.
 * Returns the number of bytes consumed, or 0 on error.
 */
size_t SVGDecodeUTF8Char(const char* data, size_t remaining, int32_t* unichar);

/**
 * Estimates the advance width of a character based on font size.
 * CJK characters use full em-width, Latin characters use ~0.6em.
 */
float EstimateCharAdvance(int32_t ch, float fontSize);

/**
 * Breaks a sequence of characters into lines that fit within boxWidth.
 * Follows the same algorithm as TextLayout::layoutLines: tracks line width,
 * breaks at the last opportunity when overflow occurs, and handles explicit
 * newlines and forced breaks.
 */
std::vector<SVGTextLine> BreakTextIntoLines(const std::vector<SVGCharInfo>& chars, float boxWidth);

/**
 * Extracts the UTF-8 substring for a given line from the full text.
 */
std::string ExtractLineText(const std::string& fullText, const std::vector<SVGCharInfo>& chars,
                            const SVGTextLine& line);

/**
 * Input parameters for multi-line text layout computation.
 */
struct SVGTextLayoutParams {
  const std::string* text;  // raw text content
  float fontSize;           // font size in pixels
  float letterSpacing;      // letter spacing in pixels
  Point position;           // Text node position
  TextAnchor textAnchor;    // Text node textAnchor
  const TextBox* textBox;   // may be nullptr
};

/**
 * Result of text layout computation (position, anchor, line-break info).
 */
struct SVGTextLayoutResult {
  float x;                         // text x coordinate
  float y;                         // text y coordinate (used for single-line)
  TextAnchor anchor;               // resolved anchor
  bool isMultiLine;                // whether layout produced multiple lines
  float lineHeight;                // line height (multi-line)
  float firstLineY;                // first line y coordinate (multi-line)
  std::vector<SVGCharInfo> chars;  // parsed character info
  std::vector<SVGTextLine> lines;  // line-break results
};

/**
 * Computes the text layout (UTF-8 parsing, line breaking, position/alignment).
 */
SVGTextLayoutResult ComputeTextLayout(const SVGTextLayoutParams& params);

using SVGGlyphPath = GlyphPath;

}  // namespace pagx
