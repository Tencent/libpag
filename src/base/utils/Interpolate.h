/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "pag/file.h"

namespace pag {
template <typename T>
inline T Interpolate(const T& a, const T& b, const float& t);

template <>
inline float Interpolate(const float& a, const float& b, const float& t) {
  return a + (b - a) * t;
}

template <>
inline uint8_t Interpolate(const uint8_t& a, const uint8_t& b, const float& t) {
  auto ret = a + (b - a) * t;
  return static_cast<uint8_t>(ret > 255 ? 255 : (ret < 0 ? 0 : ret));
}

template <>
inline uint16_t Interpolate(const uint16_t& a, const uint16_t& b, const float& t) {
  auto ret = a + (b - a) * t;
  return static_cast<uint16_t>(ret > 65535 ? 65535 : (ret < 0 ? 0 : ret));
}

template <>
inline Point Interpolate(const Point& a, const Point& b, const float& t) {
  return {Interpolate(a.x, b.x, t), Interpolate(a.y, b.y, t)};
}

template <>
inline Point3D Interpolate(const Point3D& a, const Point3D& b, const float& t) {
  return {Interpolate(a.x, b.x, t), Interpolate(a.y, b.y, t), Interpolate(a.z, b.z, t)};
}

template <>
inline Color Interpolate(const Color& a, const Color& b, const float& t) {
  return {Interpolate(a.red, b.red, t), Interpolate(a.green, b.green, t),
          Interpolate(a.blue, b.blue, t)};
}

template <>
inline int64_t Interpolate(const int64_t& a, const int64_t& b, const float& t) {
  return static_cast<int64_t>(a + (b - a) * t);
}

template <>
inline uint32_t Interpolate(const uint32_t& a, const uint32_t& b, const float& t) {
  return static_cast<uint32_t>(a + (b - a) * t);
}

template <>
inline int32_t Interpolate(const int32_t& a, const int32_t& b, const float& t) {
  return static_cast<int32_t>(a + (b - a) * t);
}

template <>
inline PathHandle Interpolate(const PathHandle& a, const PathHandle& b, const float& t) {
  auto path = new PathData();
  a->interpolate(*(b.get()), path, t);
  return PathHandle(path);
}

template <>
inline GradientColorHandle Interpolate(const GradientColorHandle& a, const GradientColorHandle& b,
                                       const float& t) {
  auto gradientColor = new GradientColor();
  a->interpolate(*(b.get()), gradientColor, t);
  return GradientColorHandle(gradientColor);
}

}  // namespace pag
