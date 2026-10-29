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
//  License is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include "pagx/types/Matrix3D.h"

namespace pagx {

static constexpr double DEGREES_TO_RADIANS = 0.017453292519943295769;
static constexpr double RADIANS_TO_DEGREES = 57.295779513082320877;

// Rotation quaternion (x, y, z, w) with w as the scalar part. The default is the identity
// rotation. Helpers below expect unit quaternions unless stated otherwise.
struct Quaternion {
  double x = 0;
  double y = 0;
  double z = 0;
  double w = 1;
};

// Decomposed form of a 4x4 transform used for interpolation and component conversion. The
// matrix is recomposed as P * T * R * K * S for column vectors: S scales by scale[], K is the upper
// triangular shear whose factors skew[0] (xy), skew[1] (xz) and skew[2] (yz) are stored as the
// tangent of the skew angle, R is the rotation quaternion, T translates by translate[], and P is
// the identity with its last row replaced by perspective[]. Matrix3D stores its values
// column-major, while the double[4][4] helpers in this file index [row][column].
struct DecomposedMatrix3D {
  double perspective[4] = {0, 0, 0, 1};
  double translate[3] = {0, 0, 0};
  Quaternion rotation = {};
  double scale[3] = {1, 1, 1};
  double skew[3] = {0, 0, 0};
};

// Transform components of a Layer besides its x / y / z translation. Each one maps to a Layer
// channel name (see Matrix3DComponentFromChannel). Count is a sentinel.
enum class Matrix3DComponent : uint8_t {
  RotateX,
  RotateY,
  RotateZ,
  ScaleX,
  ScaleY,
  ScaleZ,
  SkewX,
  SkewY,
  PerspectiveDistance,
  Count
};

// Number of valid Matrix3DComponent values.
static constexpr size_t MATRIX3D_COMPONENT_COUNT = static_cast<size_t>(Matrix3DComponent::Count);

// A transform described by independent components, composed as P * T * R * K * S like
// DecomposedMatrix3D. Rotation holds Euler angles in degrees composed as Rz * Ry * Rx, so authored
// values such as 200 degrees are kept as written. Skew holds the CSS skew(x, y) angles in degrees,
// giving the 2D shear [[1, tan(x)], [tan(y), 1]]. Perspective is the viewing distance d of the
// [0, 0, -1/d, 1] projection row, with 0 meaning no perspective.
struct Matrix3DComponents {
  double translate[3] = {0, 0, 0};
  double rotation[3] = {0, 0, 0};
  double scale[3] = {1, 1, 1};
  double skew[2] = {0, 0};
  double perspective = 0;
};

// Returns true if all 16 matrix values are finite.
inline bool IsFiniteMatrix3D(const Matrix3D& matrix) {
  for (float value : matrix.values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

// Returns the determinant of a [row][column] 4x4 matrix using Gaussian elimination with partial
// pivoting. Returns 0 as soon as a pivot magnitude falls to 1e-12 or below.
inline double Determinant4(const double source[4][4]) {
  double matrix[4][4] = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      matrix[row][column] = source[row][column];
    }
  }
  double determinant = 1;
  for (int column = 0; column < 4; ++column) {
    int pivot = column;
    for (int row = column + 1; row < 4; ++row) {
      if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
        pivot = row;
      }
    }
    double pivotValue = matrix[pivot][column];
    if (std::abs(pivotValue) <= 1.0e-12) {
      return 0;
    }
    if (pivot != column) {
      for (int i = 0; i < 4; ++i) {
        std::swap(matrix[pivot][i], matrix[column][i]);
      }
      determinant = -determinant;
    }
    determinant *= pivotValue;
    for (int row = column + 1; row < 4; ++row) {
      double factor = matrix[row][column] / pivotValue;
      for (int i = column + 1; i < 4; ++i) {
        matrix[row][i] -= factor * matrix[column][i];
      }
    }
  }
  return determinant;
}

