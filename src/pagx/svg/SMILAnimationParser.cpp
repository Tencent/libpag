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

#include "pagx/svg/SMILAnimationParser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <unordered_set>
#include "pagx/DocumentAccess.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/svg/SVGParserContext.h"
#include "pagx/svg/SVGPathParser.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/PathMeasure.h"

namespace pagx {

// Bounds for values read from untrusted SVG. An unbounded duration, repeat count or rotation angle
// would otherwise expand into an unlimited number of keyframes.
static constexpr double kMaxDurationSeconds = 86400.0;
static constexpr double kMaxRepeatCount = 1000.0;
static constexpr int kMaxRotationSteps = 10000;
// Tiling an indefinite loop up to a far begin time would otherwise expand into millions of
// keyframes; stop well before that and let the tail hold the last value.
static constexpr size_t kMaxTiledKeyframes = 200000;

// Two keyframes on the same frame describe an instantaneous jump (opacity 1 -> 0 -> 1 within one
// frame). Marking the earlier key as Hold is not enough: tracks keyed by frame deduplicate by
// timestamp and drop the later key, leaving a constant track. Shifting the earlier key one frame
// back keeps both keys distinct in any implementation. Float property channels only (alpha and
// friends); same-frame jumps on matrix channels are not verified yet.
// Keeps keyframe times strictly increasing. A fractional repeat tail can round onto the previous
// period's last key, and a same-frame jump can leave several keys on one timestamp; tracks keyed by
// time would drop all but one of them.
template <typename T>
static void EnsureStrictlyIncreasingTimes(std::vector<Keyframe<T>>* keyframes) {
  if (keyframes == nullptr) {
    return;
  }
  for (size_t i = 1; i < keyframes->size(); ++i) {
    if ((*keyframes)[i].time <= (*keyframes)[i - 1].time) {
      (*keyframes)[i].time = (*keyframes)[i - 1].time + 1;
    }
  }
}

static void ShiftSameTimeFloatKeyframes(TypedChannel<float>* channel) {
  if (channel == nullptr || channel->keyframes.size() < 2) {
    return;
  }
  for (size_t i = 1; i < channel->keyframes.size(); ++i) {
    if (channel->keyframes[i].time == channel->keyframes[i - 1].time) {
      channel->keyframes[i - 1].interpolation = KeyframeInterpolationType::Hold;
      if (channel->keyframes[i - 1].time > 0) {
        channel->keyframes[i - 1].time -= 1;
      } else {
        channel->keyframes[i].time += 1;
      }
    }
  }
  EnsureStrictlyIncreasingTimes(&channel->keyframes);
}

static void SkipWhitespaceAndComma(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(static_cast<unsigned char>(*ptr)) != 0 || *ptr == ',')) {
    ++ptr;
  }
}

double SMILAnimationParser::parseSMILClockValue(const std::string& value) {
  if (value.empty()) {
    return 0.0;
  }
  if (value == "indefinite") {
    return -1.0;
  }

  // Timecolon form: HH:MM:SS.ss (hours optional) or MM:SS.ss.
  if (value.find(':') != std::string::npos) {
    int hours = 0;
    int minutes = 0;
    double seconds = 0.0;
    auto firstColon = value.find(':');
    auto secondColon = value.find(':', firstColon + 1);
    if (secondColon != std::string::npos) {
      hours = std::atoi(value.substr(0, firstColon).c_str());
      minutes = std::atoi(value.substr(firstColon + 1, secondColon - firstColon - 1).c_str());
      seconds = std::atof(value.substr(secondColon + 1).c_str());
    } else {
      minutes = std::atoi(value.substr(0, firstColon).c_str());
      seconds = std::atof(value.substr(firstColon + 1).c_str());
    }
    return hours * 3600.0 + minutes * 60.0 + seconds;
  }

  // Suffix form: numeric value followed by an optional unit.
  // Detect the longest matching unit suffix first so "ms" wins over "s".
  if (value.size() >= 2 && value.compare(value.size() - 2, 2, "ms") == 0) {
    return std::atof(value.substr(0, value.size() - 2).c_str()) / 1000.0;
  }
  if (value.size() >= 3 && value.compare(value.size() - 3, 3, "min") == 0) {
    return std::atof(value.substr(0, value.size() - 3).c_str()) * 60.0;
  }
  if (value.size() >= 1 && value.back() == 's') {
    return std::atof(value.substr(0, value.size() - 1).c_str());
  }
  if (value.size() >= 1 && value.back() == 'h') {
    return std::atof(value.substr(0, value.size() - 1).c_str()) * 3600.0;
  }

  // Bare number means seconds per SMIL spec.
  return std::atof(value.c_str());
}

std::vector<double> SMILAnimationParser::parseKeyTimes(const std::string& value) {
  std::vector<double> result = {};
  if (value.empty()) {
    return result;
  }
  size_t pos = 0;
  while (pos < value.size()) {
    while (pos < value.size() && (std::isspace(static_cast<unsigned char>(value[pos])) != 0)) {
      ++pos;
    }
    if (pos >= value.size()) {
      break;
    }
    char* endPtr = nullptr;
    double num = std::strtod(value.c_str() + pos, &endPtr);
    if (endPtr == value.c_str() + pos) {
      return {};
    }
    result.push_back(num);
    pos = static_cast<size_t>(endPtr - value.c_str());
    while (pos < value.size() && (std::isspace(static_cast<unsigned char>(value[pos])) != 0)) {
      ++pos;
    }
    if (pos < value.size() && value[pos] == ';') {
      ++pos;
    }
  }
  if (result.empty()) {
    return result;
  }
  if (result.front() != 0.0 || result.back() != 1.0) {
    return {};
  }
  for (size_t i = 1; i < result.size(); ++i) {
    if (result[i] < result[i - 1]) {
      return {};
    }
  }
  return result;
}

std::vector<BezierControlPoints> SMILAnimationParser::parseKeySplines(const std::string& value) {
  std::vector<BezierControlPoints> result = {};
  if (value.empty()) {
    return result;
  }
  size_t pos = 0;
  while (pos < value.size()) {
    float nums[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    int filled = 0;
    while (pos < value.size() && filled < 4) {
      while (pos < value.size() && (std::isspace(static_cast<unsigned char>(value[pos])) != 0)) {
        ++pos;
      }
      if (pos >= value.size() || value[pos] == ';') {
        break;
      }
      char* endPtr = nullptr;
      double num = std::strtod(value.c_str() + pos, &endPtr);
      if (endPtr == value.c_str() + pos) {
        return {};
      }
      nums[filled++] = static_cast<float>(num);
      pos = static_cast<size_t>(endPtr - value.c_str());
    }
    if (filled != 4) {
      return {};
    }
    // X components must stay within [0,1] so the bezier is a proper easing curve.
    if (nums[0] < 0.0f || nums[0] > 1.0f || nums[2] < 0.0f || nums[2] > 1.0f) {
      return {};
    }
    BezierControlPoints controls = {};
    controls.p1x = nums[0];
    controls.p1y = nums[1];
    controls.p2x = nums[2];
    controls.p2y = nums[3];
    result.push_back(controls);
    while (pos < value.size() && (std::isspace(static_cast<unsigned char>(value[pos])) != 0)) {
      ++pos;
    }
    if (pos < value.size() && value[pos] == ';') {
      ++pos;
    }
  }
  return result;
}

// Splits a semicolon-separated values string into trimmed tokens. Used for SMIL `values`,
// `keyTimes`, and similar list attributes.
static std::vector<std::string> SplitSemicolons(const std::string& value) {
  std::vector<std::string> result = {};
  if (value.empty()) {
    return result;
  }
  size_t start = 0;
  while (start <= value.size()) {
    auto sep = value.find(';', start);
    if (sep == std::string::npos) {
      sep = value.size();
    }
    std::string token = value.substr(start, sep - start);
    // Trim whitespace.
    auto first = token.find_first_not_of(" \t\n\r");
    auto last = token.find_last_not_of(" \t\n\r");
    if (first != std::string::npos) {
      result.push_back(token.substr(first, last - first + 1));
    }
    start = sep + 1;
  }
  return result;
}

// Parses a single SMIL transform value token ("tx,ty" or "angle" or "angle cx cy") into a list
// of floats. Space and comma are both accepted as separators. Used by parseAnimateTransform to
// extract per-keyframe parameters for each transform type.
static std::vector<float> ParseFloatParams(const std::string& token) {
  std::vector<float> result = {};
  if (token.empty()) {
    return result;
  }
  const char* ptr = token.c_str();
  const char* end = ptr + token.length();
  while (ptr < end) {
    SkipWhitespaceAndComma(ptr, end);
    if (ptr >= end) {
      break;
    }
    char* endPtr = nullptr;
    float num = std::strtof(ptr, &endPtr);
    if (endPtr == ptr) {
      break;
    }
    result.push_back(num);
    ptr = endPtr;
  }
  return result;
}

// Computes the bezier handles for keyframe i given the interpolation mode and splines list.
// Returns {bezierOut, bezierIn} via out params; both are zero when interpolation is not Bezier
// or i is the last keyframe.
// A segment i runs from keyframe i to i + 1. The evaluator reads the easing handles as
// keyframe[i].bezierOut and keyframe[i + 1].bezierIn, so p1 belongs to the segment start and p2 to
// the segment end; writing both onto keyframe i leaves the end handle at its default.
static void ComputeBezierOutHandle(KeyframeInterpolationType interpolation,
                                   const std::vector<BezierControlPoints>& splines, size_t i,
                                   size_t keyframeCount, Point* outBezierOut) {
  *outBezierOut = {};
  if (interpolation == KeyframeInterpolationType::Bezier && i < keyframeCount - 1 &&
      i < splines.size()) {
    outBezierOut->x = static_cast<float>(splines[i].p1x);
    outBezierOut->y = static_cast<float>(splines[i].p1y);
  }
}

static void ComputeBezierInHandle(KeyframeInterpolationType interpolation,
                                  const std::vector<BezierControlPoints>& splines, size_t i,
                                  Point* outBezierIn) {
  *outBezierIn = {};
  if (interpolation == KeyframeInterpolationType::Bezier && i > 0 && i - 1 < splines.size()) {
    outBezierIn->x = static_cast<float>(splines[i - 1].p2x);
    outBezierIn->y = static_cast<float>(splines[i - 1].p2y);
  }
}

// Reads repeatCount attribute: returns -1.0 for "indefinite", 1.0 when absent or unparseable,
// otherwise the count. A positive fractional count truncates the final repetition at that offset
// (repeatCount="1.5" plays one and a half iterations), so it is kept instead of raised to 1.
static double ParseRepeatCount(const std::string& value) {
  if (value == "indefinite") {
    return -1.0;
  }
  if (value.empty()) {
    return 1.0;
  }
  double count = std::atof(value.c_str());
  if (!std::isfinite(count) || count <= 0.0) {
    // SMIL treats a non-positive repeatCount as an error and ignores it.
    return 1.0;
  }
  return count;
}

// Resolves the effective repeat count from repeatCount and repeatDur. SMIL: repeatDur specifies
// the total duration of the repetitions, so the equivalent count is repeatDur / dur. When both
// are set, the one that expires first wins; "indefinite" repeatCount wins outright. Returns -1.0
// for indefinite, otherwise a count >= 1.0.
static double ResolveRepeatCount(const std::string& repeatCountStr, const std::string& repeatDurStr,
                                 double durSeconds) {
  double count = ParseRepeatCount(repeatCountStr);
  if (count < 0.0) {
    return -1.0;
  }
  // repeatDur="indefinite" is the duration-side spelling of an endless repeat.
  if (repeatDurStr == "indefinite") {
    return repeatCountStr.empty() ? -1.0 : count;
  }
  double repeatDurSeconds = SMILAnimationParser::parseSMILClockValue(repeatDurStr);
  if (repeatDurSeconds > 0.0 && durSeconds > 0.0) {
    // A repeatDur shorter than one iteration is a truncated first pass, so the fraction is kept
    // rather than clamped to 1; callers end the track at that point.
    double fromDur = repeatDurSeconds / durSeconds;
    // repeatCount unset (count == 1 with an empty attribute) → repeatDur alone; both set → the
    // earlier expiry wins (the smaller count).
    if (repeatCountStr.empty() || fromDur < count) {
      count = fromDur;
    }
  }
  return count;
}

// A single syncbase reference parsed from a begin/end list item:
//   id                    → target's begin
//   id.begin (+offset)    → target's begin + offset
//   id.end   (+offset)    → target's active end (begin + dur * repeats) + offset
//   id.repeat(n) (+offset)→ target's begin + dur * n + offset
struct SyncBaseRef {
  bool valid = false;
  std::string targetId = {};
  int eventKind = 0;  // 0 = begin, 1 = end, 2 = repeat(n)
  int repeatIndex = 1;
  double offsetSeconds = 0;
};

// True when the trimmed token looks like a clock value (starts with a digit, sign, or dot).
// Interactive offsets like "click" and "accessKey(...)" fail this test.
static bool IsClockToken(const std::string& token) {
  auto first = token.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return false;
  }
  char c = token[first];
  return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}

// SMIL interactive event names; these have no PAGX equivalent and are ignored as begin/end
// offsets. Without this check a bare "click" would be mistaken for a syncbase target id.
static bool IsInteractiveEvent(const std::string& s) {
  return s == "click" || s == "focusin" || s == "focusout" || s == "activate" || s == "mousedown" ||
         s == "mouseup" || s == "mouseover" || s == "mousemove" || s == "mouseout" ||
         s == "beginEvent" || s == "endEvent" || s == "repeatEvent" ||
         s.compare(0, 10, "accessKey(") == 0;
}

// Parses one begin/end list item. Returns a valid SyncBaseRef for syncbase syntax; clock values
// and interactive offsets (click/accessKey) are not syncbases (the caller handles those).
static SyncBaseRef ParseSyncBase(const std::string& token) {
  SyncBaseRef ref = {};
  std::string s = token;
  // Trim whitespace.
  auto first = s.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return ref;
  }
  auto last = s.find_last_not_of(" \t");
  s = s.substr(first, last - first + 1);

  // Split the trailing +/- clock offset, if any. The sign must follow the event suffix and be
  // followed by a clock value: a hyphen inside a legal id ("fade-in.end") is part of the id.
  double offset = 0;
  for (size_t i = 1; i < s.size(); ++i) {
    if (s[i] != '+' && s[i] != '-') {
      continue;
    }
    if (!IsClockToken(s.substr(i + 1))) {
      continue;
    }
    // The sign only starts an offset when an event suffix precedes it, so a hyphen inside a legal
    // id ("fade-in.end") stays part of the id while "base.begin+0.5s" still splits.
    if (s.substr(0, i).rfind('.') == std::string::npos) {
      continue;
    }
    offset = SMILAnimationParser::parseSMILClockValue(s.substr(i));
    s = s.substr(0, i);
    break;
  }
  auto dot = s.rfind('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= s.size()) {
    // A bare id (no event suffix) is a valid syncbase defaulting to the target's begin. Clock
    // values ("1s") and interactive event names ("click") must not be mistaken for ids.
    if (!s.empty() && s.find_first_of(" \t()") == std::string::npos && !IsClockToken(s) &&
        !IsInteractiveEvent(s)) {
      ref.valid = true;
      ref.targetId = s;
      ref.eventKind = 0;
      ref.offsetSeconds = offset;
    }
    return ref;
  }
  ref.targetId = s.substr(0, dot);
  auto event = s.substr(dot + 1);
  if (event == "begin") {
    ref.valid = true;
    ref.eventKind = 0;
  } else if (event == "end") {
    ref.valid = true;
    ref.eventKind = 1;
  } else if (event.compare(0, 7, "repeat(") == 0 && event.back() == ')') {
    ref.valid = true;
    ref.eventKind = 2;
    ref.repeatIndex = std::max(1, atoi(event.substr(7, event.size() - 8).c_str()));
  }
  ref.offsetSeconds = offset;
  return ref;
}

// Truncates a channel's keyframes at endFrame: evaluates the channel at endFrame, drops
// keyframes beyond it, and appends a Hold keyframe carrying the evaluated value so fill="freeze"
// holds the end-time value and fill="remove" (whose base-value keyframe is added later at
// endFrame+1 by AddBaseValueKeyframes) reverts from there.
// Linear interpolation over a transform parameter sequence. A looping animation with a negative
// begin has to move that phase into the values: a PAGX track cannot start before frame 0 (the
// runtime and Ardot both clamp negative times to the first keyframe).
static std::vector<float> SampleTransformParams(const std::vector<std::vector<float>>& sets,
                                                const std::vector<double>& times, double u) {
  if (sets.empty() || times.size() != sets.size()) {
    return {};
  }
  if (u <= times.front()) {
    return sets.front();
  }
  if (u >= times.back()) {
    return sets.back();
  }
  for (size_t k = 1; k < times.size(); ++k) {
    if (u > times[k]) {
      continue;
    }
    const double span = times[k] - times[k - 1];
    const double t = span > 0.0 ? (u - times[k - 1]) / span : 0.0;
    const auto& a = sets[k - 1];
    const auto& b = sets[k];
    std::vector<float> out = {};
    const size_t count = std::max(a.size(), b.size());
    out.reserve(count);
    for (size_t j = 0; j < count; ++j) {
      const float av = j < a.size() ? a[j] : (j < b.size() ? b[j] : 0.0f);
      const float bv = j < b.size() ? b[j] : av;
      out.push_back(static_cast<float>(av + (bv - av) * t));
    }
    return out;
  }
  return sets.back();
}

// Truncates one typed channel at endFrame. When a keyframe already sits on endFrame its value is
// overwritten instead of appended, so tracks keyed by timestamp do not drop a duplicate key.
template <typename T>
static void ApplyEndTimeLimitTo(TypedChannel<T>* channel, Frame endFrame) {
  if (channel == nullptr || channel->keyframes.empty() ||
      channel->keyframes.back().time <= endFrame) {
    return;
  }
  auto value = std::get<T>(channel->evaluateAt(endFrame));
  while (!channel->keyframes.empty() && channel->keyframes.back().time > endFrame) {
    channel->keyframes.pop_back();
  }
  if (!channel->keyframes.empty() && channel->keyframes.back().time == endFrame) {
    channel->keyframes.back().value = value;
    channel->keyframes.back().interpolation = KeyframeInterpolationType::Hold;
    return;
  }
  Keyframe<T> key = {};
  key.time = endFrame;
  key.value = value;
  key.interpolation = KeyframeInterpolationType::Hold;
  channel->keyframes.push_back(key);
}

