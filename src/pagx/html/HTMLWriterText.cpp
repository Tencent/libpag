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
#include <array>
#include <cmath>
#include <random>
#include <string>
#include "base/utils/MathUtil.h"
#include "pagx/html/FontHoist.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/svg/SVGTextLayout.h"
#include "pagx/types/SelectorTypes.h"
#include "pagx/types/TrimType.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

// Mirrors tgfx/src/core/utils/FauxBoldScale.cpp so HTML export can emit a stroke width that
// matches the native PAGX render. tgfx strokes the glyph path with `fontSize * FauxBoldScale`
// and unions it with the original fill, which is a path-level operation that reliably
// thickens glyphs even at sub-pixel widths. Chromium's `-webkit-text-stroke` raster pipeline
// silently drops sub-pixel strokes (values below ~1px produce zero visible thickening), so
// using the raw tgfx value as the CSS stroke width leaves small-font fauxBold invisible in
// HTML. Add a 0.5px bias to clear Chromium's activation threshold; the resulting visible
// thickening matches the tgfx render across the common fontSize range (empirically verified
// for fontSize 9..36 against Chromium on macOS at 2x device pixel ratio).
float FauxBoldStrokeWidth(float fontSize) {
  constexpr float keySmall = 9.0f;
  constexpr float keyLarge = 36.0f;
  constexpr float valSmall = 1.0f / 24.0f;
  constexpr float valLarge = 1.0f / 32.0f;
  float scale;
  if (fontSize <= keySmall) {
    scale = valSmall;
  } else if (fontSize >= keyLarge) {
    scale = valLarge;
  } else {
    float t = (fontSize - keySmall) / (keyLarge - keySmall);
    scale = valSmall + (valLarge - valSmall) * t;
  }
  return fontSize * scale + 0.5f;
}

//==============================================================================
// Text helper statics
//==============================================================================

// Approximate character advance widths as fractions of fontSize for common proportional fonts
// (Arial / Helvetica). These values are derived from the actual glyph advance widths in
// Arial at 2048 unitsPerEm, normalized to the range [0..1] of one em.
bool IsCJKCodepoint(int32_t ch) {
  return (ch >= 0x2E80 && ch <= 0x9FFF) || (ch >= 0xF900 && ch <= 0xFAFF) ||
         (ch >= 0xFE30 && ch <= 0xFE4F) || (ch >= 0x20000 && ch <= 0x2FA1F) ||
         (ch >= 0x30000 && ch <= 0x323AF) || (ch >= 0x3000 && ch <= 0x30FF) ||
         (ch >= 0x3100 && ch <= 0x312F) || (ch >= 0xAC00 && ch <= 0xD7AF);
}

float EstimateCharAdvanceHTML(int32_t ch, float fontSize) {
  if (IsCJKCodepoint(ch)) {
    return fontSize;
  }
  if (ch == ' ') {
    return fontSize * 0.28f;
  }
  if (ch == '\t') {
    return fontSize * 4.0f;
  }
  // Per-character width ratios for ASCII printable range (Arial / Helvetica reference).
  if (ch >= '!' && ch <= '~') {
    // clang-format off
    static const float kWidths[94] = {
      // !      "      #      $      %      &      '      (      )      *
      0.28f, 0.36f, 0.56f, 0.56f, 0.89f, 0.67f, 0.19f, 0.33f, 0.33f, 0.39f,
      // +      ,      -      .      /      0      1      2      3      4
      0.58f, 0.28f, 0.33f, 0.28f, 0.28f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f,
      // 5      6      7      8      9      :      ;      <      =      >
      0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.28f, 0.28f, 0.58f, 0.58f, 0.58f,
      // ?      @      A      B      C      D      E      F      G      H
      0.56f, 1.02f, 0.67f, 0.67f, 0.72f, 0.72f, 0.67f, 0.61f, 0.78f, 0.72f,
      // I      J      K      L      M      N      O      P      Q      R
      0.28f, 0.50f, 0.67f, 0.56f, 0.83f, 0.72f, 0.78f, 0.67f, 0.78f, 0.72f,
      // S      T      U      V      W      X      Y      Z      [      backslash
      0.67f, 0.61f, 0.72f, 0.67f, 0.94f, 0.67f, 0.67f, 0.61f, 0.28f, 0.28f,
      // ]      ^      _      `      a      b      c      d      e      f
      0.28f, 0.47f, 0.56f, 0.33f, 0.56f, 0.56f, 0.50f, 0.56f, 0.56f, 0.28f,
      // g      h      i      j      k      l      m      n      o      p
      0.56f, 0.56f, 0.22f, 0.22f, 0.50f, 0.22f, 0.83f, 0.56f, 0.56f, 0.56f,
      // q      r      s      t      u      v      w      x      y      z
      0.56f, 0.33f, 0.50f, 0.28f, 0.56f, 0.50f, 0.72f, 0.50f, 0.50f, 0.50f,
      // {      |      }      ~
      0.33f, 0.26f, 0.33f, 0.58f,
    };
    // clang-format on
    return fontSize * kWidths[ch - '!'];
  }
  // Fallback for non-ASCII Latin and other scripts.
  return fontSize * 0.55f;
}

// RangeSelector shape functions
static float SelectorShapeSquare(float) {
  return 1.0f;
}

static float SelectorShapeRampUp(float t) {
  return t;
}

static float SelectorShapeRampDown(float t) {
  return 1.0f - t;
}

static float SelectorShapeTriangle(float t) {
  return t < 0.5f ? 2.0f * t : 2.0f * (1.0f - t);
}

static float SelectorShapeRound(float t) {
  return std::sin(t * static_cast<float>(M_PI));
}

static float SelectorShapeSmooth(float t) {
  return 0.5f - 0.5f * std::cos(t * static_cast<float>(M_PI));
}

float ApplySelectorShape(SelectorShape shape, float t) {
  switch (shape) {
    case SelectorShape::Square:
      return SelectorShapeSquare(t);
    case SelectorShape::RampUp:
      return SelectorShapeRampUp(t);
    case SelectorShape::RampDown:
      return SelectorShapeRampDown(t);
    case SelectorShape::Triangle:
      return SelectorShapeTriangle(t);
    case SelectorShape::Round:
      return SelectorShapeRound(t);
    case SelectorShape::Smooth:
      return SelectorShapeSmooth(t);
    default:
      return 1.0f;
  }
}

float CombineSelectorValues(SelectorMode mode, float a, float b) {
  switch (mode) {
    case SelectorMode::Add:
      return a + b;
    case SelectorMode::Subtract:
      return b >= 0 ? a * (1.0f - b) : a * (-1.0f - b);
    case SelectorMode::Intersect:
      return a * b;
    case SelectorMode::Min:
      return std::min(a, b);
    case SelectorMode::Max:
      return std::max(a, b);
    case SelectorMode::Difference:
      return std::abs(a - b);
    default:
      return a + b;
  }
}

// Lookup table for seed=0 special case, matching AE behavior (TextSelectorRenderer.cpp).
static const struct {
  int end;
  int index;
} randomRanges[] = {{2, 0}, {4, 2}, {5, 4}, {20, 5}, {69, 20}, {127, 69}, {206, 127}, {10000, 205}};

static int GetRandomIndex(int textCount) {
  for (size_t i = 0; i < sizeof(randomRanges) / sizeof(randomRanges[0]); i++) {
    if (textCount <= randomRanges[i].end) {
      return randomRanges[i].index;
    }
  }
  return 0;
}

static std::vector<size_t> CalculateRandomIndices(uint16_t seed, size_t textCount) {
  std::mt19937 rng(seed);
  std::vector<std::pair<uint32_t, size_t>> randList;
  randList.reserve(textCount);
  for (size_t i = 0; i < textCount; i++) {
    randList.push_back({rng(), i});
  }
  std::sort(randList.begin(), randList.end(),
            [](const std::pair<uint32_t, size_t>& a, const std::pair<uint32_t, size_t>& b) {
              return a.first < b.first;
            });
  std::vector<size_t> indices(textCount);
  for (size_t i = 0; i < textCount; i++) {
    indices[i] = randList[i].second;
  }
  if (seed == 0 && textCount > 1) {
    auto m = static_cast<size_t>(GetRandomIndex(static_cast<int>(textCount)));
    size_t k = 0;
    while (k < textCount && indices[k] != m) {
      k++;
    }
    if (k < textCount) {
      std::swap(indices[0], indices[k]);
    }
  }
  return indices;
}

// Cubic equation solver used by the Triangle selector shape to invert its bezier
// x-parameterisation and recover t from a given textCenter x-coordinate. Ported from tgfx
// TextSelector.cpp (Shengjin's formula) so the HTML emission matches PAGX native pixel-for-pixel.
struct CubicSolutions {
  std::array<double, 3> values = {};
  size_t count = 0;
};

