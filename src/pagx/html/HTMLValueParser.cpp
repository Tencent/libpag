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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "pagx/html/HTMLDetail.h"
#include "pagx/html/HTMLParserContext.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using namespace pagx::html;

namespace {

constexpr float kHtmlPi = 3.14159265358979323846f;

}  // namespace

Color HTMLParserContext::parseColor(const std::string& valueRaw) {
  std::string value = Trim(valueRaw);
  if (value.empty()) {
    return {0, 0, 0, 0, ColorSpace::SRGB};
  }
  std::string lowered = ToLower(value);
  if (lowered == "none" || lowered == "transparent") {
    return {0, 0, 0, 0, ColorSpace::SRGB};
  }
  if (value[0] == '#') {
    auto length = value.length();
    if (length == 4 || length == 5) {
      char expanded[9] = {};
      size_t outIdx = 0;
      for (size_t i = 1; i < length; i++) {
        expanded[outIdx++] = value[i];
        expanded[outIdx++] = value[i];
      }
      expanded[outIdx] = '\0';
      uint32_t hex = std::strtoul(expanded, nullptr, 16);
      return HexToColor(hex, /*hasAlpha=*/length == 5);
    }
    if (length == 7 || length == 9) {
      uint32_t hex = std::strtoul(value.c_str() + 1, nullptr, 16);
      return HexToColor(hex, /*hasAlpha=*/length == 9);
    }
  }
  if (value.compare(0, 3, "rgb") == 0) {
    auto open = value.find('(');
    auto close = value.find(')');
    if (open != std::string::npos && close != std::string::npos) {
      std::string inner = value.substr(open + 1, close - open - 1);
      auto comps = ParseFloatList(inner);
      Color color = {};
      color.colorSpace = ColorSpace::SRGB;
      float r = 0, g = 0, b = 0, a = 1.0f;
      if (comps.size() >= 3) {
        r = comps[0];
        g = comps[1];
        b = comps[2];
        if (comps.size() >= 4) a = comps[3];
      }
      color.red = r / 255.0f;
      color.green = g / 255.0f;
      color.blue = b / 255.0f;
      color.alpha = a;
      return color;
    }
  }
  // Named color
  const auto& named = NamedColors();
  auto it = named.find(lowered);
  if (it != named.end()) {
    return HexToColor(it->second, /*hasAlpha=*/false);
  }
  warn("html: unrecognised color value '" + value + "'; falling back to opaque black");
  return {0, 0, 0, 1, ColorSpace::SRGB};
}

float HTMLParserContext::parsePxLength(const std::string& valueRaw) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return NAN;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return NAN;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "px") return num;
  if (suffix == "%") {
    return NAN;  // percent not allowed for properties parsed via parsePxLength
  }
  if (suffix == "em" || suffix == "rem") {
    warn("html: em/rem unit not supported; treated as 16px");
    return num * 16.0f;
  }
  warn("html: length unit '" + suffix + "' not supported; treated as px");
  return num;
}

float HTMLParserContext::resolveLineHeightPx(const std::string& valueRaw, float fontSizePx) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return NAN;
  std::string lowered = ToLower(value);
  if (lowered == "normal") return NAN;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return NAN;
  std::string suffix = Trim(end);
  // Unitless: CSS spec says "the used value is this unitless <number> multiplied by the
  // element's font size". This is the most common authoring form (e.g. "line-height: 1.5") and
  // is what HTML pages such as the maimai sample rely on.
  if (suffix.empty()) {
    if (std::isnan(fontSizePx) || fontSizePx <= 0) return NAN;
    return num * fontSizePx;
  }
  if (suffix == "px") return num;
  if (suffix == "%") {
    if (std::isnan(fontSizePx) || fontSizePx <= 0) return NAN;
    return num * fontSizePx / 100.0f;
  }
  if (suffix == "em" || suffix == "rem") {
    if (std::isnan(fontSizePx) || fontSizePx <= 0) return NAN;
    return num * fontSizePx;
  }
  warn("html: line-height unit '" + suffix + "' not supported");
  return NAN;
}

