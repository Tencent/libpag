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
#include "base/utils/MathUtil.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationTimeline.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/runtime/MatrixDecompose.h"
#include "pagx/svg/SVGAnimationPlan.h"
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

// Clamps keyframes into the animation span, collapses duplicate timestamps, and pads both ends so
// the emitted keyTimes starts at 0 and ends at 1. Padding with the boundary value mirrors the
// runtime's clamp-to-end policy in EvaluateKeyframeSequence.
template <typename T>
bool NormalizeKeyframes(const std::vector<Keyframe<T>>& keyframes, Frame duration,
                        std::vector<double>* times, std::vector<T>* values,
                        std::vector<KeyframeInterpolationType>* types,
                        std::vector<Point>* bezierOuts, std::vector<Point>* bezierIns) {
  if (keyframes.empty() || duration <= 0) {
    return false;
  }
  auto span = static_cast<double>(duration);
  for (const auto& keyframe : keyframes) {
    double time = std::clamp(static_cast<double>(keyframe.time) / span, 0.0, 1.0);
    if (!times->empty() && time < times->back()) {
      continue;
    }
    if (!times->empty() && time == times->back()) {
      values->back() = keyframe.value;
      types->back() = keyframe.interpolation;
      bezierOuts->back() = keyframe.bezierOut;
      bezierIns->back() = keyframe.bezierIn;
      continue;
    }
    times->push_back(time);
    values->push_back(keyframe.value);
    types->push_back(keyframe.interpolation);
    bezierOuts->push_back(keyframe.bezierOut);
    bezierIns->push_back(keyframe.bezierIn);
  }
  if (times->empty()) {
    return false;
  }
  if (times->front() > 0) {
    times->insert(times->begin(), 0.0);
    values->insert(values->begin(), values->front());
    types->insert(types->begin(), KeyframeInterpolationType::Linear);
    bezierOuts->insert(bezierOuts->begin(), Point{});
    bezierIns->insert(bezierIns->begin(), Point{});
  }
  if (times->back() < 1.0) {
    times->push_back(1.0);
    values->push_back(values->back());
    types->push_back(KeyframeInterpolationType::Linear);
    bezierOuts->push_back(Point{});
    bezierIns->push_back(Point{});
  }
  return times->size() >= 2;
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
// each segment's easing reversed. Other loop modes just repeat the forward sequence.
void ApplyPingPong(SmilSequence* sequence, double* durationSeconds) {
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
  *durationSeconds *= 2.0;
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

bool BuildFloatSequence(const std::vector<Keyframe<float>>& keyframes, Frame duration, float scale,
                        SmilSequence* sequence) {
  std::vector<double> times;
  std::vector<float> values;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> bezierOuts;
  std::vector<Point> bezierIns;
  if (!NormalizeKeyframes(keyframes, duration, &times, &values, &types, &bezierOuts, &bezierIns)) {
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
      sequence->values.push_back(FloatToString(values[i] * scale));
      sequence->keyTimes.push_back(times[i]);
    }
    return true;
  }
  std::vector<double> flatTimes;
  std::vector<float> flatValues;
  std::vector<SmilSpline> flatSplines;
  FlattenKeyframes(times, values, types, bezierOuts, bezierIns, &flatTimes, &flatValues,
                   &flatSplines, &sequence->useSpline);
  if (scale != 1.0f) {
    for (auto& value : flatValues) {
      value *= scale;
    }
  }
  AppendFloatSequence(flatTimes, flatValues, flatSplines, sequence->useSpline, sequence);
  return true;
}

bool BuildColorSequences(const std::vector<Keyframe<Color>>& keyframes, Frame duration,
                         float alphaScale, SmilSequence* colorSequence,
                         SmilSequence* alphaSequence) {
  std::vector<double> times;
  std::vector<Color> values;
  std::vector<KeyframeInterpolationType> types;
  std::vector<Point> bezierOuts;
  std::vector<Point> bezierIns;
  if (!NormalizeKeyframes(keyframes, duration, &times, &values, &types, &bezierOuts, &bezierIns)) {
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

void AppendKeyframeTimes(const Channel* channel, std::vector<Frame>* frames) {
  if (channel == nullptr) {
    return;
  }
  if (channel->valueType() == ChannelValueType::Matrix) {
    const auto* typed = static_cast<const TypedChannel<Matrix>*>(channel);
    for (const auto& keyframe : typed->keyframes) {
      frames->push_back(keyframe.time);
    }
    return;
  }
  const auto* typed = static_cast<const TypedChannel<float>*>(channel);
  for (const auto& keyframe : typed->keyframes) {
    frames->push_back(keyframe.time);
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

  AppendKeyframeTimes(binding->channel, frames);
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
    double mirrored = timing.durationSeconds;
    for (int t = 0; t < 4; ++t) {
      ApplyPingPong(tracks[t], &mirrored);
    }
    timing.durationSeconds = mirrored;
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
                                       std::vector<std::string>* warnings)
    : _plan(plan), _warnings(warnings) {
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
    AppendKeyframeTimes(binding->channel, &frames);
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
    ApplyPingPong(&sequence, &timing.durationSeconds);
  }
  WriteAnimateElement(out, attributeName, nullptr, false, sequence, timing, nullptr, nullptr);
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
    ApplyPingPong(&sequence, &timing.durationSeconds);
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
    AppendKeyframeTimes(binding->channel, &frames);
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
      AppendKeyframeTimes(binding->channel, &frames);
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
  const auto* channel = static_cast<const TypedChannel<bool>*>(binding->channel);
  if (channel->keyframes.empty()) {
    return false;
  }
  SmilSequence sequence;
  auto span = static_cast<double>(binding->animation->duration);
  for (const auto& keyframe : channel->keyframes) {
    double time = std::clamp(static_cast<double>(keyframe.time) / span, 0.0, 1.0);
    if (!sequence.keyTimes.empty() && time < sequence.keyTimes.back()) {
      continue;
    }
    sequence.values.push_back(keyframe.value ? "visible" : "hidden");
    sequence.keyTimes.push_back(time);
  }
  if (sequence.keyTimes.empty()) {
    return false;
  }
  if (sequence.keyTimes.front() > 0) {
    sequence.values.insert(sequence.values.begin(), sequence.values.front());
    sequence.keyTimes.insert(sequence.keyTimes.begin(), 0.0);
  }
  if (sequence.keyTimes.back() < 1) {
    sequence.values.push_back(sequence.values.back());
    sequence.keyTimes.push_back(1.0);
  }
  if (sequence.values.size() < 2) {
    return false;
  }
  sequence.discrete = true;
  WriteAnimateElement(out, "visibility", nullptr, false, sequence, timing, nullptr, nullptr);
  return true;
}

bool SVGAnimationWriter::WriteChannelAnimate(XMLBuilder& out, const Node* target,
                                             const char* channelName, const char* attributeName) {
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
  if (!BuildFloatSequence(channel->keyframes, binding->animation->duration, 1.0f, &sequence)) {
    return false;
  }
  if (timing.mirror) {
    ApplyPingPong(&sequence, &timing.durationSeconds);
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
  float colorAlpha = StaticColorAlpha(colorSource);

  bool wrote = false;
  if (colorBinding != nullptr && colorBinding->channel->valueType() == ChannelValueType::Color &&
      IsSolidColorPainter(painter)) {
    SmilTiming timing;
    if (ResolveTiming(colorBinding->animation, colorBinding->timeline, &timing)) {
      const auto* channel = static_cast<const TypedChannel<Color>*>(colorBinding->channel);
      SmilSequence colorSequence;
      SmilSequence alphaSequence;
      if (BuildColorSequences(channel->keyframes, colorBinding->animation->duration,
                              painterAlpha * alphaMultiplier, &colorSequence, &alphaSequence)) {
        if (timing.mirror) {
          double mirrored = timing.durationSeconds;
          ApplyPingPong(&colorSequence, &mirrored);
          ApplyPingPong(&alphaSequence, &mirrored);
          timing.durationSeconds = mirrored;
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
      if (BuildFloatSequence(channel->keyframes, alphaBinding->animation->duration,
                             colorAlpha * alphaMultiplier, &sequence)) {
        if (timing.mirror) {
          ApplyPingPong(&sequence, &timing.durationSeconds);
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
