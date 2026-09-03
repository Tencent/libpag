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
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/types/Data.h"
#include "pagx/utils/StringParser.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageInfo.h"

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

// Parses a CSS filter amount, which is a number or a percentage (e.g. `1.4` or `140%`). An empty
// argument selects the function's default. Returns false when the token is present but malformed.
bool ParseFilterAmount(const std::string& raw, float defaultValue, float& out) {
  std::string token = Trim(raw);
  if (token.empty()) {
    out = defaultValue;
    return true;
  }
  float fraction = 0.0f;
  if (ParseCssPercentage(token, fraction)) {
    out = fraction;
    return true;
  }
  char* end = nullptr;
  float v = std::strtof(token.c_str(), &end);
  if (end == token.c_str() || static_cast<size_t>(end - token.c_str()) != token.size()) {
    return false;
  }
  out = v;
  return true;
}

// The 20-element identity matrix (4x5, row-major, normalised colour space) used as the starting
// point for the colour-adjustment filter functions.
std::array<float, 20> IdentityColorMatrix() {
  return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
          0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
}

// CSS `brightness(a)` / `contrast(a)`: per-channel linear ramp `out = a * in + bias`. brightness
// uses bias 0; contrast uses bias `0.5 - 0.5a` so the transform pivots around mid-grey.
std::array<float, 20> BrightnessContrastMatrix(float amount, float bias) {
  auto m = IdentityColorMatrix();
  m[0] = amount;
  m[6] = amount;
  m[12] = amount;
  m[4] = bias;
  m[9] = bias;
  m[14] = bias;
  return m;
}

// CSS `saturate(s)` matrix (Rec.709 luma coefficients), per the Filter Effects spec. `s == 1` is
// the identity; `s == 0` collapses to greyscale.
std::array<float, 20> SaturateMatrix(float s) {
  auto m = IdentityColorMatrix();
  m[0] = 0.213f + 0.787f * s;
  m[1] = 0.715f - 0.715f * s;
  m[2] = 0.072f - 0.072f * s;
  m[5] = 0.213f - 0.213f * s;
  m[6] = 0.715f + 0.285f * s;
  m[7] = 0.072f - 0.072f * s;
  m[10] = 0.213f - 0.213f * s;
  m[11] = 0.715f - 0.715f * s;
  m[12] = 0.072f + 0.928f * s;
  return m;
}

// CSS `grayscale(a)` matrix, interpolating from identity (a == 0) to full luma greyscale (a == 1).
// The caller clamps `a` to [0, 1].
std::array<float, 20> GrayscaleMatrix(float a) {
  return SaturateMatrix(1.0f - a);
}

// CSS `sepia(a)` matrix, per the Filter Effects spec, interpolating from identity to full sepia.
// The caller clamps `a` to [0, 1].
std::array<float, 20> SepiaMatrix(float a) {
  float inv = 1.0f - a;
  auto m = IdentityColorMatrix();
  m[0] = 0.393f + 0.607f * inv;
  m[1] = 0.769f - 0.769f * inv;
  m[2] = 0.189f - 0.189f * inv;
  m[5] = 0.349f - 0.349f * inv;
  m[6] = 0.686f + 0.314f * inv;
  m[7] = 0.168f - 0.168f * inv;
  m[10] = 0.272f - 0.272f * inv;
  m[11] = 0.534f - 0.534f * inv;
  m[12] = 0.131f + 0.869f * inv;
  return m;
}

// CSS `invert(a)`: per-channel `out = a + (1 - 2a) * in`, so `a == 1` fully inverts each channel.
std::array<float, 20> InvertMatrix(float a) {
  auto m = IdentityColorMatrix();
  float diag = 1.0f - 2.0f * a;
  m[0] = diag;
  m[6] = diag;
  m[12] = diag;
  m[4] = a;
  m[9] = a;
  m[14] = a;
  return m;
}

// CSS `opacity(a)`: scales only the alpha channel.
std::array<float, 20> OpacityMatrix(float a) {
  auto m = IdentityColorMatrix();
  m[18] = a;
  return m;
}

