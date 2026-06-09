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
};

// One resolved bezier easing (CSS cubic-bezier control points).
struct BezierEasing {
  bool isBezier = false;  // false => Linear or Hold (see isHold)
  bool isHold = false;
  float x1 = 0.0f;
  float y1 = 0.0f;
  float x2 = 0.0f;
  float y2 = 0.0f;
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

bool IsTimingKeyword(const std::string& lc) {
  return lc == "linear" || lc == "ease" || lc == "ease-in" || lc == "ease-out" ||
         lc == "ease-in-out" || lc == "step-start" || lc == "step-end" ||
         lc.rfind("cubic-bezier(", 0) == 0 || lc.rfind("steps(", 0) == 0;
}

// Resolves a CSS timing-function string to a bezier/hold easing descriptor.
BezierEasing ResolveTimingFunction(const std::string& value) {
  BezierEasing e = {};
  std::string lc = ToLower(Trim(value));
  if (lc.empty() || lc == "linear") {
    return e;  // Linear
  }
  if (lc == "step-start" || lc == "step-end" || lc.rfind("steps(", 0) == 0) {
    e.isHold = true;
    return e;
  }
  if (lc == "ease") {
    e.isBezier = true;
    e.x1 = 0.25f;
    e.y1 = 0.1f;
    e.x2 = 0.25f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-in") {
    e.isBezier = true;
    e.x1 = 0.42f;
    e.y1 = 0.0f;
    e.x2 = 1.0f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-out") {
    e.isBezier = true;
    e.x1 = 0.0f;
    e.y1 = 0.0f;
    e.x2 = 0.58f;
    e.y2 = 1.0f;
    return e;
  }
  if (lc == "ease-in-out") {
    e.isBezier = true;
    e.x1 = 0.42f;
    e.y1 = 0.0f;
    e.x2 = 0.58f;
    e.y2 = 1.0f;
    return e;
  }
  std::string fn;
  std::string args;
  if (ParseCssFunctionCall(lc, fn, args) && fn == "cubic-bezier") {
    auto parts = SplitTopLevelCommas(args);
    if (parts.size() == 4) {
      e.isBezier = true;
      e.x1 = std::strtof(Trim(parts[0]).c_str(), nullptr);
      e.y1 = std::strtof(Trim(parts[1]).c_str(), nullptr);
      e.x2 = std::strtof(Trim(parts[2]).c_str(), nullptr);
      e.y2 = std::strtof(Trim(parts[3]).c_str(), nullptr);
      return e;
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
  BezierEasing easing = ResolveTimingFunction(spec.timingFunction);
  KeyframeInterpolationType interp = KeyframeInterpolationType::Linear;
  if (easing.isHold) {
    interp = KeyframeInterpolationType::Hold;
  } else if (easing.isBezier) {
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

  if (alphaKeys.empty() && xKeys.empty() && yKeys.empty() && colorKeys.empty()) {
    return false;
  }

  // Ensure the target layer has an id (mint one when the author supplied none).
  if (layer->id.empty()) {
    layer->id = _idAllocator.generateUnique("anim");
  }

  auto* animation = _document->makeNode<Animation>(layer->id + "_anim");
  animation->frameRate = kFrameRate;
  animation->duration = static_cast<Frame>(durationFrames + delayFrames);
  if (spec.direction == "alternate" || spec.direction == "alternate-reverse") {
    animation->loop = LoopMode::PingPong;
  } else if (spec.iterationInfinite) {
    animation->loop = LoopMode::Loop;
  } else {
    if (spec.iterationCount > 1.0f) {
      _diagnostics.warn(
          "html: finite animation-iteration-count is not supported; played once "
          "[subset:animation-finite-count]");
    }
    animation->loop = LoopMode::Once;
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

  // SolidColor-targeted channel (color / background-color). The Fill carrying the SolidColor
  // moved to the inner wrapper when one was created, so search starts from there.
  if (!colorKeys.empty()) {
    Layer* fillHost = xyTarget != nullptr ? xyTarget : layer;
    SolidColor* solid = FindSolidFill(fillHost);
    if (solid == nullptr) {
      _diagnostics.warn(
          "html: 'color' animation requires a solid background-color/color fill; dropped "
          "[subset:animation-unsupported-property]");
    } else {
      if (solid->id.empty()) {
        solid->id = _idAllocator.generateUnique("anim");
      }
      auto* object = _document->makeNode<AnimationObject>();
      object->target = solid->id;
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
