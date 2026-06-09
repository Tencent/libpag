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
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Keyframe.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/SolidColor.h"

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

// Trimmed lookup helper (hoisted out of a lambda to honour the project's no-lambda rule).
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
    // A bare number is the iteration-count.
    char* end = nullptr;
    float num = std::strtof(tok.c_str(), &end);
    if (end != tok.c_str() && Trim(end).empty()) {
      spec.iterationCount = num;
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
      if (end != iter.c_str()) spec.iterationCount = n;
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
    float a = std::strtof(Trim(parts[0]).c_str(), nullptr);
    float b = std::strtof(Trim(parts[1]).c_str(), nullptr);
    float c = std::strtof(Trim(parts[2]).c_str(), nullptr);
    float d = std::strtof(Trim(parts[3]).c_str(), nullptr);
    // Only a pure translation matrix maps onto x/y; rotation/scale would need channels the
    // runtime does not have.
    if (a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f) {
      out.x = std::strtof(Trim(parts[4]).c_str(), nullptr);
      out.y = std::strtof(Trim(parts[5]).c_str(), nullptr);
      out.hasX = true;
      out.hasY = true;
      return out;
    }
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

// Component-wise linear interpolation helpers. Two specializations cover the channels the
// builder emits — float (alpha/x/y) and Color (background-color/color). Color components are
// blended in the source color space; CSS animations are author-defined in sRGB and the output
// channel preserves that, so a per-component lerp matches what a browser would render.
template <typename T>
T LerpKeyframeValue(const T& a, const T& b, float t);

template <>
float LerpKeyframeValue<float>(const float& a, const float& b, float t) {
  return a + (b - a) * t;
}

template <>
Color LerpKeyframeValue<Color>(const Color& a, const Color& b, float t) {
  Color out = a;
  out.red = a.red + (b.red - a.red) * t;
  out.green = a.green + (b.green - a.green) * t;
  out.blue = a.blue + (b.blue - a.blue) * t;
  out.alpha = a.alpha + (b.alpha - a.alpha) * t;
  return out;
}

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
      k.value = LerpKeyframeValue<T>(valueA, valueB, valueFrac);
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
template <typename T>
void ApplyFillMode(std::vector<Keyframe<T>>& keys, const std::string& fillMode, const T& baseline,
                   bool loopOnce) {
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
  // be promoted to Hold so its segment toward the baseline does not interpolate visibly.
  if (loopOnce && !fillForwards) {
    Frame lastTime = keys.back().time;
    keys.back().interpolation = KeyframeInterpolationType::Hold;
    Keyframe<T> hold = {};
    hold.time = lastTime + 1;
    hold.value = baseline;
    hold.interpolation = KeyframeInterpolationType::Hold;
    keys.push_back(hold);
  }
}

// Returns the first SolidColor painted by a Fill in `layer`'s subtree, or nullptr. The element's
// background Fill may sit on the id'd wrapper itself or on a nested host (the importer's
// "outer background + inner padded" / margin-wrapper splits), so the search descends a bounded
// number of child levels. The wrapper's own contents are checked before descending.
SolidColor* FindSolidFill(Layer* layer, int depth = 0) {
  if (layer == nullptr || depth > 3) return nullptr;
  for (auto* element : layer->contents) {
    if (element == nullptr || element->nodeType() != NodeType::Fill) continue;
    auto* fill = static_cast<Fill*>(element);
    if (fill->color != nullptr && fill->color->nodeType() == NodeType::SolidColor) {
      return static_cast<SolidColor*>(fill->color);
    }
  }
  for (auto* child : layer->children) {
    if (auto* found = FindSolidFill(child, depth + 1)) {
      return found;
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

  // Sorted copy of the keyframe stops. `reverse` direction flips the offsets.
  std::vector<CssKeyframeStop> stops = kfIt->second.stops;
  bool reversed = (spec.direction == "reverse" || spec.direction == "alternate-reverse");
  if (reversed) {
    for (auto& s : stops) s.percent = 100.0f - s.percent;
  }
  std::sort(stops.begin(), stops.end(), KeyframeStopLess);

  // Timing -> per-segment easing (applied uniformly across the animation).
  ResolvedEasing easing = ResolveTimingFunction(spec.timingFunction);
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

  // Determine the loop mode upfront — fill-mode trailing baselines only apply to finite (Once)
  // playback; Loop and PingPong have no "after" region to fill.
  LoopMode loopMode = LoopMode::Once;
  if (spec.direction == "alternate" || spec.direction == "alternate-reverse") {
    loopMode = LoopMode::PingPong;
  } else if (spec.iterationInfinite) {
    loopMode = LoopMode::Loop;
  }
  bool loopOnce = (loopMode == LoopMode::Once);

  // Inject baseline hold keyframes per CSS `animation-fill-mode`. ApplyFillMode operates
  // independently on each channel and is a no-op for channels with no keyframes.
  ApplyFillMode(alphaKeys, spec.fillMode, baselineAlpha, loopOnce);
  ApplyFillMode(xKeys, spec.fillMode, baselineXY, loopOnce);
  ApplyFillMode(yKeys, spec.fillMode, baselineXY, loopOnce);
  ApplyFillMode(colorKeys, spec.fillMode, baselineColor, loopOnce);

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
  animation->duration =
      static_cast<Frame>(durationFrames + delayFrames + (needsTrailingBaseline ? 1 : 0));
  animation->loop = loopMode;
  if (loopMode == LoopMode::Once && spec.iterationCount > 1.0f && !spec.iterationInfinite) {
    _diagnostics.warn(
        "html: finite animation-iteration-count is not supported; played once "
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
