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

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/TrimType.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

//==============================================================================
// Static path helpers
//==============================================================================

// Emit a cubic Bezier segment with control points perpendicular to the radial direction,
// matching the native renderer's AddCurveToPath algorithm.
static void AppendPolystarCurve(std::string& d, float centerX, float centerY, float angleDelta,
                                float dx1, float dy1, float roundness1, float dx2, float dy2,
                                float roundness2) {
  float cp1x = dx1 - dy1 * roundness1 * angleDelta + centerX;
  float cp1y = dy1 + dx1 * roundness1 * angleDelta + centerY;
  float cp2x = dx2 + dy2 * roundness2 * angleDelta + centerX;
  float cp2y = dy2 - dx2 * roundness2 * angleDelta + centerY;
  d += "C" + FloatToString(cp1x) + "," + FloatToString(cp1y) + " " + FloatToString(cp2x) + "," +
       FloatToString(cp2y) + " " + FloatToString(dx2 + centerX) + "," +
       FloatToString(dy2 + centerY);
}

std::string BuildPolystarPath(const Polystar* ps) {
  if (ps->pointCount <= 0.0f) {
    return {};
  }
  float centerX = ps->position.x;
  float centerY = ps->position.y;
  bool isStar = ps->type == PolystarType::Star;

  if (isStar) {
    float direction = ps->reversed ? -1.0f : 1.0f;
    float angleStep = static_cast<float>(M_PI) / ps->pointCount;
    float currentAngle = (ps->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    int numPoints = static_cast<int>(std::ceil(ps->pointCount)) * 2;
    float decimalPart = ps->pointCount - std::floor(ps->pointCount);
    int decimalIndex = -2;
    if (decimalPart != 0.0f) {
      decimalIndex = direction > 0 ? 1 : numPoints - 3;
      currentAngle -= angleStep * decimalPart * 2.0f;
    }

    float firstDx = ps->outerRadius * std::cos(currentAngle);
    float firstDy = ps->outerRadius * std::sin(currentAngle);
    float lastDx = firstDx;
    float lastDy = firstDy;

    std::string d;
    d.reserve(static_cast<size_t>(numPoints) * 50);
    d += "M" + FloatToString(lastDx + centerX) + "," + FloatToString(lastDy + centerY);

    bool outerFlag = false;
    for (int i = 0; i < numPoints; i++) {
      float angleDelta = angleStep * direction;
      float dx = 0;
      float dy = 0;
      if (i == numPoints - 1) {
        dx = firstDx;
        dy = firstDy;
      } else {
        float radius = outerFlag ? ps->outerRadius : ps->innerRadius;
        if (i == decimalIndex || i == decimalIndex + 1) {
          radius = ps->innerRadius + decimalPart * (radius - ps->innerRadius);
          angleDelta *= decimalPart;
        }
        currentAngle += angleDelta;
        dx = radius * std::cos(currentAngle);
        dy = radius * std::sin(currentAngle);
      }
      if (ps->innerRoundness != 0.0f || ps->outerRoundness != 0.0f) {
        float lastRoundness = outerFlag ? ps->innerRoundness : ps->outerRoundness;
        float roundness = outerFlag ? ps->outerRoundness : ps->innerRoundness;
        AppendPolystarCurve(d, centerX, centerY, angleDelta * 0.5f, lastDx, lastDy, lastRoundness,
                            dx, dy, roundness);
        lastDx = dx;
        lastDy = dy;
      } else {
        d += "L" + FloatToString(dx + centerX) + "," + FloatToString(dy + centerY);
      }
      outerFlag = !outerFlag;
    }
    d += "Z";
    return d;
  }

  // Polygon
  int numPoints = static_cast<int>(std::floor(ps->pointCount));
  if (numPoints < 1) {
    return {};
  }
  float direction = ps->reversed ? -1.0f : 1.0f;
  float angleStep = static_cast<float>(M_PI) * 2.0f / static_cast<float>(numPoints);
  float currentAngle = (ps->rotation - 90.0f) * static_cast<float>(M_PI) / 180.0f;

  float firstDx = ps->outerRadius * std::cos(currentAngle);
  float firstDy = ps->outerRadius * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;

  std::string d;
  d.reserve(static_cast<size_t>(numPoints) * 50);
  d += "M" + FloatToString(lastDx + centerX) + "," + FloatToString(lastDy + centerY);

  float angleDelta = angleStep * direction;
  for (int i = 1; i < numPoints; i++) {
    currentAngle += angleDelta;
    float dx = ps->outerRadius * std::cos(currentAngle);
    float dy = ps->outerRadius * std::sin(currentAngle);
    if (ps->outerRoundness != 0.0f) {
      AppendPolystarCurve(d, centerX, centerY, angleDelta * 0.25f, lastDx, lastDy,
                          ps->outerRoundness, dx, dy, ps->outerRoundness);
    } else {
      d += "L" + FloatToString(dx + centerX) + "," + FloatToString(dy + centerY);
    }
    lastDx = dx;
    lastDy = dy;
  }
  if (ps->outerRoundness != 0.0f) {
    AppendPolystarCurve(d, centerX, centerY, angleDelta * 0.25f, lastDx, lastDy, ps->outerRoundness,
                        firstDx, firstDy, ps->outerRoundness);
  }
  d += "Z";
  return d;
}

// Compute signed area of a path to determine winding direction.
// Positive = clockwise, negative = counter-clockwise.
float ComputePathSignedArea(const PathData& pathData) {
  float area = 0.0f;
  Point startPoint = {};
  Point currentPoint = {};
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        break;
      case PathVerb::Line:
        area += (currentPoint.x * pts[0].y - pts[0].x * currentPoint.y);
        currentPoint = pts[0];
        break;
      case PathVerb::Quad:
        // Approximate quadratic bezier as line to endpoint
        area += (currentPoint.x * pts[1].y - pts[1].x * currentPoint.y);
        currentPoint = pts[1];
        break;
      case PathVerb::Cubic:
        // Approximate cubic bezier as line to endpoint
        area += (currentPoint.x * pts[2].y - pts[2].x * currentPoint.y);
        currentPoint = pts[2];
        break;
      case PathVerb::Close:
        area += (currentPoint.x * startPoint.y - startPoint.x * currentPoint.y);
        currentPoint = startPoint;
        break;
    }
  });
  return area / 2.0f;
}

bool IsPathClockwise(const PathData& pathData) {
  return ComputePathSignedArea(pathData) > 0;
}

