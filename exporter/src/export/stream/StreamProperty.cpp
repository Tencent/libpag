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

#include "StreamProperty.h"
#include "utils/LayerHelper.h"
#include "utils/PAGExportSessionManager.h"

namespace exporter {

constexpr int MaxBezierTValue = 0x3FFFFFFF;

struct BezierPoint {
  BezierPoint(float x, float y, float z) : x(x), y(y), z(z) {
  }

  float x = 0;
  float y = 0;
  float z = 0;
};

static bool TSpanBigEnough(int tSpan) {
  return (tSpan >> 10) != 0;
}

static float MaxFloat3(float a, float b, float c) {
  auto t = a > b ? a : b;
  return t > c ? t : c;
}

static BezierPoint InterpolatePoint(const BezierPoint& v0, const BezierPoint& v1, const float& t) {
  auto x = v0.x + (v1.x - v0.x) * t;
  auto y = v0.y + (v1.y - v0.y) * t;
  auto z = v0.z + (v1.z - v0.z) * t;
  return {x, y, z};
}

static float Distance(const BezierPoint& a, const BezierPoint& b) {
  auto dx = a.x - b.x;
  auto dy = a.y - b.y;
  auto dz = a.z - b.z;
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

static bool DistanceExceedsLimit(const BezierPoint& pt1, const BezierPoint& pt2,
                                 const float& precision) {
  return MaxFloat3(fabsf(pt2.x - pt1.x), fabsf(pt2.y - pt1.y), fabsf(pt2.z - pt1.z)) > precision;
}

static bool CubicTooCurvy(const BezierPoint pts[4], const float& precision) {
  static float oneOfThird = 1.0f / 3;
  static float twoOfThird = 2.0f / 3;
  auto pt1 = InterpolatePoint(pts[0], pts[3], oneOfThird);
  auto pt2 = InterpolatePoint(pts[0], pts[3], twoOfThird);
  return DistanceExceedsLimit(pts[1], pt1, precision) ||
         DistanceExceedsLimit(pts[2], pt2, precision);
}

static float CaculateCubicLength(const BezierPoint* points, float distance, unsigned minT,
                                 unsigned maxT, const float& precision) {
  if (TSpanBigEnough(maxT - minT) && CubicTooCurvy(points, precision)) {
    auto halfT = (minT + maxT) >> 1;
    auto p1 = InterpolatePoint(points[0], points[1], 0.5);
    auto bc = InterpolatePoint(points[1], points[2], 0.5);
    auto p5 = InterpolatePoint(points[2], points[3], 0.5);
    auto p2 = InterpolatePoint(p1, bc, 0.5);
    auto p4 = InterpolatePoint(bc, p5, 0.5);
    auto p3 = InterpolatePoint(p2, p4, 0.5);
    BezierPoint result[7] = {points[0], p1, p2, p3, p4, p5, points[3]};
    distance = CaculateCubicLength(result, distance, minT, halfT, precision);
    distance = CaculateCubicLength(&result[3], distance, halfT, maxT, precision);
  } else {
    distance += Distance(points[0], points[3]);
  }
  return distance;
}

static bool PointOnLine(const float x1, const float y1, const float x2, const float y2,
                        const float x3, const float y3, const float& precision) {
  auto distance = x1 * y2 + x3 * y1 + x2 * y3 - x3 * y2 - x1 * y3 - x2 * y1;
  return fabs(distance) < precision;
}

static bool Point3DOnLine(const BezierPoint& point1, const BezierPoint& point2,
                          const BezierPoint& point3, const float& precision) {
  return PointOnLine(point1.x, point1.y, point2.x, point2.y, point3.x, point3.y, precision) &&
         PointOnLine(point1.x, point1.z, point2.x, point2.z, point3.x, point3.z, precision) &&
         PointOnLine(point1.y, point1.z, point2.y, point2.z, point3.y, point3.z, precision);
}

static float GetCubicLength(float startX, float startY, float startZ, float controlX1,
                            float controlY1, float controlZ1, float controlX2, float controlY2,
                            float controlZ2, float endX, float endY, float endZ,
                            float precision = 0.005f) {
  BezierPoint start(startX, startY, startZ);
  BezierPoint control1(controlX1, controlY1, controlZ1);
  BezierPoint control2(controlX2, controlY2, controlZ2);
  BezierPoint end(endX, endY, endZ);
  if (Point3DOnLine(start, end, control1, precision) &&
      Point3DOnLine(start, end, control2, precision)) {
    return Distance(start, end);
  }
  BezierPoint points[4] = {start, control1, control2, end};
  return CaculateCubicLength(points, 0, 0, MaxBezierTValue, precision);
}

static float GetCurveLength(const AEGP_StreamValue2& lastValue, const AEGP_StreamValue2& lastOut,
                            const AEGP_StreamValue2& currentIn,
                            const AEGP_StreamValue2& currentValue) {
  auto startX = static_cast<float>(lastValue.val.three_d.x);
  auto startY = static_cast<float>(lastValue.val.three_d.y);
  auto startZ = static_cast<float>(lastValue.val.three_d.z);
  auto controlX1 = static_cast<float>(startX + lastOut.val.three_d.x);
  auto controlY1 = static_cast<float>(startY + lastOut.val.three_d.y);
  auto controlZ1 = static_cast<float>(startY + lastOut.val.three_d.z);
  auto endX = static_cast<float>(currentValue.val.three_d.x);
  auto endY = static_cast<float>(currentValue.val.three_d.y);
  auto endZ = static_cast<float>(currentValue.val.three_d.z);
  auto controlX2 = static_cast<float>(endX + currentIn.val.three_d.x);
  auto controlY2 = static_cast<float>(endY + currentIn.val.three_d.y);
  auto controlZ2 = static_cast<float>(endY + currentIn.val.three_d.z);
  return GetCubicLength(startX, startY, startZ, controlX1, controlY1, controlZ1, controlX2,
                        controlY2, controlZ2, endX, endY, endZ);
}

std::vector<float> GetAverageSpeed(AEGP_StreamRefH streamHandle, int index, int dimensionality) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  std::vector<float> averageSpeedList;
  AEGP_StreamType streamType;
  Suites->StreamSuite4()->AEGP_GetStreamType(streamHandle, &streamType);
  if (streamType == AEGP_StreamType_NO_DATA) {
    averageSpeedList.push_back(1);
    return averageSpeedList;
  }

  AEGP_StreamValue2 lastValue = {};
  Suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(PluginID, streamHandle, index - 1, &lastValue);
  AEGP_StreamValue2 value = {};
  Suites->KeyframeSuite4()->AEGP_GetNewKeyframeValue(PluginID, streamHandle, index, &value);

  A_Time lastTime = {};
  Suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamHandle, index - 1, AEGP_LTimeMode_CompTime,
                                                 &lastTime);
  A_Time time = {};
  Suites->KeyframeSuite4()->AEGP_GetKeyframeTime(streamHandle, index, AEGP_LTimeMode_CompTime,
                                                 &time);
  auto duration = static_cast<float>(time.value - lastTime.value) / time.scale;