// Returns the determinant of a [row][column] 3x3 matrix.
inline double Determinant3(const double source[3][3]) {
  return source[0][0] * (source[1][1] * source[2][2] - source[1][2] * source[2][1]) -
         source[0][1] * (source[1][0] * source[2][2] - source[1][2] * source[2][0]) +
         source[0][2] * (source[1][0] * source[2][1] - source[1][1] * source[2][0]);
}

// Returns true if the matrix is finite and invertible. Without a perspective row the determinant
// factors as det(L) * m33 for the upper 3x3 block L, so only L and m33 decide and the translation
// cannot make a well-conditioned matrix look singular. A perspective row couples the translation
// into the determinant, so that case keeps the column-normalized determinant of the whole matrix,
// which is also the only way to describe a matrix whose 3x3 block is singular but is still
// invertible. Columns are normalized by their Euclidean norm, so the threshold depends neither on
// the overall scale nor on the translation.
inline bool IsInvertibleMatrix3D(const Matrix3D& matrix) {
  if (!IsFiniteMatrix3D(matrix)) {
    return false;
  }
  bool hasPerspective = matrix.getRowColumn(3, 0) != 0 || matrix.getRowColumn(3, 1) != 0 ||
                        matrix.getRowColumn(3, 2) != 0;
  if (!hasPerspective) {
    double linear[3][3] = {};
    for (int column = 0; column < 3; ++column) {
      double norm = 0;
      for (int row = 0; row < 3; ++row) {
        linear[row][column] = matrix.getRowColumn(row, column);
        norm += linear[row][column] * linear[row][column];
      }
      norm = std::sqrt(norm);
      if (!std::isfinite(norm) || norm == 0) {
        return false;
      }
      for (int row = 0; row < 3; ++row) {
        linear[row][column] /= norm;
      }
    }
    // m33 only has to be non-zero: it is the homogeneous scale, so comparing it against a fixed
    // threshold would reject a uniformly scaled matrix such as diag(1, 1, 1, 1e-10).
    return std::abs(Determinant3(linear)) > 1.0e-9 && matrix.getRowColumn(3, 3) != 0.0f;
  }
  double values[4][4] = {};
  for (int column = 0; column < 4; ++column) {
    double norm = 0;
    for (int row = 0; row < 4; ++row) {
      values[row][column] = matrix.getRowColumn(row, column);
      norm += values[row][column] * values[row][column];
    }
    norm = std::sqrt(norm);
    if (!std::isfinite(norm) || norm == 0) {
      return false;
    }
    for (int row = 0; row < 4; ++row) {
      values[row][column] /= norm;
    }
  }
  return std::abs(Determinant4(values)) > 1.0e-9;
}

// Inverts a [row][column] 4x4 matrix with Gauss-Jordan elimination and writes the result to
// inverse as 16 values in row-major order. Returns false if the matrix is singular.
inline bool Invert4(const double source[4][4], double* inverse) {
  double rows[4][8] = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      rows[row][column] = source[row][column];
      rows[row][column + 4] = row == column ? 1.0 : 0.0;
    }
  }
  for (int column = 0; column < 4; ++column) {
    int pivot = column;
    for (int row = column + 1; row < 4; ++row) {
      if (std::abs(rows[row][column]) > std::abs(rows[pivot][column])) {
        pivot = row;
      }
    }
    double pivotValue = rows[pivot][column];
    if (std::abs(pivotValue) <= 1.0e-12) {
      return false;
    }
    if (pivot != column) {
      for (int i = 0; i < 8; ++i) {
        std::swap(rows[pivot][i], rows[column][i]);
      }
    }
    for (int i = 0; i < 8; ++i) {
      rows[column][i] /= pivotValue;
    }
    for (int row = 0; row < 4; ++row) {
      if (row == column) {
        continue;
      }
      double factor = rows[row][column];
      for (int i = 0; i < 8; ++i) {
        rows[row][i] -= factor * rows[column][i];
      }
    }
  }
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      inverse[row * 4 + column] = rows[row][column + 4];
    }
  }
  return true;
}

// Returns the dot product of two 3-component vectors.
inline double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Returns the Euclidean length of a 3-component vector.
inline double Length3(const double value[3]) {
  return std::sqrt(Dot3(value, value));
}