static CubicSolutions SolveCubicEquation(double a, double b, double c, double d) {
  CubicSolutions solutions = {};
  if (a == 0) {
    return solutions;
  }
  auto A = b * b - 3 * a * c;
  auto B = b * c - 9 * a * d;
  auto C = c * c - 3 * b * d;
  auto delta = B * B - 4 * A * C;
  if (A == 0 && B == 0) {
    solutions.values[solutions.count++] = -b / (3 * a);
  } else if (delta == 0 && A != 0) {
    auto k = B / A;
    solutions.values[solutions.count++] = -b / a + k;
    solutions.values[solutions.count++] = -0.5 * k;
  } else if (delta > 0) {
    auto y1 = A * b + 1.5 * a * (-B + std::sqrt(delta));
    auto y2 = A * b + 1.5 * a * (-B - std::sqrt(delta));
    solutions.values[solutions.count++] = (-b - std::cbrt(y1) - std::cbrt(y2)) / (3 * a);
  } else if (delta < 0 && A > 0) {
    auto t = (A * b - 1.5 * a * B) / (A * std::sqrt(A));
    if (-1 < t && t < 1) {
      auto theta = std::acos(t);
      auto sqrtA = std::sqrt(A);
      auto cosA = std::cos(theta / 3);
      auto sinA = std::sin(theta / 3);
      solutions.values[solutions.count++] = (-b - 2 * sqrtA * cosA) / (3 * a);
      solutions.values[solutions.count++] = (-b + sqrtA * (cosA + std::sqrt(3.0) * sinA)) / (3 * a);
      solutions.values[solutions.count++] = (-b + sqrtA * (cosA - std::sqrt(3.0) * sinA)) / (3 * a);
    }
  }
  return solutions;
}

// Triangle shape factor with bezier-shaped easeIn/easeOut, ported from tgfx
// TextSelector.cpp CalculateTriangleFactor. Differs from a "linear triangle multiplied by
// 1 - easeIn*(1 - t)" approximation: the tgfx form is a cubic bezier whose control points pull
// the factor curve's knee toward the range center, flattening the peak and compressing the
// transition band — that's why characters far from the center ("EAS" in "EASED MOTION" with
// easeIn=easeOut=0.8) land at near-zero factor while the characters one step closer to the
// center ("ED") pick up most of the peak offset.
static float CalculateTriangleFactorBezier(float textCenter, float rangeStart, float rangeEnd,
                                           float easeOut, float easeIn) {
  if (rangeStart >= rangeEnd) {
    return 0.0f;
  }
  double x = textCenter;
  if (x < rangeStart || x > rangeEnd) {
    return 0.0f;
  }
  double rangeCenter = (rangeStart + rangeEnd) * 0.5;
  // Pick the half of the triangle that holds x, parameterising from its edge (y=0) to the
  // center (y=1). Bezier control points mirror tgfx's layout exactly.
  double x1 = x <= rangeCenter ? rangeStart : rangeEnd;
  double y1 = 0;
  double step = rangeCenter - x1;
  double x2 = easeIn >= 0 ? x1 + easeIn * step : x1;
  double y2 = easeIn >= 0 ? 0 : -easeIn;
  double x3 = easeOut >= 0 ? rangeCenter - easeOut * step : rangeCenter;
  double y3 = easeOut >= 0 ? 1 : 1 + easeOut;
  double x4 = rangeCenter;
  double y4 = 1;

  // Invert the cubic bezier x(t)=((1-t)^3 x1 + 3(1-t)^2 t x2 + 3(1-t) t^2 x3 + t^3 x4) for t.
  auto a = -x1 + 3 * x2 - 3 * x3 + x4;
  auto b = 3 * (x1 - 2 * x2 + x3);
  auto c = 3 * (-x1 + x2);
  auto d = x1 - x;

  double t = 0;
  auto solutions = SolveCubicEquation(a, b, c, d);
  for (size_t i = 0; i < solutions.count; i++) {
    auto solution = solutions.values[i];
    if ((solution >= 0 && solution <= 1) || std::abs(solution - 1) <= 1e-6) {
      t = solution;
      break;
    }
  }
  double oneMinusT = 1 - t;
  return static_cast<float>(oneMinusT * oneMinusT * oneMinusT * y1 +
                            3 * oneMinusT * oneMinusT * t * y2 + 3 * oneMinusT * t * t * y3 +
                            t * t * t * y4);
}

float ComputeRangeSelectorFactor(const RangeSelector* selector, size_t glyphIndex,
                                 size_t totalGlyphs) {
  if (totalGlyphs == 0) {
    return 0.0f;
  }
  auto textCount = static_cast<float>(totalGlyphs);
  auto textStart = static_cast<float>(glyphIndex) / textCount;
  auto textEnd = static_cast<float>(glyphIndex + 1) / textCount;
  auto textCenter = (textStart + textEnd) * 0.5f;

  float rangeStart = selector->start;
  float rangeEnd = selector->end;
  if (selector->unit == SelectorUnit::Index) {
    rangeStart = rangeStart / textCount;
    rangeEnd = rangeEnd / textCount;
  }
  float offset = selector->offset;
  if (selector->unit == SelectorUnit::Index) {
    offset = offset / textCount;
  }
  rangeStart += offset;
  rangeEnd += offset;
  if (rangeStart > rangeEnd) {
    std::swap(rangeStart, rangeEnd);
  }

  float factor = 0.0f;
  float rangeSize = rangeEnd - rangeStart;
  if (rangeSize <= 0) {
    if (textCenter >= rangeStart && textCenter <= rangeEnd) {
      factor = 1.0f;
    } else {
      factor = 0.0f;
    }
  } else {
    switch (selector->shape) {
      case SelectorShape::RampUp: {
        factor = (textCenter - rangeStart) / rangeSize;
        break;
      }
      case SelectorShape::RampDown: {
        factor = (rangeEnd - textCenter) / rangeSize;
        break;
      }
      case SelectorShape::Triangle: {
        // Use the bezier-based Triangle factor so easeIn/easeOut affect the curve exactly as
        // tgfx does — linear-triangle * post-multiplicative easing is a cheaper approximation
        // that fails to reproduce the flattened peak and compressed transition visible e.g.
        // in selector_advanced.pagx EASED MOTION (easeIn=easeOut=0.8).
        factor = CalculateTriangleFactorBezier(textCenter, rangeStart, rangeEnd, selector->easeOut,
                                               selector->easeIn);
        break;
      }
      case SelectorShape::Round: {
        float mid = (rangeStart + rangeEnd) * 0.5f;
        float radius = rangeSize * 0.5f;
        float dist = std::abs(textCenter - mid);
        if (dist >= radius) {
          factor = 0.0f;
        } else {
          factor = std::sqrt(1.0f - (dist * dist) / (radius * radius));
        }
        break;
      }
      case SelectorShape::Smooth: {
        // Mirrors tgfx TextSelector.cpp CalculateSmoothFactor: a symmetric peak centered on
        // the range's midpoint, shaped by the cubic bezier (0.5, 0.0, 0.5, 1.0). tgfx's
        // Smooth ignores easeIn/easeOut (those only affect Triangle). Emitting a monotonic
        // smoothstep here would let the right half of the range stay opaque instead of
        // fading back to zero at rangeEnd.
        if (textCenter <= rangeStart || textCenter >= rangeEnd) {
          factor = 0.0f;
          break;
        }
        float rangeCenter = (rangeStart + rangeEnd) * 0.5f;
        float x = textCenter < rangeCenter ? (textCenter - rangeStart) / (rangeCenter - rangeStart)
                                           : (rangeEnd - textCenter) / (rangeEnd - rangeCenter);
        x = std::clamp(x, 0.0f, 1.0f);
        // Cubic bezier easing with control points (0.5, 0.0) and (0.5, 1.0): solve for t
        // from x via Newton-Raphson, then evaluate y(t).
        constexpr float cx1 = 0.5f, cy1 = 0.0f, cx2 = 0.5f, cy2 = 1.0f;
        float t = x;
        for (int i = 0; i < 8; i++) {
          float tt = t * t;
          float ttt = tt * t;
          float mt = 1.0f - t;
          float mt2 = mt * mt;
          float xt = 3.0f * mt2 * t * cx1 + 3.0f * mt * tt * cx2 + ttt;
          float dx = xt - x;
          if (std::abs(dx) < 1e-6f) {
            break;
          }
          float dxt = 3.0f * mt2 * cx1 + 6.0f * mt * t * (cx2 - cx1) + 3.0f * tt * (1.0f - cx2);
          if (std::abs(dxt) < 1e-6f) {
            break;
          }
          t = std::clamp(t - dx / dxt, 0.0f, 1.0f);
        }
        float tt = t * t;
        float ttt = tt * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        factor = 3.0f * mt2 * t * cy1 + 3.0f * mt * tt * cy2 + ttt;
        break;
      }
      default: {
        if (textStart >= rangeEnd || textEnd <= rangeStart) {
          factor = 0.0f;
        } else {
          auto ts = std::max(textStart, rangeStart);
          auto te = std::min(textEnd, rangeEnd);
          factor = (te - ts) / (textEnd - textStart);
        }
        break;
      }
    }
  }
  if (factor < 0.0f) {
    factor = 0.0f;
  } else if (factor > 1.0f) {
    factor = 1.0f;
  }
  // Triangle handles easeIn/easeOut inside CalculateTriangleFactorBezier via bezier control
  // points. Other shapes are untouched by easeIn/easeOut in tgfx — see TextSelector.cpp where
  // only CalculateTriangleFactor consumes those attributes — so no post-scale is applied here.
  return factor * selector->weight;
}

