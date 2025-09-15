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

#include "Helper.h"
#include <cmath>
#include "ScopedHelper.h"

namespace Helper {

#define MaxBezierTValue 0x3FFFFFFF

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

inline bool PointOnLine(const float x1, const float y1, const float x2, const float y2,
                        const float x3, const float y3, const float& precision) {
  auto distance = x1 * y2 + x3 * y1 + x2 * y3 - x3 * y2 - x1 * y3 - x2 * y1;
  return fabs(distance) < precision;
}

inline bool Point3DOnLine(const BezierPoint& point1, const BezierPoint& point2,
                          const BezierPoint& point3, const float& precision) {
  return PointOnLine(point1.x, point1.y, point2.x, point2.y, point3.x, point3.y, precision) &&
         PointOnLine(point1.x, point1.z, point2.x, point2.z, point3.x, point3.z, precision) &&
         PointOnLine(point1.y, point1.z, point2.y, point2.z, point3.y, point3.z, precision);
}

float GetCubicLength(float startX, float startY, float startZ, float controlX1, float controlY1,
                     float controlZ1, float controlX2, float controlY2, float controlZ2, float endX,
                     float endY, float endZ, float precision) {
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

void TraversalLayers(const std::shared_ptr<exporter::PAGExportSession>& session,
                     pag::Composition* composition, pag::LayerType layerType, pag::Frame startTime,
                     LayerHandlerWithTime handler, void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  exporter::ScopedAssign<pag::ID> compID(session->compID, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    exporter::ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
      TraversalLayers(session, preComposeLayer->composition, layerType,
                      startTime + preComposeLayer->compositionStartTime, handler, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      handler(session, layer, startTime, ctx);
    }
  }
}

void TraversalLayers(const std::shared_ptr<exporter::PAGExportSession>& session,
                     pag::Composition* composition, pag::LayerType layerType, LayerHandler handler,
                     void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  exporter::ScopedAssign<pag::ID> arCI(session->compID, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    exporter::ScopedAssign<pag::ID> arLI(session->layerID, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      TraversalLayers(session, static_cast<pag::PreComposeLayer*>(layer)->composition, layerType,
                      handler, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      handler(session, layer, ctx);
    }
  }
}

void TraversalLayers(const std::shared_ptr<exporter::PAGExportSession>& session,
                     std::vector<pag::Composition*>& compositions, pag::LayerType layerType,
                     LayerHandler handler, void* ctx) {
  for (auto composition : compositions) {
    exporter::ScopedAssign<pag::ID> compID(session->compID, composition->id);
    if (composition->type() == pag::CompositionType::Vector) {
      for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
        exporter::ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
        if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
          handler(session, layer, ctx);
        }
      }
    }
  }
}

}  // namespace Helper
