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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGAnimationWriter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
#include "base/utils/MathUtil.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXNodeChannel.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/runtime/MatrixDecompose.h"
#include "pagx/svg/SVGAnimationPlan.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/ModifierResolver.h"
#include "pagx/utils/PolystarPath.h"
#include "pagx/types/Color.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/utils/StrokeGeometryUtils.h"
#include "pagx/xml/XMLBuilder.h"

namespace pagx {

namespace {

// A Hold segment is expressed inside a continuous `calcMode="spline"` track by repeating the value
// just before the next keyframe. The gap is a fraction of the segment so long segments keep their
// hold and short ones do not collapse; the floor keeps both keyTimes distinct after rounding.
constexpr double HoldGapRatio = 0.0001;
constexpr double HoldGapFloor = 0.00001;
constexpr double HoldGapCeiling = 0.01;
// A `d` track carries one full path string per sample, so the grid is capped to keep a dense point
// or bezier track from growing the SVG without bound.
constexpr size_t MaxPathDataSamples = 512;

struct SmilSpline {
  double x1 = 0;
  double y1 = 0;
  double x2 = 1;
  double y2 = 1;
};

struct SmilSequence {
  std::vector<std::string> values = {};
  std::vector<double> keyTimes = {};
  std::vector<SmilSpline> splines = {};
  bool useSpline = false;
  bool discrete = false;
};

struct SmilTiming {
  double durationSeconds = 0;
  std::string begin = {};
  bool repeatIndefinite = false;
  bool mirror = false;
};

std::string FormatNumber(double value) {
  char buffer[32] = {};
  snprintf(buffer, sizeof(buffer), "%.6g", value);
  return buffer;
}

std::string FormatSeconds(double value) {
  return FormatNumber(value) + "s";
}

bool IsHold(KeyframeInterpolationType type) {
  return type == KeyframeInterpolationType::Hold || type == KeyframeInterpolationType::None;
}

SmilSpline SplineBetween(const Point& out, const Point& in) {
  SmilSpline spline = {};
  spline.x1 = std::clamp(static_cast<double>(out.x), 0.0, 1.0);
  spline.y1 = static_cast<double>(out.y);
  spline.x2 = std::clamp(static_cast<double>(in.x), 0.0, 1.0);
  spline.y2 = static_cast<double>(in.y);
  return spline;
}

// Reverses a spline so it eases the mirrored segment used by LoopMode::PingPong.
SmilSpline MirrorSpline(const SmilSpline& spline) {
  SmilSpline result = {};
  result.x1 = 1.0 - spline.x2;
  result.y1 = 1.0 - spline.y2;
  result.x2 = 1.0 - spline.x1;
  result.y2 = 1.0 - spline.y1;
  return result;
}

std::string FormatColorValue(const Color& color) {
  int red = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  int green = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  int blue = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
  char buffer[8] = {};
  snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", red, green, blue);
  return buffer;
}

// Frames the emitted track must carry: both animation boundaries plus every keyframe inside the
// span. Keyframes outside [0, duration] shape the value at the boundary through Channel::evaluateAt
// (clamped when no key straddles it, interpolated when one does) instead of being clamped into a
// different track shape.
template <typename T>
void CollectTrackFrames(const Channel* channel, Frame duration, std::vector<double>* times) {
  auto span = static_cast<double>(duration);
  times->push_back(0.0);
  times->push_back(1.0);
  const auto* typed = static_cast<const TypedChannel<T>*>(channel);
  for (const auto& keyframe : typed->keyframes) {
    if (keyframe.time >= 0 && keyframe.time <= duration) {
      times->push_back(static_cast<double>(keyframe.time) / span);
    }
  }
  std::sort(times->begin(), times->end());
  times->erase(std::unique(times->begin(), times->end()), times->end());
}

template <typename T>
const Keyframe<T>* FindKeyframeAtTime(const Channel* channel, Frame frame) {
  const auto* typed = static_cast<const TypedChannel<T>*>(channel);
  for (const auto& keyframe : typed->keyframes) {
    if (keyframe.time == frame) {
      return &keyframe;
    }
  }
  return nullptr;
}

// Samples the channel at every track frame. Values come from the runtime evaluator, so a boundary
// frame that falls inside a keyframe segment reports the interpolated value rather than a clamped
// endpoint. Interpolation metadata is taken from the keyframe sitting exactly on that frame; a
// synthetic boundary frame has no segment of its own and uses linear.
template <typename T>
bool BuildTypedTrack(const Channel* channel, Frame duration, std::vector<double>* times,
                     std::vector<T>* values, std::vector<KeyframeInterpolationType>* types,
                     std::vector<Point>* bezierOuts, std::vector<Point>* bezierIns) {
  if (channel == nullptr || duration <= 0) {
    return false;
  }
  CollectTrackFrames<T>(channel, duration, times);
  if (times->size() < 2) {
    return false;
  }
  auto span = static_cast<double>(duration);
  for (auto time : *times) {
    auto frame = static_cast<Frame>(std::llround(time * span));
    values->push_back(std::get<T>(channel->evaluateAt(frame)));
    const Keyframe<T>* keyframe = FindKeyframeAtTime<T>(channel, frame);
    if (keyframe != nullptr) {
      types->push_back(keyframe->interpolation);
      bezierOuts->push_back(keyframe->bezierOut);
      bezierIns->push_back(keyframe->bezierIn);
    } else {
      types->push_back(KeyframeInterpolationType::Linear);
      bezierOuts->push_back(Point{});
      bezierIns->push_back(Point{});
    }
  }
  return true;
}

// Expands the normalized keyframes into a flat list of (time, value, incoming spline) entries.
// Hold segments contribute an extra entry just before the next keyframe so the value jumps there.
template <typename T>
void FlattenKeyframes(const std::vector<double>& times, const std::vector<T>& values,
                      const std::vector<KeyframeInterpolationType>& types,
                      const std::vector<Point>& bezierOuts, const std::vector<Point>& bezierIns,
                      std::vector<double>* flatTimes, std::vector<T>* flatValues,
                      std::vector<SmilSpline>* flatSplines, bool* useSpline) {
  bool anyBezier = false;
  for (auto type : types) {
    if (type == KeyframeInterpolationType::Bezier) {
      anyBezier = true;
    }
  }
  *useSpline = anyBezier;
  flatTimes->push_back(times.front());
  flatValues->push_back(values.front());
  flatSplines->push_back(SmilSpline{});
  for (size_t i = 1; i < times.size(); ++i) {
    double segmentStart = times[i - 1];
    double segmentEnd = times[i];
    if (segmentEnd <= segmentStart) {
      continue;
    }
    if (IsHold(types[i - 1])) {
      double gap =
          std::clamp((segmentEnd - segmentStart) * HoldGapRatio, HoldGapFloor, HoldGapCeiling);
      double holdTime = segmentEnd - gap;
      if (holdTime > segmentStart) {
        flatTimes->push_back(holdTime);
        flatValues->push_back(values[i - 1]);
        flatSplines->push_back(SmilSpline{});
      }
      flatTimes->push_back(segmentEnd);
      flatValues->push_back(values[i]);
      flatSplines->push_back(SmilSpline{});
      continue;
    }
    SmilSpline spline = {};
    if (types[i - 1] == KeyframeInterpolationType::Bezier) {
      spline = SplineBetween(bezierOuts[i - 1], bezierIns[i]);
    }
    flatTimes->push_back(segmentEnd);
    flatValues->push_back(values[i]);
    flatSplines->push_back(spline);
  }
}

// A LoopMode::PingPong animation is emitted as one mirrored sequence: forward then backward, with
// each segment's easing reversed. Other loop modes just repeat the forward sequence. The caller
// doubles SmilTiming::durationSeconds exactly once per emitted element, because every track of that
// element must share one dur; doubling here would compound per track.
void ApplyPingPong(SmilSequence* sequence) {
  if (sequence->values.size() < 2) {
    return;
  }
  auto forwardValues = sequence->values;
  auto forwardTimes = sequence->keyTimes;
  auto forwardSplines = sequence->splines;
  size_t count = forwardValues.size();
  for (size_t i = count - 1; i-- > 0;) {
    sequence->values.push_back(forwardValues[i]);
    sequence->keyTimes.push_back(2.0 - forwardTimes[i]);
  }
  if (sequence->useSpline && forwardSplines.size() + 1 == count) {
    for (size_t i = count - 1; i-- > 0;) {
      sequence->splines.push_back(MirrorSpline(forwardSplines[i]));
    }
  }
  for (auto& time : sequence->keyTimes) {
    time *= 0.5;
  }
}

bool ResolveTiming(const Animation* animation, const AnimationTimeline* timeline,
                   SmilTiming* timing) {
  if (animation == nullptr || animation->duration <= 0 || animation->frameRate <= 0.0f) {
    return false;
  }
  timing->durationSeconds =
      static_cast<double>(animation->duration) / static_cast<double>(animation->frameRate);
  timing->repeatIndefinite =
      animation->loop == LoopMode::Loop || animation->loop == LoopMode::PingPong;
  timing->mirror = animation->loop == LoopMode::PingPong;
  if (timeline == nullptr) {
    return true;
  }
  if (!timeline->playing) {
    timing->begin = "indefinite";
  } else if (timeline->evaluationOffset != 0) {
    auto seconds =
        static_cast<double>(timeline->evaluationOffset) / static_cast<double>(animation->frameRate);
    timing->begin = FormatSeconds(seconds);
  }
  return true;
}

std::string JoinValues(const std::vector<std::string>& values, char separator) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      result += separator;
    }
    result += values[i];
  }
  return result;
}

void WriteAnimateElement(XMLBuilder& out, const char* attributeName, const char* transformType,
                         bool additiveReplace, const SmilSequence& sequence,
                         const SmilTiming& timing, const char* extraName, const char* extraValue) {
  out.openElement(transformType == nullptr ? "animate" : "animateTransform");
  out.addAttribute("attributeName", attributeName);
  if (transformType != nullptr) {
    out.addAttribute("type", transformType);
    out.addAttribute("additive", additiveReplace ? "replace" : "sum");
  }
  if (extraName != nullptr && extraValue != nullptr) {
    out.addAttribute(extraName, extraValue);
  }
  out.addAttribute("values", JoinValues(sequence.values, ';'));
  std::vector<std::string> times;
  times.reserve(sequence.keyTimes.size());
  for (auto time : sequence.keyTimes) {
    times.push_back(FormatNumber(time));
  }
  out.addAttribute("keyTimes", JoinValues(times, ';'));
  if (sequence.discrete) {
    out.addAttribute("calcMode", "discrete");
  } else if (sequence.useSpline && sequence.splines.size() + 1 == sequence.values.size()) {
    out.addAttribute("calcMode", "spline");
    std::vector<std::string> splines;
    splines.reserve(sequence.splines.size());
    for (const auto& spline : sequence.splines) {
      splines.push_back(FormatNumber(spline.x1) + " " + FormatNumber(spline.y1) + " " +
                        FormatNumber(spline.x2) + " " + FormatNumber(spline.y2));
    }
    out.addAttribute("keySplines", JoinValues(splines, ';'));
  }
  if (!timing.begin.empty()) {
    out.addAttribute("begin", timing.begin);
  }
  out.addRequiredAttribute("dur", FormatSeconds(timing.durationSeconds).c_str());
  if (timing.repeatIndefinite) {
    out.addAttribute("repeatCount", "indefinite");
  }
  out.addAttribute("fill", "freeze");
  out.closeElementSelfClosing();
}

