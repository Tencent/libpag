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

#include "pagx/html/importer/HTMLValueParser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using namespace pagx::html;

namespace {

constexpr float HtmlPi = 3.14159265358979323846f;

// Pulls the comma-separated argument list of a `xxx-gradient(...)` call. Returns false when the
// value carries no `(...)` body or fewer than two top-level comma-separated parts (the minimum
// for a usable gradient: a leading angle/shape descriptor or a stop, plus at least one stop).
bool ExtractGradientParts(const std::string& value, std::vector<std::string>& outParts) {
  std::string args = ExtractParenArgs(value);
  if (args.empty()) return false;
  outParts = SplitTopLevelCommas(args);
  return outParts.size() >= 2;
}

}  // namespace

HTMLValueParser::HTMLValueParser(HTMLDiagnosticSink& sink, const float& canvasWidth,
                                 const float& canvasHeight)
    : _diagnostics(sink), _canvasWidth(canvasWidth), _canvasHeight(canvasHeight) {
}

void HTMLValueParser::bindDocument(PAGXDocument* document) {
  _document = document;
}

Color HTMLValueParser::parseColor(const std::string& valueRaw) {
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
    if (length == 4 || length == 5 || length == 7 || length == 9) {
      char expanded[9] = {};
      size_t outIdx = 0;
      if (length == 4 || length == 5) {
        for (size_t i = 1; i < length; i++) {
          expanded[outIdx++] = value[i];
          expanded[outIdx++] = value[i];
        }
      } else {
        for (size_t i = 1; i < length; i++) {
          expanded[outIdx++] = value[i];
        }
      }
      expanded[outIdx] = '\0';
      // strtoul stops at the first non-hex character and silently returns the prefix value,
      // so '#ZZZZZZ' would otherwise parse to 0 (opaque black) without any diagnostic.
      // Validate via endptr that every digit was consumed before trusting the result.
      char* endPtr = nullptr;
      uint32_t hex = std::strtoul(expanded, &endPtr, 16);
      if (endPtr != nullptr && *endPtr == '\0') {
        bool hasAlpha = (length == 5 || length == 9);
        return HexToColor(hex, hasAlpha);
      }
      _diagnostics.warn("html: malformed hex color '" + value + "'; falling back to opaque black");
      return {0, 0, 0, 1, ColorSpace::SRGB};
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
  // CSS Color 4 color() function. The exporter round-trips DisplayP3 fills as
  // `color(display-p3 r g b)` / `color(display-p3 r g b / a)` with channels already in [0, 1],
  // so without this branch every wide-gamut swatch falls through to the unrecognised-color
  // path and renders opaque black.
  if (lowered.compare(0, 6, "color(") == 0) {
    auto open = value.find('(');
    auto close = value.rfind(')');
    if (open != std::string::npos && close != std::string::npos && close > open) {
      std::string inner = Trim(value.substr(open + 1, close - open - 1));
      std::string loweredInner = ToLower(inner);
      if (loweredInner.compare(0, 11, "display-p3 ") == 0 ||
          loweredInner.compare(0, 11, "display-p3\t") == 0) {
        std::string channels = inner.substr(10);
        float alpha = 1.0f;
        auto slash = channels.find('/');
        if (slash != std::string::npos) {
          auto alphaComps = ParseFloatList(channels.substr(slash + 1));
          if (!alphaComps.empty()) {
            alpha = alphaComps[0];
          }
          channels = channels.substr(0, slash);
        }
        auto comps = ParseFloatList(channels);
        if (comps.size() >= 3) {
          Color color = {};
          color.colorSpace = ColorSpace::DisplayP3;
          color.red = comps[0];
          color.green = comps[1];
          color.blue = comps[2];
          color.alpha = alpha;
          return color;
        }
      }
    }
    _diagnostics.warn("html: unsupported color() value '" + value +
                      "'; falling back to opaque black");
    return {0, 0, 0, 1, ColorSpace::SRGB};
  }
  // CSS hsl()/hsla() (CSS Color 3 comma syntax + CSS Color 4 space syntax). Authored CSS
  // such as `background: hsl(120 100% 50%)` reaches us verbatim because the snapshot stage
  // can leave the function call intact (Chrome only normalises `hsl()` to `rgb()` on certain
  // computed-style channels). Without this the value falls through to the unrecognised-color
  // diagnostic and we render opaque black, which is what HUD-style canvases hit.
  if (lowered.compare(0, 3, "hsl") == 0) {
    Color hsl = {};
    if (ParseCSSHSLColor(value, hsl)) {
      return hsl;
    }
  }
  // Named color
  const auto& named = NamedColors();
  auto it = named.find(lowered);
  if (it != named.end()) {
    return HexToColor(it->second, /*hasAlpha=*/false);
  }
  _diagnostics.warn("html: unrecognised color value '" + value + "'; falling back to opaque black");
  return {0, 0, 0, 1, ColorSpace::SRGB};
}

float HTMLValueParser::parseAbsoluteLengthPx(const std::string& valueRaw) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return NAN;
  char* end = nullptr;
  float num = std::strtof(value.c_str(), &end);
  if (end == value.c_str()) return NAN;
  std::string suffix = ToLower(Trim(end));
  if (suffix == "%") {
    return NAN;  // percent not allowed for properties parsed via parseAbsoluteLengthPx
  }
  bool recognized = false;
  // fontSize is not known at this layer (see ResolveLength for the context-aware path); pass NaN
  // so em falls back to 16px to match the long-standing behaviour. _canvasWidth / _canvasHeight
  // are populated before any per-element parseAbsoluteLengthPx call; they are 0 only during canvas-size
  // resolution itself, where vw/vh are necessarily a self-referential mistake.
  float px = ConvertCssLengthToPx(num, suffix, /*fontSizePx=*/NAN, _canvasWidth, _canvasHeight,
                                  recognized);
  if (recognized) {
    if (suffix == "em" || suffix == "rem") {
      _diagnostics.warn("html: em/rem unit not supported here; treated as 16px");
    }
    return px;
  }
  _diagnostics.warn("html: length unit '" + suffix + "' not supported; treated as px");
  return num;
}