float ComputeCubicBezierLength(Point p0, Point p1, Point p2, Point p3, int depth) {
  if (depth > 8) {
    float dx = p3.x - p0.x;
    float dy = p3.y - p0.y;
    return std::sqrt(dx * dx + dy * dy);
  }
  float chordDx = p3.x - p0.x;
  float chordDy = p3.y - p0.y;
  float chordLen = std::sqrt(chordDx * chordDx + chordDy * chordDy);
  float d01x = p1.x - p0.x, d01y = p1.y - p0.y;
  float d12x = p2.x - p1.x, d12y = p2.y - p1.y;
  float d23x = p3.x - p2.x, d23y = p3.y - p2.y;
  float polyLen = std::sqrt(d01x * d01x + d01y * d01y) + std::sqrt(d12x * d12x + d12y * d12y) +
                  std::sqrt(d23x * d23x + d23y * d23y);
  if (polyLen - chordLen < 0.01f) {
    return (polyLen + chordLen) / 2.0f;
  }
  Point m01 = {(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
  Point m12 = {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
  Point m23 = {(p2.x + p3.x) / 2, (p2.y + p3.y) / 2};
  Point m012 = {(m01.x + m12.x) / 2, (m01.y + m12.y) / 2};
  Point m123 = {(m12.x + m23.x) / 2, (m12.y + m23.y) / 2};
  Point mid = {(m012.x + m123.x) / 2, (m012.y + m123.y) / 2};
  return ComputeCubicBezierLength(p0, m01, m012, mid, depth + 1) +
         ComputeCubicBezierLength(mid, m123, m23, p3, depth + 1);
}

float ComputeQuadBezierLength(Point p0, Point p1, Point p2) {
  Point c1 = {p0.x + 2.0f / 3.0f * (p1.x - p0.x), p0.y + 2.0f / 3.0f * (p1.y - p0.y)};
  Point c2 = {p2.x + 2.0f / 3.0f * (p1.x - p2.x), p2.y + 2.0f / 3.0f * (p1.y - p2.y)};
  return ComputeCubicBezierLength(p0, c1, c2, p2);
}

float ComputePathLength(const PathData& pathData) {
  float length = 0.0f;
  Point startPoint = {};
  Point currentPoint = {};
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        startPoint = pts[0];
        currentPoint = pts[0];
        break;
      case PathVerb::Line: {
        float dx = pts[0].x - currentPoint.x;
        float dy = pts[0].y - currentPoint.y;
        length += std::sqrt(dx * dx + dy * dy);
        currentPoint = pts[0];
        break;
      }
      case PathVerb::Quad:
        length += ComputeQuadBezierLength(currentPoint, pts[0], pts[1]);
        currentPoint = pts[1];
        break;
      case PathVerb::Cubic:
        length += ComputeCubicBezierLength(currentPoint, pts[0], pts[1], pts[2]);
        currentPoint = pts[2];
        break;
      case PathVerb::Close: {
        float dx = startPoint.x - currentPoint.x;
        float dy = startPoint.y - currentPoint.y;
        length += std::sqrt(dx * dx + dy * dy);
        currentPoint = startPoint;
        break;
      }
    }
  });
  return length;
}

PathData ApplyRoundCorner(const PathData& pathData, float radius) {
  if (radius <= 0 || pathData.isEmpty()) {
    return pathData;
  }
  struct Segment {
    PathVerb verb = PathVerb::Line;
    Point endPoint = {};
    Point ctrl1 = {};
    Point ctrl2 = {};
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };
  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {};
        seg.verb = PathVerb::Line;
        seg.endPoint = pts[0];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {};
        seg.verb = PathVerb::Quad;
        seg.ctrl1 = pts[0];
        seg.endPoint = pts[1];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {};
        seg.verb = PathVerb::Cubic;
        seg.ctrl1 = pts[0];
        seg.ctrl2 = pts[1];
        seg.endPoint = pts[2];
        currentContour.segments.push_back(seg);
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
  });
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  PathData result = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        result.close();
      }
      continue;
    }
    size_t n = contour.segments.size();

    std::vector<Point> vertices = {};
    vertices.reserve(n + 1);
    vertices.push_back(contour.startPoint);
    for (size_t i = 0; i < n; i++) {
      vertices.push_back(contour.segments[i].endPoint);
    }
    size_t vertexCount = vertices.size();

    size_t loopCount = contour.closed ? vertexCount : n;
    bool firstOutput = true;

    for (size_t vi = 0; vi < loopCount; vi++) {
      Point vertex = vertices[vi];

      bool incomingIsLine = (vi == 0) ? true : (contour.segments[vi - 1].verb == PathVerb::Line);
      bool outgoingIsLine = (vi < n) ? (contour.segments[vi].verb == PathVerb::Line) : true;

      bool hasPrev = (vi > 0) || contour.closed;

      if (!hasPrev || !incomingIsLine || !outgoingIsLine) {
        if (firstOutput) {
          result.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        if (vi < n) {
          auto& seg = contour.segments[vi];
          if (seg.verb == PathVerb::Line) {
            result.lineTo(seg.endPoint.x, seg.endPoint.y);
          } else if (seg.verb == PathVerb::Quad) {
            result.quadTo(seg.ctrl1.x, seg.ctrl1.y, seg.endPoint.x, seg.endPoint.y);
          } else if (seg.verb == PathVerb::Cubic) {
            result.cubicTo(seg.ctrl1.x, seg.ctrl1.y, seg.ctrl2.x, seg.ctrl2.y, seg.endPoint.x,
                           seg.endPoint.y);
          }
        }
        continue;
      }

      Point prevVertex = {};
      if (vi == 0) {
        prevVertex = contour.segments[n - 1].endPoint;
      } else if (vi == 1) {
        prevVertex = contour.startPoint;
      } else {
        prevVertex = contour.segments[vi - 2].endPoint;
      }
      Point nextVertex = (vi < n) ? contour.segments[vi].endPoint : contour.startPoint;

      float dx1 = vertex.x - prevVertex.x;
      float dy1 = vertex.y - prevVertex.y;
      float dx2 = nextVertex.x - vertex.x;
      float dy2 = nextVertex.y - vertex.y;
      float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
      float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

      if (len1 < 0.001f || len2 < 0.001f) {
        if (firstOutput) {
          result.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        if (vi < n) {
          result.lineTo(contour.segments[vi].endPoint.x, contour.segments[vi].endPoint.y);
        }
        continue;
      }

      float effectiveRadius = std::min({radius, len1 / 2.0f, len2 / 2.0f});
      float t1 = effectiveRadius / len1;
      float t2 = effectiveRadius / len2;
      Point p1 = {vertex.x - dx1 * t1, vertex.y - dy1 * t1};
      Point p2 = {vertex.x + dx2 * t2, vertex.y + dy2 * t2};
      Point cp1 = {p1.x + (vertex.x - p1.x) * kBezierKappa,
                   p1.y + (vertex.y - p1.y) * kBezierKappa};
      Point cp2 = {p2.x + (vertex.x - p2.x) * kBezierKappa,
                   p2.y + (vertex.y - p2.y) * kBezierKappa};

      if (firstOutput) {
        result.moveTo(p1.x, p1.y);
        firstOutput = false;
      } else {
        result.lineTo(p1.x, p1.y);
      }
      result.cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, p2.x, p2.y);
      if (vi == n - 1 && !contour.closed) {
        result.lineTo(contour.segments[vi].endPoint.x, contour.segments[vi].endPoint.y);
      }
    }
    if (contour.closed) {
      result.close();
    }
  }
  return result;
}