void AppendFloatSequence(const std::vector<double>& flatTimes, const std::vector<float>& flatValues,
                         const std::vector<SmilSpline>& flatSplines, bool useSpline,
                         SmilSequence* sequence) {
  sequence->useSpline = useSpline;
  for (size_t i = 0; i < flatTimes.size(); ++i) {
    sequence->values.push_back(FloatToString(flatValues[i]));
    sequence->keyTimes.push_back(flatTimes[i]);
    if (i > 0) {
      sequence->splines.push_back(flatSplines[i]);
    }
  }
}

bool BuildFloatSequence(const Channel* channel, Frame duration, float scale, float minValue,
                        float maxValue, SmilSequence* sequence) {
  std::vector<double> times;
  std::vector<float> values;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> bezierOuts;
  std::vector<Point> bezierIns;
  if (!BuildTypedTrack<float>(channel, duration, &times, &values, &types, &bezierOuts,
                              &bezierIns)) {
    return false;
  }
  bool discrete = true;
  for (size_t i = 0; i + 1 < types.size(); ++i) {
    if (!IsHold(types[i])) {
      discrete = false;
      break;
    }
  }
  if (discrete) {
    sequence->discrete = true;
    for (size_t i = 0; i < times.size(); ++i) {
      float value = std::clamp(values[i] * scale, minValue, maxValue);
      sequence->values.push_back(FloatToString(value));
      sequence->keyTimes.push_back(times[i]);
    }
    return true;
  }
  std::vector<double> flatTimes;
  std::vector<float> flatValues;
  std::vector<SmilSpline> flatSplines;
  FlattenKeyframes(times, values, types, bezierOuts, bezierIns, &flatTimes, &flatValues,
                   &flatSplines, &sequence->useSpline);
  for (auto& value : flatValues) {
    value = std::clamp(value * scale, minValue, maxValue);
  }
  AppendFloatSequence(flatTimes, flatValues, flatSplines, sequence->useSpline, sequence);
  return true;
}

bool BuildColorSequences(const Channel* channel, Frame duration, float alphaScale,
                         SmilSequence* colorSequence, SmilSequence* alphaSequence) {
  std::vector<double> times;
  std::vector<Color> values;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> bezierOuts;
  std::vector<Point> bezierIns;
  if (!BuildTypedTrack<Color>(channel, duration, &times, &values, &types, &bezierOuts,
                              &bezierIns)) {
    return false;
  }
  std::vector<double> flatTimes;
  std::vector<Color> flatValues;
  std::vector<SmilSpline> flatSplines;
  bool useSpline = false;
  FlattenKeyframes(times, values, types, bezierOuts, bezierIns, &flatTimes, &flatValues,
                   &flatSplines, &useSpline);
  colorSequence->useSpline = useSpline;
  alphaSequence->useSpline = useSpline;
  for (size_t i = 0; i < flatTimes.size(); ++i) {
    colorSequence->values.push_back(FormatColorValue(flatValues[i]));
    colorSequence->keyTimes.push_back(flatTimes[i]);
    alphaSequence->values.push_back(FloatToString(flatValues[i].alpha * alphaScale));
    alphaSequence->keyTimes.push_back(flatTimes[i]);
    if (i > 0) {
      colorSequence->splines.push_back(flatSplines[i]);
      alphaSequence->splines.push_back(flatSplines[i]);
    }
  }
  return true;
}

float EvaluateChannelFloat(const Channel* channel, Frame frame) {
  auto value = channel->evaluateAt(frame);
  auto* result = std::get_if<float>(&value);
  return result != nullptr ? *result : 0.0f;
}

Matrix EvaluateChannelMatrix(const Channel* channel, Frame frame) {
  auto value = channel->evaluateAt(frame);
  auto* result = std::get_if<Matrix>(&value);
  return result != nullptr ? *result : Matrix::Identity();
}

// Appends the animation boundaries plus every keyframe inside the span. Keyframes outside
// [0, duration] are left to Channel::evaluateAt at the boundary frames so a segment that straddles
// the start still reports its interpolated value there.
void AppendTrackFrames(const Channel* channel, Frame duration, std::vector<Frame>* frames) {
  if (channel == nullptr || duration <= 0) {
    return;
  }
  frames->push_back(0);
  frames->push_back(duration);
  if (channel->valueType() == ChannelValueType::Matrix) {
    const auto* typed = static_cast<const TypedChannel<Matrix>*>(channel);
    for (const auto& keyframe : typed->keyframes) {
      if (keyframe.time >= 0 && keyframe.time <= duration) {
        frames->push_back(keyframe.time);
      }
    }
    return;
  }
  if (channel->valueType() == ChannelValueType::Bool) {
    const auto* typed = static_cast<const TypedChannel<bool>*>(channel);
    for (const auto& keyframe : typed->keyframes) {
      if (keyframe.time >= 0 && keyframe.time <= duration) {
        frames->push_back(keyframe.time);
      }
    }
    return;
  }
  const auto* typed = static_cast<const TypedChannel<float>*>(channel);
  for (const auto& keyframe : typed->keyframes) {
    if (keyframe.time >= 0 && keyframe.time <= duration) {
      frames->push_back(keyframe.time);
    }
  }
}

// Copies per-segment easing from a single animated channel when that channel's keyframe times match
// the merged timeline exactly. A union of several channels cannot carry per-segment easing because
// each segment would need a different spline per channel.
bool CollectSegmentSplines(const Channel* channel, const std::vector<Frame>& frames,
                           std::vector<SmilSpline>* splines, bool* useSpline) {
  splines->clear();
  *useSpline = false;
  if (channel == nullptr || frames.size() < 2) {
    return false;
  }
  std::vector<Frame> keyTimes;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> outs;
  std::vector<Point> ins;
  if (channel->valueType() == ChannelValueType::Matrix) {
    const auto* typed = static_cast<const TypedChannel<Matrix>*>(channel);
    for (const auto& keyframe : typed->keyframes) {
      keyTimes.push_back(keyframe.time);
      types.push_back(keyframe.interpolation);
      outs.push_back(keyframe.bezierOut);
      ins.push_back(keyframe.bezierIn);
    }
  } else {
    const auto* typed = static_cast<const TypedChannel<float>*>(channel);
    for (const auto& keyframe : typed->keyframes) {
      keyTimes.push_back(keyframe.time);
      types.push_back(keyframe.interpolation);
      outs.push_back(keyframe.bezierOut);
      ins.push_back(keyframe.bezierIn);
    }
  }
  if (keyTimes.size() != frames.size()) {
    return false;
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    if (keyTimes[i] != frames[i]) {
      return false;
    }
  }
  bool anyBezier = false;
  for (size_t i = 0; i + 1 < frames.size(); ++i) {
    if (IsHold(types[i])) {
      splines->clear();
      return false;
    }
    if (types[i] == KeyframeInterpolationType::Bezier) {
      anyBezier = true;
      splines->push_back(SplineBetween(outs[i], ins[i + 1]));
    } else {
      splines->push_back(SmilSpline{});
    }
  }
  *useSpline = anyBezier;
  return anyBezier;
}

// Parses a `point{N}.x` / `point{N}.y` channel name. This family is not part of the reflective
// node channel registry: it addresses the PathData point array directly, mirroring the runtime's
// PathRuntimeTarget parser in LayerBuilder.
bool ParsePathPointChannel(const std::string& channel, size_t* index, bool* isX) {
  if (channel.rfind("point", 0) != 0) {
    return false;
  }
  auto dot = channel.rfind('.');
  if (dot == std::string::npos || dot + 2 != channel.size() || dot <= 5) {
    return false;
  }
  char axis = channel[dot + 1];
  if (axis != 'x' && axis != 'y') {
    return false;
  }
  size_t value = 0;
  for (size_t i = 5; i < dot; i++) {
    char c = channel[i];
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<size_t>(c - '0');
  }
  *index = value;
  *isX = axis == 'x';
  return true;
}

struct PathPointBinding {
  const SVGAnimationBinding* binding = nullptr;
  size_t index = 0;
  bool isX = false;
};

void CollectPathPointBindings(const SVGAnimationPlan* plan, const Node* path,
                              std::vector<PathPointBinding>* out) {
  auto* list = plan->BindingsFor(path);
  if (list == nullptr) {
    return;
  }
  for (const auto& binding : *list) {
    if (binding.channel->valueType() != ChannelValueType::Float) {
      continue;
    }
    size_t index = 0;
    bool isX = false;
    if (!ParsePathPointChannel(binding.channel->name, &index, &isX)) {
      continue;
    }
    out->push_back({&binding, index, isX});
  }
}

// Rebuilds the path data from a point array while keeping the authored verb list. Used when a
// sampled frame changes Path point channels before the modifier chain runs.
void FillPathDataFromPoints(const PathData& source, const std::vector<Point>& points,
                            PathData* result) {
  size_t index = 0;
  for (auto verb : source.verbs()) {
    switch (verb) {
      case PathVerb::Move:
        if (index < points.size()) {
          result->moveTo(points[index].x, points[index].y);
        }
        index += 1;
        break;
      case PathVerb::Line:
        if (index < points.size()) {
          result->lineTo(points[index].x, points[index].y);
        }
        index += 1;
        break;
      case PathVerb::Quad:
        if (index + 1 < points.size()) {
          result->quadTo(points[index].x, points[index].y, points[index + 1].x,
                         points[index + 1].y);
        }
        index += 2;
        break;
      case PathVerb::Cubic:
        if (index + 2 < points.size()) {
          result->cubicTo(points[index].x, points[index].y, points[index + 1].x,
                          points[index + 1].y, points[index + 2].x, points[index + 2].y);
        }
        index += 3;
        break;
      case PathVerb::Close:
        result->close();
        break;
    }
  }
}

// Rebuilds the path data with the sampled point array. Every frame keeps the authored verb
// structure, which is what lets SMIL interpolate the `d` values instead of switching discretely.
std::string PathDataAtFrame(const PathData& source, const std::vector<Point>& points,
                            const Point& renderPosition, float renderScale) {
  PathData frame = PathDataFromSVGString("");
  size_t index = 0;
  for (auto verb : source.verbs()) {
    switch (verb) {
      case PathVerb::Move:
        if (index < points.size()) {
          frame.moveTo(points[index].x * renderScale + renderPosition.x,
                       points[index].y * renderScale + renderPosition.y);
        }
        index += 1;
        break;
      case PathVerb::Line:
        if (index < points.size()) {
          frame.lineTo(points[index].x * renderScale + renderPosition.x,
                       points[index].y * renderScale + renderPosition.y);
        }
        index += 1;
        break;
      case PathVerb::Quad:
        if (index + 1 < points.size()) {
          frame.quadTo(points[index].x * renderScale + renderPosition.x,
                       points[index].y * renderScale + renderPosition.y,
                       points[index + 1].x * renderScale + renderPosition.x,
                       points[index + 1].y * renderScale + renderPosition.y);
        }
        index += 2;
        break;
      case PathVerb::Cubic:
        if (index + 2 < points.size()) {
          frame.cubicTo(points[index].x * renderScale + renderPosition.x,
                        points[index].y * renderScale + renderPosition.y,
                        points[index + 1].x * renderScale + renderPosition.x,
                        points[index + 1].y * renderScale + renderPosition.y,
                        points[index + 2].x * renderScale + renderPosition.x,
                        points[index + 2].y * renderScale + renderPosition.y);
        }
        index += 3;
        break;
      case PathVerb::Close:
        frame.close();
        break;
    }
  }
  return PathDataToSVGString(frame);
}

