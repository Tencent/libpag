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

using ColorMatrix = std::array<float, 20>;

// The 4x5 identity colour matrix (row-major R, G, B, A rows; last column bias). Each `*Matrix`
// builder below starts from a copy and overwrites only the coefficients its function touches.
constexpr ColorMatrix kIdentityColorMatrix = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                                              0, 0, 1, 0, 0, 0, 0, 0, 1, 0};

// Parses a CSS filter amount: a unitless number (`1.15`) or a percentage (`115%`). Returns
// `defaultValue` for an empty argument (e.g. `grayscale()` == `grayscale(1)`) and NaN for a value
// that is not a bare number/percentage, so the caller can fall back to an unsupported diagnostic.
float ParseFilterAmount(const std::string& raw, float defaultValue) {
  std::string s = Trim(raw);
  if (s.empty()) return defaultValue;
  float fraction = 0.0f;
  if (ParseCssPercentage(s, fraction)) return fraction;
  char* end = nullptr;
  float v = std::strtof(s.c_str(), &end);
  if (end == s.c_str()) return NAN;
  // Reject anything with a trailing unit (`1px`, `2em`) so it is not silently read as a number.
  if (!Trim(end).empty()) return NAN;
  return v;
}

// brightness(b): scale every colour channel by `b`.
ColorMatrix BrightnessMatrix(float b) {
  ColorMatrix m = kIdentityColorMatrix;
  m[0] = b;
  m[6] = b;
  m[12] = b;
  return m;
}

// contrast(c): out = c * in + 0.5 * (1 - c) per channel.
ColorMatrix ContrastMatrix(float c) {
  float bias = 0.5f * (1.0f - c);
  ColorMatrix m = kIdentityColorMatrix;
  m[0] = c;
  m[4] = bias;
  m[6] = c;
  m[9] = bias;
  m[12] = c;
  m[14] = bias;
  return m;
}

// invert(a): out = (1 - 2a) * in + a per channel.
ColorMatrix InvertMatrix(float a) {
  float slope = 1.0f - 2.0f * a;
  ColorMatrix m = kIdentityColorMatrix;
  m[0] = slope;
  m[4] = a;
  m[6] = slope;
  m[9] = a;
  m[12] = slope;
  m[14] = a;
  return m;
}

// opacity(o): scale the alpha channel by `o`.
ColorMatrix OpacityMatrix(float o) {
  ColorMatrix m = kIdentityColorMatrix;
  m[18] = o;
  return m;
}

// saturate(s): the CSS Filter Effects saturation matrix (Rec. 601 luma weights).
ColorMatrix SaturateMatrix(float s) {
  return {0.213f + 0.787f * s, 0.715f - 0.715f * s, 0.072f - 0.072f * s, 0, 0,
          0.213f - 0.213f * s, 0.715f + 0.285f * s, 0.072f - 0.072f * s, 0, 0,
          0.213f - 0.213f * s, 0.715f - 0.715f * s, 0.072f + 0.928f * s, 0, 0,
          0,                   0,                   0,                   1, 0};
}

// hue-rotate(deg): the CSS Filter Effects hue-rotation matrix.
ColorMatrix HueRotateMatrix(float degrees) {
  float radians = degrees * HtmlPi / 180.0f;
  float c = std::cos(radians);
  float s = std::sin(radians);
  return {0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f,
          0.072f - c * 0.072f + s * 0.928f, 0, 0,
          0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f,
          0.072f - c * 0.072f - s * 0.283f, 0, 0,
          0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f,
          0.072f + c * 0.928f + s * 0.072f, 0, 0,
          0,                                0,                                0, 1, 0};
}

// grayscale(a): interpolate towards luma-only output (Rec. 709 weights); `p = 1 - a`.
ColorMatrix GrayscaleMatrix(float a) {
  float p = 1.0f - a;
  return {0.2126f + 0.7874f * p, 0.7152f - 0.7152f * p, 0.0722f - 0.0722f * p, 0, 0,
          0.2126f - 0.2126f * p, 0.7152f + 0.2848f * p, 0.0722f - 0.0722f * p, 0, 0,
          0.2126f - 0.2126f * p, 0.7152f - 0.7152f * p, 0.0722f + 0.9278f * p, 0, 0,
          0,                     0,                     0,                     1, 0};
}

// sepia(a): interpolate towards the CSS Filter Effects sepia tint; `p = 1 - a`.
ColorMatrix SepiaMatrix(float a) {
  float p = 1.0f - a;
  return {0.393f + 0.607f * p, 0.769f - 0.769f * p, 0.189f - 0.189f * p, 0, 0,
          0.349f - 0.349f * p, 0.686f + 0.314f * p, 0.168f - 0.168f * p, 0, 0,
          0.272f - 0.272f * p, 0.534f - 0.534f * p, 0.131f + 0.869f * p, 0, 0,
          0,                   0,                   0,                   1, 0};
}

