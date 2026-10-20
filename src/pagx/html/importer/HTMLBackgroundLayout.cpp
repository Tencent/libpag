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

#include "pagx/html/importer/HTMLBackgroundLayout.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLValueParser.h"

namespace pagx {
using namespace html;

std::string BackgroundListEntry(const std::string& value, size_t index,
                                const std::string& initial) {
  auto entries = SplitTopLevelCommas(value);
  return entries.empty() || Trim(entries[index % entries.size()]).empty()
             ? initial
             : ToLower(Trim(entries[index % entries.size()]));
}

HTMLBackgroundRect BackgroundBoxRect(const HTMLBoxAttributes& box, const std::string& keyword) {
  HTMLBackgroundRect r = {0, 0, box.widthPx, box.heightPx};
  if (keyword == "padding-box" || keyword == "content-box") {
    r.x = r.y = box.borderWidthPx;
    r.width -= 2 * box.borderWidthPx;
    r.height -= 2 * box.borderWidthPx;
  }
  if (keyword == "content-box") {
    r.x += box.padding.left;
    r.y += box.padding.top;
    r.width -= box.padding.left + box.padding.right;
    r.height -= box.padding.top + box.padding.bottom;
  }
  r.width = std::max(0.0f, r.width);
  r.height = std::max(0.0f, r.height);
  return r;
}

namespace {

bool Length(const std::string& value, float base, HTMLValueParser& parser, float& out) {
  std::string s = Trim(value);
  // Chromium serializes edge offsets and mixed percentage/length positions as calc(). Support
  // sums of lengths/percentages; reject other expressions rather than interpreting a prefix.
  if (s.rfind("calc(", 0) == 0 && s.back() == ')') {
    s = s.substr(5, s.size() - 6);
    out = 0;
    size_t at = 0;
    while (at < s.size()) {
      while (at < s.size() && std::isspace(static_cast<unsigned char>(s[at]))) ++at;
      float sign = 1;
      if (at < s.size() && (s[at] == '+' || s[at] == '-')) {
        if (s[at] == '-') sign = -1;
        ++at;
      }
      while (at < s.size() && std::isspace(static_cast<unsigned char>(s[at]))) ++at;
      size_t start = at;
      while (at < s.size() && s[at] != '+' && s[at] != '-' &&
             !std::isspace(static_cast<unsigned char>(s[at])))
        ++at;
      float term = 0;
      if (start == at || !Length(s.substr(start, at - start), base, parser, term)) return false;
      out += sign * term;
    }
    return std::isfinite(out);
  }
  float percent = 0;
  if (ParseCssPercentage(s, percent)) {
    out = base * percent;
    return std::isfinite(out);
  }
  if (s.empty()) return false;
  char* end = nullptr;
  float number = std::strtof(s.c_str(), &end);
  if (end == s.c_str() || !std::isfinite(number)) return false;
  auto unit = ToLower(Trim(end));
  if (!(unit == "px" || unit == "pt" || unit == "pc" || unit == "in" || unit == "cm" ||
        unit == "mm" || unit == "q" || (unit.empty() && number == 0)))
    return false;
  out = parser.parseAbsoluteLengthPx(s);
  return std::isfinite(out);
}

bool IsBox(const std::string& value) {
  return value == "border-box" || value == "padding-box" || value == "content-box";
}

bool IsRepeat(const std::string& value) {
  return value == "repeat" || value == "no-repeat" || value == "space" || value == "round";
}

bool ResolveAxis(float areaStart, float area, float clipStart, float clipSize, float position,
                 const std::string& repeat, HTMLBackgroundAxis& axis) {
  axis.first = areaStart + position;
  axis.step = axis.size;
  axis.count = 1;
  if (repeat == "no-repeat") return true;
  if (repeat == "space") {
    float n = std::floor(area / axis.size);
    if (n < 2) return true;  // CSS uses background-position when fewer than two tiles fit.
    axis.first = areaStart;
    axis.step = (area - axis.size) / (n - 1);
  } else {
    axis.contiguous = true;
  }
  // Repetition extends to the painting area, which may be larger than the positioning area.
  double firstIndex = std::floor((clipStart - axis.first - axis.size) / axis.step) + 1;
  double endIndex = std::ceil((clipStart + clipSize - axis.first) / axis.step);
  double count = std::max(0.0, endIndex - firstIndex);
  if (!std::isfinite(count) || count > 65536) return false;
  axis.first += static_cast<float>(firstIndex) * axis.step;
  axis.count = static_cast<int>(count);
  return true;
}

}  // namespace

bool ResolveBackgroundPosition(const std::string& value, float slackX, float slackY,
                               HTMLValueParser& parser, float& x, float& y) {
  auto tokens = SplitTopLevelWhitespace(ToLower(Trim(value)));
  if (tokens.empty()) tokens = {"0%", "0%"};
  auto isX = [](const std::string& s) { return s == "left" || s == "right"; };
  auto isY = [](const std::string& s) { return s == "top" || s == "bottom"; };
  auto axis = [&](const std::string& token, float slack, float& result) {
    if (token == "center") {
      result = slack * .5f;
      return true;
    }
    if (token == "left" || token == "top") {
      result = 0;
      return true;
    }
    if (token == "right" || token == "bottom") {
      result = slack;
      return true;
    }
    return Length(token, slack, parser, result);
  };
  if (tokens.size() <= 2) {
    if (tokens.size() == 1) {
      if (isY(tokens[0])) tokens.insert(tokens.begin(), "center");
      else
        tokens.push_back("center");
    }
    if (isY(tokens[0]) || isX(tokens[1])) std::swap(tokens[0], tokens[1]);
    return !isY(tokens[0]) && !isX(tokens[1]) && axis(tokens[0], slackX, x) &&
           axis(tokens[1], slackY, y);
  }
  if (tokens.size() > 4) return false;
  bool setX = false, setY = false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& key = tokens[i];
    bool horizontal = isX(key);
    if (!horizontal && !isY(key) && key != "center") return false;
    if (key == "center") horizontal = !setX;
    if (horizontal ? setX : setY) return false;
    float slack = horizontal ? slackX : slackY;
    float result = 0;
    if (!axis(key, slack, result)) return false;
    if (i + 1 < tokens.size() && !isX(tokens[i + 1]) && !isY(tokens[i + 1]) &&
        tokens[i + 1] != "center") {
      float offset = 0;
      if (key == "center" || !Length(tokens[++i], slack, parser, offset)) return false;
      result += (key == "right" || key == "bottom") ? -offset : offset;
    }
    (horizontal ? x : y) = result;
    (horizontal ? setX : setY) = true;
  }
  return setX && setY;
}