// Evaluates the layer transform at every keyframe time of the matrix channel, using the same
// BuildLayerMatrix path the static export uses. This exporter's transform model has no rotation /
// scale / skew / anchor components, so only the authored `matrix` can be animated.
bool SampleLayerTransform(const SVGAnimationPlan* plan, const Layer* layer,
                          std::vector<Frame>* frames, std::vector<Matrix>* matrices,
                          SmilTiming* timing, Frame* durationFrames,
                          std::vector<SmilSpline>* splines, bool* useSpline) {
  if (plan == nullptr || layer == nullptr) {
    return false;
  }
  auto* binding = plan->Find(layer, "matrix");
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Matrix) {
    return false;
  }
  if (!ResolveTiming(binding->animation, binding->timeline, timing)) {
    return false;
  }
  *durationFrames = binding->animation->duration;

  AppendTrackFrames(binding->channel, *durationFrames, frames);
  if (frames->size() < 2) {
    return false;
  }
  std::sort(frames->begin(), frames->end());
  frames->erase(std::unique(frames->begin(), frames->end()), frames->end());
  if (frames->size() < 2) {
    return false;
  }

  splines->clear();
  *useSpline = false;
  CollectSegmentSplines(binding->channel, *frames, splines, useSpline);

  matrices->clear();
  matrices->reserve(frames->size());
  for (Frame frame : *frames) {
    LayerChannelOverride override = {};
    Matrix matrixValue = EvaluateChannelMatrix(binding->channel, frame);
    override.matrix = &matrixValue;
    matrices->push_back(BuildLayerMatrix(layer, &override));
  }
  return matrices->size() >= 2;
}

// Splits a matrix sequence into translate / rotate / skewX / scale tracks and emits them as additive
// <animateTransform> elements. DecomposeAffine's linear part is Rotation * Skew * Scale, which is
// exactly the order these four SVG transform functions compose in, so each component can animate
// independently and still reproduce the sampled pose at every keyframe.
bool WriteTransformTracks(XMLBuilder& out, const std::vector<Matrix>& matrices,
                          const std::vector<double>& keyTimes, SmilTiming timing,
                          const std::vector<SmilSpline>& splines, bool useSpline) {
  std::vector<DecomposedMatrix> decomposed;
  decomposed.reserve(matrices.size());
  for (const auto& matrix : matrices) {
    decomposed.push_back(
        DecomposeAffine(matrix.a, matrix.b, matrix.c, matrix.d, matrix.tx, matrix.ty));
  }

  constexpr float PositionEpsilon = 1e-4f;
  constexpr float AngleEpsilon = 1e-5f;
  bool translateVaries = false;
  bool rotateVaries = false;
  bool skewVaries = false;
  bool scaleVaries = false;
  for (const auto& values : decomposed) {
    if (std::fabs(values.translateX) > PositionEpsilon ||
        std::fabs(values.translateY) > PositionEpsilon) {
      translateVaries = true;
    }
    if (std::fabs(values.rotation) > AngleEpsilon) {
      rotateVaries = true;
    }
    if (std::fabs(values.skew) > AngleEpsilon) {
      skewVaries = true;
    }
    if (std::fabs(values.scaleX - 1.0f) > AngleEpsilon ||
        std::fabs(values.scaleY - 1.0f) > AngleEpsilon) {
      scaleVaries = true;
    }
  }
  if (!translateVaries && !rotateVaries && !skewVaries && !scaleVaries) {
    return false;
  }

  SmilSequence translateSequence;
  SmilSequence rotateSequence;
  SmilSequence skewSequence;
  SmilSequence scaleSequence;
  const double RadiansToDegrees = 180.0 / 3.14159265358979323846;
  for (size_t i = 0; i < decomposed.size(); ++i) {
    const auto& values = decomposed[i];
    translateSequence.values.push_back(FloatToString(values.translateX) + " " +
                                       FloatToString(values.translateY));
    translateSequence.keyTimes.push_back(keyTimes[i]);
    rotateSequence.values.push_back(
        FloatToString(static_cast<float>(values.rotation * RadiansToDegrees)));
    rotateSequence.keyTimes.push_back(keyTimes[i]);
    skewSequence.values.push_back(
        FloatToString(static_cast<float>(values.skew * RadiansToDegrees)));
    skewSequence.keyTimes.push_back(keyTimes[i]);
    scaleSequence.values.push_back(FloatToString(values.scaleX) + " " +
                                   FloatToString(values.scaleY));
    scaleSequence.keyTimes.push_back(keyTimes[i]);
  }

  SmilSequence* tracks[4] = {&translateSequence, &rotateSequence, &skewSequence, &scaleSequence};
  bool trackVaries[4] = {translateVaries, rotateVaries, skewVaries, scaleVaries};
  const char* trackTypes[4] = {"translate", "rotate", "skewX", "scale"};
  for (int t = 0; t < 4; ++t) {
    tracks[t]->useSpline = useSpline;
    if (useSpline) {
      tracks[t]->splines = splines;
    }
  }
  if (timing.mirror) {
    for (int t = 0; t < 4; ++t) {
      ApplyPingPong(tracks[t]);
    }
    timing.durationSeconds *= 2.0;
  }

  bool wrote = false;
  bool first = true;
  for (int t = 0; t < 4; ++t) {
    if (!trackVaries[t] || tracks[t]->values.size() < 2) {
      continue;
    }
    WriteAnimateElement(out, "transform", trackTypes[t], first, *tracks[t], timing, nullptr,
                        nullptr);
    first = false;
    wrote = true;
  }
  return wrote;
}

Matrix InvertAffine(const Matrix& matrix) {
  float determinant = matrix.a * matrix.d - matrix.b * matrix.c;
  if (std::fabs(determinant) < 1e-9f) {
    return Matrix::Identity();
  }
  float inverseDeterminant = 1.0f / determinant;
  Matrix result = {};
  result.a = matrix.d * inverseDeterminant;
  result.b = -matrix.b * inverseDeterminant;
  result.c = -matrix.c * inverseDeterminant;
  result.d = matrix.a * inverseDeterminant;
  result.tx = -(result.a * matrix.tx + result.c * matrix.ty);
  result.ty = -(result.b * matrix.tx + result.d * matrix.ty);
  return result;
}

constexpr int GroupTransformChannelCount = 9;

const char* const GroupTransformChannelNames[GroupTransformChannelCount] = {
    "position.x", "position.y", "anchor.x", "anchor.y", "rotation",
    "scale.x",    "scale.y",    "skew",     "skewAxis"};

constexpr int RectangleGeometryChannelCount = 5;

const char* const RectangleGeometryChannelNames[RectangleGeometryChannelCount] = {
    "position.x", "position.y", "size.width", "size.height", "roundness"};

constexpr int EllipseGeometryChannelCount = 4;

const char* const EllipseGeometryChannelNames[EllipseGeometryChannelCount] = {
    "position.x", "position.y", "size.width", "size.height"};

// Evaluates the Group transform at one frame, falling back to the authored value for channels the
// animation does not drive.
Matrix GroupMatrixAtFrame(const SVGAnimationPlan* plan, const Group* group, Frame frame) {
  GroupChannelOverride override = {};
  // Start from the layout-resolved position so an animated x leaves the y contribution untouched
  // when the group is placed by constraints rather than by its authored position.
  Point position = group->renderPosition();
  Point anchor = group->anchor;
  float rotation = group->rotation;
  float scaleX = group->scale.x;
  float scaleY = group->scale.y;
  float skew = group->skew;
  float skewAxis = group->skewAxis;
  bool hasPosition = false;
  bool hasAnchor = false;
  auto* positionXBinding = plan->Find(group, "position.x");
  auto* positionYBinding = plan->Find(group, "position.y");
  if (positionXBinding != nullptr) {
    position.x = EvaluateChannelFloat(positionXBinding->channel, frame);
    hasPosition = true;
  }
  if (positionYBinding != nullptr) {
    position.y = EvaluateChannelFloat(positionYBinding->channel, frame);
    hasPosition = true;
  }
  auto* anchorXBinding = plan->Find(group, "anchor.x");
  auto* anchorYBinding = plan->Find(group, "anchor.y");
  if (anchorXBinding != nullptr) {
    anchor.x = EvaluateChannelFloat(anchorXBinding->channel, frame);
    hasAnchor = true;
  }
  if (anchorYBinding != nullptr) {
    anchor.y = EvaluateChannelFloat(anchorYBinding->channel, frame);
    hasAnchor = true;
  }
  auto* rotationBinding = plan->Find(group, "rotation");
  if (rotationBinding != nullptr) {
    rotation = EvaluateChannelFloat(rotationBinding->channel, frame);
  }
  auto* scaleXBinding = plan->Find(group, "scale.x");
  if (scaleXBinding != nullptr) {
    scaleX = EvaluateChannelFloat(scaleXBinding->channel, frame);
  }
  auto* scaleYBinding = plan->Find(group, "scale.y");
  if (scaleYBinding != nullptr) {
    scaleY = EvaluateChannelFloat(scaleYBinding->channel, frame);
  }
  auto* skewBinding = plan->Find(group, "skew");
  if (skewBinding != nullptr) {
    skew = EvaluateChannelFloat(skewBinding->channel, frame);
  }
  auto* skewAxisBinding = plan->Find(group, "skewAxis");
  if (skewAxisBinding != nullptr) {
    skewAxis = EvaluateChannelFloat(skewAxisBinding->channel, frame);
  }
  if (hasPosition) {
    override.position = &position;
  }
  if (hasAnchor) {
    override.anchor = &anchor;
  }
  override.rotation = &rotation;
  override.scaleX = &scaleX;
  override.scaleY = &scaleY;
  override.skew = &skew;
  override.skewAxis = &skewAxis;
  return BuildGroupMatrix(group, &override);
}

}  // namespace

SVGAnimationWriter::SVGAnimationWriter(const SVGAnimationPlan* plan,
                                       std::vector<std::string>* warnings,
                                       PAGXDocument* document)
    : _plan(plan), _warnings(warnings), _document(document) {
}

void SVGAnimationWriter::AddWarning(std::string message) const {
  if (_warnings != nullptr) {
    _warnings->push_back(std::move(message));
  }
}

bool SVGAnimationWriter::HasLayerAnimates(const Layer* layer) const {
  return _plan != nullptr && _plan->BindingsFor(layer) != nullptr;
}

bool SVGAnimationWriter::HasPainterAnimates(const Fill* fill, const Stroke* stroke) const {
  return _plan != nullptr && _plan->HasPainterAnimation(fill, stroke);
}

bool SVGAnimationWriter::HasGroupAnimates(const Group* group) const {
  return _plan != nullptr && _plan->BindingsFor(group) != nullptr;
}

bool SVGAnimationWriter::HasPainterOpacityAnimates(const Fill* fill, const Stroke* stroke) const {
  if (_plan == nullptr) {
    return false;
  }
  return _plan->Find(fill, "alpha") != nullptr || _plan->Find(fill, "color") != nullptr ||
         _plan->Find(stroke, "alpha") != nullptr || _plan->Find(stroke, "color") != nullptr;
}