//==============================================================================
// Arc-length LUT
//==============================================================================

ArcLengthLUT BuildArcLengthLUT(const PathData& pathData, int samplesPerSegment) {
  ArcLengthLUT lut = {};
  lut.arcLengths.push_back(0.0f);
  Point startPoint = {};
  Point currentPoint = {};
  float cumLength = 0.0f;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        lut.positions.push_back(currentPoint);
        lut.tangents.push_back(0.0f);
        break;
      case PathVerb::Line: {
        float dx = pts[0].x - currentPoint.x;
        float dy = pts[0].y - currentPoint.y;
        float segLen = std::sqrt(dx * dx + dy * dy);
        float angle = std::atan2(dy, dx);
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          Point p = {currentPoint.x + dx * t, currentPoint.y + dy * t};
          cumLength += segLen / samplesPerSegment;
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[0];
        break;
      }
      case PathVerb::Quad: {
        Point p0 = currentPoint;
        Point p1 = pts[0];
        Point p2 = pts[1];
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          float u = 1.0f - t;
          Point p = {u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
                     u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y};
          Point tan = {2 * (1 - t) * (p1.x - p0.x) + 2 * t * (p2.x - p1.x),
                       2 * (1 - t) * (p1.y - p0.y) + 2 * t * (p2.y - p1.y)};
          float angle = std::atan2(tan.y, tan.x);
          float dx = p.x - lut.positions.back().x;
          float dy = p.y - lut.positions.back().y;
          cumLength += std::sqrt(dx * dx + dy * dy);
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[1];
        break;
      }
      case PathVerb::Cubic: {
        Point p0 = currentPoint;
        Point p1 = pts[0];
        Point p2 = pts[1];
        Point p3 = pts[2];
        for (int i = 1; i <= samplesPerSegment; i++) {
          float t = static_cast<float>(i) / samplesPerSegment;
          float u = 1.0f - t;
          Point p = {
              u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
              u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y};
          Point tan = {
              3 * u * u * (p1.x - p0.x) + 6 * u * t * (p2.x - p1.x) + 3 * t * t * (p3.x - p2.x),
              3 * u * u * (p1.y - p0.y) + 6 * u * t * (p2.y - p1.y) + 3 * t * t * (p3.y - p2.y)};
          float angle = std::atan2(tan.y, tan.x);
          float dx = p.x - lut.positions.back().x;
          float dy = p.y - lut.positions.back().y;
          cumLength += std::sqrt(dx * dx + dy * dy);
          lut.arcLengths.push_back(cumLength);
          lut.positions.push_back(p);
          lut.tangents.push_back(angle);
        }
        currentPoint = pts[2];
        break;
      }
      case PathVerb::Close: {
        float dx = startPoint.x - currentPoint.x;
        float dy = startPoint.y - currentPoint.y;
        float segLen = std::sqrt(dx * dx + dy * dy);
        if (segLen > 0.001f) {
          float angle = std::atan2(dy, dx);
          for (int i = 1; i <= samplesPerSegment; i++) {
            float t = static_cast<float>(i) / samplesPerSegment;
            Point p = {currentPoint.x + dx * t, currentPoint.y + dy * t};
            cumLength += segLen / samplesPerSegment;
            lut.arcLengths.push_back(cumLength);
            lut.positions.push_back(p);
            lut.tangents.push_back(angle);
          }
        }
        currentPoint = startPoint;
        break;
      }
    }
  });
  lut.totalLength = cumLength;
  return lut;
}

void SampleArcLengthLUT(const ArcLengthLUT& lut, float arcLength, Point* outPos, float* outTangent,
                        bool closed) {
  if (lut.arcLengths.empty()) {
    *outPos = {};
    *outTangent = 0;
    return;
  }
  if (closed && lut.totalLength > 0) {
    arcLength = std::fmod(arcLength, lut.totalLength);
    if (arcLength < 0) {
      arcLength += lut.totalLength;
    }
  }
  arcLength = std::max(0.0f, std::min(arcLength, lut.totalLength));
  auto it = std::lower_bound(lut.arcLengths.begin(), lut.arcLengths.end(), arcLength);
  size_t idx = static_cast<size_t>(std::distance(lut.arcLengths.begin(), it));
  if (idx >= lut.positions.size()) {
    idx = lut.positions.size() - 1;
  }
  if (idx == 0) {
    *outPos = lut.positions[0];
    *outTangent = lut.tangents[0];
    return;
  }
  float prevLen = lut.arcLengths[idx - 1];
  float nextLen = lut.arcLengths[idx];
  float segLen = nextLen - prevLen;
  float t = (segLen > 0.001f) ? (arcLength - prevLen) / segLen : 0.0f;
  Point prevPos = lut.positions[idx - 1];
  Point nextPos = lut.positions[idx];
  *outPos = {prevPos.x + (nextPos.x - prevPos.x) * t, prevPos.y + (nextPos.y - prevPos.y) * t};
  *outTangent = lut.tangents[idx];
}

//==============================================================================
// HTMLWriter – text methods
//==============================================================================

void HTMLWriter::writeText(HTMLBuilder& out, const Text* text, const Fill* fill,
                           const Stroke* stroke, const TextBox* tb, float alpha) {
  if (!text->glyphRuns.empty()) {
    writeGlyphRunSVG(out, text, fill, stroke, alpha);
    return;
  }
  if (text->text.empty()) {
    return;
  }
  bool fontHoisted =
      !_ctx->fontHoistSignature.fontFamily.empty() || _ctx->fontHoistSignature.renderFontSize > 0;
  std::string style;
  style.reserve(300);
  if (tb) {
    float tbW = tb->width;
    float tbH = tb->height;
    auto tbRenderPos = tb->renderPosition();
    float tbLeft = tbRenderPos.x;
    float tbTop = tbRenderPos.y;
    style +=
        "position:absolute;left:" + FloatToString(tbLeft) + "px;top:" + FloatToString(tbTop) + "px";
    if (!std::isnan(tbW) && tbW > 0) {
      style += ";width:" + FloatToString(tbW) + "px";
    }
    if (!std::isnan(tbH) && tbH > 0) {
      style += ";height:" + FloatToString(tbH) + "px";
    }
    if (tb->textAlign == TextAlign::Center) {
      style += ";text-align:center";
    } else if (tb->textAlign == TextAlign::End) {
      style += ";text-align:end";
    } else if (tb->textAlign == TextAlign::Justify) {
      style += ";text-align:justify";
    }
    if (tb->paragraphAlign != ParagraphAlign::Near) {
      style += ";display:flex;flex-direction:column";
      if (tb->paragraphAlign == ParagraphAlign::Middle) {
        style += ";justify-content:center";
      } else if (tb->paragraphAlign == ParagraphAlign::Far) {
        style += ";justify-content:flex-end";
      }
    }
    if (tb->writingMode == WritingMode::Vertical) {
      style += ";writing-mode:vertical-rl";
    }
    if (tb->lineHeight > 0) {
      style += ";line-height:" + FloatToString(tb->lineHeight) + "px";
    }
    if (tb->wordWrap && !std::isnan(tbW) && tbW > 0) {
      style += ";word-wrap:break-word";
    } else {
      style += ";white-space:nowrap";
    }
    if (tb->overflow == Overflow::Hidden) {
      style += ";overflow:hidden";
    }
  } else {
    auto renderPos = text->renderPosition();
    auto renderFont = text->renderFontSize();
    float ty = renderPos.y;
    if (text->baseline == TextBaseline::Alphabetic) {
      ty -= renderFont * 0.8f;
    }
    style += "position:absolute;left:" + FloatToString(renderPos.x) +
             "px;top:" + FloatToString(ty) + "px;white-space:pre";
    // line-height is hoisted to the parent Layer when fontHoisted; otherwise emit it on the span.
    if (!fontHoisted) {
      auto lineHeight = text->fontLineHeight() > 0 ? text->fontLineHeight() : renderFont;
      style += ";line-height:" + FloatToString(lineHeight) + "px";
    }
  }
  std::string textTransform;
  if (!tb) {
    if (text->textAnchor == TextAnchor::Center) {
      textTransform += "translateX(-50%)";
    } else if (text->textAnchor == TextAnchor::End) {
      textTransform += "translateX(-100%)";
    }
  }
  if (text->fauxItalic) {
    if (!textTransform.empty()) {
      textTransform += ' ';
    }
    textTransform += "skewX(-12deg)";
  }
  if (!textTransform.empty()) {
    style += ";transform:" + textTransform;
  }
  // Font properties are hoisted to the parent Layer when fontHoisted; skip them on the span.
  if (!fontHoisted) {
    if (!text->fontFamily.empty()) {
      std::string escapedFamily = text->fontFamily;
      for (size_t pos = 0; (pos = escapedFamily.find('\'', pos)) != std::string::npos; pos += 2) {
        escapedFamily.replace(pos, 1, "\\'");
      }
      style += ";font-family:'" + escapedFamily + "'";
    }
    style += ";font-size:" + FloatToString(text->renderFontSize()) + "px";
    if (text->letterSpacing != 0.0f) {
      style += ";letter-spacing:" + FloatToString(text->letterSpacing) + "px";
    }
    if (!text->fontStyle.empty()) {
      if (text->fontStyle.find("Bold") != std::string::npos) {
        style += ";font-weight:bold";
      }
      if (text->fontStyle.find("Italic") != std::string::npos) {
        style += ";font-style:italic";
      }
    }
  }
  if (text->fauxBold && !stroke) {
    style += ";-webkit-text-stroke:" + FloatToString(FauxBoldStrokeWidth(text->renderFontSize())) +
             "px currentColor";
  }
  if (fill && fill->color) {
    auto ct = fill->color->nodeType();
    if (ct == NodeType::SolidColor) {
      auto sc = static_cast<const SolidColor*>(fill->color);
      style += ";color:" + ColorToRGBA(sc->color, fill->alpha);
    } else {
      float ca = 1.0f;
      std::string css = colorToCSS(fill->color, &ca);
      if (!css.empty()) {
        style += ";background:" + css;
        style += ";-webkit-background-clip:text;background-clip:text";
        style += ";-webkit-text-fill-color:transparent";
        if (fill->alpha < 1.0f) {
          alpha *= fill->alpha;
        }
      }
    }
  }
  if (stroke && stroke->color && stroke->color->nodeType() == NodeType::SolidColor) {
    auto sc = static_cast<const SolidColor*>(stroke->color);
    style += ";-webkit-text-stroke:" + FloatToString(stroke->width) + "px " +
             ColorToRGBA(sc->color, stroke->alpha);
    style += ";paint-order:stroke fill";
    if (!fill || !fill->color) {
      style += ";-webkit-text-fill-color:transparent";
    }
  }
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }
  out.openTag("span");
  out.addAttr("style", style);
  out.closeTagWithText(text->text);
}

