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

#include "pagx/html/importer/HTMLCssCascade.h"
#include <cctype>
#include "pagx/html/importer/HTMLDetail.h"

namespace pagx::html {

namespace {

bool IsIdentChar(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) || c == '-' || c == '_';
}

// Skip a balanced brace block starting at `pos`, which must point at the opening `{`. Returns
// the position right after the matching `}`, or `css.size()` when no match is found.
size_t SkipBalancedBlock(const std::string& css, size_t pos) {
  if (pos >= css.size() || css[pos] != '{') return pos;
  int depth = 0;
  for (size_t i = pos; i < css.size(); i++) {
    char c = css[i];
    if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') {
      auto end = css.find("*/", i + 2);
      i = (end == std::string::npos) ? css.size() - 1 : end + 1;
      continue;
    }
    if (c == '{') {
      depth++;
    } else if (c == '}') {
      depth--;
      if (depth == 0) return i + 1;
    }
  }
  return css.size();
}

void SkipWhitespaceAndComments(const std::string& css, size_t& pos) {
  while (pos < css.size()) {
    char c = css[pos];
    if (std::isspace(static_cast<unsigned char>(c))) {
      pos++;
      continue;
    }
    if (c == '/' && pos + 1 < css.size() && css[pos + 1] == '*') {
      auto end = css.find("*/", pos + 2);
      pos = (end == std::string::npos) ? css.size() : end + 2;
      continue;
    }
    break;
  }
}

// Resolves a single `@keyframes` stop selector token (`0%`, `from`, `to`, ...) to a percentage in
// the [0, 100] range. Returns false when the token is not a recognised offset.
bool ParseKeyframeOffset(const std::string& token, float& outPercent) {
  std::string t = Trim(token);
  if (t.empty()) return false;
  std::string lc = ToLower(t);
  if (lc == "from") {
    outPercent = 0.0f;
    return true;
  }
  if (lc == "to") {
    outPercent = 100.0f;
    return true;
  }
  if (t.back() != '%') return false;
  char* end = nullptr;
  float v = std::strtof(t.c_str(), &end);
  if (end == t.c_str()) return false;
  // The trailing `%` must immediately follow the parsed number; otherwise tokens like `12px%`
  // would be silently accepted as 12%. `strtof` leaves `end` on the first non-numeric character,
  // which must be the final `%` of the trimmed token.
  if (end != t.c_str() + t.size() - 1) return false;
  if (v < 0.0f) v = 0.0f;
  if (v > 100.0f) v = 100.0f;
  outPercent = v;
  return true;
}

// Parses the body of a `@keyframes` block (the text between the outer braces) into stops. Each
// inner rule is `<offset[, offset]*> { declarations }`.
void ParseKeyframesBody(const std::string& body, CssKeyframesRule& rule) {
  size_t pos = 0;
  while (pos < body.size()) {
    SkipWhitespaceAndComments(body, pos);
    if (pos >= body.size()) break;
    size_t bracePos = std::string::npos;
    for (size_t i = pos; i < body.size(); i++) {
      char c = body[i];
      if (c == '/' && i + 1 < body.size() && body[i + 1] == '*') {
        auto end = body.find("*/", i + 2);
        i = (end == std::string::npos) ? body.size() - 1 : end + 1;
        continue;
      }
      if (c == '{') {
        bracePos = i;
        break;
      }
      if (c == '}') break;
    }
    if (bracePos == std::string::npos) break;
    std::string selectors = Trim(body.substr(pos, bracePos - pos));
    size_t blockEnd = SkipBalancedBlock(body, bracePos);
    std::string decls = Trim(body.substr(bracePos + 1, blockEnd - bracePos - 2));
    // Expand comma-separated offsets into one stop each.
    size_t selPos = 0;
    while (selPos <= selectors.size()) {
      size_t comma = selectors.find(',', selPos);
      std::string token =
          selectors.substr(selPos, comma == std::string::npos ? std::string::npos : comma - selPos);
      float percent = 0.0f;
      if (ParseKeyframeOffset(token, percent)) {
        CssKeyframeStop stop = {};
        stop.percent = percent;
        stop.declarations = decls;
        rule.stops.push_back(std::move(stop));
      }
      if (comma == std::string::npos) break;
      selPos = comma + 1;
    }
    pos = blockEnd;
  }
}