bool SVGAnimationWriter::HasElementOpacityAnimates(const SVGGroupTransformStep* steps,
                                                   size_t count) const {
  if (_plan == nullptr || steps == nullptr) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (_plan->Find(steps[i].group, "alpha") != nullptr) {
      return true;
    }
  }
  return false;
}

bool SVGAnimationWriter::WriteElementOpacityAnimates(XMLBuilder& out,
                                                     const SVGGroupTransformStep* steps,
                                                     size_t count, float baseAlpha,
                                                     const char* attributeName) {
  if (_plan == nullptr || steps == nullptr || count == 0 || baseAlpha <= 0.0f) {
    return false;
  }
  const Animation* animation = nullptr;
  const AnimationTimeline* timeline = nullptr;
  const Channel* singleChannel = nullptr;
  int animatedChannels = 0;
  std::vector<const Channel*> perStep(count, nullptr);
  std::vector<Frame> frames;
  for (size_t i = 0; i < count; ++i) {
    auto* binding = _plan->Find(steps[i].group, "alpha");
    if (binding == nullptr) {
      continue;
    }
    perStep[i] = binding->channel;
    singleChannel = binding->channel;
    ++animatedChannels;
    if (animation == nullptr) {
      animation = binding->animation;
      timeline = binding->timeline;
    } else if (animation != binding->animation) {
      return false;
    }
    AppendTrackFrames(binding->channel, binding->animation->duration, &frames);
  }
  if (animation == nullptr || frames.empty()) {
    return false;
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }

  float staticProduct = 1.0f;
  for (size_t i = 0; i < count; ++i) {
    staticProduct *= perStep[i] != nullptr ? EvaluateChannelFloat(perStep[i], frames.front())
                                           : steps[i].group->alpha;
  }
  if (staticProduct <= 0.0f) {
    return false;
  }

  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (animatedChannels == 1) {
    CollectSegmentSplines(singleChannel, frames, &splines, &useSpline);
  }

  SmilSequence sequence;
  sequence.useSpline = useSpline;
  for (size_t i = 0; i < frames.size(); ++i) {
    float product = 1.0f;
    for (size_t s = 0; s < count; ++s) {
      product *= perStep[s] != nullptr ? EvaluateChannelFloat(perStep[s], frames[i])
                                       : steps[s].group->alpha;
    }
    sequence.values.push_back(FloatToString(baseAlpha * product / staticProduct));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frames[i]) / span, 0.0, 1.0));
    if (i > 0 && useSpline) {
      sequence.splines.push_back(splines[i - 1]);
    }
  }
  if (sequence.values.size() < 2) {
    return false;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, attributeName, nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

namespace {

// Reads a scalar field from the node's static state so an axis that is not animated keeps its
// authored value in the emitted values list.
float StaticNodeFloat(const Node* node, const char* channelName) {
  KeyValue value;
  if (!GetNodeChannel(node, channelName, &value)) {
    return 0.0f;
  }
  auto* result = std::get_if<float>(&value);
  return result != nullptr ? *result : 0.0f;
}

// Emits one scalar attribute track from a channel that may be null (static value only).
bool WriteScalarTrack(XMLBuilder& out, const Channel* channel, float staticValue, Frame duration,
                      SmilTiming timing, const char* attributeName) {
  std::vector<Frame> frames;
  AppendTrackFrames(channel, duration, &frames);
  if (frames.size() < 2) {
    return false;
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }
  auto span = static_cast<double>(duration);
  SmilSequence sequence;
  for (Frame frame : frames) {
    sequence.values.push_back(
        FloatToString(channel != nullptr ? EvaluateChannelFloat(channel, frame) : staticValue));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, attributeName, nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

}  // namespace

bool SVGAnimationWriter::HasFilterAnimates(const Node* node) const {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  return _plan->Find(node, "blurX") != nullptr || _plan->Find(node, "blurY") != nullptr ||
         _plan->Find(node, "offsetX") != nullptr || _plan->Find(node, "offsetY") != nullptr ||
         _plan->Find(node, "color") != nullptr;
}

bool SVGAnimationWriter::HasFilterBlurAnimates(const Node* node) const {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  return _plan->Find(node, "blurX") != nullptr || _plan->Find(node, "blurY") != nullptr;
}

bool SVGAnimationWriter::HasFilterOffsetAnimates(const Node* node) const {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  return _plan->Find(node, "offsetX") != nullptr || _plan->Find(node, "offsetY") != nullptr;
}

float SVGAnimationWriter::MaxChannelValue(const Node* node, const char* channelName,
                                          float fallback) const {
  if (_plan == nullptr || node == nullptr) {
    return fallback;
  }
  auto* binding = _plan->Find(node, channelName);
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Float) {
    return fallback;
  }
  const auto* typed = static_cast<const TypedChannel<float>*>(binding->channel);
  float result = fallback;
  for (const auto& keyframe : typed->keyframes) {
    result = std::max(result, keyframe.value);
  }
  return result;
}

float SVGAnimationWriter::MaxChannelMagnitude(const Node* node, const char* channelName,
                                              float fallback) const {
  if (_plan == nullptr || node == nullptr) {
    return fallback;
  }
  auto* binding = _plan->Find(node, channelName);
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Float) {
    return fallback;
  }
  const auto* typed = static_cast<const TypedChannel<float>*>(binding->channel);
  float result = std::fabs(fallback);
  for (const auto& keyframe : typed->keyframes) {
    result = std::max(result, std::fabs(keyframe.value));
  }
  return result;
}

bool SVGAnimationWriter::WriteFilterBlurAnimates(XMLBuilder& out, const Node* node) {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  auto* xBinding = _plan->Find(node, "blurX");
  auto* yBinding = _plan->Find(node, "blurY");
  if (xBinding == nullptr && yBinding == nullptr) {
    return false;
  }
  if (xBinding != nullptr && yBinding != nullptr && xBinding->animation != yBinding->animation) {
    return false;
  }
  const auto* animation = xBinding != nullptr ? xBinding->animation : yBinding->animation;
  const auto* timeline = xBinding != nullptr ? xBinding->timeline : yBinding->timeline;
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }
  std::vector<Frame> frames;
  AppendTrackFrames(xBinding != nullptr ? xBinding->channel : nullptr, animation->duration,
                    &frames);
  AppendTrackFrames(yBinding != nullptr ? yBinding->channel : nullptr, animation->duration,
                    &frames);
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }

  float staticX = StaticNodeFloat(node, "blurX");
  float staticY = StaticNodeFloat(node, "blurY");
  // stdDeviation takes either one value for both axes or one pair; the emitted list has to keep the
  // same shape for every keyframe, so two values are used as soon as any frame differs.
  bool twoValues = false;
  std::vector<float> xs;
  std::vector<float> ys;
  for (Frame frame : frames) {
    float x = xBinding != nullptr ? EvaluateChannelFloat(xBinding->channel, frame) : staticX;
    float y = yBinding != nullptr ? EvaluateChannelFloat(yBinding->channel, frame) : staticY;
    twoValues = twoValues || FloatToString(x) != FloatToString(y);
    xs.push_back(x);
    ys.push_back(y);
  }
  auto span = static_cast<double>(animation->duration);
  SmilSequence sequence;
  for (size_t i = 0; i < frames.size(); ++i) {
    sequence.values.push_back(twoValues ? FloatToString(xs[i]) + " " + FloatToString(ys[i])
                                        : FloatToString(xs[i]));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frames[i]) / span, 0.0, 1.0));
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "stdDeviation", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::WriteFilterOffsetAnimates(XMLBuilder& out, const Node* node) {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  auto* xBinding = _plan->Find(node, "offsetX");
  auto* yBinding = _plan->Find(node, "offsetY");
  if (xBinding == nullptr && yBinding == nullptr) {
    return false;
  }
  if (xBinding != nullptr && yBinding != nullptr && xBinding->animation != yBinding->animation) {
    return false;
  }
  const auto* animation = xBinding != nullptr ? xBinding->animation : yBinding->animation;
  const auto* timeline = xBinding != nullptr ? xBinding->timeline : yBinding->timeline;
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }
  bool wrote = false;
  if (xBinding != nullptr) {
    wrote = WriteScalarTrack(out, xBinding->channel, StaticNodeFloat(node, "offsetX"),
                             animation->duration, timing, "dx") ||
            wrote;
  }
  if (yBinding != nullptr) {
    wrote = WriteScalarTrack(out, yBinding->channel, StaticNodeFloat(node, "offsetY"),
                             animation->duration, timing, "dy") ||
            wrote;
  }
  return wrote;
}

bool SVGAnimationWriter::WriteFilterColorAnimates(XMLBuilder& out, const Node* node) {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  auto* binding = _plan->Find(node, "color");
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Color) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(binding->animation, binding->timeline, &timing)) {
    return false;
  }
  SmilSequence colorSequence;
  SmilSequence alphaSequence;
  if (!BuildColorSequences(binding->channel, binding->animation->duration, 1.0f, &colorSequence,
                           &alphaSequence)) {
    return false;
  }
  if (timing.mirror) {
    ApplyPingPong(&colorSequence);
    ApplyPingPong(&alphaSequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "flood-color", nullptr, false, colorSequence, timing, nullptr, nullptr);
  WriteAnimateElement(out, "flood-opacity", nullptr, false, alphaSequence, timing, nullptr,
                      nullptr);
  return true;
}

bool SVGAnimationWriter::HasShapeGeometryAnimates(const Node* element) const {
  if (_plan == nullptr || element == nullptr) {
    return false;
  }
  auto type = element->nodeType();
  if (type != NodeType::Rectangle && type != NodeType::Ellipse) {
    return false;
  }
  bool isRectangle = type == NodeType::Rectangle;
  const char* const* names =
      isRectangle ? RectangleGeometryChannelNames : EllipseGeometryChannelNames;
  int count = isRectangle ? RectangleGeometryChannelCount : EllipseGeometryChannelCount;
  for (int i = 0; i < count; ++i) {
    if (_plan->Find(element, names[i]) != nullptr) {
      return true;
    }
  }
  return false;
}

namespace {

// Builds one float attribute track from an already sampled value list. `timing` is taken by value
// so LoopMode::PingPong doubles each track's own duration independently.
void EmitFloatTrack(XMLBuilder& out, const char* attributeName, const std::vector<float>& values,
                    const std::vector<double>& keyTimes, SmilTiming timing,
                    const std::vector<SmilSpline>& splines, bool useSpline) {
  SmilSequence sequence;
  sequence.useSpline = useSpline;
  for (size_t i = 0; i < values.size(); ++i) {
    sequence.values.push_back(FloatToString(values[i]));
    sequence.keyTimes.push_back(keyTimes[i]);
    if (i > 0 && useSpline) {
      sequence.splines.push_back(splines[i - 1]);
    }
  }
  if (sequence.values.size() < 2) {
    return;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, attributeName, nullptr, false, sequence, timing, nullptr, nullptr);
}

}  // namespace