PathData ReversePathData(const PathData& pathData) {
  if (pathData.isEmpty()) {
    return pathData;
  }
  struct Segment {
    PathVerb verb = PathVerb::Move;
    std::vector<Point> points = {};
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };
  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {PathVerb::Line, {pts[0]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {PathVerb::Quad, {pts[0], pts[1]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {PathVerb::Cubic, {pts[0], pts[1], pts[2]}};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
  });
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  PathData result = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        result.close();
      }
      continue;
    }
    auto& lastSeg = contour.segments.back();
    Point newStart = lastSeg.points.back();
    result.moveTo(newStart.x, newStart.y);

    for (int i = static_cast<int>(contour.segments.size()) - 1; i >= 0; i--) {
      auto& seg = contour.segments[static_cast<size_t>(i)];
      Point segStart = (i == 0) ? contour.startPoint
                                : contour.segments[static_cast<size_t>(i - 1)].points.back();
      switch (seg.verb) {
        case PathVerb::Line:
          result.lineTo(segStart.x, segStart.y);
          break;
        case PathVerb::Quad:
          result.quadTo(seg.points[0].x, seg.points[0].y, segStart.x, segStart.y);
          break;
        case PathVerb::Cubic:
          result.cubicTo(seg.points[1].x, seg.points[1].y, seg.points[0].x, seg.points[0].y,
                         segStart.x, segStart.y);
          break;
        default:
          break;
      }
    }
    if (contour.closed) {
      result.close();
    }
  }
  return result;
}

std::string TransformPathDataToSVG(const PathData& pathData, const Matrix& m) {
  PathData result = PathDataFromSVGString("");
  pathData.forEach([&](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move: {
        Point p = m.mapPoint(pts[0]);
        result.moveTo(p.x, p.y);
        break;
      }
      case PathVerb::Line: {
        Point p = m.mapPoint(pts[0]);
        result.lineTo(p.x, p.y);
        break;
      }
      case PathVerb::Quad: {
        Point c = m.mapPoint(pts[0]);
        Point p = m.mapPoint(pts[1]);
        result.quadTo(c.x, c.y, p.x, p.y);
        break;
      }
      case PathVerb::Cubic: {
        Point c1 = m.mapPoint(pts[0]);
        Point c2 = m.mapPoint(pts[1]);
        Point p = m.mapPoint(pts[2]);
        result.cubicTo(c1.x, c1.y, c2.x, c2.y, p.x, p.y);
        break;
      }
      case PathVerb::Close:
        result.close();
        break;
    }
  });
  return PathDataToSVGString(result);
}

PathData GeoToPathData(const Element* element, NodeType type) {
  PathData pathData = PathDataFromSVGString("");
  switch (type) {
    case NodeType::Rectangle: {
      auto r = static_cast<const Rectangle*>(element);
      float x = r->position.x - r->size.width / 2;
      float y = r->position.y - r->size.height / 2;
      float w = r->size.width;
      float h = r->size.height;
      if (r->roundness <= 0) {
        pathData.moveTo(x, y);
        if (r->reversed) {
          pathData.lineTo(x, y + h);
          pathData.lineTo(x + w, y + h);
          pathData.lineTo(x + w, y);
        } else {
          pathData.lineTo(x + w, y);
          pathData.lineTo(x + w, y + h);
          pathData.lineTo(x, y + h);
        }
        pathData.close();
      } else {
        float rn = std::min(r->roundness, std::min(w / 2, h / 2));
        pathData.moveTo(x + rn, y);
        if (r->reversed) {
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y, x, y + rn * (1 - kBezierKappa), x,
                           y + rn);
          pathData.lineTo(x, y + h - rn);
          pathData.cubicTo(x, y + h - rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y + h,
                           x + rn, y + h);
          pathData.lineTo(x + w - rn, y + h);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y + h, x + w,
                           y + h - rn * (1 - kBezierKappa), x + w, y + h - rn);
          pathData.lineTo(x + w, y + rn);
          pathData.cubicTo(x + w, y + rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa), y,
                           x + w - rn, y);
        } else {
          pathData.lineTo(x + w - rn, y);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y, x + w, y + rn * (1 - kBezierKappa),
                           x + w, y + rn);
          pathData.lineTo(x + w, y + h - rn);
          pathData.cubicTo(x + w, y + h - rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa),
                           y + h, x + w - rn, y + h);
          pathData.lineTo(x + rn, y + h);
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y + h, x, y + h - rn * (1 - kBezierKappa),
                           x, y + h - rn);
          pathData.lineTo(x, y + rn);
          pathData.cubicTo(x, y + rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y, x + rn,
                           y);
        }
        pathData.close();
      }
      break;
    }
    case NodeType::Ellipse: {
      auto e = static_cast<const Ellipse*>(element);
      float cx = e->position.x;
      float cy = e->position.y;
      float rx = e->size.width / 2;
      float ry = e->size.height / 2;
      float kx = rx * kBezierKappa;
      float ky = ry * kBezierKappa;
      pathData.moveTo(cx - rx, cy);
      if (e->reversed) {
        pathData.cubicTo(cx - rx, cy + ky, cx - kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx + kx, cy + ry, cx + rx, cy + ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy - ky, cx + kx, cy - ry, cx, cy - ry);
        pathData.cubicTo(cx - kx, cy - ry, cx - rx, cy - ky, cx - rx, cy);
      } else {
        pathData.cubicTo(cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
        pathData.cubicTo(cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
      }
      pathData.close();
      break;
    }
    case NodeType::Path: {
      auto p = static_cast<const Path*>(element);
      if (p->data) {
        if (p->reversed) {
          pathData = ReversePathData(*p->data);
        } else {
          pathData.setPathData(*p->data);
        }
      }
      break;
    }
    case NodeType::Polystar: {
      auto ps = static_cast<const Polystar*>(element);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        pathData = PathDataFromSVGString(d);
      }
      break;
    }
    default:
      break;
  }
  return pathData;
}

