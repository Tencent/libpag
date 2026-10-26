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
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <tuple>

namespace pagx {

std::string MatchCSSFontStyle(const std::vector<std::string>& styles,
                              const std::string& requested) {
  std::vector<CSSFontFace> faces;
  for (const auto& style : styles) {
    faces.push_back({style, FontStyleFromName(style)});
  }
  auto best = MatchCSSFontFace(faces, FontStyleFromName(requested));
  return best < 0 ? std::string() : faces[best].name;
}

std::vector<std::string> ParseCSSFontFamilyList(const std::string& value) {
  std::vector<std::string> families;
  std::string current;
  char quote = 0;
  bool escaped = false;
  for (size_t i = 0; i <= value.size(); ++i) {
    char c = i == value.size() ? ',' : value[i];
    if (escaped) {
      current += c;
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (quote != 0) {
      if (c == quote) quote = 0;
      else
        current += c;
    } else if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == ',') {
      auto start = current.find_first_not_of(" \t\r\n");
      auto end = current.find_last_not_of(" \t\r\n");
      if (start != std::string::npos) families.push_back(current.substr(start, end - start + 1));
      current.clear();
    } else {
      current += c;
    }
  }
  return families;
}

std::string ResolveCSSGenericFontFamily(const std::string& name) {
  std::string lower = name;
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#if defined(__APPLE__)
  if (lower == "serif" || lower == "ui-serif") return "Times";
  if (lower == "monospace" || lower == "ui-monospace") return "Menlo";
  const char* sans = "Helvetica";
#elif defined(_WIN32)
  if (lower == "serif" || lower == "ui-serif") return "Times New Roman";
  if (lower == "monospace" || lower == "ui-monospace") return "Consolas";
  const char* sans = "Arial";
#else
  if (lower == "serif" || lower == "ui-serif") return "DejaVu Serif";
  if (lower == "monospace" || lower == "ui-monospace") return "DejaVu Sans Mono";
  const char* sans = "DejaVu Sans";
#endif
  if (lower == "sans-serif" || lower == "ui-sans-serif" || lower == "system-ui" ||
      lower == "-apple-system" || lower == "blinkmacsystemfont")
    return sans;
  if (lower == "cursive" || lower == "fantasy" || lower == "ui-rounded" || lower == "emoji" ||
      lower == "math" || lower == "fangsong")
    return {};
  return name;
}

bool IsCSSGenericFontFamily(const std::string& name) {
  static const char* keywords[] = {
      "serif",         "ui-serif",           "sans-serif", "ui-sans-serif", "system-ui",
      "-apple-system", "blinkmacsystemfont", "monospace",  "ui-monospace",  "cursive",
      "fantasy",       "ui-rounded",         "emoji",      "math",          "fangsong"};
  std::string lower = name;
  for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const auto* keyword : keywords) {
    if (lower == keyword) return true;
  }
  return false;
}