void HTMLWriter::writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill,
                                  const Stroke* stroke, float alpha) {
  auto renderPos = text->renderPosition();
  auto paths = ComputeGlyphPaths(*text, renderPos.x, renderPos.y);
  bool hasBitmapGlyphs = false;
  for (auto* run : text->glyphRuns) {
    if (!run->font) {
      continue;
    }
    for (size_t i = 0; i < run->glyphs.size(); i++) {
      uint16_t glyphID = run->glyphs[i];
      if (glyphID == 0) {
        continue;
      }
      auto glyphIndex = static_cast<size_t>(glyphID) - 1;
      if (glyphIndex >= run->font->glyphs.size()) {
        continue;
      }
      auto* glyph = run->font->glyphs[glyphIndex];
      if (glyph && glyph->image) {
        hasBitmapGlyphs = true;
        break;
      }
    }
    if (hasBitmapGlyphs) {
      break;
    }
  }
  if (paths.empty() && !hasBitmapGlyphs) {
    return;
  }
  std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
  if (alpha < 1.0f) {
    svgStyle += ";opacity:" + FloatToString(alpha);
  }
  out.openTag("svg");
  out.addAttr("xmlns", "http://www.w3.org/2000/svg");
  out.addAttr("style", svgStyle);
  out.closeTagStart();
  if (!paths.empty()) {
    out.openTag("g");
    applySVGFill(out, fill);
    applySVGStroke(out, stroke);
    out.closeTagStart();
    for (auto& gp : paths) {
      out.openTag("path");
      out.addAttr("transform", MatrixToCSS(gp.transform));
      out.addAttr("d", PathDataToSVGString(*gp.pathData));
      out.closeTagSelfClosing();
    }
    out.closeTag();  // </g>
  }
  if (hasBitmapGlyphs) {
    float textPosX = renderPos.x;
    float textPosY = renderPos.y;
    for (auto* run : text->glyphRuns) {
      if (!run->font) {
        continue;
      }
      float scale = run->fontSize / static_cast<float>(run->font->unitsPerEm);
      float currentX = textPosX + run->x;
      for (size_t i = 0; i < run->glyphs.size(); i++) {
        uint16_t glyphID = run->glyphs[i];
        if (glyphID == 0) {
          continue;
        }
        auto glyphIndex = static_cast<size_t>(glyphID) - 1;
        if (glyphIndex >= run->font->glyphs.size()) {
          continue;
        }
        auto* glyph = run->font->glyphs[glyphIndex];
        if (!glyph) {
          continue;
        }
        float posX = 0;
        float posY = 0;
        if (i < run->positions.size()) {
          posX = textPosX + run->x + run->positions[i].x;
          posY = textPosY + run->y + run->positions[i].y;
          if (i < run->xOffsets.size()) {
            posX += run->xOffsets[i];
          }
        } else if (i < run->xOffsets.size()) {
          posX = textPosX + run->x + run->xOffsets[i];
          posY = textPosY + run->y;
        } else {
          posX = currentX;
          posY = textPosY + run->y;
        }
        currentX += glyph->advance * scale;
        if (!glyph->image) {
          continue;
        }
        float ox = glyph->offset.x;
        float oy = glyph->offset.y;
        Matrix glyphMatrix =
            Matrix::Translate(posX + ox * scale, posY + oy * scale) * Matrix::Scale(scale, scale);
        std::string src = GetImageSrc(glyph->image);
        out.openTag("image");
        out.addAttr("href", src);
        out.addAttr("transform", MatrixToCSS(glyphMatrix));
        out.closeTagSelfClosing();
      }
    }
  }
  out.closeTag();  // </svg>
}

