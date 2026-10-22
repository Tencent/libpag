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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace pagx {

// TagCode allocation of the PAG v4 binary format (spec/pag_binary_spec.md §1.3 and §9).
// The 10-bit code space is shared with the legacy format: 0 is End, 1-255 belong to the
// legacy codec, 256-1022 hold the new format, and 1023 is reserved.
enum class TagCode : uint16_t {
  End = 0,
  // 256-287 file level.
  FileAttributes = 256,
  StringTable = 257,
  ResourceBlock = 258,
  DocumentBlock = 259,
  SubDocumentTable = 260,
  SubDocument = 261,
  ImageTable = 262,
  DocumentAttributes = 263,
  RetainedIdTable = 264,
  // 288-351 resources.
  Image = 288,
  PathData = 289,
  SolidColor = 290,
  LinearGradient = 291,
  RadialGradient = 292,
  ConicGradient = 293,
  DiamondGradient = 294,
  ImagePattern = 295,
  Font = 296,
  CompositionBlock = 297,
  CompositionAttributes = 298,
  ViewModelBlock = 299,
  ViewModelProperty = 300,
  DataConverter = 301,
  // 352-415 layers, styles, filters, timeline drivers.
  LayerBlock = 352,
  LayerAttributes = 353,
  LayerTransform = 354,
  DropShadowStyle = 355,
  InnerShadowStyle = 356,
  BackgroundBlurStyle = 357,
  NoiseStyle = 358,
  GlassStyle = 359,
  BlurFilter = 360,
  DropShadowFilter = 361,
  InnerShadowFilter = 362,
  BlendFilter = 363,
  ColorMatrixFilter = 364,
  NoiseFilter = 365,
  AnimationTimeline = 366,
  StateMachineTimeline = 367,
  // 416-479 vector elements and text.
  Rectangle = 416,
  Ellipse = 417,
  Polystar = 418,
  Path = 419,
  TextBlock = 420,
  TextAttributes = 421,
  GlyphRun = 422,
  Fill = 423,
  Stroke = 424,
  TrimPath = 425,
  RoundCorner = 426,
  MergePath = 427,
  TextModifierBlock = 428,
  RangeSelector = 429,
  TextPath = 430,
  TextBoxBlock = 431,
  TextBoxAttributes = 432,
  GroupBlock = 433,
  GroupAttributes = 434,
  Repeater = 435,
  TextModifierAttributes = 436,
  // 480-543 animation and state machines.
  AnimationBlock = 480,
  AnimationAttributes = 489,
  AnimationObjectBlock = 481,
  AnimationObjectAttributes = 492,
  Channel = 482,
  StateMachineBlock = 483,
  StateMachineInput = 484,
  StateRegionBlock = 485,
  StateRegionAttributes = 490,
  State = 486,
  StateTransition = 487,
  StateTransitionAttributes = 491,
  TransitionCondition = 488,
  // 544-575 data binding.
  DataBind = 544,
  // 576-767 is reserved for the phase-3 migration capabilities (design doc §12.2),
  // 768-1022 for the future. Both ranges stay empty until those features land.
};

static constexpr uint16_t MaxTagCode = 1023;

}  // namespace pagx