bool SVGAnimationWriter::WriteShapeGeometryAnimates(XMLBuilder& out, const Node* element,
                                                    const Stroke* stroke) {
  if (!HasShapeGeometryAnimates(element)) {
    return false;
  }
  bool isRectangle = element->nodeType() == NodeType::Rectangle;
  const char* const* names =
      isRectangle ? RectangleGeometryChannelNames : EllipseGeometryChannelNames;
  int count = isRectangle ? RectangleGeometryChannelCount : EllipseGeometryChannelCount;

  const Animation* animation = nullptr;
  const AnimationTimeline* timeline = nullptr;
  const Channel* singleChannel = nullptr;
  int animatedCount = 0;
  std::vector<Frame> frames;
  for (int i = 0; i < count; ++i) {
    auto* binding = _plan->Find(element, names[i]);
    if (binding == nullptr) {
      continue;
    }
    ++animatedCount;
    singleChannel = binding->channel;
    if (animation == nullptr) {
      animation = binding->animation;
      timeline = binding->timeline;
    } else if (animation != binding->animation) {
      return false;
    }
    AppendTrackFrames(binding->channel, binding->animation->duration, &frames);
  }
  if (animation == nullptr || frames.size() < 2) {
    return false;
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }
  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (animatedCount == 1) {
    CollectSegmentSplines(singleChannel, frames, &splines, &useSpline);
  }

  const auto* rect = isRectangle ? static_cast<const Rectangle*>(element) : nullptr;
  const auto* ellipse = isRectangle ? nullptr : static_cast<const Ellipse*>(element);
  Point basePosition = isRectangle ? rect->renderPosition() : ellipse->renderPosition();
  Size baseSize = isRectangle ? rect->renderSize() : ellipse->renderSize();
  float baseRoundness = isRectangle ? rect->roundness : 0.0f;

  std::vector<float> xs;
  std::vector<float> ys;
  std::vector<float> widths;
  std::vector<float> heights;
  std::vector<float> roundnesses;
  std::vector<double> keyTimes;
  for (Frame frame : frames) {
    Point position = basePosition;
    Size size = baseSize;
    float roundness = baseRoundness;
    auto* binding = _plan->Find(element, "position.x");
    if (binding != nullptr) {
      position.x = EvaluateChannelFloat(binding->channel, frame);
    }
    binding = _plan->Find(element, "position.y");
    if (binding != nullptr) {
      position.y = EvaluateChannelFloat(binding->channel, frame);
    }
    binding = _plan->Find(element, "size.width");
    if (binding != nullptr) {
      size.width = EvaluateChannelFloat(binding->channel, frame);
    }
    binding = _plan->Find(element, "size.height");
    if (binding != nullptr) {
      size.height = EvaluateChannelFloat(binding->channel, frame);
    }
    binding = isRectangle ? _plan->Find(element, "roundness") : nullptr;
    if (binding != nullptr) {
      roundness = EvaluateChannelFloat(binding->channel, frame);
    }
    float x = position.x - size.width / 2.0f;
    float y = position.y - size.height / 2.0f;
    float w = size.width;
    float h = size.height;
    ApplyStrokeBoxInset(stroke, x, y, w, h, &roundness);
    xs.push_back(x);
    ys.push_back(y);
    widths.push_back(w);
    heights.push_back(h);
    roundnesses.push_back(std::max(roundness, 0.0f));
    keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }

  if (isRectangle) {
    EmitFloatTrack(out, "x", xs, keyTimes, timing, splines, useSpline);
    EmitFloatTrack(out, "y", ys, keyTimes, timing, splines, useSpline);
    EmitFloatTrack(out, "width", widths, keyTimes, timing, splines, useSpline);
    EmitFloatTrack(out, "height", heights, keyTimes, timing, splines, useSpline);
    bool hasRoundness = false;
    for (auto value : roundnesses) {
      hasRoundness = hasRoundness || value > 0.0f;
    }
    if (hasRoundness) {
      EmitFloatTrack(out, "rx", roundnesses, keyTimes, timing, splines, useSpline);
      EmitFloatTrack(out, "ry", roundnesses, keyTimes, timing, splines, useSpline);
    }
    return true;
  }

  std::vector<float> centersX;
  std::vector<float> centersY;
  std::vector<float> radiiX;
  std::vector<float> radiiY;
  bool alwaysCircular = true;
  for (size_t i = 0; i < xs.size(); ++i) {
    float rx = widths[i] / 2.0f;
    float ry = heights[i] / 2.0f;
    centersX.push_back(xs[i] + rx);
    centersY.push_back(ys[i] + ry);
    radiiX.push_back(rx);
    radiiY.push_back(ry);
    if (!pag::FloatNearlyZero(rx - ry)) {
      alwaysCircular = false;
    }
  }
  bool staticCircular = pag::FloatNearlyZero(baseSize.width / 2.0f - baseSize.height / 2.0f);
  if (staticCircular != alwaysCircular) {
    // The element was written as <circle> or <ellipse> from its static shape, so a channel that
    // would change the aspect ratio cannot be expressed on the already-emitted element.
    AddWarning(
        "shape geometry animation changes the ellipse aspect ratio; the animation is "
        "skipped because the emitted element type is fixed by the static shape.");
    return false;
  }
  EmitFloatTrack(out, "cx", centersX, keyTimes, timing, splines, useSpline);
  EmitFloatTrack(out, "cy", centersY, keyTimes, timing, splines, useSpline);
  if (staticCircular) {
    EmitFloatTrack(out, "r", radiiX, keyTimes, timing, splines, useSpline);
  } else {
    EmitFloatTrack(out, "rx", radiiX, keyTimes, timing, splines, useSpline);
    EmitFloatTrack(out, "ry", radiiY, keyTimes, timing, splines, useSpline);
  }
  return true;
}

bool SVGAnimationWriter::WriteElementTransformAnimates(XMLBuilder& out,
                                                       const SVGGroupTransformStep* steps,
                                                       size_t count,
                                                       const Matrix& staticTransform) {
  if (_plan == nullptr || steps == nullptr || count == 0) {
    return false;
  }
  const Animation* animation = nullptr;
  const AnimationTimeline* timeline = nullptr;
  const Channel* singleChannel = nullptr;
  int animatedChannels = 0;
  std::vector<Frame> frames;
  for (size_t i = 0; i < count; ++i) {
    for (int c = 0; c < GroupTransformChannelCount; ++c) {
      auto* binding = _plan->Find(steps[i].group, GroupTransformChannelNames[c]);
      if (binding == nullptr) {
        continue;
      }
      ++animatedChannels;
      singleChannel = binding->channel;
      if (animation == nullptr) {
        animation = binding->animation;
        timeline = binding->timeline;
      } else if (animation != binding->animation) {
        return false;
      }
      AppendTrackFrames(binding->channel, binding->animation->duration, &frames);
    }
  }
  if (animation == nullptr) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }
  if (frames.size() < 2) {
    return false;
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }

  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (animatedChannels == 1) {
    CollectSegmentSplines(singleChannel, frames, &splines, &useSpline);
  }

  // Rebuilding the chain from the recorded prefixes replaces each animated Group matrix with its
  // per-frame value while leaving every intermediate matrix untouched.
  std::vector<Matrix> matrices;
  matrices.reserve(frames.size());
  for (Frame frame : frames) {
    Matrix matrix = steps[0].prefix * GroupMatrixAtFrame(_plan, steps[0].group, frame);
    for (size_t i = 1; i < count; ++i) {
      Matrix previousStatic = steps[i - 1].prefix * BuildGroupMatrix(steps[i - 1].group);
      matrix = matrix * InvertAffine(previousStatic) * steps[i].prefix *
               GroupMatrixAtFrame(_plan, steps[i].group, frame);
    }
    Matrix lastStatic = steps[count - 1].prefix * BuildGroupMatrix(steps[count - 1].group);
    matrix = matrix * InvertAffine(lastStatic) * staticTransform;
    matrices.push_back(matrix);
  }

  std::vector<double> keyTimes;
  keyTimes.reserve(frames.size());
  for (auto frame : frames) {
    keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  return WriteTransformTracks(out, matrices, keyTimes, timing, splines, useSpline);
}

bool SVGAnimationWriter::WriteLayerOuterAnimates(XMLBuilder& out, const Layer* layer) {
  if (_plan == nullptr || layer == nullptr) {
    return false;
  }
  bool wrote = WriteChannelAnimate(out, layer, "alpha", "opacity");
  wrote = WriteVisibilityAnimate(out, layer) || wrote;
  return wrote;
}

bool SVGAnimationWriter::WriteLayerTransformAnimates(XMLBuilder& out, const Layer* layer) {
  std::vector<Frame> frames;
  std::vector<Matrix> matrices;
  SmilTiming timing;
  Frame durationFrames = 0;
  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (!SampleLayerTransform(_plan, layer, &frames, &matrices, &timing, &durationFrames, &splines,
                            &useSpline)) {
    return false;
  }
  auto span = static_cast<double>(durationFrames);
  if (span <= 0) {
    return false;
  }

  std::vector<double> keyTimes;
  keyTimes.reserve(frames.size());
  for (auto frame : frames) {
    keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  return WriteTransformTracks(out, matrices, keyTimes, timing, splines, useSpline);
}

Matrix SVGAnimationWriter::LayerTransformBaseMatrix(const Layer* layer) const {
  std::vector<Frame> frames;
  std::vector<Matrix> matrices;
  SmilTiming timing;
  Frame durationFrames = 0;
  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (!SampleLayerTransform(_plan, layer, &frames, &matrices, &timing, &durationFrames, &splines,
                            &useSpline) ||
      matrices.empty()) {
    return BuildLayerMatrix(layer);
  }
  return matrices.front();
}

bool SVGAnimationWriter::WriteVisibilityAnimate(XMLBuilder& out, const Node* target) {
  auto* binding = _plan->Find(target, "visible");
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Bool) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(binding->animation, binding->timeline, &timing)) {
    return false;
  }
  SmilSequence sequence;
  std::vector<double> times;
  std::vector<bool> values;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> bezierOuts;
  std::vector<Point> bezierIns;
  if (!BuildTypedTrack<bool>(binding->channel, binding->animation->duration, &times, &values,
                             &types, &bezierOuts, &bezierIns)) {
    return false;
  }
  for (size_t i = 0; i < times.size(); ++i) {
    sequence.values.push_back(values[i] ? "visible" : "hidden");
    sequence.keyTimes.push_back(times[i]);
  }
  if (sequence.values.size() < 2) {
    return false;
  }
  sequence.discrete = true;
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "visibility", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::WriteChannelAnimate(XMLBuilder& out, const Node* target,
                                             const char* channelName, const char* attributeName) {
  return WriteChannelAnimateClamped(out, target, channelName, attributeName,
                                    std::numeric_limits<float>::lowest(),
                                    std::numeric_limits<float>::max());
}

