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

#pragma once

#include <cstring>

namespace pagx {

/**
 * A 4x4 transformation matrix for 3D transforms, stored in column-major order.
 * Matrix layout:
 *
 *   | values[0]  values[4]  values[8]   values[12] |
 *   | values[1]  values[5]  values[9]   values[13] |
 *   | values[2]  values[6]  values[10]  values[14] |
 *   | values[3]  values[7]  values[11]  values[15] |
 */
struct Matrix3D {
  float values[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  /**
   * Returns the matrix value at the given row and column.
   * @param r Row index, valid range 0..3.
   * @param c Column index, valid range 0..3.
   */
  float getRowColumn(int r, int c) const {
    return values[c * 4 + r];
  }

  /**
   * Sets the matrix value at the given row and column.
   * @param r Row index, valid range 0..3.
   * @param c Column index, valid range 0..3.
   * @param value The value to set.
   */
  void setRowColumn(int r, int c, float value) {
    values[c * 4 + r] = value;
  }

  /**
   * Returns true if this is the identity matrix.
   */
  bool isIdentity() const {
    static constexpr float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return memcmp(values, identity, sizeof(values)) == 0;
  }

  bool operator==(const Matrix3D& other) const {
    return memcmp(values, other.values, sizeof(values)) == 0;
  }

  bool operator!=(const Matrix3D& other) const {
    return !(*this == other);
  }
};

}  // namespace pagx