template <typename T>
static bool KeyframeTimeLess(const Keyframe<T>& a, const Keyframe<T>& b) {
  return a.time < b.time;
}

// Appends the later segment's own keys (`from` = its first animated frame). The segment also
// carries a synthesized "state before begin" hold at frame 0; appending it would put a second key
// on an existing timestamp, and that hold would then become the left endpoint for the frames the
// earlier segment still owns.
template <typename T>
static void AppendTypedKeyframes(TypedChannel<T>* base, const TypedChannel<T>* extra, Frame from) {
  if (base == nullptr || extra == nullptr) {
    return;
  }
  for (const auto& key : extra->keyframes) {
    if (key.time < from) {
      continue;
    }
    base->keyframes.push_back(key);
  }
  // The appended segment may start later than keys already in the base track; a track keyed by
  // timestamp has to stay ordered, otherwise evaluation stops at the first out-of-order pair.
  std::stable_sort(base->keyframes.begin(), base->keyframes.end(), KeyframeTimeLess<T>);
}

// Returns the first keyframe that actually animates. A leading hold key at frame 0 is the
// synthesized "static state before begin" entry: treating it as the animation start makes a
// begin>0 segment look like it starts at 0, and the handover logic then drops the earlier segment.
template <typename T>
static Frame FirstAnimatedFrame(const TypedChannel<T>* channel) {
  if (channel == nullptr || channel->keyframes.empty()) {
    return 0;
  }
  for (const auto& keyframe : channel->keyframes) {
    if (keyframe.time <= 0 && keyframe.interpolation == KeyframeInterpolationType::Hold &&
        &keyframe != &channel->keyframes.back()) {
      continue;
    }
    return keyframe.time;
  }
  return channel->keyframes.front().time;
}

static Frame ChannelStartFrame(const Channel* channel) {
  if (channel == nullptr) {
    return 0;
  }
  switch (channel->valueType()) {
    case ChannelValueType::Float: {
      return FirstAnimatedFrame(static_cast<const TypedChannel<float>*>(channel));
    }
    case ChannelValueType::Color: {
      return FirstAnimatedFrame(static_cast<const TypedChannel<Color>*>(channel));
    }
    case ChannelValueType::Matrix: {
      return FirstAnimatedFrame(static_cast<const TypedChannel<Matrix>*>(channel));
    }
    case ChannelValueType::Bool: {
      return FirstAnimatedFrame(static_cast<const TypedChannel<bool>*>(channel));
    }
    default:
      return 0;
  }
}

// Rotates a typed track by phaseShift of one iteration: g(t) = f((t + offset) mod dur). A negative
// begin on a looping animation means it already started before frame 0, so every authored key
// moves back by `offset` (wrapped into [0, dur]) and keeps its value; both boundaries are pinned so
// the runtime never clamps a frame to a value from the wrong side of the seam.
template <typename T>
static void ShiftChannelPhase(TypedChannel<T>* channel, Frame durFrames, double phaseShift) {
  if (channel == nullptr || channel->keyframes.empty() || phaseShift <= 0.0 || durFrames <= 0) {
    return;
  }
  const auto offset = static_cast<Frame>(std::round(phaseShift * static_cast<double>(durFrames)));
  if (offset <= 0 || offset >= durFrames) {
    return;
  }
  struct ShiftedKey {
    Frame time = 0;
    Frame sourceTime = 0;
    Keyframe<T> key = {};
  };
  std::vector<ShiftedKey> entries = {};
  entries.reserve(channel->keyframes.size());
  for (const auto& key : channel->keyframes) {
    ShiftedKey entry = {};
    entry.sourceTime = key.time;
    entry.time = key.time - offset;
    if (entry.time < 0) {
      entry.time += durFrames;
    }
    entry.key = key;
    entries.push_back(entry);
  }
  std::stable_sort(entries.begin(), entries.end(), [](const ShiftedKey& a, const ShiftedKey& b) {
    if (a.time != b.time) {
      return a.time < b.time;
    }
    // Both loop endpoints can land on the seam frame; the pre-wrap value (the later source time)
    // comes first so the jump at the seam keeps its direction.
    return a.sourceTime > b.sourceTime;
  });
  std::vector<Keyframe<T>> shifted = {};
  shifted.reserve(entries.size() + 2);
  for (const auto& entry : entries) {
    auto key = entry.key;
    key.time = entry.time;
    if (!shifted.empty() && key.time <= shifted.back().time) {
      // Two source keys collapsing onto one frame: keep both by moving the later one one frame on.
      key.time = shifted.back().time + 1;
    }
    shifted.push_back(key);
  }
  auto makeBoundary = [channel](Frame time, Frame sampleTime) {
    Keyframe<T> key = {};
    key.time = time;
    key.value = std::get<T>(channel->evaluateAt(sampleTime));
    key.interpolation = KeyframeInterpolationType::Linear;
    return key;
  };
  if (shifted.front().time > 0) {
    auto head = makeBoundary(0, offset);
    if (shifted.front().interpolation == KeyframeInterpolationType::Hold) {
      head.interpolation = KeyframeInterpolationType::Hold;
    }
    shifted.insert(shifted.begin(), head);
  }
  if (shifted.back().time < durFrames) {
    // g(dur) wraps back onto the phase start, so the tail is the same value as the head.
    shifted.push_back(makeBoundary(durFrames, offset));
  }
  channel->keyframes = std::move(shifted);
}

static void ApplyEndTimeLimit(Channel* channel, ChannelValueType valueType, Frame endFrame) {
  switch (valueType) {
    case ChannelValueType::Float:
      ApplyEndTimeLimitTo(static_cast<TypedChannel<float>*>(channel), endFrame);
      break;
    case ChannelValueType::Color:
      ApplyEndTimeLimitTo(static_cast<TypedChannel<Color>*>(channel), endFrame);
      break;
    case ChannelValueType::Matrix:
      ApplyEndTimeLimitTo(static_cast<TypedChannel<Matrix>*>(channel), endFrame);
      break;
    case ChannelValueType::Bool:
      ApplyEndTimeLimitTo(static_cast<TypedChannel<bool>*>(channel), endFrame);
      break;
    default:
      break;
  }
}

// Timing info for one SMIL animation element, gathered in a pre-scan and used to solve
// syncbase references (begin="id.end") across all elements before any parsing happens.
struct TimingInfo {
  const DOMNode* element = nullptr;
  std::string id = {};
  double durSeconds = 0;
  double repeatCount = 1;  // >= 1, or -1 for indefinite
  // begin list: the earliest clock item and the first syncbase item.
  bool hasClockBegin = false;
  double clockBegin = 0;
  SyncBaseRef beginSync = {};
  bool hasBeginSync = false;
  bool beginResolved = false;
  double resolvedBegin = 0;
  // end: single value (first item of the list).
  bool hasClockEnd = false;
  double clockEnd = -1;
  SyncBaseRef endSync = {};
  bool hasEndSync = false;
  bool endResolved = false;
  double resolvedEnd = -1;
  // Member of a syncbase cycle: its begin comes from the previous lap, and the timeline loops.
  bool cyclic = false;
};

// The active duration of a timing target: dur * repeats. Indefinite repeats and missing dur
// fall back to a single dur (the sync event time is approximate in those rare cases).
static double ActiveDuration(const TimingInfo& info) {
  if (info.durSeconds <= 0) {
    return 0;
  }
  double active = info.repeatCount < 0 ? info.durSeconds : info.durSeconds * info.repeatCount;
  // An end attribute shortens the active duration: `id.end` must point at the time the target
  // really stops, not at begin + dur * repeats. The comparison is in document time.
  if (info.endResolved && info.resolvedEnd >= 0.0) {
    double limit = info.resolvedEnd - info.resolvedBegin;
    if (limit >= 0.0 && limit < active) {
      active = limit;
    }
  }
  return active;
}

// Computes the document time a syncbase reference points at, given its (already resolved)
// target.
static double SyncEventTime(const TimingInfo& target, const SyncBaseRef& ref) {
  double base = target.resolvedBegin;
  if (ref.eventKind == 1) {
    base += ActiveDuration(target);
  } else if (ref.eventKind == 2) {
    base += target.durSeconds * static_cast<double>(ref.repeatIndex);
  }
  return base + ref.offsetSeconds;
}

// Pre-scans every SMIL element in the document, parses begin/end, and solves syncbase
// references to fixed-point (chains like a.end -> b.end -> c resolve in dependency order;
// cycles fall back to 0). Interactive offsets resolve to their clock component or 0. Returns
// per-element resolved timing consumed by the parse functions.
// Clamps a clock value read from an untrusted attribute. inf/NaN would make round() undefined, and
// an unbounded time would expand into an unbounded number of keyframes.
static double SanitizeClockValue(double seconds) {
  if (!std::isfinite(seconds)) {
    return 0.0;
  }
  if (seconds > kMaxDurationSeconds) {
    return kMaxDurationSeconds;
  }
  if (seconds < -kMaxDurationSeconds) {
    return -kMaxDurationSeconds;
  }
  return seconds;
}

std::unordered_map<const DOMNode*, ResolvedTiming> SMILAnimationParser::ResolveAllTimings(
    SVGParserContext& ctx,
    const std::unordered_map<const DOMNode*, SMILAnimationGroup>& smilAnimations) {
  std::vector<TimingInfo> infos = {};
  std::unordered_map<std::string, size_t> idToIndex = {};

  // Collect timing info for every animation element (id, dur, repeatCount, begin/end lists).
  for (const auto& pair : smilAnimations) {
    const auto& group = pair.second;
    std::vector<const std::vector<std::shared_ptr<DOMNode>>*> elements = {
        &group.animates, &group.sets, &group.animateTransforms, &group.animateMotions};
    for (const auto* list : elements) {
      for (const auto& el : *list) {
        if (el == nullptr) {
          continue;
        }
        TimingInfo info = {};
        info.element = el.get();
        info.id = ctx.getAttribute(el, "id");
        info.durSeconds = SanitizeClockValue(parseSMILClockValue(ctx.getAttribute(el, "dur")));
        info.repeatCount = ResolveRepeatCount(ctx.getAttribute(el, "repeatCount"),
                                              ctx.getAttribute(el, "repeatDur"), info.durSeconds);

        auto beginTokens = SplitSemicolons(ctx.getAttribute(el, "begin"));
        for (const auto& token : beginTokens) {
          SyncBaseRef sync = ParseSyncBase(token);
          if (sync.valid) {
            if (!info.hasBeginSync) {
              info.beginSync = sync;
              info.hasBeginSync = true;
            }
            continue;
          }
          if (IsClockToken(token)) {
            double t = SanitizeClockValue(parseSMILClockValue(token));
            if (!info.hasClockBegin || t < info.clockBegin) {
              info.clockBegin = t;
              info.hasClockBegin = true;
            }
          }
          // Interactive offsets (click, accessKey) have no PAGX equivalent: skipped.
        }

        auto endTokens = SplitSemicolons(ctx.getAttribute(el, "end"));
        if (!endTokens.empty()) {
          SyncBaseRef sync = ParseSyncBase(endTokens.front());
          if (sync.valid) {
            info.endSync = sync;
            info.hasEndSync = true;
          } else if (IsClockToken(endTokens.front())) {
            info.clockEnd = SanitizeClockValue(parseSMILClockValue(endTokens.front()));
            info.hasClockEnd = true;
          }
        }

        if (!info.id.empty()) {
          idToIndex[info.id] = infos.size();
        }
        infos.push_back(std::move(info));
      }
    }
  }

  // Clock `end` values depend on nothing, so they are resolved before the begin fixed point:
  // `begin="a.end"` has to see a's real end, which ActiveDuration folds into the active duration.
  // Syncbase `end` values still need begin times and stay in the pass after the begins.
  for (auto& info : infos) {
    if (info.hasClockEnd) {
      info.resolvedEnd = info.clockEnd;
      info.endResolved = true;
    }
  }

  // Solve begin times to a fixed point.
  bool progress = true;
  while (progress) {
    progress = false;
    for (auto& info : infos) {
      if (info.beginResolved) {
        continue;
      }
      bool blocked = false;
      double best = 0;
      bool hasCandidate = false;
      if (info.hasClockBegin) {
        best = info.clockBegin;
        hasCandidate = true;
      }
      if (info.hasBeginSync) {
        auto it = idToIndex.find(info.beginSync.targetId);
        if (it == idToIndex.end()) {
          // Unknown target id: treat the offset alone as the begin time.
          double t = info.beginSync.offsetSeconds;
          if (!hasCandidate || t < best) {
            best = t;
            hasCandidate = true;
          }
        } else if (infos[it->second].beginResolved) {
          double t = SyncEventTime(infos[it->second], info.beginSync);
          if (!hasCandidate || t < best) {
            best = t;
            hasCandidate = true;
          }
        } else {
          blocked = true;
        }
      }
      if (!blocked && (hasCandidate || (!info.hasClockBegin && !info.hasBeginSync))) {
        // No begin attribute at all resolves to 0.
        info.resolvedBegin = hasCandidate ? best : 0;
        info.beginResolved = true;
        progress = true;
      }
    }
  }
  // Cyclic references (A.begin = B.end, B.begin = A.end) cannot be solved by the fixed point
  // above. Anchor the members that carry a clock value, then let the cascade resolve the rest:
  // the sequence runs one lap after another and repeats, so the timeline loops.
  bool saw_cycle = false;
  for (auto& info : infos) {
    if (info.beginResolved || !info.hasClockBegin) {
      continue;
    }
    info.resolvedBegin = info.clockBegin;
    info.beginResolved = true;
    info.cyclic = true;
    saw_cycle = true;
  }
  if (saw_cycle) {
    progress = true;
    while (progress) {
      progress = false;
      for (auto& info : infos) {
        if (info.beginResolved || !info.hasBeginSync) {
          continue;
        }
        auto it = idToIndex.find(info.beginSync.targetId);
        if (it == idToIndex.end() || !infos[it->second].beginResolved) {
          continue;
        }
        info.resolvedBegin = SyncEventTime(infos[it->second], info.beginSync);
        info.beginResolved = true;
        info.cyclic = true;
        progress = true;
      }
    }
  }
  // Remaining members are a cycle without any clock anchor: keep the previous 0 fallback.
  for (auto& info : infos) {
    if (!info.beginResolved) {
      info.resolvedBegin = 0;
      info.beginResolved = true;
      info.cyclic = true;
    }
  }

  // Solve end times (single value each: clock or syncbase).
  progress = true;
  while (progress) {
    progress = false;
    for (auto& info : infos) {
      if (info.endResolved) {
        continue;
      }
      if (info.hasClockEnd) {
        info.resolvedEnd = info.clockEnd;
        info.endResolved = true;
        progress = true;
        continue;
      }
      if (!info.hasEndSync) {
        info.endResolved = true;
        continue;
      }
      auto it = idToIndex.find(info.endSync.targetId);
      if (it == idToIndex.end()) {
        info.resolvedEnd = info.endSync.offsetSeconds;
        info.endResolved = true;
        progress = true;
      } else if (infos[it->second].beginResolved) {
        info.resolvedEnd = SyncEventTime(infos[it->second], info.endSync);
        info.endResolved = true;
        progress = true;
      }
    }
  }

  std::unordered_map<const DOMNode*, ResolvedTiming> result = {};
  for (const auto& info : infos) {
    ResolvedTiming timing = {};
    timing.beginSeconds = info.resolvedBegin;
    timing.endSeconds = info.endResolved ? info.resolvedEnd : -1;
    timing.cyclic = info.cyclic;
    result[info.element] = timing;
  }
  return result;
}

// Expands a float-typed channel's keyframes to repeat the animation repeatCount times.
// accumulate="sum" offsets repetition k by the final value of one full iteration (SMIL accumulates
// the previous iteration's end value, not its delta), so a sequence that starts at a non-zero value
// keeps climbing: cx 10→20 repeats as 20→30, never as 30→40. The fractional tail copies the keys
// the remainder round actually covers and ends at the value the authored curve has at the cut.
static void ExpandFloatRepeatCount(Channel* channel, Frame durFrames, Frame beginFrames,
                                   double repeatCount, bool accumulate) {
  auto* ch = static_cast<TypedChannel<float>*>(channel);
  auto original = ch->keyframes;
  if (original.empty() || repeatCount <= 1.0) {
    return;
  }
  int fullRepeats = static_cast<int>(std::floor(repeatCount));
  double fraction = repeatCount - std::floor(repeatCount);
  float step = 0.0f;
  if (accumulate && original.size() >= 2) {
    step = original.back().value;
  }
  // The tail value must come from the authored curve at the cut: interpolating first→last reads
  // the wrong value for a multi-keyframe sequence (values 0;1;0 cut at 0.5 ends at 1, not 0).
  TypedChannel<float> source = {};
  source.keyframes = original;
  ch->keyframes.clear();
  for (int k = 0; k < fullRepeats; ++k) {
    float offset = static_cast<float>(k) * step;
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    for (size_t idx = 0; idx < original.size(); ++idx) {
      auto key = original[idx];
      key.time += timeOffset;
      if (accumulate) {
        key.value += offset;
      }
      ch->keyframes.push_back(key);
    }
  }
  if (fraction > 0.0 && original.size() >= 2) {
    int k = fullRepeats;
    float offset = static_cast<float>(k) * step;
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    Frame cutTime = beginFrames + timeOffset + static_cast<Frame>(std::round(fraction * durFrames));
    // Copy the keys of the truncated remainder round, stopping at the cut; the key that lands on
    // the cut is re-emitted below with the value the curve has there.
    for (const auto& originalKey : original) {
      auto key = originalKey;
      key.time += timeOffset;
      if (key.time >= cutTime) {
        break;
      }
      if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
        continue;
      }
      if (accumulate) {
        key.value += offset;
      }
      ch->keyframes.push_back(key);
    }
    Keyframe<float> key = {};
    key.time = cutTime;
    if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
      // round(fraction * durFrames) can collapse onto the previous period end.
      key.time = ch->keyframes.back().time + 1;
    }
    key.value = std::get<float>(source.evaluateAt(
                    beginFrames + static_cast<Frame>(std::round(fraction * durFrames)))) +
                offset;
    key.interpolation = KeyframeInterpolationType::Linear;
    ch->keyframes.push_back(key);
  }
}