void HTMLWriter::writeTextModifier(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                                   const TextModifier* modifier, const Fill* fill,
                                   const Stroke* stroke, const TextBox* /*tb*/, float alpha) {
  for (auto& g : geos) {
    if (g.type != NodeType::Text) {
      continue;
    }
    auto text = static_cast<const Text*>(g.element);
    size_t totalGlyphs = 0;
    if (!text->glyphRuns.empty()) {
      for (auto* run : text->glyphRuns) {
        totalGlyphs += run->glyphs.size();
      }
    } else {
      const char* p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        totalGlyphs++;
        p += len;
      }
    }
    if (totalGlyphs == 0) {
      continue;
    }
    std::vector<float> factors(totalGlyphs, 0.0f);
    // Pre-compute random indices if any selector uses randomOrder.
    std::vector<size_t> randomIndices;
    for (auto* selector : modifier->selectors) {
      if (selector->nodeType() == NodeType::RangeSelector) {
        auto rs = static_cast<const RangeSelector*>(selector);
        if (rs->randomOrder && randomIndices.empty()) {
          randomIndices =
              CalculateRandomIndices(static_cast<uint16_t>(rs->randomSeed), totalGlyphs);
        }
      }
    }
    for (size_t i = 0; i < totalGlyphs; i++) {
      float combinedFactor = 0.0f;
      bool firstSelector = true;
      for (auto* selector : modifier->selectors) {
        if (selector->nodeType() == NodeType::RangeSelector) {
          auto rs = static_cast<const RangeSelector*>(selector);
          size_t idx = (rs->randomOrder && i < randomIndices.size()) ? randomIndices[i] : i;
          float selectorFactor = ComputeRangeSelectorFactor(rs, idx, totalGlyphs);
          if (firstSelector) {
            combinedFactor = selectorFactor;
            firstSelector = false;
          } else {
            combinedFactor = CombineSelectorValues(rs->mode, combinedFactor, selectorFactor);
          }
        }
      }
      factors[i] = std::clamp(combinedFactor, -1.0f, 1.0f);
    }

    if (!text->glyphRuns.empty()) {
      std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("style", svgStyle);
      out.closeTagStart();
      size_t glyphIdx = 0;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            glyphIdx++;
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (!glyph || !glyph->path) {
            glyphIdx++;
            continue;
          }
          float f = (glyphIdx < factors.size()) ? factors[glyphIdx] : 0.0f;
          float absF = std::abs(f);
          float gx = run->x;
          float gy = run->y;
          if (i < run->xOffsets.size()) {
            gx += run->xOffsets[i];
          }
          if (i < run->positions.size()) {
            gx += run->positions[i].x;
            gy += run->positions[i].y;
          }
          float anchorX = glyph->advance * 0.5f * scale;
          float anchorY = 0;
          if (i < run->anchors.size()) {
            anchorX += run->anchors[i].x * scale;
            anchorY += run->anchors[i].y * scale;
          }
          Matrix m = Matrix::Scale(scale, scale);
          float modAnchorX = anchorX + modifier->anchor.x * absF;
          float modAnchorY = anchorY + modifier->anchor.y * absF;
          m = Matrix::Translate(-modAnchorX, -modAnchorY) * m;
          float sx = 1.0f + (modifier->scale.x - 1.0f) * absF;
          float sy = 1.0f + (modifier->scale.y - 1.0f) * absF;
          m = Matrix::Scale(sx, sy) * m;
          if (!FloatNearlyZero(modifier->skew)) {
            m = Matrix::Rotate(modifier->skewAxis) * m;
            Matrix shear = {};
            shear.c = std::tan(DegreesToRadians(-modifier->skew * absF));
            m = shear * m;
            m = Matrix::Rotate(-modifier->skewAxis) * m;
          }
          if (!FloatNearlyZero(modifier->rotation)) {
            m = Matrix::Rotate(modifier->rotation * f) * m;
          }
          m = Matrix::Translate(modAnchorX, modAnchorY) * m;
          m = Matrix::Translate(modifier->position.x * f, modifier->position.y * f) * m;
          m = Matrix::Translate(gx, gy) * m;
          float glyphAlpha = 1.0f;
          if (modifier->alpha < 1.0f) {
            glyphAlpha = std::max(0.0f, 1.0f + (modifier->alpha - 1.0f) * absF);
          }
          out.openTag("g");
          out.addAttr("transform", MatrixToCSS(m));
          if (glyphAlpha < 1.0f) {
            out.addAttr("opacity", FloatToString(glyphAlpha));
          }
          out.closeTagStart();
          out.openTag("path");
          out.addAttr("d", PathDataToSVGString(*glyph->path));
          if (fill) {
            if (modifier->fillColor.has_value() && absF > 0.0f && fill->color &&
                fill->color->nodeType() == NodeType::SolidColor) {
              auto baseSC = static_cast<const SolidColor*>(fill->color);
              Color blended = LerpColor(baseSC->color, modifier->fillColor.value(), absF);
              out.addAttr("fill", ColorToSVGHex(blended));
              float fa = blended.alpha * fill->alpha;
              if (fa < 1.0f) {
                out.addAttr("fill-opacity", FloatToString(fa));
              }
            } else {
              applySVGFill(out, fill);
            }
          } else {
            out.addAttr("fill", "none");
          }
          if (stroke) {
            applySVGStroke(out, stroke);
            if (modifier->strokeColor.has_value() && absF > 0) {
              Color sc = modifier->strokeColor.value();
              float blendFactor = sc.alpha * absF;
              if (blendFactor > 0) {
                out.addAttr("stroke", ColorToSVGHex(sc));
                out.addAttr("stroke-opacity", FloatToString(blendFactor));
              }
            }
            if (modifier->strokeWidth.has_value() && absF > 0) {
              float origWidth = stroke->width;
              float modWidth = modifier->strokeWidth.value();
              float finalWidth = origWidth + (modWidth - origWidth) * absF;
              out.addAttr("stroke-width", FloatToString(finalWidth));
            }
          }
          out.closeTagSelfClosing();
          out.closeTag();  // </g>
          glyphIdx++;
        }
      }
      out.closeTag();  // </svg>
    } else {
      auto renderPos = text->renderPosition();
      auto renderFont = text->renderFontSize();
      float ty = renderPos.y;
      if (text->baseline == TextBaseline::Alphabetic) {
        ty -= renderFont * 0.8f;
      }
      auto lineHeight = text->fontLineHeight() > 0 ? text->fontLineHeight() : renderFont;
      std::string containerStyle =
          "position:absolute;white-space:nowrap;left:" + FloatToString(renderPos.x) +
          "px;top:" + FloatToString(ty) + "px;line-height:" + FloatToString(lineHeight) + "px";
      if (text->textAnchor == TextAnchor::Center) {
        containerStyle += ";transform:translateX(-50%)";
      } else if (text->textAnchor == TextAnchor::End) {
        containerStyle += ";transform:translateX(-100%)";
      }
      if (!text->fontFamily.empty()) {
        std::string escapedFamilyM = text->fontFamily;
        for (size_t p = 0; p < escapedFamilyM.size(); p++) {
          if (escapedFamilyM[p] == '\'') {
            escapedFamilyM.replace(p, 1, "\\'");
            p++;
          }
        }
        containerStyle += ";font-family:'" + escapedFamilyM + "'";
      }
      containerStyle += ";font-size:0";
      if (!text->fontStyle.empty()) {
        if (text->fontStyle.find("Bold") != std::string::npos) {
          containerStyle += ";font-weight:bold";
        }
        if (text->fontStyle.find("Italic") != std::string::npos) {
          containerStyle += ";font-style:italic";
        }
      }
      if (text->letterSpacing != 0.0f) {
        containerStyle += ";letter-spacing:" + FloatToString(text->letterSpacing) + "px";
      }
      if (alpha < 1.0f) {
        containerStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("div");
      out.addAttr("style", containerStyle);
      out.closeTagStart();
      std::string fontSizeStr = FloatToString(renderFont) + "px";
      const char* p = text->text.c_str();
      size_t charIdx = 0;
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        float f = (charIdx < factors.size()) ? factors[charIdx] : 0.0f;
        float absF = std::abs(f);
        std::string charStr(p, len);
        bool isSpace = (ch == ' ');
        if (isSpace) {
          charStr = "\u00A0";
        }
        std::string charStyle = "display:inline-block;font-size:" + fontSizeStr;
        std::string transform;
        if (!FloatNearlyZero(modifier->position.x * f) ||
            !FloatNearlyZero(modifier->position.y * f)) {
          transform += "translate(" + FloatToString(modifier->position.x * f) + "px," +
                       FloatToString(modifier->position.y * f) + "px) ";
        }
        float anchorX = modifier->anchor.x * f;
        float anchorY = modifier->anchor.y * f;
        if (!FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY)) {
          transform +=
              "translate(" + FloatToString(anchorX) + "px," + FloatToString(anchorY) + "px) ";
        }
        if (!FloatNearlyZero(modifier->rotation * f)) {
          transform += "rotate(" + FloatToString(modifier->rotation * f) + "deg) ";
        }
        float sx = 1.0f + (modifier->scale.x - 1.0f) * absF;
        float sy = 1.0f + (modifier->scale.y - 1.0f) * absF;
        if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
          transform += "scale(" + FloatToString(sx) + "," + FloatToString(sy) + ") ";
        }
        if (!FloatNearlyZero(modifier->skew * absF)) {
          // tgfx TextModifier::ApplySkew rotates by -skewAxis, applies skewX, then rotates back by
          // +skewAxis. This skews along an arbitrary axis (skewAxis=90 becomes a vertical skew).
          // CSS transform applies right-to-left on the point, so the equivalent sequence is
          // rotate(+axis) skewX(-skew) rotate(-axis). Skip the rotations when skewAxis is 0 to
          // keep the simpler output for the common pure-horizontal-skew case.
          if (!FloatNearlyZero(modifier->skewAxis)) {
            transform += "rotate(" + FloatToString(modifier->skewAxis) + "deg) ";
          }
          transform += "skewX(" + FloatToString(-modifier->skew * absF) + "deg) ";
          if (!FloatNearlyZero(modifier->skewAxis)) {
            transform += "rotate(" + FloatToString(-modifier->skewAxis) + "deg) ";
          }
        }
        if (!FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY)) {
          transform +=
              "translate(" + FloatToString(-anchorX) + "px," + FloatToString(-anchorY) + "px) ";
        }
        if (!transform.empty()) {
          charStyle += ";transform:" + transform;
          // Align transform pivot with the glyph baseline & advance center (tgfx uses
          // glyph.anchor = (advance/2, 0) on the alphabetic baseline). Without this,
          // rotation/scale would pivot around the inline-block center (50% 50%), shifting
          // glyphs incorrectly. 50% aligns with advance center; 0.8em approximates Arial
          // ascent, placing the origin on the baseline.
          charStyle += ";transform-origin:50% 0.8em";
        }
        if (modifier->alpha < 1.0f) {
          float charAlpha = std::max(0.0f, 1.0f + (modifier->alpha - 1.0f) * absF);
          charStyle += ";opacity:" + FloatToString(charAlpha);
        }
        if (fill && fill->color && fill->color->nodeType() == NodeType::SolidColor) {
          auto sc = static_cast<const SolidColor*>(fill->color);
          if (modifier->fillColor.has_value() && absF > 0.0f) {
            Color blended = LerpColor(sc->color, modifier->fillColor.value(), absF);
            charStyle += ";color:" + ColorToRGBA(blended, fill->alpha);
          } else {
            charStyle += ";color:" + ColorToRGBA(sc->color, fill->alpha);
          }
        } else if (!fill && stroke) {
          charStyle += ";color:transparent";
        }
        if (stroke) {
          Color strokeColor = {};
          float strokeWidth = stroke->width;
          bool hasStroke = false;
          if (modifier->strokeColor.has_value() && absF > 0.0f) {
            strokeColor = modifier->strokeColor.value();
            strokeColor.alpha *= absF;
            hasStroke = true;
          } else if (stroke->color && stroke->color->nodeType() == NodeType::SolidColor) {
            strokeColor = static_cast<const SolidColor*>(stroke->color)->color;
            hasStroke = true;
          }
          if (modifier->strokeWidth.has_value() && absF > 0.0f) {
            strokeWidth = stroke->width + (modifier->strokeWidth.value() - stroke->width) * absF;
          }
          if (hasStroke && strokeWidth > 0 && strokeColor.alpha > 0) {
            charStyle += ";-webkit-text-stroke:" + FloatToString(strokeWidth) + "px " +
                         ColorToRGBA(strokeColor, stroke->alpha);
            charStyle += ";paint-order:stroke fill";
          }
        }
        out.openTag("span");
        out.addAttr("style", charStyle);
        out.closeTagWithText(charStr);
        p += len;
        charIdx++;
      }
      out.closeTag();  // </div>
    }
  }
}