// Multiplies a 3-component vector by scale in place.
inline void Scale3(double value[3], double scale) {
  for (int i = 0; i < 3; ++i) {
    value[i] *= scale;
  }
}

// Subtracts basis * scale from a 3-component vector in place.
inline void SubtractScaled3(double value[3], const double basis[3], double scale) {
  for (int i = 0; i < 3; ++i) {
    value[i] -= basis[i] * scale;
  }
}

// Converts an orthonormal [row][column] 3x3 rotation matrix to a quaternion, branching on the
// largest diagonal term for numerical stability.
inline Quaternion QuaternionFromRotation(const double rotation[3][3]) {
  Quaternion q = {};
  double trace = rotation[0][0] + rotation[1][1] + rotation[2][2];
  if (trace > 0) {
    double s = std::sqrt(trace + 1.0) * 2.0;
    q.w = 0.25 * s;
    q.x = (rotation[2][1] - rotation[1][2]) / s;
    q.y = (rotation[0][2] - rotation[2][0]) / s;
    q.z = (rotation[1][0] - rotation[0][1]) / s;
  } else if (rotation[0][0] > rotation[1][1] && rotation[0][0] > rotation[2][2]) {
    double s = std::sqrt(1.0 + rotation[0][0] - rotation[1][1] - rotation[2][2]) * 2.0;
    q.w = (rotation[2][1] - rotation[1][2]) / s;
    q.x = 0.25 * s;
    q.y = (rotation[0][1] + rotation[1][0]) / s;
    q.z = (rotation[0][2] + rotation[2][0]) / s;
  } else if (rotation[1][1] > rotation[2][2]) {
    double s = std::sqrt(1.0 + rotation[1][1] - rotation[0][0] - rotation[2][2]) * 2.0;
    q.w = (rotation[0][2] - rotation[2][0]) / s;
    q.x = (rotation[0][1] + rotation[1][0]) / s;
    q.y = 0.25 * s;
    q.z = (rotation[1][2] + rotation[2][1]) / s;
  } else {
    double s = std::sqrt(1.0 + rotation[2][2] - rotation[0][0] - rotation[1][1]) * 2.0;
    q.w = (rotation[1][0] - rotation[0][1]) / s;
    q.x = (rotation[0][2] + rotation[2][0]) / s;
    q.y = (rotation[1][2] + rotation[2][1]) / s;
    q.z = 0.25 * s;
  }
  return q;
}

// Converts a unit quaternion to a [row][column] 3x3 rotation matrix.
inline void RotationFromQuaternion(const Quaternion& q, double rotation[3][3]) {
  double xx = q.x * q.x;
  double yy = q.y * q.y;
  double zz = q.z * q.z;
  double xy = q.x * q.y;
  double xz = q.x * q.z;
  double yz = q.y * q.z;
  double wx = q.w * q.x;
  double wy = q.w * q.y;
  double wz = q.w * q.z;
  rotation[0][0] = 1.0 - 2.0 * (yy + zz);
  rotation[0][1] = 2.0 * (xy - wz);
  rotation[0][2] = 2.0 * (xz + wy);
  rotation[1][0] = 2.0 * (xy + wz);
  rotation[1][1] = 1.0 - 2.0 * (xx + zz);
  rotation[1][2] = 2.0 * (yz - wx);
  rotation[2][0] = 2.0 * (xz - wy);
  rotation[2][1] = 2.0 * (yz + wx);
  rotation[2][2] = 1.0 - 2.0 * (xx + yy);
}

