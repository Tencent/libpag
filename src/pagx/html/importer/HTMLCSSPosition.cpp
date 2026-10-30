/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/importer/HTMLCSSPosition.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLValueParser.h"

namespace pagx {
namespace {
struct Value {
  double number = 0;
  double percent = 0;
  double pixels = 0;
  bool length = false;

  void scale(double factor) {
    number *= factor;
    percent *= factor;
    pixels *= factor;
  }
};

class LengthParser {
 public:
  LengthParser(const std::string& source, HTMLValueParser& parser)
      : source(source), parser(parser) {
  }

  bool parse(HTMLLengthPercentage& result) {
    Value value;
    if (!primary(value, 0)) return false;
    whitespace();
    if (at != source.size() || (!value.length && value.number != 0)) return false;
    result = {static_cast<float>(value.percent), static_cast<float>(value.pixels)};
    return std::isfinite(result.percent) && std::isfinite(result.pixels);
  }

 private:
  void whitespace() {
    while (at < source.size() && std::isspace(static_cast<unsigned char>(source[at]))) ++at;
  }

  bool primary(Value& value, int depth) {
    if (depth > 32) return false;
    whitespace();
    if (source.compare(at, 5, "calc(") == 0) {
      at += 5;
      if (!sum(value, depth + 1)) return false;
      whitespace();
      return at < source.size() && source[at++] == ')';
    }
    if (at < source.size() && source[at] == '(' && depth > 0) {
      ++at;
      if (!sum(value, depth + 1)) return false;
      whitespace();
      return at < source.size() && source[at++] == ')';
    }
    const size_t start = at;
    size_t digits = at;
    if (digits < source.size() && (source[digits] == '+' || source[digits] == '-')) ++digits;
    if (source.compare(digits, 2, "0x") == 0) return false;
    char* end = nullptr;
    double number = std::strtod(source.c_str() + at, &end);
    if (end == source.c_str() + at || !std::isfinite(number)) return false;
    at = static_cast<size_t>(end - source.c_str());
    const size_t unitStart = at;
    while (at < source.size() && std::isalpha(static_cast<unsigned char>(source[at]))) ++at;
    if (at < source.size() && source[at] == '%') ++at;
    auto unit = source.substr(unitStart, at - unitStart);
    value = {};
    if (unit.empty()) {
      value.number = number;
      return true;
    }
    value.length = true;
    if (unit == "%") {
      value.percent = number / 100;
      return true;
    }
    if (unit != "px" && unit != "pt" && unit != "pc" && unit != "in" && unit != "cm" &&
        unit != "mm" && unit != "q")
      return false;
    value.pixels = parser.parseAbsoluteLengthPx(source.substr(start, at - start));
    return std::isfinite(value.pixels);
  }

  bool product(Value& value, int depth) {
    if (!primary(value, depth)) return false;
    while (true) {
      size_t beforeSpace = at;
      whitespace();
      if (at == source.size() || (source[at] != '*' && source[at] != '/')) {
        at = beforeSpace;
        return true;
      }
      char op = source[at++];
      Value other;
      if (!primary(other, depth)) return false;
      if (op == '/') {
        if (other.length || other.number == 0) return false;
        value.scale(1 / other.number);
      } else if (!other.length) {
        value.scale(other.number);
      } else if (!value.length) {
        other.scale(value.number);
        value = other;
      } else
        return false;
    }
  }

  bool sum(Value& value, int depth) {
    if (!product(value, depth)) return false;
    while (true) {
      size_t beforeSpace = at;
      whitespace();
      if (at == source.size() || (source[at] != '+' && source[at] != '-')) return true;
      // CSS calc binary + and - require whitespace on both sides.
      if (at == beforeSpace || at + 1 == source.size() ||
          !std::isspace(static_cast<unsigned char>(source[at + 1])))
        return false;
      char op = source[at++];
      Value other;
      if (!product(other, depth) || value.length != other.length) return false;
      other.scale(op == '+' ? 1 : -1);
      value.number += other.number;
      value.percent += other.percent;
      value.pixels += other.pixels;
    }
  }

  const std::string& source;
  HTMLValueParser& parser;
  size_t at = 0;
};
}  // namespace

bool ParseHTMLLengthPercentage(const std::string& value, HTMLValueParser& parser,
                               HTMLLengthPercentage& result) {
  auto source = html::ToLower(html::Trim(value));
  return !source.empty() && LengthParser(source, parser).parse(result);
}

bool ParseHTMLCSSPosition(const std::string& value, HTMLValueParser& parser,
                          HTMLCSSPosition& result, bool allowBackgroundSyntax) {
  auto tokens = html::SplitTopLevelWhitespace(html::ToLower(html::Trim(value)));
  if (tokens.empty()) return false;
  auto isX = [](const std::string& s) { return s == "left" || s == "right"; };
  auto isY = [](const std::string& s) { return s == "top" || s == "bottom"; };
  auto keyword = [&](const std::string& s) { return isX(s) || isY(s) || s == "center"; };
  auto axis = [&](const std::string& token, HTMLLengthPercentage& out) {
    if (keyword(token)) {
      out = {token == "center" ? .5f : (token == "right" || token == "bottom" ? 1.f : 0.f), 0};
      return true;
    }
    return ParseHTMLLengthPercentage(token, parser, out);
  };
  HTMLCSSPosition parsed;
  if (tokens.size() <= 2) {
    if (tokens.size() == 1) {
      if (isY(tokens[0])) tokens.insert(tokens.begin(), "center");
      else
        tokens.push_back("center");
    }
    if (keyword(tokens[0]) && keyword(tokens[1]) && (isY(tokens[0]) || isX(tokens[1])))
      std::swap(tokens[0], tokens[1]);
    if (isY(tokens[0]) || isX(tokens[1]) || !axis(tokens[0], parsed.x) ||
        !axis(tokens[1], parsed.y))
      return false;
  } else {
    if (tokens.size() != 4 && !(allowBackgroundSyntax && tokens.size() == 3)) return false;
    bool setX = false, setY = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
      const auto& key = tokens[i];
      bool horizontal = isX(key);
      if (!horizontal && !isY(key) && !(allowBackgroundSyntax && key == "center")) return false;
      if (key == "center") {
        horizontal = !setX;
        // In the background-only three-token grammar, center can precede the other axis.
        if (i == 0 && i + 1 < tokens.size()) horizontal = isY(tokens[i + 1]);
      }
      if (horizontal ? setX : setY) return false;
      HTMLLengthPercentage position;
      axis(key, position);
      if (i + 1 < tokens.size() && !keyword(tokens[i + 1])) {
        HTMLLengthPercentage offset;
        if (key == "center" || !ParseHTMLLengthPercentage(tokens[++i], parser, offset))
          return false;
        float sign = key == "right" || key == "bottom" ? -1 : 1;
        position.percent += offset.percent * sign;
        position.pixels += offset.pixels * sign;
      } else if (!allowBackgroundSyntax)
        return false;
      (horizontal ? parsed.x : parsed.y) = position;
      (horizontal ? setX : setY) = true;
    }
    if (!setX || !setY) return false;
  }
  result = parsed;
  return true;
}
}  // namespace pagx