std::string RectToPathData(const Rectangle* r) {
  float x = r->position.x - r->size.width / 2;
  float y = r->position.y - r->size.height / 2;
  float w = r->size.width;
  float h = r->size.height;
  if (r->roundness <= 0) {
    if (r->reversed) {
      return "M" + FloatToString(x) + "," + FloatToString(y) + "V" + FloatToString(y + h) + "H" +
             FloatToString(x + w) + "V" + FloatToString(y) + "Z";
    }
    return "M" + FloatToString(x) + "," + FloatToString(y) + "H" + FloatToString(x + w) + "V" +
           FloatToString(y + h) + "H" + FloatToString(x) + "Z";
  }
  float rn = std::min(r->roundness, std::min(w / 2, h / 2));
  std::string d;
  if (r->reversed) {
    d += "M" + FloatToString(x + rn) + "," + FloatToString(y);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x) + "," +
         FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w) +
         "," + FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w - rn) +
         "," + FloatToString(y);
    d += "Z";
  } else {
    d += "M" + FloatToString(x + rn) + "," + FloatToString(y);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w) +
         "," + FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w - rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x) + "," +
         FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + rn) +
         "," + FloatToString(y);
    d += "Z";
  }
  return d;
}

std::string EllipseToPathData(const Ellipse* e) {
  float cx = e->position.x;
  float cy = e->position.y;
  float rx = e->size.width / 2;
  float ry = e->size.height / 2;
  const char* sweep = e->reversed ? "0" : "1";
  return "M" + FloatToString(cx - rx) + "," + FloatToString(cy) + "A" + FloatToString(rx) + "," +
         FloatToString(ry) + " 0 1 " + sweep + " " + FloatToString(cx + rx) + "," +
         FloatToString(cy) + "A" + FloatToString(rx) + "," + FloatToString(ry) + " 0 1 " + sweep +
         " " + FloatToString(cx - rx) + "," + FloatToString(cy) + "Z";
}

std::string ReversePathDataToSVGString(const PathData& pathData) {
  struct Segment {
    PathVerb verb = PathVerb::Move;
    std::vector<Point> points = {};
  };
  struct Contour {
    Point startPoint = {};
    std::vector<Segment> segments = {};
    bool closed = false;
  };

  std::vector<Contour> contours = {};
  Contour currentContour = {};
  bool hasContour = false;

  const auto& verbs = pathData.verbs();
  const auto& points = pathData.points();
  size_t pointIndex = 0;
  for (auto verb : verbs) {
    const Point* pts = points.data() + pointIndex;
    switch (verb) {
      case PathVerb::Move:
        if (hasContour) {
          contours.push_back(std::move(currentContour));
          currentContour = {};
        }
        currentContour.startPoint = pts[0];
        hasContour = true;
        break;
      case PathVerb::Line: {
        Segment seg = {};
        seg.verb = PathVerb::Line;
        seg.points = {pts[0]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Quad: {
        Segment seg = {};
        seg.verb = PathVerb::Quad;
        seg.points = {pts[0], pts[1]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Cubic: {
        Segment seg = {};
        seg.verb = PathVerb::Cubic;
        seg.points = {pts[0], pts[1], pts[2]};
        currentContour.segments.push_back(std::move(seg));
        break;
      }
      case PathVerb::Close:
        currentContour.closed = true;
        break;
    }
    pointIndex += PathData::PointsPerVerb(verb);
  }
  if (hasContour) {
    contours.push_back(std::move(currentContour));
  }

  std::string result;
  result.reserve(pathData.verbs().size() * 24);
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      result +=
          "M" + FloatToString(contour.startPoint.x) + " " + FloatToString(contour.startPoint.y);
      if (contour.closed) {
        result += "Z";
      }
      continue;
    }
    auto& lastSeg = contour.segments.back();
    Point newStart = lastSeg.points.back();
    result += "M" + FloatToString(newStart.x) + " " + FloatToString(newStart.y);

    for (int i = static_cast<int>(contour.segments.size()) - 1; i >= 0; i--) {
      auto& seg = contour.segments[static_cast<size_t>(i)];
      Point segStart = (i == 0) ? contour.startPoint
                                : contour.segments[static_cast<size_t>(i - 1)].points.back();
      switch (seg.verb) {
        case PathVerb::Line:
          result += "L" + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        case PathVerb::Quad:
          result += "Q" + FloatToString(seg.points[0].x) + " " + FloatToString(seg.points[0].y) +
                    " " + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        case PathVerb::Cubic:
          result += "C" + FloatToString(seg.points[1].x) + " " + FloatToString(seg.points[1].y) +
                    " " + FloatToString(seg.points[0].x) + " " + FloatToString(seg.points[0].y) +
                    " " + FloatToString(segStart.x) + " " + FloatToString(segStart.y);
          break;
        default:
          break;
      }
    }
    if (contour.closed) {
      result += "Z";
    }
  }
  return result;
}

std::string GetPathSVGString(const Path* path) {
  if (!path->data || path->data->isEmpty()) {
    return {};
  }
  if (path->reversed) {
    return ReversePathDataToSVGString(*path->data);
  }
  return PathDataToSVGString(*path->data);
}

//==============================================================================
// HTMLWriter – SVG fill/stroke helpers
//==============================================================================

void HTMLWriter::applySVGFill(HTMLBuilder& out, const Fill* fill) {
  if (!fill) {
    out.addAttr("fill", "none");
    return;
  }
  float alpha = 1.0f;
  std::string f = colorToSVGFill(fill->color, &alpha);
  out.addAttr("fill", f);
  float ea = alpha * fill->alpha;
  if (ea < 1.0f) {
    out.addAttr("fill-opacity", FloatToString(ea));
  }
  if (fill->fillRule == FillRule::EvenOdd) {
    out.addAttr("fill-rule", "evenodd");
  }
}

void HTMLWriter::applySVGStroke(HTMLBuilder& out, const Stroke* stroke, float pathLength) {
  if (!stroke) {
    return;
  }
  float alpha = 1.0f;
  std::string s = colorToSVGFill(stroke->color, &alpha);
  out.addAttr("stroke", s);
  float ea = alpha * stroke->alpha;
  if (ea < 1.0f) {
    out.addAttr("stroke-opacity", FloatToString(ea));
  }
  float effectiveWidth = stroke->width;
  if (stroke->align != StrokeAlign::Center) {
    effectiveWidth *= 2.0f;
  }
  if (effectiveWidth != 1.0f) {
    out.addAttr("stroke-width", FloatToString(effectiveWidth));
  }
  if (stroke->cap == LineCap::Round) {
    out.addAttr("stroke-linecap", "round");
  } else if (stroke->cap == LineCap::Square) {
    out.addAttr("stroke-linecap", "square");
  }
  if (stroke->join == LineJoin::Round) {
    out.addAttr("stroke-linejoin", "round");
  } else if (stroke->join == LineJoin::Bevel) {
    out.addAttr("stroke-linejoin", "bevel");
  }
  if (stroke->join == LineJoin::Miter && stroke->miterLimit != 4.0f) {
    out.addAttr("stroke-miterlimit", FloatToString(stroke->miterLimit));
  }
  if (!stroke->dashes.empty()) {
    std::vector<float> dashValues = stroke->dashes;
    if (stroke->dashAdaptive && pathLength > 0) {
      float patternLength = 0.0f;
      for (float dv : stroke->dashes) {
        patternLength += dv;
      }
      if (patternLength > 0) {
        int n = static_cast<int>(std::round(pathLength / patternLength));
        if (n > 0) {
          float scaleFactor = pathLength / (static_cast<float>(n) * patternLength);
          for (size_t j = 0; j < dashValues.size(); j++) {
            dashValues[j] = stroke->dashes[j] * scaleFactor;
          }
        }
      }
    }
    std::string d;
    for (size_t i = 0; i < dashValues.size(); i++) {
      if (i > 0) {
        d += ',';
      }
      d += FloatToString(dashValues[i]);
    }
    out.addAttr("stroke-dasharray", d);
  }
  if (stroke->dashOffset != 0.0f) {
    out.addAttr("stroke-dashoffset", FloatToString(stroke->dashOffset));
  }
}

//==============================================================================
// HTMLWriter – geometry rendering
//==============================================================================

bool HTMLWriter::canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
                        bool hasTrim, bool hasMerge) {
  if (hasTrim || hasMerge || geos.size() != 1 || stroke) {
    return false;
  }
  if (!geos[0].modifiedPathData.empty()) {
    return false;
  }
  auto t = geos[0].type;
  if (t != NodeType::Rectangle && t != NodeType::Ellipse) {
    return false;
  }
  if (fill) {
    if (fill->fillRule == FillRule::EvenOdd) {
      return false;
    }
    if (fill->color && fill->color->nodeType() == NodeType::LinearGradient) {
      auto g = static_cast<const LinearGradient*>(fill->color);
      if (!FloatNearlyZero(g->matrix.tx) || !FloatNearlyZero(g->matrix.ty)) {
        return false;
      }
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        return false;
      }
    }
    if (fill->color && fill->color->nodeType() == NodeType::RadialGradient) {
      auto g = static_cast<const RadialGradient*>(fill->color);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      bool hasRotation = !FloatNearlyZero(g->matrix.b) || !FloatNearlyZero(g->matrix.c);
      bool nonUniformScale = !FloatNearlyZero(sx - sy);
      if (hasRotation && nonUniformScale) {
        return false;
      }
    }
    if (fill->color && fill->color->nodeType() == NodeType::DiamondGradient) {
      return false;
    }
    if (fill->color && fill->color->nodeType() == NodeType::ConicGradient) {
      auto g = static_cast<const ConicGradient*>(fill->color);
      float sx = std::sqrt(g->matrix.a * g->matrix.a + g->matrix.b * g->matrix.b);
      float sy = std::sqrt(g->matrix.c * g->matrix.c + g->matrix.d * g->matrix.d);
      if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
        return false;
      }
    }
  }
  return true;
}