void HTMLWriter::writeTextPath(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                               const TextPath* textPath, const Fill* fill, const Stroke* stroke,
                               const TextBox*, float alpha) {
  if (!textPath->path || textPath->path->isEmpty()) {
    return;
  }
  PathData pathData = PathDataFromSVGString("");
  pathData = *textPath->path;
  bool isClosed = false;
  pathData.forEach([&](PathVerb verb, const Point*) {
    if (verb == PathVerb::Close) {
      isClosed = true;
    }
  });
  if (textPath->reversed) {
    PathData reversed = PathDataFromSVGString("");
    ReversePathData(pathData, reversed);
    pathData = reversed;
  }
  ArcLengthLUT lut = BuildArcLengthLUT(pathData);
  if (lut.totalLength <= 0) {
    return;
  }
  float effectiveLength = lut.totalLength - textPath->firstMargin - textPath->lastMargin;
  if (effectiveLength <= 0) {
    return;
  }

  for (auto& g : geos) {
    if (g.type != NodeType::Text) {
      continue;
    }
    auto text = static_cast<const Text*>(g.element);
    if (!text->glyphRuns.empty()) {
      std::string svgStyle = "position:absolute;left:0;top:0;overflow:visible";
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("style", svgStyle);
      out.closeTagStart();
      out.openTag("g");
      applySVGFill(out, fill);
      applySVGStroke(out, stroke);
      out.closeTagStart();
      float totalAdvance = 0.0f;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (glyph) {
            totalAdvance += glyph->advance * scale;
          }
        }
      }
      size_t glyphCount = 0;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (glyph) {
            glyphCount++;
          }
        }
      }
      float extraSpacing = 0.0f;
      if (textPath->forceAlignment && glyphCount > 1 && totalAdvance > 0) {
        extraSpacing = (effectiveLength - totalAdvance) / static_cast<float>(glyphCount - 1);
      }
      // Normal mode uses per-glyph anchor decomposition relative to baselineOrigin; forceAlignment
      // mode uses consecutive arc-length layout. See tgfx/src/layers/vectors/TextPath.cpp.
      float baselineAngleRad = textPath->baselineAngle * static_cast<float>(M_PI) / 180.0f;
      float baselineCos = std::cos(baselineAngleRad);
      float baselineSin = std::sin(baselineAngleRad);
      auto textRenderPos = text->renderPosition();
      auto baselineOrigin = textPath->renderBaselineOrigin();
      float currentArcPos = textPath->firstMargin;
      for (auto* run : text->glyphRuns) {
        if (!run->font) {
          continue;
        }
        float scale = run->fontSize / run->font->unitsPerEm;
        for (size_t i = 0; i < run->glyphs.size(); i++) {
          uint16_t glyphId = run->glyphs[i];
          if (glyphId == 0) {
            continue;
          }
          auto* glyph = (glyphId > 0 && glyphId <= run->font->glyphs.size())
                            ? run->font->glyphs[glyphId - 1]
                            : nullptr;
          if (!glyph || !glyph->path) {
            continue;
          }
          float glyphAdvance = glyph->advance * scale;
          float pathOffset = 0.0f;
          float normalOffset = 0.0f;
          float tangent = 0;
          Point pos = {};
          if (textPath->forceAlignment) {
            float glyphCenterArc = currentArcPos + glyphAdvance / 2.0f;
            SampleArcLengthLUT(lut, glyphCenterArc, &pos, &tangent, isClosed);
            if (i < run->positions.size()) {
              normalOffset = run->positions[i].y;
            }
            pathOffset = glyphCenterArc;
          } else {
            // Compute the glyph anchor in layer space, then decompose its offset from
            // baselineOrigin along the baseline angle into tangent (arc distance) and normal
            // (perpendicular-to-curve offset) components.
            float glyphAnchorX = textRenderPos.x + run->x + glyphAdvance * 0.5f;
            float glyphAnchorY = textRenderPos.y + run->y;
            if (i < run->positions.size()) {
              glyphAnchorX += run->positions[i].x;
              glyphAnchorY += run->positions[i].y;
            }
            if (i < run->xOffsets.size()) {
              glyphAnchorX += run->xOffsets[i];
            }
            if (i < run->anchors.size()) {
              glyphAnchorX += run->anchors[i].x;
              glyphAnchorY += run->anchors[i].y;
            }
            float dx = glyphAnchorX - baselineOrigin.x;
            float dy = glyphAnchorY - baselineOrigin.y;
            float tangentDistance = dx * baselineCos + dy * baselineSin;
            normalOffset = dy * baselineCos - dx * baselineSin;
            pathOffset = textPath->firstMargin + tangentDistance;
            SampleArcLengthLUT(lut, pathOffset, &pos, &tangent, isClosed);
          }
          float normalAngle = tangent + static_cast<float>(M_PI) / 2.0f;
          pos.x += normalOffset * std::cos(normalAngle);
          pos.y += normalOffset * std::sin(normalAngle);
          Matrix m = Matrix::Scale(scale, scale);
          m = Matrix::Translate(-glyph->advance / 2.0f, 0) * m;
          if (textPath->perpendicular) {
            float angleDeg = tangent * 180.0f / static_cast<float>(M_PI) - textPath->baselineAngle;
            m = Matrix::Rotate(angleDeg) * m;
          }
          m = Matrix::Translate(pos.x, pos.y) * m;
          out.openTag("path");
          out.addAttr("transform", MatrixToCSS(m));
          out.addAttr("d", PathDataToSVGString(*glyph->path));
          out.closeTagSelfClosing();
          currentArcPos += glyphAdvance + extraSpacing;
        }
      }
      out.closeTag();  // </g>
      out.closeTag();  // </svg>
    } else {
      auto renderFont = text->renderFontSize();
      float totalWidth = 0.0f;
      const char* p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        totalWidth += EstimateCharAdvanceHTML(ch, renderFont) + text->letterSpacing;
        p += len;
      }
      size_t charCount = 0;
      const char* q = text->text.c_str();
      while (*q) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(q, text->text.size() - (q - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        charCount++;
        q += len;
      }
      float extraSpacing = 0.0f;
      if (textPath->forceAlignment && charCount > 1 && totalWidth > 0) {
        extraSpacing = (effectiveLength - totalWidth) / static_cast<float>(charCount - 1);
      }
      // Normal mode uses per-character anchor decomposition relative to baselineOrigin (see
      // tgfx/src/layers/vectors/TextPath.cpp). The baseline here is approximated: character
      // anchor y sits at the alphabetic baseline (renderPos.y + renderFont * 0.8 when
      // baseline=LineBox), while anchor x is the character's advance center relative to the
      // Text origin.
      float baselineAngleRad = textPath->baselineAngle * static_cast<float>(M_PI) / 180.0f;
      float baselineCos = std::cos(baselineAngleRad);
      float baselineSin = std::sin(baselineAngleRad);
      auto textRenderPos = text->renderPosition();
      auto baselineOrigin = textPath->renderBaselineOrigin();
      float characterAnchorY = textRenderPos.y;
      if (text->baseline == TextBaseline::LineBox) {
        characterAnchorY += renderFont * 0.8f;
      }
      float characterCursorX = textRenderPos.x;
      float currentArcPos = textPath->firstMargin;
      p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        float charWidth = EstimateCharAdvanceHTML(ch, renderFont) + text->letterSpacing;
        float pathOffset = 0.0f;
        float normalOffset = 0.0f;
        float tangent = 0;
        Point pos = {};
        float charCenterArc = currentArcPos + charWidth / 2.0f;
        if (textPath->forceAlignment) {
          SampleArcLengthLUT(lut, charCenterArc, &pos, &tangent, isClosed);
          pathOffset = charCenterArc;
        } else {
          float anchorX = characterCursorX + charWidth * 0.5f;
          float dx = anchorX - baselineOrigin.x;
          float dy = characterAnchorY - baselineOrigin.y;
          float tangentDistance = dx * baselineCos + dy * baselineSin;
          normalOffset = dy * baselineCos - dx * baselineSin;
          pathOffset = textPath->firstMargin + tangentDistance;
          SampleArcLengthLUT(lut, pathOffset, &pos, &tangent, isClosed);
        }
        float normalAngle = tangent + static_cast<float>(M_PI) / 2.0f;
        pos.x += normalOffset * std::cos(normalAngle);
        pos.y += normalOffset * std::sin(normalAngle);
        std::string charStr(p, len);
        std::string charStyle = "position:absolute;left:" + FloatToString(pos.x) +
                                "px;top:" + FloatToString(pos.y) + "px";
        charStyle += ";display:inline-block";
        if (!text->fontFamily.empty()) {
          std::string escapedFamilyTP = text->fontFamily;
          for (size_t p = 0; p < escapedFamilyTP.size(); p++) {
            if (escapedFamilyTP[p] == '\'') {
              escapedFamilyTP.replace(p, 1, "\\'");
              p++;
            }
          }
          charStyle += ";font-family:'" + escapedFamilyTP + "'";
        }
        charStyle += ";font-size:" + FloatToString(renderFont) + "px";
        if (!text->fontStyle.empty()) {
          if (text->fontStyle.find("Bold") != std::string::npos) {
            charStyle += ";font-weight:bold";
          }
          if (text->fontStyle.find("Italic") != std::string::npos) {
            charStyle += ";font-style:italic";
          }
        }
        if (text->letterSpacing != 0.0f) {
          charStyle += ";letter-spacing:" + FloatToString(text->letterSpacing) + "px";
        }
        std::string transform;
        if (textPath->perpendicular) {
          float angleDeg = tangent * 180.0f / static_cast<float>(M_PI) - textPath->baselineAngle;
          transform += "rotate(" + FloatToString(angleDeg) + "deg) ";
        }
        // Align the character baseline to the path sample point. The sampled (pos.x, pos.y) is
        // where tgfx places the glyph baseline; we translate the span upward by the font ascent
        // so the baseline lands on pos.y. For Arial the typographic ascender is ~0.905em. Using
        // the full fontSize (1em) would place the span's bottom edge at pos.y, shifting glyphs
        // upward by ~0.1em.
        transform += "translate(-50%,-" + FloatToString(renderFont * 0.905f) + "px)";
        charStyle += ";transform:" + transform;
        charStyle += ";transform-origin:0 0";
        if (fill && fill->color) {
          if (fill->color->nodeType() == NodeType::SolidColor) {
            auto sc = static_cast<const SolidColor*>(fill->color);
            charStyle += ";color:" + ColorToRGBA(sc->color, fill->alpha);
          } else if (fill->color->nodeType() == NodeType::LinearGradient) {
            auto lg = static_cast<const LinearGradient*>(fill->color);
            if (!lg->colorStops.empty()) {
              float t = effectiveLength > 0 ? charCenterArc / effectiveLength : 0;
              if (t < 0) t = 0;
              if (t > 1) t = 1;
              Color sampled = lg->colorStops[0]->color;
              for (size_t si = 0; si + 1 < lg->colorStops.size(); si++) {
                float o0 = lg->colorStops[si]->offset;
                float o1 = lg->colorStops[si + 1]->offset;
                if (t >= o0 && t <= o1 && o1 > o0) {
                  float f = (t - o0) / (o1 - o0);
                  auto& c0 = lg->colorStops[si]->color;
                  auto& c1 = lg->colorStops[si + 1]->color;
                  sampled.red = c0.red + (c1.red - c0.red) * f;
                  sampled.green = c0.green + (c1.green - c0.green) * f;
                  sampled.blue = c0.blue + (c1.blue - c0.blue) * f;
                  break;
                }
              }
              charStyle += ";color:" + ColorToRGBA(sampled, fill->alpha);
            }
          } else {
            auto css = colorToCSS(fill->color, nullptr);
            if (!css.empty()) {
              charStyle += ";background:" + css;
              charStyle += ";-webkit-background-clip:text;background-clip:text";
              charStyle += ";-webkit-text-fill-color:transparent";
            }
          }
        }
        if (alpha < 1.0f) {
          charStyle += ";opacity:" + FloatToString(alpha);
        }
        out.openTag("span");
        out.addAttr("style", charStyle);
        out.closeTagWithText(charStr);
        currentArcPos += charWidth + extraSpacing;
        characterCursorX += charWidth;
        p += len;
      }
    }
  }
}