// CSS `hue-rotate(angle)` matrix, per the Filter Effects spec (Rec.709 luma with the standard
// rotation coefficients). `angleDeg` rotates the hue around the colour wheel.
std::array<float, 20> HueRotateMatrix(float angleDeg) {
  float rad = angleDeg * 3.14159265358979323846f / 180.0f;
  float c = std::cos(rad);
  float s = std::sin(rad);
  auto m = IdentityColorMatrix();
  m[0] = 0.213f + c * 0.787f - s * 0.213f;
  m[1] = 0.715f - c * 0.715f - s * 0.715f;
  m[2] = 0.072f - c * 0.072f + s * 0.928f;
  m[5] = 0.213f - c * 0.213f + s * 0.143f;
  m[6] = 0.715f + c * 0.285f + s * 0.140f;
  m[7] = 0.072f - c * 0.072f - s * 0.283f;
  m[10] = 0.213f - c * 0.213f - s * 0.787f;
  m[11] = 0.715f - c * 0.715f + s * 0.715f;
  m[12] = 0.072f + c * 0.928f + s * 0.072f;
  return m;
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
  if (lowered.compare(0, 3, "rgb") == 0) {
    auto open = value.find('(');
    auto close = value.find(')');
    if (open != std::string::npos && close != std::string::npos) {
      std::string inner = value.substr(open + 1, close - open - 1);
      auto comps = ParseFloatList(inner);
      if (comps.size() < 3) {
        _diagnostics.warn("html: malformed rgb() value '" + value +
                          "'; falling back to opaque black");
        return {0, 0, 0, 1, ColorSpace::SRGB};
      }
      Color color = {};
      color.colorSpace = ColorSpace::SRGB;
      float r = comps[0];
      float g = comps[1];
      float b = comps[2];
      float a = comps.size() >= 4 ? comps[3] : 1.0f;
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
  _diagnostics.warn("html: length unit '" + suffix + "' not supported; value ignored");
  return NAN;
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
    } else if (name == "brightness" || name == "contrast" || name == "saturate" ||
               name == "grayscale" || name == "sepia" || name == "invert" || name == "opacity") {
      // Colour-adjustment functions taking a <number>|<percentage> amount. brightness / contrast /
      // saturate / opacity default to 1 (identity); grayscale / sepia / invert default to 0.
      float defaultValue =
          (name == "brightness" || name == "contrast" || name == "saturate" || name == "opacity")
              ? 1.0f
              : 0.0f;
      float amount = defaultValue;
      if (!ParseFilterAmount(args, defaultValue, amount)) {
        _diagnostics.warn("html: invalid amount in filter '" + step.raw + "'; ignored");
        step.kind = FilterStep::Kind::Unsupported;
      } else {
        // Match the clamping browsers apply: grayscale / sepia / invert / opacity saturate at 1,
        // while brightness / contrast / saturate only forbid negatives (no upper bound).
        if (name == "grayscale" || name == "sepia" || name == "invert" || name == "opacity") {
          amount = std::clamp(amount, 0.0f, 1.0f);
        } else {
          amount = std::max(amount, 0.0f);
        }
        step.kind = FilterStep::Kind::ColorMatrix;
        if (name == "brightness") {
          step.matrix = BrightnessContrastMatrix(amount, 0.0f);
        } else if (name == "contrast") {
          step.matrix = BrightnessContrastMatrix(amount, 0.5f - 0.5f * amount);
        } else if (name == "saturate") {
          step.matrix = SaturateMatrix(amount);
        } else if (name == "grayscale") {
          step.matrix = GrayscaleMatrix(amount);
        } else if (name == "sepia") {
          step.matrix = SepiaMatrix(amount);
        } else if (name == "invert") {
          step.matrix = InvertMatrix(amount);
        } else {  // opacity
          step.matrix = OpacityMatrix(amount);
        }
      }
    } else if (name == "hue-rotate") {
      step.kind = FilterStep::Kind::ColorMatrix;
      step.matrix = HueRotateMatrix(ParseAngle(args));
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
  // The CSS gradient-line length is the "magic corners" extent L = |W*cosφ| + |H*sinφ| (φ is the
  // PAGX angle, 0deg = +X), centred on the box, so the 0% / 100% stops land exactly on the
  // covering corners. Only meaningful when the box size is known. It is also the px extent a px
  // stop offset is measured against.
  float lineLength = boxKnown ? std::abs(boxWidth * dirX) + std::abs(boxHeight * dirY) : NAN;

  // `repeating-linear-gradient` has no native PAGX equivalent (gradients carry no spread mode), so
  // tile the authored period into explicit stops across the whole gradient line. This needs the
  // pixel line length; without a concrete box we fall through to a single non-repeating period.
  GradientStops stops;
  bool tiled = false;
  if (repeating && boxKnown && lineLength > 0.0f) {
    tiled = buildRepeatingLinearStops(parts, stopStart, lineLength, stops);
  }
  if (!tiled) {
    stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false, lineLength);
    if (!finaliseGradientStops(stops)) return nullptr;
    if (repeating) {
      _diagnostics.warn(
          "html: repeating-linear-gradient approximated as a single non-repeating period");
    }
  }

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
  // on a non-square box; pixel-space endpoints keep them perpendicular for any aspect ratio.
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

namespace {

uint8_t ColorChannelToByte(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 1.0f) return 255;
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// Samples one repeating period at `linePos` (px along the gradient line). `stops` hold the
// period's (position-px, color) pairs in non-decreasing order; positions outside [firstPos,
// firstPos+periodPx) wrap. Coincident positions (hard stops) resolve to the later stop's color so
// crisp edges are preserved; a genuine two-color band interpolates linearly.
Color SampleRepeatingPeriod(const HTMLValueParser::GradientStops& stops, float firstPos,
                            float periodPx, float linePos) {
  float local = std::fmod(linePos - firstPos, periodPx);
  if (local < 0.0f) local += periodPx;
  float pos = firstPos + local;
  for (size_t i = 1; i < stops.size(); ++i) {
    float p0 = stops[i - 1].first;
    float p1 = stops[i].first;
    bool last = (i + 1 == stops.size());
    if (pos < p1 || last) {
      if (p1 - p0 <= 1e-6f) {
        return stops[i].second;
      }
      if (pos <= p0) {
        return stops[i - 1].second;
      }
      float t = (pos - p0) / (p1 - p0);
      t = std::max(0.0f, std::min(1.0f, t));
      const Color& a = stops[i - 1].second;
      const Color& b = stops[i].second;
      return {a.red + (b.red - a.red) * t, a.green + (b.green - a.green) * t,
              a.blue + (b.blue - a.blue) * t, a.alpha + (b.alpha - a.alpha) * t, a.colorSpace};
    }
  }
  return stops.back().second;
}

// CSS radial extent keywords control how far the ending shape reaches; they carry no scalar radius
// in the token itself (the radius is derived from the center and box). Returns true for any of the
// four keywords so the caller can compute the corresponding px radius from the center position.
bool IsRadialExtentKeyword(const std::string& token) {
  return token == "closest-side" || token == "closest-corner" || token == "farthest-side" ||
         token == "farthest-corner";
}

// Computes the px radius of a CSS `circle` ending shape for the given extent keyword, measured from
// a center at (cxPx, cyPx) within a (0,0)-(boxWidth,boxHeight) box. An empty/unknown keyword
// defaults to `farthest-corner`, matching CSS when the size is omitted. `closest-corner` /
// `farthest-corner` are the Euclidean distances to the nearest / farthest box corner;
// `closest-side` / `farthest-side` are the min / max of the perpendicular distances to the four
// edges.
float CircleExtentRadiusPx(const std::string& keyword, float cxPx, float cyPx, float boxWidth,
                           float boxHeight) {
  float left = std::abs(cxPx);
  float right = std::abs(boxWidth - cxPx);
  float top = std::abs(cyPx);
  float bottom = std::abs(boxHeight - cyPx);
  float dx = std::max(left, right);
  float dy = std::max(top, bottom);
  if (keyword == "closest-side") {
    return std::min(std::min(left, right), std::min(top, bottom));
  }
  if (keyword == "farthest-side") {
    return std::max(dx, dy);
  }
  if (keyword == "closest-corner") {
    float nx = std::min(left, right);
    float ny = std::min(top, bottom);
    return std::sqrt(nx * nx + ny * ny);
  }
  // farthest-corner (also the default when the size is omitted).
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

ColorSource* HTMLValueParser::parseRepeatingLinearGradientPattern(const std::string& value,
                                                                  float boxWidth, float boxHeight) {
  if (_document == nullptr) return nullptr;
  if (std::isnan(boxWidth) || std::isnan(boxHeight) || boxWidth <= 0.0f || boxHeight <= 0.0f) {
    return nullptr;
  }
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

  // Only axis-aligned patterns tile seamlessly with a 1D strip; oblique angles need a 2D tile /
  // rotation, so let the caller fall back to the gradient-stop tiling for those.
  constexpr float kAxisEps = 1e-3f;
  bool vertical = std::abs(dirX) < kAxisEps;
  bool horizontal = std::abs(dirY) < kAxisEps;
  if (!vertical && !horizontal) return nullptr;

  float lineLength = std::abs(boxWidth * dirX) + std::abs(boxHeight * dirY);
  if (!(lineLength > 0.0f)) return nullptr;

  // Parse one authored period into (position-px, color) pairs, filling and clamping positions the
  // same way as the gradient-stop path.
  GradientStops period;
  for (size_t i = stopStart; i < parts.size(); ++i) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    size_t positions = 0;
    for (size_t t = 1; t < tokens.size() && t <= 2; ++t) {
      const std::string& tk = tokens[t];
      float pos = NAN;
      if (!tk.empty() && tk.back() == '%') {
        float fraction = NAN;
        if (ParseCssPercentage(tk, fraction)) pos = fraction * lineLength;
      } else {
        pos = parseAbsoluteLengthPx(tk);
      }
      period.emplace_back(pos, color);
      ++positions;
    }
    if (positions == 0) period.emplace_back(NAN, color);
  }
  if (period.size() < 2) return nullptr;
  if (std::isnan(period.front().first)) period.front().first = 0.0f;
  if (std::isnan(period.back().first)) period.back().first = lineLength;
  for (size_t i = 1; i + 1 < period.size(); ++i) {
    if (!std::isnan(period[i].first)) continue;
    size_t next = i + 1;
    while (next < period.size() && std::isnan(period[next].first)) ++next;
    float prev = period[i - 1].first;
    float nxt = next < period.size() ? period[next].first : lineLength;
    float steps = static_cast<float>(next - (i - 1));
    period[i].first = prev + (nxt - prev) / steps;
  }
  for (size_t i = 1; i < period.size(); ++i) {
    if (period[i].first < period[i - 1].first) period[i].first = period[i - 1].first;
  }
  float firstPos = period.front().first;
  float periodPx = period.back().first - firstPos;
  if (!(periodPx > 0.0f)) return nullptr;

  // One-period tile at native resolution. Reject a period coarser than the box (no visible repeat;
  // the gradient path renders it faithfully) or absurdly large to bound the embedded image.
  int tileN = static_cast<int>(std::lround(periodPx));
  if (tileN < 1) tileN = 1;
  constexpr int kMaxTile = 4096;
  if (tileN > kMaxTile || static_cast<float>(tileN) > lineLength) return nullptr;

  // Covering-corner start of the gradient line, so the tile's baked phase matches CSS (positions
  // are measured from the line's 0 end).
  float startAxis = vertical ? boxHeight * 0.5f - dirY * lineLength * 0.5f
                             : boxWidth * 0.5f - dirX * lineLength * 0.5f;
  float dir = vertical ? dirY : dirX;

  // The tile has `tileN` integer pixels but the period may be fractional; scale it so the tile maps
  // to exactly one `periodPx`-tall (or wide) span in the layer, keeping the repeat seamless and the
  // spacing exact. For an integer period this is exactly 1.0 (no resampling).
  float tileScale = periodPx / static_cast<float>(tileN);

  int tileW = vertical ? 1 : tileN;
  int tileH = vertical ? tileN : 1;
  std::vector<uint8_t> rgba(static_cast<size_t>(tileW) * static_cast<size_t>(tileH) * 4, 0);
  for (int idx = 0; idx < tileN; ++idx) {
    float devicePos = static_cast<float>(idx) * tileScale;
    float linePos = (devicePos - startAxis) * dir;
    Color c = SampleRepeatingPeriod(period, firstPos, periodPx, linePos);
    uint8_t* px = rgba.data() + static_cast<size_t>(idx) * 4;
    px[0] = ColorChannelToByte(c.red);
    px[1] = ColorChannelToByte(c.green);
    px[2] = ColorChannelToByte(c.blue);
    px[3] = ColorChannelToByte(c.alpha);
  }

  tgfx::Bitmap bitmap(tileW, tileH, /*alphaOnly=*/false, /*tryHardware=*/false);
  if (bitmap.isEmpty()) return nullptr;
  auto srcInfo = tgfx::ImageInfo::Make(tileW, tileH, tgfx::ColorType::RGBA_8888,
                                       tgfx::AlphaType::Unpremultiplied);
  if (!bitmap.writePixels(srcInfo, rgba.data())) return nullptr;
  auto png = bitmap.encode(tgfx::EncodedFormat::PNG, 100);
  if (png == nullptr || png->empty()) return nullptr;

  auto* image = _document->makeNode<Image>();
  image->data = Data::MakeWithCopy(png->data(), png->size());

  auto* pattern = _document->makeNode<ImagePattern>();
  pattern->image = image;
  pattern->tileModeX = TileMode::Repeat;
  pattern->tileModeY = TileMode::Repeat;
  // The tile sits in the layer's pixel space (scaleMode None) with the phase baked into the pixels;
  // the matrix only stretches the tile along its axis to the exact fractional period (identity for
  // an integer period). Nearest sampling keeps the hard line edges crisp.
  if (vertical) {
    pattern->matrix.d = tileScale;
  } else {
    pattern->matrix.a = tileScale;
  }
  pattern->filterMode = FilterMode::Nearest;
  pattern->mipmapMode = MipmapMode::None;
  pattern->scaleMode = ScaleMode::None;
  return pattern;
}

bool HTMLValueParser::buildRepeatingLinearStops(const std::vector<std::string>& parts,
                                                size_t stopStart, float lineLength,
                                                GradientStops& out) {
  // Parse one authored period into normalised (offset-along-line, color) pairs: a percentage is
  // already line-relative, a px length is normalised by the line length, and a color with no
  // position leaves NaN for `tileRepeatingStops` to fill. A color may carry up to two positions
  // (CSS shorthand for two coincident stops that form a hard colour edge).
  for (size_t i = stopStart; i < parts.size(); ++i) {
    auto tokens = SplitTopLevelWhitespace(parts[i]);
    if (tokens.empty()) continue;
    Color color = parseColor(tokens[0]);
    size_t positions = 0;
    for (size_t t = 1; t < tokens.size() && t <= 2; ++t) {
      const std::string& tk = tokens[t];
      float offset = NAN;
      if (!tk.empty() && tk.back() == '%') {
        float fraction = NAN;
        if (ParseCssPercentage(tk, fraction)) offset = fraction;
      } else {
        float px = parseAbsoluteLengthPx(tk);
        if (!std::isnan(px)) offset = px / lineLength;
      }
      out.emplace_back(offset, color);
      ++positions;
    }
    if (positions == 0) out.emplace_back(NAN, color);
  }
  if (out.size() < 2) return false;
  return tileRepeatingStops(out);
}

bool HTMLValueParser::tileRepeatingStops(GradientStops& stops) {
  if (stops.size() < 2) return false;
  if (std::isnan(stops.front().first)) stops.front().first = 0.0f;
  if (std::isnan(stops.back().first)) stops.back().first = 1.0f;
  for (size_t i = 1; i + 1 < stops.size(); ++i) {
    if (!std::isnan(stops[i].first)) continue;
    size_t next = i + 1;
    while (next < stops.size() && std::isnan(stops[next].first)) ++next;
    float prev = stops[i - 1].first;
    float nxt = next < stops.size() ? stops[next].first : 1.0f;
    float steps = static_cast<float>(next - (i - 1));
    stops[i].first = prev + (nxt - prev) / steps;
  }
  for (size_t i = 1; i < stops.size(); ++i) {
    if (stops[i].first < stops[i - 1].first) stops[i].first = stops[i - 1].first;
  }
  float origin = stops.front().first;
  float span = stops.back().first - origin;
  if (!(span > 0.0f)) return false;
  GradientStops period = stops;
  for (auto& stop : period) stop.first -= origin;

  GradientStops out;
  constexpr size_t kMaxStops = 1024;
  bool truncated = false;
  bool done = false;
  for (int k = 0; !done; ++k) {
    float base = static_cast<float>(k) * span;
    if (base > 1.0f) break;
    for (const auto& stop : period) {
      float pos = base + stop.first;
      if (pos >= 1.0f) {
        out.emplace_back(1.0f, stop.second);
        done = true;
        break;
      }
      out.emplace_back(pos, stop.second);
      if (out.size() >= kMaxStops) {
        truncated = true;
        done = true;
        break;
      }
    }
  }
  if (out.empty()) return false;
  if (out.back().first < 1.0f) out.emplace_back(1.0f, out.back().second);
  stops = std::move(out);
  if (truncated) {
    _diagnostics.warn("html: repeating gradient truncated (too many repetitions)");
  }
  return true;
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
  // offsets can be normalised against the gradient's radius in px.
  auto grad = _document->makeNode<RadialGradient>();
  grad->center = {0.5f, 0.5f};
  grad->radius = 0.5f;
  if (hasDescriptor) {
    parseRadialDescriptor(first, boxWidth, boxHeight, grad);
  }

  // The gradient's px extent is its radius in px, used both to tile a repeating period and to
  // normalise px-positioned stops onto the [0,1] radius axis. With the default fitsToGeometry model
  // the exporter scales the normalised radius by box width, so recover px as `radius * boxWidth`; a
  // px circle already stores its radius in px. NaN when the box size is unknown.
  float radiusPx = NAN;
  if (!std::isnan(boxWidth) && boxWidth > 0.0f) {
    radiusPx = grad->fitsToGeometry ? grad->radius * boxWidth : grad->radius;
  }

  // `repeating-radial-gradient` tiles the authored period across the normalised radius (offset
  // 1.0 == the gradient radius).
  GradientStops stops;
  bool tiled = false;
  if (repeating && std::isfinite(radiusPx) && radiusPx > 0.0f) {
    tiled = buildRepeatingLinearStops(parts, stopStart, radiusPx, stops);
  }
  if (!tiled) {
    stops = parseGradientStops(parts, stopStart, /*interpretAngularOffset=*/false, radiusPx);
    if (!finaliseGradientStops(stops)) return nullptr;
    if (repeating) {
      _diagnostics.warn(
          "html: repeating-radial-gradient approximated as a single non-repeating period");
    }
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

  // A single explicit length implies a circle. An extent keyword without a shape still uses CSS's
  // default ellipse, so it must not enter the circle-only pixel-radius path below.
  bool implicitCircle =
      !explicitEllipse && sizeTokens.size() == 1 && !IsRadialExtentKeyword(sizeTokens[0]);
  bool isCircle = explicitCircle || implicitCircle;

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

  // Radius: a length token divided by boxWidth recovers the normalised radius (a bare `<pct>%` is
  // already box-relative); track whether it came from an explicit px length so a circle on a
  // non-square box can later switch to the fitsToGeometry=false pixel model. An extent keyword
  // (or, for a circle, an omitted size — CSS defaults it to farthest-corner) has no scalar radius
  // in the token, so a circle derives the px radius from its center and the box; `circleExtentPx`
  // then routes it through the pixel model below since the value is already in px.
  bool radiusFromPxLength = false;
  bool circleExtentPx = false;
  if (!sizeTokens.empty() && boxWidth > 0) {
    float radius = resolveRadialLength(sizeTokens[0], boxWidth);
    if (!std::isnan(radius)) {
      grad->radius = radius;
      radiusFromPxLength = !sizeTokens[0].empty() && sizeTokens[0].back() != '%';
    } else if (IsRadialExtentKeyword(sizeTokens[0])) {
      // Only an explicit `circle` maps cleanly to PAGX's single radius. An implicit shape with an
      // extent keyword (or an explicit ellipse) is an ellipse in CSS and needs per-axis radii the
      // model can't represent, so keep the box-filling default and surface a diagnostic.
      if (explicitCircle && boxHeight > 0) {
        grad->radius = CircleExtentRadiusPx(sizeTokens[0], grad->center.x * boxWidth,
                                            grad->center.y * boxHeight, boxWidth, boxHeight);
        circleExtentPx = true;
      } else {
        _diagnostics.warn("html: radial-gradient size '" + sizeTokens[0] +
                          "' not supported; using box-filling radius");
      }
    } else {
      _diagnostics.warn("html: radial-gradient size '" + sizeTokens[0] +
                        "' not supported; using box-filling radius");
    }
  } else if (sizeTokens.empty() && explicitCircle && boxWidth > 0 && boxHeight > 0) {
    // A `circle` with no size defaults to farthest-corner in CSS.
    grad->radius = CircleExtentRadiusPx("", grad->center.x * boxWidth, grad->center.y * boxHeight,
                                        boxWidth, boxHeight);
    circleExtentPx = true;
  }

  // Keep a circle's single radius isotropic. The default fitsToGeometry=true model scales the
  // normalised radius by box width and height independently, so on a non-square box it would render
  // a circle as an ellipse; such circles switch to the fitsToGeometry=false pixel model (center /
  // radius in the geometry's local px space, where the box spans (0,0)-(boxWidth,boxHeight)). On a
  // square box the normalised model is already isotropic, so keep the compact representation:
  // extent/omitted sizes carry a px radius that is normalised back by boxWidth, while an explicit
  // px length was already normalised above. Ellipses and percentage sizes stay normalised too.
  bool nonSquare = boxWidth > 0 && boxHeight > 0 && std::abs(boxWidth - boxHeight) > 0.01f;
  if (circleExtentPx) {
    if (nonSquare) {
      grad->center = {grad->center.x * boxWidth, grad->center.y * boxHeight};
      grad->fitsToGeometry = false;
    } else {
      grad->radius = grad->radius / boxWidth;
    }
  } else if (isCircle && radiusFromPxLength && nonSquare) {
    grad->center = {grad->center.x * boxWidth, grad->center.y * boxHeight};
    grad->radius = grad->radius * boxWidth;
    grad->fitsToGeometry = false;
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

ConicGradient* HTMLValueParser::parseConicGradient(const std::string& value, bool repeating) {
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
  // `repeating-conic-gradient` tiles the authored angular period across the full turn; the stop
  // offsets are already normalised (angle / 360), so the shared normalised tiler applies directly.
  if (repeating) {
    if (!tileRepeatingStops(stops)) {
      if (!finaliseGradientStops(stops)) return nullptr;
      _diagnostics.warn(
          "html: repeating-conic-gradient approximated as a single non-repeating period");
    }
  } else if (!finaliseGradientStops(stops)) {
    return nullptr;
  }

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

  // CSS interpolates gradient stops in premultiplied-alpha space, so a `transparent` (or any
  // alpha=0) stop contributes only its neighbour's colour as the alpha fades — e.g. a
  // `rgba(220,210,255,0.4) -> transparent` ramp stays purple while vanishing. The renderer
  // interpolates unpremultiplied, where a keyword `transparent` carries black RGB and would drag
  // the ramp toward grey/black. Rewrite each fully transparent stop's RGB to that of its nearest
  // opaque neighbour (alpha kept at 0) so the unpremultiplied interpolation matches CSS. A stop
  // between two opaque colours prefers the earlier neighbour to avoid tinting the visible,
  // higher-alpha side of the fade.
  for (size_t i = 0; i < stops.size(); ++i) {
    if (stops[i].second.alpha > 0.0f) continue;
    size_t donor = stops.size();
    for (size_t back = i; back-- > 0;) {
      if (stops[back].second.alpha > 0.0f) {
        donor = back;
        break;
      }
    }
    if (donor == stops.size()) {
      for (size_t fwd = i + 1; fwd < stops.size(); ++fwd) {
        if (stops[fwd].second.alpha > 0.0f) {
          donor = fwd;
          break;
        }
      }
    }
    if (donor != stops.size()) {
      stops[i].second.red = stops[donor].second.red;
      stops[i].second.green = stops[donor].second.green;
      stops[i].second.blue = stops[donor].second.blue;
    }
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
