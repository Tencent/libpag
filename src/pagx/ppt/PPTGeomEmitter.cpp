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

#include "pagx/ppt/PPTGeomEmitter.h"
#include <algorithm>
#include <cmath>
#include "pagx/xml/XMLBuilder.h"

namespace pagx {

int64_t PxToEMU(float px) {
  return static_cast<int64_t>(std::round(static_cast<double>(px) * EMU_PER_PX));
}

void EmitPoint(XMLBuilder& out, const char* tag, float x, float y, float ofsX, float ofsY) {
  out.open(tag).gt();
  out.open("a:pt").attr("x", PxToEMU(x - ofsX)).attr("y", PxToEMU(y - ofsY)).sc();
  out.end();
}

void EmitQuadBezTo(XMLBuilder& out, const char* tag, float x0, float y0, float x1, float y1,
                   float ofsX, float ofsY) {
  out.open(tag).gt();
  out.open("a:pt").attr("x", PxToEMU(x0 - ofsX)).attr("y", PxToEMU(y0 - ofsY)).sc();
  out.open("a:pt").attr("x", PxToEMU(x1 - ofsX)).attr("y", PxToEMU(y1 - ofsY)).sc();
  out.end();
}

void EmitCubicBezTo(XMLBuilder& out, float x0, float y0, float x1, float y1, float x2, float y2,
                    float ofsX, float ofsY) {
  out.open("a:cubicBezTo").gt();
  out.open("a:pt").attr("x", PxToEMU(x0 - ofsX)).attr("y", PxToEMU(y0 - ofsY)).sc();
  out.open("a:pt").attr("x", PxToEMU(x1 - ofsX)).attr("y", PxToEMU(y1 - ofsY)).sc();
  out.open("a:pt").attr("x", PxToEMU(x2 - ofsX)).attr("y", PxToEMU(y2 - ofsY)).sc();
  out.end();
}

void EmitContourSegments(XMLBuilder& out, const PathContour& c, float csX, float csY, float sOfsX,
                         float sOfsY) {
  for (const auto& s : c.segs) {
    switch (s.verb) {
      case PathVerb::Line:
        EmitPoint(out, "a:lnTo", s.pts[0].x * csX, s.pts[0].y * csY, sOfsX, sOfsY);
        break;
      case PathVerb::Quad:
        EmitQuadBezTo(out, "a:quadBezTo", s.pts[0].x * csX, s.pts[0].y * csY, s.pts[1].x * csX,
                      s.pts[1].y * csY, sOfsX, sOfsY);
        break;
      case PathVerb::Cubic:
        EmitCubicBezTo(out, s.pts[0].x * csX, s.pts[0].y * csY, s.pts[1].x * csX, s.pts[1].y * csY,
                       s.pts[2].x * csX, s.pts[2].y * csY, sOfsX, sOfsY);
        break;
      default:
        break;
    }
  }
}

void EmitContour(XMLBuilder& out, const PathContour& c, float csX, float csY, float sOfsX,
                 float sOfsY) {
  EmitPoint(out, "a:moveTo", c.start.x * csX, c.start.y * csY, sOfsX, sOfsY);
  EmitContourSegments(out, c, csX, csY, sOfsX, sOfsY);
  if (c.closed) {
    out.open("a:close").sc();
  }
}

// Stitches an outer contour with its nested inner contours into a single contour
// using zero-width bridge lines. The bridge goes from the outer's start to each
// inner contour's start and returns along the same line, producing a self-overlapping
// edge pair that cancels in any scan-line rasterizer.
void EmitBridgedGroup(XMLBuilder& out, const std::vector<PathContour>& contours,
                      const std::vector<size_t>& group, float csX, float csY, float sOfsX,
                      float sOfsY) {
  const auto& outer = contours[group[0]];

  EmitPoint(out, "a:moveTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  EmitContourSegments(out, outer, csX, csY, sOfsX, sOfsY);
  if (outer.closed) {
    EmitPoint(out, "a:lnTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  }

  for (size_t i = 1; i < group.size(); i++) {
    const auto& inner = contours[group[i]];
    if (inner.segs.empty()) {
      continue;
    }
    EmitPoint(out, "a:lnTo", inner.start.x * csX, inner.start.y * csY, sOfsX, sOfsY);
    EmitContourSegments(out, inner, csX, csY, sOfsX, sOfsY);
    if (inner.closed) {
      EmitPoint(out, "a:lnTo", inner.start.x * csX, inner.start.y * csY, sOfsX, sOfsY);
    }
    EmitPoint(out, "a:lnTo", outer.start.x * csX, outer.start.y * csY, sOfsX, sOfsY);
  }

  out.open("a:close").sc();
}

void EmitGroupCustGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                       float scaleX, float scaleY, float scaledOfsX, float scaledOfsY) {
  out.open("a:custGeom").gt();
  out.open("a:avLst").sc();
  out.open("a:gdLst").sc();
  out.open("a:ahLst").sc();
  out.open("a:cxnLst").sc();
  out.open("a:rect").attr("l", "0").attr("t", "0").attr("r", "r").attr("b", "b").sc();
  out.open("a:pathLst").gt();
  out.open("a:path").attr("w", pathWidth).attr("h", pathHeight).gt();

  // Zero-length segments at opposite corners of the coordinate space.  They
  // are invisible but force the content bounding box to match the declared
  // (w, h), preventing renderers that auto-fit shapes to actual content bounds
  // (e.g. WeChat) from rescaling each group independently.
  out.open("a:moveTo").gt();
  out.open("a:pt").attr("x", int64_t(0)).attr("y", int64_t(0)).sc();
  out.end();
  out.open("a:lnTo").gt();
  out.open("a:pt").attr("x", int64_t(0)).attr("y", int64_t(0)).sc();
  out.end();
  out.open("a:moveTo").gt();
  out.open("a:pt").attr("x", pathWidth).attr("y", pathHeight).sc();
  out.end();
  out.open("a:lnTo").gt();
  out.open("a:pt").attr("x", pathWidth).attr("y", pathHeight).sc();
  out.end();

  if (group.size() > 1) {
    EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
  } else {
    EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
  }
  out.end();  // a:path
  out.end();  // a:pathLst
  out.end();  // a:custGeom
}

}  // namespace pagx