// Returns the Hamilton product a * b, which applies b first and then a to column vectors.
inline Quaternion MultiplyQuaternion(const Quaternion& a, const Quaternion& b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// Builds a rotation from Euler angles in degrees composed as Rz * Ry * Rx for column vectors, so
// the X rotation is applied first.
inline Quaternion QuaternionFromEulerXYZDegrees(double xDegrees, double yDegrees, double zDegrees) {
  double halfX = xDegrees * DEGREES_TO_RADIANS * 0.5;
  double halfY = yDegrees * DEGREES_TO_RADIANS * 0.5;
  double halfZ = zDegrees * DEGREES_TO_RADIANS * 0.5;
  Quaternion qx = {std::sin(halfX), 0, 0, std::cos(halfX)};
  Quaternion qy = {0, std::sin(halfY), 0, std::cos(halfY)};
  Quaternion qz = {0, 0, std::sin(halfZ), std::cos(halfZ)};
  return MultiplyQuaternion(MultiplyQuaternion(qz, qy), qx);
}

// Wraps an angle in degrees into (-180, 180].
inline double WrapDegrees(double degrees) {
  double wrapped = std::fmod(degrees, 360.0);
  if (wrapped > 180) {
    wrapped -= 360;
  } else if (wrapped <= -180) {
    wrapped += 360;
  }
  return wrapped;
}

// Recovers Euler angles in degrees, in the Rz * Ry * Rx convention of
// QuaternionFromEulerXYZDegrees, from a quaternion, which is normalized first. Every rotation has
// two solutions, (x, y, z) and (x + 180, 180 - y, z + 180); this returns the canonical one with the
// smaller |x| + |z|, each angle within (-180, 180], so a pure Y rotation beyond 90 degrees reads
// back as that Y angle instead of an equivalent set with X and Z at 180. At gimbal lock
// (|cos y| <= 1e-6, just wide enough to absorb float round-off) the whole remaining rotation is
// assigned to X and Z is 0. When both solutions weigh the same (for example x = z = 90) the asin
// solution with Y in [-90, 90] is kept. Since a matrix keeps no winding, the result may differ from
// the angles a rotation was authored with. Returns false for a zero-length or non-finite
// quaternion.
inline bool EulerXYZDegreesFromQuaternion(const Quaternion& quaternion, double output[3]) {
  if (output == nullptr) {
    return false;
  }
  double length = std::sqrt(quaternion.x * quaternion.x + quaternion.y * quaternion.y +
                            quaternion.z * quaternion.z + quaternion.w * quaternion.w);
  if (!std::isfinite(length) || length <= 1.0e-12) {
    return false;
  }
  Quaternion normalized = {quaternion.x / length, quaternion.y / length, quaternion.z / length,
                           quaternion.w / length};
  double rotation[3][3] = {};
  RotationFromQuaternion(normalized, rotation);
  double sinY = std::clamp(-rotation[2][0], -1.0, 1.0);
  double y = std::asin(sinY);
  double cosY = std::cos(y);
  double x = 0;
  double z = 0;
  if (std::abs(cosY) > 1.0e-6) {
    x = std::atan2(rotation[2][1], rotation[2][2]);
    z = std::atan2(rotation[1][0], rotation[0][0]);
  } else {
    x = std::atan2(sinY >= 0 ? rotation[0][1] : -rotation[0][1], rotation[1][1]);
  }
  double first[3] = {x * RADIANS_TO_DEGREES, y * RADIANS_TO_DEGREES, z * RADIANS_TO_DEGREES};
  double second[3] = {WrapDegrees(first[0] + 180), WrapDegrees(180 - first[1]),
                      WrapDegrees(first[2] + 180)};
  double firstWeight = std::abs(first[0]) + std::abs(first[2]);
  double secondWeight = std::abs(second[0]) + std::abs(second[2]);
  const double* chosen = secondWeight < firstWeight - 1.0e-6 ? second : first;
  for (int i = 0; i < 3; ++i) {
    output[i] = chosen[i];
  }
  return std::isfinite(output[0]) && std::isfinite(output[1]) && std::isfinite(output[2]);
}

// Spherically interpolates between two unit quaternions along the shortest arc. Nearly parallel
// inputs use a normalized linear interpolation to avoid dividing by a vanishing sine.
inline Quaternion SlerpQuaternion(const Quaternion& first, const Quaternion& second, double t) {
  Quaternion a = first;
  Quaternion b = second;
  double dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (dot < 0) {
    dot = -dot;
    b.x = -b.x;
    b.y = -b.y;
    b.z = -b.z;
    b.w = -b.w;
  }
  dot = std::clamp(dot, -1.0, 1.0);
  if (dot > 0.9995) {
    Quaternion result = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                         a.w + (b.w - a.w) * t};
    double length = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z +
                              result.w * result.w);
    result.x /= length;
    result.y /= length;
    result.z /= length;
    result.w /= length;
    return result;
  }
  double angle = std::acos(dot);
  double sinAngle = std::sin(angle);
  double weightA = std::sin((1.0 - t) * angle) / sinAngle;
  double weightB = std::sin(t * angle) / sinAngle;
  return {a.x * weightA + b.x * weightB, a.y * weightA + b.y * weightB,
          a.z * weightA + b.z * weightB, a.w * weightA + b.w * weightB};
}

