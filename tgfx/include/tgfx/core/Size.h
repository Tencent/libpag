/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <cmath>

namespace tgfx {
/**
 * ISize holds two 32-bit integer dimensions.
 */
struct ISize {
  /**
   * Span on the x-axis.
   */
  int width;

  /**
   * Span on the y-axis.
   */
  int height;

  static ISize Make(int w, int h) {
    return {w, h};
  }

  static ISize MakeEmpty() {
    return {0, 0};
  }

  void set(int w, int h) {
    *this = ISize{w, h};
  }

  /**
   * Returns true if width == 0 && height == 0
   */
  bool isZero() const {
    return 0 == width && 0 == height;
  }

  /**
   * Returns true if either width or height are <= 0
   */
  bool isEmpty() const {
    return width <= 0 || height <= 0;
  }

  /**
   * Set the width and height to 0.
   */
  void setEmpty() {
    *this = ISize{0, 0};
  }

  /**
   * Returns true if a is equivalent to b.
   */
  friend bool operator==(const ISize& a, const ISize& b) {
    return a.width == b.width && a.height == b.height;
  }

  /**
   * Returns true if a is not equivalent to b.
   */
  friend bool operator!=(const ISize& a, const ISize& b) {
    return a.width != b.width || a.height != b.height;
  }
};

/**
 * Size holds two 32-bit floating dimensions.
 */
struct Size {
  /**
   * Span on the x-axis.
   */
  float width;

  /**
   * Span on the y-axis.
   */
  float height;

  static Size Make(float w, float h) {
    return {w, h};
  }

  static Size Make(const ISize& src) {
    return {static_cast<float>(src.width), static_cast<float>(src.height)};
  }

  Size& operator=(const ISize& src) {
    return *this = Size{static_cast<float>(src.width), static_cast<float>(src.height)};
  }

  static Size MakeEmpty() {
    return {0, 0};
  }

  void set(float w, float h) {
    *this = Size{w, h};
  }

  /**
   * Returns true if width == 0 && height == 0
   */
  bool isZero() const {
    return 0 == width && 0 == height;
  }

  /**
   * Returns true if either width or height are <= 0
   */
  bool isEmpty() const {
    return width <= 0 || height <= 0;
  }

  /**
   * Set the width and height to 0.
   */
  void setEmpty() {
    *this = Size{0, 0};
  }

  /**
   * Returns a ISize by rounding of width and height.
   */
  ISize toRound() {
    return {static_cast<int>(roundf(width)), static_cast<int>(roundf(height))};
  }

  /**
   * Returns a ISize by ceiling of width and height.
   */
  ISize toCeil() {
    return {static_cast<int>(ceilf(width)), static_cast<int>(ceilf(height))};
  }

  /**
   * Returns a ISize by flooring of width and height.
   */
  ISize toFloor() {
    return {static_cast<int>(floorf(width)), static_cast<int>(floorf(height))};
  }

  /**
   * Returns true if a is equivalent to b.
   */
  friend bool operator==(const Size& a, const Size& b) {
    return a.width == b.width && a.height == b.height;
  }

  /**
   * Returns true if a is not equivalent to b.
   */
  friend bool operator!=(const Size& a, const Size& b) {
    return a.width != b.width || a.height != b.height;
  }
};
}  // namespace tgfx
