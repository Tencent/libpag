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
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/runtime/KeyframeEvaluator.h"

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

// Decomposed translate component of a single-function CSS `transform` value.
struct TranslateDecomp {
  bool hasX = false;
  bool hasY = false;
  bool hasUnsupported = false;  // rotate/scale/skew/non-translate matrix
  float x = 0.0f;
  float y = 0.0f;
};

// Parses a single finite float from a token (whitespace tolerated). Returns true and writes the
// result on success; on failure (non-numeric, NaN, infinity) leaves the output untouched. Used to
// validate matrix/matrix3d arguments before their components are emitted as translation values
// — without this guard, `strtof`'s 0-on-failure behavior would silently produce (0, 0) translates
// for malformed transforms.
bool ParseFiniteFloat(const std::string& token, float& outValue) {
  std::string t = Trim(token);
  const char* begin = t.c_str();
  char* end = nullptr;
  float v = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(v)) return false;
  outValue = v;
  return true;
}

TranslateDecomp DecomposeTranslate(const std::string& transformValue, HTMLValueParser& parser) {
  TranslateDecomp out = {};
  std::string trimmed = Trim(transformValue);
  std::string lc = ToLower(trimmed);
  if (trimmed.empty() || lc == "none") return out;
  std::string fn;
  std::string args;
  if (!ParseCssFunctionCall(trimmed, fn, args)) {
    out.hasUnsupported = true;
    return out;
  }
  auto parts = SplitTopLevelCommas(args);
  if (fn == "translatex" && parts.size() == 1) {
    out.x = parser.parseAbsoluteLengthPx(parts[0]);
    out.hasX = !std::isnan(out.x);
    if (!out.hasX) out.x = 0.0f;
    return out;
  }
  if (fn == "translatey" && parts.size() == 1) {
    out.y = parser.parseAbsoluteLengthPx(parts[0]);
    out.hasY = !std::isnan(out.y);
    if (!out.hasY) out.y = 0.0f;
    return out;
  }
  if (fn == "translate" && (parts.size() == 1 || parts.size() == 2)) {
    out.x = parser.parseAbsoluteLengthPx(parts[0]);
    out.hasX = !std::isnan(out.x);
    if (!out.hasX) out.x = 0.0f;
    if (parts.size() == 2) {
      out.y = parser.parseAbsoluteLengthPx(parts[1]);
      out.hasY = !std::isnan(out.y);
      if (!out.hasY) out.y = 0.0f;
    }
    return out;
  }
  if (fn == "matrix" && parts.size() == 6) {
    // Parse the linear part (a, b, c, d) for an identity check; tx/ty (e, f) carry the
    // translation. `strtof` returns 0 with an unmoved end pointer for non-numeric input, which
    // would silently turn malformed `matrix(...)` values into a (0, 0) translation. Validate
    // that each component actually parsed and is finite before using its value, otherwise flag
    // the whole transform as unsupported and skip the channel.
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
    if (!ParseFiniteFloat(parts[0], a) || !ParseFiniteFloat(parts[1], b) ||
        !ParseFiniteFloat(parts[2], c) || !ParseFiniteFloat(parts[3], d) ||
        !ParseFiniteFloat(parts[4], out.x) || !ParseFiniteFloat(parts[5], out.y)) {
      out = {};
      out.hasUnsupported = true;
      return out;
    }
    out.hasX = true;
    out.hasY = true;
    if (!(a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f)) out.hasUnsupported = true;
    return out;
  }
  if (fn == "matrix3d" && parts.size() == 16) {
    // Column-major 4×4: indices 12,13 are the x/y translation. Everything else (scale/rotation/
    // skew/perspective and the unsupported tz at index 14) is dropped, mirroring the 2D matrix
    // handling above. Validate every component to avoid silently emitting (0, 0) translations
    // when the matrix3d args are malformed.
    bool pureTranslate = true;
    for (int i = 0; i < 16 && pureTranslate; i++) {
      if (i == 12 || i == 13) continue;
      float v = 0.0f;
      if (!ParseFiniteFloat(parts[i], v)) {
        out = {};
        out.hasUnsupported = true;
        return out;
      }
      float identity = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
      if (v != identity) pureTranslate = false;
    }
    if (!ParseFiniteFloat(parts[12], out.x) || !ParseFiniteFloat(parts[13], out.y)) {
      out = {};
      out.hasUnsupported = true;
      return out;
    }
    out.hasX = true;
    out.hasY = true;
    if (!pureTranslate) out.hasUnsupported = true;
    return out;
  }
  out.hasUnsupported = true;
  return out;
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
  // function would otherwise be silently misparsed by ResolveTimingFunction. Detect the comma
  // and emit a diagnostic so authors aren't surprised that only the first easing is honoured.
  if (spec.timingFunction.find(',') != std::string::npos) {
    _diagnostics.warn(
        "html: only the first animation-timing-function in a comma-separated list is imported "
        "[subset:animation-multiple]");
    auto commaPos = spec.timingFunction.find(',');
    spec.timingFunction = Trim(spec.timingFunction.substr(0, commaPos));
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
  std::vector<Keyframe<Color>> colorKeys;
  bool warnedTransform = false;

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
        TranslateDecomp t = DecomposeTranslate(val, _valueParser);
        if (t.hasUnsupported && !warnedTransform) {
          _diagnostics.warn(
              "html: only 'translate' is animatable in 'transform'; rotate/scale/skew dropped "
              "[subset:animation-unsupported-property]");
          warnedTransform = true;
        }
        if (t.hasX) xKeys.push_back({time, t.x, interp, {}, {}});
        if (t.hasY) yKeys.push_back({time, t.y, interp, {}, {}});
      } else if (prop == "color" || prop == "background-color") {
        Color c = _valueParser.parseColor(val);
        colorKeys.push_back({time, c, interp, {}, {}});
      } else {
        _diagnostics.warn("html: animated property '" + prop +
                          "' is not supported; dropped [subset:animation-unsupported-property]");
      }
    }
  }

  // Apply bezier handles between consecutive keyframes for each channel.
  ApplyBezierHandles(alphaKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(xKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(yKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);
  ApplyBezierHandles(colorKeys, interp, easing.x1, easing.y1, easing.x2, easing.y2);

  // CSS `steps()` / `step-start` / `step-end` are not a runtime interpolation type — they expand
  // into n hold keyframes per @keyframes segment so the runtime can replay the staircase
  // verbatim. Channels that received no values are no-ops here.
  if (easing.kind == ResolvedEasing::Kind::Steps) {
    ExpandSteps(alphaKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(xKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(yKeys, easing.stepCount, easing.stepJump);
    ExpandSteps(colorKeys, easing.stepCount, easing.stepJump);
  }

  if (alphaKeys.empty() && xKeys.empty() && yKeys.empty() && colorKeys.empty()) {
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
  float baselineAlpha = layer->alpha;
  float baselineXY = 0.0f;
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

  if (animation->objects.empty()) {
    return false;
  }
  _document->animations.push_back(animation);
  return true;
}

}  // namespace pagx
