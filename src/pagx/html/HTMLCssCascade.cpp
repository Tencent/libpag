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

#include "pagx/html/HTMLCssCascade.h"
#include <cctype>
#include "pagx/html/HTMLParserContext.h"

namespace pagx::css_cascade {

using pagx::detail::ToLower;
using pagx::detail::Trim;

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
                                           std::vector<std::string>& droppedAtRules) {
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

}  // namespace pagx::css_cascade