// Expands a Color-typed channel's keyframes similarly to ExpandFloatRepeatCount, accumulating
// each color channel independently.
static void ExpandColorRepeatCount(Channel* channel, Frame durFrames, Frame beginFrames,
                                   double repeatCount, bool accumulate) {
  auto* ch = static_cast<TypedChannel<Color>*>(channel);
  auto original = ch->keyframes;
  if (original.empty() || repeatCount <= 1.0) {
    return;
  }
  int fullRepeats = static_cast<int>(std::floor(repeatCount));
  double fraction = repeatCount - std::floor(repeatCount);
  Color step = {0, 0, 0, 0};
  if (accumulate && original.size() >= 2) {
    step.red = original.back().value.red;
    step.green = original.back().value.green;
    step.blue = original.back().value.blue;
    step.alpha = original.back().value.alpha;
  }
  TypedChannel<Color> source = {};
  source.keyframes = original;
  ch->keyframes.clear();
  for (int k = 0; k < fullRepeats; ++k) {
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    for (size_t idx = 0; idx < original.size(); ++idx) {
      auto key = original[idx];
      key.time += timeOffset;
      if (accumulate) {
        key.value.red += static_cast<float>(k) * step.red;
        key.value.green += static_cast<float>(k) * step.green;
        key.value.blue += static_cast<float>(k) * step.blue;
        key.value.alpha += static_cast<float>(k) * step.alpha;
      }
      ch->keyframes.push_back(key);
    }
  }
  if (fraction > 0.0 && original.size() >= 2) {
    int k = fullRepeats;
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    Frame cutTime = beginFrames + timeOffset + static_cast<Frame>(std::round(fraction * durFrames));
    for (const auto& originalKey : original) {
      auto key = originalKey;
      key.time += timeOffset;
      if (key.time >= cutTime) {
        // The key on the cut is re-emitted below from the curve.
        break;
      }
      if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
        continue;
      }
      if (accumulate) {
        key.value.red += static_cast<float>(k) * step.red;
        key.value.green += static_cast<float>(k) * step.green;
        key.value.blue += static_cast<float>(k) * step.blue;
        key.value.alpha += static_cast<float>(k) * step.alpha;
      }
      ch->keyframes.push_back(key);
    }
    Keyframe<Color> key = {};
    key.time = cutTime;
    if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
      // round(fraction * durFrames) can collapse onto the previous period end.
      key.time = ch->keyframes.back().time + 1;
    }
    key.value = std::get<Color>(
        source.evaluateAt(beginFrames + static_cast<Frame>(std::round(fraction * durFrames))));
    if (accumulate) {
      key.value.red += static_cast<float>(k) * step.red;
      key.value.green += static_cast<float>(k) * step.green;
      key.value.blue += static_cast<float>(k) * step.blue;
      key.value.alpha += static_cast<float>(k) * step.alpha;
    }
    key.interpolation = KeyframeInterpolationType::Linear;
    ch->keyframes.push_back(key);
  }
}

// Copies one iteration of a matrix/bool track into the expanded channel, then closes the
// fractional remainder round at the value the authored track has when the repetition is cut.
template <typename T>
static void ExpandTypedRepeatCount(TypedChannel<T>* ch, Frame durFrames, Frame beginFrames,
                                   double repeatCount) {
  auto original = ch->keyframes;
  if (original.empty() || repeatCount <= 1.0) {
    return;
  }
  int fullRepeats = static_cast<int>(std::floor(repeatCount));
  double fraction = repeatCount - std::floor(repeatCount);
  TypedChannel<T> source = {};
  source.keyframes = original;
  ch->keyframes.clear();
  for (int k = 0; k < fullRepeats; ++k) {
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    for (size_t idx = 0; idx < original.size(); ++idx) {
      auto key = original[idx];
      key.time += timeOffset;
      ch->keyframes.push_back(key);
    }
  }
  if (fraction > 0.0 && original.size() >= 2) {
    int k = fullRepeats;
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    Frame cutTime = beginFrames + timeOffset + static_cast<Frame>(std::round(fraction * durFrames));
    // Copy the keys the remainder round covers, so a multi-keyframe sequence keeps its shape
    // instead of collapsing into a single interpolated endpoint.
    for (const auto& originalKey : original) {
      auto key = originalKey;
      key.time += timeOffset;
      if (key.time >= cutTime) {
        // The key on the cut is re-emitted below from the curve.
        break;
      }
      if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
        continue;
      }
      ch->keyframes.push_back(key);
    }
    Keyframe<T> key = {};
    key.time = cutTime;
    if (!ch->keyframes.empty() && key.time <= ch->keyframes.back().time) {
      // round(fraction * durFrames) can collapse onto the previous period end.
      key.time = ch->keyframes.back().time + 1;
    }
    key.value = std::get<T>(
        source.evaluateAt(beginFrames + static_cast<Frame>(std::round(fraction * durFrames))));
    key.interpolation = KeyframeInterpolationType::Linear;
    ch->keyframes.push_back(key);
  }
}

// Dispatches repeatCount expansion to the correct TypedChannel<T> specialization based on
// valueType. Non-numeric types (bool/string) repeat without accumulation.
static void ExpandRepeatCount(Channel* channel, ChannelValueType valueType, Frame durFrames,
                              Frame beginFrames, double repeatCount, bool accumulate) {
  if (repeatCount <= 1.0) {
    return;
  }
  if (repeatCount > kMaxRepeatCount) {
    repeatCount = kMaxRepeatCount;
  }
  switch (valueType) {
    case ChannelValueType::Float:
      ExpandFloatRepeatCount(channel, durFrames, beginFrames, repeatCount, accumulate);
      break;
    case ChannelValueType::Color:
      ExpandColorRepeatCount(channel, durFrames, beginFrames, repeatCount, accumulate);
      break;
    case ChannelValueType::Matrix: {
      // Matrix repeats copy keyframes without accumulation. animateTransform accumulate on a
      // baked matrix is intentionally ignored, matching animateMotion's accumulate behaviour.
      ExpandTypedRepeatCount(static_cast<TypedChannel<Matrix>*>(channel), durFrames, beginFrames,
                             repeatCount);
      break;
    }
    case ChannelValueType::Bool: {
      ExpandTypedRepeatCount(static_cast<TypedChannel<bool>*>(channel), durFrames, beginFrames,
                             repeatCount);
      break;
    }
    default:
      break;
  }
}

// Recomputes keyTimes so that time is distributed proportionally to the distance between
// adjacent values (calcMode="paced"). Only supports float and Color value types; other types
// fall back to uniform distribution. Returns true when keyTimes were recomputed.
static bool ComputePacedKeyTimes(const std::vector<KeyValue>& values, ChannelValueType valueType,
                                 std::vector<double>* outKeyTimes) {
  if (values.size() < 2 || outKeyTimes == nullptr) {
    return false;
  }
  std::vector<double> distances = {};
  double totalDist = 0.0;
  for (size_t i = 1; i < values.size(); ++i) {
    double d = 0.0;
    if (valueType == ChannelValueType::Float) {
      d = std::abs(std::get<float>(values[i]) - std::get<float>(values[i - 1]));
    } else if (valueType == ChannelValueType::Color) {
      const auto& c0 = std::get<Color>(values[i - 1]);
      const auto& c1 = std::get<Color>(values[i]);
      double dr = c1.red - c0.red;
      double dg = c1.green - c0.green;
      double db = c1.blue - c0.blue;
      d = std::sqrt(dr * dr + dg * dg + db * db);
    } else {
      return false;
    }
    distances.push_back(d);
    totalDist += d;
  }
  if (totalDist <= 0.0) {
    return false;
  }
  outKeyTimes->resize(values.size());
  (*outKeyTimes)[0] = 0.0;
  double cumul = 0.0;
  for (size_t i = 0; i < distances.size(); ++i) {
    cumul += distances[i];
    (*outKeyTimes)[i + 1] = cumul / totalDist;
  }
  return true;
}

// A channel paired with its source SMIL element's additive flag, used by MergeAdditiveChannels
// to precompute additive="sum" overlays into a single keyframe sequence per channel name.
struct ChannelWithAdditive {
  Channel* channel = nullptr;
  bool additive = false;
};

// Maps keyframe time to its index so the additive merge resolves neighbours in O(1). Scanning the
// keyframe vector once per union time made the merge O(n^2), which shows up on rotation tracks
// split into 20-degree steps.
template <typename T>
static std::unordered_map<Frame, size_t> BuildKeyframeTimeIndex(const TypedChannel<T>& channel) {
  std::unordered_map<Frame, size_t> index = {};
  index.reserve(channel.keyframes.size());
  for (size_t i = 0; i < channel.keyframes.size(); ++i) {
    index.emplace(channel.keyframes[i].time, i);
  }
  return index;
}

// Merges one additive channel into the base channel for float-typed channels. Collects the union
// of keyframe times from both channels, samples each at every time point, and replaces base's
// keyframes with the summed values. Interpolation mode and bezier handles at a given time are
// taken from whichever channel already has a keyframe there (base takes priority).
static void MergeAdditiveFloat(Channel* base, const Channel* additive) {
  auto* baseCh = static_cast<TypedChannel<float>*>(base);
  auto* additiveCh = static_cast<const TypedChannel<float>*>(additive);
  if (baseCh->keyframes.empty() || additiveCh->keyframes.empty()) {
    return;
  }

  std::set<Frame> allTimes = {};
  for (const auto& key : baseCh->keyframes) {
    allTimes.insert(key.time);
  }
  for (const auto& key : additiveCh->keyframes) {
    allTimes.insert(key.time);
  }

  const auto baseTimes = BuildKeyframeTimeIndex(*baseCh);
  const auto additiveTimes = BuildKeyframeTimeIndex(*additiveCh);
  std::vector<Keyframe<float>> merged = {};
  for (Frame time : allTimes) {
    float baseVal = std::get<float>(base->evaluateAt(time));
    // Before the additive track begins, SMIL adds the identity value rather than the track's
    // first keyframe, otherwise the base value would be summed with itself during the lead-in.
    float additiveVal = 0.0f;
    if (time >= additiveCh->keyframes.front().time) {
      additiveVal = std::get<float>(additive->evaluateAt(time));
    }
    Keyframe<float> key = {};
    key.time = time;
    key.value = baseVal + additiveVal;
    // Prefer base's interpolation/bezier when the time matches a base keyframe; otherwise take
    // additive's. Times present in neither (shouldn't happen) default to Linear.
    auto baseIt = baseTimes.find(time);
    if (baseIt != baseTimes.end()) {
      const auto& src = baseCh->keyframes[baseIt->second];
      key.interpolation = src.interpolation;
      key.bezierOut = src.bezierOut;
      key.bezierIn = src.bezierIn;
    } else {
      auto addIt = additiveTimes.find(time);
      if (addIt != additiveTimes.end()) {
        const auto& src = additiveCh->keyframes[addIt->second];
        key.interpolation = src.interpolation;
        key.bezierOut = src.bezierOut;
        key.bezierIn = src.bezierIn;
      }
    }
    merged.push_back(key);
  }
  baseCh->keyframes = std::move(merged);
}

// Merges one additive channel into the base channel for Color-typed channels. Each RGBA component
// is summed independently, mirroring the float merge logic.
static void MergeAdditiveColor(Channel* base, const Channel* additive) {
  auto* baseCh = static_cast<TypedChannel<Color>*>(base);
  auto* additiveCh = static_cast<const TypedChannel<Color>*>(additive);
  if (baseCh->keyframes.empty() || additiveCh->keyframes.empty()) {
    return;
  }

  std::set<Frame> allTimes = {};
  for (const auto& key : baseCh->keyframes) {
    allTimes.insert(key.time);
  }
  for (const auto& key : additiveCh->keyframes) {
    allTimes.insert(key.time);
  }

  const auto baseTimes = BuildKeyframeTimeIndex(*baseCh);
  const auto additiveTimes = BuildKeyframeTimeIndex(*additiveCh);
  std::vector<Keyframe<Color>> merged = {};
  for (Frame time : allTimes) {
    Color baseVal = std::get<Color>(base->evaluateAt(time));
    // Same identity rule as the float merge: no add before the additive track begins.
    Color additiveVal = {0, 0, 0, 0};
    if (time >= additiveCh->keyframes.front().time) {
      additiveVal = std::get<Color>(additive->evaluateAt(time));
    }
    Keyframe<Color> key = {};
    key.time = time;
    key.value.red = baseVal.red + additiveVal.red;
    key.value.green = baseVal.green + additiveVal.green;
    key.value.blue = baseVal.blue + additiveVal.blue;
    key.value.alpha = baseVal.alpha + additiveVal.alpha;
    auto baseIt = baseTimes.find(time);
    if (baseIt != baseTimes.end()) {
      const auto& src = baseCh->keyframes[baseIt->second];
      key.interpolation = src.interpolation;
      key.bezierOut = src.bezierOut;
      key.bezierIn = src.bezierIn;
    } else {
      auto addIt = additiveTimes.find(time);
      if (addIt != additiveTimes.end()) {
        const auto& src = additiveCh->keyframes[addIt->second];
        key.interpolation = src.interpolation;
        key.bezierOut = src.bezierOut;
        key.bezierIn = src.bezierIn;
      }
    }
    merged.push_back(key);
  }
  baseCh->keyframes = std::move(merged);
}

// Builds a Matrix from one animateTransform keyframe's parameter list. type selects the transform
// kind; params carries the numeric arguments as parsed by ParseFloatParams:
//   translate tx [ty]         -> Translate(tx, ty ?: tx)
//   scale    sx [sy]           -> Scale(sx, sy ?: sx)
//   rotate   angle [cx cy]    -> Translate(cx,cy) * Rotate(angle) * Translate(-cx,-cy)
//   skewX    angle             -> matrix with c = tan(angle)
//   skewY    angle             -> matrix with b = tan(angle)
static Matrix TransformParamsToMatrix(const std::string& type, const std::vector<float>& params) {
  if (type == "translate") {
    float tx = (params.size() > 0) ? params[0] : 0.0f;
    float ty = (params.size() > 1) ? params[1] : 0.0f;
    return Matrix::Translate(tx, ty);
  }
  if (type == "scale") {
    float sx = (params.size() > 0) ? params[0] : 1.0f;
    float sy = (params.size() > 1) ? params[1] : sx;
    return Matrix::Scale(sx, sy);
  }
  if (type == "rotate") {
    float angle = (params.size() > 0) ? params[0] : 0.0f;
    if (params.size() >= 3) {
      float cx = params[1];
      float cy = params[2];
      return Matrix::Translate(cx, cy) * Matrix::Rotate(angle) * Matrix::Translate(-cx, -cy);
    }
    return Matrix::Rotate(angle);
  }
  if (type == "skewX") {
    float angle = (params.size() > 0) ? params[0] : 0.0f;
    float t = std::tan(angle * 3.14159265358979323846f / 180.0f);
    Matrix m = {};
    m.c = t;
    return m;
  }
  if (type == "skewY") {
    float angle = (params.size() > 0) ? params[0] : 0.0f;
    float t = std::tan(angle * 3.14159265358979323846f / 180.0f);
    Matrix m = {};
    m.b = t;
    return m;
  }
  return Matrix::Identity();
}

// Merges one additive="sum" matrix channel into the base matrix channel by composing both at the
// union of their keyframe times. Matrix additive composition is multiplication (base * additive),
// matching how SVG layers consecutive additive transforms. Mirrors MergeAdditiveFloat/Color.
static void MergeAdditiveMatrix(Channel* base, const Channel* additive) {
  auto* baseCh = static_cast<TypedChannel<Matrix>*>(base);
  auto* additiveCh = static_cast<const TypedChannel<Matrix>*>(additive);
  if (baseCh->keyframes.empty() || additiveCh->keyframes.empty()) {
    return;
  }
  std::set<Frame> allTimes = {};
  for (const auto& key : baseCh->keyframes) {
    allTimes.insert(key.time);
  }
  for (const auto& key : additiveCh->keyframes) {
    allTimes.insert(key.time);
  }
  const auto baseTimes = BuildKeyframeTimeIndex(*baseCh);
  const auto additiveTimes = BuildKeyframeTimeIndex(*additiveCh);
  std::vector<Keyframe<Matrix>> merged = {};
  for (Frame time : allTimes) {
    Matrix baseVal = std::get<Matrix>(base->evaluateAt(time));
    Matrix additiveVal = std::get<Matrix>(additive->evaluateAt(time));
    Keyframe<Matrix> key = {};
    key.time = time;
    key.value = baseVal * additiveVal;
    auto baseIt = baseTimes.find(time);
    if (baseIt != baseTimes.end()) {
      const auto& src = baseCh->keyframes[baseIt->second];
      key.interpolation = src.interpolation;
      key.bezierOut = src.bezierOut;
      key.bezierIn = src.bezierIn;
    } else {
      auto addIt = additiveTimes.find(time);
      if (addIt != additiveTimes.end()) {
        const auto& src = additiveCh->keyframes[addIt->second];
        key.interpolation = src.interpolation;
        key.bezierOut = src.bezierOut;
        key.bezierIn = src.bezierIn;
      }
    }
    merged.push_back(key);
  }
  baseCh->keyframes = std::move(merged);
}

// Merges additive="sum" Bool channels by logical OR at each keyframe time. SMIL does not define
// arithmetic addition for booleans; the common interpretation (and the one that matches SVG's
// `display`/`visibility` accumulation) is that the element stays visible if any contributing
// animation says so. Mirrors MergeAdditiveFloat/Color/Matrix's union-of-times approach.
static void MergeAdditiveBool(Channel* base, const Channel* additive) {
  auto* baseCh = static_cast<TypedChannel<bool>*>(base);
  auto* additiveCh = static_cast<const TypedChannel<bool>*>(additive);
  if (baseCh->keyframes.empty() || additiveCh->keyframes.empty()) {
    return;
  }
  std::set<Frame> allTimes = {};
  for (const auto& key : baseCh->keyframes) {
    allTimes.insert(key.time);
  }
  for (const auto& key : additiveCh->keyframes) {
    allTimes.insert(key.time);
  }
  const auto baseTimes = BuildKeyframeTimeIndex(*baseCh);
  const auto additiveTimes = BuildKeyframeTimeIndex(*additiveCh);
  std::vector<Keyframe<bool>> merged = {};
  for (Frame time : allTimes) {
    bool baseVal = std::get<bool>(base->evaluateAt(time));
    bool additiveVal = std::get<bool>(additive->evaluateAt(time));
    Keyframe<bool> key = {};
    key.time = time;
    key.value = baseVal || additiveVal;
    auto baseIt = baseTimes.find(time);
    if (baseIt != baseTimes.end()) {
      key.interpolation = baseCh->keyframes[baseIt->second].interpolation;
    } else {
      auto addIt = additiveTimes.find(time);
      if (addIt != additiveTimes.end()) {
        key.interpolation = additiveCh->keyframes[addIt->second].interpolation;
      }
    }
    merged.push_back(key);
  }
  baseCh->keyframes = std::move(merged);
}