// Lowers a CSS colour filter function (`name(args)`) to its 4x5 colour matrix. Returns false when
// `name` is not a colour filter function or its argument fails to parse, leaving the caller to
// treat the step as unsupported.
bool ColorFilterMatrix(const std::string& name, const std::string& args, ColorMatrix& out) {
  auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
  if (name == "hue-rotate") {
    out = HueRotateMatrix(ParseAngle(args));
    return true;
  }
  float amount = ParseFilterAmount(args, 1.0f);
  if (std::isnan(amount)) return false;
  if (name == "brightness") {
    out = BrightnessMatrix(std::max(0.0f, amount));
  } else if (name == "contrast") {
    out = ContrastMatrix(std::max(0.0f, amount));
  } else if (name == "saturate") {
    out = SaturateMatrix(std::max(0.0f, amount));
  } else if (name == "grayscale") {
    out = GrayscaleMatrix(clamp(amount, 0.0f, 1.0f));
  } else if (name == "sepia") {
    out = SepiaMatrix(clamp(amount, 0.0f, 1.0f));
  } else if (name == "invert") {
    out = InvertMatrix(clamp(amount, 0.0f, 1.0f));
  } else if (name == "opacity") {
    out = OpacityMatrix(clamp(amount, 0.0f, 1.0f));
  } else {
    return false;
  }
  return true;
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
  // path and renders opaque black. Chrome's getComputedStyle also normalises colors mixed with
  // currentColor or color-mix into `color(srgb ...)` even when the source was plain rgba(), so
  // the srgb form is recovered too (via ParseCSSColorFunction). Any other color space cannot be
  // mapped to PAGX's sRGB pipeline, so it degrades to opaque black with a targeted diagnostic.
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
    Color colorFn = {};
    if (ParseCSSColorFunction(value, colorFn)) {
      return colorFn;
    }
    // Reach here only when the value is well-formed CSS but uses a color space we cannot map
    // to PAGX's sRGB pipeline. Emitting a dedicated diagnostic prevents users from chasing
    // the generic "unrecognised color value" message which would suggest a typo instead.
    _diagnostics.warn("html: unsupported color() with non-sRGB color space '" + value +
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
    } else if (name == "url") {
      // `filter: url(#id)` references an SVG `<filter>` def. Strip optional quotes and the
      // leading '#'; the caller resolves the id through the shared-defs table.
      std::string ref = Trim(args);
      if (ref.size() >= 2 && (ref.front() == '"' || ref.front() == '\'') &&
          ref.back() == ref.front()) {
        ref = Trim(ref.substr(1, ref.size() - 2));
      }
      if (!ref.empty() && ref.front() == '#') {
        ref = ref.substr(1);
      }
      step.kind = ref.empty() ? FilterStep::Kind::Unsupported : FilterStep::Kind::SvgRef;
      step.refId = ref;
    } else if (ColorMatrix cm; ColorFilterMatrix(name, args, cm)) {
      // CSS colour filter functions (hue-rotate / saturate / brightness / contrast / grayscale /
      // sepia / invert / opacity) all lower to a 4x5 colour matrix driving a ColorMatrixFilter.
      step.kind = FilterStep::Kind::ColorMatrix;
      step.matrix = cm;
    } else {
      step.kind = FilterStep::Kind::Unsupported;
    }
    out.push_back(step);
    pos = end + 1;
  }
  return out;
}

LinearGradient* HTMLValueParser::parseLinearGradient(const std::string& value, float boxWidth,
                                                     float boxHeight, bool repeating) {
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
  float angle = CssToPagxAngle(cssAngle) * HtmlPi / 180.0f;
  float dirX = std::cos(angle);
  float dirY = std::sin(angle);
  bool boxKnown =
      !(std::isnan(boxWidth) || std::isnan(boxHeight) || boxWidth <= 0.0f || boxHeight <= 0.0f);
  // The CSS gradient-line length is the "magic corners" extent L = |W*sinφ| + |H*cosφ| (φ is the
  // PAGX angle, 0deg = +X), which is also the px extent a px stop offset is measured against.
  float lineLength = boxKnown ? std::abs(boxWidth * dirX) + std::abs(boxHeight * dirY) : NAN;
  GradientStops stops =
      parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false, lineLength);
  if (!finaliseGradientStops(stops)) return nullptr;
  if (repeating) appendRepeatingReset(stops);

  auto grad = _document->makeNode<LinearGradient>();
  if (!boxKnown) {
    // Box size unknown (e.g. a text fill, or an unsized box): fall back to the geometry-normalised
    // (0,0)-(1,1) space. The gradient line spans a fixed unit half-length through the center, which
    // is only exact for axis-aligned gradients but keeps the default fitsToGeometry behaviour.
    grad->startPoint = {0.5f - dirX * 0.5f, 0.5f - dirY * 0.5f};
    grad->endPoint = {0.5f + dirX * 0.5f, 0.5f + dirY * 0.5f};
    emitColorStops(grad->colorStops, stops);
    return grad;
  }

  // With a concrete box, resolve the gradient line in absolute pixel space and disable per-geometry
  // fitting (matching the SVG importer). PAGX's fitsToGeometry=true non-uniformly scales the
  // normalised (0,0)-(1,1) line by the box size, which tilts the equal-color lines off perpendicular
  // on a non-square box; pixel-space endpoints keep them perpendicular for any aspect ratio. The
  // line is centred on the box so the 0% / 100% stops land exactly on the covering corners.
  float halfX = dirX * lineLength * 0.5f;
  float halfY = dirY * lineLength * 0.5f;
  float cx = boxWidth * 0.5f;
  float cy = boxHeight * 0.5f;
  grad->startPoint = {cx - halfX, cy - halfY};
  grad->endPoint = {cx + halfX, cy + halfY};
  grad->fitsToGeometry = false;
  emitColorStops(grad->colorStops, stops);
  return grad;
}

