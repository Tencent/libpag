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
#include "pagx/html/importer/HTMLCSSPosition.h"
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
    r.x = box.borderWidths.left;
    r.y = box.borderWidths.top;
    r.width -= box.borderWidths.left + box.borderWidths.right;
    r.height -= box.borderWidths.top + box.borderWidths.bottom;
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
  HTMLLengthPercentage length;
  if (!ParseHTMLLengthPercentage(value, parser, length)) return false;
  out = length.resolve(base);
  return std::isfinite(out);
}

bool IsBox(const std::string& value) {
  return value == "border-box" || value == "padding-box" || value == "content-box" ||
         value == "text";
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
  HTMLCSSPosition position;
  if (!ParseHTMLCSSPosition(value.empty() ? "0% 0%" : value, parser, position, true)) return false;
  x = position.x.resolve(slackX);
  y = position.y.resolve(slackY);
  return std::isfinite(x) && std::isfinite(y);
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