inline bool RecomposeMatrix3D(const DecomposedMatrix3D& value, Matrix3D* output);

// Decomposes a 4x4 transform into perspective, translation, rotation, scale and skew (see
// DecomposedMatrix3D), as the exact inverse of RecomposeMatrix3D. The perspective row q is solved
// from M = P * A, where A is the matrix with its last row replaced by [0, 0, 0, 1]; the matrix is
// then normalized by q[3] so the stored perspective row always ends in 1. Without perspective q[3]
// is the [3][3] value. Gram-Schmidt orthogonalization of the upper 3x3 yields scale and skew, with
// a negative determinant folded into the Z scale. Returns false if the matrix is non-finite or
// singular, q[3] is near zero, a scale axis degenerates, or recomposing the result differs from
// the normalized source by more than a relative 1e-5 in any value.
inline bool DecomposeMatrix3D(const Matrix3D& source, DecomposedMatrix3D* output) {
  if (output == nullptr || !IsInvertibleMatrix3D(source)) {
    return false;
  }
  double matrix[4][4] = {};
  double magnitude = 0;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      matrix[row][column] = source.getRowColumn(row, column);
      magnitude = std::max(magnitude, std::abs(matrix[row][column]));
    }
  }
  double perspective[4] = {0, 0, 0, matrix[3][3]};
  bool hasPerspective = std::abs(matrix[3][0]) > 1.0e-12 * magnitude ||
                        std::abs(matrix[3][1]) > 1.0e-12 * magnitude ||
                        std::abs(matrix[3][2]) > 1.0e-12 * magnitude;
  if (hasPerspective) {
    double affine[4][4] = {};
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        affine[row][column] = matrix[row][column];
      }
    }
    affine[3][0] = affine[3][1] = affine[3][2] = 0;
    affine[3][3] = 1;
    double inverse[16] = {};
    if (!Invert4(affine, inverse)) {
      return false;
    }
    double lastRow[4] = {matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]};
    for (int row = 0; row < 4; ++row) {
      perspective[row] = 0;
      for (int column = 0; column < 4; ++column) {
        perspective[row] += inverse[column * 4 + row] * lastRow[column];
      }
    }
  }
  // Normalize by the scalar of the perspective factor rather than by [3][3], so the result is the
  // exact inverse of RecomposeMatrix3D, whose perspective row always ends in 1. The two differ when
  // a perspective is combined with a Z translation. The relative-zero guard measures the scalar
  // against the last row it was solved from, not the whole matrix: the translation column plays no
  // part in the solve, and including it would reject invertible matrices with a huge translation.
  double normalization = perspective[3];
  double lastRowMagnitude = 0;
  for (int column = 0; column < 4; ++column) {
    lastRowMagnitude = std::max(lastRowMagnitude, std::abs(matrix[3][column]));
  }
  if (std::abs(normalization) <= 1.0e-12 * lastRowMagnitude) {
    return false;
  }
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      matrix[row][column] /= normalization;
    }
  }
  perspective[3] = 1;
  double normalizedSource[4][4] = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      normalizedSource[row][column] = matrix[row][column];
    }
  }
  if (hasPerspective) {
    for (int column = 0; column < 4; ++column) {
      matrix[3][column] = column == 3 ? 1.0 : 0.0;
    }
  }

  DecomposedMatrix3D result = {};
  for (int i = 0; i < 4; ++i) {
    result.perspective[i] = perspective[i];
  }
  for (int i = 0; i < 3; ++i) {
    result.translate[i] = matrix[i][3];
    matrix[i][3] = 0;
  }
  double columns[3][3] = {};
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      columns[column][row] = matrix[row][column];
    }
  }
  result.scale[0] = Length3(columns[0]);
  if (result.scale[0] <= 1.0e-12) return false;
  Scale3(columns[0], 1.0 / result.scale[0]);

  result.skew[0] = Dot3(columns[0], columns[1]);
  SubtractScaled3(columns[1], columns[0], result.skew[0]);
  result.scale[1] = Length3(columns[1]);
  if (result.scale[1] <= 1.0e-12) return false;
  Scale3(columns[1], 1.0 / result.scale[1]);
  result.skew[0] /= result.scale[1];

  result.skew[1] = Dot3(columns[0], columns[2]);
  SubtractScaled3(columns[2], columns[0], result.skew[1]);
  result.skew[2] = Dot3(columns[1], columns[2]);
  SubtractScaled3(columns[2], columns[1], result.skew[2]);
  result.scale[2] = Length3(columns[2]);
  if (result.scale[2] <= 1.0e-12) return false;
  Scale3(columns[2], 1.0 / result.scale[2]);
  result.skew[1] /= result.scale[2];
  result.skew[2] /= result.scale[2];

  double orientation[3] = {columns[0][1] * columns[1][2] - columns[0][2] * columns[1][1],
                           columns[0][2] * columns[1][0] - columns[0][0] * columns[1][2],
                           columns[0][0] * columns[1][1] - columns[0][1] * columns[1][0]};
  if (Dot3(orientation, columns[2]) < 0) {
    result.scale[2] = -result.scale[2];
    result.skew[1] = -result.skew[1];
    result.skew[2] = -result.skew[2];
    Scale3(columns[2], -1);
  }
  double rotation[3][3] = {};
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      rotation[row][column] = columns[column][row];
    }
  }
  result.rotation = QuaternionFromRotation(rotation);
  Matrix3D normalizedMatrix = {};
  Matrix3D recomposed = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      normalizedMatrix.setRowColumn(row, column, static_cast<float>(normalizedSource[row][column]));
    }
  }
  if (!RecomposeMatrix3D(result, &recomposed)) {
    return false;
  }
  for (int i = 0; i < 16; ++i) {
    float scale = std::max(1.0f, std::abs(normalizedMatrix.values[i]));
    if (std::abs(recomposed.values[i] - normalizedMatrix.values[i]) > 1.0e-5f * scale) {
      return false;
    }
  }
  *output = result;
  return true;
}