RadialGradient* HTMLValueParser::parseRadialGradient(const std::string& value, float boxWidth,
                                                     float boxHeight, bool repeating) {
  std::vector<std::string> parts;
  if (!ExtractGradientParts(value, parts)) return nullptr;
  size_t stopStart = 0;
  // CSS Color 4 lets the first comma-separated segment declare the gradient shape (`circle`,
  // `ellipse`), an explicit size (`closest-side`, `farthest-side`, `closest-corner`,
  // `farthest-corner`, or two <length-percentage>s), and the center via `at <position>`. We
  // ignore the geometry for now — the gradient is mapped onto the box like a normalized 50%
  // disc — but we must recognise the header so its tokens are not parsed as the first color
  // stop. The earlier check only saw the literal words "circle"/"ellipse"/"at", which let the
  // common `radial-gradient(closest-side, …)` form fall through and produced a bogus
  // "unrecognised color value 'closest-side'" diagnostic followed by an opaque-black stop.
  static constexpr const char* kShapeKeywords[] = {
      "circle",       "ellipse",        " at ",          "at ",
      "closest-side", "closest-corner", "farthest-side", "farthest-corner",
  };
  std::string first = ToLower(Trim(parts[0]));
  bool hasDescriptor = false;
  for (const char* kw : kShapeKeywords) {
    if (first.find(kw) != std::string::npos) {
      hasDescriptor = true;
      break;
    }
  }
  // Two-number form e.g. "60% 40%" / "120px 90px" (followed optionally by "at ..."). A pure
  // numeric/length token would never start a valid color stop in CSS, so treat any leading
  // segment that begins with a digit, sign or dot as the shape header. Authors who wrote a
  // single bare number per color stop hit `parseGradientStops`'s offset handling, which
  // expects the offset to follow the color, not precede it — so the leading-digit form is
  // unambiguous here.
  if (!hasDescriptor && !first.empty()) {
    char c = first[0];
    if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) hasDescriptor = true;
  }
  if (hasDescriptor) {
    stopStart = 1;
  }

  // Resolve the descriptor (center / radius / coordinate space) before parsing stops so px stop
  // offsets can be normalised against the gradient's radius in px. The radius in px is `radius *
  // boxWidth` in the default fitsToGeometry model (the normalised radius stretched by the box) and
  // already-px when the circle switched to the pixel-space model on a non-square box.
  RadialDescriptor desc;
  if (hasDescriptor) {
    parseRadialDescriptor(first, boxWidth, boxHeight, desc);
  }
  float radiusPx = NAN;
  if (std::isfinite(boxWidth) && boxWidth > 0.0f) {
    radiusPx = desc.fitsToGeometry ? desc.radius * boxWidth : desc.radius;
  }
  GradientStops stops =
      parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false, radiusPx);
  if (!finaliseGradientStops(stops)) return nullptr;
  if (repeating) appendRepeatingReset(stops);

  auto grad = _document->makeNode<RadialGradient>();
  grad->center = {desc.centerX, desc.centerY};
  grad->radius = desc.radius;
  grad->fitsToGeometry = desc.fitsToGeometry;
  emitColorStops(grad->colorStops, stops);
  return grad;
}