//==============================================================================
// HTMLWriter – trim & group/repeater/composition
//==============================================================================

void HTMLWriter::applyTrimAttrs(HTMLBuilder& builder, const TrimPath* trim) {
  if (!trim) {
    return;
  }
  if (trim->start == 0.0f && trim->end == 1.0f && FloatNearlyZero(trim->offset)) {
    return;
  }
  builder.addAttr("pathLength", "1");
  float offsetFrac = trim->offset / 360.0f;
  float s = trim->start + offsetFrac;
  float e = trim->end + offsetFrac;
  float shift = std::floor(s);
  s -= shift;
  e -= shift;
  bool wrapping = (e > 1.0f);
  if (e < s) {
    std::swap(s, e);
  }
  if (!wrapping) {
    float visible = e - s;
    float gap = 1.0f - visible;
    builder.addAttr("stroke-dasharray", FloatToString(visible) + " " + FloatToString(gap));
    builder.addAttr("stroke-dashoffset", FloatToString(-s));
  } else {
    float seg1 = 1.0f - s;
    float seg2 = (e > 1.0f) ? (e - 1.0f) : e;
    float gap = std::max(0.0f, 1.0f - seg1 - seg2);
    // Use 4-value dasharray to avoid SVG odd-count auto-duplication.
    builder.addAttr("stroke-dasharray", FloatToString(seg1) + " " + FloatToString(gap) + " " +
                                            FloatToString(seg2) + " 0");
    builder.addAttr("stroke-dashoffset", FloatToString(-s));
  }
}

float HTMLWriter::computeGeoPathLength(const GeoInfo& geo) {
  if (!geo.modifiedPathData.empty()) {
    PathData pathData = PathDataFromSVGString(geo.modifiedPathData);
    return ComputePathLength(pathData);
  }
  switch (geo.type) {
    case NodeType::Rectangle: {
      auto r = static_cast<const Rectangle*>(geo.element);
      if (r->roundness <= 0) {
        return 2 * (r->size.width + r->size.height);
      }
      PathData pathData = PathDataFromSVGString("");
      GeoToPathData(geo.element, geo.type, pathData);
      return ComputePathLength(pathData);
    }
    case NodeType::Ellipse: {
      PathData pathData = PathDataFromSVGString("");
      GeoToPathData(geo.element, geo.type, pathData);
      return ComputePathLength(pathData);
    }
    case NodeType::Path: {
      auto p = static_cast<const Path*>(geo.element);
      if (p->data) {
        return ComputePathLength(*p->data);
      }
      return 0;
    }
    case NodeType::Polystar: {
      std::string d = BuildPolystarPath(static_cast<const Polystar*>(geo.element));
      if (!d.empty()) {
        PathData pathData = PathDataFromSVGString(d);
        return ComputePathLength(pathData);
      }
      return 0;
    }
    default:
      return 0;
  }
}

bool HTMLWriter::isContinuousTrimVisible(const TrimPath* trim,
                                         const std::vector<float>& pathLengths, float totalLength,
                                         size_t geoIndex) {
  if (!trim || trim->type != TrimType::Continuous || totalLength <= 0.0f ||
      geoIndex >= pathLengths.size()) {
    return true;
  }
  float shapeStart = 0.0f;
  for (size_t i = 0; i < geoIndex; i++) {
    shapeStart += pathLengths[i];
  }
  float shapeLength = pathLengths[geoIndex];
  if (shapeLength <= 0.0f) {
    return false;
  }
  float shapeEnd = shapeStart + shapeLength;
  float offsetFrac = trim->offset / 360.0f;
  float start = trim->start + offsetFrac;
  float end = trim->end + offsetFrac;
  float shift = std::floor(start);
  start -= shift;
  end -= shift;
  float trimStart = start * totalLength;
  float trimEnd = end * totalLength;
  if (trimEnd <= totalLength) {
    return trimStart < shapeEnd && trimEnd > shapeStart;
  }
  float seg1End = totalLength;
  float seg2End = trimEnd - totalLength;
  return (trimStart < shapeEnd && seg1End > shapeStart) ||
         (0.0f < shapeEnd && seg2End > shapeStart);
}