// Merges additive="sum" channels into their preceding base (replace) channel. Channels sharing
// the same name are grouped; within each group, the first channel is the base and subsequent
// additive="sum" channels are sampled and summed into it. A subsequent additive="replace"
// channel resets the base. Returns the deduplicated channel list (one entry per name).
static std::vector<Channel*> MergeAdditiveChannels(std::vector<ChannelWithAdditive>* channels) {
  if (channels->empty()) {
    return {};
  }
  // Group channels by name, preserving document order within each group.
  std::unordered_map<std::string, std::vector<size_t>> nameToIndices = {};
  for (size_t i = 0; i < channels->size(); ++i) {
    nameToIndices[(*channels)[i].channel->name].push_back(i);
  }

  std::vector<Channel*> result = {};
  // Track which indices have been consumed by a merge so we skip them in the output.
  std::vector<bool> consumed(channels->size(), false);
  for (size_t i = 0; i < channels->size(); ++i) {
    if (consumed[i]) {
      continue;
    }
    auto& indices = nameToIndices[(*channels)[i].channel->name];
    Channel* base = (*channels)[i].channel;
    for (size_t j = 1; j < indices.size(); ++j) {
      size_t idx = indices[j];
      consumed[idx] = true;
      if ((*channels)[idx].additive) {
        // additive="sum": merge into base. Only float and Color are supported; other types
        // fall through (the additive channel is dropped, matching replace-on-incompatible).
        if (base->valueType() == ChannelValueType::Float) {
          MergeAdditiveFloat(base, (*channels)[idx].channel);
        } else if (base->valueType() == ChannelValueType::Color) {
          MergeAdditiveColor(base, (*channels)[idx].channel);
        } else if (base->valueType() == ChannelValueType::Matrix) {
          MergeAdditiveMatrix(base, (*channels)[idx].channel);
        } else if (base->valueType() == ChannelValueType::Bool) {
          MergeAdditiveBool(base, (*channels)[idx].channel);
        }
      } else {
        // additive="replace": the previous segment stays active until the new one starts. Truncate
        // the old track at the handover and append the new keys so both halves survive instead of
        // the earlier segment disappearing.
        auto* next = (*channels)[idx].channel;
        Frame handover = ChannelStartFrame(next);
        if (handover <= ChannelStartFrame(base)) {
          // Both segments start at the same time, so the later one simply replaces the earlier.
          base = next;
          continue;
        }
        // The earlier segment owns everything up to the frame before the handover; the later one
        // starts exactly on it, so the two halves never share a timestamp.
        ApplyEndTimeLimit(base, base->valueType(), handover - 1);
        switch (base->valueType()) {
          case ChannelValueType::Float:
            AppendTypedKeyframes(static_cast<TypedChannel<float>*>(base),
                                 static_cast<TypedChannel<float>*>(next), handover);
            break;
          case ChannelValueType::Color:
            AppendTypedKeyframes(static_cast<TypedChannel<Color>*>(base),
                                 static_cast<TypedChannel<Color>*>(next), handover);
            break;
          case ChannelValueType::Matrix:
            AppendTypedKeyframes(static_cast<TypedChannel<Matrix>*>(base),
                                 static_cast<TypedChannel<Matrix>*>(next), handover);
            break;
          case ChannelValueType::Bool:
            AppendTypedKeyframes(static_cast<TypedChannel<bool>*>(base),
                                 static_cast<TypedChannel<bool>*>(next), handover);
            break;
          default:
            break;
        }
      }
    }
    result.push_back(base);
  }
  return result;
}

// Reads the static (base) value of a Layer-level or content-node channel from the PAGX node tree.
// Used by fill="remove" and begin-offset logic to restore the element's pre-animation state.
// For Color channels, only SolidColor sources are supported; gradients/patterns fall back to
// opaque black.
static KeyValue ReadBaseValue(const ChannelTarget& target) {
  if (target.node == nullptr) {
    return 0.0f;
  }
  auto nodeType = target.node->nodeType();
  if (target.valueType == ChannelValueType::Float) {
    if (nodeType == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(target.node);
      if (target.channelName == "alpha") return layer->alpha;
      if (target.channelName == "x") return layer->x;
      if (target.channelName == "y") return layer->y;
    } else if (nodeType == NodeType::Fill) {
      auto* fill = static_cast<Fill*>(target.node);
      if (target.channelName == "alpha") return fill->alpha;
    } else if (nodeType == NodeType::Stroke) {
      auto* stroke = static_cast<Stroke*>(target.node);
      if (target.channelName == "width") return stroke->width;
      if (target.channelName == "alpha") return stroke->alpha;
      if (target.channelName == "dashOffset") return stroke->dashOffset;
      if (target.channelName == "miterLimit") return stroke->miterLimit;
    } else if (nodeType == NodeType::Ellipse) {
      auto* ellipse = static_cast<Ellipse*>(target.node);
      if (target.channelName == "position.x") return ellipse->position.x;
      if (target.channelName == "position.y") return ellipse->position.y;
      if (target.channelName == "size.width") return ellipse->size.width;
      if (target.channelName == "size.height") return ellipse->size.height;
    } else if (nodeType == NodeType::Rectangle) {
      auto* rect = static_cast<Rectangle*>(target.node);
      if (target.channelName == "position.x") return rect->position.x;
      if (target.channelName == "position.y") return rect->position.y;
      if (target.channelName == "size.width") return rect->size.width;
      if (target.channelName == "size.height") return rect->size.height;
      if (target.channelName == "roundness") return rect->roundness;
    } else if (nodeType == NodeType::Group) {
      auto* group = static_cast<Group*>(target.node);
      if (target.channelName == "position.x") return group->position.x;
      if (target.channelName == "position.y") return group->position.y;
      if (target.channelName == "rotation") return group->rotation;
      if (target.channelName == "scale.x") return group->scale.x;
      if (target.channelName == "scale.y") return group->scale.y;
      if (target.channelName == "skew") return group->skew;
      if (target.channelName == "anchor.x") return group->anchor.x;
      if (target.channelName == "anchor.y") return group->anchor.y;
    } else if (nodeType == NodeType::BlurFilter) {
      auto* f = static_cast<BlurFilter*>(target.node);
      if (target.channelName == "blurX") return f->blurX;
      if (target.channelName == "blurY") return f->blurY;
    } else if (nodeType == NodeType::DropShadowFilter) {
      auto* f = static_cast<DropShadowFilter*>(target.node);
      if (target.channelName == "blurX") return f->blurX;
      if (target.channelName == "blurY") return f->blurY;
      if (target.channelName == "offsetX") return f->offsetX;
      if (target.channelName == "offsetY") return f->offsetY;
    } else if (nodeType == NodeType::InnerShadowFilter) {
      auto* f = static_cast<InnerShadowFilter*>(target.node);
      if (target.channelName == "blurX") return f->blurX;
      if (target.channelName == "blurY") return f->blurY;
      if (target.channelName == "offsetX") return f->offsetX;
      if (target.channelName == "offsetY") return f->offsetY;
    }
    return 0.0f;
  }
  if (target.valueType == ChannelValueType::Color) {
    ColorSource* source = nullptr;
    if (nodeType == NodeType::Fill) {
      source = static_cast<Fill*>(target.node)->color;
    } else if (nodeType == NodeType::Stroke) {
      source = static_cast<Stroke*>(target.node)->color;
    }
    if (source != nullptr && source->nodeType() == NodeType::SolidColor) {
      return static_cast<SolidColor*>(source)->color;
    }
    return Color{0.0f, 0.0f, 0.0f, 1.0f};
  }
  if (target.valueType == ChannelValueType::Bool) {
    if (nodeType == NodeType::Layer && target.channelName == "visible") {
      return static_cast<Layer*>(target.node)->visible;
    }
    return false;
  }
  return 0.0f;
}

// PAGX channels replace the property when evaluated, while an additive="sum" track only carries
// deltas. When a channel has no replace segment to merge into, the target's static value has to be
// folded into the track: without it the target collapses to the delta at frame 0, and an animation
// with begin>0 shows that delta before it starts (the runtime clamps to the first keyframe).
static void FoldStaticBaseIntoAdditiveChannel(Channel* channel, Node* targetNode,
                                              const std::string& targetId) {
  if (channel == nullptr || targetNode == nullptr) {
    return;
  }
  ChannelTarget target = {};
  target.node = targetNode;
  target.nodeId = targetId;
  target.channelName = channel->name;
  target.valueType = channel->valueType();
  const KeyValue baseValue = ReadBaseValue(target);
  switch (channel->valueType()) {
    case ChannelValueType::Float: {
      auto* ch = static_cast<TypedChannel<float>*>(channel);
      if (ch->keyframes.empty()) {
        return;
      }
      const float base = std::get<float>(baseValue);
      for (auto& key : ch->keyframes) {
        key.value += base;
      }
      if (ch->keyframes.front().time > 0) {
        Keyframe<float> key = {};
        key.time = 0;
        key.value = base;
        key.interpolation = KeyframeInterpolationType::Hold;
        ch->keyframes.insert(ch->keyframes.begin(), key);
      }
      break;
    }
    case ChannelValueType::Color: {
      auto* ch = static_cast<TypedChannel<Color>*>(channel);
      if (ch->keyframes.empty()) {
        return;
      }
      const Color base = std::get<Color>(baseValue);
      for (auto& key : ch->keyframes) {
        key.value.red += base.red;
        key.value.green += base.green;
        key.value.blue += base.blue;
        key.value.alpha += base.alpha;
      }
      if (ch->keyframes.front().time > 0) {
        Keyframe<Color> key = {};
        key.time = 0;
        key.value = base;
        key.interpolation = KeyframeInterpolationType::Hold;
        ch->keyframes.insert(ch->keyframes.begin(), key);
      }
      break;
    }
    case ChannelValueType::Bool: {
      auto* ch = static_cast<TypedChannel<bool>*>(channel);
      if (ch->keyframes.empty()) {
        return;
      }
      const bool base = std::get<bool>(baseValue);
      for (auto& key : ch->keyframes) {
        key.value = key.value || base;
      }
      if (ch->keyframes.front().time > 0) {
        Keyframe<bool> key = {};
        key.time = 0;
        key.value = base;
        key.interpolation = KeyframeInterpolationType::Hold;
        ch->keyframes.insert(ch->keyframes.begin(), key);
      }
      break;
    }
    default:
      // Matrix folding needs the layer's full static pose and is applied by buildAnimation;
      // string/int channels have no additive arithmetic.
      break;
  }
}

static bool FloatKeyframeTimeLess(const Keyframe<float>& a, const Keyframe<float>& b) {
  return a.time < b.time;
}

static bool ColorKeyframeTimeLess(const Keyframe<Color>& a, const Keyframe<Color>& b) {
  return a.time < b.time;
}

static bool BoolKeyframeTimeLess(const Keyframe<bool>& a, const Keyframe<bool>& b) {
  return a.time < b.time;
}

static bool MatrixKeyframeTimeLess(const Keyframe<Matrix>& a, const Keyframe<Matrix>& b) {
  return a.time < b.time;
}

// Sorts a channel's keyframes by time after inserting base-value keyframes out of order
// (e.g. a begin-offset Hold keyframe at frame 0 appended after the animation keyframes).
static void SortKeyframes(Channel* channel, ChannelValueType valueType) {
  switch (valueType) {
    case ChannelValueType::Float: {
      auto* ch = static_cast<TypedChannel<float>*>(channel);
      std::sort(ch->keyframes.begin(), ch->keyframes.end(), FloatKeyframeTimeLess);
      break;
    }
    case ChannelValueType::Color: {
      auto* ch = static_cast<TypedChannel<Color>*>(channel);
      std::sort(ch->keyframes.begin(), ch->keyframes.end(), ColorKeyframeTimeLess);
      break;
    }
    case ChannelValueType::Bool: {
      auto* ch = static_cast<TypedChannel<bool>*>(channel);
      std::sort(ch->keyframes.begin(), ch->keyframes.end(), BoolKeyframeTimeLess);
      break;
    }
    case ChannelValueType::Matrix: {
      auto* ch = static_cast<TypedChannel<Matrix>*>(channel);
      std::sort(ch->keyframes.begin(), ch->keyframes.end(), MatrixKeyframeTimeLess);
      break;
    }
    default:
      break;
  }
}

// Forward declaration: AppendKeyframe is defined later in this file but AddBaseValueKeyframes
// (below) needs to call it.
static void AppendKeyframe(Channel* channel, ChannelValueType valueType, Frame time, KeyValue value,
                           KeyframeInterpolationType interpolation, Point bezierOut,
                           Point bezierIn);

// Appends base-value Hold keyframes for fill="remove" and begin-offset semantics, then sorts
// the channel's keyframes by time. Called after all animation keyframes (including repeatCount
// expansion) have been generated.
// - When beginFrames > 0: inserts a Hold keyframe at frame 0 with baseValue so the element
//   shows its static state before the animation starts.
// - When fillFreeze is false (SMIL default "remove"): inserts a Hold keyframe at endFrame+1
//   with baseValue so the element reverts to its static state after the animation ends. The +1
//   offset avoids colliding with the last animation keyframe at endFrame, which would otherwise
//   be shadowed by the base-value keyframe during evaluation.
// An indefinite repeat never reaches its end, so the "revert to base value" keyframe must not be
// emitted: it would sit one frame past the animation and remain active for the rest of the
// timeline (which is longer when other objects run longer), teleporting the target back to its
// static pose mid-loop.
static bool IsIndefiniteRepeat(const std::shared_ptr<DOMNode>& element) {
  if (element == nullptr) {
    return false;
  }
  const auto* repeatCount = element->findAttribute("repeatCount");
  if (repeatCount != nullptr && *repeatCount == "indefinite") {
    return true;
  }
  const auto* repeatDur = element->findAttribute("repeatDur");
  return repeatDur != nullptr && *repeatDur == "indefinite";
}

static void AddBaseValueKeyframes(Channel* channel, ChannelValueType valueType, KeyValue baseValue,
                                  Frame beginFrames, Frame endFrame, bool fillFreeze,
                                  bool additive = false) {
  // An additive channel has no meaningful pre-begin value of its own: its frame-0 base key would
  // become part of the additive segment and shift the merge window back to 0.
  if (beginFrames > 0 && !additive) {
    AppendKeyframe(channel, valueType, 0, baseValue, KeyframeInterpolationType::Hold, {}, {});
  }
  if (!fillFreeze && endFrame > 0) {
    // An additive channel stores deltas, so after the animation ends it must fall back to the
    // identity delta; writing the static base here would make the runtime compose base + base.
    KeyValue endValue = baseValue;
    if (additive) {
      switch (valueType) {
        case ChannelValueType::Float:
          endValue = 0.0f;
          break;
        case ChannelValueType::Color:
          endValue = Color{0.0f, 0.0f, 0.0f, 0.0f};
          break;
        case ChannelValueType::Matrix:
          endValue = Matrix::Identity();
          break;
        default:
          break;
      }
    }
    AppendKeyframe(channel, valueType, endFrame + 1, endValue, KeyframeInterpolationType::Hold, {},
                   {});
  }
  if (beginFrames > 0 || (!fillFreeze && endFrame > 0)) {
    SortKeyframes(channel, valueType);
  }
}