std::vector<HTMLParserContext::ShadowSpec> HTMLParserContext::parseShadowList(
    const std::string& value) {
  std::vector<ShadowSpec> out;
  if (value.empty()) return out;
  auto items = SplitTopLevelCommas(value);
  for (auto& item : items) {
    auto tokens = SplitTopLevelWhitespace(item);
    if (tokens.empty()) continue;
    ShadowSpec s;
    std::vector<float> lengths;
    std::vector<std::string> nonLengths;
    for (auto& t : tokens) {
      std::string lt = ToLower(t);
      if (lt == "inset") {
        s.inset = true;
        continue;
      }
      // try as length
      char* end = nullptr;
      float num = std::strtof(t.c_str(), &end);
      if (end != t.c_str()) {
        std::string suffix = Trim(end);
        if (suffix.empty() || suffix == "px") {
          lengths.push_back(num);
          continue;
        }
      }
      nonLengths.push_back(t);
    }
    if (lengths.size() >= 2) {
      s.offsetX = lengths[0];
      s.offsetY = lengths[1];
      if (lengths.size() >= 3) s.blur = lengths[2];
      if (lengths.size() >= 4) {
        s.spread = lengths[3];
        if (s.spread != 0) warn("html: box-shadow spread is not supported and was ignored");
      }
    } else {
      warn("html: malformed box-shadow '" + item + "'");
      continue;
    }
    if (!nonLengths.empty()) {
      // Join color tokens back (handles rgb(...) etc.).
      std::string colorStr;
      for (size_t i = 0; i < nonLengths.size(); i++) {
        if (i) colorStr.push_back(' ');
        colorStr += nonLengths[i];
      }
      s.color = parseColor(colorStr);
    } else {
      s.color = {0, 0, 0, 1.0f, ColorSpace::SRGB};
    }
    out.push_back(s);
  }
  return out;
}

std::vector<HTMLParserContext::FilterStep> HTMLParserContext::parseFilterChain(
    const std::string& value) {
  std::vector<FilterStep> out;
  if (value.empty()) return out;
  size_t pos = 0;
  while (pos < value.size()) {
    while (pos < value.size() && std::isspace(static_cast<unsigned char>(value[pos]))) pos++;
    if (pos >= value.size()) break;
    size_t start = pos;
    size_t paren = value.find('(', pos);
    if (paren == std::string::npos) break;
    std::string name = ToLower(Trim(value.substr(start, paren - start)));
    int depth = 1;
    size_t end = paren + 1;
    while (end < value.size() && depth > 0) {
      if (value[end] == '(') depth++;
      else if (value[end] == ')')
        depth--;
      if (depth > 0) end++;
    }
    if (end >= value.size()) break;
    std::string args = value.substr(paren + 1, end - paren - 1);
    FilterStep step;
    step.raw = value.substr(start, end - start + 1);
    if (name == "blur") {
      float b = parsePxLength(args);
      step.kind = FilterStep::Kind::Blur;
      step.blurX = std::isnan(b) ? 0 : b;
      step.blurY = step.blurX;
    } else if (name == "drop-shadow") {
      step.kind = FilterStep::Kind::DropShadow;
      auto shadows = parseShadowList(args);
      if (!shadows.empty()) {
        step.shadow = shadows.front();
      }
    } else {
      step.kind = FilterStep::Kind::Unsupported;
    }
    out.push_back(step);
    pos = end + 1;
  }
  return out;
}