void HTMLWriter::renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                              BlendMode painterBlend) {
  bool isRect = geo.type == NodeType::Rectangle;
  float left = 0, top = 0, w = 0, h = 0;
  float roundness = 0;
  bool isEllipse = false;

  if (isRect) {
    auto rect = static_cast<const Rectangle*>(geo.element);
    left = rect->position.x - rect->size.width / 2;
    top = rect->position.y - rect->size.height / 2;
    w = rect->size.width;
    h = rect->size.height;
    roundness = std::min(rect->roundness, std::min(w / 2, h / 2));
  } else {
    auto el = static_cast<const Ellipse*>(geo.element);
    left = el->position.x - el->size.width / 2;
    top = el->position.y - el->size.height / 2;
    w = el->size.width;
    h = el->size.height;
    isEllipse = true;
  }

  std::string style = "position:absolute;left:" + FloatToString(left) +
                      "px;top:" + FloatToString(top) + "px;width:" + FloatToString(w) +
                      "px;height:" + FloatToString(h) + "px";

  if (isEllipse) {
    style += ";border-radius:50%";
  } else if (roundness > 0) {
    style += ";border-radius:" + FloatToString(roundness) + "px";
  }

  if (fill && fill->color) {
    float ca = 1.0f;
    std::string css = colorToCSS(fill->color, &ca);
    auto ct = fill->color->nodeType();
    if (ct == NodeType::LinearGradient || ct == NodeType::RadialGradient ||
        ct == NodeType::ConicGradient) {
      style += ";background:" + css;
      float fillOpacity = ca * fill->alpha;
      if (fillOpacity < 1.0f) {
        alpha *= fillOpacity;
      }
    } else if (ct == NodeType::ImagePattern) {
      auto p = static_cast<const ImagePattern*>(fill->color);
      style += ";background-image:" + css;
      std::string repeatX = "no-repeat";
      std::string repeatY = "no-repeat";
      if (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Mirror) {
        repeatX = "repeat";
      }
      if (p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Mirror) {
        repeatY = "repeat";
      }
      if (repeatX == repeatY) {
        style += ";background-repeat:" + repeatX;
      } else {
        style += ";background-repeat:" + repeatX + " " + repeatY;
      }
      if (!p->matrix.isIdentity()) {
        float sx = std::sqrt(p->matrix.a * p->matrix.a + p->matrix.b * p->matrix.b);
        float sy = std::sqrt(p->matrix.c * p->matrix.c + p->matrix.d * p->matrix.d);
        if (!FloatNearlyZero(sx - 1.0f) || !FloatNearlyZero(sy - 1.0f)) {
          style += ";background-size:" + FloatToString(sx * 100.0f) + "% " +
                   FloatToString(sy * 100.0f) + "%";
        }
        if (!FloatNearlyZero(p->matrix.tx) || !FloatNearlyZero(p->matrix.ty)) {
          style += ";background-position:" + FloatToString(p->matrix.tx) + "px " +
                   FloatToString(p->matrix.ty) + "px";
        }
      } else {
        // For non-tiling modes (Clamp/Decal), do not set background-size.
        // The image stays at its natural size with no-repeat (closest to Decal behavior).
        // CSS has no equivalent for Clamp (edge pixel extension).
      }
      // TODO: ImagePattern.mipmapMode is ignored. CSS/SVG has no direct control over mipmap
      // sampling; browsers handle mipmap generation internally.
      if (p->filterMode == FilterMode::Nearest) {
        style += ";image-rendering:pixelated";
      }
      if (fill->alpha < 1.0f) {
        alpha *= fill->alpha;
      }
    } else if (ct == NodeType::SolidColor) {
      float ea = ca * fill->alpha;
      if (ea < 1.0f) {
        auto sc = static_cast<const SolidColor*>(fill->color);
        style += ";background-color:" + ColorToRGBA(sc->color, fill->alpha);
      } else {
        style += ";background-color:" + css;
      }
    } else {
      style += ";background-color:" + css;
    }
  }
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }

  out.openTag("div");
  out.addAttr("style", style);
  out.closeTagSelfClosing();
}

