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
#include <filesystem>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLStaticImageRenderer.h"
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

// Returns true when this ImagePattern's tile semantics and matrix form can be faithfully
// expressed by CSS background-{image,repeat,size,position} without rasterization. Mirror has no
// CSS equivalent and Clamp's edge-pixel extension is not expressible either, so both must continue
// through the PNG path. Rotated or skewed matrices also fall back to PNG. Pure scale, pure
// translation, and identity all map cleanly onto CSS.
static bool imagePatternCSSable(const ImagePattern* p) {
  bool xOk = (p->tileModeX == TileMode::Repeat || p->tileModeX == TileMode::Decal);
  bool yOk = (p->tileModeY == TileMode::Repeat || p->tileModeY == TileMode::Decal);
  if (!xOk || !yOk) {
    return false;
  }
  if (!FloatNearlyZero(p->matrix.b) || !FloatNearlyZero(p->matrix.c)) {
    return false;
  }
  return true;
}

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
  auto psCenter = ps->renderPosition();
  float centerX = psCenter.x;
  float centerY = psCenter.y;
  float outerR = ps->renderOuterRadius();
  float innerR = ps->renderInnerRadius();
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

    float firstDx = outerR * std::cos(currentAngle);
    float firstDy = outerR * std::sin(currentAngle);
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
        float radius = outerFlag ? outerR : innerR;
        if (i == decimalIndex || i == decimalIndex + 1) {
          radius = innerR + decimalPart * (radius - innerR);
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

  float firstDx = outerR * std::cos(currentAngle);
  float firstDy = outerR * std::sin(currentAngle);
  float lastDx = firstDx;
  float lastDy = firstDy;

  std::string d;
  d.reserve(static_cast<size_t>(numPoints) * 50);
  d += "M" + FloatToString(lastDx + centerX) + "," + FloatToString(lastDy + centerY);

  float angleDelta = angleStep * direction;
  for (int i = 1; i < numPoints; i++) {
    currentAngle += angleDelta;
    float dx = outerR * std::cos(currentAngle);
    float dy = outerR * std::sin(currentAngle);
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

void ApplyRoundCorner(const PathData& pathData, float radius, PathData& output) {
  if (radius <= 0 || pathData.isEmpty()) {
    output = pathData;
    return;
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

  output = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      output.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        output.close();
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
          output.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        if (vi < n) {
          auto& seg = contour.segments[vi];
          if (seg.verb == PathVerb::Line) {
            output.lineTo(seg.endPoint.x, seg.endPoint.y);
          } else if (seg.verb == PathVerb::Quad) {
            output.quadTo(seg.ctrl1.x, seg.ctrl1.y, seg.endPoint.x, seg.endPoint.y);
          } else if (seg.verb == PathVerb::Cubic) {
            output.cubicTo(seg.ctrl1.x, seg.ctrl1.y, seg.ctrl2.x, seg.ctrl2.y, seg.endPoint.x,
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
          output.moveTo(vertex.x, vertex.y);
          firstOutput = false;
        }
        if (vi < n) {
          output.lineTo(contour.segments[vi].endPoint.x, contour.segments[vi].endPoint.y);
        }
        continue;
      }

      // Compute the angle between the two edge directions emanating from the vertex.
      // Skia reverses the incoming tangent so both vectors point away from the vertex:
      // dir1 = normalized direction from vertex toward prevVertex (reversed incoming edge).
      // dir2 = normalized direction from vertex toward nextVertex (outgoing edge).
      float ndx1 = -dx1 / len1, ndy1 = -dy1 / len1;
      float ndx2 = dx2 / len2, ndy2 = dy2 / len2;
      float dot = ndx1 * ndx2 + ndy1 * ndy2;
      dot = std::max(-1.0f, std::min(1.0f, dot));
      float halfAngle = std::acos(dot) / 2.0f;
      float sinHalf = std::sin(halfAngle);
      float cosHalf = std::cos(halfAngle);
      float tanHalf = (sinHalf > 1e-6f) ? (sinHalf / cosHalf) : 1e9f;

      // tangentDistance = radius / tan(halfAngle): distance from vertex along each edge to the
      // arc tangent point. Clamp so it doesn't exceed half the edge length.
      float tangentDist = radius / tanHalf;
      tangentDist = std::min({tangentDist, len1 / 2.0f, len2 / 2.0f});
      float t1 = tangentDist / len1;
      float t2 = tangentDist / len2;
      Point p1 = {vertex.x - dx1 * t1, vertex.y - dy1 * t1};
      Point p2 = {vertex.x + dx2 * t2, vertex.y + dy2 * t2};

      // Angle-adaptive bezier handle length matching Skia's ArcCubicBezierHandleLength:
      // 1. Compute the chord between the two tangent points.
      // 2. Derive the circle radius from chord and the angle between the tangent vectors.
      // 3. handleLength = (4*(1-cos(α/2)))/(3*sin(α/2)) * circleRadius
      // The tangent vectors at p1 (toward vertex) and p2 (away from vertex) subtend angle
      // = π - interiorAngle = arcAngle, which is also the arc's sweep angle.
      float arcAngle = static_cast<float>(M_PI) - 2.0f * halfAngle;
      float sinHalfArc = std::sin(arcAngle / 2.0f);
      float cosHalfArc = std::cos(arcAngle / 2.0f);
      float handleFraction = 0;
      if (std::abs(sinHalfArc) > 1e-6f) {
        float chordX = p2.x - p1.x;
        float chordY = p2.y - p1.y;
        float chord = std::sqrt(chordX * chordX + chordY * chordY);
        float circleRadius = (chord / 2.0f) / sinHalfArc;
        handleFraction = (4.0f * (1.0f - cosHalfArc)) / (3.0f * sinHalfArc) * circleRadius;
      }
      Point cp1 = {p1.x - ndx1 * handleFraction, p1.y - ndy1 * handleFraction};
      Point cp2 = {p2.x - ndx2 * handleFraction, p2.y - ndy2 * handleFraction};

      if (firstOutput) {
        output.moveTo(p1.x, p1.y);
        firstOutput = false;
      } else {
        output.lineTo(p1.x, p1.y);
      }
      output.cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, p2.x, p2.y);
      if (vi == n - 1 && !contour.closed) {
        output.lineTo(contour.segments[vi].endPoint.x, contour.segments[vi].endPoint.y);
      }
    }
    if (contour.closed) {
      output.close();
    }
  }
}

void ReversePathData(const PathData& pathData, PathData& output) {
  if (pathData.isEmpty()) {
    output = pathData;
    return;
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

  output = PathDataFromSVGString("");
  for (auto& contour : contours) {
    if (contour.segments.empty()) {
      output.moveTo(contour.startPoint.x, contour.startPoint.y);
      if (contour.closed) {
        output.close();
      }
      continue;
    }
    auto& lastSeg = contour.segments.back();
    Point newStart = lastSeg.points.back();
    output.moveTo(newStart.x, newStart.y);

    for (int i = static_cast<int>(contour.segments.size()) - 1; i >= 0; i--) {
      auto& seg = contour.segments[static_cast<size_t>(i)];
      Point segStart = (i == 0) ? contour.startPoint
                                : contour.segments[static_cast<size_t>(i - 1)].points.back();
      switch (seg.verb) {
        case PathVerb::Line:
          output.lineTo(segStart.x, segStart.y);
          break;
        case PathVerb::Quad:
          output.quadTo(seg.points[0].x, seg.points[0].y, segStart.x, segStart.y);
          break;
        case PathVerb::Cubic:
          output.cubicTo(seg.points[1].x, seg.points[1].y, seg.points[0].x, seg.points[0].y,
                         segStart.x, segStart.y);
          break;
        default:
          break;
      }
    }
    if (contour.closed) {
      output.close();
    }
  }
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

void GeoToPathData(const Element* element, NodeType type, PathData& pathData) {
  pathData = PathDataFromSVGString("");
  switch (type) {
    case NodeType::Rectangle: {
      auto r = static_cast<const Rectangle*>(element);
      auto pos = r->renderPosition();
      auto size = r->renderSize();
      float x = pos.x - size.width / 2;
      float y = pos.y - size.height / 2;
      float w = size.width;
      float h = size.height;
      if (r->roundness <= 0) {
        // Match native (tgfx) start point: top-right corner, CW direction.
        pathData.moveTo(x + w, y);
        if (r->reversed) {
          pathData.lineTo(x, y);
          pathData.lineTo(x, y + h);
          pathData.lineTo(x + w, y + h);
        } else {
          pathData.lineTo(x + w, y + h);
          pathData.lineTo(x, y + h);
          pathData.lineTo(x, y);
        }
        pathData.close();
      } else {
        float rn = std::min(r->roundness, std::min(w / 2, h / 2));
        // Match native (tgfx) RRect startIndex=2: start at right edge top-rn, CW direction.
        pathData.moveTo(x + w, y + rn);
        if (r->reversed) {
          pathData.cubicTo(x + w, y + rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa), y,
                           x + w - rn, y);
          pathData.lineTo(x + rn, y);
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y, x, y + rn * (1 - kBezierKappa), x,
                           y + rn);
          pathData.lineTo(x, y + h - rn);
          pathData.cubicTo(x, y + h - rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y + h,
                           x + rn, y + h);
          pathData.lineTo(x + w - rn, y + h);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y + h, x + w,
                           y + h - rn * (1 - kBezierKappa), x + w, y + h - rn);
        } else {
          pathData.lineTo(x + w, y + h - rn);
          pathData.cubicTo(x + w, y + h - rn * (1 - kBezierKappa), x + w - rn * (1 - kBezierKappa),
                           y + h, x + w - rn, y + h);
          pathData.lineTo(x + rn, y + h);
          pathData.cubicTo(x + rn * (1 - kBezierKappa), y + h, x, y + h - rn * (1 - kBezierKappa),
                           x, y + h - rn);
          pathData.lineTo(x, y + rn);
          pathData.cubicTo(x, y + rn * (1 - kBezierKappa), x + rn * (1 - kBezierKappa), y, x + rn,
                           y);
          pathData.lineTo(x + w - rn, y);
          pathData.cubicTo(x + w - rn * (1 - kBezierKappa), y, x + w, y + rn * (1 - kBezierKappa),
                           x + w, y + rn);
        }
        pathData.close();
      }
      break;
    }
    case NodeType::Ellipse: {
      auto e = static_cast<const Ellipse*>(element);
      auto pos = e->renderPosition();
      auto size = e->renderSize();
      float cx = pos.x;
      float cy = pos.y;
      float rx = size.width / 2;
      float ry = size.height / 2;
      float kx = rx * kBezierKappa;
      float ky = ry * kBezierKappa;
      // Match native (tgfx/Skia) start point: top center (12 o'clock), CW direction.
      pathData.moveTo(cx, cy - ry);
      if (e->reversed) {
        pathData.cubicTo(cx - kx, cy - ry, cx - rx, cy - ky, cx - rx, cy);
        pathData.cubicTo(cx - rx, cy + ky, cx - kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx + kx, cy + ry, cx + rx, cy + ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy - ky, cx + kx, cy - ry, cx, cy - ry);
      } else {
        pathData.cubicTo(cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy);
        pathData.cubicTo(cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry);
        pathData.cubicTo(cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy);
        pathData.cubicTo(cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry);
      }
      pathData.close();
      break;
    }
    case NodeType::Path: {
      auto p = static_cast<const Path*>(element);
      if (p->data) {
        if (p->reversed) {
          ReversePathData(*p->data, pathData);
        } else {
          pathData = *p->data;
        }
        float scale = p->renderScale();
        auto pos = p->renderPosition();
        if (scale != 1.0f || pos.x != 0.0f || pos.y != 0.0f) {
          Matrix m = Matrix::Scale(scale, scale);
          m.tx = pos.x;
          m.ty = pos.y;
          pathData.transform(m);
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
}

std::string RectToPathData(const Rectangle* r) {
  auto pos = r->renderPosition();
  auto size = r->renderSize();
  float x = pos.x - size.width / 2;
  float y = pos.y - size.height / 2;
  float w = size.width;
  float h = size.height;
  // tgfx starts its rectangle path at the top-right corner (SkRRect startIndex=2 degrades to
  // SkPath::addRect with startIndex=1 for zero-roundness) and walks clockwise in screen space
  // (y-axis down): top-right -> bottom-right -> bottom-left -> top-left -> close. For non-dashed
  // fills/strokes this is invisible, but `stroke-dasharray` relies on absolute path length from
  // the starting point, so we must match tgfx's start corner *and* traversal direction verbatim
  // or dashes land at different phases on each edge (breaks stroke_dash_offset_cycle).
  if (r->roundness <= 0) {
    if (r->reversed) {
      // Counter-clockwise in screen space from top-right:
      // (x+w,y) -> (x,y) -> (x,y+h) -> (x+w,y+h) -> close.
      return "M" + FloatToString(x + w) + "," + FloatToString(y) + "H" + FloatToString(x) + "V" +
             FloatToString(y + h) + "H" + FloatToString(x + w) + "Z";
    }
    // Clockwise in screen space from top-right:
    // (x+w,y) -> (x+w,y+h) -> (x,y+h) -> (x,y) -> close.
    return "M" + FloatToString(x + w) + "," + FloatToString(y) + "V" + FloatToString(y + h) + "H" +
           FloatToString(x) + "V" + FloatToString(y) + "Z";
  }
  float rn = std::min(r->roundness, std::min(w / 2, h / 2));
  std::string d;
  if (r->reversed) {
    // Counter-clockwise from top-right, starting just past the top-right round corner.
    d += "M" + FloatToString(x + w) + "," + FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w - rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x) + "," +
         FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + rn) +
         "," + FloatToString(y);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 0 " + FloatToString(x + w) +
         "," + FloatToString(y + rn);
    d += "Z";
  } else {
    // Clockwise from top-right, starting just past the top-right round corner (SkRRect
    // startIndex=2 puts the move-to at the corner's bottom tangent and walks along the right
    // edge downward first).
    d += "M" + FloatToString(x + w) + "," + FloatToString(y + rn);
    d += "V" + FloatToString(y + h - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w - rn) +
         "," + FloatToString(y + h);
    d += "H" + FloatToString(x + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x) + "," +
         FloatToString(y + h - rn);
    d += "V" + FloatToString(y + rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + rn) +
         "," + FloatToString(y);
    d += "H" + FloatToString(x + w - rn);
    d += "A" + FloatToString(rn) + "," + FloatToString(rn) + " 0 0 1 " + FloatToString(x + w) +
         "," + FloatToString(y + rn);
    d += "Z";
  }
  return d;
}

std::string EllipseToPathData(const Ellipse* e) {
  auto pos = e->renderPosition();
  auto size = e->renderSize();
  float cx = pos.x;
  float cy = pos.y;
  float rx = size.width / 2;
  float ry = size.height / 2;
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
  float scale = path->renderScale();
  auto pos = path->renderPosition();
  bool needsTransform = scale != 1.0f || pos.x != 0.0f || pos.y != 0.0f;
  if (!needsTransform) {
    if (path->reversed) {
      return ReversePathDataToSVGString(*path->data);
    }
    return PathDataToSVGString(*path->data);
  }
  PathData transformed = PathDataFromSVGString("");
  transformed = *path->data;
  Matrix m = Matrix::Scale(scale, scale);
  m.tx = pos.x;
  m.ty = pos.y;
  transformed.transform(m);
  if (path->reversed) {
    return ReversePathDataToSVGString(transformed);
  }
  return PathDataToSVGString(transformed);
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
  std::string f = fill->color ? colorToSVGFill(fill->color, &alpha) : "#000000";
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
  std::string s = stroke->color ? colorToSVGFill(stroke->color, &alpha) : "#000000";
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
    // Both SVG `stroke-dashoffset` (SVG 1.1 §11.4) and tgfx's Skia `SkDashPathEffect` phase
    // use the same sign convention: a positive value advances the dash pattern forward so
    // the path start lies `offset` units into the pattern. Pass the value through verbatim.
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
    auto pos = rect->renderPosition();
    auto size = rect->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
    roundness = std::min(rect->roundness, std::min(w / 2, h / 2));
  } else {
    auto el = static_cast<const Ellipse*>(geo.element);
    auto pos = el->renderPosition();
    auto size = el->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
    isEllipse = true;
  }

  // Use CSS inset:0 when the element fills its parent via constraints (left=0 right=0 top=0
  // bottom=0), so it adapts to dynamic flex sizing instead of using fixed pixel values.
  bool fillsParent = false;
  if (isRect) {
    auto* r = static_cast<const Rectangle*>(geo.element);
    fillsParent = !std::isnan(r->left) && FloatNearlyZero(r->left) && !std::isnan(r->right) &&
                  FloatNearlyZero(r->right) && !std::isnan(r->top) && FloatNearlyZero(r->top) &&
                  !std::isnan(r->bottom) && FloatNearlyZero(r->bottom);
  } else {
    auto* e = static_cast<const Ellipse*>(geo.element);
    fillsParent = !std::isnan(e->left) && FloatNearlyZero(e->left) && !std::isnan(e->right) &&
                  FloatNearlyZero(e->right) && !std::isnan(e->top) && FloatNearlyZero(e->top) &&
                  !std::isnan(e->bottom) && FloatNearlyZero(e->bottom);
  }

  std::string style;
  if (fillsParent) {
    style = "position:absolute;inset:0";
  } else {
    style = "position:absolute;left:" + FloatToString(left) + "px;top:" + FloatToString(top) +
            "px;width:" + FloatToString(w) + "px;height:" + FloatToString(h) + "px";
  }

  if (isEllipse) {
    style += ";border-radius:50%";
  } else if (roundness > 0) {
    style += ";border-radius:" + FloatToString(roundness) + "px";
  }

  if (fill) {
    if (fill->color) {
      float ca = 1.0f;
      std::string css = colorToCSS(fill->color, &ca, left, top, w, h);
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
        // The dispatcher in paintGeos gates Mirror/Clamp to the PNG path, so we should only see
        // Repeat and Decal here. Map defensively: anything that is not Repeat becomes no-repeat
        // rather than silently aliasing Mirror onto repeat (which was a latent bug before the
        // gate fix because the code path was unreachable in practice).
        auto repeatFor = [](TileMode m) -> const char* {
          return m == TileMode::Repeat ? "repeat" : "no-repeat";
        };
        std::string repeatX = repeatFor(p->tileModeX);
        std::string repeatY = repeatFor(p->tileModeY);
        if (repeatX == repeatY) {
          style += ";background-repeat:" + repeatX;
        } else {
          style += ";background-repeat:" + repeatX + " " + repeatY;
        }
        // When any axis tiles, the tile unit on screen must equal (sx*imgW, sy*imgH) CSS pixels
        // to mirror tgfx's pattern->setMatrix() inverse texture-coordinate transform. That needs
        // the image's native dimensions; emit background-size in absolute pixels when we can
        // decode them, otherwise leave it off and let the browser use the natural image size
        // (which is correct for identity matrices and a graceful degradation otherwise).
        bool anyRepeat = (p->tileModeX == TileMode::Repeat) || (p->tileModeY == TileMode::Repeat);
        if (anyRepeat) {
          auto size = GetImageNativeSize(p->image);
          if (size.first > 0 && size.second > 0) {
            float sx = std::sqrt(p->matrix.a * p->matrix.a + p->matrix.b * p->matrix.b);
            float sy = std::sqrt(p->matrix.c * p->matrix.c + p->matrix.d * p->matrix.d);
            float tileW = sx * static_cast<float>(size.first);
            float tileH = sy * static_cast<float>(size.second);
            style +=
                ";background-size:" + FloatToString(tileW) + "px " + FloatToString(tileH) + "px";
          }
        }
        if (!FloatNearlyZero(p->matrix.tx) || !FloatNearlyZero(p->matrix.ty)) {
          style += ";background-position:" + FloatToString(p->matrix.tx) + "px " +
                   FloatToString(p->matrix.ty) + "px";
        }
        // ImagePattern.mipmapMode has no CSS equivalent; browsers pick internal mipmap policy.
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
    } else {
      style += ";background-color:" + ColorToRGBA({0, 0, 0, 255}, fill->alpha);
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
  // When stroke uses ConicGradient, browsers cannot render it via SVG pattern+foreignObject.
  // Instead, render fill normally (without stroke), then append a separate SVG that uses
  // an inline <mask> (white stroke shapes) + <foreignObject> (CSS conic-gradient background).
  // foreignObject as a direct SVG child with mask= works in browsers, unlike foreignObject
  // inside <pattern> which is not supported for stroke/fill rendering.
  bool conicStroke =
      stroke && stroke->color && stroke->color->nodeType() == NodeType::ConicGradient;
  if (conicStroke) {
    // Render fill-only pass first (reuse standard SVG path with no stroke).
    if (fill) {
      renderSVG(out, geos, fill, nullptr, alpha, painterBlend, trim, MergePathMode::Append);
    }
    // Compute bounding box of all geometries including stroke padding.
    float pad = (stroke->align != StrokeAlign::Center) ? stroke->width * 2.0f : stroke->width;
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
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            auto pos = e->renderPosition();
            auto size = e->renderSize();
            gx = pos.x - size.width / 2;
            gy = pos.y - size.height / 2;
            gw = size.width;
            gh = size.height;
            break;
          }
          case NodeType::Rectangle: {
            auto r = static_cast<const Rectangle*>(g.element);
            auto pos = r->renderPosition();
            auto size = r->renderSize();
            gx = pos.x - size.width / 2;
            gy = pos.y - size.height / 2;
            gw = size.width;
            gh = size.height;
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
    float bw = x1 - x0;
    float bh = y1 - y0;
    if (bw <= 0 || bh <= 0) {
      return;
    }
    // Build a new SVG containing: <defs><mask>(stroke shapes)</mask></defs>
    //   + <foreignObject mask="url(#maskId)">(CSS conic-gradient div)</foreignObject>
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
    out.addAttr("width", FloatToString(bw));
    out.addAttr("height", FloatToString(bh));
    out.addAttr("viewBox", FloatToString(x0) + " " + FloatToString(y0) + " " + FloatToString(bw) +
                               " " + FloatToString(bh));
    out.addAttr("style", svgStyle);
    out.closeTagStart();
    // Inline <mask> with stroke shapes rendered in white.
    std::string maskId = _ctx->nextId("cmask");
    out.openTag("defs");
    out.closeTagStart();
    out.openTag("mask");
    out.addAttr("id", maskId);
    out.addAttr("maskUnits", "userSpaceOnUse");
    out.addAttr("x", FloatToString(x0));
    out.addAttr("y", FloatToString(y0));
    out.addAttr("width", FloatToString(bw));
    out.addAttr("height", FloatToString(bh));
    out.closeTagStart();
    for (auto& g : geos) {
      if (!g.modifiedPathData.empty()) {
        out.openTag("path");
        out.addAttr("d", g.modifiedPathData);
        out.addAttr("fill", "none");
        out.addAttr("stroke", "white");
        out.addAttr("stroke-width", FloatToString(stroke->width));
        if (stroke->cap == LineCap::Round) {
          out.addAttr("stroke-linecap", "round");
        }
        out.closeTagSelfClosing();
        continue;
      }
      switch (g.type) {
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          auto pos = e->renderPosition();
          auto size = e->renderSize();
          out.openTag("ellipse");
          out.addAttr("cx", FloatToString(pos.x));
          out.addAttr("cy", FloatToString(pos.y));
          out.addAttr("rx", FloatToString(size.width / 2));
          out.addAttr("ry", FloatToString(size.height / 2));
          out.addAttr("fill", "none");
          out.addAttr("stroke", "white");
          out.addAttr("stroke-width", FloatToString(stroke->width));
          if (stroke->cap == LineCap::Round) {
            out.addAttr("stroke-linecap", "round");
          }
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Rectangle: {
          auto r = static_cast<const Rectangle*>(g.element);
          auto pos = r->renderPosition();
          auto size = r->renderSize();
          float rx = pos.x - size.width / 2;
          float ry = pos.y - size.height / 2;
          out.openTag("rect");
          out.addAttr("x", FloatToString(rx));
          out.addAttr("y", FloatToString(ry));
          out.addAttr("width", FloatToString(size.width));
          out.addAttr("height", FloatToString(size.height));
          if (r->roundness > 0) {
            out.addAttr("rx", FloatToString(r->roundness));
          }
          out.addAttr("fill", "none");
          out.addAttr("stroke", "white");
          out.addAttr("stroke-width", FloatToString(stroke->width));
          out.closeTagSelfClosing();
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          std::string d = GetPathSVGString(p);
          if (!d.empty()) {
            out.openTag("path");
            out.addAttr("d", d);
            out.addAttr("fill", "none");
            out.addAttr("stroke", "white");
            out.addAttr("stroke-width", FloatToString(stroke->width));
            if (stroke->cap == LineCap::Round) {
              out.addAttr("stroke-linecap", "round");
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
            out.addAttr("fill", "none");
            out.addAttr("stroke", "white");
            out.addAttr("stroke-width", FloatToString(stroke->width));
            if (stroke->cap == LineCap::Round) {
              out.addAttr("stroke-linecap", "round");
            }
            out.closeTagSelfClosing();
          }
          break;
        }
        default:
          break;
      }
    }
    out.closeTag();  // </mask>
    out.closeTag();  // </defs>
    // foreignObject covering the viewBox, masked to stroke shape, with CSS conic-gradient.
    auto css = colorToCSS(stroke->color, nullptr);
    if (!css.empty()) {
      out.openTag("foreignObject");
      out.addAttr("x", FloatToString(x0));
      out.addAttr("y", FloatToString(y0));
      out.addAttr("width", FloatToString(bw));
      out.addAttr("height", FloatToString(bh));
      out.addAttr("mask", "url(#" + maskId + ")");
      out.closeTagStart();
      out.openTag("div");
      out.addAttr("xmlns", "http://www.w3.org/1999/xhtml");
      out.addAttr("style", "width:100%;height:100%;background:" + css);
      out.closeTagSelfClosing();
      out.closeTag();  // </foreignObject>
    }
    out.closeTag();  // </svg>
    return;
  }
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
          auto pos = r->renderPosition();
          auto size = r->renderSize();
          gx = pos.x - size.width / 2;
          gy = pos.y - size.height / 2;
          gw = size.width;
          gh = size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          auto pos = e->renderPosition();
          auto size = e->renderSize();
          gx = pos.x - size.width / 2;
          gy = pos.y - size.height / 2;
          gw = size.width;
          gh = size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            float scale = p->renderScale();
            auto ppos = p->renderPosition();
            gx = ppos.x + b.x * scale;
            gy = ppos.y + b.y * scale;
            gw = b.width * scale;
            gh = b.height * scale;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          auto ppos = ps->renderPosition();
          float r = std::max(ps->renderOuterRadius(), ps->renderInnerRadius());
          gx = ppos.x - r;
          gy = ppos.y - r;
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
    out.closeTagStart();
    if (stroke->align == StrokeAlign::Outside) {
      // For outside stroke, build a single <path> with the outer rect and inner shape combined,
      // using fill-rule="evenodd" to clip to only the exterior region. Using separate <rect>
      // elements with clip-rule does not work because clip-rule only applies to <path> elements.
      std::string combinedD;
      // Outer rectangle path
      combinedD += "M" + FloatToString(x0) + "," + FloatToString(y0);
      combinedD += "H" + FloatToString(x0 + sw);
      combinedD += "V" + FloatToString(y0 + sh);
      combinedD += "H" + FloatToString(x0);
      combinedD += "Z";
      // Inner shape path(s)
      for (auto& g : geos) {
        switch (g.type) {
          case NodeType::Rectangle: {
            auto r = static_cast<const Rectangle*>(g.element);
            combinedD += RectToPathData(r);
            break;
          }
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            combinedD += EllipseToPathData(e);
            break;
          }
          case NodeType::Path: {
            auto p = static_cast<const Path*>(g.element);
            std::string clipD = GetPathSVGString(p);
            if (!clipD.empty()) {
              combinedD += clipD;
            }
            break;
          }
          case NodeType::Polystar: {
            auto ps = static_cast<const Polystar*>(g.element);
            std::string clipD = BuildPolystarPath(ps);
            if (!clipD.empty()) {
              combinedD += clipD;
            }
            break;
          }
          default:
            break;
        }
      }
      out.openTag("path");
      out.addAttr("d", combinedD);
      out.addAttr("fill-rule", "evenodd");
      out.addAttr("clip-rule", "evenodd");
      out.closeTagSelfClosing();
    } else {
      // Inside stroke: clip to shape interior
      for (auto& g : geos) {
        switch (g.type) {
          case NodeType::Rectangle: {
            auto r = static_cast<const Rectangle*>(g.element);
            auto pos = r->renderPosition();
            auto size = r->renderSize();
            float cx = pos.x - size.width / 2;
            float cy = pos.y - size.height / 2;
            out.openTag("rect");
            if (!FloatNearlyZero(cx)) {
              out.addAttr("x", FloatToString(cx));
            }
            if (!FloatNearlyZero(cy)) {
              out.addAttr("y", FloatToString(cy));
            }
            out.addAttr("width", FloatToString(size.width));
            out.addAttr("height", FloatToString(size.height));
            if (r->roundness > 0) {
              out.addAttr("rx", FloatToString(r->roundness));
            }
            out.closeTagSelfClosing();
            break;
          }
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            auto pos = e->renderPosition();
            auto size = e->renderSize();
            out.openTag("ellipse");
            out.addAttr("cx", FloatToString(pos.x));
            out.addAttr("cy", FloatToString(pos.y));
            out.addAttr("rx", FloatToString(size.width / 2));
            out.addAttr("ry", FloatToString(size.height / 2));
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
    if (isContinuousTrim && !isContinuousTrimVisible(trim, pathLengths, totalPathLength, geoIdx)) {
      continue;
    }
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
        // `<rect>` doesn't let us control the path-start corner, but `stroke-dasharray`
        // phase depends on where the path begins. tgfx uses top-right as the start corner
        // (via SkRRect startIndex=2), so emit a `<path>` whenever a dashed stroke exists
        // and let RectToPathData produce the matching start.
        bool needsPathForDash = stroke && !stroke->dashes.empty();
        if (trim || needsPathForDash) {
          std::string d;
          if (needsPathForDash && !trim) {
            d = RectToPathData(r);
          } else {
            PathData pathData = PathDataFromSVGString("");
            GeoToPathData(g.element, g.type, pathData);
            d = PathDataToSVGString(pathData);
          }
          out.openTag("path");
          out.addAttr("d", d);
        } else {
          auto pos = r->renderPosition();
          auto size = r->renderSize();
          float x = pos.x - size.width / 2;
          float y = pos.y - size.height / 2;
          out.openTag("rect");
          if (!FloatNearlyZero(x)) {
            out.addAttr("x", FloatToString(x));
          }
          if (!FloatNearlyZero(y)) {
            out.addAttr("y", FloatToString(y));
          }
          out.addAttr("width", FloatToString(size.width));
          out.addAttr("height", FloatToString(size.height));
          if (r->roundness > 0) {
            out.addAttr("rx", FloatToString(r->roundness));
            out.addAttr("ry", FloatToString(r->roundness));
          }
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
        if (trim) {
          PathData pathData = PathDataFromSVGString("");
          GeoToPathData(g.element, g.type, pathData);
          std::string d = PathDataToSVGString(pathData);
          out.openTag("path");
          out.addAttr("d", d);
        } else {
          auto e = static_cast<const Ellipse*>(g.element);
          auto pos = e->renderPosition();
          auto size = e->renderSize();
          out.openTag("ellipse");
          out.addAttr("cx", FloatToString(pos.x));
          out.addAttr("cy", FloatToString(pos.y));
          out.addAttr("rx", FloatToString(size.width / 2));
          out.addAttr("ry", FloatToString(size.height / 2));
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
            auto pos = r->renderPosition();
            auto size = r->renderSize();
            gx = pos.x - size.width / 2;
            gy = pos.y - size.height / 2;
            gw = size.width;
            gh = size.height;
            break;
          }
          case NodeType::Ellipse: {
            auto e = static_cast<const Ellipse*>(g.element);
            auto pos = e->renderPosition();
            auto size = e->renderSize();
            gx = pos.x - size.width / 2;
            gy = pos.y - size.height / 2;
            gw = size.width;
            gh = size.height;
            break;
          }
          case NodeType::Path: {
            auto p = static_cast<const Path*>(g.element);
            if (p->data && !p->data->isEmpty()) {
              Rect b = p->data->getBounds();
              float scale = p->renderScale();
              auto ppos = p->renderPosition();
              gx = ppos.x + b.x * scale;
              gy = ppos.y + b.y * scale;
              gw = b.width * scale;
              gh = b.height * scale;
            }
            break;
          }
          case NodeType::Polystar: {
            auto ps = static_cast<const Polystar*>(g.element);
            auto ppos = ps->renderPosition();
            float r = std::max(ps->renderOuterRadius(), ps->renderInnerRadius());
            gx = ppos.x - r;
            gy = ppos.y - r;
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
        GeoToPathData(g.element, g.type, pathData);
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
        PathData reversed = PathDataFromSVGString("");
        ReversePathData(pathData, reversed);
        mergedPath += PathDataToSVGString(reversed);
      } else {
        mergedPath += PathDataToSVGString(pathData);
      }
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
          auto pos = r->renderPosition();
          auto size = r->renderSize();
          gx = pos.x - size.width / 2;
          gy = pos.y - size.height / 2;
          gw = size.width;
          gh = size.height;
          break;
        }
        case NodeType::Ellipse: {
          auto e = static_cast<const Ellipse*>(g.element);
          auto pos = e->renderPosition();
          auto size = e->renderSize();
          gx = pos.x - size.width / 2;
          gy = pos.y - size.height / 2;
          gw = size.width;
          gh = size.height;
          break;
        }
        case NodeType::Path: {
          auto p = static_cast<const Path*>(g.element);
          if (p->data && !p->data->isEmpty()) {
            Rect b = p->data->getBounds();
            float scale = p->renderScale();
            auto ppos = p->renderPosition();
            gx = ppos.x + b.x * scale;
            gy = ppos.y + b.y * scale;
            gw = b.width * scale;
            gh = b.height * scale;
          }
          break;
        }
        case NodeType::Polystar: {
          auto ps = static_cast<const Polystar*>(g.element);
          auto ppos = ps->renderPosition();
          float r = std::max(ps->renderOuterRadius(), ps->renderInnerRadius());
          gx = ppos.x - r;
          gy = ppos.y - r;
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
  if (fill && fill->color && fill->color->nodeType() == NodeType::DiamondGradient &&
      geos.size() == 1 && !stroke) {
    renderDiamondCanvas(out, geos[0], fill, alpha, painterBlend);
    return;
  }
  if (fill && fill->color && fill->color->nodeType() == NodeType::ImagePattern &&
      geos.size() == 1 && !stroke) {
    auto p = static_cast<const ImagePattern*>(fill->color);
    // Only patterns whose tile semantics have no CSS equivalent (Mirror, Clamp) or whose matrix
    // carries rotation/skew need the PNG rasterization path. Pure-scale repeat and all decal
    // variants fall through to the CSS branch below, which keeps the HTML DOM-inspectable and
    // avoids writing PNGs for simple tiled fills.
    if (p->image && !imagePatternCSSable(p)) {
      renderImagePatternCanvas(out, geos[0], fill, alpha, painterBlend);
      return;
    }
  }
  if (canCSS(geos, fill, stroke, hasTrim, false)) {
    renderCSSDiv(out, geos[0], fill, alpha, painterBlend);
  } else {
    renderSVG(out, geos, fill, stroke, alpha, painterBlend, trim, mergeMode);
  }
}

void HTMLWriter::renderDiamondCanvas(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill,
                                     float alpha, BlendMode painterBlend) {
  if (_ctx->staticImgDir.empty()) {
    // Rasterization disabled; silently skip. Caller can enable it by setting staticImgDir in
    // HTMLExportOptions.
    return;
  }
  auto dg = static_cast<const DiamondGradient*>(fill->color);
  float left = 0, top = 0, w = 0, h = 0;
  float roundness = 0;
  if (geo.type == NodeType::Rectangle) {
    auto r = static_cast<const Rectangle*>(geo.element);
    auto pos = r->renderPosition();
    auto size = r->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
    roundness = std::min(r->roundness, std::min(w / 2, h / 2));
  } else if (geo.type == NodeType::Ellipse) {
    auto e = static_cast<const Ellipse*>(geo.element);
    auto pos = e->renderPosition();
    auto size = e->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  std::string imgId = _ctx->nextId("dgc");
  std::string fileName = _ctx->staticImgNamePrefix + imgId + ".png";
  std::filesystem::create_directories(_ctx->staticImgDir);
  std::string absPath = _ctx->staticImgDir;
  if (!absPath.empty() && absPath.back() != '/') {
    absPath += "/";
  }
  absPath += fileName;

  bool ok = false;
  if (geo.type == NodeType::Ellipse) {
    ok = HTMLStaticImageRenderer::RenderDiamondEllipseToPng(left, top, w, h, dg,
                                                            _ctx->staticImgPixelRatio, absPath);
  } else {
    ok = HTMLStaticImageRenderer::RenderDiamondToPng(left, top, w, h, roundness, dg,
                                                     _ctx->staticImgPixelRatio, absPath);
  }
  if (!ok) {
    return;
  }

  std::string style = "position:absolute;left:" + FloatToString(left) +
                      "px;top:" + FloatToString(top) + "px;width:" + FloatToString(w) +
                      "px;height:" + FloatToString(h) + "px";
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }

  out.openTag("img");
  out.addAttr("src", _ctx->staticImgUrlPrefix + fileName);
  out.addAttr("style", style);
  out.closeTagSelfClosing();
}

void HTMLWriter::renderImagePatternCanvas(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill,
                                          float alpha, BlendMode painterBlend) {
  if (_ctx->staticImgDir.empty()) {
    return;
  }
  auto p = static_cast<const ImagePattern*>(fill->color);
  if (!p->image) {
    return;
  }
  float left = 0, top = 0, w = 0, h = 0;
  float roundness = 0;
  if (geo.type == NodeType::Rectangle) {
    auto r = static_cast<const Rectangle*>(geo.element);
    auto pos = r->renderPosition();
    auto size = r->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
    roundness = std::min(r->roundness, std::min(w / 2, h / 2));
  } else if (geo.type == NodeType::Ellipse) {
    auto e = static_cast<const Ellipse*>(geo.element);
    auto pos = e->renderPosition();
    auto size = e->renderSize();
    left = pos.x - size.width / 2;
    top = pos.y - size.height / 2;
    w = size.width;
    h = size.height;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  std::string imgId = _ctx->nextId("ipc");
  std::string fileName = _ctx->staticImgNamePrefix + imgId + ".png";
  std::filesystem::create_directories(_ctx->staticImgDir);
  std::string absPath = _ctx->staticImgDir;
  if (!absPath.empty() && absPath.back() != '/') {
    absPath += "/";
  }
  absPath += fileName;

  bool ok = false;
  if (geo.type == NodeType::Ellipse) {
    ok = HTMLStaticImageRenderer::RenderImagePatternEllipseToPng(
        left, top, w, h, p, _ctx->staticImgPixelRatio, absPath);
  } else {
    ok = HTMLStaticImageRenderer::RenderImagePatternToPng(left, top, w, h, roundness, p,
                                                          _ctx->staticImgPixelRatio, absPath);
  }
  if (!ok) {
    return;
  }

  std::string style = "position:absolute;left:" + FloatToString(left) +
                      "px;top:" + FloatToString(top) + "px;width:" + FloatToString(w) +
                      "px;height:" + FloatToString(h) + "px";
  if (alpha < 1.0f) {
    style += ";opacity:" + FloatToString(alpha);
  }
  if (painterBlend != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(painterBlend);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }
  if (p->filterMode == FilterMode::Nearest) {
    style += ";image-rendering:pixelated";
  }

  out.openTag("img");
  out.addAttr("src", _ctx->staticImgUrlPrefix + fileName);
  out.addAttr("style", style);
  out.closeTagSelfClosing();
}

}  // namespace pagx
