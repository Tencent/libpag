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

#include "pagx/html/importer/HTMLAnimationBuilder.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/runtime/KeyframeEvaluator.h"
#include "pagx/svg/SVGPathParser.h"

namespace pagx {

using namespace pagx::html;

namespace {

constexpr float kFrameRate = 60.0f;

// Parsed `animation` shorthand / longhand declaration for a single element.
struct AnimationSpec {
  std::string name = {};
  float durationSeconds = 0.0f;
  float delaySeconds = 0.0f;
  std::string timingFunction = "linear";
  bool iterationInfinite = false;
  float iterationCount = 1.0f;
  std::string direction = "normal";
  // CSS `animation-fill-mode`: none (default) | forwards | backwards | both. Controls whether the
  // first and/or last keyframe values are "filled" outside the active duration window. None means
  // the element shows its non-animated baseline value before the delay ends and after the
  // animation finishes.
  std::string fillMode = "none";
};

// One resolved CSS animation timing-function. Exactly one shape is active per resolve, selected
// by `kind`. Bezier control points are valid only when `kind == Bezier`; `steps` is valid only
// when `kind == Steps`.
struct ResolvedEasing {
  enum class Kind : uint8_t { Linear, Bezier, Steps };

  // CSS `steps(n, <jump-term>)`. Each variant moves the discontinuity within the [0,1] segment
  // differently; see https://drafts.csswg.org/css-easing-1/#step-easing-functions for the
  // canonical semantics.
  enum class Jump : uint8_t {
    End,    // steps(n) / steps(n, jump-end) / steps(n, end). Default per CSS.
    Start,  // steps(n, jump-start) / steps(n, start).
    None,   // steps(n, jump-none). Requires n >= 2; reaches both endpoints.
    Both,   // steps(n, jump-both). Skips both endpoints.
  };

