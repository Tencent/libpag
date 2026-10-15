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

struct Quaternion {
  double x = 0;
  double y = 0;
  double z = 0;
  double w = 1;
};

struct DecomposedMatrix3D {
  double perspective[4] = {0, 0, 0, 1};
  double translate[3] = {0, 0, 0};
  Quaternion rotation = {};
  double scale[3] = {1, 1, 1};
  double skew[3] = {0, 0, 0};
};

enum class Matrix3DComponent : uint8_t {
  RotateX,
  RotateY,
  RotateZ,
  ScaleX,
  ScaleY,
  ScaleZ,
  SkewXY,
  SkewXZ,
  SkewYZ,
  PerspectiveDistance,
  Count
};

static constexpr size_t Matrix3DComponentCount = static_cast<size_t>(Matrix3DComponent::Count);

inline bool IsFiniteMatrix3D(const Matrix3D& matrix) {
  for (float value : matrix.values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

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

inline bool IsInvertibleMatrix3D(const Matrix3D& matrix) {
  if (!IsFiniteMatrix3D(matrix)) {
    return false;
  }
  double values[4][4] = {};
  double scale = 0;
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      values[row][column] = matrix.getRowColumn(row, column);
      scale = std::max(scale, std::abs(values[row][column]));
    }
  }
  if (scale == 0) {
    return false;
  }
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      values[row][column] /= scale;
    }
  }
  return std::abs(Determinant4(values)) > 1.0e-12;
}

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

inline double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline double Length3(const double value[3]) {
  return std::sqrt(Dot3(value, value));
}

inline void Scale3(double value[3], double scale) {
  for (int i = 0; i < 3; ++i) {
    value[i] *= scale;
  }
}

inline void SubtractScaled3(double value[3], const double basis[3], double scale) {
  for (int i = 0; i < 3; ++i) {
    value[i] -= basis[i] * scale;
  }
}

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

inline Quaternion MultiplyQuaternion(const Quaternion& a, const Quaternion& b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

inline Quaternion QuaternionFromEulerXYZDegrees(double xDegrees, double yDegrees, double zDegrees) {
  static constexpr double DegreesToRadians = 0.017453292519943295769;
  double halfX = xDegrees * DegreesToRadians * 0.5;
  double halfY = yDegrees * DegreesToRadians * 0.5;
  double halfZ = zDegrees * DegreesToRadians * 0.5;
  Quaternion qx = {std::sin(halfX), 0, 0, std::cos(halfX)};
  Quaternion qy = {0, std::sin(halfY), 0, std::cos(halfY)};
  Quaternion qz = {0, 0, std::sin(halfZ), std::cos(halfZ)};
  return MultiplyQuaternion(MultiplyQuaternion(qz, qy), qx);
}

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
  static constexpr double RadiansToDegrees = 57.295779513082320877;
  double sinY = std::clamp(-rotation[2][0], -1.0, 1.0);
  double y = std::asin(sinY);
  double cosY = std::cos(y);
  double x = 0;
  double z = 0;
  if (std::abs(cosY) > 1.0e-8) {
    x = std::atan2(rotation[2][1], rotation[2][2]);
    z = std::atan2(rotation[1][0], rotation[0][0]);
  } else {
    x = std::atan2(sinY >= 0 ? rotation[0][1] : -rotation[0][1], rotation[1][1]);
  }
  output[0] = x * RadiansToDegrees;
  output[1] = y * RadiansToDegrees;
  output[2] = z * RadiansToDegrees;
  return std::isfinite(output[0]) && std::isfinite(output[1]) && std::isfinite(output[2]);
}

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
  if (std::abs(matrix[3][3]) <= 1.0e-12 * magnitude) {
    return false;
  }
  double normalization = matrix[3][3];
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      matrix[row][column] /= normalization;
    }
  }
  double normalizedSource[4][4] = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      normalizedSource[row][column] = matrix[row][column];
    }
  }
  double perspective[4] = {0, 0, 0, 1};
  if (std::abs(matrix[3][0]) > 1.0e-12 || std::abs(matrix[3][1]) > 1.0e-12 ||
      std::abs(matrix[3][2]) > 1.0e-12) {
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

inline bool RecomposeMatrix3D(const DecomposedMatrix3D& value, Matrix3D* output) {
  if (output == nullptr) return false;
  double rotation[3][3] = {};
  RotationFromQuaternion(value.rotation, rotation);
  double shearScale[3][3] = {};
  shearScale[0][0] = value.scale[0];
  shearScale[0][1] = value.skew[0] * value.scale[1];
  shearScale[1][1] = value.scale[1];
  shearScale[0][2] = value.skew[1] * value.scale[2];
  shearScale[1][2] = value.skew[2] * value.scale[2];
  shearScale[2][2] = value.scale[2];
  double affine[4][4] = {};
  for (int row = 0; row < 3; ++row) {
    affine[row][3] = value.translate[row];
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
    perspective[3][column] = value.perspective[column];
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
  return IsInvertibleMatrix3D(*output);
}

inline bool IsMatrix3DSemanticallyRepresentable(const DecomposedMatrix3D& matrix) {
  constexpr double Tolerance = 1.0e-8;
  if (std::abs(matrix.perspective[0]) > Tolerance || std::abs(matrix.perspective[1]) > Tolerance ||
      std::abs(matrix.perspective[3] - 1.0) > Tolerance || matrix.perspective[2] > 0) {
    return false;
  }
  if (std::abs(matrix.perspective[2]) > 1.0e-12) {
    double distance = -1.0 / matrix.perspective[2];
    if (!std::isfinite(distance) || distance > 3.0e38) {
      return false;
    }
  }
  return true;
}

inline bool Matrix3DComponentFromChannel(const std::string& channel, Matrix3DComponent* component) {
  if (component == nullptr) {
    return false;
  }
  static const char* names[] = {"rotation.x", "rotation.y", "rotation.z", "scale.x",
                                "scale.y",    "scale.z",    "skew.xy",    "skew.xz",
                                "skew.yz",    "perspective"};
  for (size_t i = 0; i < Matrix3DComponentCount; ++i) {
    if (channel == names[i]) {
      *component = static_cast<Matrix3DComponent>(i);
      return true;
    }
  }
  return false;
}

inline bool IsMatrix3DDataBindOnlyChannel(const std::string& channel) {
  Matrix3DComponent component = Matrix3DComponent::RotateX;
  return Matrix3DComponentFromChannel(channel, &component);
}

inline bool ReadMatrix3DComponent(const DecomposedMatrix3D& matrix, Matrix3DComponent component,
                                  float* output) {
  if (output == nullptr) {
    return false;
  }
  static constexpr double RadiansToDegrees = 57.295779513082320877;
  double value = 0;
  switch (component) {
    case Matrix3DComponent::RotateX:
    case Matrix3DComponent::RotateY:
    case Matrix3DComponent::RotateZ: {
      double rotation[3] = {};
      if (!EulerXYZDegreesFromQuaternion(matrix.rotation, rotation)) {
        return false;
      }
      value = rotation[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::RotateX)];
      break;
    }
    case Matrix3DComponent::ScaleX:
    case Matrix3DComponent::ScaleY:
    case Matrix3DComponent::ScaleZ:
      value =
          matrix.scale[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::ScaleX)];
      break;
    case Matrix3DComponent::SkewXY:
    case Matrix3DComponent::SkewXZ:
    case Matrix3DComponent::SkewYZ:
      value = std::atan(matrix.skew[static_cast<int>(component) -
                                    static_cast<int>(Matrix3DComponent::SkewXY)]) *
              RadiansToDegrees;
      break;
    case Matrix3DComponent::PerspectiveDistance:
      if (!IsMatrix3DSemanticallyRepresentable(matrix)) {
        return false;
      }
      value = std::abs(matrix.perspective[2]) <= 1.0e-12 ? 0 : -1.0 / matrix.perspective[2];
      break;
    case Matrix3DComponent::Count:
      return false;
  }
  if (!std::isfinite(value) || std::abs(value) > 3.0e38) {
    return false;
  }
  *output = static_cast<float>(value);
  return true;
}