// Composes a 4x4 transform as P * T * R * (K * S) for column vectors, where shearScale is the
// [row][column] product of the shear K and the scale S, translate is T and perspective is the last
// row of P. Returns false if any resulting value is non-finite or exceeds the float range; output
// may be partially written in that case.
inline bool ComposeMatrix3DFromParts(const Quaternion& rotationQuaternion,
                                     const double shearScale[3][3], const double translate[3],
                                     const double perspectiveRow[4], Matrix3D* output) {
  if (output == nullptr) return false;
  double rotation[3][3] = {};
  RotationFromQuaternion(rotationQuaternion, rotation);
  double affine[4][4] = {};
  for (int row = 0; row < 3; ++row) {
    affine[row][3] = translate[row];
    for (int column = 0; column < 3; ++column) {
      for (int k = 0; k < 3; ++k) {
        affine[row][column] += rotation[row][k] * shearScale[k][column];
      }
    }
  }
  affine[3][3] = 1;
  double perspective[4][4] = {};
  for (int row = 0; row < 4; ++row) {
    perspective[row][row] = 1;
  }
  for (int column = 0; column < 4; ++column) {
    perspective[3][column] = perspectiveRow[column];
  }
  double result[4][4] = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      for (int k = 0; k < 4; ++k) {
        result[row][column] += perspective[row][k] * affine[k][column];
      }
      if (!std::isfinite(result[row][column]) || std::abs(result[row][column]) > 3.0e38) {
        return false;
      }
      output->setRowColumn(row, column, static_cast<float>(result[row][column]));
    }
  }
  return true;
}