  Kind kind = Kind::Linear;
  // Bezier control points (Kind::Bezier).
  float x1 = 0.0f;
  float y1 = 0.0f;
  float x2 = 0.0f;
  float y2 = 0.0f;
  // Steps shape (Kind::Steps).
  int stepCount = 1;
  Jump stepJump = Jump::End;
};

// Trimmed style-map lookup helper. Keeps the call sites tidy when many longhands need the same
// `find + trim or empty` dance.
std::string GetTrimmed(const std::unordered_map<std::string, std::string>& style, const char* key) {
  auto it = style.find(key);
  return it == style.end() ? std::string() : Trim(it->second);
}

bool ParseTimeSeconds(const std::string& token, float& outSeconds) {
  std::string t = Trim(token);
  if (t.empty()) return false;
  char* end = nullptr;
  float v = std::strtof(t.c_str(), &end);
  if (end == t.c_str()) return false;
  std::string unit = ToLower(Trim(end));
  if (unit == "ms") {
    outSeconds = v / 1000.0f;
    return true;
  }
  if (unit == "s") {
    outSeconds = v;
    return true;
  }
  return false;  // unitless or unknown: not a time token
}

bool IsDirectionKeyword(const std::string& lc) {
  return lc == "normal" || lc == "reverse" || lc == "alternate" || lc == "alternate-reverse";
}

bool IsFillModeKeyword(const std::string& lc) {
  return lc == "none" || lc == "forwards" || lc == "backwards" || lc == "both";
}

bool IsTimingKeyword(const std::string& lc) {
  return lc == "linear" || lc == "ease" || lc == "ease-in" || lc == "ease-out" ||
         lc == "ease-in-out" || lc == "step-start" || lc == "step-end" ||
         lc.rfind("cubic-bezier(", 0) == 0 || lc.rfind("steps(", 0) == 0;
}

// Reverses a resolved easing in place so it describes the same shape the browser would apply
// when `animation-direction: reverse` (or `alternate-reverse`) flips the keyframes. CSS reverses
// the easing along with the keyframe order — `B'(t) = 1 - B(1 - t)` — which for a cubic-bezier
// maps `(x1, y1, x2, y2)` to `(1 - x2, 1 - y2, 1 - x1, 1 - y1)` (e.g. ease-in -> ease-out), and
// for steps swaps Start and End jumps while leaving None / Both (already symmetric) unchanged.
// Linear is its own reverse.
void ReverseEasing(ResolvedEasing& easing) {
  if (easing.kind == ResolvedEasing::Kind::Bezier) {
    float nx1 = 1.0f - easing.x2;
    float ny1 = 1.0f - easing.y2;
    float nx2 = 1.0f - easing.x1;
    float ny2 = 1.0f - easing.y1;
    easing.x1 = nx1;
    easing.y1 = ny1;
    easing.x2 = nx2;
    easing.y2 = ny2;
  } else if (easing.kind == ResolvedEasing::Kind::Steps) {
    if (easing.stepJump == ResolvedEasing::Jump::Start) {
      easing.stepJump = ResolvedEasing::Jump::End;
    } else if (easing.stepJump == ResolvedEasing::Jump::End) {
      easing.stepJump = ResolvedEasing::Jump::Start;
    }
  }
}

// Resolves a CSS timing-function string to an easing descriptor. Unknown / malformed values
// fall back to Linear.
ResolvedEasing ResolveTimingFunction(const std::string& value) {
  ResolvedEasing e = {};
  std::string lc = ToLower(Trim(value));
  if (lc.empty() || lc == "linear") {
    return e;  // Linear
  }
  // CSS `step-start` and `step-end` are exact aliases for `steps(1, jump-start)` and
  // `steps(1, jump-end)` respectively, so funnel them through the steps path.
  if (lc == "step-start") {
    e.kind = ResolvedEasing::Kind::Steps;
    e.stepCount = 1;
    e.stepJump = ResolvedEasing::Jump::Start;
    return e;
  }
  if (lc == "step-end") {
    e.kind = ResolvedEasing::Kind::Steps;
    e.stepCount = 1;
    e.stepJump = ResolvedEasing::Jump::End;
    return e;
  }
  if (lc == "ease") {
    e.kind = ResolvedEasing::Kind::Bezier;
    e.x1 = 0.25f;
    e.y1 = 0.1f;
    e.x2 = 0.25f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-in") {
    e.kind = ResolvedEasing::Kind::Bezier;
    e.x1 = 0.42f;
    e.y1 = 0.0f;
    e.x2 = 1.0f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-out") {
    e.kind = ResolvedEasing::Kind::Bezier;
    e.x1 = 0.0f;
    e.y1 = 0.0f;
    e.x2 = 0.58f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-in-out") {
    e.kind = ResolvedEasing::Kind::Bezier;
    e.x1 = 0.42f;
    e.y1 = 0.0f;
    e.x2 = 0.58f;
    e.y2 = 1.0f;
    return e;
  }
  std::string fn;
  std::string args;
  if (ParseCssFunctionCall(lc, fn, args)) {
    if (fn == "cubic-bezier") {
      auto parts = SplitTopLevelCommas(args);
      if (parts.size() == 4) {
        e.kind = ResolvedEasing::Kind::Bezier;
        e.x1 = std::strtof(Trim(parts[0]).c_str(), nullptr);
        e.y1 = std::strtof(Trim(parts[1]).c_str(), nullptr);
        e.x2 = std::strtof(Trim(parts[2]).c_str(), nullptr);
        e.y2 = std::strtof(Trim(parts[3]).c_str(), nullptr);
        return e;
      }
    } else if (fn == "steps") {
      auto parts = SplitTopLevelCommas(args);
      if (!parts.empty()) {
        char* end = nullptr;
        long n = std::strtol(Trim(parts[0]).c_str(), &end, 10);
        if (end != parts[0].c_str() && n >= 1) {
          ResolvedEasing::Jump jump = ResolvedEasing::Jump::End;  // CSS default
          if (parts.size() >= 2) {
            std::string mode = ToLower(Trim(parts[1]));
            if (mode == "jump-end" || mode == "end") {
              jump = ResolvedEasing::Jump::End;
            } else if (mode == "jump-start" || mode == "start") {
              jump = ResolvedEasing::Jump::Start;
            } else if (mode == "jump-none") {
              jump = ResolvedEasing::Jump::None;
            } else if (mode == "jump-both") {
              jump = ResolvedEasing::Jump::Both;
            }
          }
          // `jump-none` is undefined for n == 1; fall back to `end` to keep producing a sane
          // shape rather than dropping the timing entirely.
          if (jump == ResolvedEasing::Jump::None && n < 2) {
            jump = ResolvedEasing::Jump::End;
          }
          e.kind = ResolvedEasing::Kind::Steps;
          e.stepCount = static_cast<int>(n);
          e.stepJump = jump;
          return e;
        }
      }
    }
  }
  return e;  // default Linear
}

// Parses the `animation` shorthand (first comma-separated entry only) into `spec`. Tokens are
// classified by shape: <time> -> duration then delay; timing keyword/function -> timing;
// `infinite`/number -> iteration; direction keyword -> direction; otherwise the keyframes name.
void ParseAnimationShorthand(const std::string& value, AnimationSpec& spec, bool& multiple) {
  auto entries = SplitTopLevelCommas(value);
  multiple = entries.size() > 1;
  if (entries.empty()) return;
  auto tokens = SplitTopLevelWhitespace(entries[0]);
  int timeIndex = 0;
  for (auto& tok : tokens) {
    std::string lc = ToLower(tok);
    float seconds = 0.0f;
    if (ParseTimeSeconds(tok, seconds)) {
      if (timeIndex == 0) {
        spec.durationSeconds = seconds;
      } else if (timeIndex == 1) {
        spec.delaySeconds = seconds;
      }
      timeIndex++;
      continue;
    }
    if (IsTimingKeyword(lc)) {
      spec.timingFunction = lc;
      continue;
    }
    if (lc == "infinite") {
      spec.iterationInfinite = true;
      continue;
    }
    if (IsDirectionKeyword(lc)) {
      spec.direction = lc;
      continue;
    }
    if (IsFillModeKeyword(lc)) {
      spec.fillMode = lc;
      continue;
    }
    // `animation-play-state` (running / paused) is not part of the animation shorthand grammar, but
    // getComputedStyle serialises the `animation` property with it appended (e.g.
    // `1s linear … both running pagxAnim0`). Inline-SVG shapes are frozen from computed style, so
    // skip it here — otherwise `running` would be mistaken for the (name-last) keyframes name.
    if (lc == "running" || lc == "paused") {
      continue;
    }
    // A bare number is the iteration-count. Numeric values override any earlier `infinite`
    // keyword in the same shorthand declaration (CSS shorthand parsing is order-dependent and
    // the numeric token assigns iteration-count, which is mutually exclusive with infinite).
    char* end = nullptr;
    float num = std::strtof(tok.c_str(), &end);
    if (end != tok.c_str() && Trim(end).empty()) {
      spec.iterationCount = num;
      spec.iterationInfinite = false;
      continue;
    }
    // `none`/`initial` keep the default empty name; anything else is the keyframes name.
    if (lc != "none" && lc != "initial" && spec.name.empty()) {
      spec.name = tok;
    }
  }
}

// Applies longhand `animation-*` declarations on top of `spec` (longhands win over the shorthand
// for any property they specify, matching CSS cascade for individual declarations).
void ApplyAnimationLonghands(const std::unordered_map<std::string, std::string>& style,
                             AnimationSpec& spec) {
  std::string name = GetTrimmed(style, "animation-name");
  if (!name.empty() && ToLower(name) != "none") spec.name = name;
  std::string dur = GetTrimmed(style, "animation-duration");
  float seconds = 0.0f;
  if (!dur.empty() && ParseTimeSeconds(dur, seconds)) spec.durationSeconds = seconds;
  std::string delay = GetTrimmed(style, "animation-delay");
  if (!delay.empty() && ParseTimeSeconds(delay, seconds)) spec.delaySeconds = seconds;
  std::string timing = GetTrimmed(style, "animation-timing-function");
  if (!timing.empty()) spec.timingFunction = ToLower(timing);
  std::string iter = GetTrimmed(style, "animation-iteration-count");
  if (!iter.empty()) {
    if (ToLower(iter) == "infinite") {
      spec.iterationInfinite = true;
    } else {
      char* end = nullptr;
      float n = std::strtof(iter.c_str(), &end);
      if (end != iter.c_str()) {
        spec.iterationCount = n;
        spec.iterationInfinite = false;
      }
    }
  }
  std::string dir = GetTrimmed(style, "animation-direction");
  if (!dir.empty()) spec.direction = ToLower(dir);
  std::string fillMode = GetTrimmed(style, "animation-fill-mode");
  if (!fillMode.empty()) {
    std::string lc = ToLower(fillMode);
    if (IsFillModeKeyword(lc)) spec.fillMode = lc;
  }
}

constexpr float kPi = 3.14159265358979323846f;

// Parses a single finite float from a token (whitespace tolerated). Returns true and writes the
// result on success; on failure (non-numeric, NaN, infinity) leaves the output untouched. Used to
// validate matrix/scale arguments before they are folded into a transform matrix — without this
// guard, `strtof`'s 0-on-failure behavior would silently produce degenerate matrices for malformed
// transforms.
bool ParseFiniteFloat(const std::string& token, float& outValue) {
  std::string t = Trim(token);
  const char* begin = t.c_str();
  char* end = nullptr;
  float v = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(v)) return false;
  outValue = v;
  return true;
}

// Parses a CSS `transform` value — a single function or a space-separated compound chain — into a
// 2D affine `Matrix`. The chain is multiplied left-to-right (`A B C` -> A * B * C), matching CSS
// application order. `pureTranslate` is set true only when every function in the chain is a
// translation (`translate[X|Y]`, or a `matrix(...)` whose linear part is identity), letting the
// caller route the animation through the cheaper layout-preserving `x`/`y` channels. Returns false
// when the value contains a function outside the animatable 2D subset (matrix3d / rotate3d /
// perspective / unparseable), so the caller drops the keyframe transform with a diagnostic.
//
// `transform-origin` is NOT folded in here — the matrix is the raw CSS value, mirroring
// `HTMLTransform::matrix`; the caller pivots it via `T(cx,cy) * m * T(-cx,-cy)` exactly like
// `HTMLLayerBuilder::applyBoxTransform` does for the static path.
bool ParseTransformMatrix(const std::string& value, HTMLValueParser& parser, Matrix& outMatrix,
                          bool& pureTranslate) {
  outMatrix = Matrix::Identity();
  pureTranslate = true;
  std::string trimmed = Trim(value);
  std::string lc = ToLower(trimmed);
  if (trimmed.empty() || lc == "none") {
    return true;  // identity == translate(0, 0)
  }
  auto funcs = SplitTopLevelWhitespace(trimmed);
  if (funcs.empty()) return false;
  bool any = false;
  for (const auto& fnCall : funcs) {
    std::string fn;
    std::string args;
    if (!ParseCssFunctionCall(Trim(fnCall), fn, args)) return false;
    auto parts = SplitTopLevelCommas(args);
    Matrix m = Matrix::Identity();
    bool isTranslate = false;
    if (fn == "translate" && (parts.size() == 1 || parts.size() == 2)) {
      float tx = parser.parseAbsoluteLengthPx(parts[0]);
      if (std::isnan(tx)) return false;
      float ty = 0.0f;
      if (parts.size() == 2) {
        ty = parser.parseAbsoluteLengthPx(parts[1]);
        if (std::isnan(ty)) return false;
      }
      m = Matrix::Translate(tx, ty);
      isTranslate = true;
    } else if (fn == "translatex" && parts.size() == 1) {
      float tx = parser.parseAbsoluteLengthPx(parts[0]);
      if (std::isnan(tx)) return false;
      m = Matrix::Translate(tx, 0.0f);
      isTranslate = true;
    } else if (fn == "translatey" && parts.size() == 1) {
      float ty = parser.parseAbsoluteLengthPx(parts[0]);
      if (std::isnan(ty)) return false;
      m = Matrix::Translate(0.0f, ty);
      isTranslate = true;
    } else if (fn == "scale" && (parts.size() == 1 || parts.size() == 2)) {
      float sx = 0.0f;
      if (!ParseFiniteFloat(parts[0], sx)) return false;
      float sy = sx;
      if (parts.size() == 2 && !ParseFiniteFloat(parts[1], sy)) return false;
      m = Matrix::Scale(sx, sy);
    } else if (fn == "scalex" && parts.size() == 1) {
      float sx = 0.0f;
      if (!ParseFiniteFloat(parts[0], sx)) return false;
      m = Matrix::Scale(sx, 1.0f);
    } else if (fn == "scaley" && parts.size() == 1) {
      float sy = 0.0f;
      if (!ParseFiniteFloat(parts[0], sy)) return false;
      m = Matrix::Scale(1.0f, sy);
    } else if (fn == "rotate" && parts.size() == 1) {
      m = Matrix::Rotate(ParseAngle(parts[0]));
    } else if (fn == "skewx" && parts.size() == 1) {
      // CSS skewX(α) == [1 0 tan(α) 1 0 0]; the x-shear lives in `c`.
      m.c = std::tan(ParseAngle(parts[0]) * kPi / 180.0f);
    } else if (fn == "skewy" && parts.size() == 1) {
      // CSS skewY(α) == [1 tan(α) 0 1 0 0]; the y-shear lives in `b`.
      m.b = std::tan(ParseAngle(parts[0]) * kPi / 180.0f);
    } else if (fn == "skew" && (parts.size() == 1 || parts.size() == 2)) {
      m.c = std::tan(ParseAngle(parts[0]) * kPi / 180.0f);
      if (parts.size() == 2) {
        m.b = std::tan(ParseAngle(parts[1]) * kPi / 180.0f);
      }
    } else if (fn == "matrix" && parts.size() == 6) {
      float v[6];
      for (int i = 0; i < 6; i++) {
        if (!ParseFiniteFloat(parts[i], v[i])) return false;
      }
      m.a = v[0];
      m.b = v[1];
      m.c = v[2];
      m.d = v[3];
      m.tx = v[4];
      m.ty = v[5];
      isTranslate = (v[0] == 1.0f && v[1] == 0.0f && v[2] == 0.0f && v[3] == 1.0f);
    } else {
      return false;  // matrix3d / rotate3d / perspective / unknown -> drop
    }
    if (!isTranslate) pureTranslate = false;
    outMatrix = outMatrix * m;
    any = true;
  }
  return any;
}

// Resolves a single CSS `transform-origin` component (one axis) to absolute pixels. Handles px /
// keyword (left/top = 0, right/bottom = dim, center = dim/2) / percentage forms. Returns NaN when
// the dimension is unknown (so a percentage / keyword can't be resolved) or the token is unparsed.
float ResolveOriginComponent(const std::string& token, float dim, HTMLValueParser& parser) {
  std::string t = Trim(token);
  std::string lc = ToLower(t);
  if (lc == "center") return std::isnan(dim) ? NAN : dim * 0.5f;
  if (lc == "left" || lc == "top") return 0.0f;
  if (lc == "right" || lc == "bottom") return dim;
  if (!t.empty() && t.back() == '%') {
    char* end = nullptr;
    float p = std::strtof(t.c_str(), &end);
    if (end == t.c_str()) return NAN;
    return std::isnan(dim) ? NAN : dim * p / 100.0f;
  }
  return parser.parseAbsoluteLengthPx(t);
}

// Resolves the transform-origin pivot (cx, cy) in the element's local box space, mirroring
// `HTMLLayerBuilder::applyBoxTransform` so animated transforms pivot exactly like the static path.
// Prefers an explicit `transform-origin`; falls back to the box centre from the resolved (or
// layer-authored) width/height. Returns false when neither a usable origin nor box dimensions are
// known, in which case the caller pivots at the top-left and warns.
bool ResolvePivot(const std::unordered_map<std::string, std::string>& style, const Layer* layer,
                  HTMLValueParser& parser, float& cx, float& cy) {
  float w = parser.parseAbsoluteLengthPx(GetTrimmed(style, "width"));
  float h = parser.parseAbsoluteLengthPx(GetTrimmed(style, "height"));
  if (std::isnan(w) && layer != nullptr) w = layer->width;
  if (std::isnan(h) && layer != nullptr) h = layer->height;

  std::string origin = ToLower(GetTrimmed(style, "transform-origin"));
  bool originIsCentre =
      origin.empty() || origin == "center" || origin == "center center" || origin == "50% 50%";
  if (!originIsCentre) {
    auto tokens = SplitTopLevelWhitespace(origin);
    if (tokens.size() == 2) {
      float ox = ResolveOriginComponent(tokens[0], w, parser);
      float oy = ResolveOriginComponent(tokens[1], h, parser);
      if (!std::isnan(ox) && !std::isnan(oy)) {
        cx = ox;
        cy = oy;
        return true;
      }
    }
  }
  if (!std::isnan(w) && !std::isnan(h)) {
    cx = w * 0.5f;
    cy = h * 0.5f;
    return true;
  }
  return false;
}

// Ordering predicate for keyframe stops by ascending offset.
bool KeyframeStopLess(const CssKeyframeStop& a, const CssKeyframeStop& b) {
  return a.percent < b.percent;
}

// Writes bezier control handles between consecutive keyframes of a channel. No-op unless the
// channel uses bezier interpolation.
template <typename T>
void ApplyBezierHandles(std::vector<Keyframe<T>>& keys, KeyframeInterpolationType interp, float x1,
                        float y1, float x2, float y2) {
  if (interp != KeyframeInterpolationType::Bezier) return;
  for (size_t i = 0; i + 1 < keys.size(); i++) {
    keys[i].bezierOut = Point{x1, y1};
    keys[i + 1].bezierIn = Point{x2, y2};
  }
}

// Component-wise lerp for the channels the builder emits is provided by the runtime
// `LerpKeyframeValue` template (KeyframeEvaluator.h). Color goes through `LerpColor`, which
// handles per-channel clamping for back-style overshoot easings; reusing that template keeps
// importer-time and runtime semantics identical.

// Returns the [0, 1] output fraction for step `s` (0-indexed) of an n-step easing under the
// given jump mode. The CSS easing-1 spec defines four jump variants that differ only in which
// of the n+1 candidate output positions are used:
//   jump-end:   { 0/n, 1/n, ..., (n-1)/n }   (default; "no jump at start")
//   jump-start: { 1/n, 2/n, ..., n/n }       ("no jump at end")
//   jump-none:  { 0/(n-1), 1/(n-1), ..., 1 } (n distinct values, n-1 jumps; needs n>=2)
//   jump-both:  { 1/(n+1), 2/(n+1), ..., n/(n+1) } (jumps at both ends)
float StepValueFraction(int s, int n, ResolvedEasing::Jump jump) {
  switch (jump) {
    case ResolvedEasing::Jump::End:
      return static_cast<float>(s) / static_cast<float>(n);
    case ResolvedEasing::Jump::Start:
      return static_cast<float>(s + 1) / static_cast<float>(n);
    case ResolvedEasing::Jump::None:
      return static_cast<float>(s) / static_cast<float>(n - 1);
    case ResolvedEasing::Jump::Both:
      return static_cast<float>(s + 1) / static_cast<float>(n + 1);
  }
  return 0.0f;
}

// Expands every adjacent-keyframe segment in `keys` into n hold keyframes that reproduce a CSS
// `steps(n, jump)` timing function. The original endpoints' times are preserved and the original
// last keyframe is written verbatim with Hold interpolation; intermediate keyframes are
// time-quantized to the runtime's integer Frame grid via std::llround. Trailing same-value
// duplicates are harmless to the runtime (a Hold whose neighbour shares its value is a no-op).
template <typename T>
void ExpandSteps(std::vector<Keyframe<T>>& keys, int n, ResolvedEasing::Jump jump) {
  if (keys.empty()) return;
  if (keys.size() < 2 || n < 1) {
    for (auto& k : keys) k.interpolation = KeyframeInterpolationType::Hold;
    return;
  }
  std::vector<Keyframe<T>> out;
  out.reserve(keys.size() + (keys.size() - 1) * static_cast<size_t>(n));
  for (size_t i = 0; i + 1 < keys.size(); i++) {
    Frame timeA = keys[i].time;
    Frame timeB = keys[i + 1].time;
    const T& valueA = keys[i].value;
    const T& valueB = keys[i + 1].value;
    long long dt = static_cast<long long>(timeB) - static_cast<long long>(timeA);
    if (dt <= 0) {
      Keyframe<T> k = keys[i];
      k.interpolation = KeyframeInterpolationType::Hold;
      out.push_back(k);
      continue;
    }
    for (int s = 0; s < n; s++) {
      float timeFrac = static_cast<float>(s) / static_cast<float>(n);
      float valueFrac = StepValueFraction(s, n, jump);
      Keyframe<T> k = {};
      k.time = static_cast<Frame>(timeA + std::llround(timeFrac * static_cast<float>(dt)));
      k.value = LerpKeyframeValue<T>(valueA, valueB, static_cast<double>(valueFrac));
      k.interpolation = KeyframeInterpolationType::Hold;
      out.push_back(k);
    }
  }
  Keyframe<T> last = keys.back();
  last.interpolation = KeyframeInterpolationType::Hold;
  out.push_back(last);
  keys = std::move(out);
}

// Applies CSS `animation-fill-mode` to a per-channel keyframe sequence by injecting baseline
// hold keyframes outside the active duration window. The runtime evaluator clamps to the first /
// last keyframe value when the position falls outside the keyframe range, which is wrong for
// fill-mode `none`/`backwards`/`forwards` whenever the animation does not start at t=0 or does
// not end at the keyframe's last value. Inserting a Hold keyframe carrying the layer's
// non-animated baseline value at the boundaries forces the runtime to display that baseline.
//
// Boundary mapping (activeStart/activeEnd are the first and last existing keyframe times after
// offsetting by delay; leading baseline is inserted when activeStart > 0):
//
//   fillMode | t < activeStart                     | t > activeEnd (Once only)
//   ---------|-------------------------------------|------------------------------------------
//   none     | insert Hold(baseline) at frame 0    | insert Hold(baseline) at activeEnd + 1
//   forwards | insert Hold(baseline) at frame 0    | leave alone (last keyframe already holds)
//   backwards| leave alone (extrapolates to first) | insert Hold(baseline) at activeEnd + 1
//   both     | leave alone                         | leave alone
//
// `loopOnce` is true only when the animation will not repeat — for Loop/PingPong, the post-active
// region is occupied by subsequent iterations so the trailing baseline must not be inserted.
//
// `activeEnd` is the last frame of the active duration window (delayFrames + durationFrames).
// Using the keyframe's own back time would shrink the playback window when @keyframes omits
// the 100% terminal stop (e.g. only 0% or 0%+50%): everything past the last authored keyframe
// would be clamped to the baseline instead of holding the last value across the rest of the
// active duration.
template <typename T>
void ApplyFillMode(std::vector<Keyframe<T>>& keys, const std::string& fillMode, const T& baseline,
                   bool loopOnce, Frame activeEnd) {
  if (keys.empty()) return;
  bool fillBackwards = (fillMode == "backwards" || fillMode == "both");
  bool fillForwards = (fillMode == "forwards" || fillMode == "both");
  // Pre-active baseline. CSS `none` and `forwards` both display the baseline before the animation
  // starts; `backwards` and `both` extrapolate the first keyframe value backwards instead. Skipped
  // when the first keyframe already lives at frame 0 (no gap to fill).
  if (keys.front().time > ZeroFrame && !fillBackwards) {
    Keyframe<T> hold = {};
    hold.time = ZeroFrame;
    hold.value = baseline;
    hold.interpolation = KeyframeInterpolationType::Hold;
    keys.insert(keys.begin(), hold);
  }
  // Post-active baseline. Only meaningful for finite (Once) playback: Loop/PingPong have no
  // "after" region. CSS `none` and `backwards` both revert to the baseline once the animation
  // finishes; `forwards` and `both` keep the last keyframe value. The previous-last keyframe must
  // be promoted to Hold so its segment toward the baseline does not interpolate visibly. The
  // baseline is anchored at `activeEnd + 1`; if the keyframes' last time is earlier than the
  // active end, the previous-last value is held across the gap up to that anchor.
  if (loopOnce && !fillForwards) {
    Frame lastKeyTime = keys.back().time;
    keys.back().interpolation = KeyframeInterpolationType::Hold;
    if (lastKeyTime < activeEnd) {
      Keyframe<T> hold = {};
      hold.time = activeEnd;
      hold.value = keys.back().value;
      hold.interpolation = KeyframeInterpolationType::Hold;
      keys.push_back(hold);
    }
    Keyframe<T> baselineHold = {};
    baselineHold.time = activeEnd + 1;
    baselineHold.value = baseline;
    baselineHold.interpolation = KeyframeInterpolationType::Hold;
    keys.push_back(baselineHold);
  }
}

// Returns the first SolidColor painted by a Fill in `layer`'s subtree, or nullptr. The element's
// background Fill may sit on the id'd wrapper itself or on a nested host (the importer's
// "outer background + inner padded" / margin-wrapper splits), so the search descends a bounded
// number of child levels. The wrapper's own contents are checked before descending.
//
// The text path keeps its Fill nested inside a Group container (e.g. a TextBox-emitted text
// host whose `<Text>+<Fill>` pair sits in `Group.elements` rather than directly in
// `Layer.contents`), so this also descends into Group/TextBox containers encountered while
// scanning `Layer.contents`. NodeType is used in lieu of dynamic_cast (banned project-wide).
//
// Layer-vs-Element nesting is bounded with separate caps so a deeply Group-nested text fill
// inside a margin-wrapped + transform-split layer does not silently exhaust a single shared
// counter (the caller would see a misleading "no solid fill" warning).
constexpr int kMaxLayerDepth = 4;
constexpr int kMaxElementDepth = 4;

SolidColor* FindSolidFillInElements(const std::vector<Element*>& elements, int elementDepth);

SolidColor* FindSolidFill(Layer* layer, int layerDepth = 0) {
  if (layer == nullptr || layerDepth > kMaxLayerDepth) return nullptr;
  if (auto* found = FindSolidFillInElements(layer->contents, 0)) {
    return found;
  }
  for (auto* child : layer->children) {
    if (auto* found = FindSolidFill(child, layerDepth + 1)) {
      return found;
    }
  }
  return nullptr;
}

SolidColor* FindSolidFillInElements(const std::vector<Element*>& elements, int elementDepth) {
  if (elementDepth > kMaxElementDepth) return nullptr;
  for (auto* element : elements) {
    if (element == nullptr) continue;
    NodeType type = element->nodeType();
    if (type == NodeType::Fill) {
      auto* fill = static_cast<Fill*>(element);
      if (fill->color != nullptr && fill->color->nodeType() == NodeType::SolidColor) {
        return static_cast<SolidColor*>(fill->color);
      }
      continue;
    }
    if (type == NodeType::Group || type == NodeType::TextBox) {
      auto* group = static_cast<Group*>(element);
      if (auto* found = FindSolidFillInElements(group->elements, elementDepth + 1)) {
        return found;
      }
    }
  }
  return nullptr;
}

// Splits `outer` into a layout-only outer plus a fully-visual `inner` so a CSS `transform`
// animation can target `inner` without disturbing the layout box. The runtime `x` / `y`
// channels overwrite the matrix translation absolutely, which would otherwise clobber the
// position layout has assigned to `outer` (e.g. a flex child's main-axis offset, or the
// padding origin of a constraint-laid-out child). By moving every visual / container attribute
// onto a child Layer that fills its parent (`percentWidth/Height = 100`, no positional
// constraints, `includeInLayout = false`), `inner.layoutBounds` is guaranteed to be (0, 0) so
// the CSS keyframe values can be written verbatim onto `inner.x` / `inner.y` without baking in
// any static layout offset.
//
// `outer` keeps the parent-facing slot (width/height/percent/positional-constraints/flex,
// `includeInLayout`) and the existing `id` used by alpha targets and external references.
// All other state — internal layout (layout/gap/padding/alignment/arrangement),
// transform / visibility / compositing (matrix/matrix3D/preserve3D/blendMode/visible/
// antiAlias/groupOpacity/passThroughBackground), content/styles/filters/children/composition,
// clip/mask state — is moved to `inner`. After the split, `outer.children` contains `inner`
// only. `alpha` stays on `outer` because the alpha channel still targets the original id.
Layer* SplitForTransformAnimation(Layer* outer, PAGXDocument* document,
                                  HTMLIdAllocator& idAllocator) {
  auto* inner = document->makeNode<Layer>();
  inner->id = idAllocator.generateUnique("anim");

  inner->layout = outer->layout;
  inner->gap = outer->gap;
  inner->padding = outer->padding;
  inner->alignment = outer->alignment;
  inner->arrangement = outer->arrangement;
  outer->layout = LayoutMode::None;
  outer->gap = 0.0f;
  outer->padding = {};
  outer->alignment = Alignment::Stretch;
  outer->arrangement = Arrangement::Start;

  inner->matrix = outer->matrix;
  inner->matrix3D = outer->matrix3D;
  inner->preserve3D = outer->preserve3D;
  inner->blendMode = outer->blendMode;
  inner->visible = outer->visible;
  inner->antiAlias = outer->antiAlias;
  inner->groupOpacity = outer->groupOpacity;
  inner->passThroughBackground = outer->passThroughBackground;
  outer->matrix = {};
  outer->matrix3D = {};
  outer->preserve3D = false;
  outer->blendMode = BlendMode::Normal;
  outer->antiAlias = true;
  outer->groupOpacity = true;
  outer->passThroughBackground = true;

  inner->scrollRect = outer->scrollRect;
  inner->hasScrollRect = outer->hasScrollRect;
  inner->clipToBounds = outer->clipToBounds;
  inner->mask = outer->mask;
  inner->maskType = outer->maskType;
  outer->scrollRect = {};
  outer->hasScrollRect = false;
  outer->clipToBounds = false;
  outer->mask = nullptr;
  outer->maskType = MaskType::Alpha;

  inner->composition = outer->composition;
  outer->composition = nullptr;

  inner->contents = std::move(outer->contents);
  inner->styles = std::move(outer->styles);
  inner->filters = std::move(outer->filters);
  inner->children = std::move(outer->children);
  outer->contents = {};
  outer->styles = {};
  outer->filters = {};
  outer->children = {};

  inner->percentWidth = 100.0f;
  inner->percentHeight = 100.0f;
  inner->includeInLayout = false;

  outer->children.push_back(inner);
  return inner;
}

// Extracts the quoted `d` payload from an animated `clip-path: path("...")` keyframe value. The
// capture pipeline normalizes every clip-path shape (inset/circle/ellipse/polygon/path) to this
// single canonical `path("<d>")` form in border-box pixels, so the importer only has to recognize
// `path(...)` here. An optional fill-rule prefix (`path(evenodd, "...")`) is skipped. Returns an
// empty string for any other form.
std::string ExtractClipPathD(const std::string& value) {
  std::string v = Trim(value);
  auto lp = v.find('(');
  if (lp == std::string::npos) return std::string();
  std::string fn = ToLower(Trim(v.substr(0, lp)));
  if (fn != "path") return std::string();
  auto rp = v.rfind(')');
  if (rp == std::string::npos || rp <= lp) return std::string();
  std::string inner = v.substr(lp + 1, rp - lp - 1);
  // Find the first quote (skips an optional `evenodd,` / `nonzero,` fill-rule prefix).
  auto q = inner.find_first_of("'\"");
  if (q == std::string::npos) return std::string();
  char quote = inner[q];
  auto end = inner.find(quote, q + 1);
  if (end == std::string::npos) return std::string();
  return Trim(inner.substr(q + 1, end - q - 1));
}

// Builds (or rebuilds) a contour mask on `clipTarget` whose geometry is a single filled Path with
// the given verb / point structure, and returns that Path so the caller can attach per-point
// animation channels to it. Reuses an existing mask layer (e.g. one synthesized for the element's
// settled-state clip) by replacing its contents, so an animated element ends up with exactly one
// mask whose Path point order matches the channel indices. A fresh white Fill makes the shape draw
// into the contour mask coverage.
Path* BuildContourMaskPath(Layer* clipTarget, const std::vector<PathVerb>& verbs,
                           const std::vector<Point>& points, PAGXDocument* document,
                           HTMLIdAllocator& idAllocator) {
  if (clipTarget == nullptr || document == nullptr || verbs.empty()) return nullptr;

  auto* pathData = document->makeNode<PathData>();
  size_t idx = 0;
  for (auto verb : verbs) {
    switch (verb) {
      case PathVerb::Move:
        if (idx < points.size()) pathData->moveTo(points[idx].x, points[idx].y);
        idx += 1;
        break;
      case PathVerb::Line:
        if (idx < points.size()) pathData->lineTo(points[idx].x, points[idx].y);
        idx += 1;
        break;
      case PathVerb::Quad:
        if (idx + 1 < points.size()) {
          pathData->quadTo(points[idx].x, points[idx].y, points[idx + 1].x, points[idx + 1].y);
        }
        idx += 2;
        break;
      case PathVerb::Cubic:
        if (idx + 2 < points.size()) {
          pathData->cubicTo(points[idx].x, points[idx].y, points[idx + 1].x, points[idx + 1].y,
                            points[idx + 2].x, points[idx + 2].y);
        }
        idx += 3;
        break;
      case PathVerb::Close:
        pathData->close();
        break;
    }
  }

  auto* path = document->makeNode<Path>();
  path->data = pathData;
  auto* fill = document->makeNode<Fill>();
  auto* solid = document->makeNode<SolidColor>();
  solid->color = {1.0f, 1.0f, 1.0f, 1.0f, ColorSpace::SRGB};
  fill->color = solid;

  Layer* maskLayer = clipTarget->mask;
  if (maskLayer == nullptr) {
    maskLayer = document->makeNode<Layer>();
    maskLayer->visible = false;
    maskLayer->includeInLayout = false;
    maskLayer->id = idAllocator.generateUnique("mask");
    clipTarget->children.push_back(maskLayer);
    clipTarget->mask = maskLayer;
  }
  clipTarget->maskType = MaskType::Contour;
  maskLayer->contents.clear();
  maskLayer->contents.push_back(path);
  maskLayer->contents.push_back(fill);
  return path;
}

}  // namespace