void HTMLValueParser::parseRadialDescriptor(const std::string& descriptor, float boxWidth,
                                            float boxHeight, RadialDescriptor& out) {
  auto tokens = SplitTopLevelWhitespace(descriptor);
  std::vector<std::string> sizeTokens;
  std::vector<std::string> positionTokens;
  bool afterAt = false;
  bool explicitCircle = false;
  bool explicitEllipse = false;
  for (auto& token : tokens) {
    if (token == "at") {
      afterAt = true;
      continue;
    }
    if (token == "circle") {
      explicitCircle = true;
      continue;
    }
    if (token == "ellipse") {
      explicitEllipse = true;
      continue;
    }
    if (afterAt) {
      positionTokens.push_back(token);
    } else {
      sizeTokens.push_back(token);
    }
  }

  // Radius: the exporter writes `rx = radius * boxWidth` (and an ellipse's `ry` is implied by the
  // box height under PAGX's single-radius + fitsToGeometry model), so a length token divided by
  // boxWidth recovers the normalised radius. A bare `<pct>%` is already box-relative. Track whether
  // the radius came from an explicit px length so a circle on a non-square box can later switch to
  // the fitsToGeometry=false pixel model (see below).
  bool radiusFromPxLength = false;
  if (!sizeTokens.empty() && boxWidth > 0) {
    float radius = resolveRadialLength(sizeTokens[0], boxWidth);
    if (!std::isnan(radius)) {
      out.radius = radius;
      radiusFromPxLength = !sizeTokens[0].empty() && sizeTokens[0].back() != '%';
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
  if (!std::isnan(cx)) out.centerX = cx;
  if (!std::isnan(cy)) out.centerY = cy;

  // A CSS `circle <r>px` keeps a single uniform radius regardless of box aspect ratio. PAGX's
  // default fitsToGeometry=true model stretches the normalised radius by box width and height
  // independently, so on a non-square box it would render the circle as an ellipse. Switch such a
  // circle to the fitsToGeometry=false pixel model (center/radius in the geometry's local px
  // space, where the box spans (0,0)-(boxWidth,boxHeight)) so the radius stays isotropic. Square
  // boxes, ellipses, and percentage/extent sizes keep the compact normalised representation.
  bool isCircle = explicitCircle || (!explicitEllipse && sizeTokens.size() == 1);
  if (isCircle && radiusFromPxLength && boxWidth > 0 && boxHeight > 0 &&
      std::abs(boxWidth - boxHeight) > 0.01f) {
    out.centerX = out.centerX * boxWidth;
    out.centerY = out.centerY * boxHeight;
    out.radius = out.radius * boxWidth;
    out.fitsToGeometry = false;
  }
}

float HTMLValueParser::resolveRadialLength(const std::string& token, float boxAxis) {
  if (token.empty()) return NAN;
  if (token.back() == '%') {
    float fraction = NAN;
    if (ParseCssPercentage(token, fraction)) {
      return fraction;
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
    const std::vector<std::string>& parts, size_t startIndex, bool interpretAngularOffset,
    float pxOffsetScale) {
  const bool normalisePx = std::isfinite(pxOffsetScale) && pxOffsetScale > 0.0f;
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
        float fraction = NAN;
        if (ParseCssPercentage(off, fraction)) {
          offset = fraction;
        } else {
          _diagnostics.warn("html: malformed gradient stop offset '" + off +
                            "'; inferring position");
        }
      } else if (interpretAngularOffset && !off.empty() && off.find("deg") != std::string::npos) {
        offset = ParseAngle(off) / 360.0f;
      } else if (!interpretAngularOffset) {
        float v = parseAbsoluteLengthPx(off);
        if (!std::isnan(v)) {
          // A px stop offset is an absolute distance along the gradient ray. PAGX color-stop
          // offsets are normalised to [0,1] where 1.0 is the gradient's extent, so divide by that
          // extent (line length / radius) when it is known. e.g. `radial-gradient(#f80 1.4px,
          // transparent 1.6px)` on a large box would otherwise store 1.4 / 1.6 — both past the 1.0
          // edge — and paint the whole box with the first color instead of a tiny dot.
          offset = normalisePx ? v / pxOffsetScale : v;
        }
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

void HTMLValueParser::appendRepeatingReset(GradientStops& stops) {
  if (stops.size() < 2) return;
  float lastOffset = stops.back().first;
  // Only meaningful when the declared pattern ends before the gradient's 1.0 extent — that leftover
  // [lastOffset, 1] band is what a real repeating gradient would tile. If the last stop already
  // sits at (or past) the edge there is nothing to reset.
  if (!(lastOffset < 1.0f)) return;
  // Copy by value: the emplace_back calls below can reallocate `stops` and invalidate a reference
  // into it.
  Color firstColor = stops.front().second;
  // A hair past the last stop so the tile boundary stays sharp instead of ramping the last color
  // across the whole leftover band, then hold the first color out to the edge. Together these make
  // the downgrade render one period at its true scale with the base color filling the remainder.
  float resetOffset = std::min(1.0f, lastOffset + 1.0e-4f);
  stops.emplace_back(resetOffset, firstColor);
  if (resetOffset < 1.0f) stops.emplace_back(1.0f, firstColor);
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