// Composes a 4x4 transform as P * T * R * K * S from decomposed components (see
// DecomposedMatrix3D). Unlike RecomposeMatrix3D the result may be singular, such as for a zero
// scale. Returns false if any resulting value is non-finite or exceeds the float range.
inline bool ComposeMatrix3D(const DecomposedMatrix3D& value, Matrix3D* output) {
  double shearScale[3][3] = {};
  shearScale[0][0] = value.scale[0];
  shearScale[0][1] = value.skew[0] * value.scale[1];
  shearScale[1][1] = value.scale[1];
  shearScale[0][2] = value.skew[1] * value.scale[2];
  shearScale[1][2] = value.skew[2] * value.scale[2];
  shearScale[2][2] = value.scale[2];
  return ComposeMatrix3DFromParts(value.rotation, shearScale, value.translate, value.perspective,
                                  output);
}

// Recomposes a 4x4 transform as P * T * R * K * S from decomposed components (see
// DecomposedMatrix3D). Returns false if the composition fails or the result is not invertible.
inline bool RecomposeMatrix3D(const DecomposedMatrix3D& value, Matrix3D* output) {
  return ComposeMatrix3D(value, output) && IsInvertibleMatrix3D(*output);
}

// Returns true if the perspective of a DecomposeMatrix3D result, whose last perspective value is
// always 1, can be expressed by the single perspective distance component, that is it has the form
// [0, 0, -1/d, 1] with d > 0, or is [0, 0, 0, 1] for none.
inline bool IsMatrix3DComponentRepresentable(const DecomposedMatrix3D& matrix) {
  constexpr double Tolerance = 1.0e-8;
  return std::abs(matrix.perspective[0]) <= Tolerance &&
         std::abs(matrix.perspective[1]) <= Tolerance && matrix.perspective[2] <= 0;
}

// Maps a Layer channel name ("rotation.x", "scale.y", "skew.x", "perspective", ...) to its
// Matrix3DComponent. Returns false if the name is not a transform component channel.
inline bool Matrix3DComponentFromChannel(const std::string& channel, Matrix3DComponent* component) {
  if (component == nullptr) {
    return false;
  }
  static const char* names[] = {"rotation.x", "rotation.y", "rotation.z", "scale.x",    "scale.y",
                                "scale.z",    "skew.x",     "skew.y",     "perspective"};
  for (size_t i = 0; i < MATRIX3D_COMPONENT_COUNT; ++i) {
    if (channel == names[i]) {
      *component = static_cast<Matrix3DComponent>(i);
      return true;
    }
  }
  return false;
}

// Returns the field of components that stores the given component.
inline double* Matrix3DComponentValue(Matrix3DComponents* components, Matrix3DComponent component) {
  switch (component) {
    case Matrix3DComponent::RotateX:
    case Matrix3DComponent::RotateY:
    case Matrix3DComponent::RotateZ:
      return &components->rotation[static_cast<int>(component) -
                                   static_cast<int>(Matrix3DComponent::RotateX)];
    case Matrix3DComponent::ScaleX:
    case Matrix3DComponent::ScaleY:
    case Matrix3DComponent::ScaleZ:
      return &components->scale[static_cast<int>(component) -
                                static_cast<int>(Matrix3DComponent::ScaleX)];
    case Matrix3DComponent::SkewX:
    case Matrix3DComponent::SkewY:
      return &components
                  ->skew[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::SkewX)];
    case Matrix3DComponent::PerspectiveDistance:
      return &components->perspective;
    case Matrix3DComponent::Count:
      break;
  }
  return nullptr;
}

