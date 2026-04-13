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
#include <cmath>
#include <string>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
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
  srand(seed);
  std::vector<std::pair<int, size_t>> randList;
  randList.reserve(textCount);
  for (size_t i = 0; i < textCount; i++) {
    randList.push_back({rand(), i});
  }
  std::sort(randList.begin(), randList.end(),
            [](const std::pair<int, size_t>& a, const std::pair<int, size_t>& b) {
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

float ComputeRangeSelectorFactor(const RangeSelector* selector, size_t glyphIndex,
                                 size_t totalGlyphs) {
  if (totalGlyphs == 0) {
    return 0.0f;
  }
  float glyphPos = 0.0f;
  if (selector->unit == SelectorUnit::Percentage) {
    float denom = totalGlyphs > 1 ? static_cast<float>(totalGlyphs - 1) : 1.0f;
    glyphPos = static_cast<float>(glyphIndex) / denom;
  } else {
    glyphPos = static_cast<float>(glyphIndex);
  }
  float offset = selector->offset;
  if (selector->unit == SelectorUnit::Percentage) {
    glyphPos -= offset;
  } else {
    glyphPos -= offset;
  }
  if (selector->unit == SelectorUnit::Percentage) {
    if (glyphPos < 0.0f || glyphPos > 1.0f) {
      glyphPos = std::fmod(glyphPos, 1.0f);
      if (glyphPos < 0) {
        glyphPos += 1.0f;
      }
    }
  }
  if (selector->unit == SelectorUnit::Index) {
    glyphPos = glyphPos / static_cast<float>(totalGlyphs);
    glyphPos = std::fmod(glyphPos, 1.0f);
    if (glyphPos < 0) {
      glyphPos += 1.0f;
    }
  }
  float start = selector->start;
  float end = selector->end;
  if (selector->unit == SelectorUnit::Index) {
    start = start / static_cast<float>(totalGlyphs);
    end = end / static_cast<float>(totalGlyphs);
  }
  if (glyphPos < start || glyphPos > end) {
    return 0.0f;
  }
  float rangeSize = end - start;
  if (rangeSize <= 0) {
    return 1.0f;
  }
  float t = (glyphPos - start) / rangeSize;
  float rawInfluence = ApplySelectorShape(selector->shape, t);
  if (selector->easeIn > 0 && t < 0.5f) {
    float easeT = t * 2.0f;
    rawInfluence *= 1.0f - (1.0f - easeT) * selector->easeIn;
  }
  if (selector->easeOut > 0 && t > 0.5f) {
    float easeT = (t - 0.5f) * 2.0f;
    rawInfluence *= 1.0f - easeT * selector->easeOut;
  }
  return rawInfluence * selector->weight;
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
  std::string style;
  style.reserve(300);
  if (tb) {
    float tbW = tb->width;
    float tbH = tb->height;
    float tbLeft = !std::isnan(tbW) ? (tb->position.x - tbW * 0.5f) : tb->position.x;
    float tbTop = !std::isnan(tbH) ? (tb->position.y - tbH * 0.5f) : tb->position.y;
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
    float ty = text->position.y;
    if (text->baseline == TextBaseline::Alphabetic) {
      ty -= text->fontSize * 0.8f;
    }
    style += "position:absolute;left:" + FloatToString(text->position.x) +
             "px;top:" + FloatToString(ty) + "px;white-space:pre";
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
  if (!text->fontFamily.empty()) {
    std::string escapedFamily = text->fontFamily;
    for (size_t pos = 0; (pos = escapedFamily.find('\'', pos)) != std::string::npos; pos += 2) {
      escapedFamily.replace(pos, 1, "\\'");
    }
    style += ";font-family:'" + escapedFamily + "'";
  }
  style += ";font-size:" + FloatToString(text->fontSize) + "px";
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
  if (text->fauxBold && !stroke) {
    style += ";-webkit-text-stroke:0.02em currentColor";
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
  auto paths = ComputeGlyphPaths(*text, text->position.x, text->position.y);
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
    float textPosX = text->position.x;
    float textPosY = text->position.y;
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
      float ty = text->position.y;
      if (text->baseline == TextBaseline::Alphabetic) {
        ty -= text->fontSize * 0.8f;
      }
      std::string containerStyle =
          "position:absolute;white-space:nowrap;left:" + FloatToString(text->position.x) +
          "px;top:" + FloatToString(ty) + "px";
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
      if (text->letterSpacing != 0.0f) {
        containerStyle += ";letter-spacing:" + FloatToString(text->letterSpacing) + "px";
      }
      if (alpha < 1.0f) {
        containerStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("div");
      out.addAttr("style", containerStyle);
      out.closeTagStart();
      std::string fontSizeStr = FloatToString(text->fontSize) + "px";
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
          transform += "skewX(" + FloatToString(-modifier->skew * absF) + "deg) ";
        }
        if (!FloatNearlyZero(anchorX) || !FloatNearlyZero(anchorY)) {
          transform +=
              "translate(" + FloatToString(-anchorX) + "px," + FloatToString(-anchorY) + "px) ";
        }
        if (!transform.empty()) {
          charStyle += ";transform:" + transform;
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
  if (textPath->baselineAngle != 0) {
    float ox = textPath->baselineOrigin.x;
    float oy = textPath->baselineOrigin.y;
    auto m = Matrix::Translate(-ox, -oy);
    m = Matrix::Rotate(textPath->baselineAngle) * m;
    m = Matrix::Translate(ox, oy) * m;
    pathData.transform(m);
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
      float spacingScale =
          textPath->forceAlignment && totalAdvance > 0 ? effectiveLength / totalAdvance : 1.0f;
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
          float glyphAdvance = glyph->advance * scale * spacingScale;
          float glyphCenterArc = currentArcPos + glyphAdvance / 2.0f;
          Point pos = {};
          float tangent = 0;
          SampleArcLengthLUT(lut, glyphCenterArc, &pos, &tangent, isClosed);
          float yOffset = 0;
          if (i < run->positions.size()) {
            yOffset = run->positions[i].y;
          }
          float normalAngle = tangent + static_cast<float>(M_PI) / 2.0f;
          pos.x += yOffset * std::cos(normalAngle);
          pos.y += yOffset * std::sin(normalAngle);
          Matrix m = Matrix::Scale(scale, scale);
          m = Matrix::Translate(-glyph->advance / 2.0f, 0) * m;
          if (textPath->perpendicular) {
            float angleDeg = tangent * 180.0f / static_cast<float>(M_PI);
            m = Matrix::Rotate(angleDeg) * m;
          }
          m = Matrix::Translate(pos.x, pos.y) * m;
          out.openTag("path");
          out.addAttr("transform", MatrixToCSS(m));
          out.addAttr("d", PathDataToSVGString(*glyph->path));
          out.closeTagSelfClosing();
          currentArcPos += glyphAdvance;
        }
      }
      out.closeTag();  // </g>
      out.closeTag();  // </svg>
    } else {
      float totalWidth = 0.0f;
      const char* p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        totalWidth += EstimateCharAdvanceHTML(ch, text->fontSize) + text->letterSpacing;
        p += len;
      }
      float spacingScale =
          textPath->forceAlignment && totalWidth > 0 ? effectiveLength / totalWidth : 1.0f;
      float currentArcPos = textPath->firstMargin;
      p = text->text.c_str();
      while (*p) {
        int32_t ch = 0;
        size_t len = SVGDecodeUTF8Char(p, text->text.size() - (p - text->text.c_str()), &ch);
        if (len == 0) {
          break;
        }
        float charWidth =
            (EstimateCharAdvanceHTML(ch, text->fontSize) + text->letterSpacing) * spacingScale;
        float charCenterArc = currentArcPos + charWidth / 2.0f;
        Point pos = {};
        float tangent = 0;
        SampleArcLengthLUT(lut, charCenterArc, &pos, &tangent, isClosed);
        std::string charStr(p, len);
        float ascent = text->fontSize * 0.8f;
        float normalSign = textPath->reversed ? 1.0f : -1.0f;
        float normalAngle = tangent + static_cast<float>(M_PI) / 2.0f;
        float offsetX = normalSign * ascent * std::cos(normalAngle);
        float offsetY = normalSign * ascent * std::sin(normalAngle);
        std::string charStyle = "position:absolute;left:" + FloatToString(pos.x + offsetX) +
                                "px;top:" + FloatToString(pos.y + offsetY) + "px";
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
        charStyle += ";font-size:" + FloatToString(text->fontSize) + "px";
        std::string transform;
        transform += "translateX(-50%)";
        if (textPath->perpendicular) {
          float angleDeg = tangent * 180.0f / static_cast<float>(M_PI);
          transform += " rotate(" + FloatToString(angleDeg) + "deg)";
        }
        charStyle += ";transform:" + transform;
        charStyle += ";transform-origin:center center";
        if (fill && fill->color && fill->color->nodeType() == NodeType::SolidColor) {
          auto sc = static_cast<const SolidColor*>(fill->color);
          charStyle += ";color:" + ColorToRGBA(sc->color, fill->alpha);
        }
        if (alpha < 1.0f) {
          charStyle += ";opacity:" + FloatToString(alpha);
        }
        out.openTag("span");
        out.addAttr("style", charStyle);
        out.closeTagWithText(charStr);
        currentArcPos += charWidth;
        p += len;
      }
    }
  }
}

//==============================================================================
// HTMLWriter – trim & group/repeater/composition
//==============================================================================

void HTMLWriter::applyTrimAttrs(HTMLBuilder& builder, const TrimPath* trim, bool isEllipse) {
  if (!trim) {
    return;
  }
  if (trim->start == 0.0f && trim->end == 1.0f && FloatNearlyZero(trim->offset)) {
    return;
  }
  builder.addAttr("pathLength", "1");
  // PAGX ellipse starts at 12 o'clock CCW; SVG ellipse starts at 3 o'clock CW.
  // Offset = 0.75 accounts for both the starting point difference and direction reversal.
  float ellipseAdj = isEllipse ? 0.75f : 0.0f;
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
    builder.addAttr("stroke-dashoffset", FloatToString(-(s + ellipseAdj)));
  } else {
    float seg1 = 1.0f - s;
    float seg2 = (e > 1.0f) ? (e - 1.0f) : e;
    float gap = std::max(0.0f, 1.0f - seg1 - seg2);
    // Use 4-value dasharray to avoid SVG odd-count auto-duplication.
    builder.addAttr("stroke-dasharray", FloatToString(seg1) + " " + FloatToString(gap) + " " +
                                            FloatToString(seg2) + " 0");
    builder.addAttr("stroke-dashoffset", FloatToString(-(s + ellipseAdj)));
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
      float rn = std::min(r->roundness, std::min(r->size.width / 2, r->size.height / 2));
      float straightLen = 2 * (r->size.width + r->size.height - 4 * rn);
      float arcLen = 2 * static_cast<float>(M_PI) * rn;
      return straightLen + arcLen;
    }
    case NodeType::Ellipse: {
      auto e = static_cast<const Ellipse*>(geo.element);
      float rx = e->size.width / 2;
      float ry = e->size.height / 2;
      float h = ((rx - ry) * (rx - ry)) / ((rx + ry) * (rx + ry));
      return static_cast<float>(M_PI) * (rx + ry) * (1 + 3 * h / (10 + std::sqrt(4 - 3 * h)));
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

void HTMLWriter::applyTrimAttrsContinuous(HTMLBuilder& builder, const TrimPath* trim,
                                          const std::vector<float>& pathLengths, float totalLength,
                                          size_t geoIndex, bool isEllipse) {
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
    builder.addAttr("stroke-dasharray", "0");
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
    builder.addAttr("stroke-dasharray", "0");
    return;
  }

  builder.addAttr("pathLength", "1");
  // PAGX ellipse starts at 12 o'clock CCW; SVG ellipse starts at 3 o'clock CW.
  // Offset = 0.75 accounts for both the starting point difference and direction reversal.
  float ellipseAdj = isEllipse ? 0.75f : 0.0f;
  if (localEnd <= 1.0f) {
    float visible = localEnd - localStart;
    float gap = 1.0f - visible;
    builder.addAttr("stroke-dasharray", FloatToString(visible) + " " + FloatToString(gap));
    builder.addAttr("stroke-dashoffset", FloatToString(-(localStart + ellipseAdj)));
  } else {
    float seg1 = 1.0f - localStart;
    float seg2 = localEnd - 1.0f;
    float gap = 1.0f - seg1 - seg2;
    builder.addAttr("stroke-dasharray",
                    FloatToString(seg2) + " " + FloatToString(gap) + " " + FloatToString(seg1));
    if (!FloatNearlyZero(ellipseAdj)) {
      builder.addAttr("stroke-dashoffset", FloatToString(-ellipseAdj));
    }
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
                               const Stroke* stroke, float alpha, const TrimPath* trim) {
  if (rep->copies <= 0) {
    return;
  }
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
    float denom = std::max(std::ceil(rep->copies) - 1.0f, 1.0f);
    float np = std::clamp((static_cast<float>(idx) + rep->offset) / denom, 0.0f, 1.0f);
    float ca = rep->startAlpha + (rep->endAlpha - rep->startAlpha) * np;
    if (idx == n - 1 && frac > 0 && frac < 1.0f) {
      ca *= frac;
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