// Finds the first element of the given NodeType in a Layer's contents. Returns nullptr when not
// found.
namespace {

// Depth-first search for the first element of `type`. A referenced leaf shape can sit inside a
// Group (a <use> with x/y wraps it to carry the instance offset), so a flat scan of the layer's
// contents would miss the animation target entirely.
Element* FindElementOfType(Element* element, NodeType type) {
  if (element == nullptr) {
    return nullptr;
  }
  if (element->nodeType() == type) {
    return element;
  }
  if (element->nodeType() != NodeType::Group) {
    return nullptr;
  }
  auto* group = static_cast<Group*>(element);
  for (auto* child : group->elements) {
    if (auto* found = FindElementOfType(child, type)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

Element* SMILAnimationParser::findContentNode(const AnimatedNodeInfo& nodeInfo, NodeType type) {
  if (nodeInfo.targetLayer != nullptr) {
    for (auto* el : nodeInfo.targetLayer->contents) {
      if (auto* found = FindElementOfType(el, type)) {
        return found;
      }
    }
  }
  return nullptr;
}

// Ensures the given content node has an id, allocating one via ctx when missing, and records the
// mapping in nodeInfo.contentNodeIds so buildAnimation can resolve the target later.
std::string SMILAnimationParser::ensureContentNodeId(SVGParserContext& ctx, PAGXDocument* doc,
                                                     Element* node,
                                                     const AnimatedNodeInfo& nodeInfo,
                                                     const char* prefix) {
  if (node == nullptr) {
    return {};
  }
  auto it = nodeInfo.contentNodeIds.find(node);
  if (it != nodeInfo.contentNodeIds.end()) {
    return it->second;
  }
  std::string id = node->id;
  if (id.empty()) {
    id = ctx.generateUniqueId(prefix);
    node->id = id;
  }
  // PAGAnimation::resolveTargets resolves targets through findNode, so a content node carrying an
  // id assigned by the importer still has to be registered for the animation to apply.
  if (doc->findNode(id) != node) {
    detail::DocumentAccess::RegisterNode(*doc, node, id);
  }
  return id;
}

ChannelTarget SMILAnimationParser::resolveAnimateTarget(SVGParserContext& ctx, PAGXDocument* doc,
                                                        const std::string& attributeName,
                                                        const AnimatedNodeInfo& nodeInfo) {
  ChannelTarget target = {};
  // Filter primitive animations: <animate> on feGaussianBlur/feOffset drives the PAGX filter
  // node's blur/offset channels. stdDeviation maps to blurX+blurY (a single animate drives
  // both channels, mirroring how `r` drives both size.width and size.height on a circle); the
  // two-value "sx sy" form is split in parseAnimate's stdDeviation special case.
  if (nodeInfo.targetFilter != nullptr) {
    if (attributeName == "stdDeviation") {
      target.node = nodeInfo.targetFilter;
      target.nodeId = nodeInfo.targetId;
      target.channelName = "blurX";
      target.valueType = ChannelValueType::Float;
      return target;
    }
    if (attributeName == "dx") {
      target.node = nodeInfo.targetFilter;
      target.nodeId = nodeInfo.targetId;
      target.channelName = "offsetX";
      target.valueType = ChannelValueType::Float;
      return target;
    }
    if (attributeName == "dy") {
      target.node = nodeInfo.targetFilter;
      target.nodeId = nodeInfo.targetId;
      target.channelName = "offsetY";
      target.valueType = ChannelValueType::Float;
      return target;
    }
    // Unsupported filter attributeName — fall through to return empty target.
    return target;
  }
  if (attributeName == "opacity") {
    target.node = nodeInfo.targetLayer;
    target.nodeId = nodeInfo.targetId;
    target.channelName = "alpha";
    target.valueType = ChannelValueType::Float;
    return target;
  }
  if (attributeName == "display" || attributeName == "visibility") {
    target.node = nodeInfo.targetLayer;
    target.nodeId = nodeInfo.targetId;
    target.channelName = "visible";
    target.valueType = ChannelValueType::Bool;
    return target;
  }
  if (attributeName == "fill" || attributeName == "fill-opacity") {
    auto* fill = static_cast<Fill*>(findContentNode(nodeInfo, NodeType::Fill));
    target.node = fill;
    target.nodeId = ensureContentNodeId(ctx, doc, fill, nodeInfo, "anim_fill");
    target.channelName = (attributeName == "fill") ? "color" : "alpha";
    target.valueType =
        (attributeName == "fill") ? ChannelValueType::Color : ChannelValueType::Float;
    return target;
  }
  if (attributeName == "stroke" || attributeName == "stroke-opacity" ||
      attributeName == "stroke-width" || attributeName == "stroke-dashoffset" ||
      attributeName == "stroke-miterlimit") {
    auto* stroke = static_cast<Stroke*>(findContentNode(nodeInfo, NodeType::Stroke));
    target.node = stroke;
    target.nodeId = ensureContentNodeId(ctx, doc, stroke, nodeInfo, "anim_stroke");
    if (attributeName == "stroke") {
      target.channelName = "color";
      target.valueType = ChannelValueType::Color;
    } else if (attributeName == "stroke-opacity") {
      target.channelName = "alpha";
      target.valueType = ChannelValueType::Float;
    } else if (attributeName == "stroke-width") {
      target.channelName = "width";
      target.valueType = ChannelValueType::Float;
    } else if (attributeName == "stroke-dashoffset") {
      target.channelName = "dashOffset";
      target.valueType = ChannelValueType::Float;
    } else {
      target.channelName = "miterLimit";
      target.valueType = ChannelValueType::Float;
    }
    return target;
  }
  // cx/cy/r are ellipse-only geometry attributes.
  if (attributeName == "cx" || attributeName == "cy" || attributeName == "r") {
    auto* ellipse = static_cast<Ellipse*>(findContentNode(nodeInfo, NodeType::Ellipse));
    target.node = ellipse;
    target.nodeId = ensureContentNodeId(ctx, doc, ellipse, nodeInfo, "anim_shape");
    if (attributeName == "cx") {
      target.channelName = "position.x";
    } else if (attributeName == "cy") {
      target.channelName = "position.y";
    } else {
      target.channelName = "size.width";
    }
    target.valueType = ChannelValueType::Float;
    return target;
  }
  // x/y/width/height are rectangle-only geometry attributes.
  if (attributeName == "x" || attributeName == "y" || attributeName == "width" ||
      attributeName == "height") {
    auto* rect = static_cast<Rectangle*>(findContentNode(nodeInfo, NodeType::Rectangle));
    target.node = rect;
    target.nodeId = ensureContentNodeId(ctx, doc, rect, nodeInfo, "anim_shape");
    if (attributeName == "x") {
      target.channelName = "position.x";
    } else if (attributeName == "y") {
      target.channelName = "position.y";
    } else if (attributeName == "width") {
      target.channelName = "size.width";
    } else {
      target.channelName = "size.height";
    }
    target.valueType = ChannelValueType::Float;
    return target;
  }
  // rx/ry are ambiguous: on <ellipse> they are radii (size), on <rect> they are corner roundness.
  // Route by the actual content node type so <rect rx> animates roundness instead of being
  // dropped by the ellipse branch above.
  if (attributeName == "rx" || attributeName == "ry") {
    auto* ellipse = static_cast<Ellipse*>(findContentNode(nodeInfo, NodeType::Ellipse));
    if (ellipse != nullptr) {
      target.node = ellipse;
      target.nodeId = ensureContentNodeId(ctx, doc, ellipse, nodeInfo, "anim_shape");
      target.channelName = (attributeName == "rx") ? "size.width" : "size.height";
      target.valueType = ChannelValueType::Float;
      return target;
    }
    auto* rect = static_cast<Rectangle*>(findContentNode(nodeInfo, NodeType::Rectangle));
    target.node = rect;
    target.nodeId = ensureContentNodeId(ctx, doc, rect, nodeInfo, "anim_shape");
    target.channelName = "roundness";
    target.valueType = ChannelValueType::Float;
    return target;
  }
  // Unsupported attributeName (including "transform" which should use animateTransform).
  return target;
}

KeyValue SMILAnimationParser::parseValue(SVGParserContext& ctx, const std::string& str,
                                         ChannelValueType valueType) {
  switch (valueType) {
    case ChannelValueType::Float: {
      return std::strtof(str.c_str(), nullptr);
    }
    case ChannelValueType::Color: {
      return ctx.parseColor(str);
    }
    case ChannelValueType::Bool: {
      // SVG display/visibility use string values ("none"/"inline", "visible"/"hidden") that
      // must map to bool for the Layer.visible channel. "true"/"1" are also accepted.
      return str == "true" || str == "1" || str == "inline" || str == "visible";
    }
    case ChannelValueType::Int: {
      return static_cast<int>(std::strtol(str.c_str(), nullptr, 10));
    }
    default:
      return str;
  }
}

// Creates a TypedChannel<T> for the given value type, populated with the supplied keyframes.
// The keyframe times, values, interpolation modes, and bezier handles must already be aligned.
static Channel* CreateChannelForType(PAGXDocument* doc, ChannelValueType valueType,
                                     const std::string& name) {
  switch (valueType) {
    case ChannelValueType::Float: {
      auto* ch = doc->makeNode<TypedChannel<float>>();
      ch->name = name;
      return ch;
    }
    case ChannelValueType::Bool: {
      auto* ch = doc->makeNode<TypedChannel<bool>>();
      ch->name = name;
      return ch;
    }
    case ChannelValueType::Int: {
      auto* ch = doc->makeNode<TypedChannel<int>>();
      ch->name = name;
      return ch;
    }
    case ChannelValueType::String: {
      auto* ch = doc->makeNode<TypedChannel<std::string>>();
      ch->name = name;
      return ch;
    }
    case ChannelValueType::Color: {
      auto* ch = doc->makeNode<TypedChannel<Color>>();
      ch->name = name;
      return ch;
    }
    case ChannelValueType::Matrix: {
      auto* ch = doc->makeNode<TypedChannel<Matrix>>();
      ch->name = name;
      return ch;
    }
    default:
      return nullptr;
  }
}

// Appends a keyframe to the correct TypedChannel<T> specialization by dispatching on valueType.
// The KeyValue is unpacked into the matching T before storing.
static void AppendKeyframe(Channel* channel, ChannelValueType valueType, Frame time, KeyValue value,
                           KeyframeInterpolationType interpolation, Point bezierOut,
                           Point bezierIn) {
  switch (valueType) {
    case ChannelValueType::Float: {
      auto* ch = static_cast<TypedChannel<float>*>(channel);
      Keyframe<float> key = {};
      key.time = time;
      key.value = std::get<float>(value);
      key.interpolation = interpolation;
      key.bezierOut = bezierOut;
      key.bezierIn = bezierIn;
      ch->keyframes.push_back(key);
      break;
    }
    case ChannelValueType::Bool: {
      auto* ch = static_cast<TypedChannel<bool>*>(channel);
      Keyframe<bool> key = {};
      key.time = time;
      key.value = std::get<bool>(value);
      key.interpolation = interpolation;
      ch->keyframes.push_back(key);
      break;
    }
    case ChannelValueType::Int: {
      auto* ch = static_cast<TypedChannel<int>*>(channel);
      Keyframe<int> key = {};
      key.time = time;
      key.value = std::get<int>(value);
      key.interpolation = interpolation;
      ch->keyframes.push_back(key);
      break;
    }
    case ChannelValueType::String: {
      auto* ch = static_cast<TypedChannel<std::string>*>(channel);
      Keyframe<std::string> key = {};
      key.time = time;
      key.value = std::get<std::string>(value);
      key.interpolation = interpolation;
      ch->keyframes.push_back(key);
      break;
    }
    case ChannelValueType::Color: {
      auto* ch = static_cast<TypedChannel<Color>*>(channel);
      Keyframe<Color> key = {};
      key.time = time;
      key.value = std::get<Color>(value);
      key.interpolation = interpolation;
      key.bezierOut = bezierOut;
      key.bezierIn = bezierIn;
      ch->keyframes.push_back(key);
      break;
    }
    case ChannelValueType::Matrix: {
      auto* ch = static_cast<TypedChannel<Matrix>*>(channel);
      Keyframe<Matrix> key = {};
      key.time = time;
      key.value = std::get<Matrix>(value);
      key.interpolation = interpolation;
      key.bezierOut = bezierOut;
      key.bezierIn = bezierIn;
      ch->keyframes.push_back(key);
      break;
    }
    default:
      break;
  }
}

// Parse an `animateMotion` keyPoints list. Unlike keyTimes the values need not ascend — a motion
// can travel to one end of the path and come back — so only the [0, 1] range is enforced.
static std::vector<double> ParseKeyPointList(const std::string& text) {
  std::vector<double> points = {};
  const char* cursor = text.c_str();
  while (*cursor != '\0') {
    if (*cursor == ';' || std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
      ++cursor;
      continue;
    }
    char* end = nullptr;
    const double value = std::strtod(cursor, &end);
    if (end == cursor || value < -1e-6 || value > 1.0 + 1e-6) {
      return {};
    }
    points.push_back(value);
    cursor = end;
  }
  return points;
}

// SVG `pathLength` normalises dash values into the author's coordinate space. The static
// dasharray/dashoffset are scaled back to user units when the Stroke is created, so animated
// stroke-dashoffset keyframes need the same scale; otherwise the dash phase jumps at frame 0.
void SMILAnimationParser::ScaleDashOffsetKeyframes(const std::shared_ptr<DOMNode>& element,
                                                   std::vector<Channel*>& channels,
                                                   SVGParserContext& ctx) {
  if (element == nullptr) {
    return;
  }
  const std::string path_length = ctx.getAttribute(element, "pathLength");
  const float author_length = path_length.empty() ? 0.0f : strtof(path_length.c_str(), nullptr);
  if (author_length <= 0.0f) {
    return;
  }
  const float real_length = ctx.computePathTotalLength(element);
  if (real_length <= 0.0f) {
    return;
  }
  const float scale = real_length / author_length;
  for (auto* ch : channels) {
    auto* float_channel = static_cast<TypedChannel<float>*>(ch);
    for (auto& key : float_channel->keyframes) {
      key.value *= scale;
    }
  }
}

void SMILAnimationParser::ApplyAttributeUnitConversions(SVGParserContext& ctx, PAGXDocument* doc,
                                                        const std::string& attributeName,
                                                        const AnimatedNodeInfo& nodeInfo,
                                                        const ChannelTarget& target,
                                                        std::vector<Channel*>& channels) {
  if (channels.empty() || target.valueType != ChannelValueType::Float) {
    return;
  }
  auto scaleChannel = [](Channel* channel, float factor, float offset) {
    auto* ch = static_cast<TypedChannel<float>*>(channel);
    for (auto& key : ch->keyframes) {
      key.value = key.value * factor + offset;
    }
  };

  // `r` drives both axes of an ellipse, and the PAGX Ellipse channel stores diameters.
  if (attributeName == "r") {
    auto* heightChannel = CreateChannelForType(doc, ChannelValueType::Float, "size.height");
    if (heightChannel != nullptr) {
      auto* widthChannel = static_cast<TypedChannel<float>*>(channels.front());
      for (const auto& key : widthChannel->keyframes) {
        AppendKeyframe(heightChannel, ChannelValueType::Float, key.time, key.value,
                       key.interpolation, key.bezierOut, key.bezierIn);
      }
      channels.push_back(heightChannel);
    }
    for (auto* channel : channels) {
      scaleChannel(channel, 2.0f, 0.0f);
    }
    return;
  }

  // `rx`/`ry` are radii on an <ellipse>; on a <rect> they drive roundness (a pixel value).
  if ((attributeName == "rx" || attributeName == "ry") && target.node != nullptr &&
      target.node->nodeType() == NodeType::Ellipse) {
    scaleChannel(channels.front(), 2.0f, 0.0f);
    return;
  }

  // SVG rects are anchored at the top-left corner while a PAGX Rectangle is anchored at its
  // center: `x`/`y` values shift by half the static size.
  if (attributeName == "x" || attributeName == "y") {
    auto* rect = static_cast<Rectangle*>(findContentNode(nodeInfo, NodeType::Rectangle));
    if (rect != nullptr) {
      const float offset =
          (attributeName == "x") ? rect->size.width / 2.0f : rect->size.height / 2.0f;
      scaleChannel(channels.front(), 1.0f, offset);
    }
    return;
  }

  if (attributeName != "stroke-dashoffset") {
    return;
  }
  // SVG `pathLength` normalises dash values into the author's coordinate space. The static
  // dasharray/dashoffset are scaled back to user units when the Stroke is created, so animated
  // stroke-dashoffset keyframes need the same scale; otherwise the dash phase jumps at frame 0.
  ScaleDashOffsetKeyframes(nodeInfo.domElement, channels, ctx);
  // <circle>/<ellipse> need the same phase+direction correction that addFillStroke applies to the
  // static Stroke.dashOffset (90-degree phase alignment with SVG's 3 o'clock start + tgfx's
  // reverse-path dashOffset direction). Without it the animated dasharray draws in the mirror
  // direction from the SVG side, producing symmetric arc frames rather than a matching animation.
  auto element = nodeInfo.domElement;
  if (element == nullptr || (element->name != "circle" && element->name != "ellipse")) {
    return;
  }
  float rx = 0.0f;
  float ry = 0.0f;
  if (element->name == "circle") {
    rx = ry = ctx.parseLength(ctx.getAttribute(element, "r"), ctx.getViewBoxWidth());
  } else {
    rx = ctx.parseLength(ctx.getAttribute(element, "rx"), ctx.getViewBoxWidth());
    ry = ctx.parseLength(ctx.getAttribute(element, "ry"), ctx.getViewBoxHeight());
  }
  const float sum = rx + ry;
  if (sum <= 0.0f) {
    return;
  }
  // Ramanujan's first approximation; exact when rx == ry.
  const float h = (rx - ry) * (rx - ry) / (sum * sum);
  const float perimeter =
      static_cast<float>(M_PI) * sum * (1.0f + 3.0f * h / (10.0f + std::sqrt(4.0f - 3.0f * h)));
  const float correction = perimeter / 4.0f;
  for (auto* channel : channels) {
    scaleChannel(channel, 1.0f, -correction);
  }
}

std::vector<Channel*> SMILAnimationParser::parseAnimate(
    SVGParserContext& ctx, PAGXDocument* doc, const std::shared_ptr<DOMNode>& animElement,
    const AnimatedNodeInfo& nodeInfo, float frameRate, const ResolvedTiming& timing,
    Frame& outEndFrame, std::string& outTargetId) {
  std::vector<Channel*> channels = {};
  outTargetId = nodeInfo.targetId;
  if (!animElement) {
    return channels;
  }

  auto attributeName = ctx.getAttribute(animElement, "attributeName");
  if (attributeName.empty()) {
    return channels;
  }

  auto target = resolveAnimateTarget(ctx, doc, attributeName, nodeInfo);
  if (target.node == nullptr) {
    // attributeName not supported (e.g. "transform"); skip this animation.
    return channels;
  }
  outTargetId = target.nodeId;

  // Parse timing attributes.
  auto durStr = ctx.getAttribute(animElement, "dur");
  double durSeconds = parseSMILClockValue(durStr);
  if (!std::isfinite(durSeconds) || durSeconds <= 0.0) {
    // No duration: cannot build keyframes.
    return channels;
  }
  if (durSeconds > kMaxDurationSeconds) {
    durSeconds = kMaxDurationSeconds;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));
  if (durFrames < 1) {
    // A sub-frame duration would collapse every keyframe onto frame 0.
    durFrames = 1;
  }

  // begin comes pre-resolved by buildAnimation (syncbase references like "id.end" solved against
  // their targets). Negative values keep negative keyframe times: the animation started before
  // the timeline origin, so evaluation at time 0 lands mid-animation via the natural clamp.
  Frame beginFrames = static_cast<Frame>(std::round(timing.beginSeconds * frameRate));
  // A negative begin on an indefinitely repeating animation is a phase offset: fold it into the
  // values and start the track at frame 0, matching the transform and motion paths.
  double phaseShift = 0.0;
  if (beginFrames < 0 && durFrames > 0 && IsIndefiniteRepeat(animElement)) {
    phaseShift = std::fmod(static_cast<double>(-beginFrames) / static_cast<double>(durFrames), 1.0);
    beginFrames = 0;
  }

  // Parse values: either explicit `values` list or from/to/by.
  auto valuesStr = ctx.getAttribute(animElement, "values");
  auto fromStr = ctx.getAttribute(animElement, "from");
  auto toStr = ctx.getAttribute(animElement, "to");
  auto byStr = ctx.getAttribute(animElement, "by");

  // to-only/by-only start from the element's current value. That base has to be composed in
  // SVG-attribute space: some PAGX channels use a different unit (radius -> diameter, rect x ->
  // center) and the attribute conversions only run over the finished keyframes, so starting from
  // the runtime value would convert the base a second time.
  auto readAttributeBase = [&ctx, &nodeInfo, &target, &attributeName]() -> KeyValue {
    if (nodeInfo.domElement != nullptr) {
      std::string attribute = ctx.getAttribute(nodeInfo.domElement, attributeName);
      if (!attribute.empty()) {
        return parseValue(ctx, attribute, target.valueType);
      }
    }
    // Attribute not written on the element: colors, alpha and visibility share their unit with the
    // PAGX channel, so the static value is the right base there; geometric attributes default to 0
    // in SVG and are converted together with the animated values.
    if (target.valueType == ChannelValueType::Color || target.valueType == ChannelValueType::Bool ||
        attributeName == "opacity" || attributeName == "fill-opacity" ||
        attributeName == "stroke-opacity" || attributeName == "stroke-width" ||
        attributeName == "stroke-miterlimit") {
      return ReadBaseValue(target);
    }
    return parseValue(ctx, "0", target.valueType);
  };

  std::vector<KeyValue> keyValues = {};
  if (!valuesStr.empty()) {
    auto tokens = SplitSemicolons(valuesStr);
    for (const auto& token : tokens) {
      keyValues.push_back(parseValue(ctx, token, target.valueType));
    }
  } else if (!fromStr.empty() && !toStr.empty()) {
    keyValues.push_back(parseValue(ctx, fromStr, target.valueType));
    keyValues.push_back(parseValue(ctx, toStr, target.valueType));
  } else if (!fromStr.empty() && !byStr.empty()) {
    // to = from + by (numeric only; by is ignored for non-numeric types).
    KeyValue fromValue = parseValue(ctx, fromStr, target.valueType);
    KeyValue byValue = parseValue(ctx, byStr, target.valueType);
    keyValues.push_back(fromValue);
    if (target.valueType == ChannelValueType::Float) {
      keyValues.push_back(std::get<float>(fromValue) + std::get<float>(byValue));
    } else {
      // For non-numeric types, by is not meaningful; fall back to from→to semantics with by as to.
      keyValues.push_back(byValue);
    }
  } else if (!toStr.empty()) {
    // to-only: SMIL animates from the element's current value to `to`.
    keyValues.push_back(readAttributeBase());
    keyValues.push_back(parseValue(ctx, toStr, target.valueType));
  } else if (!byStr.empty()) {
    // by-only: from the base value to base + by (numeric additive types only).
    KeyValue byBase = readAttributeBase();
    KeyValue byValue = parseValue(ctx, byStr, target.valueType);
    keyValues.push_back(byBase);
    if (target.valueType == ChannelValueType::Float) {
      keyValues.push_back(std::get<float>(byBase) + std::get<float>(byValue));
    } else if (target.valueType == ChannelValueType::Color) {
      Color c = std::get<Color>(byBase);
      const Color& by = std::get<Color>(byValue);
      c.red += by.red;
      c.green += by.green;
      c.blue += by.blue;
      c.alpha += by.alpha;
      keyValues.push_back(c);
    } else {
      keyValues.push_back(byValue);
    }
  } else {
    return channels;
  }

  if (keyValues.size() < 2) {
    return channels;
  }

  // Parse keyTimes.
  auto keyTimesStr = ctx.getAttribute(animElement, "keyTimes");
  auto keyTimes = parseKeyTimes(keyTimesStr);
  if (keyTimes.empty()) {
    // Default: uniform distribution in [0, 1].
    keyTimes.resize(keyValues.size());
    for (size_t i = 0; i < keyValues.size(); ++i) {
      keyTimes[i] = static_cast<double>(i) / static_cast<double>(keyValues.size() - 1);
    }
  } else if (keyTimes.size() != keyValues.size()) {
    // Mismatch: fall back to uniform.
    keyTimes.resize(keyValues.size());
    for (size_t i = 0; i < keyValues.size(); ++i) {
      keyTimes[i] = static_cast<double>(i) / static_cast<double>(keyValues.size() - 1);
    }
  }

  // Parse calcMode and keySplines.
  auto calcMode = ctx.getAttribute(animElement, "calcMode");
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;
  std::vector<BezierControlPoints> splines = {};
  if (calcMode == "discrete") {
    interpolation = KeyframeInterpolationType::Hold;
    if (keyTimesStr.empty()) {
      // Each discrete value owns one equal slot. The linear default (i / (n - 1)) would show the
      // last value only at the very end of the animation instead of for the last 1/n of it.
      for (size_t i = 0; i < keyTimes.size(); ++i) {
        keyTimes[i] = static_cast<double>(i) / static_cast<double>(keyTimes.size());
      }
    }
  } else if (calcMode == "spline") {
    interpolation = KeyframeInterpolationType::Bezier;
    splines = parseKeySplines(ctx.getAttribute(animElement, "keySplines"));
  } else if (calcMode == "paced") {
    // Recompute keyTimes based on value distance so the animation progresses at constant speed.
    interpolation = KeyframeInterpolationType::Linear;
    std::vector<double> pacedKeyTimes = {};
    if (ComputePacedKeyTimes(keyValues, target.valueType, &pacedKeyTimes)) {
      keyTimes = pacedKeyTimes;
    }
  }

  // Build the primary channel.
  auto* primaryChannel = CreateChannelForType(doc, target.valueType, target.channelName);
  if (primaryChannel == nullptr) {
    return channels;
  }
  for (size_t i = 0; i < keyValues.size(); ++i) {
    Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
    Point bezierOut = {};
    Point bezierIn = {};
    if (interpolation == KeyframeInterpolationType::Bezier && i < keyValues.size() - 1) {
      if (i < splines.size()) {
        bezierOut = {static_cast<float>(splines[i].p1x), static_cast<float>(splines[i].p1y)};
        bezierIn = {static_cast<float>(splines[i].p2x), static_cast<float>(splines[i].p2y)};
      }
    }
    AppendKeyframe(primaryChannel, target.valueType, time, keyValues[i], interpolation, bezierOut,
                   bezierIn);
  }
  if (target.valueType == ChannelValueType::Float) {
    ShiftSameTimeFloatKeyframes(static_cast<TypedChannel<float>*>(primaryChannel));
  }
  channels.push_back(primaryChannel);

  // Attribute-space keyframe values -> PAGX channel units (radius -> diameter incl. the companion
  // axis, rect corner -> center, dash phase). Shared with <set> so both write the same units.
  ApplyAttributeUnitConversions(ctx, doc, attributeName, nodeInfo, target, channels);

  // Special case: "stdDeviation" on a filter drives both blurX and blurY. SVG's stdDeviation
  // accepts "x" (uniform) or "x y" (per-axis). parseValue only reads the first number, so
  // primaryChannel (blurX) already holds the first value; here we build blurY from the second
  // value (or copy blurX when only one value was given per token). Re-parses the raw values/from
  // /to strings so the second number is not lost.
  if (attributeName == "stdDeviation" && target.valueType == ChannelValueType::Float &&
      target.node != nullptr) {
    auto nt = target.node->nodeType();
    if (nt == NodeType::BlurFilter || nt == NodeType::DropShadowFilter ||
        nt == NodeType::InnerShadowFilter) {
      auto* blurYChannel = CreateChannelForType(doc, ChannelValueType::Float, "blurY");
      if (blurYChannel != nullptr) {
        // Collect the raw value tokens in document order (values, or from+to).
        std::vector<std::string> rawTokens = {};
        if (!valuesStr.empty()) {
          rawTokens = SplitSemicolons(valuesStr);
        } else if (!fromStr.empty() && !toStr.empty()) {
          rawTokens.push_back(fromStr);
          rawTokens.push_back(toStr);
        }
        // rawTokens aligns 1:1 with primaryChannel->keyframes (same order, same count).
        auto* blurXChannel = static_cast<TypedChannel<float>*>(primaryChannel);
        for (size_t i = 0; i < blurXChannel->keyframes.size(); ++i) {
          float yVal = blurXChannel->keyframes[i].value;  // default: same as x
          if (i < rawTokens.size()) {
            auto params = ParseFloatParams(rawTokens[i]);
            if (params.size() >= 2) {
              yVal = params[1];
            }
          }
          const auto& srcKey = blurXChannel->keyframes[i];
          AppendKeyframe(blurYChannel, ChannelValueType::Float, srcKey.time, yVal,
                         srcKey.interpolation, srcKey.bezierOut, srcKey.bezierIn);
        }
        channels.push_back(blurYChannel);
      }
    }
  }

  // Fold the negative-begin phase into the values so the track starts at frame 0.
  if (phaseShift > 0.0) {
    for (auto* ch : channels) {
      if (ch->valueType() == ChannelValueType::Float) {
        ShiftChannelPhase(static_cast<TypedChannel<float>*>(ch), durFrames, phaseShift);
      } else if (ch->valueType() == ChannelValueType::Color) {
        ShiftChannelPhase(static_cast<TypedChannel<Color>*>(ch), durFrames, phaseShift);
      }
    }
  }

  // Expand repeatCount (resolved against repeatDur): integer N replicates the keyframe sequence
  // N times with time offset. accumulate="sum" offsets each repetition's values by
  // k*(lastValue - firstValue). "indefinite" is handled by buildAnimation (sets LoopMode::Loop),
  // not expanded here.
  double repeatCount = ResolveRepeatCount(ctx.getAttribute(animElement, "repeatCount"),
                                          ctx.getAttribute(animElement, "repeatDur"), durSeconds);
  // SMIL ignores accumulate on a to-animation: there is no per-iteration delta to add, the end
  // value is the animation's target.
  const bool toOnly = valuesStr.empty() && fromStr.empty() && !toStr.empty() && byStr.empty();
  bool accumulate = (ctx.getAttribute(animElement, "accumulate") == "sum") && !toOnly;
  if (repeatCount > 1.0) {
    for (auto* ch : channels) {
      ExpandRepeatCount(ch, target.valueType, durFrames, beginFrames, repeatCount, accumulate);
    }
    outEndFrame = beginFrames + static_cast<Frame>(durFrames * repeatCount);
  } else if (repeatCount > 0.0 && repeatCount < 1.0) {
    // A repeatDur shorter than one iteration truncates the single pass.
    Frame limit = beginFrames + static_cast<Frame>(std::round(repeatCount * durFrames));
    for (auto* ch : channels) {
      ApplyEndTimeLimit(ch, target.valueType, limit);
    }
    outEndFrame = limit;
  } else {
    outEndFrame = beginFrames + durFrames;
  }

  // end limits the active duration: keyframes beyond it are truncated and a Hold keyframe with
  // the end-time value is appended, so freeze holds that value and remove reverts from there.
  // endSeconds < 0 means "no end constraint" (see ResolvedTiming). Comparing it directly against a
  // negative begin made the sentinel look like a real end time: begin=-1.3s satisfied -1 > -1.3 and
  // the parser truncated the track to -60, leaving every later sample past the last keyframe.
  if (timing.endSeconds >= 0.0 && timing.endSeconds > timing.beginSeconds) {
    Frame endLimit = static_cast<Frame>(std::round(timing.endSeconds * frameRate));
    if (endLimit < outEndFrame) {
      for (auto* ch : channels) {
        ApplyEndTimeLimit(ch, target.valueType, endLimit);
      }
      outEndFrame = endLimit;
    }
  }

  // Add base-value keyframes for begin-offset and fill="remove" semantics.
  // SMIL default fill is "remove" (element reverts to static state after animation ends).
  // fill="freeze" keeps the last animated value, which is PAGX's natural post-endframe behavior.
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze") || IsIndefiniteRepeat(animElement);
  KeyValue baseValue = ReadBaseValue(target);
  for (auto* ch : channels) {
    AddBaseValueKeyframes(ch, target.valueType, baseValue, beginFrames, outEndFrame, fillFreeze,
                          ctx.getAttribute(animElement, "additive") == "sum");
  }

  // SVG rects are anchored at the top-left corner while a PAGX Rectangle is anchored at its
  // center, so animating size alone would grow the shape from the center. Mirror the size
  // keyframes onto a position channel at the same timestamps to pin the animated edge to the
  // static top-left corner.
  if ((attributeName == "width" || attributeName == "height") &&
      target.valueType == ChannelValueType::Float && !channels.empty()) {
    auto* rect = static_cast<Rectangle*>(findContentNode(nodeInfo, NodeType::Rectangle));
    if (rect != nullptr) {
      const bool horizontal = (attributeName == "width");
      const float anchor = horizontal ? rect->position.x - rect->size.width * 0.5f
                                      : rect->position.y - rect->size.height * 0.5f;
      auto* size_channel = static_cast<TypedChannel<float>*>(channels.front());
      auto* anchor_channel = CreateChannelForType(doc, ChannelValueType::Float,
                                                  horizontal ? "position.x" : "position.y");
      for (const auto& key : size_channel->keyframes) {
        AppendKeyframe(anchor_channel, ChannelValueType::Float, key.time, anchor + key.value * 0.5f,
                       key.interpolation, key.bezierOut, key.bezierIn);
      }
      channels.push_back(anchor_channel);
    }
  }
  return channels;
}
std::vector<Channel*> SMILAnimationParser::parseSet(SVGParserContext& ctx, PAGXDocument* doc,
                                                    const std::shared_ptr<DOMNode>& setElement,
                                                    const AnimatedNodeInfo& nodeInfo,
                                                    float frameRate, const ResolvedTiming& timing,
                                                    Frame& outEndFrame, std::string& outTargetId) {
  std::vector<Channel*> channels = {};
  outTargetId = nodeInfo.targetId;
  if (!setElement) {
    return channels;
  }

  auto attributeName = ctx.getAttribute(setElement, "attributeName");
  if (attributeName.empty()) {
    return channels;
  }

  auto toStr = ctx.getAttribute(setElement, "to");
  if (toStr.empty()) {
    // SMIL 2.0 spec (Section 3.5.2): when <set> has no `to` attribute, "no animation occurs".
    // Skipping is the normative behavior, not a missing fallback.
    return channels;
  }

  auto target = resolveAnimateTarget(ctx, doc, attributeName, nodeInfo);
  if (target.node == nullptr) {
    return channels;
  }
  outTargetId = target.nodeId;

  // begin comes pre-resolved by buildAnimation (syncbase references solved); negative values
  // mean the set applied before the timeline origin.
  Frame beginFrames = static_cast<Frame>(std::round(timing.beginSeconds * frameRate));

  auto durStr = ctx.getAttribute(setElement, "dur");
  double durSeconds = parseSMILClockValue(durStr);

  auto* channel = CreateChannelForType(doc, target.valueType, target.channelName);
  if (channel == nullptr) {
    return channels;
  }

  KeyValue value = parseValue(ctx, toStr, target.valueType);
  AppendKeyframe(channel, target.valueType, beginFrames, value, KeyframeInterpolationType::Hold, {},
                 {});

  // The `to` value is authored in SVG-attribute space, so it needs the same attribute -> channel
  // conversions <animate> applies (radius -> diameter incl. the companion axis, rect corner ->
  // center, dash phase). Running them before the base-value keyframes keeps this track and the
  // static value in the same unit.
  std::vector<Channel*> setChannels = {channel};
  ApplyAttributeUnitConversions(ctx, doc, attributeName, nodeInfo, target, setChannels);

  // Compute end frame and determine fill mode. <set> defaults to fill="remove": when dur is
  // set the element reverts to base value after begin+dur. When dur is absent the <set> effect
  // is permanent (treated as freeze), matching SMIL semantics for indefinite-duration <set>.
  Frame endFrame = beginFrames;
  bool fillFreeze = (durSeconds <= 0.0) || (ctx.getAttribute(setElement, "fill") == "freeze") ||
                    IsIndefiniteRepeat(setElement);
  if (durSeconds > 0.0) {
    endFrame = beginFrames + static_cast<Frame>(std::round(durSeconds * frameRate));
  } else if (timing.endSeconds >= 0.0 && timing.endSeconds > timing.beginSeconds) {
    // A <set> without dur is indefinite, but a finite end still bounds its active duration; with
    // the default fill="remove" the value must revert after that end instead of holding forever.
    endFrame = static_cast<Frame>(std::round(timing.endSeconds * frameRate));
    fillFreeze = false;
  }
  // end limits the active duration: the set value holds until end, then remove reverts.
  // endSeconds < 0 means "no end constraint" (see ResolvedTiming). Comparing it directly against a
  // negative begin made the sentinel look like a real end time: begin=-1.3s satisfied -1 > -1.3 and
  // the parser truncated the track to -60, leaving every later sample past the last keyframe.
  if (timing.endSeconds >= 0.0 && timing.endSeconds > timing.beginSeconds) {
    Frame endLimit = static_cast<Frame>(std::round(timing.endSeconds * frameRate));
    if (endLimit < endFrame) {
      endFrame = endLimit;
    }
  }
  outEndFrame = endFrame;

  // Add base-value keyframes for begin-offset and fill="remove" semantics.
  KeyValue baseValue = ReadBaseValue(target);
  for (auto* setChannel : setChannels) {
    AddBaseValueKeyframes(setChannel, target.valueType, baseValue, beginFrames, endFrame,
                          fillFreeze, ctx.getAttribute(setElement, "additive") == "sum");
  }

  channels = std::move(setChannels);
  return channels;
}

std::vector<Channel*> SMILAnimationParser::parseAnimateTransform(
    SVGParserContext& ctx, PAGXDocument* doc, const std::shared_ptr<DOMNode>& animElement,
    const AnimatedNodeInfo& nodeInfo, float frameRate, const ResolvedTiming& timing,
    Frame& outEndFrame) {
  std::vector<Channel*> channels = {};
  if (!animElement || nodeInfo.targetLayer == nullptr) {
    return channels;
  }

  // The Layer node exposes a runtime "matrix" channel (TypedChannel<Matrix>) that replaces the
  // layer's static matrix when applied, affecting both contents and children. Bake each
  // animateTransform keyframe into a full Matrix and drive the Layer's matrix channel directly —
  // for shape elements as well as <g> elements. This matches SMIL additive="replace" (default)
  // semantics: the baked matrix replaces the layer's static transform. For additive="sum" each
  // keyframe is pre-composed with the layer's static matrix (static * animated) so the animation
  // adds on top of it. Multiple animateTransform elements with additive="sum" are composed by
  // MergeAdditiveMatrix in buildAnimation. Known limitation: matrix decomposition interpolation
  // cannot recover multi-turn rotation (rotate 0->720 collapses to the shortest arc).
  auto type = ctx.getAttribute(animElement, "type");
  if (type.empty()) {
    return channels;
  }

  // Parse values: either explicit `values` list or from/to.
  auto valuesStr = ctx.getAttribute(animElement, "values");
  auto fromStr = ctx.getAttribute(animElement, "from");
  auto toStr = ctx.getAttribute(animElement, "to");

  std::vector<std::vector<float>> paramSets = {};
  if (!valuesStr.empty()) {
    auto tokens = SplitSemicolons(valuesStr);
    for (const auto& token : tokens) {
      paramSets.push_back(ParseFloatParams(token));
    }
  } else if (!fromStr.empty() && !toStr.empty()) {
    paramSets.push_back(ParseFloatParams(fromStr));
    paramSets.push_back(ParseFloatParams(toStr));
  } else {
    return channels;
  }

  if (paramSets.size() < 2) {
    return channels;
  }

  // Parse timing attributes (same logic as parseAnimate, inlined to keep this method
  // self-contained and avoid exposing internal timing structs in the header).
  auto durStr = ctx.getAttribute(animElement, "dur");
  double durSeconds = parseSMILClockValue(durStr);
  if (!std::isfinite(durSeconds) || durSeconds <= 0.0) {
    return channels;
  }
  if (durSeconds > kMaxDurationSeconds) {
    durSeconds = kMaxDurationSeconds;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));
  if (durFrames < 1) {
    // A sub-frame duration would collapse every keyframe onto frame 0.
    durFrames = 1;
  }

  // begin comes pre-resolved by buildAnimation (syncbase references solved). A negative begin on a
  // looping animation is a phase offset rather than a negative timeline: fold it into the sampled
  // parameters so the track starts at frame 0 and covers the whole cycle.
  Frame beginFrames = static_cast<Frame>(std::round(timing.beginSeconds * frameRate));
  double phaseShift = 0.0;
  if (beginFrames < 0 && durFrames > 0 && IsIndefiniteRepeat(animElement)) {
    phaseShift = std::fmod(static_cast<double>(-beginFrames) / static_cast<double>(durFrames), 1.0);
    beginFrames = 0;
  }

  auto keyTimesStr = ctx.getAttribute(animElement, "keyTimes");
  auto keyTimes = parseKeyTimes(keyTimesStr);
  if (keyTimes.empty() || keyTimes.size() != paramSets.size()) {
    keyTimes.resize(paramSets.size());
    for (size_t i = 0; i < paramSets.size(); ++i) {
      keyTimes[i] = static_cast<double>(i) / static_cast<double>(paramSets.size() - 1);
    }
  }

  auto calcMode = ctx.getAttribute(animElement, "calcMode");
  KeyframeInterpolationType interpolation = KeyframeInterpolationType::Linear;
  std::vector<BezierControlPoints> splines = {};
  if (calcMode == "discrete") {
    interpolation = KeyframeInterpolationType::Hold;
  } else if (calcMode == "spline") {
    interpolation = KeyframeInterpolationType::Bezier;
    splines = parseKeySplines(ctx.getAttribute(animElement, "keySplines"));
  }

  // Rotate interpolation on the matrix channel goes through DecomposeAffine/MixDecomposed,
  // which recovers rotation via atan2 confined to (-pi, pi] and interpolates the shortest arc.
  // A Linear keyframe pair whose angle delta exceeds 180 degrees therefore loses winding:
  // 0->360 bakes both endpoints to the identity matrix (the tween never rotates), and 0->270
  // is recovered as -90 (reversed direction). Split such segments into <=180-degree steps at
  // bake time so every interpolated step stays inside the shortest-arc window; time is split
  // proportionally to keep the angular velocity constant, and rotation centers (when the
  // three-parameter form is used) are linearly interpolated across the inserted keyframes.
  // Only Linear interpolation is split: Bezier-eased multi-turn rotations stay unsplit (rare;
  // splitting would need spline subdivision) and keep the shortest-arc limitation, while Hold
  // never interpolates so it has no winding to lose.
  if (type == "rotate" && interpolation == KeyframeInterpolationType::Linear &&
      paramSets.size() >= 2) {
    std::vector<std::vector<float>> expandedParams = {};
    std::vector<double> expandedTimes = {};
    expandedParams.reserve(paramSets.size() * 2);
    expandedTimes.reserve(paramSets.size() * 2);
    expandedParams.push_back(paramSets.front());
    expandedTimes.push_back(keyTimes.front());
    for (size_t i = 1; i < paramSets.size(); ++i) {
      float prevAngle = (paramSets[i - 1].size() > 0) ? paramSets[i - 1][0] : 0.0f;
      float curAngle = (paramSets[i].size() > 0) ? paramSets[i][0] : 0.0f;
      float delta = curAngle - prevAngle;
      if (std::abs(delta) < 180.0f) {
        expandedParams.push_back(paramSets[i]);
        expandedTimes.push_back(keyTimes[i]);
        continue;
      }
      float prevCX = (paramSets[i - 1].size() >= 3) ? paramSets[i - 1][1] : 0.0f;
      float prevCY = (paramSets[i - 1].size() >= 3) ? paramSets[i - 1][2] : 0.0f;
      float curCX = (paramSets[i].size() >= 3) ? paramSets[i][1] : 0.0f;
      float curCY = (paramSets[i].size() >= 3) ? paramSets[i][2] : 0.0f;
      bool hasCenter = paramSets[i - 1].size() >= 3 || paramSets[i].size() >= 3;
      // The step bound is 179 rather than 180 degrees: an inserted keyframe at exactly 180
      // degrees sits on the atan2 ±pi boundary, where float sin(pi) is a tiny negative number,
      // the keyframe decomposes to -pi instead of +pi, and the shortest-arc wrap (which uses
      // strict inequality) leaves the diff unwrapped — reversing that step's direction.
      // 179 degrees only guards winding loss; linear matrix interpolation still cuts a chord
      // across the arc between keys, so a 120-degree step renders the shape inside its orbit
      // (radius shrinks up to 13%). Cap the step at 20 degrees as well: the chord sagitta stays
      // sub-pixel for typical orbit radii, and a full turn becomes 18 keys.
      constexpr float kMaxRotateStep = 20.0f;
      int steps = static_cast<int>(std::ceil(std::abs(delta) / kMaxRotateStep));
      if (steps > kMaxRotationSteps) {
        steps = kMaxRotationSteps;
      }
      for (int k = 1; k <= steps; ++k) {
        float t = static_cast<float>(k) / static_cast<float>(steps);
        std::vector<float> params = {};
        params.push_back(prevAngle + delta * t);
        if (hasCenter) {
          params.push_back(prevCX + (curCX - prevCX) * t);
          params.push_back(prevCY + (curCY - prevCY) * t);
        }
        expandedParams.push_back(std::move(params));
        expandedTimes.push_back(keyTimes[i - 1] + (keyTimes[i] - keyTimes[i - 1]) * t);
      }
    }
    paramSets = std::move(expandedParams);
    keyTimes = std::move(expandedTimes);
  }

  if (phaseShift > 0.0 && !paramSets.empty()) {
    std::vector<std::vector<float>> resampled = {};
    resampled.reserve(paramSets.size());
    for (size_t i = 0; i < paramSets.size(); ++i) {
      double u = keyTimes[i] + phaseShift;
      u -= std::floor(u);
      auto params = SampleTransformParams(paramSets, keyTimes, u);
      if (params.empty()) {
        params = paramSets[i];
      }
      resampled.push_back(std::move(params));
    }
    paramSets = std::move(resampled);
  }

  // Bake each keyframe's transform params into a full Matrix on a single "matrix" channel
  // targeting the Layer. The runtime evaluates Matrix keyframes with decomposition interpolation
  // (LerpKeyframeValue<Matrix>) and applies keySplines bezier easing through the same
  // EvaluateKeyframeSequence path as scalar channels.
  bool additive = (ctx.getAttribute(animElement, "additive") == "sum");
  const auto& staticMatrix = nodeInfo.targetLayer->matrix;
  auto* ch = CreateChannelForType(doc, ChannelValueType::Matrix, "matrix");
  // SVG transform animations rotate and scale about the element's anchor, so the transient
  // translation must keep that fixed point. CSS-style matrix animations keep the linear spelling;
  // motion samples interpolate linearly between path samples and do not opt in.
  ch->pivotInterpolation = true;
  for (size_t i = 0; i < paramSets.size(); ++i) {
    Matrix m = TransformParamsToMatrix(type, paramSets[i]);
    if (additive) {
      // SMIL additive="sum": the animated transform post-multiplies the base value. The static pose
      // is applied once after all sum tracks are merged; baking it per track would multiply it in
      // repeatedly (S*A*S*B) and the merge cannot divide it back out for singular matrices.
    } else if (ctx.contentMatrixLayer() == nodeInfo.targetLayer) {
      // The viewBox mapping was pre-multiplied onto this very layer. A replaced pose must keep it,
      // otherwise the animation drops the viewBox scale/offset as soon as it starts. When the
      // animated layer was hoisted under a pivot parent, the mapping lives on that parent and
      // applying it here would square it.
      m = ctx.getContentMatrix() * m;
    }
    Point bo = {}, bi = {};
    ComputeBezierOutHandle(interpolation, splines, i, paramSets.size(), &bo);
    ComputeBezierInHandle(interpolation, splines, i, &bi);
    Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
    AppendKeyframe(ch, ChannelValueType::Matrix, time, m, interpolation, bo, bi);
  }
  channels.push_back(ch);

  double repeatCount = ResolveRepeatCount(ctx.getAttribute(animElement, "repeatCount"),
                                          ctx.getAttribute(animElement, "repeatDur"), durSeconds);
  if (repeatCount > 1.0) {
    ExpandRepeatCount(ch, ChannelValueType::Matrix, durFrames, beginFrames, repeatCount, false);
    outEndFrame = beginFrames + static_cast<Frame>(durFrames * repeatCount);
  } else if (repeatCount > 0.0 && repeatCount < 1.0) {
    // A repeatDur shorter than one iteration truncates the single pass.
    Frame limit = beginFrames + static_cast<Frame>(std::round(repeatCount * durFrames));
    ApplyEndTimeLimit(ch, ChannelValueType::Matrix, limit);
    outEndFrame = limit;
  } else {
    outEndFrame = beginFrames + durFrames;
  }

  // end limits the active duration: matrix keyframes beyond it are truncated with a Hold
  // keyframe carrying the end-time value.
  // endSeconds < 0 means "no end constraint" (see ResolvedTiming). Comparing it directly against a
  // negative begin made the sentinel look like a real end time: begin=-1.3s satisfied -1 > -1.3 and
  // the parser truncated the track to -60, leaving every later sample past the last keyframe.
  if (timing.endSeconds >= 0.0 && timing.endSeconds > timing.beginSeconds) {
    Frame endLimit = static_cast<Frame>(std::round(timing.endSeconds * frameRate));
    if (endLimit < outEndFrame) {
      ApplyEndTimeLimit(ch, ChannelValueType::Matrix, endLimit);
      outEndFrame = endLimit;
    }
  }

  // Base value is the layer's static matrix so fill="remove" reverts to the pre-animation
  // transform and begin-offset holds the static state before the animation starts.
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze") || IsIndefiniteRepeat(animElement);
  AddBaseValueKeyframes(ch, ChannelValueType::Matrix, staticMatrix, beginFrames, outEndFrame,
                        fillFreeze, additive);
  return channels;
}

// Ramer–Douglas–Peucker over sampled path positions: keeps the curvature-defining samples and
// drops the ones that lie within `tolerance` of the chord, so a smooth arc does not need a keyframe
// every few units.
static void SimplifyMotionSamples(const std::vector<tgfx::Point>& points, size_t first, size_t last,
                                  float tolerance, std::vector<char>* keep) {
  if (last <= first + 1) {
    return;
  }
  const tgfx::Point& a = points[first];
  const tgfx::Point& b = points[last];
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float lengthSquared = dx * dx + dy * dy;
  float maxDistance = 0.0f;
  size_t farthest = first;
  for (size_t i = first + 1; i < last; ++i) {
    const tgfx::Point& p = points[i];
    float distance = 0.0f;
    if (lengthSquared <= 1e-6f) {
      distance = std::hypot(p.x - a.x, p.y - a.y);
    } else {
      // Distance to the segment, not to its infinite line: a path that folds back on itself
      // (M0 0 L100 0 L20 0) puts its extreme point on the line's extension, and measuring against
      // the extension would drop that point and flatten the turn.
      float t = (dx * (p.x - a.x) + dy * (p.y - a.y)) / lengthSquared;
      t = std::max(0.0f, std::min(1.0f, t));
      distance = std::hypot(p.x - (a.x + dx * t), p.y - (a.y + dy * t));
    }
    if (distance > maxDistance) {
      maxDistance = distance;
      farthest = i;
    }
  }
  if (maxDistance <= tolerance || farthest == first) {
    return;
  }
  (*keep)[farthest] = 1;
  SimplifyMotionSamples(points, first, farthest, tolerance, keep);
  SimplifyMotionSamples(points, farthest, last, tolerance, keep);
}

std::vector<Channel*> SMILAnimationParser::parseAnimateMotion(
    SVGParserContext& ctx, PAGXDocument* doc, const std::shared_ptr<DOMNode>& animElement,
    const AnimatedNodeInfo& nodeInfo, float frameRate, const ResolvedTiming& timing,
    Frame& outEndFrame) {
  std::vector<Channel*> channels = {};
  if (!animElement || nodeInfo.targetLayer == nullptr) {
    return channels;
  }

  // Like parseAnimateTransform, motion animations bake path position (and optional rotation)
  // into a Matrix per keyframe on the Layer's runtime "matrix" channel — for shape elements as
  // well as <g> elements. additive="sum" pre-composes the layer's static matrix.

  // Resolve the motion path: either the `path` attribute on <animateMotion> itself, or an
  // <mpath href="#id"> child referencing a <path> element in defs.
  std::string pathStr = ctx.getAttribute(animElement, "path");
  if (pathStr.empty()) {
    auto child = animElement->getFirstChild();
    while (child) {
      if (child->name == "mpath") {
        std::string href = ctx.getHrefAttribute(child);
        std::string refId = ctx.resolveUrl(href);
        auto definition = ctx.findDefinition(refId);
        if (definition != nullptr) {
          pathStr = ctx.getAttribute(definition, "d");
        }
        break;
      }
      child = child->getNextSibling();
    }
  }
  if (pathStr.empty()) {
    return channels;
  }

  // Parse timing.
  auto durStr = ctx.getAttribute(animElement, "dur");
  double durSeconds = parseSMILClockValue(durStr);
  if (!std::isfinite(durSeconds) || durSeconds <= 0.0) {
    return channels;
  }
  if (durSeconds > kMaxDurationSeconds) {
    durSeconds = kMaxDurationSeconds;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));
  if (durFrames < 1) {
    // A sub-frame duration would collapse every keyframe onto frame 0.
    durFrames = 1;
  }

  // begin comes pre-resolved by buildAnimation (syncbase references solved).
  Frame beginFrames = static_cast<Frame>(std::round(timing.beginSeconds * frameRate));
  // A negative begin on a looping animation is a phase offset, not a shift of the whole timeline:
  // keeping the keyframes at negative times leaves every sample after the last keyframe, so the
  // player clamps to it and the element freezes. Fold the offset into the path progress and start
  // the track at frame 0 so the loop covers the full cycle.
  double phaseShift = 0.0;
  if (beginFrames < 0 && durFrames > 0 && IsIndefiniteRepeat(animElement)) {
    phaseShift = std::fmod(static_cast<double>(-beginFrames) / static_cast<double>(durFrames), 1.0);
    beginFrames = 0;
  }

  // Build tgfx::Path from the SVG path data string.
  auto pathData = PathDataFromSVGString(pathStr);
  tgfx::Path tgfxPath = ToTGFX(pathData);
  auto pathMeasure = tgfx::PathMeasure::MakeFrom(tgfxPath);
  if (pathMeasure == nullptr) {
    return channels;
  }
  float totalLength = pathMeasure->getLength();
  if (totalLength <= 0.0f) {
    return channels;
  }

  // Parse keyPoints: optional list of normalized [0,1] offsets along the path, one per keyframe.
  // When absent, path positions are sampled uniformly.
  // keyPoints may move back and forth along the path (a knob that travels to one end and
  // returns), so this list is parsed without the monotonic requirement of keyTimes.
  std::vector<double> keyPoints = ParseKeyPointList(ctx.getAttribute(animElement, "keyPoints"));
  // A single keyPoint cannot describe a motion track: drop it and fall back to uniform sampling.
  if (keyPoints.size() < 2) {
    keyPoints.clear();
  }
  // keyTimes places the keyPoints samples on the timeline; a mismatched list falls back to an even
  // distribution instead of reading past the end.
  std::vector<double> motionKeyTimes = {};
  if (!keyPoints.empty()) {
    motionKeyTimes = parseKeyTimes(ctx.getAttribute(animElement, "keyTimes"));
    if (motionKeyTimes.size() != keyPoints.size()) {
      motionKeyTimes.clear();
    }
  }

  // Control poses of the motion: each one anchors a path progress to a normalized time. Without
  // keyPoints the element sweeps the whole path once; with keyPoints each authored point pins one
  // progress/time pair, and the span between two of them is sampled along the path (below).
  struct MotionControl {
    double progress = 0.0;
    double time = 0.0;
  };
  std::vector<MotionControl> controls = {};
  if (keyPoints.empty()) {
    // A loop phase shift slides the whole sweep along the path. Progress keeps advancing past 1.0
    // so the sequence stays monotonic across the wrap; the distance is taken modulo the path
    // length, and without a shift the final sample must stay on the path end rather than wrap.
    controls.push_back({phaseShift, 0.0});
    controls.push_back({phaseShift + 1.0, 1.0});
  } else {
    for (size_t i = 0; i < keyPoints.size(); ++i) {
      const double time = motionKeyTimes.empty()
                              ? static_cast<double>(i) / static_cast<double>(keyPoints.size() - 1)
                              : motionKeyTimes[i];
      controls.push_back({keyPoints[i], time});
    }
  }

  // Parse rotate attribute: "auto" / "auto-reverse" / <number> (default "0").
  std::string rotateStr = ctx.getAttribute(animElement, "rotate", "0");
  bool rotateAuto = (rotateStr == "auto");
  bool rotateAutoReverse = (rotateStr == "auto-reverse");
  float fixedRotation = 0.0f;
  bool hasFixedRotation = false;
  if (!rotateAuto && !rotateAutoReverse) {
    fixedRotation = std::strtof(rotateStr.c_str(), nullptr);
    hasFixedRotation = true;
  }

  // Bake each path sample into Translate(pos) * Rotate(angle) (rotation only when auto/auto-
  // reverse; fixed rotation is also baked into each keyframe's matrix). A single "matrix"
  // channel targets the Layer; fill="remove" reverts to the layer's static matrix.
  bool additive = (ctx.getAttribute(animElement, "additive") == "sum");
  const auto& staticMatrix = nodeInfo.targetLayer->matrix;
  auto* ch = CreateChannelForType(doc, ChannelValueType::Matrix, "matrix");
  struct MotionKey {
    Frame time = 0;
    Matrix matrix = {};
    tgfx::Point position = {};
    /// Authored control pose: kept through simplification so the curve is pinned at these points.
    bool control = false;
  };
  std::vector<MotionKey> samples;
  for (size_t segment = 0; segment + 1 < controls.size(); ++segment) {
    const auto& start = controls[segment];
    const auto& end = controls[segment + 1];
    const double deltaProgress = end.progress - start.progress;
    // Adaptive density: one sample per 10 user units of the span, clamped so a short hop still has
    // enough samples to describe a curve while a long sweep does not explode into thousands of keys.
    double spanLength = std::abs(deltaProgress) * static_cast<double>(totalLength);
    int steps = static_cast<int>(std::ceil(spanLength / 10.0));
    if (steps < 8) {
      steps = 8;
    }
    if (steps > 128) {
      steps = 128;
    }
    // The first sample of every later span repeats the previous span's endpoint.
    for (int i = (segment == 0 ? 0 : 1); i <= steps; ++i) {
      const double u = static_cast<double>(i) / static_cast<double>(steps);
      const double progress = start.progress + deltaProgress * u;
      double pathProgress = phaseShift > 0.0 ? progress - std::floor(progress) : progress;
      if (pathProgress < 0.0) {
        pathProgress = 0.0;
      } else if (pathProgress > 1.0) {
        pathProgress = 1.0;
      }
      const float distance = static_cast<float>(pathProgress) * totalLength;
      tgfx::Point pos = {};
      tgfx::Point tan = {};
      if (!pathMeasure->getPosTan(distance, &pos, &tan)) {
        continue;
      }
      Matrix m = Matrix::Translate(pos.x, pos.y);
      if (rotateAuto || rotateAutoReverse) {
        float angle = atan2f(tan.y, tan.x) * 180.0f / static_cast<float>(M_PI);
        if (rotateAutoReverse) {
          angle += 180.0f;
        }
        m = m * Matrix::Rotate(angle);
      } else if (hasFixedRotation) {
        m = m * Matrix::Rotate(fixedRotation);
      }
      if (!additive) {
        // animateMotion supplements the element's own transform even without additive="sum"; SVG
        // never lets a motion path replace the base transform. staticMatrix already carries the
        // viewBox mapping, so no separate content-matrix step is needed here.
        m = staticMatrix * m;
      }
      const double sampleTime = start.time + (end.time - start.time) * u;
      Frame time = beginFrames + static_cast<Frame>(std::round(sampleTime * durFrames));
      const bool isControl = (segment == 0 && i == 0) || (i == steps);
      if (!samples.empty() && samples.back().time == time) {
        // Several samples can round onto one frame; an authored control pose wins over a sampled
        // interior key, and the later sample wins otherwise.
        if (isControl || !samples.back().control) {
          samples.back() = {time, m, pos, isControl};
        }
        continue;
      }
      samples.push_back({time, m, pos, isControl});
    }
  }
  if (samples.size() >= 3) {
    std::vector<tgfx::Point> positions;
    positions.reserve(samples.size());
    std::vector<char> keep(samples.size(), 0);
    for (size_t i = 0; i < samples.size(); ++i) {
      positions.push_back(samples[i].position);
      if (samples[i].control) {
        keep[i] = 1;
      }
    }
    keep.front() = 1;
    keep.back() = 1;
    // Simplify each span between two control poses: interior samples that stay within a pixel of
    // the chord are dropped, the authored poses themselves never are.
    size_t spanStart = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
      if (keep[i] == 0) {
        continue;
      }
      SimplifyMotionSamples(positions, spanStart, i, 1.0f, &keep);
      spanStart = i;
    }
    std::vector<MotionKey> simplified;
    simplified.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
      if (keep[i]) {
        simplified.push_back(samples[i]);
      }
    }
    if (simplified.size() >= 2) {
      samples = std::move(simplified);
    }
  }
  for (const auto& sample : samples) {
    AppendKeyframe(ch, ChannelValueType::Matrix, sample.time, sample.matrix,
                   KeyframeInterpolationType::Linear, {}, {});
  }
  channels.push_back(ch);

  double repeatCount = ResolveRepeatCount(ctx.getAttribute(animElement, "repeatCount"),
                                          ctx.getAttribute(animElement, "repeatDur"), durSeconds);
  if (repeatCount > 1.0) {
    ExpandRepeatCount(ch, ChannelValueType::Matrix, durFrames, beginFrames, repeatCount, false);
    outEndFrame = beginFrames + static_cast<Frame>(durFrames * repeatCount);
  } else if (repeatCount > 0.0 && repeatCount < 1.0) {
    // A repeatDur shorter than one iteration truncates the single pass.
    Frame limit = beginFrames + static_cast<Frame>(std::round(repeatCount * durFrames));
    ApplyEndTimeLimit(ch, ChannelValueType::Matrix, limit);
    outEndFrame = limit;
  } else {
    outEndFrame = beginFrames + durFrames;
  }
  // end limits the active duration: matrix keyframes beyond it are truncated with a Hold
  // keyframe carrying the end-time value.
  // endSeconds < 0 means "no end constraint" (see ResolvedTiming). Comparing it directly against a
  // negative begin made the sentinel look like a real end time: begin=-1.3s satisfied -1 > -1.3 and
  // the parser truncated the track to -60, leaving every later sample past the last keyframe.
  if (timing.endSeconds >= 0.0 && timing.endSeconds > timing.beginSeconds) {
    Frame endLimit = static_cast<Frame>(std::round(timing.endSeconds * frameRate));
    if (endLimit < outEndFrame) {
      ApplyEndTimeLimit(ch, ChannelValueType::Matrix, endLimit);
      outEndFrame = endLimit;
    }
  }
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze") || IsIndefiniteRepeat(animElement);
  AddBaseValueKeyframes(ch, ChannelValueType::Matrix, staticMatrix, beginFrames, outEndFrame,
                        fillFreeze, additive);
  return channels;
}