namespace {

std::string TrimAscii(const std::string& s) {
  size_t begin = 0;
  size_t end = s.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(begin, end - begin);
}

std::string ToLowerAscii(std::string s) {
  for (auto& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

}  // namespace

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

namespace {

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

double CSSFontWidthFromClass(int widthClass) {
  static constexpr double widths[] = {50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200};
  return widthClass >= 1 && widthClass <= 9 ? widths[widthClass - 1] : 100;
}

CSSFontStyle ParseCSSFontStyle(const std::string& weight, const std::string& slant,
                               const std::string& width) {
  CSSFontStyle result;
  auto w = ToLowerAscii(TrimAscii(weight));
  if (w == "bold" || w == "bolder") result.weight = 700;
  else if (w == "lighter")
    result.weight = 300;
  else {
    char* end = nullptr;
    double value = std::strtod(w.c_str(), &end);
    if (end != w.c_str() && *end == '\0' && std::isfinite(value) && value >= 1 && value <= 1000)
      result.weight = value;
  }
  auto s = ToLowerAscii(TrimAscii(slant));
  if (s == "italic") result.slant = CSSFontSlant::Italic;
  else if (s == "oblique" || s.compare(0, 8, "oblique ") == 0)
    result.slant = CSSFontSlant::Oblique;
  auto stretch = ToLowerAscii(TrimAscii(width));
  static const char* names[] = {"ultra-condensed", "extra-condensed", "condensed",
                                "semi-condensed",  "normal",          "semi-expanded",
                                "expanded",        "extra-expanded",  "ultra-expanded"};
  for (int i = 0; i < 9; ++i) {
    if (stretch == names[i]) result.width = CSSFontWidthFromClass(i + 1);
  }
  char* end = nullptr;
  double value = std::strtod(stretch.c_str(), &end);
  if (end != stretch.c_str() && std::string(end) == "%" && std::isfinite(value) && value > 0)
    result.width = value;
  return result;
}

CSSFontStyle FontStyleFromName(const std::string& name) {
  CSSFontStyle result;
  result.weight = ParseFontStyleName(name).weight;
  std::istringstream tokens(ToLowerAscii(name));
  std::string token;
  std::string modifier;
  while (tokens >> token) {
    // Vendors use both "Semi Condensed" and "SemiCondensed" (also hyphenated).
    token.erase(std::remove(token.begin(), token.end(), '-'), token.end());
    if (token == "ultra" || token == "extra" || token == "semi") {
      modifier = token;
      continue;
    }
    if (token == "condensed" || token == "expanded" || token == "bold" || token == "light")
      token = modifier + token;
    modifier.clear();
    static const char* widths[] = {"ultracondensed", "extracondensed", "condensed",
                                   "semicondensed",  "normal",         "semiexpanded",
                                   "expanded",       "extraexpanded",  "ultraexpanded"};
    for (int i = 0; i < 9; ++i) {
      if (i != 4 && token == widths[i]) result.width = CSSFontWidthFromClass(i + 1);
    }
    if (token == "narrow" || token == "compressed") result.width = 75;
    if (token == "extended") result.width = 125;
    if (token == "italic") result.slant = CSSFontSlant::Italic;
    if (token == "oblique") result.slant = CSSFontSlant::Oblique;
    auto weight = WeightKeywordToNumeric(token);
    if (weight) result.weight = weight;
    char* end = nullptr;
    auto number = std::strtod(token.c_str(), &end);
    if (end != token.c_str() && std::isfinite(number)) {
      if (*end == '\0' && number >= 1 && number <= 1000) result.weight = number;
      else if (std::string(end) == "%" && number > 0)
        result.width = number;
    }
  }
  return result;
}

std::string FontStyleRequestName(const CSSFontStyle& style) {
  std::ostringstream name;
  name.imbue(std::locale::classic());
  name << std::setprecision(std::numeric_limits<double>::max_digits10);
  if (style.width != 100) name << style.width << "% ";
  auto weight = static_cast<int>(style.weight);
  const char* keyword = weight == style.weight && weight % 100 == 0 && weight <= 900
                            ? WeightKeywordForRoundedHundreds(weight)
                            : nullptr;
  if (keyword) name << keyword;
  else if (style.weight != 400)
    name << style.weight;
  if (style.slant != CSSFontSlant::Normal) {
    if (!name.str().empty() && name.str().back() != ' ') name << ' ';
    name << (style.slant == CSSFontSlant::Italic ? "Italic" : "Oblique");
  }
  return TrimAscii(name.str());
}

namespace {
using Rank = std::pair<int, double>;
Rank WidthRank(double desired, double value) {
  if (desired <= 100) return value <= desired ? Rank{0, desired - value} : Rank{1, value - desired};
  return value >= desired ? Rank{0, value - desired} : Rank{1, desired - value};
}
Rank WeightRank(double desired, double value) {
  if (desired < 400) return value <= desired ? Rank{0, desired - value} : Rank{1, value - desired};
  if (desired > 500) return value >= desired ? Rank{0, value - desired} : Rank{1, desired - value};
  if (value >= desired && value <= 500) return {0, value - desired};
  return value < desired ? Rank{1, desired - value} : Rank{2, value - desired};
}
int SlantRank(CSSFontSlant desired, CSSFontSlant value) {
  if (desired == value) return 0;
  if (desired == CSSFontSlant::Normal) return value == CSSFontSlant::Oblique ? 1 : 2;
  return value == CSSFontSlant::Normal ? 2 : 1;
}
}  // namespace

int CompareCSSFontStyles(const CSSFontStyle& request, const CSSFontStyle& a,
                         const CSSFontStyle& b) {
  auto rank = [&](const CSSFontStyle& value) {
    return std::make_tuple(WidthRank(request.width, value.width),
                           SlantRank(request.slant, value.slant),
                           WeightRank(request.weight, value.weight));
  };
  auto first = rank(a);
  auto second = rank(b);
  return first < second ? -1 : first == second ? 0 : 1;
}

int MatchCSSFontFace(const std::vector<CSSFontFace>& faces, const CSSFontStyle& request) {
  int best = -1;
  for (size_t i = 0; i < faces.size(); ++i) {
    auto order = best < 0 ? -1 : CompareCSSFontStyles(request, faces[i].style, faces[best].style);
    if (order < 0 || (order == 0 && faces[i].name < faces[best].name)) best = static_cast<int>(i);
  }
  return best;
}

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

FontStyleSynthesis ResolveFontStyleSynthesis(const std::string& cssFontWeight,
                                             const std::string& cssFontStyle) {
  FontStyleSynthesis out;
  // Both the weight and slant axes are carried as real-face style labels (Bold / SemiBold /
  // Italic / Medium Italic ...) so the renderer resolves the authored face when it is installed
  // or embedded. fauxItalic stays true as a synthesis fallback for when the styled italic face
  // is unavailable: layout detects the resolved typeface and drops the flag in that case.
  out.fontStyleName = ResolveFontStyleName(cssFontWeight, cssFontStyle);
  out.fauxBold = false;
  out.fauxItalic = IsItalicCssStyle(cssFontStyle);
  return out;
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