LinearGradient* HTMLParserContext::parseLinearGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  float cssAngle = 180.0f;  // CSS default: to bottom
  size_t stopStart = 0;
  std::string first = Trim(parts[0]);
  std::string firstLower = ToLower(first);
  if (firstLower.compare(0, 3, "to ") == 0) {
    cssAngle = CssDirectionToAngle(firstLower);
    stopStart = 1;
  } else if (firstLower.find("deg") != std::string::npos ||
             firstLower.find("rad") != std::string::npos ||
             firstLower.find("turn") != std::string::npos) {
    cssAngle = ParseAngle(first);
    stopStart = 1;
  }
  GradientStops stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false);
  if (!finaliseGradientStops(stops)) return nullptr;

  auto grad = _document->makeNode<LinearGradient>();
  float angle = CssToPagxAngle(cssAngle) * kHtmlPi / 180.0f;
  float cx = 0.5f, cy = 0.5f;
  float half = 0.5f;
  grad->startPoint = {cx - std::cos(angle) * half, cy - std::sin(angle) * half};
  grad->endPoint = {cx + std::cos(angle) * half, cy + std::sin(angle) * half};
  emitColorStops(grad->colorStops, stops);
  return grad;
}

RadialGradient* HTMLParserContext::parseRadialGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  size_t stopStart = 0;
  // Allow leading shape descriptor like "circle at center", "ellipse 50% 50%", etc.
  std::string first = ToLower(Trim(parts[0]));
  if (first.find("circle") != std::string::npos || first.find("ellipse") != std::string::npos ||
      first.find("at") != std::string::npos) {
    stopStart = 1;
  }
  GradientStops stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false);
  if (!finaliseGradientStops(stops)) return nullptr;

  auto grad = _document->makeNode<RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  emitColorStops(grad->colorStops, stops);
  return grad;
}

ConicGradient* HTMLParserContext::parseConicGradient(const std::string& value) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return nullptr;
  auto parts = SplitTopLevelCommas(args);
  if (parts.size() < 2) return nullptr;
  size_t stopStart = 0;
  float cssAngle = 0.0f;
  std::string first = ToLower(Trim(parts[0]));
  if (first.compare(0, 5, "from ") == 0) {
    cssAngle = ParseAngle(first.substr(5));
    stopStart = 1;
  }
  GradientStops stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/true);
  if (!finaliseGradientStops(stops)) return nullptr;

  auto grad = _document->makeNode<ConicGradient>();
  grad->center = {0.5f, 0.5f};
  grad->startAngle = CssToPagxAngle(cssAngle);
  grad->endAngle = grad->startAngle + 360.0f;
  emitColorStops(grad->colorStops, stops);
  return grad;
}

HTMLParserContext::GradientStops HTMLParserContext::parseGradientStops(
    const std::vector<std::string>& parts, size_t startIndex, bool interpretAngularOffset) {
  GradientStops stops;
  for (size_t i = startIndex; i < parts.size(); i++) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    float offset = NAN;
    if (tokens.size() >= 2) {
      const std::string& off = tokens[1];
      if (!off.empty() && off.back() == '%') {
        offset = std::strtof(off.c_str(), nullptr) / 100.0f;
      } else if (interpretAngularOffset && !off.empty() && off.find("deg") != std::string::npos) {
        offset = ParseAngle(off) / 360.0f;
      } else if (!interpretAngularOffset) {
        float v = parsePxLength(off);
        if (!std::isnan(v)) offset = v;
      }
    }
    stops.emplace_back(offset, color);
  }
  return stops;
}

bool HTMLParserContext::finaliseGradientStops(GradientStops& stops) {
  if (stops.empty()) return false;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  for (size_t i = 1; i + 1 < stops.size(); i++) {
    if (std::isnan(stops[i].first)) {
      // Linear interpolation between bracketing known stops.
      size_t prev = i - 1;
      size_t next = i + 1;
      while (next < stops.size() && std::isnan(stops[next].first)) next++;
      if (next < stops.size()) {
        float span = stops[next].first - stops[prev].first;
        float steps = static_cast<float>(next - prev);
        stops[i].first = stops[prev].first + span * 1.0f / steps;
      } else {
        stops[i].first = 1.0f;
      }
    }
  }
  return true;
}

template <typename T>
void HTMLParserContext::emitColorStops(T& targetStops, const GradientStops& stops) {
  for (const auto& [offset, color] : stops) {
    auto stop = _document->makeNode<ColorStop>();
    stop->offset = offset;
    stop->color = color;
    targetStops.push_back(stop);
  }
}

}  // namespace pagx