uint32_t HexDigitValue(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<uint32_t>(c - '0');
  }
  return static_cast<uint32_t>(std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
}

void AppendUTF8(uint32_t codepoint, std::string& out) {
  if (codepoint == 0 || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
    codepoint = 0xFFFD;
  }
  if (codepoint < 0x80) {
    out += static_cast<char>(codepoint);
  } else if (codepoint < 0x800) {
    out += static_cast<char>(0xC0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint < 0x10000) {
    out += static_cast<char>(0xE0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codepoint & 0x3F));
  }
}

// Consumes the CSS escape whose backslash precedes `pos`, appends the character it denotes to
// `out`, and returns the position after it. Per CSS Syntax, a hex escape takes up to six digits
// plus one optional whitespace terminator, and a backslash-newline is a line continuation.
size_t ConsumeCssEscape(const std::string& css, size_t pos, std::string& out) {
  if (pos >= css.size()) {
    return pos;
  }
  if (std::isxdigit(static_cast<unsigned char>(css[pos]))) {
    uint32_t codepoint = 0;
    size_t end = pos;
    while (end < css.size() && end - pos < 6 &&
           std::isxdigit(static_cast<unsigned char>(css[end]))) {
      codepoint = codepoint * 16 + HexDigitValue(css[end]);
      end++;
    }
    if (end < css.size() && std::isspace(static_cast<unsigned char>(css[end]))) {
      end++;
    }
    AppendUTF8(codepoint, out);
    return end;
  }
  if (css[pos] != '\n') {
    out += css[pos];
  }
  return pos + 1;
}

// Reads the CSS string whose opening quote is at `pos` into `out` (escapes resolved) and returns
// the position after the closing quote.
size_t ReadCssString(const std::string& css, size_t pos, std::string& out) {
  char quote = css[pos];
  size_t i = pos + 1;
  while (i < css.size() && css[i] != quote) {
    if (css[i] == '\\') {
      i = ConsumeCssEscape(css, i + 1, out);
    } else {
      out += css[i++];
    }
  }
  return i < css.size() ? i + 1 : i;
}

std::string UnescapeCss(const std::string& text) {
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '\\') {
      i = ConsumeCssEscape(text, i + 1, out);
    } else {
      out += text[i++];
    }
  }
  return out;
}

std::string QuoteCssString(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\a ";
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}

std::string ParseFontFaceFamily(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  if (value[0] == '"' || value[0] == '\'') {
    std::string family;
    ReadCssString(value, 0, family);
    return family;
  }
  // An unquoted family is a sequence of identifiers, which CSS joins with single spaces.
  std::string family;
  for (auto& word : SplitTopLevelWhitespace(UnescapeCss(value))) {
    if (!family.empty()) {
      family += ' ';
    }
    family += word;
  }
  return family;
}

bool StartsWithUrlFunction(const std::string& value, size_t pos) {
  if (pos + 4 > value.size() || ToLower(value.substr(pos, 4)) != "url(") {
    return false;
  }
  return pos == 0 || !IsIdentChar(value[pos - 1]);
}

std::vector<std::string> ParseFontFaceSources(const std::string& value) {
  std::vector<std::string> sources;
  size_t i = 0;
  while (i < value.size()) {
    char c = value[i];
    // Quoted strings outside url() belong to local() / format() and are skipped whole, so a quote
    // or parenthesis inside them cannot desynchronise the scan.
    if (c == '"' || c == '\'') {
      std::string skipped;
      i = ReadCssString(value, i, skipped);
      continue;
    }
    if (!StartsWithUrlFunction(value, i)) {
      i++;
      continue;
    }
    size_t p = i + 4;
    while (p < value.size() && std::isspace(static_cast<unsigned char>(value[p]))) {
      p++;
    }
    std::string url;
    if (p < value.size() && (value[p] == '"' || value[p] == '\'')) {
      p = ReadCssString(value, p, url);
      while (p < value.size() && value[p] != ')') {
        p++;
      }
    } else {
      std::string raw;
      while (p < value.size() && value[p] != ')') {
        if (value[p] == '\\') {
          p = ConsumeCssEscape(value, p + 1, raw);
        } else {
          raw += value[p++];
        }
      }
      url = Trim(raw);
    }
    if (!url.empty()) {
      sources.push_back(std::move(url));
    }
    i = p < value.size() ? p + 1 : p;
  }
  return sources;
}