// An element with repeatCount="indefinite" only carries one cycle of keyframes, while the timeline
// runs for the longest animation in the document (a 3s orbit next to a 4s spin lasts 4s). Tile the
// cycle so the object keeps moving until the timeline loops instead of freezing on its last pose.
template <typename T>
static void TileTypedChannel(TypedChannel<T>* channel, Frame period, Frame maxEnd) {
  if (channel == nullptr || period <= 0) {
    return;
  }
  // A tiled copy of the first keyframe lands exactly on the cycle's last keyframe, and runtime
  // evaluation rejects a channel whose keyframe times are not unique (the whole track then falls
  // back to the static pose, which looks like "nothing animates"). Drop such duplicates.
  std::unordered_set<Frame> usedTimes;
  for (const auto& keyframe : channel->keyframes) {
    usedTimes.insert(keyframe.time);
  }
  const auto base = channel->keyframes;
  for (Frame offset = period; offset < maxEnd; offset += period) {
    if (channel->keyframes.size() >= kMaxTiledKeyframes) {
      break;
    }
    for (const auto& source : base) {
      Frame time = source.time + offset;
      if (time > maxEnd) {
        break;
      }
      if (!usedTimes.insert(time).second) {
        continue;
      }
      auto copy = source;
      copy.time = time;
      channel->keyframes.push_back(copy);
    }
  }
}

