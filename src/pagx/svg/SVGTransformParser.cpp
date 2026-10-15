/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/svg/SVGTransformParser.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "base/utils/MathUtil.h"

namespace pagx {

using pag::DegreesToRadians;

static void SkipWhitespace(const char*& ptr, const char* end) {
  while (ptr < end && (std::isspace(*ptr) || *ptr == ',')) {
    ++ptr;
  }
}

static float ReadNumber(const char*& ptr, const char* end) {
  SkipWhitespace(ptr, end);
  const char* start = ptr;
  if (ptr < end && (*ptr == '-' || *ptr == '+')) {
    ++ptr;
  }
  while (ptr < end && (std::isdigit(*ptr) || *ptr == '.')) {
    ++ptr;
  }
  if (ptr < end && (*ptr == 'e' || *ptr == 'E')) {
    ++ptr;
    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
      ++ptr;
    }
    while (ptr < end && std::isdigit(*ptr)) {
      ++ptr;
    }
  }
  if (start == ptr) {
    return 0.0f;
  }
  return strtof(start, nullptr);
}

Matrix MatrixFromSVGTransform(const std::string& value) {
  Matrix result = Matrix::Identity();
  if (value.empty()) {
    return result;
  }

  const char* ptr = value.c_str();
  const char* end = ptr + value.length();

  while (ptr < end) {
    SkipWhitespace(ptr, end);
    if (ptr >= end) {
      break;
    }

    const char* funcStart = ptr;
    while (ptr < end && std::isalpha(*ptr)) {
      ++ptr;
    }
    size_t funcLen = static_cast<size_t>(ptr - funcStart);

    SkipWhitespace(ptr, end);
    if (ptr >= end || *ptr != '(') {
      break;
    }
    ++ptr;

    Matrix matrix = Matrix::Identity();

    if (funcLen == 9 && memcmp(funcStart, "translate", 9) == 0) {
      float tx = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      float ty = 0;
      if (ptr < end && *ptr != ')') {
        ty = ReadNumber(ptr, end);
      }
      matrix = Matrix::Translate(tx, ty);
    } else if (funcLen == 5 && memcmp(funcStart, "scale", 5) == 0) {
      float sx = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      float sy = sx;
      if (ptr < end && *ptr != ')') {
        sy = ReadNumber(ptr, end);
      }
      matrix = Matrix::Scale(sx, sy);
    } else if (funcLen == 6 && memcmp(funcStart, "rotate", 6) == 0) {
      float angle = ReadNumber(ptr, end);
      SkipWhitespace(ptr, end);
      if (ptr < end && *ptr != ')') {
        float cx = ReadNumber(ptr, end);
        float cy = ReadNumber(ptr, end);
        matrix = Matrix::Translate(cx, cy) * Matrix::Rotate(angle) * Matrix::Translate(-cx, -cy);
      } else {
        matrix = Matrix::Rotate(angle);
      }
    } else if (funcLen == 5 && memcmp(funcStart, "skewX", 5) == 0) {
      float angle = ReadNumber(ptr, end);
      matrix.c = std::tan(DegreesToRadians(angle));
    } else if (funcLen == 5 && memcmp(funcStart, "skewY", 5) == 0) {
      float angle = ReadNumber(ptr, end);
      matrix.b = std::tan(DegreesToRadians(angle));
    } else if (funcLen == 6 && memcmp(funcStart, "matrix", 6) == 0) {
      matrix.a = ReadNumber(ptr, end);
      matrix.b = ReadNumber(ptr, end);
      matrix.c = ReadNumber(ptr, end);
      matrix.d = ReadNumber(ptr, end);
      matrix.tx = ReadNumber(ptr, end);
      matrix.ty = ReadNumber(ptr, end);
    }

    SkipWhitespace(ptr, end);
    if (ptr < end && *ptr == ')') {
      ++ptr;
    }

    result = result * matrix;
  }

  return result;
}

}  // namespace pagx
