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

#include "pagx/html/importer/HTMLDetail.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <utility>
#include "pagx/html/importer/HTMLBoxAttributes.h"

namespace pagx::html {

bool IsContainerTag(const std::string& tag) {
  return tag == "div" || tag == "section" || tag == "header" || tag == "footer" || tag == "main" ||
         tag == "aside" || tag == "nav" || tag == "article" || tag == "body";
}

bool IsTextLeafTag(const std::string& tag) {
  return tag == "p" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" ||
         tag == "h6" || tag == "span" || tag == "a";
}

bool IsInlineRunTag(const std::string& tag) {
  return tag == "span" || tag == "a" || tag == "br";
}

bool IsInlineLeafChildName(const std::string& name) {
  return IsInlineRunTag(name) || name == "br";
}

std::vector<std::string> SplitTopLevelCommas(const std::string& s) {
  std::vector<std::string> out;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '(') {
      depth++;
    } else if (c == ')') {
      if (depth > 0) depth--;
    } else if (c == ',' && depth == 0) {
      out.push_back(Trim(s.substr(start, i - start)));
      start = i + 1;
    }
  }
  out.push_back(Trim(s.substr(start)));
  return out;
}

std::vector<std::string> SplitTopLevelWhitespace(const std::string& s) {
  std::vector<std::string> out;
  int depth = 0;
  std::string cur;
  for (char c : s) {
    if (c == '(') {
      depth++;
      cur.push_back(c);
    } else if (c == ')') {
      if (depth > 0) depth--;
      cur.push_back(c);
    } else if (depth == 0 && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

std::vector<std::pair<std::string, std::string>> ParseStyleDeclarations(const std::string& style) {
  std::vector<std::pair<std::string, std::string>> out;
  size_t pos = 0;
  while (pos < style.size()) {
    while (pos < style.size() && std::isspace(static_cast<unsigned char>(style[pos]))) {
      ++pos;
    }
    if (pos + 1 < style.size() && style[pos] == '/' && style[pos + 1] == '*') {
      auto commentEnd = style.find("*/", pos + 2);
      pos = (commentEnd == std::string::npos) ? style.size() : commentEnd + 2;
      continue;
    }
    size_t colonPos = style.find(':', pos);
    if (colonPos == std::string::npos) {
      break;
    }
    std::string propName = Trim(style.substr(pos, colonPos - pos));
    size_t semicolonPos = std::string::npos;
    int parenDepth = 0;
    for (size_t i = colonPos + 1; i < style.size(); i++) {
      if (style[i] == '(') {
        parenDepth++;
      } else if (style[i] == ')') {
        if (parenDepth > 0) parenDepth--;
      } else if (style[i] == ';' && parenDepth == 0) {
        semicolonPos = i;
        break;
      }
    }
    if (semicolonPos == std::string::npos) {
      semicolonPos = style.size();
    }
    std::string propValue = Trim(style.substr(colonPos + 1, semicolonPos - colonPos - 1));
    if (!propName.empty() && !propValue.empty()) {
      out.emplace_back(std::move(propName), std::move(propValue));
    }
    pos = semicolonPos + 1;
  }
  return out;
}

void ParseStyleString(const std::string& styleStr,
                      std::unordered_map<std::string, std::string>& out) {
  for (auto& decl : ParseStyleDeclarations(styleStr)) {
    out[ToLower(decl.first)] = std::move(decl.second);
  }
}

namespace {

std::string StripSurroundingQuotes(const std::string& s) {
  if (s.size() < 2) return s;
  char first = s.front();
  char last = s.back();
  if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

}  // namespace

std::vector<std::string> ParseFontFamilyTokens(const std::string& raw) {
  std::vector<std::string> out;
  if (Trim(raw).empty()) return out;
  auto tokens = SplitTopLevelCommas(raw);
  out.reserve(tokens.size());
  for (auto& token : tokens) {
    auto trimmed = Trim(token);
    if (trimmed.empty()) continue;
    auto stripped = Trim(StripSurroundingQuotes(trimmed));
    if (!stripped.empty()) {
      out.push_back(std::move(stripped));
    }
  }
  return out;
}

std::string ResolveGenericFontFamily(const std::string& name) {
  std::string lower = ToLower(Trim(name));
  if (lower.empty()) return {};

#if defined(__APPLE__)
  if (lower == "serif" || lower == "ui-serif") return "Times";
  if (lower == "sans-serif" || lower == "ui-sans-serif" || lower == "system-ui" ||
      lower == "-apple-system" || lower == "blinkmacsystemfont") {
    return "Helvetica";
  }
  if (lower == "monospace" || lower == "ui-monospace") return "Menlo";
#elif defined(_WIN32)
  if (lower == "serif" || lower == "ui-serif") return "Times New Roman";
  if (lower == "sans-serif" || lower == "ui-sans-serif" || lower == "system-ui" ||
      lower == "-apple-system" || lower == "blinkmacsystemfont") {
    return "Arial";
  }
  if (lower == "monospace" || lower == "ui-monospace") return "Consolas";
#else
  if (lower == "serif" || lower == "ui-serif") return "DejaVu Serif";
  if (lower == "sans-serif" || lower == "ui-sans-serif" || lower == "system-ui" ||
      lower == "-apple-system" || lower == "blinkmacsystemfont") {
    return "DejaVu Sans";
  }
  if (lower == "monospace" || lower == "ui-monospace") return "DejaVu Sans Mono";
#endif

  // Recognised generics with no concrete mapping on the current platform. Distinguished
  // from "not a generic" so callers can warn and drop the token.
  if (lower == "cursive" || lower == "fantasy" || lower == "ui-rounded" || lower == "emoji" ||
      lower == "math" || lower == "fangsong") {
    return {};
  }

  return name;
}

const std::unordered_map<std::string, uint32_t>& NamedColors() {
  // clang-format off
  static const std::unordered_map<std::string, uint32_t> table = {
      {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7}, {"aqua", 0x00FFFF},
      {"aquamarine", 0x7FFFD4}, {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC},
      {"bisque", 0xFFE4C4}, {"black", 0x000000}, {"blanchedalmond", 0xFFEBCD},
      {"blue", 0x0000FF}, {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
      {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0}, {"chartreuse", 0x7FFF00},
      {"chocolate", 0xD2691E}, {"coral", 0xFF7F50}, {"cornflowerblue", 0x6495ED},
      {"cornsilk", 0xFFF8DC}, {"crimson", 0xDC143C}, {"cyan", 0x00FFFF},
      {"darkblue", 0x00008B}, {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
      {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400}, {"darkgrey", 0xA9A9A9},
      {"darkkhaki", 0xBDB76B}, {"darkmagenta", 0x8B008B}, {"darkolivegreen", 0x556B2F},
      {"darkorange", 0xFF8C00}, {"darkorchid", 0x9932CC}, {"darkred", 0x8B0000},
      {"darksalmon", 0xE9967A}, {"darkseagreen", 0x8FBC8F}, {"darkslateblue", 0x483D8B},
      {"darkslategray", 0x2F4F4F}, {"darkslategrey", 0x2F4F4F}, {"darkturquoise", 0x00CED1},
      {"darkviolet", 0x9400D3}, {"deeppink", 0xFF1493}, {"deepskyblue", 0x00BFFF},
      {"dimgray", 0x696969}, {"dimgrey", 0x696969}, {"dodgerblue", 0x1E90FF},
      {"firebrick", 0xB22222}, {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22},
      {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC}, {"ghostwhite", 0xF8F8FF},
      {"gold", 0xFFD700}, {"goldenrod", 0xDAA520}, {"gray", 0x808080},
      {"green", 0x008000}, {"greenyellow", 0xADFF2F}, {"grey", 0x808080},
      {"honeydew", 0xF0FFF0}, {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C},
      {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0}, {"khaki", 0xF0E68C},
      {"lavender", 0xE6E6FA}, {"lavenderblush", 0xFFF0F5}, {"lawngreen", 0x7CFC00},
      {"lemonchiffon", 0xFFFACD}, {"lightblue", 0xADD8E6}, {"lightcoral", 0xF08080},
      {"lightcyan", 0xE0FFFF}, {"lightgoldenrodyellow", 0xFAFAD2}, {"lightgray", 0xD3D3D3},
      {"lightgreen", 0x90EE90}, {"lightgrey", 0xD3D3D3}, {"lightpink", 0xFFB6C1},
      {"lightsalmon", 0xFFA07A}, {"lightseagreen", 0x20B2AA}, {"lightskyblue", 0x87CEFA},
      {"lightslategray", 0x778899}, {"lightslategrey", 0x778899}, {"lightsteelblue", 0xB0C4DE},
      {"lightyellow", 0xFFFFE0}, {"lime", 0x00FF00}, {"limegreen", 0x32CD32},
      {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF}, {"maroon", 0x800000},
      {"mediumaquamarine", 0x66CDAA}, {"mediumblue", 0x0000CD}, {"mediumorchid", 0xBA55D3},
      {"mediumpurple", 0x9370DB}, {"mediumseagreen", 0x3CB371}, {"mediumslateblue", 0x7B68EE},
      {"mediumspringgreen", 0x00FA9A}, {"mediumturquoise", 0x48D1CC}, {"mediumvioletred", 0xC71585},
      {"midnightblue", 0x191970}, {"mintcream", 0xF5FFFA}, {"mistyrose", 0xFFE4E1},
      {"moccasin", 0xFFE4B5}, {"navajowhite", 0xFFDEAD}, {"navy", 0x000080},
      {"oldlace", 0xFDF5E6}, {"olive", 0x808000}, {"olivedrab", 0x6B8E23},
      {"orange", 0xFFA500}, {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
      {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98}, {"paleturquoise", 0xAFEEEE},
      {"palevioletred", 0xDB7093}, {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9},
      {"peru", 0xCD853F}, {"pink", 0xFFC0CB}, {"plum", 0xDDA0DD},
      {"powderblue", 0xB0E0E6}, {"purple", 0x800080}, {"rebeccapurple", 0x663399},
      {"red", 0xFF0000}, {"rosybrown", 0xBC8F8F}, {"royalblue", 0x4169E1},
      {"saddlebrown", 0x8B4513}, {"salmon", 0xFA8072}, {"sandybrown", 0xF4A460},
      {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE}, {"sienna", 0xA0522D},
      {"silver", 0xC0C0C0}, {"skyblue", 0x87CEEB}, {"slateblue", 0x6A5ACD},
      {"slategray", 0x708090}, {"slategrey", 0x708090}, {"snow", 0xFFFAFA},
      {"springgreen", 0x00FF7F}, {"steelblue", 0x4682B4}, {"tan", 0xD2B48C},
      {"teal", 0x008080}, {"thistle", 0xD8BFD8}, {"tomato", 0xFF6347},
      {"transparent", 0x000000}, {"turquoise", 0x40E0D0}, {"violet", 0xEE82EE},
      {"wheat", 0xF5DEB3}, {"white", 0xFFFFFF}, {"whitesmoke", 0xF5F5F5},
      {"yellow", 0xFFFF00}, {"yellowgreen", 0x9ACD32},
  };
  // clang-format on
  return table;
}

const std::unordered_map<std::string, std::string>& ElementDefaults() {
  // clang-format off
  static const std::unordered_map<std::string, std::string> table = {
      {"body", "font-family: Arial; font-size: 14px; color: #1E293B;"},
      {"h1", "font-size: 32px; font-weight: bold;"},
      {"h2", "font-size: 24px; font-weight: bold;"},
      {"h3", "font-size: 20px; font-weight: bold;"},
      {"h4", "font-size: 18px; font-weight: bold;"},
      {"h5", "font-size: 16px; font-weight: bold;"},
      {"h6", "font-size: 14px; font-weight: bold;"},
      {"a", "color: #2563EB; text-decoration: underline;"},
  };
  // clang-format on
  return table;
}

static std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
BuildParsedElementDefaults() {
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> out;
  for (const auto& kv : ElementDefaults()) {
    ParseStyleString(kv.second, out[kv.first]);
  }
  return out;
}

const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>&
ParsedElementDefaults() {
  static const auto table = BuildParsedElementDefaults();
  return table;
}

void LowercaseTagsInPlace(const std::shared_ptr<DOMNode>& node, int depth) {
  if (!node) return;
  if (depth >= MAX_HTML_RECURSION_DEPTH) return;
  if (node->type == DOMNodeType::Element) {
    node->name = ToLower(std::move(node->name));
    // Inline SVG: SVG attribute and tag names are case-sensitive (viewBox,
    // preserveAspectRatio, linearGradient, clipPath, ...). The whole subtree is
    // serialized verbatim into importDirective.content and re-parsed by the SVG
    // importer in `pagx resolve`, so we must preserve case here. Lower-casing the
    // <svg> element's own tag is still fine (the importer dispatches on "svg").
    if (node->name == "svg") {
      return;
    }
    // Lower-case attribute names while we're here (CSS keys must be lower-case anyway).
    for (auto& attr : node->attributes) {
      attr.name = ToLower(std::move(attr.name));
    }
  }
  auto child = node->firstChild;
  while (child) {
    LowercaseTagsInPlace(child, depth + 1);
    child = child->nextSibling;
  }
}

std::shared_ptr<DOMNode> MakeStrayTextSpan(const std::string& text) {
  auto span = std::make_shared<DOMNode>();
  span->name = "span";
  span->type = DOMNodeType::Element;
  auto textNode = std::make_shared<DOMNode>();
  textNode->name = text;
  textNode->type = DOMNodeType::Text;
  span->firstChild = textNode;
  return span;
}

void UnlinkChild(const std::shared_ptr<DOMNode>& parent, const std::shared_ptr<DOMNode>& prev,
                 const std::shared_ptr<DOMNode>& child) {
  if (!parent || !child) return;
  auto next = child->nextSibling;
  if (prev) {
    prev->nextSibling = next;
  } else {
    parent->firstChild = next;
  }
  child->nextSibling = nullptr;
}

std::shared_ptr<DOMNode> ReplaceChild(const std::shared_ptr<DOMNode>& parent,
                                      const std::shared_ptr<DOMNode>& prev,
                                      const std::shared_ptr<DOMNode>& child,
                                      const std::shared_ptr<DOMNode>& replacement) {
  if (!parent || !child || !replacement) return child ? child->nextSibling : nullptr;
  auto next = child->nextSibling;
  replacement->nextSibling = next;
  if (prev) {
    prev->nextSibling = replacement;
  } else {
    parent->firstChild = replacement;
  }
  child->nextSibling = nullptr;
  return next;
}

std::string EscapeXml(const std::string& text, bool isAttribute) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    switch (c) {
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += isAttribute ? "&quot;" : "\"";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

float ParseAngle(const std::string& raw) {
  std::string s = Trim(raw);
  if (s.empty()) return 0.0f;
  char* end = nullptr;
  float v = std::strtof(s.c_str(), &end);
  if (end == s.c_str()) return 0.0f;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "deg") return v;
  if (suffix == "rad") return v * 180.0f / 3.14159265358979323846f;
  if (suffix == "turn") return v * 360.0f;
  return v;
}

float CssDirectionToAngle(const std::string& kw) {
  std::string s = ToLower(Trim(kw));
  if (s.compare(0, 3, "to ") != 0) return 180.0f;
  s = Trim(s.substr(3));
  if (s == "top") return 0.0f;
  if (s == "right") return 90.0f;
  if (s == "bottom") return 180.0f;
  if (s == "left") return 270.0f;
  if (s == "top right" || s == "right top") return 45.0f;
  if (s == "bottom right" || s == "right bottom") return 135.0f;
  if (s == "bottom left" || s == "left bottom") return 225.0f;
  if (s == "top left" || s == "left top") return 315.0f;
  return 180.0f;
}

std::string ExtractParenArgs(const std::string& value) {
  // Match the first '(' to its corresponding ')' by tracking nesting depth, instead of
  // using rfind(')'). When the input contains multiple top-level groups such as
  // `radial-gradient(...), radial-gradient(...)`, rfind would jump to the very last ')'
  // and treat all groups as one, producing nonsensical args.
  auto open = value.find('(');
  if (open == std::string::npos) return {};
  int depth = 0;
  for (size_t i = open; i < value.size(); ++i) {
    char c = value[i];
    if (c == '(') {
      ++depth;
    } else if (c == ')') {
      --depth;
      if (depth == 0) {
        return value.substr(open + 1, i - open - 1);
      }
    }
  }
  return {};
}

std::string DirectoryOf(const std::string& filePath) {
  auto lastSlash = filePath.find_last_of("/\\");
  if (lastSlash == std::string::npos) {
    return {};
  }
  return filePath.substr(0, lastSlash + 1);
}

bool LooksAbsolutePath(const std::string& src) {
  if (src.empty()) return false;
  if (src[0] == '/' || src[0] == '\\') return true;
  if (src.find("://") != std::string::npos) return true;
  if (src.compare(0, 5, "data:") == 0) return true;
  // Windows drive letter ("C:/")
  if (src.size() > 2 && std::isalpha(static_cast<unsigned char>(src[0])) && src[1] == ':' &&
      (src[2] == '/' || src[2] == '\\')) {
    return true;
  }
  return false;
}

Color HexToColor(uint32_t hex, bool hasAlpha) {
  Color color = {};
  color.colorSpace = ColorSpace::SRGB;
  if (hasAlpha) {
    color.red = static_cast<float>((hex >> 24) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.blue = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.alpha = static_cast<float>(hex & 0xFF) / 255.0f;
  } else {
    color.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
    color.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
    color.blue = static_cast<float>(hex & 0xFF) / 255.0f;
    color.alpha = 1.0f;
  }
  return color;
}

Padding BuildPaddingShorthand(const std::vector<float>& nums) {
  Padding p = {};
  if (nums.size() == 1) {
    p.top = p.right = p.bottom = p.left = nums[0];
  } else if (nums.size() == 2) {
    p.top = p.bottom = nums[0];
    p.left = p.right = nums[1];
  } else if (nums.size() == 3) {
    p.top = nums[0];
    p.left = p.right = nums[1];
    p.bottom = nums[2];
  } else if (nums.size() >= 4) {
    p.top = nums[0];
    p.right = nums[1];
    p.bottom = nums[2];
    p.left = nums[3];
  }
  return p;
}

bool ParseSizingDimension(const std::string& raw, float& outPx, float& outPct) {
  std::string value = Trim(raw);
  if (value.empty()) return false;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return false;
  std::string suffix = Trim(end);
  if (suffix == "%") {
    outPct = num;
    return true;
  }
  if (suffix.empty() || suffix == "px") {
    outPx = num;
    return true;
  }
  if (suffix == "em" || suffix == "rem") {
    outPx = num * 16.0f;
    return true;
  }
  outPx = num;
  return true;
}

float ConvertCssLengthToPx(float num, const std::string& unit, float fontSizePx, float canvasW,
                           float canvasH, bool& recognized) {
  recognized = false;
  if (unit.empty() || unit == "px") {
    recognized = true;
    return num;
  }
  if (unit == "em") {
    recognized = true;
    float base = (std::isfinite(fontSizePx) && fontSizePx > 0) ? fontSizePx : 16.0f;
    return num * base;
  }
  if (unit == "rem") {
    recognized = true;
    return num * 16.0f;
  }
  if (unit == "pt") {
    recognized = true;
    return num * 4.0f / 3.0f;
  }
  if (unit == "vw") {
    if (std::isfinite(canvasW) && canvasW > 0) {
      recognized = true;
      return num * canvasW / 100.0f;
    }
    return NAN;
  }
  if (unit == "vh") {
    if (std::isfinite(canvasH) && canvasH > 0) {
      recognized = true;
      return num * canvasH / 100.0f;
    }
    return NAN;
  }
  return NAN;
}

std::string CollapseHTMLWhitespace(const std::string& raw, bool trimLeading, bool trimTrailing) {
  std::string normalized;
  normalized.reserve(raw.size());
  bool prevSpace = false;
  for (char c : raw) {
    if (c == '\n') {
      if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
      }
      normalized.push_back('\n');
      prevSpace = false;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      if (!prevSpace) {
        normalized.push_back(' ');
        prevSpace = true;
      }
    } else {
      normalized.push_back(c);
      prevSpace = false;
    }
  }
  if (trimTrailing) {
    while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\n')) {
      normalized.pop_back();
    }
  }
  if (trimLeading) {
    size_t start = 0;
    while (start < normalized.size() && normalized[start] == ' ') start++;
    if (start > 0) normalized.erase(0, start);
  }
  return normalized;
}

std::string FormatRoundedNumber(float v) {
  if (std::isfinite(v)) {
    float rounded = std::round(v * 1000.0f) / 1000.0f;
    if (rounded == std::floor(rounded) && std::fabs(rounded) < 1e9f) {
      std::ostringstream oss;
      oss << static_cast<long long>(rounded);
      return oss.str();
    }
  }
  std::ostringstream oss;
  oss.precision(6);
  oss << v;
  return oss.str();
}

std::string EmitPx(float px) {
  return FormatRoundedNumber(px) + "px";
}

std::string EmitPercent(float pct) {
  return FormatRoundedNumber(pct) + "%";
}

std::string EmitPaddingShorthand(float top, float right, float bottom, float left) {
  if (top == right && right == bottom && bottom == left) return EmitPx(top);
  if (top == bottom && left == right) return EmitPx(top) + " " + EmitPx(right);
  if (left == right) return EmitPx(top) + " " + EmitPx(right) + " " + EmitPx(bottom);
  return EmitPx(top) + " " + EmitPx(right) + " " + EmitPx(bottom) + " " + EmitPx(left);
}

}  // namespace pagx::html