  switch (streamType) {
    case AEGP_StreamType_ThreeD_SPATIAL:
    case AEGP_StreamType_TwoD_SPATIAL: {
      AEGP_StreamValue2 spatialTemp = {};
      AEGP_StreamValue2 spatialIn = {};
      AEGP_StreamValue2 spatialOut = {};
      Suites->KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(
          PluginID, streamHandle, index - 1, &spatialTemp, &spatialOut);
      Suites->KeyframeSuite4()->AEGP_GetNewKeyframeSpatialTangents(PluginID, streamHandle, index,
                                                                   &spatialIn, &spatialTemp);
      auto curveLength = GetCurveLength(lastValue, spatialOut, spatialIn, value);
      Suites->StreamSuite4()->AEGP_DisposeStreamValue(&spatialTemp);
      Suites->StreamSuite4()->AEGP_DisposeStreamValue(&spatialIn);
      Suites->StreamSuite4()->AEGP_DisposeStreamValue(&spatialOut);
      averageSpeedList.push_back(curveLength / duration);
    } break;
    case AEGP_StreamType_ThreeD:
      averageSpeedList.push_back(static_cast<float>(value.val.three_d.x - lastValue.val.three_d.x) /
                                 duration);
      averageSpeedList.push_back(static_cast<float>(value.val.three_d.y - lastValue.val.three_d.y) /
                                 duration);
      averageSpeedList.push_back(static_cast<float>(value.val.three_d.z - lastValue.val.three_d.z) /
                                 duration);
      break;
    case AEGP_StreamType_TwoD:
      averageSpeedList.push_back(static_cast<float>(value.val.two_d.x - lastValue.val.two_d.x) /
                                 duration);
      averageSpeedList.push_back(static_cast<float>(value.val.two_d.y - lastValue.val.two_d.y) /
                                 duration);
      break;
    case AEGP_StreamType_OneD:
      averageSpeedList.push_back(static_cast<float>(value.val.one_d - lastValue.val.one_d) /
                                 duration);
      break;
    case AEGP_StreamType_COLOR:
      averageSpeedList.push_back(
          static_cast<float>(value.val.color.redF - lastValue.val.color.redF) * 255 / duration);
      averageSpeedList.push_back(
          static_cast<float>(value.val.color.greenF - lastValue.val.color.greenF) * 255 / duration);
      averageSpeedList.push_back(
          static_cast<float>(value.val.color.blueF - lastValue.val.color.blueF) * 255 / duration);
      break;
    default:
      averageSpeedList.push_back(1);
      break;
  }

  Suites->StreamSuite4()->AEGP_DisposeStreamValue(&value);
  Suites->StreamSuite4()->AEGP_DisposeStreamValue(&lastValue);

  if (dimensionality == 1 && averageSpeedList.size() > 1) {
    float averageSpeed = 0;
    for (const auto& speed : averageSpeedList) {
      averageSpeed += speed * speed;
    }
    averageSpeedList.clear();
    averageSpeedList.push_back(sqrtf(averageSpeed));
  }
  return averageSpeedList;
}

void CheckStreamExpression(AEGP_StreamRefH streamHandle) {
  const auto& Suites = GetSuites();
  const auto& PluginID = GetPluginID();

  A_Boolean canVary = false;
  Suites->StreamSuite4()->AEGP_CanVaryOverTime(streamHandle, &canVary);
  if (canVary) {
    A_Boolean flag = false;
    Suites->StreamSuite4()->AEGP_GetExpressionState(PluginID, streamHandle, &flag);
    if (flag) {
      PAGExportSessionManager::GetInstance()->recordWarning(AlertInfoType::Expression);
    }
  }
}

}  // namespace exporter