void HTMLWriter::renderSVG(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, float alpha, BlendMode painterBlend,
                           const TrimPath* trim, MergePathMode) {
  float pad = 0.0f;
  if (stroke) {
    pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
  }
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (auto& g : geos) {
    float gx = 0, gy = 0, gw = 0, gh = 0;
    if (!g.modifiedPathData.empty()) {
      PathData pd = PathDataFromSVGString(g.modifiedPathData);
      if (!pd.isEmpty()) {
        Rect b = pd.getBounds();
        gx = b.x;
        gy = b.y;
        gw = b.width;
        gh = b.height;
      }
    } else {
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          gx = r->position.x - r->size.width / 2;
          gy = r->position.y - r->size.height / 2;
          gw = r->size.width;
          gh = r->size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          gx = e->position.x - e->size.width / 2;
          gy = e->position.y - e->size.height / 2;
          gw = e->size.width;
          gh = e->size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            gx = b.x;
            gy = b.y;
            gw = b.width;
            gh = b.height;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          float r = std::max(ps->outerRadius, ps->innerRadius);
          gx = ps->position.x - r;
          gy = ps->position.y - r;
          gw = r * 2;
          gh = r * 2;
          break;
        }
        default:
          break;
      }
    }
    x0 = std::min(x0, gx - pad);
    y0 = std::min(y0, gy - pad);
    x1 = std::max(x1, gx + gw + pad);
    y1 = std::max(y1, gy + gh + pad);
  }
  float sw = x1 - x0;
  float sh = y1 - y0;
  if (sw <= 0 || sh <= 0) {
    return;
  }

  std::string svgStyle =
      "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      svgStyle += ";mix-blend-mode:";
      svgStyle += blendStr;
    }
  }
  if (alpha < 1.0f) {
    svgStyle += ";opacity:" + FloatToString(alpha);
  }

  out.openTag("svg");
  out.addAttr("xmlns", "http://www.w3.org/2000/svg");
  out.addAttr("width", FloatToString(sw));
  out.addAttr("height", FloatToString(sh));
  out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                             " " + FloatToString(sh));
  out.addAttr("style", svgStyle);
  out.closeTagStart();

  bool needStrokeClip = stroke && stroke->align != StrokeAlign::Center;
  std::string strokeClipId;
  if (needStrokeClip) {
    strokeClipId = _ctx->nextId("sclip");
    out.openTag("defs");
    out.closeTagStart();
    out.openTag("clipPath");
    out.addAttr("id", strokeClipId);
    if (stroke->align == StrokeAlign::Outside) {
      out.addAttr("clip-rule", "evenodd");
    }
    out.closeTagStart();
    if (stroke->align == StrokeAlign::Outside) {
      out.openTag("rect");
      out.addAttr("x", FloatToString(x0));
      out.addAttr("y", FloatToString(y0));
      out.addAttr("width", FloatToString(sw));
      out.addAttr("height", FloatToString(sh));
      out.closeTagSelfClosing();
    }
    for (auto& g : geos) {
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          float cx = r->position.x - r->size.width / 2;
          float cy = r->position.y - r->size.height / 2;
          out.openTag("rect");
          if (!FloatNearlyZero(cx)) {
            out.addAttr("x", FloatToString(cx));
          }
          if (!FloatNearlyZero(cy)) {
            out.addAttr("y", FloatToString(cy));
          }
          out.addAttr("width", FloatToString(r->size.width));
          out.addAttr("height", FloatToString(r->size.height));
          if (r->roundness > 0) {
            out.addAttr("rx", FloatToString(r->roundness));
          }
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          out.openTag("ellipse");
          out.addAttr("cx", FloatToString(e->position.x));
          out.addAttr("cy", FloatToString(e->position.y));
          out.addAttr("rx", FloatToString(e->size.width / 2));
          out.addAttr("ry", FloatToString(e->size.height / 2));
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          std::string clipD = GetPathSVGString(p);
          if (!clipD.empty()) {
            out.openTag("path");
            out.addAttr("d", clipD);
            out.closeTagSelfClosing();
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          std::string clipD = BuildPolystarPath(ps);
          if (!clipD.empty()) {
            out.openTag("path");
            out.addAttr("d", clipD);
            out.closeTagSelfClosing();
          }
          break;
        }
        default:
          break;
      }
    }
    out.closeTag();  // </clipPath>
    out.closeTag();  // </defs>
  }

  if (needStrokeClip) {
    out.openTag("g");
    out.addAttr("clip-path", "url(#" + strokeClipId + ")");
    out.closeTagStart();
  }

  bool isContinuousTrim = trim && trim->type == TrimType::Continuous;
  std::vector<float> pathLengths = {};
  float totalPathLength = 0.0f;
  if (isContinuousTrim) {
    for (auto& g : geos) {
      float len = computeGeoPathLength(g);
      pathLengths.push_back(len);
      totalPathLength += len;
    }
  }

  for (size_t geoIdx = 0; geoIdx < geos.size(); geoIdx++) {
    auto& g = geos[geoIdx];
    if (!g.modifiedPathData.empty()) {
      out.openTag("path");
      out.addAttr("d", g.modifiedPathData);
      applySVGFill(out, trim ? nullptr : fill);
      if (isContinuousTrim) {
        applySVGStroke(out, stroke, computeGeoPathLength(g));
        applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
      } else {
        applySVGStroke(out, stroke);
        applyTrimAttrs(out, trim);
      }
      out.closeTagSelfClosing();
      continue;
    }
    switch (g.type) {
      case NodeType::Rectangle: {
        auto r = static_cast<const Rectangle*>(g.element);
        float x = r->position.x - r->size.width / 2;
        float y = r->position.y - r->size.height / 2;
        out.openTag("rect");
        if (!FloatNearlyZero(x)) {
          out.addAttr("x", FloatToString(x));
        }
        if (!FloatNearlyZero(y)) {
          out.addAttr("y", FloatToString(y));
        }
        out.addAttr("width", FloatToString(r->size.width));
        out.addAttr("height", FloatToString(r->size.height));
        if (r->roundness > 0) {
          out.addAttr("rx", FloatToString(r->roundness));
          out.addAttr("ry", FloatToString(r->roundness));
        }
        applySVGFill(out, trim ? nullptr : fill);
        if (isContinuousTrim) {
          applySVGStroke(out, stroke, computeGeoPathLength(g));
          applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
        } else {
          applySVGStroke(out, stroke);
          applyTrimAttrs(out, trim);
        }
        out.closeTagSelfClosing();
        break;
      }
      case NodeType::Ellipse: {
        auto e = static_cast<const Ellipse*>(g.element);
        out.openTag("ellipse");
        out.addAttr("cx", FloatToString(e->position.x));
        out.addAttr("cy", FloatToString(e->position.y));
        out.addAttr("rx", FloatToString(e->size.width / 2));
        out.addAttr("ry", FloatToString(e->size.height / 2));
        applySVGFill(out, trim ? nullptr : fill);
        if (isContinuousTrim) {
          applySVGStroke(out, stroke, computeGeoPathLength(g));
          applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx, true);
        } else {
          applySVGStroke(out, stroke);
          applyTrimAttrs(out, trim, true);
        }
        out.closeTagSelfClosing();
        break;
      }
      case NodeType::Path: {
        auto p = static_cast<const Path*>(g.element);
        std::string d = GetPathSVGString(p);
        if (!d.empty()) {
          out.openTag("path");
          out.addAttr("d", d);
          applySVGFill(out, trim ? nullptr : fill);
          if (isContinuousTrim) {
            applySVGStroke(out, stroke, computeGeoPathLength(g));
            applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
          } else {
            applySVGStroke(out, stroke);
            applyTrimAttrs(out, trim);
          }
          out.closeTagSelfClosing();
        }
        break;
      }
      case NodeType::Polystar: {
        auto ps = static_cast<const Polystar*>(g.element);
        std::string d = BuildPolystarPath(ps);
        if (!d.empty()) {
          out.openTag("path");
          out.addAttr("d", d);
          applySVGFill(out, trim ? nullptr : fill);
          if (isContinuousTrim) {
            applySVGStroke(out, stroke, computeGeoPathLength(g));
            applyTrimAttrsContinuous(out, trim, pathLengths, totalPathLength, geoIdx);
          } else {
            applySVGStroke(out, stroke);
            applyTrimAttrs(out, trim);
          }
          out.closeTagSelfClosing();
        }
        break;
      }
      default:
        break;
    }
  }
  if (needStrokeClip) {
    out.closeTag();  // </g> stroke clip group
  }
  out.closeTag();  // </svg>
}