bool SVGAnimationWriter::WriteChannelAnimateClamped(XMLBuilder& out, const Node* target,
                                                    const char* channelName,
                                                    const char* attributeName, float minValue,
                                                    float maxValue) {
  auto* binding = _plan->Find(target, channelName);
  if (binding == nullptr || binding->channel->valueType() != ChannelValueType::Float) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(binding->animation, binding->timeline, &timing)) {
    return false;
  }
  const auto* channel = static_cast<const TypedChannel<float>*>(binding->channel);
  SmilSequence sequence;
  if (!BuildFloatSequence(channel, binding->animation->duration, 1.0f, minValue, maxValue,
                          &sequence)) {
    return false;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, attributeName, nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::WritePainterAnimates(XMLBuilder& out, const Fill* fill,
                                              const Stroke* stroke, float alphaMultiplier) {
  if (_plan == nullptr) {
    return false;
  }
  bool wrote = false;
  if (fill != nullptr) {
    wrote = WritePaintAnimates(out, fill, "fill", alphaMultiplier) || wrote;
  }
  if (stroke != nullptr) {
    wrote = WritePaintAnimates(out, stroke, "stroke", alphaMultiplier) || wrote;
    wrote = WriteChannelAnimate(out, stroke, "width", "stroke-width") || wrote;
    wrote = WriteChannelAnimate(out, stroke, "dashOffset", "stroke-dashoffset") || wrote;
    wrote = WriteChannelAnimate(out, stroke, "miterLimit", "stroke-miterlimit") || wrote;
  }
  return wrote;
}

bool SVGAnimationWriter::HasStopAnimates(const ColorStop* stop) const {
  if (_plan == nullptr || stop == nullptr) {
    return false;
  }
  return _plan->Find(stop, "offset") != nullptr || _plan->Find(stop, "color") != nullptr;
}

bool SVGAnimationWriter::HasStopColorAnimates(const ColorStop* stop) const {
  return _plan != nullptr && stop != nullptr && _plan->Find(stop, "color") != nullptr;
}

bool SVGAnimationWriter::StopColorTrackIsWideGamut(const ColorStop* stop) const {
  if (!HasStopColorAnimates(stop)) {
    return false;
  }
  auto* binding = _plan->Find(stop, "color");
  if (binding->channel->valueType() != ChannelValueType::Color) {
    return false;
  }
  const auto* channel = static_cast<const TypedChannel<Color>*>(binding->channel);
  for (const auto& keyframe : channel->keyframes) {
    if (keyframe.value.colorSpace != ColorSpace::SRGB) {
      return true;
    }
  }
  return false;
}

bool SVGAnimationWriter::WriteStopAnimates(XMLBuilder& out, const ColorStop* stop) {
  if (_plan == nullptr || stop == nullptr) {
    return false;
  }
  bool wrote = WriteChannelAnimate(out, stop, "offset", "offset");
  auto* colorBinding = _plan->Find(stop, "color");
  if (colorBinding == nullptr || colorBinding->channel->valueType() != ChannelValueType::Color) {
    return wrote;
  }
  SmilTiming timing;
  if (!ResolveTiming(colorBinding->animation, colorBinding->timeline, &timing)) {
    return wrote;
  }
  const auto* channel = static_cast<const TypedChannel<Color>*>(colorBinding->channel);
  SmilSequence colorSequence;
  SmilSequence alphaSequence;
  if (!BuildColorSequences(channel, colorBinding->animation->duration, 1.0f, &colorSequence,
                           &alphaSequence)) {
    return wrote;
  }
  if (timing.mirror) {
    ApplyPingPong(&colorSequence);
    ApplyPingPong(&alphaSequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "stop-color", nullptr, false, colorSequence, timing, nullptr, nullptr);
  WriteAnimateElement(out, "stop-opacity", nullptr, false, alphaSequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::HasGradientGeometryAnimates(const Node* gradient) const {
  if (_plan == nullptr || gradient == nullptr) {
    return false;
  }
  if (gradient->nodeType() == NodeType::LinearGradient) {
    static const char* names[] = {"startPoint.x", "startPoint.y", "endPoint.x", "endPoint.y"};
    for (const auto* name : names) {
      if (_plan->Find(gradient, name) != nullptr) {
        return true;
      }
    }
    return false;
  }
  if (gradient->nodeType() == NodeType::RadialGradient) {
    return _plan->Find(gradient, "center.x") != nullptr ||
           _plan->Find(gradient, "center.y") != nullptr ||
           _plan->Find(gradient, "radius") != nullptr;
  }
  return false;
}

bool SVGAnimationWriter::WriteGradientGeometryAnimates(XMLBuilder& out, const Node* gradient) {
  if (!HasGradientGeometryAnimates(gradient)) {
    return false;
  }
  if (gradient->nodeType() == NodeType::LinearGradient) {
    bool wrote = WriteChannelAnimate(out, gradient, "startPoint.x", "x1");
    wrote = WriteChannelAnimate(out, gradient, "startPoint.y", "y1") || wrote;
    wrote = WriteChannelAnimate(out, gradient, "endPoint.x", "x2") || wrote;
    wrote = WriteChannelAnimate(out, gradient, "endPoint.y", "y2") || wrote;
    return wrote;
  }
  bool wrote = WriteChannelAnimate(out, gradient, "center.x", "cx");
  wrote = WriteChannelAnimate(out, gradient, "center.y", "cy") || wrote;
  // A negative radius is invalid in SVG; the sampled keyframes are clamped so the track never
  // carries a value the browser would reject.
  return WriteChannelAnimateClamped(out, gradient, "radius", "r", 0.0f,
                                    std::numeric_limits<float>::max()) ||
         wrote;
}

bool SVGAnimationWriter::HasPathPointAnimates(const Node* path) const {
  if (_plan == nullptr || path == nullptr || path->nodeType() != NodeType::Path) {
    return false;
  }
  std::vector<PathPointBinding> bindings;
  CollectPathPointBindings(_plan, path, &bindings);
  return !bindings.empty();
}

bool SVGAnimationWriter::WritePathDataAnimates(XMLBuilder& out, const Node* node,
                                               const Point& renderPosition, float renderScale) {
  if (_plan == nullptr || node == nullptr || node->nodeType() != NodeType::Path) {
    return false;
  }
  const auto* path = static_cast<const Path*>(node);
  if (path->data == nullptr) {
    return false;
  }
  std::vector<PathPointBinding> bindings;
  CollectPathPointBindings(_plan, node, &bindings);
  if (bindings.empty()) {
    return false;
  }
  const Animation* animation = bindings.front().binding->animation;
  for (const auto& binding : bindings) {
    if (binding.binding->animation != animation) {
      AddWarning("path point channels on <Path> '" + node->id +
                 "' span multiple animations; the d track is dropped.");
      return false;
    }
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, bindings.front().binding->timeline, &timing)) {
    return false;
  }

  std::vector<Frame> frames;
  for (const auto& binding : bindings) {
    AppendTrackFrames(binding.binding->channel, animation->duration, &frames);
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }

  // A single shared easing can ride on calcMode="spline"; mixed per-channel easing has no single
  // keySplines list, so those segments are densified and interpolated linearly.
  bool allHold = true;
  bool splineShared = true;
  std::vector<SmilSpline> sharedSplines;
  for (size_t i = 0; i < bindings.size(); i++) {
    const auto* typed = static_cast<const TypedChannel<float>*>(bindings[i].binding->channel);
    for (size_t k = 0; k + 1 < typed->keyframes.size(); k++) {
      if (!IsHold(typed->keyframes[k].interpolation)) {
        allHold = false;
      }
    }
    std::vector<SmilSpline> splines;
    bool useSpline = false;
    if (!CollectSegmentSplines(bindings[i].binding->channel, frames, &splines, &useSpline)) {
      splineShared = false;
      continue;
    }
    if (i == 0) {
      sharedSplines = splines;
    } else if (splines.size() != sharedSplines.size()) {
      splineShared = false;
    } else {
      for (size_t k = 0; k < splines.size(); k++) {
        if (splines[k].x1 != sharedSplines[k].x1 || splines[k].y1 != sharedSplines[k].y1 ||
            splines[k].x2 != sharedSplines[k].x2 || splines[k].y2 != sharedSplines[k].y2) {
          splineShared = false;
          break;
        }
      }
    }
  }
  bool useSpline = splineShared && !allHold && !sharedSplines.empty();

  if (!allHold && !useSpline) {
    // Add a frame just before each Hold boundary so a linear track still snaps instead of
    // ramping, and sub-sample Bezier segments because the track can only carry one easing.
    std::vector<Frame> detail;
    bool hasCurve = false;
    for (const auto& binding : bindings) {
      const auto* typed = static_cast<const TypedChannel<float>*>(binding.binding->channel);
      for (size_t k = 0; k + 1 < typed->keyframes.size(); k++) {
        Frame start = std::max(typed->keyframes[k].time, Frame(0));
        Frame end = std::min(typed->keyframes[k + 1].time, animation->duration);
        if (end <= start) {
          continue;
        }
        auto type = typed->keyframes[k].interpolation;
        if (IsHold(type)) {
          double gapFrames = std::clamp(static_cast<double>(end - start) * HoldGapRatio,
                                        HoldGapFloor * span, HoldGapCeiling * span);
          auto holdTime = static_cast<Frame>(static_cast<double>(end) - gapFrames);
          if (holdTime > start) {
            detail.push_back(holdTime);
          }
          continue;
        }
        if (type != KeyframeInterpolationType::Bezier) {
          continue;
        }
        hasCurve = true;
        constexpr int SubSamples = 8;
        for (int s = 1; s < SubSamples; s++) {
          detail.push_back(start + static_cast<Frame>((end - start) * s / SubSamples));
        }
      }
    }
    if (!detail.empty()) {
      frames.insert(frames.end(), detail.begin(), detail.end());
      std::sort(frames.begin(), frames.end());
      frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
    }
    if (hasCurve) {
      AddWarning("path point channels on <Path> '" + node->id +
                 "' use different bezier easings; the d track is sampled to keep one interpolation "
                 "track.");
    }
  }

  std::vector<Point> points = path->data->points();
  if (frames.size() > MaxPathDataSamples) {
    std::vector<Frame> reduced;
    reduced.reserve(MaxPathDataSamples);
    for (size_t i = 0; i < MaxPathDataSamples; i++) {
      size_t index = i * (frames.size() - 1) / (MaxPathDataSamples - 1);
      reduced.push_back(frames[index]);
    }
    std::sort(reduced.begin(), reduced.end());
    reduced.erase(std::unique(reduced.begin(), reduced.end()), reduced.end());
    frames = std::move(reduced);
    AddWarning("path point track on <Path> '" + node->id +
               "' exceeded 512 samples and was downsampled; dense bezier segments lose some "
               "intermediate shape.");
  }
  SmilSequence sequence;
  for (auto frame : frames) {
    for (const auto& binding : bindings) {
      if (binding.index >= points.size()) {
        continue;
      }
      auto value = binding.binding->channel->evaluateAt(frame);
      const auto* coordinate = std::get_if<float>(&value);
      if (coordinate == nullptr) {
        continue;
      }
      if (binding.isX) {
        points[binding.index].x = *coordinate;
      } else {
        points[binding.index].y = *coordinate;
      }
    }
    sequence.values.push_back(
        PathDataAtFrame(*path->data, points, renderPosition, renderScale));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  if (sequence.values.size() < 2) {
    return false;
  }
  if (allHold) {
    sequence.discrete = true;
  } else if (useSpline) {
    sequence.useSpline = true;
    sequence.splines = sharedSplines;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "d", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::HasTextPositionAnimates(const Node* text) const {
  if (_plan == nullptr || text == nullptr || text->nodeType() != NodeType::Text) {
    return false;
  }
  return _plan->Find(text, "x") != nullptr || _plan->Find(text, "y") != nullptr;
}

bool SVGAnimationWriter::WriteTextPositionAnimates(XMLBuilder& out, const Node* node) {
  if (!HasTextPositionAnimates(node)) {
    return false;
  }
  auto* xBinding = _plan->Find(node, "x");
  auto* yBinding = _plan->Find(node, "y");
  if (xBinding != nullptr && yBinding != nullptr && xBinding->animation != yBinding->animation) {
    AddWarning("channels 'x' and 'y' on <Text> '" + node->id +
               "' belong to different animations; the translate track is dropped.");
    return false;
  }
  const auto* primary = xBinding != nullptr ? xBinding : yBinding;
  SmilTiming timing;
  if (!ResolveTiming(primary->animation, primary->timeline, &timing)) {
    return false;
  }
  auto span = static_cast<double>(primary->animation->duration);
  if (span <= 0) {
    return false;
  }
  std::vector<Frame> frames;
  if (xBinding != nullptr) {
    AppendTrackFrames(xBinding->channel, primary->animation->duration, &frames);
  }
  if (yBinding != nullptr) {
    AppendTrackFrames(yBinding->channel, primary->animation->duration, &frames);
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }

  const auto* text = static_cast<const Text*>(node);
  SmilSequence sequence;
  for (auto frame : frames) {
    float x = text->position.x;
    float y = text->position.y;
    if (xBinding != nullptr) {
      auto value = xBinding->channel->evaluateAt(frame);
      if (const auto* coordinate = std::get_if<float>(&value)) {
        x = *coordinate;
      }
    }
    if (yBinding != nullptr) {
      auto value = yBinding->channel->evaluateAt(frame);
      if (const auto* coordinate = std::get_if<float>(&value)) {
        y = *coordinate;
      }
    }
    sequence.values.push_back(FloatToString(x - text->position.x) + " " +
                              FloatToString(y - text->position.y));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  // The host <g> carries no static transform, so the animation replaces an empty transform.
  WriteAnimateElement(out, "transform", "translate", true, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::HasPolystarDataAnimates(const Node* polystar) const {
  if (_plan == nullptr || polystar == nullptr || polystar->nodeType() != NodeType::Polystar) {
    return false;
  }
  auto* list = _plan->BindingsFor(polystar);
  if (list == nullptr || list->empty()) {
    return false;
  }
  for (const auto& binding : *list) {
    const auto& name = binding.channel->name;
    // pointCount and reversed change the vertex list or its direction, so they cannot be carried
    // by one interpolated d track.
    if (name != "rotation" && name != "outerRadius" && name != "innerRadius" &&
        name != "outerRoundness" && name != "innerRoundness" && name != "position.x" &&
        name != "position.y") {
      return false;
    }
  }
  return true;
}

bool SVGAnimationWriter::WritePolystarDataAnimates(XMLBuilder& out, const Node* node) {
  if (!HasPolystarDataAnimates(node) || _document == nullptr) {
    return false;
  }
  const auto* polystar = static_cast<const Polystar*>(node);
  auto* list = _plan->BindingsFor(node);
  const Animation* animation = list->front().animation;
  for (const auto& binding : *list) {
    if (binding.animation != animation) {
      AddWarning("geometry channels on <Polystar> '" + node->id +
                 "' span multiple animations; the d track is dropped.");
      return false;
    }
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, list->front().timeline, &timing)) {
    return false;
  }
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }

  std::vector<Frame> frames;
  bool allHold = true;
  bool hasCurve = false;
  std::vector<Frame> detail;
  for (const auto& binding : *list) {
    const auto* typed = static_cast<const TypedChannel<float>*>(binding.channel);
    AppendTrackFrames(binding.channel, animation->duration, &frames);
    for (size_t i = 0; i + 1 < typed->keyframes.size(); i++) {
      auto type = typed->keyframes[i].interpolation;
      if (!IsHold(type)) {
        allHold = false;
      }
      Frame start = std::max<Frame>(typed->keyframes[i].time, 0);
      Frame end = std::min<Frame>(typed->keyframes[i + 1].time, animation->duration);
      if (end <= start) {
        continue;
      }
      if (IsHold(type)) {
        double gapFrames = std::clamp(static_cast<double>(end - start) * HoldGapRatio,
                                      HoldGapFloor * span, HoldGapCeiling * span);
        auto holdTime = static_cast<Frame>(static_cast<double>(end) - gapFrames);
        if (holdTime > start) {
          detail.push_back(holdTime);
        }
      } else if (type == KeyframeInterpolationType::Bezier) {
        hasCurve = true;
        constexpr int SubSamples = 8;
        for (int s = 1; s < SubSamples; s++) {
          detail.push_back(start + static_cast<Frame>((end - start) * s / SubSamples));
        }
      }
    }
  }
  if (hasCurve) {
    AddWarning("geometry channels on <Polystar> '" + node->id +
               "' use bezier easing; the d track is sampled linearly.");
  }
  if (!detail.empty()) {
    frames.insert(frames.end(), detail.begin(), detail.end());
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }
  if (frames.size() > MaxPathDataSamples) {
    std::vector<Frame> reduced;
    reduced.reserve(MaxPathDataSamples);
    for (size_t i = 0; i < MaxPathDataSamples; i++) {
      size_t index = i * (frames.size() - 1) / (MaxPathDataSamples - 1);
      reduced.push_back(frames[index]);
    }
    std::sort(reduced.begin(), reduced.end());
    reduced.erase(std::unique(reduced.begin(), reduced.end()), reduced.end());
    frames = std::move(reduced);
    AddWarning("geometry track on <Polystar> '" + node->id +
               "' exceeded 512 samples and was downsampled; dense bezier segments lose some "
               "intermediate shape.");
  }

  SmilSequence sequence;
  for (auto frame : frames) {
    // Radii, rotation and position all move the content bounds, so the layout-resolved placement
    // is recomputed from a snapshot instead of reusing the static render scale.
    auto* snapshot = _document->makeNode<Polystar>();
    *snapshot = *polystar;
    snapshot->id.clear();
    for (const auto& binding : *list) {
      auto value = binding.channel->evaluateAt(frame);
      const auto* number = std::get_if<float>(&value);
      if (number == nullptr) {
        continue;
      }
      const auto& name = binding.channel->name;
      if (name == "rotation") {
        snapshot->rotation = *number;
      } else if (name == "outerRadius") {
        snapshot->outerRadius = *number;
      } else if (name == "innerRadius") {
        snapshot->innerRadius = *number;
      } else if (name == "outerRoundness") {
        snapshot->outerRoundness = *number;
      } else if (name == "innerRoundness") {
        snapshot->innerRoundness = *number;
      } else if (name == "position.x") {
        snapshot->position.x = *number;
      } else if (name == "position.y") {
        snapshot->position.y = *number;
      }
    }
    PolystarSample sample = SamplePolystar(snapshot);
    sample.forceCurves = true;
    sequence.values.push_back(TGFXPathToSVGString(PolystarToTGFXPath(sample)));
    sequence.keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  if (sequence.values.size() < 2) {
    return false;
  }
  if (allHold) {
    sequence.discrete = true;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "d", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::HasNoiseSeedAnimates(const Node* noise) const {
  if (_plan == nullptr || noise == nullptr || noise->nodeType() != NodeType::NoiseFilter) {
    return false;
  }
  return _plan->Find(noise, "seed") != nullptr;
}

bool SVGAnimationWriter::WriteNoiseSeedAnimates(XMLBuilder& out, const Node* noise) {
  if (!HasNoiseSeedAnimates(noise)) {
    return false;
  }
  return WriteChannelAnimate(out, noise, "seed", "seed");
}

bool SVGAnimationWriter::HasBakedShapeAnimates(const Node* source) const {
  if (_plan == nullptr || source == nullptr) {
    return false;
  }
  auto* list = _plan->BindingsFor(source);
  if (list == nullptr || list->empty()) {
    return false;
  }
  for (const auto& binding : *list) {
    const auto& name = binding.channel->name;
    if (source->nodeType() == NodeType::Rectangle) {
      if (name != "position.x" && name != "position.y" && name != "size.width" &&
          name != "size.height" && name != "roundness") {
        return false;
      }
    } else if (source->nodeType() == NodeType::Ellipse) {
      if (name != "position.x" && name != "position.y" && name != "size.width" &&
          name != "size.height") {
        return false;
      }
    } else if (source->nodeType() == NodeType::Path) {
      size_t index = 0;
      bool isX = false;
      if (!ParsePathPointChannel(name, &index, &isX)) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool SVGAnimationWriter::WriteSampledDataAnimates(XMLBuilder& out, const Node* source,
                                                  std::vector<std::string> values,
                                                  std::vector<double> keyTimes, bool discrete) {
  if (_plan == nullptr || source == nullptr || values.size() < 2 ||
      values.size() != keyTimes.size()) {
    return false;
  }
  auto* list = _plan->BindingsFor(source);
  if (list == nullptr || list->empty()) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(list->front().animation, list->front().timeline, &timing)) {
    return false;
  }
  SmilSequence sequence;
  sequence.values = std::move(values);
  sequence.keyTimes = std::move(keyTimes);
  sequence.discrete = discrete;
  if (timing.mirror) {
    ApplyPingPong(&sequence);
    timing.durationSeconds *= 2.0;
  }
  WriteAnimateElement(out, "d", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::BuildBakedShapeDataTrack(const Path* baked, ModifierResolver* resolver,
                                                  const std::vector<Element*>& scope,
                                                  SVGSampledDataTrack* track) {
  if (_plan == nullptr || baked == nullptr || resolver == nullptr || track == nullptr) {
    return false;
  }
  const Element* source = resolver->sourceOf(baked);
  if (source == nullptr || !HasBakedShapeAnimates(source)) {
    return false;
  }
  auto* list = _plan->BindingsFor(source);
  if (list == nullptr || list->empty()) {
    return false;
  }
  const Animation* animation = list->front().animation;
  for (const auto& binding : *list) {
    if (binding.animation != animation) {
      AddWarning("geometry channels on <" + std::string(NodeTypeName(source->nodeType())) + "> '" +
                 source->id + "' span multiple animations; the d track is dropped.");
      return false;
    }
  }
  auto span = static_cast<double>(animation->duration);
  if (span <= 0) {
    return false;
  }
  auto* sourcePath = source->nodeType() == NodeType::Path
                         ? const_cast<Path*>(static_cast<const Path*>(source))
                         : nullptr;
  if (sourcePath != nullptr && sourcePath->data == nullptr) {
    return false;
  }

  std::vector<Frame> frames;
  bool allHold = true;
  bool hasCurve = false;
  std::vector<Frame> detail;
  for (const auto& binding : *list) {
    const auto* typed = static_cast<const TypedChannel<float>*>(binding.channel);
    AppendTrackFrames(binding.channel, animation->duration, &frames);
    for (size_t i = 0; i + 1 < typed->keyframes.size(); i++) {
      auto type = typed->keyframes[i].interpolation;
      if (!IsHold(type)) {
        allHold = false;
      }
      Frame start = std::max<Frame>(typed->keyframes[i].time, 0);
      Frame end = std::min<Frame>(typed->keyframes[i + 1].time, animation->duration);
      if (end <= start) {
        continue;
      }
      if (IsHold(type)) {
        double gapFrames = std::clamp(static_cast<double>(end - start) * HoldGapRatio,
                                      HoldGapFloor * span, HoldGapCeiling * span);
        auto holdTime = static_cast<Frame>(static_cast<double>(end) - gapFrames);
        if (holdTime > start) {
          detail.push_back(holdTime);
        }
      } else if (type == KeyframeInterpolationType::Bezier) {
        hasCurve = true;
        constexpr int SubSamples = 8;
        for (int s = 1; s < SubSamples; s++) {
          detail.push_back(start + static_cast<Frame>((end - start) * s / SubSamples));
        }
      }
    }
  }
  if (hasCurve) {
    AddWarning("geometry channels on <" + std::string(NodeTypeName(source->nodeType())) + "> '" +
               source->id + "' use bezier easing; the d track is sampled linearly.");
  }
  if (!detail.empty()) {
    frames.insert(frames.end(), detail.begin(), detail.end());
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  if (frames.size() < 2) {
    return false;
  }
  // Each sample re-runs the whole modifier chain, so the grid stays small.
  constexpr size_t MaxBakeSamples = 32;
  if (frames.size() > MaxBakeSamples) {
    std::vector<Frame> reduced;
    reduced.reserve(MaxBakeSamples);
    for (size_t i = 0; i < MaxBakeSamples; i++) {
      size_t index = i * (frames.size() - 1) / (MaxBakeSamples - 1);
      reduced.push_back(frames[index]);
    }
    std::sort(reduced.begin(), reduced.end());
    reduced.erase(std::unique(reduced.begin(), reduced.end()), reduced.end());
    frames = std::move(reduced);
    AddWarning("geometry track on <" + std::string(NodeTypeName(source->nodeType())) + "> '" +
               source->id + "' exceeded 32 re-bake samples and was downsampled.");
  }

  track->values.clear();
  track->keyTimes.clear();
  for (auto frame : frames) {
    ShapeGeometryOverride override = {};
    override.target = source;
    if (source->nodeType() == NodeType::Rectangle) {
      const auto* rect = static_cast<const Rectangle*>(source);
      override.position = rect->renderPosition();
      override.size = rect->renderSize();
      override.roundness = rect->roundness;
      override.hasPosition = true;
      override.hasSize = true;
      override.hasRoundness = true;
      for (const auto& binding : *list) {
        auto value = binding.channel->evaluateAt(frame);
        const auto* number = std::get_if<float>(&value);
        if (number == nullptr) {
          continue;
        }
        const auto& name = binding.channel->name;
        if (name == "position.x") {
          override.position.x = *number;
        } else if (name == "position.y") {
          override.position.y = *number;
        } else if (name == "size.width") {
          override.size.width = *number;
        } else if (name == "size.height") {
          override.size.height = *number;
        } else if (name == "roundness") {
          override.roundness = *number;
        }
      }
    } else if (source->nodeType() == NodeType::Ellipse) {
      const auto* ellipse = static_cast<const Ellipse*>(source);
      override.position = ellipse->renderPosition();
      override.size = ellipse->renderSize();
      override.hasPosition = true;
      override.hasSize = true;
      for (const auto& binding : *list) {
        auto value = binding.channel->evaluateAt(frame);
        const auto* number = std::get_if<float>(&value);
        if (number == nullptr) {
          continue;
        }
        const auto& name = binding.channel->name;
        if (name == "position.x") {
          override.position.x = *number;
        } else if (name == "position.y") {
          override.position.y = *number;
        } else if (name == "size.width") {
          override.size.width = *number;
        } else if (name == "size.height") {
          override.size.height = *number;
        }
      }
    }
    PathData local = PathDataFromSVGString("");
    PathData* savedData = nullptr;
    if (sourcePath != nullptr) {
      std::vector<Point> points = sourcePath->data->points();
      for (const auto& binding : *list) {
        auto value = binding.channel->evaluateAt(frame);
        const auto* number = std::get_if<float>(&value);
        size_t pointIndex = 0;
        bool isX = false;
        if (number == nullptr ||
            !ParsePathPointChannel(binding.channel->name, &pointIndex, &isX) ||
            pointIndex >= points.size()) {
          continue;
        }
        if (isX) {
          points[pointIndex].x = *number;
        } else {
          points[pointIndex].y = *number;
        }
      }
      FillPathDataFromPoints(*sourcePath->data, points, &local);
      savedData = sourcePath->data;
      sourcePath->data = &local;
    }
    std::string d;
    auto resolved = resolver->resolveWithOverride(scope, override);
    if (savedData != nullptr) {
      sourcePath->data = savedData;
    }
    for (auto* element : resolved) {
      if (element->nodeType() != NodeType::Path) {
        continue;
      }
      if (resolver->sourceOf(element) != source) {
        continue;
      }
      d = PathDataToSVGString(*static_cast<const Path*>(element)->data);
      break;
    }
    if (d.empty()) {
      AddWarning("geometry track on <" + std::string(NodeTypeName(source->nodeType())) + "> '" +
                 source->id + "' could not be re-baked through its modifiers.");
      return false;
    }
    track->values.push_back(std::move(d));
    track->keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  if (track->values.size() < 2) {
    return false;
  }
  track->baked = baked;
  track->source = source;
  track->discrete = allHold;
  return true;
}

bool SVGAnimationWriter::WriteWrapTransformAnimates(XMLBuilder& out, const Node* node,
                                                    const Matrix& parent,
                                                    const Matrix& staticMatrix) {
  if (_plan == nullptr || node == nullptr) {
    return false;
  }
  const Animation* animation = nullptr;
  const AnimationTimeline* timeline = nullptr;
  const Channel* singleChannel = nullptr;
  int animatedChannels = 0;
  std::vector<Frame> frames;
  for (int i = 0; i < GroupTransformChannelCount; ++i) {
    auto* binding = _plan->Find(node, GroupTransformChannelNames[i]);
    if (binding == nullptr) {
      continue;
    }
    ++animatedChannels;
    singleChannel = binding->channel;
    if (animation == nullptr) {
      animation = binding->animation;
      timeline = binding->timeline;
    } else if (animation != binding->animation) {
      return false;
    }
    AppendTrackFrames(binding->channel, binding->animation->duration, &frames);
  }
  if (animation == nullptr) {
    return false;
  }
  SmilTiming timing;
  if (!ResolveTiming(animation, timeline, &timing)) {
    return false;
  }
  std::sort(frames.begin(), frames.end());
  frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
  auto span = static_cast<double>(animation->duration);
  if (span <= 0 || frames.size() < 2) {
    return false;
  }
  std::vector<SmilSpline> splines;
  bool useSpline = false;
  if (animatedChannels == 1) {
    CollectSegmentSplines(singleChannel, frames, &splines, &useSpline);
  }
  const auto* group = static_cast<const Group*>(node);
  Matrix inverseStatic = InvertAffine(staticMatrix);
  std::vector<Matrix> matrices;
  std::vector<double> keyTimes;
  for (auto frame : frames) {
    Matrix animated = parent * GroupMatrixAtFrame(_plan, group, frame);
    matrices.push_back(animated * inverseStatic);
    keyTimes.push_back(std::clamp(static_cast<double>(frame) / span, 0.0, 1.0));
  }
  return WriteTransformTracks(out, matrices, keyTimes, timing, splines, useSpline);
}

bool SVGAnimationWriter::WritePaintAnimates(XMLBuilder& out, const Node* painter,
                                            const char* paintAttribute, float alphaMultiplier) {
  auto* colorBinding = _plan->Find(painter, "color");
  auto* alphaBinding = _plan->Find(painter, "alpha");

  float painterAlpha = 1.0f;
  const ColorSource* colorSource = nullptr;
  if (painter->nodeType() == NodeType::Fill) {
    painterAlpha = static_cast<const Fill*>(painter)->alpha;
    colorSource = static_cast<const Fill*>(painter)->color;
  } else if (painter->nodeType() == NodeType::Stroke) {
    painterAlpha = static_cast<const Stroke*>(painter)->alpha;
    colorSource = static_cast<const Stroke*>(painter)->color;
  }
  if (colorBinding == nullptr && colorSource != nullptr &&
      colorSource->nodeType() == NodeType::SolidColor) {
    // A SolidColor resource may be shared by several Fill / Stroke painters. Each consumer emits
    // its own SMIL track so every painted element follows the source binding.
    colorBinding = _plan->Find(colorSource, "color");
  }
  float colorAlpha = StaticColorAlpha(colorSource);

  bool wrote = false;
  if (colorBinding != nullptr && colorBinding->channel->valueType() == ChannelValueType::Color &&
      IsSolidColorPainter(painter)) {
    SmilTiming timing;
    if (ResolveTiming(colorBinding->animation, colorBinding->timeline, &timing)) {
      const auto* channel = static_cast<const TypedChannel<Color>*>(colorBinding->channel);
      SmilSequence colorSequence;
      SmilSequence alphaSequence;
      if (BuildColorSequences(channel, colorBinding->animation->duration,
                              painterAlpha * alphaMultiplier, &colorSequence, &alphaSequence)) {
        if (timing.mirror) {
          ApplyPingPong(&colorSequence);
          ApplyPingPong(&alphaSequence);
          timing.durationSeconds *= 2.0;
        }
        WriteAnimateElement(out, paintAttribute, nullptr, false, colorSequence, timing, nullptr,
                            nullptr);
        wrote = true;
        // Fill.alpha owns fill-opacity when both are animated; the color's own alpha then only
        // survives through the Fill.alpha baseline.
        if (alphaBinding == nullptr) {
          std::string opacityAttribute = std::string(paintAttribute) + "-opacity";
          WriteAnimateElement(out, opacityAttribute.c_str(), nullptr, false, alphaSequence, timing,
                              nullptr, nullptr);
        }
      }
    }
  } else if (colorBinding != nullptr && !IsSolidColorPainter(painter)) {
    AddWarning(std::string("channel 'color' on a gradient or pattern ") +
               (painter->nodeType() == NodeType::Fill ? "fill" : "stroke") +
               " is dropped: SMIL cannot animate a color source reference.");
  }

  if (alphaBinding != nullptr) {
    SmilTiming timing;
    if (ResolveTiming(alphaBinding->animation, alphaBinding->timeline, &timing)) {
      const auto* channel = static_cast<const TypedChannel<float>*>(alphaBinding->channel);
      SmilSequence sequence;
      if (BuildFloatSequence(channel, alphaBinding->animation->duration,
                             colorAlpha * alphaMultiplier,
                             std::numeric_limits<float>::lowest(),
                             std::numeric_limits<float>::max(), &sequence)) {
        if (timing.mirror) {
          ApplyPingPong(&sequence);
          timing.durationSeconds *= 2.0;
        }
        std::string opacityAttribute = std::string(paintAttribute) + "-opacity";
        WriteAnimateElement(out, opacityAttribute.c_str(), nullptr, false, sequence, timing,
                            nullptr, nullptr);
        wrote = true;
      }
    }
  }
  return wrote;
}

float SVGAnimationWriter::StaticColorAlpha(const ColorSource* source) {
  if (source != nullptr && source->nodeType() == NodeType::SolidColor) {
    return static_cast<const SolidColor*>(source)->color.alpha;
  }
  return 1.0f;
}

bool SVGAnimationWriter::IsSolidColorPainter(const Node* painter) {
  const ColorSource* source = nullptr;
  if (painter->nodeType() == NodeType::Fill) {
    source = static_cast<const Fill*>(painter)->color;
  } else if (painter->nodeType() == NodeType::Stroke) {
    source = static_cast<const Stroke*>(painter)->color;
  }
  return source != nullptr && source->nodeType() == NodeType::SolidColor;
}

}  // namespace pagx