static void TileChannelKeyframes(Channel* channel, Frame period, Frame maxEnd) {
  if (channel == nullptr) {
    return;
  }
  switch (channel->valueType()) {
    case ChannelValueType::Float:
      TileTypedChannel(static_cast<TypedChannel<float>*>(channel), period, maxEnd);
      break;
    case ChannelValueType::Color:
      TileTypedChannel(static_cast<TypedChannel<Color>*>(channel), period, maxEnd);
      break;
    case ChannelValueType::Matrix:
      TileTypedChannel(static_cast<TypedChannel<Matrix>*>(channel), period, maxEnd);
      break;
    case ChannelValueType::Bool:
      TileTypedChannel(static_cast<TypedChannel<bool>*>(channel), period, maxEnd);
      break;
    default:
      break;
  }
}

Frame SMILAnimationParser::RepeatPeriodFrames(SVGParserContext& ctx,
                                              const std::shared_ptr<DOMNode>& element,
                                              float frameRate) {
  auto durSeconds = parseSMILClockValue(ctx.getAttribute(element, "dur"));
  if (!std::isfinite(durSeconds) || durSeconds <= 0.0) {
    return 0;
  }
  return static_cast<Frame>(std::round(durSeconds * frameRate));
}

// Orders the per-element animation entries by their first target id so the emitted
// AnimationObject list does not depend on unordered_map iteration order.
struct AnimatedEntryLess {
  using Entry = std::pair<const DOMNode* const, std::vector<AnimatedNodeInfo>>;

