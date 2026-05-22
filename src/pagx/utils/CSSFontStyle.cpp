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

#include "CSSFontStyle.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace pagx {

namespace {

std::string TrimAscii(const std::string& s) {
  size_t begin = 0;
  size_t end = s.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(begin, end - begin);
}

std::string ToLowerAscii(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Converts a CSS font-weight token to a numeric weight in the OpenType 1-1000 range. Returns
// 400 for unrecognized inputs so unknown values collapse to Regular instead of producing a
// surprising style. `bolder`/`lighter` are treated as relative-to-normal because the cascade
// upstream (browser snapshot or SVG resolver) normally resolves them to absolute numbers
// before we get here; the keywords are kept as a defensive fallback.
int CssFontWeightToNumeric(const std::string& cssFontWeight) {
  std::string w = ToLowerAscii(TrimAscii(cssFontWeight));
  if (w.empty() || w == "normal") return 400;
  if (w == "bold") return 700;
  if (w == "bolder") return 700;
  if (w == "lighter") return 300;
  char* end = nullptr;
  long n = std::strtol(w.c_str(), &end, 10);
  if (end == w.c_str() || n <= 0) return 400;
  if (n < 1) n = 1;
  if (n > 1000) n = 1000;
  return static_cast<int>(n);
}

// Rounds a numeric weight to the nearest multiple of 100 and returns the canonical PAGX
// keyword. Returns nullptr for Regular (400) so callers can leave the weight portion empty.
const char* WeightKeywordForRoundedHundreds(int numericWeight) {
  int rounded = ((numericWeight + 50) / 100) * 100;
  if (rounded < 100) rounded = 100;
  if (rounded > 900) rounded = 900;
  switch (rounded) {
    case 100:
      return "Thin";
    case 200:
      return "ExtraLight";
    case 300:
      return "Light";
    case 400:
      return nullptr;
    case 500:
      return "Medium";
    case 600:
      return "SemiBold";
    case 700:
      return "Bold";
    case 800:
      return "ExtraBold";
    case 900:
      return "Black";
    default:
      return nullptr;
  }
}

bool IsItalicCssStyle(const std::string& cssFontStyle) {
  std::string s = ToLowerAscii(TrimAscii(cssFontStyle));
  return s == "italic" || s == "oblique";
}

// Maps a single weight keyword token (already lowercased and trimmed) to a numeric weight.
// Recognises both PAGX keywords (thin/extralight/light/regular/medium/semibold/bold/extrabold/
// black) and a handful of vendor aliases (book, demi, heavy, ultra). Returns 0 for unknown
// tokens so callers can fall through to numeric parsing.
int WeightKeywordToNumeric(const std::string& token) {
  if (token == "thin" || token == "hairline") return 100;
  if (token == "extralight" || token == "ultralight") return 200;
  if (token == "light") return 300;
  if (token == "regular" || token == "normal" || token == "book") return 400;
  if (token == "medium") return 500;
  if (token == "semibold" || token == "demibold" || token == "demi") return 600;
  if (token == "bold") return 700;
  if (token == "extrabold" || token == "ultrabold") return 800;
  if (token == "black" || token == "heavy" || token == "ultra") return 900;
  return 0;
}

}  // namespace

std::string ResolveFontStyleName(const std::string& cssFontWeight,
                                 const std::string& cssFontStyle) {
  const char* weightKeyword =
      WeightKeywordForRoundedHundreds(CssFontWeightToNumeric(cssFontWeight));
  bool italic = IsItalicCssStyle(cssFontStyle);
  if (weightKeyword && italic) {
    return std::string(weightKeyword) + " Italic";
  }
  if (weightKeyword) {
    return weightKeyword;
  }
  if (italic) {
    return "Italic";
  }
  return std::string();
}

ParsedFontStyle ParseFontStyleName(const std::string& fontStyleName) {
  ParsedFontStyle out;
  bool sawWeightToken = false;
  size_t pos = 0;
  while (pos < fontStyleName.size()) {
    while (pos < fontStyleName.size() &&
           std::isspace(static_cast<unsigned char>(fontStyleName[pos]))) {
      ++pos;
    }
    size_t end = pos;
    while (end < fontStyleName.size() &&
           !std::isspace(static_cast<unsigned char>(fontStyleName[end]))) {
      ++end;
    }
    if (end == pos) {
      break;
    }
    std::string token = ToLowerAscii(fontStyleName.substr(pos, end - pos));
    pos = end;

    if (token == "italic" || token == "oblique") {
      out.italic = true;
      continue;
    }
    int weight = WeightKeywordToNumeric(token);
    if (weight > 0) {
      out.weight = weight;
      sawWeightToken = true;
      continue;
    }
    char* numericEnd = nullptr;
    long n = std::strtol(token.c_str(), &numericEnd, 10);
    if (numericEnd != token.c_str() && *numericEnd == '\0' && n > 0) {
      if (n < 100) n = 100;
      if (n > 900) n = 900;
      out.weight = static_cast<int>(n);
      sawWeightToken = true;
      continue;
    }
    // Unknown token: ignore. Common on macOS where CTFontCopyName may return localised
    // labels or compound names ("W4", "Pro" etc.); leaving the weight at its current value
    // is safer than guessing.
  }
  if (!sawWeightToken) {
    out.weight = 400;
  }
  return out;
}

}  // namespace pagx