float HTMLValueParser::resolveLineHeightPx(const std::string& valueRaw, float fontSizePx) {
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
  _diagnostics.warn("html: line-height unit '" + suffix + "' not supported");
  return NAN;
}

std::vector<HTMLValueParser::ShadowSpec> HTMLValueParser::parseShadowList(
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
      if (lengths.size() >= 4 && lengths[3] != 0) {
        _diagnostics.warn("html: box-shadow spread is not supported and was ignored");
      }
    } else {
      _diagnostics.warn("html: malformed box-shadow '" + item + "'");
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

std::vector<HTMLValueParser::FilterStep> HTMLValueParser::parseFilterChain(
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
    if (end >= value.size()) {
      // The opener at `paren` never matched a closer. Warn and stop so callers see a
      // diagnostic instead of silently dropping the remainder of the chain.
      _diagnostics.warn("html: unmatched '(' in filter '" + value + "'; remainder ignored");
      break;
    }
    std::string args = value.substr(paren + 1, end - paren - 1);
    FilterStep step;
    step.raw = value.substr(start, end - start + 1);
    if (name == "blur") {
      float b = parseAbsoluteLengthPx(args);
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

LinearGradient* HTMLValueParser::parseLinearGradient(const std::string& value) {
  std::vector<std::string> parts;
  if (!ExtractGradientParts(value, parts)) return nullptr;
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
  float angle = CssToPagxAngle(cssAngle) * HtmlPi / 180.0f;
  float cx = 0.5f, cy = 0.5f;
  float half = 0.5f;
  grad->startPoint = {cx - std::cos(angle) * half, cy - std::sin(angle) * half};
  grad->endPoint = {cx + std::cos(angle) * half, cy + std::sin(angle) * half};
  emitColorStops(grad->colorStops, stops);
  return grad;
}

RadialGradient* HTMLValueParser::parseRadialGradient(const std::string& value, float boxWidth,
                                                     float boxHeight) {
  std::vector<std::string> parts;
  if (!ExtractGradientParts(value, parts)) return nullptr;
  size_t stopStart = 0;
  // Allow leading shape descriptor like "circle at center", "ellipse 50% 50%", etc.
  std::string first = ToLower(Trim(parts[0]));
  bool hasDescriptor = first.find("circle") != std::string::npos ||
                       first.find("ellipse") != std::string::npos ||
                       first.find("at") != std::string::npos;
  if (hasDescriptor) {
    stopStart = 1;
  }
  GradientStops stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false);
  if (!finaliseGradientStops(stops)) return nullptr;

  auto grad = _document->makeNode<RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  if (hasDescriptor) {
    parseRadialDescriptor(first, boxWidth, boxHeight, grad);
  }
  emitColorStops(grad->colorStops, stops);
  return grad;
}

void HTMLValueParser::parseRadialDescriptor(const std::string& descriptor, float boxWidth,
                                            float boxHeight, RadialGradient* grad) {
  auto tokens = SplitTopLevelWhitespace(descriptor);
  std::vector<std::string> sizeTokens;
  std::vector<std::string> positionTokens;
  bool afterAt = false;
  for (auto& token : tokens) {
    if (token == "at") {
      afterAt = true;
      continue;
    }
    if (token == "circle" || token == "ellipse") continue;
    if (afterAt) {
      positionTokens.push_back(token);
    } else {
      sizeTokens.push_back(token);
    }
  }

  // Radius: the exporter writes `rx = radius * boxWidth` (and an ellipse's `ry` is implied by the
  // box height under PAGX's single-radius + fitsToGeometry model), so a length token divided by
  // boxWidth recovers the normalised radius. A bare `<pct>%` is already box-relative.
  if (!sizeTokens.empty() && boxWidth > 0) {
    float radius = resolveRadialLength(sizeTokens[0], boxWidth);
    if (!std::isnan(radius)) {
      grad->radius = radius;
    } else {
      // Extent keywords (closest-side / farthest-corner / ...) have no scalar PAGX radius; keep
      // the box-filling default and surface a diagnostic instead of silently mis-sizing.
      _diagnostics.warn("html: radial-gradient size '" + sizeTokens[0] +
                        "' not supported; using box-filling radius");
    }
  }

  // Position: `at <x> <y>`. Axis-locked keywords (left/right -> x, top/bottom -> y) are assigned
  // first so author order is irrelevant (`at top left` == `at left top`); the remaining `center`
  // and length tokens then fill the still-unset axes in x-then-y order, matching CSS.
  float cx = NAN;
  float cy = NAN;
  std::vector<std::string> freeTokens;
  for (auto& token : positionTokens) {
    if (token == "left") {
      cx = 0.0f;
    } else if (token == "right") {
      cx = 1.0f;
    } else if (token == "top") {
      cy = 0.0f;
    } else if (token == "bottom") {
      cy = 1.0f;
    } else {
      freeTokens.push_back(token);
    }
  }
  for (auto& token : freeTokens) {
    if (std::isnan(cx) && boxWidth > 0) {
      cx = (token == "center") ? 0.5f : resolveRadialLength(token, boxWidth);
    } else if (std::isnan(cy) && boxHeight > 0) {
      cy = (token == "center") ? 0.5f : resolveRadialLength(token, boxHeight);
    }
  }
  if (!std::isnan(cx)) grad->center.x = cx;
  if (!std::isnan(cy)) grad->center.y = cy;
}

float HTMLValueParser::resolveRadialLength(const std::string& token, float boxAxis) {
  if (token.empty()) return NAN;
  if (token.back() == '%') {
    char* endPtr = nullptr;
    float pct = std::strtof(token.c_str(), &endPtr);
    if (endPtr != nullptr && endPtr != token.c_str() &&
        static_cast<size_t>(endPtr - token.c_str()) == token.size() - 1) {
      return pct / 100.0f;
    }
    return NAN;
  }
  float px = parseAbsoluteLengthPx(token);
  if (std::isnan(px)) return NAN;
  return px / boxAxis;
}

ConicGradient* HTMLValueParser::parseConicGradient(const std::string& value) {
  std::vector<std::string> parts;
  if (!ExtractGradientParts(value, parts)) return nullptr;
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

HTMLValueParser::GradientStops HTMLValueParser::parseGradientStops(
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
        // Validate the percent token end-to-end so '50%' parses while 'abc%' is rejected.
        // strtof silently returns 0 on a leading non-digit, which would otherwise be treated
        // as a valid 0% offset and shift the entire gradient stop layout.
        char* endPtr = nullptr;
        float pct = std::strtof(off.c_str(), &endPtr);
        if (endPtr != nullptr && endPtr != off.c_str() &&
            static_cast<size_t>(endPtr - off.c_str()) == off.size() - 1) {
          offset = pct / 100.0f;
        } else {
          _diagnostics.warn("html: malformed gradient stop offset '" + off +
                            "'; inferring position");
        }
      } else if (interpretAngularOffset && !off.empty() && off.find("deg") != std::string::npos) {
        offset = ParseAngle(off) / 360.0f;
      } else if (!interpretAngularOffset) {
        float v = parseAbsoluteLengthPx(off);
        if (!std::isnan(v)) offset = v;
      }
    }
    stops.emplace_back(offset, color);
  }
  return stops;
}