bool ResolveBackgroundLayout(const HTMLBoxAttributes& box, size_t index, float intrinsicWidth,
                             float intrinsicHeight, HTMLValueParser& parser,
                             HTMLBackgroundLayout& result) {
  result = {};
  auto origin = BackgroundListEntry(box.backgroundOrigin, index, "padding-box");
  auto clip = BackgroundListEntry(box.backgroundClip, index, "border-box");
  if (!IsBox(origin) || !IsBox(clip)) return false;
  result.origin = BackgroundBoxRect(box, origin);
  result.clip = BackgroundBoxRect(box, clip);
  if (!std::isfinite(result.origin.width) || !std::isfinite(result.origin.height)) return false;
  float w = result.origin.width, h = result.origin.height;
  auto size = BackgroundListEntry(box.backgroundSize, index, "auto");
  auto sizes = SplitTopLevelWhitespace(size);
  bool intrinsic = intrinsicWidth > 0 && intrinsicHeight > 0;
  bool autoX = true, autoY = true;
  float tw = NAN, th = NAN;
  if (size == "contain" || size == "cover") {
    tw = w;
    th = h;
    if (intrinsic) {
      float sx = w / intrinsicWidth, sy = h / intrinsicHeight;
      float scale = size == "contain" ? std::min(sx, sy) : std::max(sx, sy);
      tw = intrinsicWidth * scale;
      th = intrinsicHeight * scale;
    }
    autoX = autoY = false;
  } else {
    if (sizes.empty() || sizes.size() > 2) return false;
    autoX = sizes[0] == "auto";
    autoY = sizes.size() == 1 || sizes[1] == "auto";
    if (!autoX && !Length(sizes[0], w, parser, tw)) return false;
    if (!autoY && !Length(sizes[1], h, parser, th)) return false;
    if (autoX && autoY) {
      tw = intrinsic ? intrinsicWidth : w;
      th = intrinsic ? intrinsicHeight : h;
    } else if (autoX) {
      tw = intrinsic ? th * intrinsicWidth / intrinsicHeight : w;
    } else if (autoY) {
      th = intrinsic ? tw * intrinsicHeight / intrinsicWidth : h;
    }
  }
  if (tw < 0 || th < 0 || !std::isfinite(tw) || !std::isfinite(th)) return false;
  result.x.size = tw;
  result.y.size = th;
  if (tw == 0 || th == 0 || w == 0 || h == 0) return true;
  auto repeats =
      SplitTopLevelWhitespace(BackgroundListEntry(box.backgroundRepeat, index, "repeat"));
  if (repeats.size() == 1) {
    if (repeats[0] == "repeat-x") repeats = {"repeat", "no-repeat"};
    else if (repeats[0] == "repeat-y")
      repeats = {"no-repeat", "repeat"};
    else
      repeats.push_back(repeats[0]);
  }
  if (repeats.size() != 2 || !IsRepeat(repeats[0]) || !IsRepeat(repeats[1])) return false;
  if (repeats[0] == "round") {
    result.x.size = w / std::max(1.0f, std::floor(w / tw + .5f));
    if (intrinsic && autoY && repeats[1] != "round") result.y.size *= result.x.size / tw;
  }
  if (repeats[1] == "round") {
    result.y.size = h / std::max(1.0f, std::floor(h / th + .5f));
    if (intrinsic && autoX && repeats[0] != "round") result.x.size *= result.y.size / th;
  }
  float x = 0, y = 0;
  if (!ResolveBackgroundPosition(BackgroundListEntry(box.backgroundPosition, index, "0% 0%"),
                                 w - result.x.size, h - result.y.size, parser, x, y))
    return false;
  return ResolveAxis(result.origin.x, w, result.clip.x, result.clip.width, x, repeats[0],
                     result.x) &&
         ResolveAxis(result.origin.y, h, result.clip.y, result.clip.height, y, repeats[1],
                     result.y);
}

}  // namespace pagx
