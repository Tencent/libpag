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

namespace {

void EmitPoint(XMLBuilder& out, const char* tag, float x, float y, float ofsX, float ofsY) {
  out.openElement(tag).closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x - ofsX))
      .addRequiredAttribute("y", PxToEMU(y - ofsY))
      .closeElementSelfClosing();
  out.closeElement();
}

void EmitQuadBezTo(XMLBuilder& out, const char* tag, float x0, float y0, float x1, float y1,
                   float ofsX, float ofsY) {
  out.openElement(tag).closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x0 - ofsX))
      .addRequiredAttribute("y", PxToEMU(y0 - ofsY))
      .closeElementSelfClosing();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x1 - ofsX))
      .addRequiredAttribute("y", PxToEMU(y1 - ofsY))
      .closeElementSelfClosing();
  out.closeElement();
}

void EmitCubicBezTo(XMLBuilder& out, float x0, float y0, float x1, float y1, float x2, float y2,
                    float ofsX, float ofsY) {
  out.openElement("a:cubicBezTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x0 - ofsX))
      .addRequiredAttribute("y", PxToEMU(y0 - ofsY))
      .closeElementSelfClosing();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x1 - ofsX))
      .addRequiredAttribute("y", PxToEMU(y1 - ofsY))
      .closeElementSelfClosing();
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(x2 - ofsX))
      .addRequiredAttribute("y", PxToEMU(y2 - ofsY))
      .closeElementSelfClosing();
  out.closeElement();
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
    out.openElement("a:close").closeElementSelfClosing();
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

  out.openElement("a:close").closeElementSelfClosing();
}

// Emits one <a:path> for the given group: bridged when the group has nested
// inner contours, plain otherwise. Caller is responsible for the surrounding
// <a:pathLst> / <a:custGeom>.
void EmitGroupPath(XMLBuilder& out, const std::vector<PathContour>& contours,
                   const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                   float scaleX, float scaleY, float scaledOfsX, float scaledOfsY) {
  out.openElement("a:path")
      .addRequiredAttribute("w", pathWidth)
      .addRequiredAttribute("h", pathHeight)
      .closeElementStart();
  if (group.size() > 1) {
    EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
  } else {
    EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
  }
  out.closeElement();  // a:path
}

void EmitCustGeomHeader(XMLBuilder& out) {
  out.openElement("a:custGeom").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.openElement("a:gdLst").closeElementSelfClosing();
  out.openElement("a:ahLst").closeElementSelfClosing();
  out.openElement("a:cxnLst").closeElementSelfClosing();
  out.openElement("a:rect")
      .addRequiredAttribute("l", "0")
      .addRequiredAttribute("t", "0")
      .addRequiredAttribute("r", "r")
      .addRequiredAttribute("b", "b")
      .closeElementSelfClosing();
}

}  // namespace

void EmitContourGeomFromGroups(XMLBuilder& out, const std::vector<PathContour>& contours,
                               const std::vector<std::vector<size_t>>& groups, int64_t pathWidth,
                               int64_t pathHeight, float scaleX, float scaleY, float scaledOfsX,
                               float scaledOfsY) {
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();
  for (const auto& group : groups) {
    EmitGroupPath(out, contours, group, pathWidth, pathHeight, scaleX, scaleY, scaledOfsX,
                  scaledOfsY);
  }
  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

void EmitFlatContourGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                         int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                         float scaledOfsX, float scaledOfsY) {
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();
  out.openElement("a:path")
      .addRequiredAttribute("w", pathWidth)
      .addRequiredAttribute("h", pathHeight)
      .closeElementStart();
  for (const auto& c : contours) {
    EmitContour(out, c, scaleX, scaleY, scaledOfsX, scaledOfsY);
  }
  out.closeElement();  // a:path
  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

void EmitGroupCustGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                       float scaleX, float scaleY, float scaledOfsX, float scaledOfsY) {
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();
  out.openElement("a:path")
      .addRequiredAttribute("w", pathWidth)
      .addRequiredAttribute("h", pathHeight)
      .closeElementStart();

  // Zero-length segments at opposite corners of the coordinate space.  They
  // are invisible but force the content bounding box to match the declared
  // (w, h), preventing renderers that auto-fit shapes to actual content bounds
  // (e.g. WeChat) from rescaling each group independently.
  out.openElement("a:moveTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", int64_t(0))
      .addRequiredAttribute("y", int64_t(0))
      .closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:lnTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", int64_t(0))
      .addRequiredAttribute("y", int64_t(0))
      .closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:moveTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", pathWidth)
      .addRequiredAttribute("y", pathHeight)
      .closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:lnTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", pathWidth)
      .addRequiredAttribute("y", pathHeight)
      .closeElementSelfClosing();
  out.closeElement();

  if (group.size() > 1) {
    EmitBridgedGroup(out, contours, group, scaleX, scaleY, scaledOfsX, scaledOfsY);
  } else {
    EmitContour(out, contours[group[0]], scaleX, scaleY, scaledOfsX, scaledOfsY);
  }
  out.closeElement();  // a:path
  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

}  // namespace pagx