bool HTMLValueParser::finaliseGradientStops(GradientStops& stops) {
  if (stops.empty()) return false;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  // Walk left-to-right and resolve any NaN offset by interpolating from the previous known
  // anchor to the next known anchor. Because `stops[i]` is updated in place, the next
  // iteration's `prev` is the value we just wrote — that's intentional: filling a run of
  // NaNs sequentially produces the same evenly spaced result as solving the whole run
  // simultaneously, since each step's slope is recomputed against the remaining width.
  for (size_t i = 1; i + 1 < stops.size(); ++i) {
    if (!std::isnan(stops[i].first)) continue;
    size_t next = i + 1;
    while (next < stops.size() && std::isnan(stops[next].first)) ++next;
    float prevOffset = stops[i - 1].first;
    float nextOffset = next < stops.size() ? stops[next].first : 1.0f;
    float steps = static_cast<float>(next - (i - 1));
    stops[i].first = prevOffset + (nextOffset - prevOffset) / steps;
  }
  return true;
}

template <typename T>
void HTMLValueParser::emitColorStops(T& targetStops, const GradientStops& stops) {
  for (const auto& [offset, color] : stops) {
    auto stop = _document->makeNode<ColorStop>();
    stop->offset = offset;
    stop->color = color;
    targetStops.push_back(stop);
  }
}

// Explicit instantiations for the gradient containers that emit ColorStops.
template void HTMLValueParser::emitColorStops(std::vector<ColorStop*>&, const GradientStops&);

}  // namespace pagx
