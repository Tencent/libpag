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
#include <cstdlib>
#include <cstring>
#include <set>
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/svg/SVGParserContext.h"
#include "pagx/svg/SVGPathParser.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/PathMeasure.h"

namespace pagx {

static void SkipWhitespaceAndComma(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(static_cast<unsigned char>(*ptr)) != 0 || *ptr == ',')) {
    ++ptr;
  }
}

static float ReadNumber(const char*& ptr, const char* end) {
  SkipWhitespaceAndComma(ptr, end);
  char* endPtr = nullptr;
  float num = std::strtof(ptr, &endPtr);
  if (endPtr == ptr) {
    return 0.0f;
  }
  ptr = endPtr;
  return num;
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

std::vector<TransformComponent> SMILAnimationParser::parseTransformComponents(
    const std::string& value) {
  std::vector<TransformComponent> result = {};
  if (value.empty()) {
    return result;
  }

  const char* ptr = value.c_str();
  const char* end = ptr + value.length();

  while (ptr < end) {
    SkipWhitespaceAndComma(ptr, end);
    if (ptr >= end) {
      break;
    }

    const char* funcStart = ptr;
    while (ptr < end && std::isalpha(static_cast<unsigned char>(*ptr)) != 0) {
      ++ptr;
    }
    size_t funcLen = static_cast<size_t>(ptr - funcStart);

    SkipWhitespaceAndComma(ptr, end);
    if (ptr >= end || *ptr != '(') {
      break;
    }
    ++ptr;

    TransformComponent component = {};
    if (funcLen == 9 && std::memcmp(funcStart, "translate", 9) == 0) {
      component.type = TransformComponent::Type::Translate;
      component.params[0] = ReadNumber(ptr, end);
      component.params[1] = ReadNumber(ptr, end);
    } else if (funcLen == 5 && std::memcmp(funcStart, "scale", 5) == 0) {
      component.type = TransformComponent::Type::Scale;
      component.params[0] = ReadNumber(ptr, end);
      SkipWhitespaceAndComma(ptr, end);
      if (ptr < end && *ptr != ')') {
        component.params[1] = ReadNumber(ptr, end);
      } else {
        component.params[1] = component.params[0];
      }
    } else if (funcLen == 6 && std::memcmp(funcStart, "rotate", 6) == 0) {
      component.type = TransformComponent::Type::Rotate;
      component.params[0] = ReadNumber(ptr, end);
      SkipWhitespaceAndComma(ptr, end);
      if (ptr < end && *ptr != ')') {
        component.params[1] = ReadNumber(ptr, end);
        component.params[2] = ReadNumber(ptr, end);
      }
    } else if (funcLen == 5 && std::memcmp(funcStart, "skewX", 5) == 0) {
      component.type = TransformComponent::Type::SkewX;
      component.params[0] = ReadNumber(ptr, end);
    } else if (funcLen == 5 && std::memcmp(funcStart, "skewY", 5) == 0) {
      component.type = TransformComponent::Type::SkewY;
      component.params[0] = ReadNumber(ptr, end);
    } else if (funcLen == 6 && std::memcmp(funcStart, "matrix", 6) == 0) {
      component.type = TransformComponent::Type::Matrix;
      for (int i = 0; i < 6; ++i) {
        component.params[i] = ReadNumber(ptr, end);
      }
    } else {
      // Unknown function: skip to the matching closing paren.
      while (ptr < end && *ptr != ')') {
        ++ptr;
      }
    }

    SkipWhitespaceAndComma(ptr, end);
    if (ptr < end && *ptr == ')') {
      ++ptr;
    }

    result.push_back(component);
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
static void ComputeBezierHandles(KeyframeInterpolationType interpolation,
                                 const std::vector<BezierControlPoints>& splines, size_t i,
                                 size_t keyframeCount, Point* outBezierOut, Point* outBezierIn) {
  *outBezierOut = {};
  *outBezierIn = {};
  if (interpolation == KeyframeInterpolationType::Bezier && i < keyframeCount - 1 &&
      i < splines.size()) {
    outBezierOut->x = static_cast<float>(splines[i].p1x);
    outBezierOut->y = static_cast<float>(splines[i].p1y);
    outBezierIn->x = static_cast<float>(splines[i].p2x);
    outBezierIn->y = static_cast<float>(splines[i].p2y);
  }
}

// Reads repeatCount attribute: returns -1 for "indefinite", 0 for unparseable, otherwise the
// integer count (clamped to >=1). Fractional counts are floored to the integer part.
static int ParseRepeatCount(const std::string& value) {
  if (value == "indefinite") {
    return -1;
  }
  if (value.empty()) {
    return 1;
  }
  double count = std::atof(value.c_str());
  if (count < 1.0) {
    return 1;
  }
  return static_cast<int>(std::floor(count));
}

// Expands a float-typed channel's keyframes to repeat the animation repeatCount times. When
// accumulate is true, each repetition's values are offset by k * (lastValue - firstValue) so the
// animation accumulates across loops. The original keyframes are replaced by the expanded set.
static void ExpandFloatRepeatCount(Channel* channel, Frame durFrames, int repeatCount,
                                   bool accumulate) {
  auto* ch = static_cast<TypedChannel<float>*>(channel);
  auto original = ch->keyframes;
  if (original.empty() || repeatCount <= 1) {
    return;
  }
  float delta = 0.0f;
  if (accumulate && original.size() >= 2) {
    delta = original.back().value - original.front().value;
  }
  ch->keyframes.clear();
  for (int k = 0; k < repeatCount; ++k) {
    float offset = static_cast<float>(k) * delta;
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    for (auto key : original) {
      key.time += timeOffset;
      if (accumulate) {
        key.value += offset;
      }
      ch->keyframes.push_back(key);
    }
  }
}

// Expands a Color-typed channel's keyframes similarly to ExpandFloatRepeatCount, accumulating
// each color channel independently.
static void ExpandColorRepeatCount(Channel* channel, Frame durFrames, int repeatCount,
                                   bool accumulate) {
  auto* ch = static_cast<TypedChannel<Color>*>(channel);
  auto original = ch->keyframes;
  if (original.empty() || repeatCount <= 1) {
    return;
  }
  Color delta = {0, 0, 0, 0};
  if (accumulate && original.size() >= 2) {
    delta.red = original.back().value.red - original.front().value.red;
    delta.green = original.back().value.green - original.front().value.green;
    delta.blue = original.back().value.blue - original.front().value.blue;
    delta.alpha = original.back().value.alpha - original.front().value.alpha;
  }
  ch->keyframes.clear();
  for (int k = 0; k < repeatCount; ++k) {
    Frame timeOffset = static_cast<Frame>(k) * durFrames;
    for (auto key : original) {
      key.time += timeOffset;
      if (accumulate) {
        key.value.red += static_cast<float>(k) * delta.red;
        key.value.green += static_cast<float>(k) * delta.green;
        key.value.blue += static_cast<float>(k) * delta.blue;
        key.value.alpha += static_cast<float>(k) * delta.alpha;
      }
      ch->keyframes.push_back(key);
    }
  }
}

// Dispatches repeatCount expansion to the correct TypedChannel<T> specialization based on
// valueType. Non-numeric types (bool/string) repeat without accumulation.
static void ExpandRepeatCount(Channel* channel, ChannelValueType valueType, Frame durFrames,
                              int repeatCount, bool accumulate) {
  if (repeatCount <= 1) {
    return;
  }
  switch (valueType) {
    case ChannelValueType::Float:
      ExpandFloatRepeatCount(channel, durFrames, repeatCount, accumulate);
      break;
    case ChannelValueType::Color:
      ExpandColorRepeatCount(channel, durFrames, repeatCount, accumulate);
      break;
    case ChannelValueType::Matrix: {
      // Matrix repeats copy keyframes without accumulation. animateTransform accumulate on a
      // baked matrix is intentionally ignored, matching animateMotion's accumulate behaviour.
      auto* ch = static_cast<TypedChannel<Matrix>*>(channel);
      auto original = ch->keyframes;
      if (original.empty()) {
        break;
      }
      ch->keyframes.clear();
      for (int k = 0; k < repeatCount; ++k) {
        Frame timeOffset = static_cast<Frame>(k) * durFrames;
        for (auto key : original) {
          key.time += timeOffset;
          ch->keyframes.push_back(key);
        }
      }
      break;
    }
    default: {
      // For non-numeric types, repeat keyframes without accumulation. Use float as a generic
      // channel since we cannot easily template-dispatch here; but bool/string channels need
      // their own handling. For now, only float and color support repeat expansion.
      break;
    }
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

  std::vector<Keyframe<float>> merged = {};
  for (Frame time : allTimes) {
    float baseVal = std::get<float>(base->evaluateAt(time));
    float additiveVal = std::get<float>(additive->evaluateAt(time));
    Keyframe<float> key = {};
    key.time = time;
    key.value = baseVal + additiveVal;
    // Prefer base's interpolation/bezier when the time matches a base keyframe; otherwise take
    // additive's. Times present in neither (shouldn't happen) default to Linear.
    auto baseIt = std::find_if(baseCh->keyframes.begin(), baseCh->keyframes.end(),
                               [time](const Keyframe<float>& k) { return k.time == time; });
    if (baseIt != baseCh->keyframes.end()) {
      key.interpolation = baseIt->interpolation;
      key.bezierOut = baseIt->bezierOut;
      key.bezierIn = baseIt->bezierIn;
    } else {
      auto addIt = std::find_if(additiveCh->keyframes.begin(), additiveCh->keyframes.end(),
                                [time](const Keyframe<float>& k) { return k.time == time; });
      if (addIt != additiveCh->keyframes.end()) {
        key.interpolation = addIt->interpolation;
        key.bezierOut = addIt->bezierOut;
        key.bezierIn = addIt->bezierIn;
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

  std::vector<Keyframe<Color>> merged = {};
  for (Frame time : allTimes) {
    Color baseVal = std::get<Color>(base->evaluateAt(time));
    Color additiveVal = std::get<Color>(additive->evaluateAt(time));
    Keyframe<Color> key = {};
    key.time = time;
    key.value.red = baseVal.red + additiveVal.red;
    key.value.green = baseVal.green + additiveVal.green;
    key.value.blue = baseVal.blue + additiveVal.blue;
    key.value.alpha = baseVal.alpha + additiveVal.alpha;
    auto baseIt = std::find_if(baseCh->keyframes.begin(), baseCh->keyframes.end(),
                               [time](const Keyframe<Color>& k) { return k.time == time; });
    if (baseIt != baseCh->keyframes.end()) {
      key.interpolation = baseIt->interpolation;
      key.bezierOut = baseIt->bezierOut;
      key.bezierIn = baseIt->bezierIn;
    } else {
      auto addIt = std::find_if(additiveCh->keyframes.begin(), additiveCh->keyframes.end(),
                                [time](const Keyframe<Color>& k) { return k.time == time; });
      if (addIt != additiveCh->keyframes.end()) {
        key.interpolation = addIt->interpolation;
        key.bezierOut = addIt->bezierOut;
        key.bezierIn = addIt->bezierIn;
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
  std::vector<Keyframe<Matrix>> merged = {};
  for (Frame time : allTimes) {
    Matrix baseVal = std::get<Matrix>(base->evaluateAt(time));
    Matrix additiveVal = std::get<Matrix>(additive->evaluateAt(time));
    Keyframe<Matrix> key = {};
    key.time = time;
    key.value = baseVal * additiveVal;
    auto baseIt = std::find_if(baseCh->keyframes.begin(), baseCh->keyframes.end(),
                               [time](const Keyframe<Matrix>& k) { return k.time == time; });
    if (baseIt != baseCh->keyframes.end()) {
      key.interpolation = baseIt->interpolation;
      key.bezierOut = baseIt->bezierOut;
      key.bezierIn = baseIt->bezierIn;
    } else {
      auto addIt = std::find_if(additiveCh->keyframes.begin(), additiveCh->keyframes.end(),
                                [time](const Keyframe<Matrix>& k) { return k.time == time; });
      if (addIt != additiveCh->keyframes.end()) {
        key.interpolation = addIt->interpolation;
        key.bezierOut = addIt->bezierOut;
        key.bezierIn = addIt->bezierIn;
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
        }
      } else {
        // additive="replace": this channel becomes the new base.
        base = (*channels)[idx].channel;
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

// Reads the static (base) value of a Group transform channel (position/rotation/scale/skew).
// Returns 0 for unknown channels; callers that need scale base should handle the {1,1} default
// by reading the Group directly when channelName is scale.x/scale.y.
static float ReadGroupBaseValue(Group* group, const std::string& channelName) {
  if (group == nullptr) {
    return 0.0f;
  }
  if (channelName == "position.x") return group->position.x;
  if (channelName == "position.y") return group->position.y;
  if (channelName == "rotation") return group->rotation;
  if (channelName == "scale.x") return group->scale.x;
  if (channelName == "scale.y") return group->scale.y;
  if (channelName == "skew") return group->skew;
  return 0.0f;
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
static void AddBaseValueKeyframes(Channel* channel, ChannelValueType valueType, KeyValue baseValue,
                                  Frame beginFrames, Frame endFrame, bool fillFreeze) {
  if (beginFrames > 0) {
    AppendKeyframe(channel, valueType, 0, baseValue, KeyframeInterpolationType::Hold, {}, {});
  }
  if (!fillFreeze && endFrame > 0) {
    AppendKeyframe(channel, valueType, endFrame + 1, baseValue, KeyframeInterpolationType::Hold, {},
                   {});
  }
  if (beginFrames > 0 || (!fillFreeze && endFrame > 0)) {
    SortKeyframes(channel, valueType);
  }
}

// Finds the first element of the given NodeType in a Layer's contents (or its animGroup's
// elements when present). Returns nullptr when not found.
Element* SMILAnimationParser::findContentNode(const AnimatedNodeInfo& nodeInfo, NodeType type) {
  if (nodeInfo.animGroup != nullptr) {
    for (auto* el : nodeInfo.animGroup->elements) {
      if (el != nullptr && el->nodeType() == type) {
        return el;
      }
    }
  } else if (nodeInfo.targetLayer != nullptr) {
    for (auto* el : nodeInfo.targetLayer->contents) {
      if (el != nullptr && el->nodeType() == type) {
        return el;
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
    doc->registerNode(node, id);
  }
  return id;
}

ChannelTarget SMILAnimationParser::resolveAnimateTarget(SVGParserContext& ctx, PAGXDocument* doc,
                                                        const std::string& attributeName,
                                                        const AnimatedNodeInfo& nodeInfo) {
  ChannelTarget target = {};
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
      return str == "true" || str == "1";
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

std::vector<Channel*> SMILAnimationParser::parseAnimate(SVGParserContext& ctx, PAGXDocument* doc,
                                                        const std::shared_ptr<DOMNode>& animElement,
                                                        const AnimatedNodeInfo& nodeInfo,
                                                        float frameRate, Frame& outEndFrame,
                                                        std::string& outTargetId) {
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
  if (durSeconds <= 0.0) {
    // No duration: cannot build keyframes.
    return channels;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));

  auto beginStr = ctx.getAttribute(animElement, "begin");
  double beginSeconds = parseSMILClockValue(beginStr);
  Frame beginFrames =
      (beginSeconds > 0.0) ? static_cast<Frame>(std::round(beginSeconds * frameRate)) : 0;

  // Parse values: either explicit `values` list or from/to/by.
  auto valuesStr = ctx.getAttribute(animElement, "values");
  auto fromStr = ctx.getAttribute(animElement, "from");
  auto toStr = ctx.getAttribute(animElement, "to");
  auto byStr = ctx.getAttribute(animElement, "by");

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
  } else {
    // to-only / by-only without base value is not supported in this phase.
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
  channels.push_back(primaryChannel);

  // Special case: "r" on a circle drives both size.width and size.height (r*2).
  if (attributeName == "r" && target.valueType == ChannelValueType::Float) {
    auto* sizeChannel = CreateChannelForType(doc, ChannelValueType::Float, "size.height");
    if (sizeChannel != nullptr) {
      for (size_t i = 0; i < keyValues.size(); ++i) {
        Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
        float r = std::get<float>(keyValues[i]);
        KeyValue sizeValue = r * 2.0f;
        Point bezierOut = {};
        Point bezierIn = {};
        if (interpolation == KeyframeInterpolationType::Bezier && i < keyValues.size() - 1) {
          if (i < splines.size()) {
            bezierOut = {static_cast<float>(splines[i].p1x), static_cast<float>(splines[i].p1y)};
            bezierIn = {static_cast<float>(splines[i].p2x), static_cast<float>(splines[i].p2y)};
          }
        }
        AppendKeyframe(sizeChannel, ChannelValueType::Float, time, sizeValue, interpolation,
                       bezierOut, bezierIn);
      }
      channels.push_back(sizeChannel);
    }
    // Also adjust the primary channel (size.width) values to r*2.
    auto* widthChannel = static_cast<TypedChannel<float>*>(primaryChannel);
    for (auto& key : widthChannel->keyframes) {
      key.value *= 2.0f;
    }
  }

  // Special case: "rx"/"ry" on ellipse need *2 to convert radius to diameter. On <rect>,
  // rx/ry drive roundness (a pixel value) and must NOT be scaled.
  if ((attributeName == "rx" || attributeName == "ry") &&
      target.valueType == ChannelValueType::Float && target.node != nullptr &&
      target.node->nodeType() == NodeType::Ellipse) {
    auto* sizeChannel = static_cast<TypedChannel<float>*>(primaryChannel);
    for (auto& key : sizeChannel->keyframes) {
      key.value *= 2.0f;
    }
  }

  // Special case: "x"/"y" on rectangle need +width/2 or +height/2 to convert top-left to center.
  if ((attributeName == "x" || attributeName == "y") &&
      target.valueType == ChannelValueType::Float) {
    auto* rect = static_cast<Rectangle*>(findContentNode(nodeInfo, NodeType::Rectangle));
    if (rect != nullptr) {
      float offset = (attributeName == "x") ? rect->size.width / 2.0f : rect->size.height / 2.0f;
      auto* posChannel = static_cast<TypedChannel<float>*>(primaryChannel);
      for (auto& key : posChannel->keyframes) {
        key.value += offset;
      }
    }
  }

  // Expand repeatCount: integer N replicates the keyframe sequence N times with time offset.
  // accumulate="sum" offsets each repetition's values by k*(lastValue - firstValue).
  // "indefinite" is handled by buildAnimation (sets LoopMode::Loop), not expanded here.
  auto repeatCountStr = ctx.getAttribute(animElement, "repeatCount");
  int repeatCount = ParseRepeatCount(repeatCountStr);
  bool accumulate = (ctx.getAttribute(animElement, "accumulate") == "sum");
  if (repeatCount > 1) {
    for (auto* ch : channels) {
      ExpandRepeatCount(ch, target.valueType, durFrames, repeatCount, accumulate);
    }
    outEndFrame = beginFrames + durFrames * repeatCount;
  } else {
    outEndFrame = beginFrames + durFrames;
  }

  // Add base-value keyframes for begin-offset and fill="remove" semantics.
  // SMIL default fill is "remove" (element reverts to static state after animation ends).
  // fill="freeze" keeps the last animated value, which is PAGX's natural post-endframe behavior.
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze");
  KeyValue baseValue = ReadBaseValue(target);
  for (auto* ch : channels) {
    AddBaseValueKeyframes(ch, target.valueType, baseValue, beginFrames, outEndFrame, fillFreeze);
  }
  return channels;
}
std::vector<Channel*> SMILAnimationParser::parseSet(SVGParserContext& ctx, PAGXDocument* doc,
                                                    const std::shared_ptr<DOMNode>& setElement,
                                                    const AnimatedNodeInfo& nodeInfo,
                                                    float frameRate, Frame& outEndFrame,
                                                    std::string& outTargetId) {
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
    return channels;
  }

  auto target = resolveAnimateTarget(ctx, doc, attributeName, nodeInfo);
  if (target.node == nullptr) {
    return channels;
  }
  outTargetId = target.nodeId;

  auto beginStr = ctx.getAttribute(setElement, "begin");
  double beginSeconds = parseSMILClockValue(beginStr);
  Frame beginFrames =
      (beginSeconds > 0.0) ? static_cast<Frame>(std::round(beginSeconds * frameRate)) : 0;

  auto durStr = ctx.getAttribute(setElement, "dur");
  double durSeconds = parseSMILClockValue(durStr);

  auto* channel = CreateChannelForType(doc, target.valueType, target.channelName);
  if (channel == nullptr) {
    return channels;
  }

  KeyValue value = parseValue(ctx, toStr, target.valueType);
  AppendKeyframe(channel, target.valueType, beginFrames, value, KeyframeInterpolationType::Hold, {},
                 {});

  // Compute end frame and determine fill mode. <set> defaults to fill="remove": when dur is
  // set the element reverts to base value after begin+dur. When dur is absent the <set> effect
  // is permanent (treated as freeze), matching SMIL semantics for indefinite-duration <set>.
  Frame endFrame = beginFrames;
  bool fillFreeze = (durSeconds <= 0.0) || (ctx.getAttribute(setElement, "fill") == "freeze");
  if (durSeconds > 0.0) {
    endFrame = beginFrames + static_cast<Frame>(std::round(durSeconds * frameRate));
  }
  outEndFrame = endFrame;

  // Add base-value keyframes for begin-offset and fill="remove" semantics.
  KeyValue baseValue = ReadBaseValue(target);
  AddBaseValueKeyframes(channel, target.valueType, baseValue, beginFrames, endFrame, fillFreeze);

  channels.push_back(channel);
  return channels;
}

std::vector<Channel*> SMILAnimationParser::parseAnimateTransform(
    SVGParserContext& ctx, PAGXDocument* doc, const std::shared_ptr<DOMNode>& animElement,
    const AnimatedNodeInfo& nodeInfo, float frameRate, Frame& outEndFrame) {
  std::vector<Channel*> channels = {};
  if (!animElement) {
    return channels;
  }

  // <g> elements carry their renderable children in layer->children, which Group.elements cannot
  // hold (Group is an Element, children are Layers). So no animGroup is created for <g> in
  // registerAnimatedElement, and transform animations cannot drive Group scalar channels. The
  // Layer node exposes a runtime "matrix" channel (TypedChannel<Matrix>) that replaces the
  // layer's static matrix when applied, affecting both contents and children. Bake each
  // animateTransform keyframe into a full Matrix and drive the Layer's matrix channel directly.
  // This also matches SMIL additive="replace" (default) semantics: the baked matrix replaces the
  // layer's static transform. additive="sum" composition across multiple animateTransform elements
  // is handled by MergeAdditiveMatrix in buildAnimation; a single additive="sum" animateTransform
  // does not pre-compose the static matrix (known limitation).
  bool useLayerMatrix = (nodeInfo.animGroup == nullptr && nodeInfo.targetLayer != nullptr);

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
  if (durSeconds <= 0.0) {
    return channels;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));

  auto beginStr = ctx.getAttribute(animElement, "begin");
  double beginSeconds = parseSMILClockValue(beginStr);
  Frame beginFrames =
      (beginSeconds > 0.0) ? static_cast<Frame>(std::round(beginSeconds * frameRate)) : 0;

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

  if (useLayerMatrix) {
    // Bake each keyframe's transform params into a full Matrix on a single "matrix" channel
    // targeting the Layer. Bezier handles are recorded for completeness but the runtime matrix
    // interpolation (MixTGFXMatrix) does not consume them; Linear/Hold is used effectively.
    auto* ch = CreateChannelForType(doc, ChannelValueType::Matrix, "matrix");
    for (size_t i = 0; i < paramSets.size(); ++i) {
      Matrix m = TransformParamsToMatrix(type, paramSets[i]);
      Point bo = {}, bi = {};
      ComputeBezierHandles(interpolation, splines, i, paramSets.size(), &bo, &bi);
      Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
      AppendKeyframe(ch, ChannelValueType::Matrix, time, m, interpolation, bo, bi);
    }
    channels.push_back(ch);

    auto repeatCountStr = ctx.getAttribute(animElement, "repeatCount");
    int repeatCount = ParseRepeatCount(repeatCountStr);
    if (repeatCount > 1) {
      ExpandRepeatCount(ch, ChannelValueType::Matrix, durFrames, repeatCount, false);
      outEndFrame = beginFrames + durFrames * repeatCount;
    } else {
      outEndFrame = beginFrames + durFrames;
    }

    // Base value is the layer's static matrix so fill="remove" reverts to the pre-animation
    // transform and begin-offset holds the static state before the animation starts.
    auto fillStr = ctx.getAttribute(animElement, "fill");
    bool fillFreeze = (fillStr == "freeze");
    AddBaseValueKeyframes(ch, ChannelValueType::Matrix, nodeInfo.targetLayer->matrix, beginFrames,
                          outEndFrame, fillFreeze);
    return channels;
  }

  if (type == "translate") {
    auto* chX = CreateChannelForType(doc, ChannelValueType::Float, "position.x");
    auto* chY = CreateChannelForType(doc, ChannelValueType::Float, "position.y");
    for (size_t i = 0; i < paramSets.size(); ++i) {
      float tx = (paramSets[i].size() > 0) ? paramSets[i][0] : 0.0f;
      float ty = (paramSets[i].size() > 1) ? paramSets[i][1] : 0.0f;
      Point bo = {}, bi = {};
      ComputeBezierHandles(interpolation, splines, i, paramSets.size(), &bo, &bi);
      Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
      AppendKeyframe(chX, ChannelValueType::Float, time, tx, interpolation, bo, bi);
      AppendKeyframe(chY, ChannelValueType::Float, time, ty, interpolation, bo, bi);
    }
    channels.push_back(chX);
    channels.push_back(chY);
  } else if (type == "scale") {
    auto* chX = CreateChannelForType(doc, ChannelValueType::Float, "scale.x");
    auto* chY = CreateChannelForType(doc, ChannelValueType::Float, "scale.y");
    for (size_t i = 0; i < paramSets.size(); ++i) {
      float sx = (paramSets[i].size() > 0) ? paramSets[i][0] : 1.0f;
      float sy = (paramSets[i].size() > 1) ? paramSets[i][1] : sx;
      Point bo = {}, bi = {};
      ComputeBezierHandles(interpolation, splines, i, paramSets.size(), &bo, &bi);
      Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
      AppendKeyframe(chX, ChannelValueType::Float, time, sx, interpolation, bo, bi);
      AppendKeyframe(chY, ChannelValueType::Float, time, sy, interpolation, bo, bi);
    }
    channels.push_back(chX);
    channels.push_back(chY);
  } else if (type == "rotate") {
    auto* chR = CreateChannelForType(doc, ChannelValueType::Float, "rotation");
    bool hasAnchor = false;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    for (size_t i = 0; i < paramSets.size(); ++i) {
      float angle = (paramSets[i].size() > 0) ? paramSets[i][0] : 0.0f;
      // SVG rotate accepts "angle cx cy" three-parameter form: rotation pivots around (cx, cy).
      // PAGX Group has a single static anchor, so we take the first keyframe's cx/cy. When later
      // keyframes carry different centers the rotation is still driven by the angle channel;
      // the pivot mismatch is a known limitation.
      if (paramSets[i].size() >= 3 && !hasAnchor) {
        anchorX = paramSets[i][1];
        anchorY = paramSets[i][2];
        hasAnchor = true;
      }
      Point bo = {}, bi = {};
      ComputeBezierHandles(interpolation, splines, i, paramSets.size(), &bo, &bi);
      Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
      AppendKeyframe(chR, ChannelValueType::Float, time, angle, interpolation, bo, bi);
    }
    if (hasAnchor) {
      nodeInfo.animGroup->anchor = {anchorX, anchorY};
    }
    channels.push_back(chR);
  } else if (type == "skewX" || type == "skewY") {
    auto* chS = CreateChannelForType(doc, ChannelValueType::Float, "skew");
    for (size_t i = 0; i < paramSets.size(); ++i) {
      float angle = (paramSets[i].size() > 0) ? paramSets[i][0] : 0.0f;
      Point bo = {}, bi = {};
      ComputeBezierHandles(interpolation, splines, i, paramSets.size(), &bo, &bi);
      Frame time = beginFrames + static_cast<Frame>(std::round(keyTimes[i] * durFrames));
      AppendKeyframe(chS, ChannelValueType::Float, time, angle, interpolation, bo, bi);
    }
    // skewAxis is a static property of the Group: 0 degrees for skewX, 90 for skewY.
    nodeInfo.animGroup->skewAxis = (type == "skewX") ? 0.0f : 90.0f;
    channels.push_back(chS);
  } else {
    // Unsupported transform type.
    return channels;
  }

  // Expand repeatCount for transform channels (all float-typed).
  auto repeatCountStr = ctx.getAttribute(animElement, "repeatCount");
  int repeatCount = ParseRepeatCount(repeatCountStr);
  bool accumulate = (ctx.getAttribute(animElement, "accumulate") == "sum");
  if (repeatCount > 1) {
    for (auto* ch : channels) {
      ExpandRepeatCount(ch, ChannelValueType::Float, durFrames, repeatCount, accumulate);
    }
    outEndFrame = beginFrames + durFrames * repeatCount;
  } else {
    outEndFrame = beginFrames + durFrames;
  }

  // Add base-value keyframes for begin-offset and fill="remove" semantics. Each channel's base
  // value is read from the Group's current static transform property.
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze");
  for (auto* ch : channels) {
    float baseFloat = ReadGroupBaseValue(nodeInfo.animGroup, ch->name);
    AddBaseValueKeyframes(ch, ChannelValueType::Float, baseFloat, beginFrames, outEndFrame,
                          fillFreeze);
  }
  return channels;
}

std::vector<Channel*> SMILAnimationParser::parseAnimateMotion(
    SVGParserContext& ctx, PAGXDocument* doc, const std::shared_ptr<DOMNode>& animElement,
    const AnimatedNodeInfo& nodeInfo, float frameRate, Frame& outEndFrame) {
  std::vector<Channel*> channels = {};
  if (!animElement) {
    return channels;
  }

  // Like parseAnimateTransform, <g> targets use the Layer's "matrix" channel instead of Group
  // scalar channels. Path position (and optional rotation) are baked into a Matrix per keyframe.
  bool useLayerMatrix = (nodeInfo.animGroup == nullptr && nodeInfo.targetLayer != nullptr);

  // Resolve the motion path: either the `path` attribute on <animateMotion> itself, or an
  // <mpath href="#id"> child referencing a <path> element in defs.
  std::string pathStr = ctx.getAttribute(animElement, "path");
  if (pathStr.empty()) {
    auto child = animElement->getFirstChild();
    while (child) {
      if (child->name == "mpath") {
        std::string href = ctx.getHrefAttribute(child);
        std::string refId = ctx.resolveUrl(href);
        auto defIt = ctx._defs.find(refId);
        if (defIt != ctx._defs.end()) {
          pathStr = ctx.getAttribute(defIt->second, "d");
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
  if (durSeconds <= 0.0) {
    return channels;
  }
  Frame durFrames = static_cast<Frame>(std::round(durSeconds * frameRate));

  auto beginStr = ctx.getAttribute(animElement, "begin");
  double beginSeconds = parseSMILClockValue(beginStr);
  Frame beginFrames =
      (beginSeconds > 0.0) ? static_cast<Frame>(std::round(beginSeconds * frameRate)) : 0;

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
  auto keyPointsStr = ctx.getAttribute(animElement, "keyPoints");
  std::vector<double> keyPoints = {};
  if (!keyPointsStr.empty()) {
    keyPoints = parseKeyTimes(keyPointsStr);
    // keyPoints must start at 0 and end at 1 and be monotonic for well-formed motion.
    if (keyPoints.empty() || keyPoints.front() != 0.0 || keyPoints.back() != 1.0) {
      keyPoints.clear();
    }
  }

  // Determine sample count: adaptive based on path length, clamped to [32, 256]. When keyPoints
  // are provided, the sample count is fixed by the keyPoints list.
  int sampleCount = 0;
  if (!keyPoints.empty()) {
    sampleCount = static_cast<int>(keyPoints.size());
  } else {
    sampleCount = static_cast<int>(totalLength / 10.0f);
    if (sampleCount < 32) sampleCount = 32;
    if (sampleCount > 256) sampleCount = 256;
  }
  if (sampleCount < 2) {
    sampleCount = 2;
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

  if (useLayerMatrix) {
    // Bake each path sample into Translate(pos) * Rotate(angle) (rotation only when auto/auto-
    // reverse; fixed rotation is also baked into each keyframe's matrix). A single "matrix"
    // channel targets the Layer; fill="remove" reverts to the layer's static matrix.
    auto* ch = CreateChannelForType(doc, ChannelValueType::Matrix, "matrix");
    for (int i = 0; i < sampleCount; ++i) {
      double normalizedTime = static_cast<double>(i) / static_cast<double>(sampleCount - 1);
      double pathProgress = normalizedTime;
      if (!keyPoints.empty()) {
        pathProgress = keyPoints[i];
      }
      float distance = static_cast<float>(pathProgress) * totalLength;
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
      Frame time = beginFrames + static_cast<Frame>(std::round(normalizedTime * durFrames));
      AppendKeyframe(ch, ChannelValueType::Matrix, time, m, KeyframeInterpolationType::Linear, {},
                     {});
    }
    channels.push_back(ch);

    auto repeatCountStr = ctx.getAttribute(animElement, "repeatCount");
    int repeatCount = ParseRepeatCount(repeatCountStr);
    if (repeatCount > 1) {
      ExpandRepeatCount(ch, ChannelValueType::Matrix, durFrames, repeatCount, false);
      outEndFrame = beginFrames + durFrames * repeatCount;
    } else {
      outEndFrame = beginFrames + durFrames;
    }
    auto fillStr = ctx.getAttribute(animElement, "fill");
    bool fillFreeze = (fillStr == "freeze");
    AddBaseValueKeyframes(ch, ChannelValueType::Matrix, nodeInfo.targetLayer->matrix, beginFrames,
                          outEndFrame, fillFreeze);
    return channels;
  }

  // Sample the path and build keyframe values for position.x, position.y, and optionally rotation.
  auto* chX = CreateChannelForType(doc, ChannelValueType::Float, "position.x");
  auto* chY = CreateChannelForType(doc, ChannelValueType::Float, "position.y");
  Channel* chR = nullptr;
  if (rotateAuto || rotateAutoReverse) {
    chR = CreateChannelForType(doc, ChannelValueType::Float, "rotation");
  } else if (hasFixedRotation) {
    // Fixed rotation: set as a static base value on the Group instead of animating.
    nodeInfo.animGroup->rotation = fixedRotation;
  }

  for (int i = 0; i < sampleCount; ++i) {
    double normalizedTime = static_cast<double>(i) / static_cast<double>(sampleCount - 1);
    double pathProgress = normalizedTime;
    if (!keyPoints.empty()) {
      pathProgress = keyPoints[i];
    }
    float distance = static_cast<float>(pathProgress) * totalLength;
    tgfx::Point pos = {};
    tgfx::Point tan = {};
    if (!pathMeasure->getPosTan(distance, &pos, &tan)) {
      continue;
    }
    Frame time = beginFrames + static_cast<Frame>(std::round(normalizedTime * durFrames));
    AppendKeyframe(chX, ChannelValueType::Float, time, pos.x, KeyframeInterpolationType::Linear, {},
                   {});
    AppendKeyframe(chY, ChannelValueType::Float, time, pos.y, KeyframeInterpolationType::Linear, {},
                   {});
    if (chR != nullptr) {
      float angle = atan2f(tan.y, tan.x) * 180.0f / static_cast<float>(M_PI);
      if (rotateAutoReverse) {
        angle += 180.0f;
      }
      AppendKeyframe(chR, ChannelValueType::Float, time, angle, KeyframeInterpolationType::Linear,
                     {}, {});
    }
  }
  channels.push_back(chX);
  channels.push_back(chY);
  if (chR != nullptr) {
    channels.push_back(chR);
  }

  // Expand repeatCount for motion channels (all float-typed). Motion paths do not support
  // accumulate="sum" in a meaningful way (the path repeats, values don't accumulate), so
  // accumulate is ignored here.
  auto repeatCountStr = ctx.getAttribute(animElement, "repeatCount");
  int repeatCount = ParseRepeatCount(repeatCountStr);
  if (repeatCount > 1) {
    for (auto* ch : channels) {
      ExpandRepeatCount(ch, ChannelValueType::Float, durFrames, repeatCount, false);
    }
    outEndFrame = beginFrames + durFrames * repeatCount;
  } else {
    outEndFrame = beginFrames + durFrames;
  }

  // Add base-value keyframes for begin-offset and fill="remove" semantics. Each channel's base
  // value is read from the Group's current static transform property (position/rotation).
  auto fillStr = ctx.getAttribute(animElement, "fill");
  bool fillFreeze = (fillStr == "freeze");
  for (auto* ch : channels) {
    float baseFloat = ReadGroupBaseValue(nodeInfo.animGroup, ch->name);
    AddBaseValueKeyframes(ch, ChannelValueType::Float, baseFloat, beginFrames, outEndFrame,
                          fillFreeze);
  }
  return channels;
}

Animation* SMILAnimationParser::buildAnimation(
    SVGParserContext& ctx, PAGXDocument* doc,
    const std::unordered_map<const DOMNode*, SMILAnimationGroup>& smilAnimations,
    const std::unordered_map<const DOMNode*, AnimatedNodeInfo>& animatedNodeMap, float frameRate) {
  if (animatedNodeMap.empty()) {
    return nullptr;
  }

  auto* animation = doc->makeNode<Animation>();
  animation->frameRate = frameRate;
  animation->loop = LoopMode::Once;
  Frame maxEndFrame = 0;
  bool hasIndefiniteLoop = false;

  for (const auto& pair : animatedNodeMap) {
    const auto& nodeInfo = pair.second;
    auto smilIt = smilAnimations.find(pair.first);
    if (smilIt == smilAnimations.end()) {
      continue;
    }
    const auto& group = smilIt->second;

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

    for (const auto& animEl : group.animates) {
      Frame endFrame = 0;
      bool additive = (ctx.getAttribute(animEl, "additive") == "sum");
      std::string targetId;
      auto parsedChannels = parseAnimate(ctx, doc, animEl, nodeInfo, frameRate, endFrame, targetId);
      for (auto* ch : parsedChannels) {
        layerEntries.push_back({ch, additive, targetId});
      }
      if (endFrame > layerEndFrame) {
        layerEndFrame = endFrame;
      }
      auto repeatCount = ctx.getAttribute(animEl, "repeatCount");
      if (repeatCount == "indefinite") {
        hasIndefiniteLoop = true;
      }
    }
    for (const auto& setEl : group.sets) {
      Frame endFrame = 0;
      bool additive = (ctx.getAttribute(setEl, "additive") == "sum");
      std::string targetId;
      auto parsedChannels = parseSet(ctx, doc, setEl, nodeInfo, frameRate, endFrame, targetId);
      for (auto* ch : parsedChannels) {
        layerEntries.push_back({ch, additive, targetId});
      }
      if (endFrame > layerEndFrame) {
        layerEndFrame = endFrame;
      }
    }

    // <g> elements (animGroup == nullptr, targetLayer != nullptr) drive the Layer's "matrix"
    // channel via baked Matrix keyframes instead of Group scalar channels. Collect them into
    // layerEntries so they share the Layer-targeted AnimationObject with <animate>/<set> channels
    // (e.g. opacity), and MergeAdditiveChannels composes multiple additive matrix channels.
    if (nodeInfo.animGroup == nullptr && nodeInfo.targetLayer != nullptr &&
        (!group.animateTransforms.empty() || !group.animateMotions.empty())) {
      for (const auto& transformEl : group.animateTransforms) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(transformEl, "additive") == "sum");
        auto parsedChannels =
            parseAnimateTransform(ctx, doc, transformEl, nodeInfo, frameRate, endFrame);
        for (auto* ch : parsedChannels) {
          layerEntries.push_back({ch, additive, nodeInfo.targetId});
        }
        if (endFrame > layerEndFrame) {
          layerEndFrame = endFrame;
        }
        auto repeatCount = ctx.getAttribute(transformEl, "repeatCount");
        if (repeatCount == "indefinite") {
          hasIndefiniteLoop = true;
        }
      }
      for (const auto& motionEl : group.animateMotions) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(motionEl, "additive") == "sum");
        auto parsedChannels = parseAnimateMotion(ctx, doc, motionEl, nodeInfo, frameRate, endFrame);
        for (auto* ch : parsedChannels) {
          layerEntries.push_back({ch, additive, nodeInfo.targetId});
        }
        if (endFrame > layerEndFrame) {
          layerEndFrame = endFrame;
        }
        auto repeatCount = ctx.getAttribute(motionEl, "repeatCount");
        if (repeatCount == "indefinite") {
          hasIndefiniteLoop = true;
        }
      }
    }

    // Group channels by targetId and create one AnimationObject per target.
    std::unordered_map<std::string, std::vector<ChannelWithAdditive>> groupedByTarget;
    for (const auto& entry : layerEntries) {
      groupedByTarget[entry.targetId].push_back({entry.channel, entry.additive});
    }
    for (auto& [targetId, channels] : groupedByTarget) {
      auto* obj = doc->makeNode<AnimationObject>();
      obj->target = targetId;
      obj->channels = MergeAdditiveChannels(&channels);
      if (!obj->channels.empty()) {
        animation->objects.push_back(obj);
      }
    }
    if (layerEndFrame > maxEndFrame) {
      maxEndFrame = layerEndFrame;
    }

    // AnimationObject for the Group (drives position/scale/rotation/skew via
    // <animateTransform> and position/rotation via <animateMotion>). Separate from the Layer
    // object because PAGX requires each AnimationObject to target a single node.
    bool hasGroupAnimations = nodeInfo.animGroup != nullptr &&
                              (!group.animateTransforms.empty() || !group.animateMotions.empty());
    if (hasGroupAnimations) {
      auto* groupObject = doc->makeNode<AnimationObject>();
      groupObject->target = nodeInfo.animGroupId;
      Frame groupEndFrame = 0;
      std::vector<ChannelWithAdditive> groupChannels = {};
      for (const auto& transformEl : group.animateTransforms) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(transformEl, "additive") == "sum");
        auto parsedChannels =
            parseAnimateTransform(ctx, doc, transformEl, nodeInfo, frameRate, endFrame);
        for (auto* ch : parsedChannels) {
          groupChannels.push_back({ch, additive});
        }
        if (endFrame > groupEndFrame) {
          groupEndFrame = endFrame;
        }
        auto repeatCount = ctx.getAttribute(transformEl, "repeatCount");
        if (repeatCount == "indefinite") {
          hasIndefiniteLoop = true;
        }
      }
      for (const auto& motionEl : group.animateMotions) {
        Frame endFrame = 0;
        bool additive = (ctx.getAttribute(motionEl, "additive") == "sum");
        auto parsedChannels = parseAnimateMotion(ctx, doc, motionEl, nodeInfo, frameRate, endFrame);
        for (auto* ch : parsedChannels) {
          groupChannels.push_back({ch, additive});
        }
        if (endFrame > groupEndFrame) {
          groupEndFrame = endFrame;
        }
        auto repeatCount = ctx.getAttribute(motionEl, "repeatCount");
        if (repeatCount == "indefinite") {
          hasIndefiniteLoop = true;
        }
      }
      groupObject->channels = MergeAdditiveChannels(&groupChannels);
      if (!groupObject->channels.empty()) {
        animation->objects.push_back(groupObject);
      }
      if (groupEndFrame > maxEndFrame) {
        maxEndFrame = groupEndFrame;
      }
    }
  }

  if (animation->objects.empty()) {
    return nullptr;
  }

  animation->duration = maxEndFrame;
  if (hasIndefiniteLoop) {
    animation->loop = LoopMode::Loop;
  }
  return animation;
}

}  // namespace pagx
