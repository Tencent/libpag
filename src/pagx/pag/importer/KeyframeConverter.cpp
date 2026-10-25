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
//  or required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Legacy keyframe migration (P3-5, design doc §5.3/§7.4): converts legacy range-based
// keyframes (startValue/endValue over [startTime, endTime)) into PAGX point keyframes.
// Regular properties convert keyframe-by-keyframe ({startTime, startValue}); spatial
// properties (position/anchor with bezier spatial handles) sample per frame because PAGX
// has no spatial-interpolation concept. Multi-dimensional bezier eases collapse to the
// first dimension's handles (PAGX stores one easing per keyframe).

#include <string>
#include <vector>
#include "pag/file.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Keyframe.h"

namespace pagx {
namespace {

KeyframeInterpolationType ConvertInterpolation(pag::KeyframeInterpolationType type) {
  switch (type) {
    case pag::KeyframeInterpolationType::None:
      return KeyframeInterpolationType::None;
    case pag::KeyframeInterpolationType::Linear:
      return KeyframeInterpolationType::Linear;
    case pag::KeyframeInterpolationType::Bezier:
      return KeyframeInterpolationType::Bezier;
    case pag::KeyframeInterpolationType::Hold:
      return KeyframeInterpolationType::Hold;
  }
  return KeyframeInterpolationType::Hold;
}

// Copies the first dimension's easing handles onto the new keyframe.
void ConvertEase(const std::vector<pag::Point>& bezierOut, const std::vector<pag::Point>& bezierIn,
                 Keyframe<float>* out) {
  if (!bezierOut.empty()) {
    out->bezierOut = {bezierOut[0].x, bezierOut[0].y};
  }
  if (!bezierIn.empty()) {
    out->bezierIn = {bezierIn[0].x, bezierIn[0].y};
  }
}

// Point keyframes for a scalar float channel.
void ConvertFloatChannel(pag::Property<float>* legacy, float frameRateDivisor,
                         TypedChannel<float>* channel) {
  (void)frameRateDivisor;
  auto* animatable = dynamic_cast<pag::AnimatableProperty<float>*>(legacy);
  if (animatable == nullptr) {
    return;
  }
  for (auto* legacyKeyframe : animatable->keyframes) {
    Keyframe<float> keyframe = {};
    keyframe.time = legacyKeyframe->startTime;
    keyframe.value = legacyKeyframe->startValue;
    keyframe.interpolation = ConvertInterpolation(legacyKeyframe->interpolationType);
    ConvertEase(legacyKeyframe->bezierOut, legacyKeyframe->bezierIn, &keyframe);
    channel->keyframes.push_back(keyframe);
  }
}

using FloatKeyframeList = std::vector<Keyframe<float>>;

// Opacity keyframes (uint8 0-255) normalized to the 0-1 alpha range.
void ConvertOpacityChannel(pag::Property<pag::Opacity>* legacy, float frameRate,
                           TypedChannel<float>* channel) {
  (void)frameRate;
  auto* animatable = dynamic_cast<pag::AnimatableProperty<pag::Opacity>*>(legacy);
  if (animatable == nullptr) {
    return;
  }
  for (auto* legacyKeyframe : animatable->keyframes) {
    Keyframe<float> keyframe = {};
    keyframe.time = legacyKeyframe->startTime;
    keyframe.value = legacyKeyframe->startValue / 255.0f;
    keyframe.interpolation = ConvertInterpolation(legacyKeyframe->interpolationType);
    ConvertEase(legacyKeyframe->bezierOut, legacyKeyframe->bezierIn, &keyframe);
    channel->keyframes.push_back(keyframe);
  }
}

// Point keyframes for a two-axis channel pair emitted as two separate float channels
// ("name.x" / "name.y"); spatial interpolation samples per frame.
void ConvertPointChannel(pag::Property<pag::Point>* legacy, const std::string& baseName,
                         bool spatial, float frameRate, PAGXDocument* document,
                         AnimationObject* object) {
  auto* animatable = dynamic_cast<pag::AnimatableProperty<pag::Point>*>(legacy);
  if (animatable == nullptr) {
    return;
  }
  auto* channelX = document->makeNode<TypedChannel<float>>();
  channelX->name = baseName + ".x";
  auto* channelY = document->makeNode<TypedChannel<float>>();
  channelY->name = baseName + ".y";
  if (spatial) {
    // Spatial bezier handles have no PAGX equivalent (design doc §7.4: sample per frame).
    auto first = animatable->keyframes.front()->startTime;
    auto last = animatable->keyframes.back()->endTime;
    for (auto frame = first; frame < last; frame++) {
      auto value = animatable->getValueAt(frame);
      Keyframe<float> keyX = {};
      keyX.time = frame;
      keyX.value = value.x;
      keyX.interpolation = KeyframeInterpolationType::Linear;
      channelX->keyframes.push_back(keyX);
      Keyframe<float> keyY = {};
      keyY.time = frame;
      keyY.value = value.y;
      keyY.interpolation = KeyframeInterpolationType::Linear;
      channelY->keyframes.push_back(keyY);
    }
  } else {
    for (auto* legacyKeyframe : animatable->keyframes) {
      Keyframe<float> keyX = {};
      keyX.time = legacyKeyframe->startTime;
      keyX.value = legacyKeyframe->startValue.x;
      keyX.interpolation = ConvertInterpolation(legacyKeyframe->interpolationType);
      ConvertEase(legacyKeyframe->bezierOut, legacyKeyframe->bezierIn, &keyX);
      channelX->keyframes.push_back(keyX);
      Keyframe<float> keyY = {};
      keyY.time = legacyKeyframe->startTime;
      keyY.value = legacyKeyframe->startValue.y;
      keyY.interpolation = ConvertInterpolation(legacyKeyframe->interpolationType);
      ConvertEase(legacyKeyframe->bezierOut, legacyKeyframe->bezierIn, &keyY);
      channelY->keyframes.push_back(keyY);
    }
  }
  (void)frameRate;
  object->channels.push_back(channelX);
  object->channels.push_back(channelY);
}

}  // namespace

// Position keyframes: the spatial point maps to two scalar channels "x" and "y", sampled
// per frame when the legacy property carries spatial handles.
void ConvertPositionChannel(pag::Property<pag::Point>* legacy, float frameRate,
                            PAGXDocument* document, AnimationObject* object) {
  auto* animatable = dynamic_cast<pag::AnimatableProperty<pag::Point>*>(legacy);
  if (animatable == nullptr) {
    return;
  }
  auto* channelX = document->makeNode<TypedChannel<float>>();
  channelX->name = "x";
  auto* channelY = document->makeNode<TypedChannel<float>>();
  channelY->name = "y";
  auto first = animatable->keyframes.front()->startTime;
  auto last = animatable->keyframes.back()->endTime;
  for (auto frame = first; frame < last; frame++) {
    auto value = animatable->getValueAt(frame);
    Keyframe<float> keyX = {};
    keyX.time = frame;
    keyX.value = value.x;
    keyX.interpolation = KeyframeInterpolationType::Linear;
    channelX->keyframes.push_back(keyX);
    Keyframe<float> keyY = {};
    keyY.time = frame;
    keyY.value = value.y;
    keyY.interpolation = KeyframeInterpolationType::Linear;
    channelY->keyframes.push_back(keyY);
  }
  (void)frameRate;
  object->channels.push_back(channelX);
  object->channels.push_back(channelY);
}

// Public entry used by LegacyPAGImporter: migrates one legacy layer's Transform2D animation
// into channels on the given object (target = the layer's converted id).
void MigrateTransform2DAnimation(pag::Transform2D* transform, float frameRate,
                                 PAGXDocument* document, AnimationObject* object) {
  if (transform == nullptr) {
    return;
  }
  if (transform->anchorPoint != nullptr && transform->anchorPoint->animatable()) {
    ConvertPointChannel(transform->anchorPoint, "anchor", true, frameRate, document, object);
  }
  if (transform->position != nullptr && transform->position->animatable()) {
    // Legacy position is the layer translation: it maps to the x / y channels, not anchor.
    ConvertPositionChannel(transform->position, frameRate, document, object);
  }
  if (transform->xPosition != nullptr && transform->xPosition->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "x";
    ConvertFloatChannel(transform->xPosition, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (transform->yPosition != nullptr && transform->yPosition->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "y";
    ConvertFloatChannel(transform->yPosition, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (transform->scale != nullptr && transform->scale->animatable()) {
    ConvertPointChannel(transform->scale, "scale", false, frameRate, document, object);
  }
  if (transform->rotation != nullptr && transform->rotation->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "rotation";
    ConvertFloatChannel(transform->rotation, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (transform->opacity != nullptr && transform->opacity->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "alpha";
    ConvertOpacityChannel(transform->opacity, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// In/out points (design doc §7.4): the legacy layer's [startTime, startTime + duration)
// visible window becomes a Hold "visible" channel — true from the start, false outside.
void MigrateLayerVisibility(pag::Layer* legacyLayer, PAGXDocument* document,
                            AnimationObject* object) {
  if (legacyLayer == nullptr) {
    return;
  }
  auto* channel = document->makeNode<TypedChannel<bool>>();
  channel->name = "visible";
  Keyframe<bool> on = {};
  on.time = legacyLayer->startTime;
  on.value = true;
  on.interpolation = KeyframeInterpolationType::Hold;
  channel->keyframes.push_back(on);
  // The visible window closes at the out point; when the window covers the whole composition
  // no closing keyframe is needed (the Hold keeps true forever).
  auto outPoint = legacyLayer->startTime + legacyLayer->duration;
  auto containing = legacyLayer->containingComposition;
  auto containingFrameCount =
      containing != nullptr
          ? static_cast<Frame>(llround(static_cast<double>(containing->duration) *
                                       static_cast<double>(containing->frameRate)))
          : 0;
  if (outPoint < containingFrameCount) {
    Keyframe<bool> off = {};
    off.time = outPoint;
    off.value = false;
    off.interpolation = KeyframeInterpolationType::Hold;
    channel->keyframes.push_back(off);
  }
  object->channels.push_back(channel);
}

}  // namespace pagx