inline bool SetMatrix3DComponent(const DecomposedMatrix3D& source, Matrix3DComponent component,
                                 float input, DecomposedMatrix3D* output) {
  if (output == nullptr || !std::isfinite(input) || !IsMatrix3DSemanticallyRepresentable(source)) {
    return false;
  }
  static constexpr double DegreesToRadians = 0.017453292519943295769;
  DecomposedMatrix3D updated = source;
  double value = input;
  switch (component) {
    case Matrix3DComponent::RotateX:
    case Matrix3DComponent::RotateY:
    case Matrix3DComponent::RotateZ: {
      double rotation[3] = {};
      if (!EulerXYZDegreesFromQuaternion(source.rotation, rotation)) {
        return false;
      }
      rotation[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::RotateX)] = value;
      updated.rotation = QuaternionFromEulerXYZDegrees(rotation[0], rotation[1], rotation[2]);
      break;
    }
    case Matrix3DComponent::ScaleX:
    case Matrix3DComponent::ScaleY:
    case Matrix3DComponent::ScaleZ:
      updated.scale[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::ScaleX)] =
          value;
      break;
    case Matrix3DComponent::SkewXY:
    case Matrix3DComponent::SkewXZ:
    case Matrix3DComponent::SkewYZ: {
      double radians = value * DegreesToRadians;
      if (std::abs(std::cos(radians)) <= 1.0e-8) {
        return false;
      }
      updated.skew[static_cast<int>(component) - static_cast<int>(Matrix3DComponent::SkewXY)] =
          std::tan(radians);
      break;
    }
    case Matrix3DComponent::PerspectiveDistance:
      if (value < 0) {
        return false;
      }
      updated.perspective[0] = 0;
      updated.perspective[1] = 0;
      updated.perspective[2] = value == 0 ? 0 : -1.0 / value;
      updated.perspective[3] = 1;
      break;
    case Matrix3DComponent::Count:
      return false;
  }
  Matrix3D candidate = {};
  if (!RecomposeMatrix3D(updated, &candidate)) {
    return false;
  }
  *output = updated;
  return true;
}

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
    if (RecomposeMatrix3D(mixed, &result)) {
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