  bool operator()(const Entry* a, const Entry* b) const {
    const std::string& left = a->second.empty() ? EmptyId() : a->second.front().targetId;
    const std::string& right = b->second.empty() ? EmptyId() : b->second.front().targetId;
    return left < right;
  }

 private:
  static const std::string& EmptyId() {
    static const std::string empty = {};
    return empty;
  }
};

Animation* SMILAnimationParser::buildAnimation(
    SVGParserContext& ctx, PAGXDocument* doc,
    const std::unordered_map<const DOMNode*, SMILAnimationGroup>& smilAnimations,
    const std::unordered_map<const DOMNode*, std::vector<AnimatedNodeInfo>>& animatedNodeMap,
    float frameRate) {
  if (animatedNodeMap.empty()) {
    return nullptr;
  }

  auto* animation = doc->makeNode<Animation>();
  animation->frameRate = frameRate;
  animation->loop = LoopMode::Once;
  Frame maxEndFrame = 0;
  bool hasIndefiniteLoop = false;
  // Objects whose SMIL element repeats forever. Their keyframes only cover one cycle, so they are
  // tiled up to the timeline duration once it is known (periods differ per element: 3s vs 4s).
  struct InfiniteObjectEntry {
    AnimationObject* object = nullptr;
    Frame period = 0;
  };
  std::vector<InfiniteObjectEntry> infiniteObjects = {};

  // Solve begin/end timing for every SMIL element up front (syncbase references like
  // begin="id.end" need cross-element dependency resolution before any parsing).
  auto timings = ResolveAllTimings(ctx, smilAnimations);
  ResolvedTiming defaultTiming = {};
  // A syncbase cycle repeats by construction (each animation restarts the other), so the whole
  // timeline loops instead of playing a single lap.
  for (const auto& entry : timings) {
    if (entry.second.cyclic) {
      hasIndefiniteLoop = true;
      break;
    }
  }

  // The map is unordered, so walk it in target-id order: the AnimationObject list has to be
  // reproducible, and per-target ordering alone does not pin the cross-element order.
  std::vector<const AnimatedEntryLess::Entry*> nodeEntries = {};
  nodeEntries.reserve(animatedNodeMap.size());
  for (const auto& entry : animatedNodeMap) {
    nodeEntries.push_back(&entry);
  }
  std::sort(nodeEntries.begin(), nodeEntries.end(), AnimatedEntryLess{});

  for (const auto* nodeEntry : nodeEntries) {
    const auto& pair = *nodeEntry;
    auto smilIt = smilAnimations.find(pair.first);
    if (smilIt == smilAnimations.end()) {
      continue;
    }
    const auto& group = smilIt->second;
    for (const auto& nodeInfo : pair.second) {
      bool elementIndefinite = false;

      // Collect <animate>/<set> channels grouped by their actual target node id. A single SVG
      // element may carry animations that drive different PAGX nodes (e.g. opacity → Layer,
      // stroke-dashoffset → Stroke), so we must create one AnimationObject per target.
      struct ChannelEntry {
        Channel* channel = nullptr;
        bool additive = false;
        std::string targetId = {};
      };
      std::vector<ChannelEntry> layerEntries = {};
      Frame layerEndFrame = 0;
      Frame indefinitePeriod = 0;

      for (const auto& animEl : group.animates) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(animEl, "additive") == "sum");
        std::string targetId;
        auto parsedChannels =
            parseAnimate(ctx, doc, animEl, nodeInfo, frameRate,
                         timings.count(animEl.get()) ? timings[animEl.get()] : defaultTiming,
                         endFrame, targetId);
        for (auto* ch : parsedChannels) {
          layerEntries.push_back({ch, additive, targetId});
        }
        if (endFrame > layerEndFrame) {
          layerEndFrame = endFrame;
        }
        if (IsIndefiniteRepeat(animEl)) {
          hasIndefiniteLoop = true;
          elementIndefinite = true;
          if (indefinitePeriod <= 0) {
            indefinitePeriod = RepeatPeriodFrames(ctx, animEl, frameRate);
          }
        }
      }
      for (const auto& setEl : group.sets) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(setEl, "additive") == "sum");
        std::string targetId;
        auto parsedChannels = parseSet(
            ctx, doc, setEl, nodeInfo, frameRate,
            timings.count(setEl.get()) ? timings[setEl.get()] : defaultTiming, endFrame, targetId);
        for (auto* ch : parsedChannels) {
          layerEntries.push_back({ch, additive, targetId});
        }
        if (endFrame > layerEndFrame) {
          layerEndFrame = endFrame;
        }
      }

      // animateTransform/animateMotion bake Matrix keyframes onto the Layer's runtime "matrix"
      // channel. Collect them into layerEntries so they share the Layer-targeted AnimationObject
      // with <animate>/<set> channels (e.g. opacity), and MergeAdditiveChannels composes multiple
      // additive matrix channels.
      if (nodeInfo.targetLayer != nullptr &&
          (!group.animateTransforms.empty() || !group.animateMotions.empty())) {
        for (const auto& transformEl : group.animateTransforms) {
          Frame endFrame = 0;
          bool additive = (ctx.getAttribute(transformEl, "additive") == "sum");
          auto parsedChannels = parseAnimateTransform(
              ctx, doc, transformEl, nodeInfo, frameRate,
              timings.count(transformEl.get()) ? timings[transformEl.get()] : defaultTiming,
              endFrame);
          for (auto* ch : parsedChannels) {
            layerEntries.push_back({ch, additive, nodeInfo.targetId});
          }
          if (endFrame > layerEndFrame) {
            layerEndFrame = endFrame;
          }
          if (IsIndefiniteRepeat(transformEl)) {
            hasIndefiniteLoop = true;
            elementIndefinite = true;
            if (indefinitePeriod <= 0) {
              indefinitePeriod = RepeatPeriodFrames(ctx, transformEl, frameRate);
            }
          }
        }
        for (const auto& motionEl : group.animateMotions) {
          Frame endFrame = 0;
          bool additive = (ctx.getAttribute(motionEl, "additive") == "sum");
          auto parsedChannels = parseAnimateMotion(
              ctx, doc, motionEl, nodeInfo, frameRate,
              timings.count(motionEl.get()) ? timings[motionEl.get()] : defaultTiming, endFrame);
          for (auto* ch : parsedChannels) {
            layerEntries.push_back({ch, additive, nodeInfo.targetId});
          }
          if (endFrame > layerEndFrame) {
            layerEndFrame = endFrame;
          }
          if (IsIndefiniteRepeat(motionEl)) {
            hasIndefiniteLoop = true;
            elementIndefinite = true;
            if (indefinitePeriod <= 0) {
              indefinitePeriod = RepeatPeriodFrames(ctx, motionEl, frameRate);
            }
          }
        }
      }

      // Group channels by targetId and create one AnimationObject per target.
      std::unordered_map<std::string, std::vector<ChannelWithAdditive>> groupedByTarget;
      for (const auto& entry : layerEntries) {
        groupedByTarget[entry.targetId].push_back({entry.channel, entry.additive});
      }
      // Emit targets in id order so the AnimationObject list is reproducible; the iteration order of
      // an unordered_map would otherwise leak into the exported document.
      std::vector<std::string> targetIds = {};
      targetIds.reserve(groupedByTarget.size());
      for (const auto& entry : groupedByTarget) {
        targetIds.push_back(entry.first);
      }
      std::sort(targetIds.begin(), targetIds.end());
      for (const auto& targetId : targetIds) {
        auto& channels = groupedByTarget[targetId];
        auto* obj = doc->makeNode<AnimationObject>();
        obj->target = targetId;
        // A sum chain composes as static * A1 * A2...; the tracks carry deltas only, so the static
        // pose is folded in exactly once here. A replace base track already defines the whole
        // pose and must not receive it.
        bool matrixBaseIsAdditive = false;
        // Channel names that carry a replace segment: their merged track already spells the full
        // value, so no static base may be folded into them.
        std::unordered_set<std::string> replaceNames = {};
        for (const auto& entry : channels) {
          if (entry.channel->name == "matrix" &&
              entry.channel->valueType() == ChannelValueType::Matrix) {
            matrixBaseIsAdditive = entry.additive;
          }
          if (!entry.additive) {
            replaceNames.insert(entry.channel->name);
          }
        }
        obj->channels = MergeAdditiveChannels(&channels);
        auto* targetNode = doc->findNode(targetId);
        if (matrixBaseIsAdditive) {
          if (targetNode != nullptr && targetNode->nodeType() == NodeType::Layer) {
            const auto& staticPose = static_cast<Layer*>(targetNode)->matrix;
            for (auto* channel : obj->channels) {
              if (channel->name != "matrix" || channel->valueType() != ChannelValueType::Matrix) {
                continue;
              }
              auto* matrixChannel = static_cast<TypedChannel<Matrix>*>(channel);
              for (auto& keyframe : matrixChannel->keyframes) {
                keyframe.value = staticPose * keyframe.value;
              }
            }
          }
        }
        for (auto* channel : obj->channels) {
          if (replaceNames.count(channel->name) != 0) {
            continue;
          }
          FoldStaticBaseIntoAdditiveChannel(channel, targetNode, targetId);
        }
        if (!obj->channels.empty()) {
          animation->objects.push_back(obj);
          if (elementIndefinite && layerEndFrame > 0) {
            infiniteObjects.push_back(
                {obj, indefinitePeriod > 0 ? indefinitePeriod : layerEndFrame});
          }
        }
      }
      if (layerEndFrame > maxEndFrame) {
        maxEndFrame = layerEndFrame;
      }
    }
  }

  if (animation->objects.empty()) {
    return nullptr;
  }

  // Timeline length is the longest animation; shorter infinite repetitions are tiled up to it so
  // they keep moving instead of freezing on their last pose.
  animation->duration = maxEndFrame;
  for (const auto& entry : infiniteObjects) {
    if (entry.object == nullptr || entry.period <= 0 || entry.period >= maxEndFrame) {
      continue;
    }
    for (auto* channel : entry.object->channels) {
      TileChannelKeyframes(channel, entry.period, maxEndFrame);
    }
  }
  if (hasIndefiniteLoop) {
    animation->loop = LoopMode::Loop;
  }
  return animation;
}

}  // namespace pagx