HTMLAnimationBuilder::HTMLAnimationBuilder(HTMLDiagnosticSink& sink, HTMLValueParser& valueParser,
                                           HTMLIdAllocator& idAllocator)
    : _diagnostics(sink), _valueParser(valueParser), _idAllocator(idAllocator) {
}

void HTMLAnimationBuilder::bindDocument(PAGXDocument* document) {
  _document = document;
}

void HTMLAnimationBuilder::setKeyframes(
    const std::unordered_map<std::string, html::CssKeyframesRule>* keyframes) {
  _keyframes = keyframes;
}

bool HTMLAnimationBuilder::buildForElement(
    const std::unordered_map<std::string, std::string>& resolvedStyle, Layer* layer) {
  if (_document == nullptr || layer == nullptr || _keyframes == nullptr) return false;

  // Gather the animation spec from the shorthand and/or longhands.
  AnimationSpec spec = {};
  bool multiple = false;
  auto shorthandIt = resolvedStyle.find("animation");
  bool hasShorthand = shorthandIt != resolvedStyle.end() && !Trim(shorthandIt->second).empty();
  bool hasLonghand = resolvedStyle.count("animation-name") > 0;
  if (!hasShorthand && !hasLonghand) return false;
  if (hasShorthand) {
    ParseAnimationShorthand(shorthandIt->second, spec, multiple);
  }
  ApplyAnimationLonghands(resolvedStyle, spec);
  if (multiple) {
    _diagnostics.warn(
        "html: only the first animation in a comma-separated 'animation' list is imported "
        "[subset:animation-multiple]");
  }
  // CSS allows a comma-separated list of timing functions to pair with the comma-separated
  // animation list; we only support a single animation per element, so a list-valued timing
  // function would otherwise be silently misparsed by ResolveTimingFunction. Detect a *top-level*
  // comma (commas inside `cubic-bezier(...)` / `steps(...)` arguments do not count) and emit a
  // diagnostic so authors aren't surprised that only the first easing is honoured.
  {
    int parenDepth = 0;
    size_t topComma = std::string::npos;
    for (size_t i = 0; i < spec.timingFunction.size(); i++) {
      char c = spec.timingFunction[i];
      if (c == '(') parenDepth++;
      else if (c == ')') {
        if (parenDepth > 0) parenDepth--;
      } else if (c == ',' && parenDepth == 0) {
        topComma = i;
        break;
      }
    }
    if (topComma != std::string::npos) {
      _diagnostics.warn(
          "html: only the first animation-timing-function in a comma-separated list is imported "
          "[subset:animation-multiple]");
      spec.timingFunction = Trim(spec.timingFunction.substr(0, topComma));
    }
  }
  if (spec.name.empty()) return false;
  if (spec.durationSeconds <= 0.0f) {
    _diagnostics.warn(
        "html: animation '" + spec.name +
        "' has no positive duration; skipped [subset:animation-unsupported-property]");
    return false;
  }

  auto kfIt = _keyframes->find(spec.name);
  if (kfIt == _keyframes->end()) {
    _diagnostics.warn("html: animation references unknown @keyframes '" + spec.name +
                      "'; dropped [subset:animation-unknown-keyframes]");
    return false;
  }

  // Sorted copy of the keyframe stops. `reverse` direction flips the offsets and the timing
  // function (CSS reverses the easing alongside the keyframes — e.g. ease-in becomes ease-out).
  // `alternate` keeps the forward easing on the first iteration; `alternate-reverse` starts in
  // reverse, so the reversal applies to the same set of direction values.
  std::vector<CssKeyframeStop> stops = kfIt->second.stops;
  bool reversed = (spec.direction == "reverse" || spec.direction == "alternate-reverse");
  if (reversed) {
    for (auto& s : stops) s.percent = 100.0f - s.percent;
  }
  // stable_sort preserves authoring order between stops that share an offset (CSS allows
  // duplicate `0%` / `100%` selectors), so the later declaration overrides the earlier one
  // exactly the same way the browser would resolve it.
  std::stable_sort(stops.begin(), stops.end(), KeyframeStopLess);

  // Timing -> per-segment easing (applied uniformly across the animation).
  ResolvedEasing easing = ResolveTimingFunction(spec.timingFunction);
  if (reversed) {
    ReverseEasing(easing);
  }
  KeyframeInterpolationType interp = KeyframeInterpolationType::Linear;
  if (easing.kind == ResolvedEasing::Kind::Steps) {
    // The per-segment shape is composed of n hold sub-keyframes. The keyframes initially carry
    // Hold so the per-segment composition is well defined before ExpandSteps fans them out.
    interp = KeyframeInterpolationType::Hold;
  } else if (easing.kind == ResolvedEasing::Kind::Bezier) {
    interp = KeyframeInterpolationType::Bezier;
  }

  long long durationFrames = std::llround(spec.durationSeconds * kFrameRate);
  long long delayFrames = 0;
  if (spec.delaySeconds < 0.0f) {
    _diagnostics.warn(
        "html: negative animation-delay is not supported; clamped to 0 "
        "[subset:animation-unsupported-property]");
  } else {
    delayFrames = std::llround(spec.delaySeconds * kFrameRate);
  }

  // Per-channel keyframe collection.
  std::vector<Keyframe<float>> alphaKeys;
  std::vector<Keyframe<float>> xKeys;
  std::vector<Keyframe<float>> yKeys;
  std::vector<Keyframe<Matrix>> matrixKeys;
  std::vector<Keyframe<Color>> colorKeys;
  bool warnedTransform = false;

  // Raw (un-pivoted) CSS transform matrix per keyframe, collected first so we can decide once
  // whether the whole animation is a pure translation (cheap layout-preserving x/y channels) or a
  // general affine (the `matrix` channel). `transformAllPureTranslate` stays true only while every
  // keyframe's transform is a translation.
  std::vector<std::pair<Frame, Matrix>> transformStops;
  bool transformAllPureTranslate = true;

  // Per-keyframe `filter` decomposition. A glow / shadow authored as
  // `filter: drop-shadow(...)` (and `none` at rest) is lowered onto the runtime's
  // animatable DropShadowFilter channels; a `blur(...)` onto BlurFilter. Each stop
  // folds the chain onto ONE representative drop-shadow (largest blur*alpha — the
  // dominant glow) plus one blur radius, mirroring the capture-side decomposition.
  // A stop with no drop-shadow (`none`) records `hasShadow=false` so the channel
  // ramps its alpha / blur from zero rather than snapping.
  struct FilterStop {
    Frame time = 0;
    bool sawFilter = false;
    bool hasShadow = false;
    float sx = 0.0f;
    float sy = 0.0f;
    float sblur = 0.0f;
    Color scolor = {};
    bool hasBlur = false;
    float blur = 0.0f;
  };
  std::vector<FilterStop> filterStops;

  // Per-keyframe clip-path geometry (animated contour mask). The capture pipeline normalizes every
  // clip-path shape to a canonical `path("d")` in border-box pixels; each stop stores that path's
  // flattened point list. All stops of one animation share the same verb / point structure (CSS
  // only interpolates matching shapes), so per-point float channels (`point{i}.x/.y`) line up
  // one-to-one across the timeline. `clipVerbs` captures the structure of the first stop for the
  // consistency check below.
  struct ClipStop {
    Frame time = 0;
    std::vector<Point> points;
  };
  std::vector<ClipStop> clipStops;
  std::vector<PathVerb> clipVerbs;
  size_t clipPointCount = 0;
  bool clipStructureConsistent = true;

  for (const auto& stop : stops) {
    Frame time = static_cast<Frame>(
        delayFrames + std::llround(stop.percent / 100.0f * static_cast<float>(durationFrames)));
    std::unordered_map<std::string, std::string> decls;
    ParseStyleString(stop.declarations, decls);
    for (const auto& kv : decls) {
      const std::string& prop = kv.first;
      const std::string& val = kv.second;
      if (prop == "opacity") {
        std::string trimmedVal = Trim(val);
        char* end = nullptr;
        float v = std::strtof(trimmedVal.c_str(), &end);
        if (end == trimmedVal.c_str()) continue;
        std::string unit = Trim(end);
        // CSS opacity accepts a unitless number or a percentage; reject everything else so a
        // typo like "0.5px" no longer slips through as 0.5.
        if (!unit.empty() && unit != "%") continue;
        if (unit == "%") v /= 100.0f;
        v = std::max(0.0f, std::min(1.0f, v));
        alphaKeys.push_back({time, v, interp, {}, {}});
      } else if (prop == "transform") {
        Matrix m = {};
        bool pureTranslate = false;
        if (ParseTransformMatrix(val, _valueParser, m, pureTranslate)) {
          transformStops.push_back({time, m});
          if (!pureTranslate) transformAllPureTranslate = false;
        } else if (!warnedTransform) {
          _diagnostics.warn("html: unsupported 'transform' value '" + Trim(val) +
                            "' in @keyframes; dropped [subset:animation-unsupported-property]");
          warnedTransform = true;
        }
      } else if (prop == "color" || prop == "background-color") {
        Color c = _valueParser.parseColor(val);
        colorKeys.push_back({time, c, interp, {}, {}});
      } else if (prop == "filter") {
        FilterStop fs;
        fs.time = time;
        fs.sawFilter = true;
        std::string trimmed = Trim(val);
        if (ToLower(trimmed) != "none" && !trimmed.empty()) {
          auto steps = _valueParser.parseFilterChain(val);
          float bestScore = -1.0f;
          for (const auto& st : steps) {
            if (st.kind == HTMLValueParser::FilterStep::Kind::DropShadow) {
              float score = st.shadow.blur * st.shadow.color.alpha;
              if (score >= bestScore) {
                bestScore = score;
                fs.hasShadow = true;
                fs.sx = st.shadow.offsetX;
                fs.sy = st.shadow.offsetY;
                fs.sblur = st.shadow.blur;
                fs.scolor = st.shadow.color;
              }
            } else if (st.kind == HTMLValueParser::FilterStep::Kind::Blur) {
              fs.hasBlur = true;
              fs.blur = std::max(fs.blur, st.blurX);
            }
          }
        }
        filterStops.push_back(fs);
      } else if (prop == "clip-path") {
        std::string d = ExtractClipPathD(val);
        if (d.empty()) {
          clipStructureConsistent = false;
        } else {
          PathData pd = PathDataFromSVGString(d);
          ClipStop cs;
          cs.time = time;
          cs.points.assign(pd.points().begin(), pd.points().end());
          if (clipStops.empty()) {
            clipVerbs.assign(pd.verbs().begin(), pd.verbs().end());
            clipPointCount = cs.points.size();
          } else if (cs.points.size() != clipPointCount) {
            clipStructureConsistent = false;
          }
          clipStops.push_back(std::move(cs));
        }
      } else {
        _diagnostics.warn("html: animated property '" + prop +
                          "' is not supported; dropped [subset:animation-unsupported-property]");
      }
    }
  }

  // Lower the collected transform matrices onto the playable channel(s). A pure-translation
  // animation keeps the existing `x`/`y` path (it preserves the layout-assigned position through
  // the inner-wrapper split below, and avoids matrix-decompose interpolation entirely). Any
  // scale / rotate / skew (or a non-translation `matrix(...)`) routes the *full* affine through a
  // single `matrix` channel, pivoted around the element's transform-origin exactly like the static
  // `HTMLLayerBuilder::applyBoxTransform` path so the animated and static transforms agree.
  if (!transformStops.empty()) {
    if (transformAllPureTranslate) {
      for (const auto& ts : transformStops) {
        xKeys.push_back({ts.first, ts.second.tx, interp, {}, {}});
        yKeys.push_back({ts.first, ts.second.ty, interp, {}, {}});
      }
    } else {
      float cx = 0.0f;
      float cy = 0.0f;
      bool hasPivot = ResolvePivot(resolvedStyle, layer, _valueParser, cx, cy);
      if (!hasPivot) {
        _diagnostics.warn(
            "html: transform animation without resolvable transform-origin; pivoting at top-left "
            "may differ from CSS [subset:animation-unsupported-property]");
      }
      Matrix toCenter = Matrix::Translate(cx, cy);
      Matrix fromCenter = Matrix::Translate(-cx, -cy);
      for (const auto& ts : transformStops) {
        Matrix m = ts.second;
        if (hasPivot) {
          m = toCenter * m * fromCenter;
        }
        matrixKeys.push_back({ts.first, m, interp, {}, {}});
      }
    }
  }

  // Apply bezier handles between consecutive keyframes for each channel.
  ApplyBezierHandles(alphaKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(xKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(yKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(matrixKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(colorKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);

  // CSS `steps()` / `step-start` / `step-end` are not a runtime interpolation type — they expand
  // into n hold keyframes per @keyframes segment so the runtime can replay the staircase
  // verbatim. Channels that received no values are no-ops here.
  if (easing.kind == ResolvedEasing::Kind::Steps) {
    ExpandSteps(alphaKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(xKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(yKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(matrixKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(colorKeys, easing.stepCount, easing.stepJump);
  }

  // Whether the filter channel carries any playable motion: at least one stop must actually name a
  // drop-shadow / blur (a timeline of only `none` varies nowhere and is skipped by the capture, but
  // guard here too so an all-`none` filter never mints an empty filter node).
  bool filterHasShadow = false;
  bool filterHasBlur = false;
  for (const auto& fs : filterStops) {
    if (fs.hasShadow) filterHasShadow = true;
    if (fs.hasBlur) filterHasBlur = true;
  }

  // An animated clip-path is usable only when every stop resolved to a compatible geometry (same
  // shape function → same verb / point structure). CSS itself refuses to interpolate mismatched
  // shapes, so a mismatch here means the author animated between incompatible forms; drop the clip
  // channel with a diagnostic rather than emit a corrupt morph.
  bool clipUsable = clipStructureConsistent && clipStops.size() >= 2 && clipPointCount > 0;
  if (!clipUsable && !clipStops.empty()) {
    _diagnostics.warn(
        "html: animated clip-path keyframes do not share a compatible shape; dropped "
        "[subset:animation-unsupported-property]");
  }

  if (alphaKeys.empty() && xKeys.empty() && yKeys.empty() && matrixKeys.empty() &&
      colorKeys.empty() && !filterHasShadow && !filterHasBlur && !clipUsable) {
    return false;
  }

  // Resolve the per-channel baseline values used by `animation-fill-mode` outside the active
  // duration window. The baseline is the element's pre-animation static value, which is what CSS
  // displays during the delay (when fill-mode is `none` or `forwards`) and after the animation
  // ends (when fill-mode is `none` or `backwards`).
  //   alpha: the layer's resolved opacity, already populated from CSS by the layer importer.
  //   x / y: zero — translate-animated layers route their channels onto an inner wrapper whose
  //          layoutBounds origin is (0, 0); the CSS keyframe values stack on top of that.
  //   color: the SolidColor's current color, populated from CSS background-color/color before
  //          this method runs. The fill must be located before SplitForTransformAnimation moves
  //          contents to the inner wrapper, but the SolidColor pointer remains stable across the
  //          split and is reused later as the channel target.
  //   matrix: the layer's static transform (`layer->matrix`), which the renderer already seeds as
  //          the animatable baseline (LayerRuntimeTarget::initTransform). The CSS keyframe matrices
  //          override it during playback and fill-mode restores it outside the active window.
  float baselineAlpha = layer->alpha;
  float baselineXY = 0.0f;
  Matrix baselineMatrix = layer->matrix;
  Color baselineColor = {};
  SolidColor* baselineSolid = nullptr;
  if (!colorKeys.empty()) {
    baselineSolid = FindSolidFill(layer);
    if (baselineSolid != nullptr) {
      baselineColor = baselineSolid->color;
    }
  }

  // CSS `animation-iteration-count: 0` (or any non-positive value) suppresses playback entirely.
  // Negative values are invalid per spec; reject them upfront so the rest of the pipeline does
  // not have to reason about empty playback windows.
  if (!spec.iterationInfinite && spec.iterationCount <= 0.0f) {
    _diagnostics.warn(
        "html: animation-iteration-count <= 0 suppresses playback; dropped "
        "[subset:animation-finite-count]");
    return false;
  }

  // Determine the loop mode upfront — fill-mode trailing baselines only apply to finite (Once)
  // playback; Loop and PingPong have no "after" region to fill. CSS `alternate` requires a
  // finite count to mean "play forward then reverse N-1 more times and stop"; the runtime has
  // only the tri-state Once/Loop/PingPong, so finite alternation is downgraded to Once with a
  // diagnostic. PingPong is reserved for genuinely infinite alternation.
  LoopMode loopMode = LoopMode::Once;
  bool isAlternate = (spec.direction == "alternate" || spec.direction == "alternate-reverse");
  if (isAlternate && spec.iterationInfinite) {
    loopMode = LoopMode::PingPong;
  } else if (spec.iterationInfinite) {
    loopMode = LoopMode::Loop;
  }
  bool loopOnce = (loopMode == LoopMode::Once);

  // Inject baseline hold keyframes per CSS `animation-fill-mode`. ApplyFillMode operates
  // independently on each channel and is a no-op for channels with no keyframes. The active end
  // anchor is the last frame of the active duration window — using `delay + duration` keeps the
  // last authored keyframe held across any gap between it and the duration boundary, which
  // matters when @keyframes omits a 100% terminal stop.
  Frame activeEnd = static_cast<Frame>(delayFrames + durationFrames);
  ApplyFillMode(alphaKeys, spec.fillMode, baselineAlpha, loopOnce, activeEnd);
  ApplyFillMode(xKeys, spec.fillMode, baselineXY, loopOnce, activeEnd);
  ApplyFillMode(yKeys, spec.fillMode, baselineXY, loopOnce, activeEnd);
  ApplyFillMode(matrixKeys, spec.fillMode, baselineMatrix, loopOnce, activeEnd);
  // Color channel fill-mode is conditional on having a target SolidColor; if the lookup failed
  // the channel is dropped below, so the fill-mode mutation would be wasted work and the
  // diagnostic message would be misleading.
  if (!colorKeys.empty() && baselineSolid != nullptr) {
    ApplyFillMode(colorKeys, spec.fillMode, baselineColor, loopOnce, activeEnd);
  }

  // Ensure the target layer has an id (mint one when the author supplied none).
  if (layer->id.empty()) {
    layer->id = _idAllocator.generateUnique("anim");
  }

  // Trailing baselines are placed one frame past the active end (lastActiveTime + 1), so the
  // animation duration must be extended by one frame for the runtime to actually sample them
  // (LoopMode::Once clamps currentTime to `animation->duration` once playback ends). Mirrors the
  // condition in ApplyFillMode that triggers the trailing-hold injection.
  bool needsTrailingBaseline =
      loopOnce && (spec.fillMode == "none" || spec.fillMode == "backwards");

  auto* animation = _document->makeNode<Animation>(layer->id + "_anim");
  animation->frameRate = kFrameRate;
  // Loop/PingPong: CSS animation-delay only delays the first iteration, so the looped duration
  // must not include delayFrames (the runtime would otherwise re-play the delay gap each cycle).
  // Once: include delayFrames to keep the baseline visible during the gap, and extend by one
  // frame when fill-mode requires a trailing baseline sample (LoopMode::Once clamps currentTime
  // to `animation->duration` once playback ends, so the trailing hold needs to live within it).
  if (loopOnce) {
    animation->duration =
        static_cast<Frame>(durationFrames + delayFrames + (needsTrailingBaseline ? 1 : 0));
  } else {
    animation->duration = static_cast<Frame>(durationFrames);
  }
  animation->loop = loopMode;
  if (!spec.iterationInfinite && spec.iterationCount != 1.0f) {
    _diagnostics.warn(
        "html: finite animation-iteration-count is not supported; played once "
        "[subset:animation-finite-count]");
  } else if (isAlternate && !spec.iterationInfinite) {
    _diagnostics.warn(
        "html: finite alternate animation-direction is not supported; played once "
        "[subset:animation-finite-count]");
  }

  // CSS `transform: translate(...)` is an offset on top of the layout-assigned position, but
  // the runtime `x` / `y` channels overwrite the matrix translation absolutely. To keep the
  // CSS semantics under any positioning mode (absolute, flex child, padding-anchored child),
  // wrap the layer once and route x/y onto a fully-visual inner layer whose layoutBounds are
  // always (0, 0). Done lazily so layers without transform animations stay flat.
  Layer* xyTarget = nullptr;
  if (!xKeys.empty() || !yKeys.empty()) {
    xyTarget = SplitForTransformAnimation(layer, _document, _idAllocator);
  }

  // Layer-targeted alpha channel — stays on the original `layer` so the existing id keeps
  // identifying the alpha-animated element.
  if (!alphaKeys.empty()) {
    auto* object = _document->makeNode<AnimationObject>();
    object->target = layer->id;
    auto* ch = _document->makeNode<TypedChannel<float>>();
    ch->name = "alpha";
    ch->keyframes = std::move(alphaKeys);
    object->channels.push_back(ch);
    animation->objects.push_back(object);
  }

  // Layer-targeted x / y channels — routed to the inner wrapper when present so the CSS
  // translate values stack on top of the layout offset instead of overwriting it.
  if (!xKeys.empty() || !yKeys.empty()) {
    auto* object = _document->makeNode<AnimationObject>();
    object->target = xyTarget != nullptr ? xyTarget->id : layer->id;
    if (!xKeys.empty()) {
      auto* ch = _document->makeNode<TypedChannel<float>>();
      ch->name = "x";
      ch->keyframes = std::move(xKeys);
      object->channels.push_back(ch);
    }
    if (!yKeys.empty()) {
      auto* ch = _document->makeNode<TypedChannel<float>>();
      ch->name = "y";
      ch->keyframes = std::move(yKeys);
      object->channels.push_back(ch);
    }
    animation->objects.push_back(object);
  }

  // Layer-targeted `matrix` channel for scale / rotate / skew (and any non-translation transform).
  // Unlike x/y, the matrix channel animates the element's own transform (`Layer.matrix`) and not
  // its layout position, so no inner-wrapper split is needed: the renderer recomposes the final
  // transform as `Translate(layoutPosition) * matrix`, keeping the layout offset intact while the
  // keyframes drive scale/rotate/skew/translate about the baked transform-origin pivot.
  if (!matrixKeys.empty()) {
    auto* object = _document->makeNode<AnimationObject>();
    object->target = layer->id;
    auto* ch = _document->makeNode<TypedChannel<Matrix>>();
    ch->name = "matrix";
    ch->keyframes = std::move(matrixKeys);
    object->channels.push_back(ch);
    animation->objects.push_back(object);
  }

  // SolidColor-targeted channel (color / background-color). The fill was located before
  // SplitForTransformAnimation moved contents into the inner wrapper; the SolidColor pointer
  // remains stable across the split so the cached `baselineSolid` is reused as the channel
  // target without re-searching the (now-empty) outer layer.
  if (!colorKeys.empty()) {
    if (baselineSolid == nullptr) {
      _diagnostics.warn(
          "html: 'color' animation requires a solid background-color/color fill; dropped "
          "[subset:animation-unsupported-property]");
    } else {
      if (baselineSolid->id.empty()) {
        baselineSolid->id = _idAllocator.generateUnique("anim");
      }
      auto* object = _document->makeNode<AnimationObject>();
      object->target = baselineSolid->id;
      auto* ch = _document->makeNode<TypedChannel<Color>>();
      ch->name = "color";
      ch->keyframes = std::move(colorKeys);
      object->channels.push_back(ch);
      animation->objects.push_back(object);
    }
  }

  // Filter channels (glow / shadow / blur). The runtime animates the parameters of a
  // DropShadowFilter (offsetX/offsetY/blurX/blurY/color) and a BlurFilter (blurX/blurY) in place, so
  // a `filter: drop-shadow(...)` / `blur(...)` keyframe animation lowers onto those channels. The
  // filter node lives on whichever layer holds the visual content — the inner wrapper when an x/y
  // split happened, otherwise the layer itself (a scale/matrix animation does not split). An
  // existing static filter node is reused as the animation target; otherwise a zero/transparent
  // baseline node is minted so the channel has something to drive and fill-mode can restore "off".
  if (filterHasShadow || filterHasBlur) {
    Layer* fxLayer = xyTarget != nullptr ? xyTarget : layer;

    if (filterHasShadow) {
      // Representative RGB for `none` stops: the most opaque authored shadow, so a
      // none <-> glow transition ramps alpha on a stable hue instead of lerping toward black.
      Color repColor = {};
      float bestAlpha = -1.0f;
      for (const auto& fs : filterStops) {
        if (fs.hasShadow && fs.scolor.alpha > bestAlpha) {
          bestAlpha = fs.scolor.alpha;
          repColor = fs.scolor;
        }
      }

      DropShadowFilter* ds = nullptr;
      for (auto* f : fxLayer->filters) {
        if (f != nullptr && f->nodeType() == NodeType::DropShadowFilter) {
          ds = static_cast<DropShadowFilter*>(f);
          break;
        }
      }
      if (ds == nullptr) {
        ds = _document->makeNode<DropShadowFilter>();
        ds->offsetX = 0.0f;
        ds->offsetY = 0.0f;
        ds->blurX = 0.0f;
        ds->blurY = 0.0f;
        ds->color = {repColor.red, repColor.green, repColor.blue, 0.0f, repColor.colorSpace};
        fxLayer->filters.push_back(ds);
      }
      if (ds->id.empty()) {
        ds->id = _idAllocator.generateUnique("anim");
      }

      std::vector<Keyframe<float>> dsOffsetX;
      std::vector<Keyframe<float>> dsOffsetY;
      std::vector<Keyframe<float>> dsBlurX;
      std::vector<Keyframe<float>> dsBlurY;
      std::vector<Keyframe<Color>> dsColor;
      for (const auto& fs : filterStops) {
        float ox = fs.hasShadow ? fs.sx : 0.0f;
        float oy = fs.hasShadow ? fs.sy : 0.0f;
        float bl = fs.hasShadow ? fs.sblur : 0.0f;
        Color c = fs.hasShadow
                      ? fs.scolor
                      : Color{repColor.red, repColor.green, repColor.blue, 0.0f, repColor.colorSpace};
        dsOffsetX.push_back({fs.time, ox, interp, {}, {}});
        dsOffsetY.push_back({fs.time, oy, interp, {}, {}});
        dsBlurX.push_back({fs.time, bl, interp, {}, {}});
        dsBlurY.push_back({fs.time, bl, interp, {}, {}});
        dsColor.push_back({fs.time, c, interp, {}, {}});
      }
      ApplyBezierHandles(dsOffsetX, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      ApplyBezierHandles(dsOffsetY, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      ApplyBezierHandles(dsBlurX, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      ApplyBezierHandles(dsBlurY, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      ApplyBezierHandles(dsColor, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      if (easing.kind == ResolvedEasing::Kind::Steps) {
        ExpandSteps(dsOffsetX, easing.stepCount, easing.stepJump);
        ExpandSteps(dsOffsetY, easing.stepCount, easing.stepJump);
        ExpandSteps(dsBlurX, easing.stepCount, easing.stepJump);
        ExpandSteps(dsBlurY, easing.stepCount, easing.stepJump);
        ExpandSteps(dsColor, easing.stepCount, easing.stepJump);
      }
      ApplyFillMode(dsOffsetX, spec.fillMode, ds->offsetX, loopOnce, activeEnd);
      ApplyFillMode(dsOffsetY, spec.fillMode, ds->offsetY, loopOnce, activeEnd);
      ApplyFillMode(dsBlurX, spec.fillMode, ds->blurX, loopOnce, activeEnd);
      ApplyFillMode(dsBlurY, spec.fillMode, ds->blurY, loopOnce, activeEnd);
      ApplyFillMode(dsColor, spec.fillMode, ds->color, loopOnce, activeEnd);

      auto* object = _document->makeNode<AnimationObject>();
      object->target = ds->id;
      auto addFloat = [&](const char* name, std::vector<Keyframe<float>>& keys) {
        auto* ch = _document->makeNode<TypedChannel<float>>();
        ch->name = name;
        ch->keyframes = std::move(keys);
        object->channels.push_back(ch);
      };
      addFloat("offsetX", dsOffsetX);
      addFloat("offsetY", dsOffsetY);
      addFloat("blurX", dsBlurX);
      addFloat("blurY", dsBlurY);
      auto* colorCh = _document->makeNode<TypedChannel<Color>>();
      colorCh->name = "color";
      colorCh->keyframes = std::move(dsColor);
      object->channels.push_back(colorCh);
      animation->objects.push_back(object);
    }

    if (filterHasBlur) {
      BlurFilter* bf = nullptr;
      for (auto* f : fxLayer->filters) {
        if (f != nullptr && f->nodeType() == NodeType::BlurFilter) {
          bf = static_cast<BlurFilter*>(f);
          break;
        }
      }
      if (bf == nullptr) {
        bf = _document->makeNode<BlurFilter>();
        bf->blurX = 0.0f;
        bf->blurY = 0.0f;
        fxLayer->filters.push_back(bf);
      }
      if (bf->id.empty()) {
        bf->id = _idAllocator.generateUnique("anim");
      }

      std::vector<Keyframe<float>> bfBlurX;
      std::vector<Keyframe<float>> bfBlurY;
      for (const auto& fs : filterStops) {
        float bl = fs.hasBlur ? fs.blur : 0.0f;
        bfBlurX.push_back({fs.time, bl, interp, {}, {}});
        bfBlurY.push_back({fs.time, bl, interp, {}, {}});
      }
      ApplyBezierHandles(bfBlurX, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      ApplyBezierHandles(bfBlurY, interp, easing.x1, easing.y1, easing.x2, easing.y2);
      if (easing.kind == ResolvedEasing::Kind::Steps) {
        ExpandSteps(bfBlurX, easing.stepCount, easing.stepJump);
        ExpandSteps(bfBlurY, easing.stepCount, easing.stepJump);
      }
      ApplyFillMode(bfBlurX, spec.fillMode, bf->blurX, loopOnce, activeEnd);
      ApplyFillMode(bfBlurY, spec.fillMode, bf->blurY, loopOnce, activeEnd);

      auto* object = _document->makeNode<AnimationObject>();
      object->target = bf->id;
      auto* chX = _document->makeNode<TypedChannel<float>>();
      chX->name = "blurX";
      chX->keyframes = std::move(bfBlurX);
      object->channels.push_back(chX);
      auto* chY = _document->makeNode<TypedChannel<float>>();
      chY->name = "blurY";
      chY->keyframes = std::move(bfBlurY);
      object->channels.push_back(chY);
      animation->objects.push_back(object);
    }
  }

  // Animated clip-path (contour mask morph). The mask geometry is driven by per-point float
  // channels on a Path node: the renderer's PathRuntimeTarget interprets "point{i}.x/.y" and
  // rebuilds the tgfx path each frame. A mask must therefore be a single Path whose PathData point
  // order matches the channel indices, so we (re)build the contour mask here from the base (first)
  // keyframe, replacing whatever static geometry the settled-state clip synthesis produced. This
  // runs after the transform split so the mask lands on the layer that actually holds the content.
  if (clipUsable) {
    Layer* clipTarget = (xyTarget != nullptr) ? xyTarget : layer;

    // Base geometry = first stop; the mask's static Path shows this when no animation is active.
    Path* maskPath =
        BuildContourMaskPath(clipTarget, clipVerbs, clipStops.front().points, _document,
                             _idAllocator);
    if (maskPath != nullptr) {
      if (maskPath->id.empty()) {
        maskPath->id = _idAllocator.generateUnique("clip");
      }
      auto* object = _document->makeNode<AnimationObject>();
      object->target = maskPath->id;
      for (size_t i = 0; i < clipPointCount; i++) {
        const Point& base = clipStops.front().points[i];
        std::vector<Keyframe<float>> xKeysPt;
        std::vector<Keyframe<float>> yKeysPt;
        bool xVary = false;
        bool yVary = false;
        for (const auto& cs : clipStops) {
          if (i >= cs.points.size()) continue;
          float x = cs.points[i].x;
          float y = cs.points[i].y;
          xKeysPt.push_back({cs.time, x, interp, {}, {}});
          yKeysPt.push_back({cs.time, y, interp, {}, {}});
          if (std::abs(x - base.x) > 1e-3f) xVary = true;
          if (std::abs(y - base.y) > 1e-3f) yVary = true;
        }
        ApplyBezierHandles(xKeysPt, interp, easing.x1, easing.y1, easing.x2, easing.y2);
        ApplyBezierHandles(yKeysPt, interp, easing.x1, easing.y1, easing.x2, easing.y2);
        if (easing.kind == ResolvedEasing::Kind::Steps) {
          ExpandSteps(xKeysPt, easing.stepCount, easing.stepJump);
          ExpandSteps(yKeysPt, easing.stepCount, easing.stepJump);
        }
        ApplyFillMode(xKeysPt, spec.fillMode, base.x, loopOnce, activeEnd);
        ApplyFillMode(yKeysPt, spec.fillMode, base.y, loopOnce, activeEnd);
        if (xVary) {
          auto* ch = _document->makeNode<TypedChannel<float>>();
          ch->name = "point" + std::to_string(i) + ".x";
          ch->keyframes = std::move(xKeysPt);
          object->channels.push_back(ch);
        }
        if (yVary) {
          auto* ch = _document->makeNode<TypedChannel<float>>();
          ch->name = "point" + std::to_string(i) + ".y";
          ch->keyframes = std::move(yKeysPt);
          object->channels.push_back(ch);
        }
      }
      if (!object->channels.empty()) {
        animation->objects.push_back(object);
      }
    }
  }

  if (animation->objects.empty()) {
    return false;
  }
  _document->animations.push_back(animation);
  return true;
}

bool HTMLAnimationBuilder::buildForInlineSvgShape(
    const std::unordered_map<std::string, std::string>& style, const std::string& fillTargetId,
    const std::string& strokeTargetId, float dashScale) {
  if (_document == nullptr || _keyframes == nullptr) return false;
  auto shorthandIt = style.find("animation");
  bool hasShorthand = shorthandIt != style.end() && !Trim(shorthandIt->second).empty();
  bool hasLonghand = style.count("animation-name") > 0;
  if (!hasShorthand && !hasLonghand) return false;

  // Each comma-separated entry in the `animation` shorthand is an independent animation with its
  // own duration / delay / timing (the line-draw + fill-in idiom lists two: `hbDraw …, hbFill …`).
  // Build one `Animation` per entry. Longhands only apply to a single-animation declaration.
  std::vector<std::string> entries;
  if (hasShorthand) {
    entries = SplitTopLevelCommas(shorthandIt->second);
  }
  if (entries.empty()) {
    entries.push_back(std::string());  // longhand-only declaration
  }

  auto parseFloatVal = [](const std::string& v, float& out) -> bool {
    std::string t = Trim(v);
    char* end = nullptr;
    float f = std::strtof(t.c_str(), &end);
    if (end == t.c_str()) return false;
    out = f;
    return true;
  };

  bool builtAny = false;
  for (const auto& entry : entries) {
    AnimationSpec spec = {};
    bool dummyMultiple = false;
    if (!Trim(entry).empty()) {
      ParseAnimationShorthand(entry, spec, dummyMultiple);
    }
    if (entries.size() == 1) {
      ApplyAnimationLonghands(style, spec);
    }
    if (spec.name.empty() || spec.durationSeconds <= 0.0f) continue;

    auto kfIt = _keyframes->find(spec.name);
    if (kfIt == _keyframes->end()) {
      _diagnostics.warn("html: inline-svg animation references unknown @keyframes '" + spec.name +
                        "'; dropped [subset:animation-unknown-keyframes]");
      continue;
    }

    std::vector<CssKeyframeStop> stops = kfIt->second.stops;
    bool reversed = (spec.direction == "reverse" || spec.direction == "alternate-reverse");
    if (reversed) {
      for (auto& s : stops) s.percent = 100.0f - s.percent;
    }
    std::stable_sort(stops.begin(), stops.end(), KeyframeStopLess);

    ResolvedEasing easing = ResolveTimingFunction(spec.timingFunction);
    if (reversed) ReverseEasing(easing);
    KeyframeInterpolationType interp = KeyframeInterpolationType::Linear;
    if (easing.kind == ResolvedEasing::Kind::Steps) {
      interp = KeyframeInterpolationType::Hold;
    } else if (easing.kind == ResolvedEasing::Kind::Bezier) {
      interp = KeyframeInterpolationType::Bezier;
    }

    long long durationFrames = std::llround(spec.durationSeconds * kFrameRate);
    long long delayFrames =
        spec.delaySeconds > 0.0f ? std::llround(spec.delaySeconds * kFrameRate) : 0;

    std::vector<Keyframe<Color>> fillColorKeys;
    std::vector<Keyframe<float>> fillAlphaKeys;
    std::vector<Keyframe<Color>> strokeColorKeys;
    std::vector<Keyframe<float>> strokeAlphaKeys;
    std::vector<Keyframe<float>> dashKeys;

    for (const auto& stop : stops) {
      Frame time = static_cast<Frame>(
          delayFrames + std::llround(stop.percent / 100.0f * static_cast<float>(durationFrames)));
      std::unordered_map<std::string, std::string> decls;
      ParseStyleString(stop.declarations, decls);
      for (const auto& kv : decls) {
        const std::string& prop = kv.first;
        const std::string& val = kv.second;
        if (prop == "fill") {
          fillColorKeys.push_back({time, _valueParser.parseColor(val), interp, {}, {}});
        } else if (prop == "fill-opacity") {
          float f = 0.0f;
          if (parseFloatVal(val, f)) {
            fillAlphaKeys.push_back({time, std::max(0.0f, std::min(1.0f, f)), interp, {}, {}});
          }
        } else if (prop == "stroke") {
          strokeColorKeys.push_back({time, _valueParser.parseColor(val), interp, {}, {}});
        } else if (prop == "stroke-opacity") {
          float f = 0.0f;
          if (parseFloatVal(val, f)) {
            strokeAlphaKeys.push_back({time, std::max(0.0f, std::min(1.0f, f)), interp, {}, {}});
          }
        } else if (prop == "stroke-dashoffset") {
          // Author-space value (normalised by `pathLength`); rescale into user units exactly like
          // the static dash import (SVGImporter multiplies by realLength / authorLength).
          float f = 0.0f;
          if (parseFloatVal(val, f)) {
            dashKeys.push_back({time, f * dashScale, interp, {}, {}});
          }
        }
        // opacity / transform have no per-shape painter target and are ignored here.
      }
    }

    if (fillColorKeys.empty() && fillAlphaKeys.empty() && strokeColorKeys.empty() &&
        strokeAlphaKeys.empty() && dashKeys.empty()) {
      continue;
    }

    ApplyBezierHandles(fillColorKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
    ApplyBezierHandles(fillAlphaKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
    ApplyBezierHandles(strokeColorKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
    ApplyBezierHandles(strokeAlphaKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
    ApplyBezierHandles(dashKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
    if (easing.kind == ResolvedEasing::Kind::Steps) {
      ExpandSteps(fillColorKeys, easing.stepCount, easing.stepJump);
      ExpandSteps(fillAlphaKeys, easing.stepCount, easing.stepJump);
      ExpandSteps(strokeColorKeys, easing.stepCount, easing.stepJump);
      ExpandSteps(strokeAlphaKeys, easing.stepCount, easing.stepJump);
      ExpandSteps(dashKeys, easing.stepCount, easing.stepJump);
    }

    LoopMode loopMode = LoopMode::Once;
    bool isAlternate = (spec.direction == "alternate" || spec.direction == "alternate-reverse");
    if (isAlternate && spec.iterationInfinite) {
      loopMode = LoopMode::PingPong;
    } else if (spec.iterationInfinite) {
      loopMode = LoopMode::Loop;
    }
    bool loopOnce = (loopMode == LoopMode::Once);
    Frame activeEnd = static_cast<Frame>(delayFrames + durationFrames);

    // The shape's static painter value shows outside the active window. The capture bakes an
    // explicit 0% stop carrying the resting value, so the first keyframe is a faithful per-channel
    // baseline for `animation-fill-mode` forwards / backwards.
    if (!fillColorKeys.empty()) {
      ApplyFillMode(fillColorKeys, spec.fillMode, fillColorKeys.front().value, loopOnce, activeEnd);
    }
    if (!fillAlphaKeys.empty()) {
      ApplyFillMode(fillAlphaKeys, spec.fillMode, fillAlphaKeys.front().value, loopOnce, activeEnd);
    }
    if (!strokeColorKeys.empty()) {
      ApplyFillMode(strokeColorKeys, spec.fillMode, strokeColorKeys.front().value, loopOnce,
                    activeEnd);
    }
    if (!strokeAlphaKeys.empty()) {
      ApplyFillMode(strokeAlphaKeys, spec.fillMode, strokeAlphaKeys.front().value, loopOnce,
                    activeEnd);
    }
    if (!dashKeys.empty()) {
      ApplyFillMode(dashKeys, spec.fillMode, dashKeys.front().value, loopOnce, activeEnd);
    }

    bool needsTrailingBaseline =
        loopOnce && (spec.fillMode == "none" || spec.fillMode == "backwards");

    std::string animId = (strokeTargetId.empty() ? fillTargetId : strokeTargetId) + "_" +
                         spec.name + "_anim";
    auto* animation = _document->makeNode<Animation>(animId);
    animation->frameRate = kFrameRate;
    if (loopOnce) {
      animation->duration =
          static_cast<Frame>(durationFrames + delayFrames + (needsTrailingBaseline ? 1 : 0));
    } else {
      animation->duration = static_cast<Frame>(durationFrames);
    }
    animation->loop = loopMode;

    // Group channels by their target painter node (fill vs stroke).
    if (!fillColorKeys.empty() || !fillAlphaKeys.empty()) {
      if (fillTargetId.empty()) {
        _diagnostics.warn(
            "html: inline-svg 'fill' animation has no fill painter to target; dropped "
            "[subset:animation-unsupported-property]");
      } else {
        auto* object = _document->makeNode<AnimationObject>();
        object->target = fillTargetId;
        if (!fillColorKeys.empty()) {
          auto* ch = _document->makeNode<TypedChannel<Color>>();
          ch->name = "color";
          ch->keyframes = std::move(fillColorKeys);
          object->channels.push_back(ch);
        }
        if (!fillAlphaKeys.empty()) {
          auto* ch = _document->makeNode<TypedChannel<float>>();
          ch->name = "alpha";
          ch->keyframes = std::move(fillAlphaKeys);
          object->channels.push_back(ch);
        }
        animation->objects.push_back(object);
      }
    }
    if (!strokeColorKeys.empty() || !strokeAlphaKeys.empty() || !dashKeys.empty()) {
      if (strokeTargetId.empty()) {
        _diagnostics.warn(
            "html: inline-svg 'stroke' animation has no stroke painter to target; dropped "
            "[subset:animation-unsupported-property]");
      } else {
        auto* object = _document->makeNode<AnimationObject>();
        object->target = strokeTargetId;
        if (!strokeColorKeys.empty()) {
          auto* ch = _document->makeNode<TypedChannel<Color>>();
          ch->name = "color";
          ch->keyframes = std::move(strokeColorKeys);
          object->channels.push_back(ch);
        }
        if (!strokeAlphaKeys.empty()) {
          auto* ch = _document->makeNode<TypedChannel<float>>();
          ch->name = "alpha";
          ch->keyframes = std::move(strokeAlphaKeys);
          object->channels.push_back(ch);
        }
        if (!dashKeys.empty()) {
          auto* ch = _document->makeNode<TypedChannel<float>>();
          ch->name = "dashOffset";
          ch->keyframes = std::move(dashKeys);
          object->channels.push_back(ch);
        }
        animation->objects.push_back(object);
      }
    }

    if (animation->objects.empty()) continue;
    _document->animations.push_back(animation);
    builtAny = true;
  }
  return builtAny;
}

}  // namespace pagx