void HTMLWriter::renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                           bool hasMerge, MergePathMode mergeMode) {
  if (geos.empty()) {
    return;
  }
  BlendMode painterBlend = BlendMode::Normal;
  if (fill && fill->blendMode != BlendMode::Normal) {
    painterBlend = fill->blendMode;
  } else if (stroke && stroke->blendMode != BlendMode::Normal) {
    painterBlend = stroke->blendMode;
  }
  if (hasMerge) {
    std::string mergedPath;
    std::string fillRule = "nonzero";
    bool needClipPath = (mergeMode == MergePathMode::Intersect);

    if (needClipPath) {
      float pad = 0.0f;
      if (stroke) {
        pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
      }
      float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
      for (auto& g : geos) {
        float gx = 0, gy = 0, gw = 0, gh = 0;
        switch (g.type) {
          case NodeType::Rectangle: {
            auto r = static_cast<const Rectangle*>(g.element);
            gx = r->position.x - r->size.width / 2;
            gy = r->position.y - r->size.height / 2;
            gw = r->size.width;
            gh = r->size.height;
            break;
          }
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            gx = e->position.x - e->size.width / 2;
            gy = e->position.y - e->size.height / 2;
            gw = e->size.width;
            gh = e->size.height;
            break;
          }
          case NodeType::Path: {
            auto p = static_cast<const Path*>(g.element);
            if (p->data && !p->data->isEmpty()) {
              Rect b = p->data->getBounds();
              gx = b.x;
              gy = b.y;
              gw = b.width;
              gh = b.height;
            }
            break;
          }
          case NodeType::Polystar: {
            auto ps = static_cast<const Polystar*>(g.element);
            float r = std::max(ps->outerRadius, ps->innerRadius);
            gx = ps->position.x - r;
            gy = ps->position.y - r;
            gw = r * 2;
            gh = r * 2;
            break;
          }
          default:
            break;
        }
        x0 = std::min(x0, gx - pad);
        y0 = std::min(y0, gy - pad);
        x1 = std::max(x1, gx + gw + pad);
        y1 = std::max(y1, gy + gh + pad);
      }
      float sw = x1 - x0;
      float sh = y1 - y0;
      if (sw <= 0 || sh <= 0) {
        return;
      }
      std::string svgStyle =
          "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
      if (painterBlend != BlendMode::Normal) {
        auto blendStr = BlendModeToMixBlendMode(painterBlend);
        if (blendStr) {
          svgStyle += ";mix-blend-mode:";
          svgStyle += blendStr;
        }
      }
      if (alpha < 1.0f) {
        svgStyle += ";opacity:" + FloatToString(alpha);
      }
      out.openTag("svg");
      out.addAttr("xmlns", "http://www.w3.org/2000/svg");
      out.addAttr("width", FloatToString(sw));
      out.addAttr("height", FloatToString(sh));
      out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                                 " " + FloatToString(sh));
      out.addAttr("style", svgStyle);
      out.closeTagStart();

      std::vector<std::string> clipIds = {};
      if (geos.size() > 1) {
        out.openTag("defs");
        out.closeTagStart();
        for (size_t i = 1; i < geos.size(); i++) {
          std::string clipId = _ctx->nextId("mpclip");
          clipIds.push_back(clipId);
          out.openTag("clipPath");
          out.addAttr("id", clipId);
          out.closeTagStart();
          auto& g = geos[i];
          std::string pathD;
          if (!g.modifiedPathData.empty()) {
            pathD = g.modifiedPathData;
          } else {
            switch (g.type) {
              case NodeType::Rectangle:
                pathD = RectToPathData(static_cast<const Rectangle*>(g.element));
                break;
              case NodeType::Ellipse:
                pathD = EllipseToPathData(static_cast<const Ellipse*>(g.element));
                break;
              case NodeType::Path:
                pathD = GetPathSVGString(static_cast<const Path*>(g.element));
                break;
              case NodeType::Polystar:
                pathD = BuildPolystarPath(static_cast<const Polystar*>(g.element));
                break;
              default:
                break;
            }
          }
          if (!pathD.empty()) {
            out.openTag("path");
            out.addAttr("d", pathD);
            out.closeTagSelfClosing();
          }
          out.closeTag();  // </clipPath>
        }
        out.closeTag();  // </defs>
      }

      auto& firstGeo = geos[0];
      std::string firstPathD;
      if (!firstGeo.modifiedPathData.empty()) {
        firstPathD = firstGeo.modifiedPathData;
      } else {
        switch (firstGeo.type) {
          case NodeType::Rectangle:
            firstPathD = RectToPathData(static_cast<const Rectangle*>(firstGeo.element));
            break;
          case NodeType::Ellipse:
            firstPathD = EllipseToPathData(static_cast<const Ellipse*>(firstGeo.element));
            break;
          case NodeType::Path:
            firstPathD = GetPathSVGString(static_cast<const Path*>(firstGeo.element));
            break;
          case NodeType::Polystar:
            firstPathD = BuildPolystarPath(static_cast<const Polystar*>(firstGeo.element));
            break;
          default:
            break;
        }
      }
      if (!firstPathD.empty()) {
        for (size_t i = clipIds.size(); i >= 1; i--) {
          out.openTag("g");
          out.addAttr("clip-path", "url(#" + clipIds[i - 1] + ")");
          out.closeTagStart();
        }
        out.openTag("path");
        out.addAttr("d", firstPathD);
        applySVGFill(out, fill);
        applySVGStroke(out, stroke);
        out.closeTagSelfClosing();
        for (size_t i = 1; i < geos.size(); i++) {
          out.closeTag();
        }
      }
      out.closeTag();  // </svg>
      return;
    }

    for (size_t i = 0; i < geos.size(); i++) {
      auto& g = geos[i];
      PathData pathData = PathDataFromSVGString("");
      if (!g.modifiedPathData.empty()) {
        pathData = PathDataFromSVGString(g.modifiedPathData);
      } else {
        pathData = GeoToPathData(g.element, g.type);
      }
      bool needReverse = false;
      switch (mergeMode) {
        case MergePathMode::Union:
          if (!IsPathClockwise(pathData)) {
            needReverse = true;
          }
          fillRule = "nonzero";
          break;
        case MergePathMode::Xor:
          fillRule = "evenodd";
          break;
        case MergePathMode::Difference:
          if (i == 0) {
            if (!IsPathClockwise(pathData)) {
              needReverse = true;
            }
          } else {
            if (IsPathClockwise(pathData)) {
              needReverse = true;
            }
          }
          fillRule = "nonzero";
          break;
        case MergePathMode::Append:
        default:
          break;
      }
      if (needReverse) {
        pathData = ReversePathData(pathData);
      }
      mergedPath += PathDataToSVGString(pathData);
    }
    if (mergedPath.empty()) {
      return;
    }
    float pad = 0.0f;
    if (stroke) {
      pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
    }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (auto& g : geos) {
      float gx = 0, gy = 0, gw = 0, gh = 0;
      switch (g.type) {
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          gx = r->position.x - r->size.width / 2;
          gy = r->position.y - r->size.height / 2;
          gw = r->size.width;
          gh = r->size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          gx = e->position.x - e->size.width / 2;
          gy = e->position.y - e->size.height / 2;
          gw = e->size.width;
          gh = e->size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            gx = b.x;
            gy = b.y;
            gw = b.width;
            gh = b.height;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          float r = std::max(ps->outerRadius, ps->innerRadius);
          gx = ps->position.x - r;
          gy = ps->position.y - r;
          gw = r * 2;
          gh = r * 2;
          break;
        }
        default:
          break;
      }
      x0 = std::min(x0, gx - pad);
      y0 = std::min(y0, gy - pad);
      x1 = std::max(x1, gx + gw + pad);
      y1 = std::max(y1, gy + gh + pad);
    }
    float sw = x1 - x0;
    float sh = y1 - y0;
    if (sw <= 0 || sh <= 0) {
      return;
    }
    std::string svgStyle =
        "position:absolute;left:" + FloatToString(x0) + "px;top:" + FloatToString(y0) + "px";
    if (painterBlend != BlendMode::Normal) {
      auto blendStr = BlendModeToMixBlendMode(painterBlend);
      if (blendStr) {
        svgStyle += ";mix-blend-mode:";
        svgStyle += blendStr;
      }
    }
    if (alpha < 1.0f) {
      svgStyle += ";opacity:" + FloatToString(alpha);
    }
    out.openTag("svg");
    out.addAttr("xmlns", "http://www.w3.org/2000/svg");
    out.addAttr("width", FloatToString(sw));
    out.addAttr("height", FloatToString(sh));
    out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(sw) +
                               " " + FloatToString(sh));
    out.addAttr("style", svgStyle);
    out.closeTagStart();
    out.openTag("path");
    out.addAttr("d", mergedPath);
    if (fillRule != "nonzero") {
      out.addAttr("fill-rule", fillRule);
    }
    applySVGFill(out, fill);
    applySVGStroke(out, stroke);
    out.closeTagSelfClosing();
    out.closeTag();
    return;
  }
  if (canCSS(geos, fill, stroke, hasTrim, false)) {
    renderCSSDiv(out, geos[0], fill, alpha, painterBlend);
  } else {
    renderSVG(out, geos, fill, stroke, alpha, painterBlend, trim, mergeMode);
  }
}

}  // namespace pagx
