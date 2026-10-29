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

#include <cmath>
#include <string>
#include <typeinfo>
#include <vector>
#include "base/utils/MathUtil.h"
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
  // The legacy keyframe is a segment [startTime, endTime] interpolating startValue to
  // endValue; contiguous keys carry the next start as their end, but the FINAL segment
  // end value has no successor key: without appending it the evaluator clamps later
  // times to the last start value.

  if (!animatable->keyframes.empty()) {
    auto* last = animatable->keyframes.back();
    if (last->endTime > last->startTime) {
      Keyframe<float> keyframe = {};
      keyframe.time = last->endTime;
      keyframe.value = last->endValue;
      keyframe.interpolation = ConvertInterpolation(last->interpolationType);
      ConvertEase(last->bezierOut, last->bezierIn, &keyframe);
      channel->keyframes.push_back(keyframe);
    }
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

  if (!animatable->keyframes.empty()) {
    auto* last = animatable->keyframes.back();
    if (last->endTime > last->startTime) {
      Keyframe<float> keyframe = {};
      keyframe.time = last->endTime;
      keyframe.value = last->endValue / 255.0f;
      keyframe.interpolation = ConvertInterpolation(last->interpolationType);
      ConvertEase(last->bezierOut, last->bezierIn, &keyframe);
      channel->keyframes.push_back(keyframe);
    }
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
    // The final legacy segment [startTime, endTime] carries an end value with no successor
    // key; append it so later times clamp to the segment end (see ConvertFloatChannel).
    if (!animatable->keyframes.empty()) {
      auto* lastKey = animatable->keyframes.back();
      if (lastKey->endTime > lastKey->startTime) {
        Keyframe<float> endX = {};
        endX.time = lastKey->endTime;
        endX.value = lastKey->endValue.x;
        endX.interpolation = ConvertInterpolation(lastKey->interpolationType);
        ConvertEase(lastKey->bezierOut, lastKey->bezierIn, &endX);
        channelX->keyframes.push_back(endX);
        Keyframe<float> endY = {};
        endY.time = lastKey->endTime;
        endY.value = lastKey->endValue.y;
        endY.interpolation = ConvertInterpolation(lastKey->interpolationType);
        ConvertEase(lastKey->bezierOut, lastKey->bezierIn, &endY);
        channelY->keyframes.push_back(endY);
      }
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

// Group transform channels: the Group node exposes position as a point ("position.x"/"position.y"),
// unlike Layer's separated x/y fields, so position routes through ConvertPointChannel.
void MigrateGroupTransformAnimation(pag::ShapeTransform* transform, float frameRate,
                                    PAGXDocument* document, AnimationObject* object) {
  if (transform == nullptr) {
    return;
  }
  if (transform->anchorPoint != nullptr && transform->anchorPoint->animatable()) {
    ConvertPointChannel(transform->anchorPoint, "anchor", true, frameRate, document, object);
  }
  if (transform->position != nullptr && transform->position->animatable()) {
    ConvertPointChannel(transform->position, "position", true, frameRate, document, object);
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
  if (transform->skew != nullptr && transform->skew->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "skew";
    ConvertFloatChannel(transform->skew, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (transform->skewAxis != nullptr && transform->skewAxis->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "skewAxis";
    ConvertFloatChannel(transform->skewAxis, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (transform->opacity != nullptr && transform->opacity->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "alpha";
    ConvertOpacityChannel(transform->opacity, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// Glow effect channels (spec 7.5): threshold/radius/intensity are plain float channels on the
// GlowFilter node. Legacy effect keyframes are composition-absolute (FrameCache evaluates at
// contentFrame + startTime), so the raw keyframe times carry over without a layer offset.
void MigrateGlowEffectAnimation(pag::GlowEffect* effect, float frameRate, PAGXDocument* document,
                                AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* properties[3] = {effect->glowThreshold, effect->glowRadius,
                                         effect->glowIntensity};
  const char* channelNames[3] = {"glowThreshold", "glowRadius", "glowIntensity"};
  for (size_t i = 0; i < 3; i++) {
    auto* legacy = properties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = channelNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// Mosaic effect channels (spec 7.5): the block counts are uint16 legacy properties but float
// PAGX channels, so the keyframes convert value-by-value (interpolation and bezier eases carry
// over, and the final segment endpoint is appended like ConvertFloatChannel does). sharpColors
// animates as a Hold bool channel. Legacy effect keyframes are composition-absolute, so raw
// keyframe times carry over without a layer offset.
static void ConvertMosaicBlockChannel(pag::Property<uint16_t>* legacy,
                                      TypedChannel<float>* channel) {
  auto* animatable = dynamic_cast<pag::AnimatableProperty<uint16_t>*>(legacy);
  if (animatable == nullptr) {
    return;
  }
  for (auto* legacyKeyframe : animatable->keyframes) {
    Keyframe<float> keyframe = {};
    keyframe.time = legacyKeyframe->startTime;
    keyframe.value = static_cast<float>(legacyKeyframe->startValue);
    keyframe.interpolation = ConvertInterpolation(legacyKeyframe->interpolationType);
    ConvertEase(legacyKeyframe->bezierOut, legacyKeyframe->bezierIn, &keyframe);
    channel->keyframes.push_back(keyframe);
  }
  if (!animatable->keyframes.empty()) {
    auto* last = animatable->keyframes.back();
    if (last->endTime > last->startTime) {
      Keyframe<float> keyframe = {};
      keyframe.time = last->endTime;
      keyframe.value = static_cast<float>(last->endValue);
      keyframe.interpolation = ConvertInterpolation(last->interpolationType);
      ConvertEase(last->bezierOut, last->bezierIn, &keyframe);
      channel->keyframes.push_back(keyframe);
    }
  }
}

void MigrateMosaicEffectAnimation(pag::MosaicEffect* effect, float frameRate,
                                  PAGXDocument* document, AnimationObject* object) {
  (void)frameRate;
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<uint16_t>* blockProperties[2] = {effect->horizontalBlocks, effect->verticalBlocks};
  const char* blockChannelNames[2] = {"horizontalBlocks", "verticalBlocks"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = blockProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = blockChannelNames[i];
    ConvertMosaicBlockChannel(legacy, channel);
    object->channels.push_back(channel);
  }
  if (effect->sharpColors != nullptr && effect->sharpColors->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<bool>*>(effect->sharpColors);
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto* channel = document->makeNode<TypedChannel<bool>>();
      channel->name = "sharpColors";
      for (auto* legacyKeyframe : animatable->keyframes) {
        Keyframe<bool> keyframe = {};
        keyframe.time = legacyKeyframe->startTime;
        keyframe.value = legacyKeyframe->startValue;
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      object->channels.push_back(channel);
    }
  }
}

// RadialBlur effect channels (spec 7.5): amount is a plain float channel and the center is a
// SPATIAL point property, so it routes through ConvertPointChannel's per-frame sampling
// (spatial bezier handles have no PAGX equivalent). The mode/antialias enums carry no PAGX
// animation channels and stay static. Legacy effect keyframes are composition-absolute, so
// raw keyframe times carry over without a layer offset.
void MigrateRadialBlurEffectAnimation(pag::RadialBlurEffect* effect, float frameRate,
                                      PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  if (effect->amount != nullptr && effect->amount->animatable()) {
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = "amount";
    ConvertFloatChannel(effect->amount, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (effect->center != nullptr && effect->center->animatable()) {
    ConvertPointChannel(effect->center, "center", true, frameRate, document, object);
  }
}

// CornerPin effect channels (spec 7.5): the four corners are SPATIAL point properties, so
// they route through ConvertPointChannel's per-frame sampling (spatial bezier handles have no
// PAGX equivalent). Legacy effect keyframes are composition-absolute, so raw keyframe times
// carry over without a layer offset.
void MigrateCornerPinEffectAnimation(pag::CornerPinEffect* effect, float frameRate,
                                     PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<pag::Point>* cornerProperties[4] = {effect->upperLeft, effect->upperRight,
                                                    effect->lowerLeft, effect->lowerRight};
  const char* cornerNames[4] = {"upperLeft", "upperRight", "lowerLeft", "lowerRight"};
  for (int i = 0; i < 4; i++) {
    auto* legacy = cornerProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    ConvertPointChannel(legacy, cornerNames[i], true, frameRate, document, object);
  }
}

// Bulge effect channels (spec 7.5): plain float channels for the radii/height/taper, a spatial
// point channel for the center, and a Hold bool channel for pinning. The legacy renderer
// ignores taperRadius (kept for data fidelity). Legacy effect keyframes are
// composition-absolute, so raw keyframe times carry over without a layer offset.
void MigrateBulgeEffectAnimation(pag::BulgeEffect* effect, float frameRate, PAGXDocument* document,
                                 AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* floatProperties[4] = {effect->horizontalRadius, effect->verticalRadius,
                                              effect->bulgeHeight, effect->taperRadius};
  const char* floatNames[4] = {"horizontalRadius", "verticalRadius", "bulgeHeight", "taperRadius"};
  for (int i = 0; i < 4; i++) {
    auto* legacy = floatProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = floatNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (effect->bulgeCenter != nullptr && effect->bulgeCenter->animatable()) {
    ConvertPointChannel(effect->bulgeCenter, "bulgeCenter", true, frameRate, document, object);
  }
  if (effect->pinning != nullptr && effect->pinning->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<bool>*>(effect->pinning);
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto* channel = document->makeNode<TypedChannel<bool>>();
      channel->name = "pinning";
      for (auto* legacyKeyframe : animatable->keyframes) {
        Keyframe<bool> keyframe = {};
        keyframe.time = legacyKeyframe->startTime;
        keyframe.value = legacyKeyframe->startValue;
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      object->channels.push_back(channel);
    }
  }
}

// MotionTile effect channels (spec 7.5): plain float channels for the sizes and phase, a
// spatial point channel for the tile center, and Hold bool channels for the flags. Legacy
// effect keyframes are composition-absolute, so raw keyframe times carry over without a layer
// offset.
void MigrateMotionTileEffectAnimation(pag::MotionTileEffect* effect, float frameRate,
                                      PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* floatProperties[5] = {effect->tileWidth, effect->tileHeight,
                                              effect->outputWidth, effect->outputHeight,
                                              effect->phase};
  const char* floatNames[5] = {"tileWidth", "tileHeight", "outputWidth", "outputHeight", "phase"};
  for (int i = 0; i < 5; i++) {
    auto* legacy = floatProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = floatNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
  if (effect->tileCenter != nullptr && effect->tileCenter->animatable()) {
    ConvertPointChannel(effect->tileCenter, "tileCenter", true, frameRate, document, object);
  }
  pag::Property<bool>* boolProperties[2] = {effect->mirrorEdges, effect->horizontalPhaseShift};
  const char* boolNames[2] = {"mirrorEdges", "horizontalPhaseShift"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = boolProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* animatable = dynamic_cast<pag::AnimatableProperty<bool>*>(legacy);
    if (animatable == nullptr || animatable->keyframes.empty()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<bool>>();
    channel->name = boolNames[i];
    for (auto* legacyKeyframe : animatable->keyframes) {
      Keyframe<bool> keyframe = {};
      keyframe.time = legacyKeyframe->startTime;
      keyframe.value = legacyKeyframe->startValue;
      keyframe.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(keyframe);
    }
    object->channels.push_back(channel);
  }
}

// LevelsIndividual effect channels (spec 7.5): twenty plain float channels covering the
// global/red/green/blue level values. Legacy effect keyframes are composition-absolute, so raw
// keyframe times carry over without a layer offset.
void MigrateLevelsIndividualEffectAnimation(pag::LevelsIndividualEffect* effect, float frameRate,
                                            PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* properties[20] = {
      effect->inputBlack,      effect->inputWhite,       effect->gamma,
      effect->outputBlack,     effect->outputWhite,      effect->redInputBlack,
      effect->redInputWhite,   effect->redGamma,         effect->redOutputBlack,
      effect->redOutputWhite,  effect->greenInputBlack,  effect->greenInputWhite,
      effect->greenGamma,      effect->greenOutputBlack, effect->greenOutputWhite,
      effect->blueInputBlack,  effect->blueInputWhite,   effect->blueGamma,
      effect->blueOutputBlack, effect->blueOutputWhite};
  const char* channelNames[20] = {
      "inputBlack",      "inputWhite",      "gamma",      "outputBlack",      "outputWhite",
      "redInputBlack",   "redInputWhite",   "redGamma",   "redOutputBlack",   "redOutputWhite",
      "greenInputBlack", "greenInputWhite", "greenGamma", "greenOutputBlack", "greenOutputWhite",
      "blueInputBlack",  "blueInputWhite",  "blueGamma",  "blueOutputBlack",  "blueOutputWhite"};
  for (int i = 0; i < 20; i++) {
    auto* legacy = properties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = channelNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// HueSaturation effect channels (spec 7.5): only the colorize triplet animates in the legacy
// (the hue/saturation/lightness slots are plain vectors). Legacy effect keyframes are
// composition-absolute, so raw keyframe times carry over without a layer offset.
void MigrateHueSaturationEffectAnimation(pag::HueSaturationEffect* effect, float frameRate,
                                         PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* properties[3] = {effect->colorizeHue, effect->colorizeSaturation,
                                         effect->colorizeLightness};
  const char* channelNames[3] = {"colorizeHue", "colorizeSaturation", "colorizeLightness"};
  for (int i = 0; i < 3; i++) {
    auto* legacy = properties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = channelNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// BrightnessContrast effect channels (spec 7.5): brightness and contrast animate as float
// channels. The useOldVersion flag is data-only (the legacy renderer ignores it). Legacy
// effect keyframes are composition-absolute, so raw keyframe times carry over without a layer
// offset.
void MigrateBrightnessContrastEffectAnimation(pag::BrightnessContrastEffect* effect,
                                              float frameRate, PAGXDocument* document,
                                              AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  pag::Property<float>* properties[2] = {effect->brightness, effect->contrast};
  const char* channelNames[2] = {"brightness", "contrast"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = properties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = channelNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
}

// DisplacementMap effect channels (spec 7.5): the max displacements animate as float channels
// and the flags as Hold bool channels; the source/behavior enums carry no PAGX animation
// channels and stay static. Legacy effect keyframes are composition-absolute, so raw keyframe
// times carry over without a layer offset.
void MigrateDisplacementMapEffectAnimation(pag::DisplacementMapEffect* effect, float frameRate,
                                           PAGXDocument* document, AnimationObject* object) {
  if (effect == nullptr || object == nullptr) {
    return;
  }
  // The legacy gates the whole effect off once the referenced map layer leaves its time window
  // (DisplacementMapEffect::visibleAt), which the renderer reproduces by skipping the filter
  // entirely; a Hold bool channel on the filter carries that window. The other visibleAt
  // conditions (both selectors const, or both max displacements zero) collapse to identity on
  // their own in the shader.
  auto* mapLayer = effect->displacementMapLayer;
  if (mapLayer != nullptr) {
    auto* channel = document->makeNode<TypedChannel<bool>>();
    channel->name = "enabled";
    auto start = mapLayer->startTime;
    auto end = mapLayer->startTime + mapLayer->duration;
    if (start > 0) {
      Keyframe<bool> off = {};
      off.time = 0;
      off.value = false;
      off.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(off);
    }
    Keyframe<bool> on = {};
    on.time = start;
    on.value = true;
    on.interpolation = KeyframeInterpolationType::Hold;
    channel->keyframes.push_back(on);
    auto* containing = mapLayer->containingComposition;
    auto containingFrameCount =
        containing != nullptr
            ? static_cast<Frame>(llround(static_cast<double>(containing->duration) *
                                         static_cast<double>(containing->frameRate)))
            : 0;
    if (end < containingFrameCount) {
      Keyframe<bool> off = {};
      off.time = end;
      off.value = false;
      off.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(off);
    }
    object->channels.push_back(channel);
  }
  pag::Property<float>* floatProperties[2] = {effect->maxHorizontalDisplacement,
                                              effect->maxVerticalDisplacement};
  const char* floatNames[2] = {"maxHorizontalDisplacement", "maxVerticalDisplacement"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = floatProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = floatNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    object->channels.push_back(channel);
  }
  pag::Property<bool>* boolProperties[2] = {effect->edgeBehavior, effect->expandOutput};
  const char* boolNames[2] = {"edgeBehavior", "expandOutput"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = boolProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* animatable = dynamic_cast<pag::AnimatableProperty<bool>*>(legacy);
    if (animatable == nullptr || animatable->keyframes.empty()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<bool>>();
    channel->name = boolNames[i];
    for (auto* legacyKeyframe : animatable->keyframes) {
      Keyframe<bool> keyframe = {};
      keyframe.time = legacyKeyframe->startTime;
      keyframe.value = legacyKeyframe->startValue;
      keyframe.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(keyframe);
    }
    object->channels.push_back(channel);
  }
  // The behavior and source selectors animate as discrete properties in the legacy; they ride Int
  // Hold channels (the PAGX channel system has no enum value type), each key holding its
  // segment's start value. The legacy and PAGX enums share their element order, so the int value
  // round-trips through static_cast on both the writer and the reader side.
  pag::Property<pag::DisplacementMapBehavior>* behavior = effect->displacementMapBehavior;
  if (behavior != nullptr && behavior->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<uint8_t>*>(
        reinterpret_cast<pag::Property<uint8_t>*>(static_cast<void*>(behavior)));
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto* channel = document->makeNode<TypedChannel<int>>();
      channel->name = "displacementMapBehavior";
      for (auto* legacyKeyframe : animatable->keyframes) {
        Keyframe<int> keyframe = {};
        keyframe.time = legacyKeyframe->startTime;
        keyframe.value = static_cast<int>(legacyKeyframe->startValue);
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      // The final legacy segment's end value has no successor key; without this trailing key
      // every later frame would hold the last start value instead of the segment's end value.
      auto* lastKeyframe = animatable->keyframes.back();
      if (lastKeyframe->endTime > lastKeyframe->startTime) {
        Keyframe<int> endKeyframe = {};
        endKeyframe.time = lastKeyframe->endTime;
        endKeyframe.value = static_cast<int>(lastKeyframe->endValue);
        endKeyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(endKeyframe);
      }
      object->channels.push_back(channel);
    }
  }
  // The effect compositing option rides an alpha channel: the legacy gates the whole effect off
  // when the opacity hits zero (visibleAt), which the shader's effectOpacity mix reproduces.
  auto* opacity = effect->effectOpacity;
  if (opacity != nullptr) {
    auto staticOpaque = opacity->getValueAt(0) >= 255;
    if (opacity->animatable() || !staticOpaque) {
      auto* channel = document->makeNode<TypedChannel<float>>();
      channel->name = "effectOpacity";
      if (opacity->animatable()) {
        ConvertOpacityChannel(opacity, frameRate, channel);
      } else {
        Keyframe<float> keyframe = {};
        keyframe.time = 0;
        keyframe.value = opacity->getValueAt(0) / 255.0f;
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      object->channels.push_back(channel);
    }
  }
  pag::Property<pag::DisplacementMapSource>* sources[2] = {effect->useForHorizontalDisplacement,
                                                           effect->useForVerticalDisplacement};
  const char* sourceNames[2] = {"useForHorizontalDisplacement", "useForVerticalDisplacement"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = sources[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* animatable = dynamic_cast<pag::AnimatableProperty<uint8_t>*>(
        reinterpret_cast<pag::Property<uint8_t>*>(static_cast<void*>(legacy)));
    if (animatable == nullptr || animatable->keyframes.empty()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<int>>();
    channel->name = sourceNames[i];
    for (auto* legacyKeyframe : animatable->keyframes) {
      Keyframe<int> keyframe = {};
      keyframe.time = legacyKeyframe->startTime;
      keyframe.value = static_cast<int>(legacyKeyframe->startValue);
      keyframe.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(keyframe);
    }
    auto* lastKeyframe = animatable->keyframes.back();
    if (lastKeyframe->endTime > lastKeyframe->startTime) {
      Keyframe<int> endKeyframe = {};
      endKeyframe.time = lastKeyframe->endTime;
      endKeyframe.value = static_cast<int>(lastKeyframe->endValue);
      endKeyframe.interpolation = KeyframeInterpolationType::Hold;
      channel->keyframes.push_back(endKeyframe);
    }
    object->channels.push_back(channel);
  }
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

// Returns true when any sibling in the containing composition uses this layer as its track
// matte source (the target side of the pag::Layer::trackMatteLayer back-reference).
static bool IsTrackMatteSource(pag::Layer* layer) {
  auto* composition = layer->containingComposition;
  if (composition == nullptr || composition->type() != pag::CompositionType::Vector) {
    return false;
  }
  for (auto* sibling : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (sibling != nullptr && sibling->trackMatteLayer == layer) {
      return true;
    }
  }
  return false;
}

// In/out points (design doc §7.4): the legacy layer's [startTime, startTime + duration)
// visible window becomes a Hold "visible" channel — true from the start, false outside.
// An isActive=false layer never draws as regular content in the legacy, but its content stays
// readable by referencing effects. Track matte sources keep the window channel: the runtime
// needs them visible for tgfx hasValidMask() and suppresses their drawing via maskOwner.
// Other inactive layers (displacement map sources, guide layers) have no drawing suppression,
// so the static visible=false must stay authoritative — no channel may flip it on inside the
// time window.
void MigrateLayerVisibility(pag::Layer* legacyLayer, PAGXDocument* document,
                            AnimationObject* object) {
  if (legacyLayer == nullptr) {
    return;
  }
  if (!legacyLayer->isActive && !IsTrackMatteSource(legacyLayer)) {
    return;
  }
  auto* channel = document->makeNode<TypedChannel<bool>>();
  channel->name = "visible";
  // TrackMatteIsEmpty semantics (legacy Modifier::MakeMask): a NON-inverted matte whose source
  // left its in/out window skips the target entirely, so the window is the intersection of both.
  // An INVERTED matte with an empty source keeps the target fully visible (MakeMask returns
  // null), which is exactly what the runtime already does when the mask layer turns invisible
  // (tgfx drops the mask), so inverted mattes keep their own window.
  auto startTime = legacyLayer->startTime;
  auto duration = legacyLayer->duration;
  auto matteType = legacyLayer->trackMatteType;
  auto invertedMatte = matteType == pag::TrackMatteType::AlphaInverted ||
                       matteType == pag::TrackMatteType::LumaInverted;
  if (legacyLayer->trackMatteLayer != nullptr && !invertedMatte) {
    auto* source = legacyLayer->trackMatteLayer;
    auto start = std::max(startTime, source->startTime);
    auto end = std::min(startTime + duration, source->startTime + source->duration);
    startTime = start;
    duration = end > start ? end - start : 0;
  }
  // A delayed in point means the layer is hidden before it: without this leading key the
  // Hold evaluation clamps to the first keyframe and the layer leaks visible from time 0.
  if (startTime > 0) {
    Keyframe<bool> off = {};
    off.time = 0;
    off.value = false;
    off.interpolation = KeyframeInterpolationType::Hold;
    channel->keyframes.push_back(off);
  }
  if (duration > 0) {
    Keyframe<bool> on = {};
    on.time = startTime;
    on.value = true;
    on.interpolation = KeyframeInterpolationType::Hold;
    channel->keyframes.push_back(on);
  }
  // The visible window closes at the out point; when the window covers the whole composition
  // no closing keyframe is needed (the Hold keeps true forever).
  auto outPoint = startTime + duration;
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

// Channels for the gradient overlay style's animated properties. The gradient colors, fill type
// and blend mode are static in practice (the importer warns when they animate, mirroring the
// effect skeleton); reverse rides a Hold bool channel. The offset point flattens into the two
// offsetX/offsetY channels the style node exposes.
void MigrateGradientOverlayStyleAnimation(pag::GradientOverlayStyle* style, float frameRate,
                                          PAGXDocument* document, AnimationObject* object) {
  if (style == nullptr || object == nullptr) {
    return;
  }
  auto* opacity = style->opacity;
  if (opacity != nullptr) {
    auto staticOpaque = opacity->getValueAt(0) >= 255;
    if (opacity->animatable() || !staticOpaque) {
      auto* channel = document->makeNode<TypedChannel<float>>();
      channel->name = "alpha";
      if (opacity->animatable()) {
        ConvertOpacityChannel(opacity, frameRate, channel);
      } else {
        Keyframe<float> keyframe = {};
        keyframe.time = 0;
        keyframe.value = opacity->getValueAt(0) / 255.0f;
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      object->channels.push_back(channel);
    }
  }
  pag::Property<float>* floatProperties[2] = {style->angle, style->scale};
  const char* floatNames[2] = {"angle", "scale"};
  for (int i = 0; i < 2; i++) {
    auto* legacy = floatProperties[i];
    if (legacy == nullptr || !legacy->animatable()) {
      continue;
    }
    auto* channel = document->makeNode<TypedChannel<float>>();
    channel->name = floatNames[i];
    ConvertFloatChannel(legacy, frameRate, channel);
    if (!channel->keyframes.empty()) {
      object->channels.push_back(channel);
    }
  }
  if (style->offset != nullptr && style->offset->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<pag::Point>*>(style->offset);
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto* channelX = document->makeNode<TypedChannel<float>>();
      channelX->name = "offsetX";
      auto* channelY = document->makeNode<TypedChannel<float>>();
      channelY->name = "offsetY";
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
      auto* last = animatable->keyframes.back();
      if (last->endTime > last->startTime) {
        Keyframe<float> keyX = {};
        keyX.time = last->endTime;
        keyX.value = last->endValue.x;
        keyX.interpolation = ConvertInterpolation(last->interpolationType);
        ConvertEase(last->bezierOut, last->bezierIn, &keyX);
        channelX->keyframes.push_back(keyX);
        Keyframe<float> keyY = {};
        keyY.time = last->endTime;
        keyY.value = last->endValue.y;
        keyY.interpolation = ConvertInterpolation(last->interpolationType);
        ConvertEase(last->bezierOut, last->bezierIn, &keyY);
        channelY->keyframes.push_back(keyY);
      }
      object->channels.push_back(channelX);
      object->channels.push_back(channelY);
    }
  }
  if (style->reverse != nullptr && style->reverse->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<bool>*>(style->reverse);
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto* channel = document->makeNode<TypedChannel<bool>>();
      channel->name = "reverse";
      for (auto* legacyKeyframe : animatable->keyframes) {
        Keyframe<bool> keyframe = {};
        keyframe.time = legacyKeyframe->startTime;
        keyframe.value = legacyKeyframe->startValue;
        keyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(keyframe);
      }
      auto* last = animatable->keyframes.back();
      if (last->endTime > last->startTime) {
        Keyframe<bool> endKeyframe = {};
        endKeyframe.time = last->endTime;
        endKeyframe.value = last->endValue;
        endKeyframe.interpolation = KeyframeInterpolationType::Hold;
        channel->keyframes.push_back(endKeyframe);
      }
      object->channels.push_back(channel);
    }
  }
}

// Channels for the drop shadow style's animated properties. The polar angle/distance pair
// flattens into per-frame-sampled offsetX/offsetY channels (interpolating polar samples
// linearly would bow the shadow path), size maps through the static spread factor into the
// blur pair, and color plus opacity combine into one per-frame color channel carrying the
// alpha.
void MigrateDropShadowStyleAnimation(pag::DropShadowStyle* style, float frameRate,
                                     PAGXDocument* document, AnimationObject* object) {
  (void)frameRate;
  if (style == nullptr || object == nullptr) {
    return;
  }
  auto* angleAnimatable = style->angle != nullptr && style->angle->animatable()
                              ? dynamic_cast<pag::AnimatableProperty<float>*>(style->angle)
                              : nullptr;
  auto* distanceAnimatable = style->distance != nullptr && style->distance->animatable()
                                 ? dynamic_cast<pag::AnimatableProperty<float>*>(style->distance)
                                 : nullptr;
  if (angleAnimatable != nullptr || distanceAnimatable != nullptr) {
    auto first = pag::Frame{0};
    auto last = pag::Frame{0};
    if (angleAnimatable != nullptr && !angleAnimatable->keyframes.empty()) {
      first = angleAnimatable->keyframes.front()->startTime;
      last = angleAnimatable->keyframes.back()->endTime;
    }
    if (distanceAnimatable != nullptr && !distanceAnimatable->keyframes.empty()) {
      if (distanceAnimatable->keyframes.front()->startTime < first || first == last) {
        first = distanceAnimatable->keyframes.front()->startTime;
      }
      last = std::max(last, distanceAnimatable->keyframes.back()->endTime);
    }
    auto* channelX = document->makeNode<TypedChannel<float>>();
    channelX->name = "offsetX";
    auto* channelY = document->makeNode<TypedChannel<float>>();
    channelY->name = "offsetY";
    for (auto frame = first; frame < last; frame++) {
      auto distance = style->distance->getValueAt(frame);
      auto angle = style->angle->getValueAt(frame);
      auto offsetX = 0.0f;
      auto offsetY = 0.0f;
      if (distance > 0.0f) {
        auto radians = pag::DegreesToRadians(angle - 180.0f);
        offsetX = cosf(radians) * distance;
        offsetY = -sinf(radians) * distance;
      }
      Keyframe<float> keyX = {};
      keyX.time = frame;
      keyX.value = offsetX;
      keyX.interpolation = KeyframeInterpolationType::Linear;
      channelX->keyframes.push_back(keyX);
      Keyframe<float> keyY = {};
      keyY.time = frame;
      keyY.value = offsetY;
      keyY.interpolation = KeyframeInterpolationType::Linear;
      channelY->keyframes.push_back(keyY);
    }
    object->channels.push_back(channelX);
    object->channels.push_back(channelY);
  }
  if (style->size != nullptr && style->size->animatable()) {
    auto* animatable = dynamic_cast<pag::AnimatableProperty<float>*>(style->size);
    if (animatable != nullptr && !animatable->keyframes.empty()) {
      auto spread = style->spread != nullptr ? style->spread->getValueAt(0) : 0.0f;
      auto* channelX = document->makeNode<TypedChannel<float>>();
      channelX->name = "blurX";
      auto* channelY = document->makeNode<TypedChannel<float>>();
      channelY->name = "blurY";
      for (auto frame = animatable->keyframes.front()->startTime;
           frame < animatable->keyframes.back()->endTime; frame++) {
        auto blur = style->size->getValueAt(frame) * (1.0f - spread) / 2.0f;
        Keyframe<float> keyX = {};
        keyX.time = frame;
        keyX.value = blur;
        keyX.interpolation = KeyframeInterpolationType::Linear;
        channelX->keyframes.push_back(keyX);
        Keyframe<float> keyY = {};
        keyY.time = frame;
        keyY.value = blur;
        keyY.interpolation = KeyframeInterpolationType::Linear;
        channelY->keyframes.push_back(keyY);
      }
      object->channels.push_back(channelX);
      object->channels.push_back(channelY);
    }
  }
  auto* colorAnimatable = style->color != nullptr && style->color->animatable()
                              ? dynamic_cast<pag::AnimatableProperty<pag::Color>*>(style->color)
                              : nullptr;
  auto* opacityAnimatable =
      style->opacity != nullptr && style->opacity->animatable()
          ? dynamic_cast<pag::AnimatableProperty<pag::Opacity>*>(style->opacity)
          : nullptr;
  if (colorAnimatable != nullptr || opacityAnimatable != nullptr) {
    auto first = pag::Frame{0};
    auto last = pag::Frame{0};
    if (colorAnimatable != nullptr && !colorAnimatable->keyframes.empty()) {
      first = colorAnimatable->keyframes.front()->startTime;
      last = colorAnimatable->keyframes.back()->endTime;
    }
    if (opacityAnimatable != nullptr && !opacityAnimatable->keyframes.empty()) {
      if (opacityAnimatable->keyframes.front()->startTime < first || first == last) {
        first = opacityAnimatable->keyframes.front()->startTime;
      }
      last = std::max(last, opacityAnimatable->keyframes.back()->endTime);
    }
    auto* channel = document->makeNode<TypedChannel<Color>>();
    channel->name = "color";
    for (auto frame = first; frame < last; frame++) {
      auto color = style->color->getValueAt(frame);
      auto opacity = style->opacity->getValueAt(frame);
      Keyframe<Color> keyframe = {};
      keyframe.time = frame;
      keyframe.value = {color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f,
                        opacity / 255.0f, ColorSpace::SRGB};
      keyframe.interpolation = KeyframeInterpolationType::Linear;
      channel->keyframes.push_back(keyframe);
    }
    object->channels.push_back(channel);
  }
}

}  // namespace pagx