void ApplyFontFaceDescriptor(const std::string& declaration, CssFontFaceRule& rule) {
  // Descriptor names never contain a colon, so the first one ends the name.
  auto colon = declaration.find(':');
  if (colon == std::string::npos) {
    return;
  }
  std::string name = ToLower(Trim(declaration.substr(0, colon)));
  std::string value = Trim(declaration.substr(colon + 1));
  if (name == "font-family") {
    rule.family = ParseFontFaceFamily(value);
  } else if (name == "src") {
    rule.sources = ParseFontFaceSources(value);
  }
}

// Splits an `@font-face` body on the semicolons that end its descriptors. Semicolons inside
// quotes, parentheses (an unquoted data: URL) or comments do not end a descriptor.
CssFontFaceRule ParseFontFaceBody(const std::string& body) {
  CssFontFaceRule rule = {};
  std::string declaration;
  int parenDepth = 0;
  size_t i = 0;
  while (i < body.size()) {
    char c = body[i];
    if (c == '/' && i + 1 < body.size() && body[i + 1] == '*') {
      auto end = body.find("*/", i + 2);
      i = end == std::string::npos ? body.size() : end + 2;
      continue;
    }
    if (c == '"' || c == '\'') {
      std::string skipped;
      size_t end = ReadCssString(body, i, skipped);
      declaration += body.substr(i, end - i);
      i = end;
      continue;
    }
    if (c == '(') {
      parenDepth++;
    } else if (c == ')' && parenDepth > 0) {
      parenDepth--;
    } else if (c == ';' && parenDepth == 0) {
      ApplyFontFaceDescriptor(declaration, rule);
      declaration.clear();
      i++;
      continue;
    }
    declaration += c;
    i++;
  }
  ApplyFontFaceDescriptor(declaration, rule);
  return rule;
}

}  // namespace

ParsedSelector ClassifySelector(const std::string& selector) {
  ParsedSelector result = {};
  result.raw = selector;
  if (selector.empty()) {
    return result;
  }
  if (selector == "*") {
    result.kind = SelectorKind::Universal;
    return result;
  }
  // Class selector: a single `.ident` with no further punctuation.
  if (selector[0] == '.') {
    std::string cls = selector.substr(1);
    if (cls.empty()) return result;
    for (char c : cls) {
      if (!IsIdentChar(c)) {
        return result;  // descendant, pseudo, attribute, etc.
      }
    }
    result.kind = SelectorKind::Class;
    result.key = std::move(cls);
    return result;
  }
  // Element selector: identifier consisting of ASCII letters/digits/`-`/`_`.
  for (char c : selector) {
    if (!IsIdentChar(c)) {
      return result;
    }
  }
  result.kind = SelectorKind::Element;
  result.key = ToLower(selector);
  return result;
}