// Composes a 4x4 transform as P * T * R * K * S from a component set (see Matrix3DComponents). The
// result may be singular, such as for a zero scale. Returns false if a value is non-finite, a skew
// is +/-90 degrees, the perspective distance is negative, or the result overflows.
inline bool ComposeMatrix3DComponents(const Matrix3DComponents& components, Matrix3D* output) {
  double shear[2] = {};
  for (int i = 0; i < 2; ++i) {
    double radians = components.skew[i] * DEGREES_TO_RADIANS;
    if (!std::isfinite(radians) || std::abs(std::cos(radians)) <= 1.0e-8) {
      return false;
    }
    shear[i] = std::tan(radians);
  }
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(components.translate[i]) || !std::isfinite(components.rotation[i]) ||
        !std::isfinite(components.scale[i])) {
      return false;
    }
  }
  if (!std::isfinite(components.perspective) || components.perspective < 0) {
    return false;
  }
  double shearScale[3][3] = {};
  shearScale[0][0] = components.scale[0];
  shearScale[0][1] = shear[0] * components.scale[1];
  shearScale[1][0] = shear[1] * components.scale[0];
  shearScale[1][1] = components.scale[1];
  shearScale[2][2] = components.scale[2];
  double perspective[4] = {0, 0, 0, 1};
  if (components.perspective > 0) {
    perspective[2] = -1.0 / components.perspective;
  }
  auto rotation = QuaternionFromEulerXYZDegrees(components.rotation[0], components.rotation[1],
                                                components.rotation[2]);
  return ComposeMatrix3DFromParts(rotation, shearScale, components.translate, perspective, output);
}

// Recovers a component set from a 4x4 transform. Rotation is the canonical Euler set returned by
// EulerXYZDegreesFromQuaternion. The decomposition only yields an X skew, so skew y is 0. Returns
// false if the matrix cannot be decomposed, its perspective is not a single Z distance, or it
// carries a shear involving Z, which the components cannot express.
inline bool Matrix3DComponentsFromMatrix(const Matrix3D& matrix, Matrix3DComponents* output) {
  DecomposedMatrix3D value = {};
  if (output == nullptr || !DecomposeMatrix3D(matrix, &value) ||
      !IsMatrix3DComponentRepresentable(value) || std::abs(value.skew[1]) > 1.0e-6 ||
      std::abs(value.skew[2]) > 1.0e-6) {
    return false;
  }
  Matrix3DComponents result = {};
  if (!EulerXYZDegreesFromQuaternion(value.rotation, result.rotation)) {
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    result.translate[i] = value.translate[i];
    result.scale[i] = value.scale[i];
  }
  result.skew[0] = std::atan(value.skew[0]) * RADIANS_TO_DEGREES;
  result.perspective = std::abs(value.perspective[2]) <= 1.0e-12 ? 0 : -1.0 / value.perspective[2];
  *output = result;
  return true;
}

// Interpolates two 4x4 transforms. t <= 0 returns first and t >= 1 returns second. Otherwise both
// are decomposed and their components are interpolated, using slerp for rotation; if either
// decomposition or the composition fails, the matrices are interpolated element-wise instead.
// A finite singular interpolated pose is kept rather than switching interpolation algorithms.
inline Matrix3D LerpMatrix3D(const Matrix3D& first, const Matrix3D& second, double t) {
  if (first == second || t <= 0) return first;
  if (t >= 1) return second;
  DecomposedMatrix3D a = {};
  DecomposedMatrix3D b = {};
  if (DecomposeMatrix3D(first, &a) && DecomposeMatrix3D(second, &b)) {
    DecomposedMatrix3D mixed = {};
    for (int i = 0; i < 4; ++i) {
      mixed.perspective[i] = a.perspective[i] + (b.perspective[i] - a.perspective[i]) * t;
    }
    for (int i = 0; i < 3; ++i) {
      mixed.translate[i] = a.translate[i] + (b.translate[i] - a.translate[i]) * t;
      mixed.scale[i] = a.scale[i] + (b.scale[i] - a.scale[i]) * t;
      mixed.skew[i] = a.skew[i] + (b.skew[i] - a.skew[i]) * t;
    }
    mixed.rotation = SlerpQuaternion(a.rotation, b.rotation, t);
    Matrix3D result = {};
    if (ComposeMatrix3D(mixed, &result)) {
      return result;
    }
  }
  Matrix3D result = {};
  for (int i = 0; i < 16; ++i) {
    result.values[i] = static_cast<float>(
        first.values[i] + (static_cast<double>(second.values[i]) - first.values[i]) * t);
  }
  return result;
}

}  // namespace pagx