void HTMLWriter::applyTrimAttrsContinuous(HTMLBuilder& builder, const TrimPath* trim,
                                          const std::vector<float>& pathLengths, float totalLength,
                                          size_t geoIndex) {
  if (!trim || trim->type != TrimType::Continuous) {
    applyTrimAttrs(builder, trim);
    return;
  }
  if (totalLength <= 0.0f || geoIndex >= pathLengths.size()) {
    return;
  }
  float shapeStart = 0.0f;
  for (size_t i = 0; i < geoIndex; i++) {
    shapeStart += pathLengths[i];
  }
  float shapeLength = pathLengths[geoIndex];
  float shapeEnd = shapeStart + shapeLength;
  if (shapeLength <= 0.0f) {
    builder.addAttr("stroke-dasharray", "0 1");
    return;
  }

  float offsetFrac = trim->offset / 360.0f;
  float start = trim->start + offsetFrac;
  float end = trim->end + offsetFrac;
  float shift = std::floor(start);
  start -= shift;
  end -= shift;
  float trimStart = start * totalLength;
  float trimEnd = end * totalLength;

  float localStart = 0.0f;
  float localEnd = 0.0f;
  bool hasSegment = false;

  if (trimEnd <= totalLength) {
    if (trimStart < shapeEnd && trimEnd > shapeStart) {
      localStart = std::max(0.0f, trimStart - shapeStart) / shapeLength;
      localEnd = std::min(shapeLength, trimEnd - shapeStart) / shapeLength;
      hasSegment = true;
    }
  } else {
    float seg1Start = trimStart;
    float seg1End = totalLength;
    float seg2Start = 0.0f;
    float seg2End = trimEnd - totalLength;
    bool hasSeg1 = seg1Start < shapeEnd && seg1End > shapeStart;
    bool hasSeg2 = seg2Start < shapeEnd && seg2End > shapeStart;
    if (hasSeg1 && hasSeg2) {
      localStart = std::max(0.0f, seg1Start - shapeStart) / shapeLength;
      localEnd = std::min(shapeLength, seg2End - shapeStart) / shapeLength + 1.0f;
      hasSegment = true;
    } else if (hasSeg1) {
      localStart = std::max(0.0f, seg1Start - shapeStart) / shapeLength;
      localEnd = std::min(shapeLength, seg1End - shapeStart) / shapeLength;
      hasSegment = true;
    } else if (hasSeg2) {
      localStart = std::max(0.0f, seg2Start - shapeStart) / shapeLength;
      localEnd = std::min(shapeLength, seg2End - shapeStart) / shapeLength;
      hasSegment = true;
    }
  }

  if (!hasSegment) {
    builder.addAttr("stroke-dasharray", "0 1");
    return;
  }

  builder.addAttr("pathLength", "1");
  if (localEnd <= 1.0f) {
    float visible = localEnd - localStart;
    float gap = 1.0f - visible;
    builder.addAttr("stroke-dasharray", FloatToString(visible) + " " + FloatToString(gap));
    builder.addAttr("stroke-dashoffset", FloatToString(-localStart));
  } else {
    float seg1 = 1.0f - localStart;
    float seg2 = localEnd - 1.0f;
    float gap = 1.0f - seg1 - seg2;
    builder.addAttr("stroke-dasharray",
                    FloatToString(seg2) + " " + FloatToString(gap) + " " + FloatToString(seg1));
  }
}

void HTMLWriter::writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute) {
  Matrix gm = BuildGroupMatrix(group);
  std::string style = "position:relative";
  if (!gm.isIdentity()) {
    style += ";transform:" + MatrixToCSS(gm);
    style += ";transform-origin:0 0";
  }
  if (group->alpha < 1.0f) {
    float ea = distribute ? (group->alpha * alpha) : group->alpha;
    style += ";opacity:" + FloatToString(ea);
  }
  out.openTag("div");
  out.addAttr("class", "pagx-group");
  out.addAttr("style", style);
  out.closeTagStart();
  writeElements(out, group->elements, 1.0f, false, LayerPlacement::Background);
  bool hasForegroundPainter = false;
  for (auto* e : group->elements) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForegroundPainter = true;
        break;
      }
    }
  }
  if (hasForegroundPainter) {
    writeElements(out, group->elements, 1.0f, false, LayerPlacement::Foreground);
  }
  out.closeTag();
}

void HTMLWriter::writeRepeater(HTMLBuilder& out, const Repeater* rep,
                               const std::vector<GeoInfo>& geos, const Fill* fill,
                               const Stroke* stroke, float alpha, const TrimPath* trim,
                               bool applyCopyAlphaDecay) {
  if (rep->copies <= 0) {
    return;
  }
  // Grab and clear the parent layer's Repeater origin offset so recursive writeRepeater calls
  // don't double-apply it. The offset is non-zero only when the enclosing layer shifted its
  // div into negative quadrants to cover every copy's union bounds (see writeLayer).
  float originOffsetX = _ctx->repeaterOriginOffsetX;
  float originOffsetY = _ctx->repeaterOriginOffsetY;
  _ctx->repeaterOriginOffsetX = 0;
  _ctx->repeaterOriginOffsetY = 0;
  int n = static_cast<int>(std::ceil(rep->copies));
  float frac = rep->copies - std::floor(rep->copies);
  for (int i = 0; i < n; i++) {
    int idx = (rep->order == RepeaterOrder::AboveOriginal) ? i : (n - 1 - i);
    float prog = static_cast<float>(idx) + rep->offset;
    Matrix m = {};
    if (!FloatNearlyZero(rep->anchor.x) || !FloatNearlyZero(rep->anchor.y)) {
      m = Matrix::Translate(-rep->anchor.x, -rep->anchor.y);
    }
    float sx = std::pow(rep->scale.x, prog);
    float sy = std::pow(rep->scale.y, prog);
    if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
      m = Matrix::Scale(sx, sy) * m;
    }
    float rot = rep->rotation * prog;
    if (!FloatNearlyZero(rot)) {
      m = Matrix::Rotate(rot) * m;
    }
    float px = rep->position.x * prog;
    float py = rep->position.y * prog;
    if (!FloatNearlyZero(px) || !FloatNearlyZero(py)) {
      m = Matrix::Translate(px, py) * m;
    }
    if (!FloatNearlyZero(rep->anchor.x) || !FloatNearlyZero(rep->anchor.y)) {
      m = Matrix::Translate(rep->anchor.x, rep->anchor.y) * m;
    }
    // Prepend the parent layer div's shift so the copy still paints at the layer origin
    // inside the expanded div (see HTMLWriterContext::repeaterOriginOffsetX/Y).
    if (!FloatNearlyZero(originOffsetX) || !FloatNearlyZero(originOffsetY)) {
      m = Matrix::Translate(-originOffsetX, -originOffsetY) * m;
    }
    // Match tgfx Repeater::apply: alphaT = progress / maxCount, where maxCount == ceil(copies).
    // Using copies-1 as the denominator forces the last rendered copy to land exactly on
    // endAlpha, but tgfx lets only the asymptote (i == maxCount) reach endAlpha, so the final
    // copy at i = maxCount-1 sits at startAlpha + (endAlpha - startAlpha) * (N-1)/N.
    // When the enclosing layer's Fill/Stroke was declared *after* the Repeater in PAGX order,
    // tgfx applies copyAlpha onto an empty painter list (painters don't exist yet) and the
    // later Fill/Stroke attaches to every clone with alpha=1.0. Honour that by skipping the
    // per-copy opacity in that case (showcase_mandala Middle Ring: Stroke sits after the
    // Repeater, so every petal must render at full alpha; otherwise petals appear to fade
    // with the `endAlpha=0.5` ramp, producing colours that don't match PAGX native).
    float ca = 1.0f;
    if (applyCopyAlphaDecay) {
      float denom = std::max(std::ceil(rep->copies), 1.0f);
      float np = std::clamp((static_cast<float>(idx) + rep->offset) / denom, 0.0f, 1.0f);
      ca = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * np;
      if (idx == n - 1 && frac > 0 && frac < 1.0f) {
        ca *= frac;
      }
    } else if (idx == n - 1 && frac > 0 && frac < 1.0f) {
      // Fractional last copy still contributes a partial coverage even when painters attach
      // after the Repeater: tgfx's final-copy fractional bookkeeping is independent of the
      // start/end alpha ramp.
      ca = frac;
    }
    float ea = ca * alpha;
    if (!m.isIdentity() || ea < 1.0f) {
      out.openTag("div");
      std::string s = "position:absolute";
      if (!m.isIdentity()) {
        s += ";transform:" + MatrixToCSS(m);
        s += ";transform-origin:0 0";
      }
      if (ea < 1.0f) {
        s += ";opacity:" + FloatToString(ea);
      }
      out.addAttr("style", s);
      out.closeTagStart();
      renderGeo(out, geos, fill, stroke, 1.0f, false, trim, false);
      out.closeTag();
    } else {
      renderGeo(out, geos, fill, stroke, 1.0f, false, trim, false);
    }
  }
}

void HTMLWriter::writeComposition(HTMLBuilder& out, const Composition* comp, float alpha,
                                  bool distribute) {
  if (_ctx->visitedCompositions.count(comp)) {
    return;
  }
  _ctx->visitedCompositions.insert(comp);
  bool needContainer = comp->width > 0 && comp->height > 0;
  if (needContainer) {
    out.openTag("div");
    out.addAttr("style", "position:relative;width:" + FloatToString(comp->width) +
                             "px;height:" + FloatToString(comp->height) + "px");
    out.closeTagStart();
  }
  for (auto* layer : comp->layers) {
    writeLayer(out, layer, alpha, distribute);
  }
  if (needContainer) {
    out.closeTag();
  }
  _ctx->visitedCompositions.erase(comp);
}

}  // namespace pagx