std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules,
                                           std::vector<CssKeyframesRule>& keyframesRules,
                                           std::vector<CssFontFaceRule>& fontFaceRules) {
  std::vector<RawCssRule> rules;
  size_t pos = 0;
  while (pos < css.size()) {
    SkipWhitespaceAndComments(css, pos);
    if (pos >= css.size()) break;

    if (css[pos] == '@') {
      // Consume the at-rule prelude up to either `;` (no-block rule) or `{` (block rule).
      size_t start = pos;
      size_t semi = std::string::npos;
      size_t brace = std::string::npos;
      for (size_t i = pos; i < css.size(); i++) {
        char c = css[i];
        if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') {
          auto end = css.find("*/", i + 2);
          i = (end == std::string::npos) ? css.size() - 1 : end + 1;
          continue;
        }
        if (c == ';') {
          semi = i;
          break;
        }
        if (c == '{') {
          brace = i;
          break;
        }
      }
      std::string head =
          Trim(css.substr(start, (semi != std::string::npos ? semi : brace) - start));
      // `@keyframes <name>` / `@-webkit-keyframes <name>` are captured rather than dropped so the
      // animation subset can map them onto PAGX animations.
      auto preludeTokens = SplitTopLevelWhitespace(head);
      std::string atName = preludeTokens.empty() ? "" : ToLower(preludeTokens[0]);
      bool isKeyframes = (atName == "@keyframes" || atName == "@-webkit-keyframes");
      if (isKeyframes && brace != std::string::npos && preludeTokens.size() >= 2) {
        CssKeyframesRule rule = {};
        rule.name = preludeTokens[1];
        size_t blockEnd = SkipBalancedBlock(css, brace);
        std::string body = css.substr(brace + 1, blockEnd - brace - 2);
        ParseKeyframesBody(body, rule);
        if (!rule.stops.empty()) {
          keyframesRules.push_back(std::move(rule));
        }
        pos = blockEnd;
        continue;
      }
      if (atName == "@font-face" && brace != std::string::npos) {
        size_t blockEnd = SkipBalancedBlock(css, brace);
        auto rule = ParseFontFaceBody(css.substr(brace + 1, blockEnd - brace - 2));
        if (!rule.family.empty() && !rule.sources.empty()) {
          fontFaceRules.push_back(std::move(rule));
          pos = blockEnd;
          continue;
        }
      }
      droppedAtRules.push_back(head);
      if (brace != std::string::npos) {
        pos = SkipBalancedBlock(css, brace);
      } else if (semi != std::string::npos) {
        pos = semi + 1;
      } else {
        pos = css.size();
      }
      continue;
    }

    size_t bracePos = std::string::npos;
    for (size_t i = pos; i < css.size(); i++) {
      char c = css[i];
      if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') {
        auto end = css.find("*/", i + 2);
        i = (end == std::string::npos) ? css.size() - 1 : end + 1;
        continue;
      }
      if (c == '{') {
        bracePos = i;
        break;
      }
      if (c == '}') {
        // Stray closing brace — recover by skipping it.
        break;
      }
    }
    if (bracePos == std::string::npos) {
      break;
    }
    std::string selectors = Trim(css.substr(pos, bracePos - pos));
    size_t blockEnd = SkipBalancedBlock(css, bracePos);
    if (blockEnd == css.size() && (blockEnd <= bracePos || css[css.size() - 1] != '}')) {
      // Unterminated block; bail.
      break;
    }
    std::string body = css.substr(bracePos + 1, blockEnd - bracePos - 2);
    if (!selectors.empty()) {
      RawCssRule rule = {};
      rule.selectors = std::move(selectors);
      rule.declarations = Trim(body);
      rules.push_back(std::move(rule));
    }
    pos = blockEnd;
  }
  return rules;
}

std::string SerializeKeyframes(const std::vector<CssKeyframesRule>& keyframesRules) {
  std::string out;
  for (const auto& rule : keyframesRules) {
    out += "@keyframes ";
    out += rule.name;
    out += " {";
    for (const auto& stop : rule.stops) {
      out += ' ';
      out += FormatRoundedNumber(stop.percent);
      out += "% { ";
      out += stop.declarations;
      // Ensure the declaration block is terminated so re-parsing stays robust.
      if (!stop.declarations.empty() && stop.declarations.back() != ';') {
        out += ';';
      }
      out += " }";
    }
    out += " }\n";
  }
  return out;
}

std::string SerializeFontFaces(const std::vector<CssFontFaceRule>& fontFaceRules) {
  std::string out;
  for (const auto& rule : fontFaceRules) {
    out += "@font-face { font-family: ";
    out += QuoteCssString(rule.family);
    out += "; src: ";
    for (size_t i = 0; i < rule.sources.size(); i++) {
      if (i > 0) {
        out += ", ";
      }
      out += "url(";
      out += QuoteCssString(rule.sources[i]);
      out += ')';
    }
    out += "; }\n";
  }
  return out;
}

}  // namespace pagx::html
