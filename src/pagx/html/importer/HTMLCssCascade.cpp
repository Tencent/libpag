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
                                           std::vector<CssKeyframesRule>& keyframesRules) {
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

std::vector<RawCssRule> TokenizeStyleSheet(const std::string& css,
                                           std::vector<std::string>& droppedAtRules) {
  std::vector<CssKeyframesRule> ignored;
  return TokenizeStyleSheet(css, droppedAtRules, ignored);
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

}  // namespace pagx::html
