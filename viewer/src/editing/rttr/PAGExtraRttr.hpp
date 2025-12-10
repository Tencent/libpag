/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#pragma once
#include <rttr/registration.h>
#include "pag/file.h"
#include "pag/types.h"

inline void RegisterExtraTypes() {
#define REGISTER_PAG_PROPERTY(type, pname, aname, kname)                                        \
  rttr::registration::class_<pag::Property<type>>(pname).property("value",                      \
                                                                  &pag::Property<type>::value); \
  rttr::registration::class_<pag::AnimatableProperty<type>>(aname).property(                    \
      "keyframes", &pag::AnimatableProperty<type>::keyframes);                                  \
  rttr::registration::class_<pag::Keyframe<type>>(kname)                                        \
      .property("startValue", &pag::Keyframe<type>::startValue)                                 \
      .property("endValue", &pag::Keyframe<type>::endValue)                                     \
      .property("startTime", &pag::Keyframe<type>::startTime)                                   \
      .property("endTime", &pag::Keyframe<type>::endTime)                                       \
      .property("interpolationType", &pag::Keyframe<type>::interpolationType)                   \
      .property("bezierOut", &pag::Keyframe<type>::bezierOut)                                   \
      .property("bezierIn", &pag::Keyframe<type>::bezierIn)                                     \
      .property("spatialOut", &pag::Keyframe<type>::spatialOut)                                 \
      .property("spatialIn", &pag::Keyframe<type>::spatialIn);

  REGISTER_PAG_PROPERTY(float, "pag::Property<float>", "pag::AnimatableProperty<float>",
                        "pag::Keyframe<float>");
  REGISTER_PAG_PROPERTY(pag::Point, "pag::Property<pag::Point>",
                        "pag::AnimatableProperty<pag::Point>", "pag::Keyframe<pag::Point>");
  REGISTER_PAG_PROPERTY(pag::Point3D, "pag::Property<pag::Point3D>",
                        "pag::AnimatableProperty<pag::Point3D>", "pag::Keyframe<pag::Point3D>");
  REGISTER_PAG_PROPERTY(pag::Opacity, "pag::Property<pag::Opacity>",
                        "pag::AnimatableProperty<pag::Opacity>", "pag::Keyframe<pag::Opacity>");
  REGISTER_PAG_PROPERTY(pag::PathHandle, "pag::Property<pag::PathHandle>",
                        "pag::AnimatableProperty<pag::PathHandle>",
                        "pag::Keyframe<pag::PathHandle>");
  REGISTER_PAG_PROPERTY(pag::Color, "pag::Property<pag::Color>",
                        "pag::AnimatableProperty<pag::Color>", "pag::Keyframe<pag::Color>");
  REGISTER_PAG_PROPERTY(pag::Percent, "pag::Property<pag::Percent>",
                        "pag::AnimatableProperty<pag::Percent>", "pag::Keyframe<pag::Percent>");
  REGISTER_PAG_PROPERTY(pag::GradientColorHandle, "pag::Property<pag::GradientColorHandle>",
                        "pag::AnimatableProperty<pag::GradientColorHandle>",
                        "pag::Keyframe<pag::GradientColorHandle>");
  REGISTER_PAG_PROPERTY(pag::TextDocumentHandle, "pag::Property<pag::TextDocumentHandle>",
                        "pag::AnimatableProperty<pag::TextDocumentHandle>",
                        "pag::Keyframe<pag::TextDocumentHandle>");
  REGISTER_PAG_PROPERTY(pag::Frame, "pag::Property<pag::Frame>",
                        "pag::AnimatableProperty<pag::Frame>", "pag::Keyframe<pag::Frame>");
}