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
#include <initializer_list>
#include <optional>
#include "pagx/xml/XMLBuilder.h"

namespace pagx {

int64_t PxToEMU(float px) {
  return static_cast<int64_t>(std::round(static_cast<double>(px) * EMU_PER_PX));
}

namespace {

// Bundles the per-emit coordinate transform (scale + offset) used to map a
// PathContour's original pixel coordinates into output space. Previously the
// four components were threaded through every emit helper as independent
// floats, which made call sites noisy and easy to reorder incorrectly.
struct CoordScale {
  float scaleX;
  float scaleY;
  float offX;
  float offY;

  Point map(const Point& p) const {
    return {p.x * scaleX - offX, p.y * scaleY - offY};
  }
};

// Writes a single <a:pt x=... y=.../> with EMU-converted coordinates.
void EmitPt(XMLBuilder& out, const Point& p) {
  out.openElement("a:pt")
      .addRequiredAttribute("x", PxToEMU(p.x))
      .addRequiredAttribute("y", PxToEMU(p.y))
      .closeElementSelfClosing();
}

// Writes `<tag> <a:pt/>... </tag>`. Covers a:moveTo / a:lnTo (1 pt),
// a:quadBezTo (2 pts) and a:cubicBezTo (3 pts) with a single primitive.
void EmitPtList(XMLBuilder& out, const char* tag, std::initializer_list<Point> pts) {
  out.openElement(tag).closeElementStart();
  for (const auto& p : pts) {
    EmitPt(out, p);
  }
  out.closeElement();
}

void EmitContourSegments(XMLBuilder& out, const PathContour& c, const CoordScale& ts) {
  for (const auto& s : c.segs) {
    switch (s.verb) {
      case PathVerb::Line:
        EmitPtList(out, "a:lnTo", {ts.map(s.pts[0])});
        break;
      case PathVerb::Quad:
        EmitPtList(out, "a:quadBezTo", {ts.map(s.pts[0]), ts.map(s.pts[1])});
        break;
      case PathVerb::Cubic:
        EmitPtList(out, "a:cubicBezTo", {ts.map(s.pts[0]), ts.map(s.pts[1]), ts.map(s.pts[2])});
        break;
      default:
        break;
    }
  }
}

void EmitContour(XMLBuilder& out, const PathContour& c, const CoordScale& ts) {
  EmitPtList(out, "a:moveTo", {ts.map(c.start)});
  EmitContourSegments(out, c, ts);
  if (c.closed) {
    out.openElement("a:close").closeElementSelfClosing();
  }
}

// Stitches an outer contour with its nested inner contours into a single
// self-overlapping path using zero-width bridge lines. Each inner ring is
// entered from the outer start and exited back to the outer start along the
// same line, so the paired edges cancel under any scan-line rasteriser and
// even-odd fill renders the expected holes.
void EmitBridgedGroup(XMLBuilder& out, const std::vector<PathContour>& contours,
                      const std::vector<size_t>& group, const CoordScale& ts) {
  const auto& outer = contours[group[0]];
  const Point outerStart = ts.map(outer.start);

  EmitPtList(out, "a:moveTo", {outerStart});
  EmitContourSegments(out, outer, ts);
  if (outer.closed) {
    // Explicit lnTo back to the outer start provides a fixed anchor from
    // which the following bridge lines can depart.
    EmitPtList(out, "a:lnTo", {outerStart});
  }

  for (size_t i = 1; i < group.size(); i++) {
    const auto& inner = contours[group[i]];
    if (inner.segs.empty()) {
      continue;
    }
    const Point innerStart = ts.map(inner.start);
    EmitPtList(out, "a:lnTo", {innerStart});
    EmitContourSegments(out, inner, ts);
    if (inner.closed) {
      EmitPtList(out, "a:lnTo", {innerStart});
    }
    EmitPtList(out, "a:lnTo", {outerStart});
  }

  out.openElement("a:close").closeElementSelfClosing();
}

// Writes the geometry content for a group without opening its <a:path>.
// Split out of EmitGroupPath so EmitCustGeomCore's InlineSegments branch can
// place corner marker segments in the same <a:path> as the main geometry.
void EmitGroupPathBody(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, const CoordScale& ts) {
  if (group.size() > 1) {
    EmitBridgedGroup(out, contours, group, ts);
  } else {
    EmitContour(out, contours[group[0]], ts);
  }
}

void OpenPath(XMLBuilder& out, int64_t pathWidth, int64_t pathHeight) {
  out.openElement("a:path")
      .addRequiredAttribute("w", pathWidth)
      .addRequiredAttribute("h", pathHeight)
      .closeElementStart();
}

// Emits a complete <a:path>...</a:path> element for one group. Use when no
// bounds-marker needs to share the same path.
void EmitGroupPath(XMLBuilder& out, const std::vector<PathContour>& contours,
                   const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                   const CoordScale& ts) {
  OpenPath(out, pathWidth, pathHeight);
  EmitGroupPathBody(out, contours, group, ts);
  out.closeElement();
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

// Writes a zero-length (moveTo; lnTo) pair at (x, y). WeChat infers the path
// bounding box from drawable segment extent, so emitting these at (0,0) and
// (pathWidth, pathHeight) pins the bbox to the declared size.
void EmitZeroLengthMarker(XMLBuilder& out, int64_t x, int64_t y) {
  out.openElement("a:moveTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", x)
      .addRequiredAttribute("y", y)
      .closeElementSelfClosing();
  out.closeElement();
  out.openElement("a:lnTo").closeElementStart();
  out.openElement("a:pt")
      .addRequiredAttribute("x", x)
      .addRequiredAttribute("y", y)
      .closeElementSelfClosing();
  out.closeElement();
}

void EmitCornerBoundsMarkerSegments(XMLBuilder& out, int64_t pathWidth, int64_t pathHeight) {
  EmitZeroLengthMarker(out, 0, 0);
  EmitZeroLengthMarker(out, pathWidth, pathHeight);
}

// Emits a standalone strokeless sibling <a:path> carrying only the corner
// bounds markers. Used by StandaloneStrokelessPath so round-capped strokes on
// the enclosing sp cannot render visible dots at the corner points.
void EmitStandaloneMarkerPath(XMLBuilder& out, int64_t pathWidth, int64_t pathHeight) {
  out.openElement("a:path")
      .addRequiredAttribute("w", pathWidth)
      .addRequiredAttribute("h", pathHeight)
      .addRequiredAttribute("fill", "none")
      .addRequiredAttribute("stroke", "0")
      .closeElementStart();
  EmitCornerBoundsMarkerSegments(out, pathWidth, pathHeight);
  out.closeElement();
}

// Shared <a:custGeom> writer: emits one <a:path> per group, with an optional
// per-group corner bounds marker placed according to markerStyle. When
// markerStyle is nullopt no marker is emitted (plain geometry). Groups are
// treated independently — each group carries its own marker so the strategy
// composes cleanly across any number of groups.
void EmitCustGeomCore(XMLBuilder& out, const std::vector<PathContour>& contours,
                      const std::vector<std::vector<size_t>>& groups, int64_t pathWidth,
                      int64_t pathHeight, const CoordScale& ts,
                      std::optional<BoundsMarkerStyle> markerStyle) {
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();

  for (const auto& group : groups) {
    if (!markerStyle.has_value()) {
      EmitGroupPath(out, contours, group, pathWidth, pathHeight, ts);
      continue;
    }
    // Bounds marker strategy documented on BoundsMarkerStyle. Both strategies
    // keep the bbox pinned to (pathWidth, pathHeight) for WeChat; the
    // difference is whether the marker segments share the main <a:path> (safe
    // when the sp has no stroke) or live in a strokeless sibling path (needed
    // when the sp may carry a round-capped stroke that would otherwise render
    // visible dots at the corners).
    if (*markerStyle == BoundsMarkerStyle::StandaloneStrokelessPath) {
      EmitStandaloneMarkerPath(out, pathWidth, pathHeight);
      EmitGroupPath(out, contours, group, pathWidth, pathHeight, ts);
    } else {
      OpenPath(out, pathWidth, pathHeight);
      EmitCornerBoundsMarkerSegments(out, pathWidth, pathHeight);
      EmitGroupPathBody(out, contours, group, ts);
      out.closeElement();
    }
  }

  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

}  // namespace

void EmitContourGeomFromGroups(XMLBuilder& out, const std::vector<PathContour>& contours,
                               const std::vector<std::vector<size_t>>& groups, int64_t pathWidth,
                               int64_t pathHeight, float scaleX, float scaleY, float scaledOfsX,
                               float scaledOfsY) {
  const CoordScale ts{scaleX, scaleY, scaledOfsX, scaledOfsY};
  EmitCustGeomCore(out, contours, groups, pathWidth, pathHeight, ts, std::nullopt);
}

void EmitFlatContourGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                         int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                         float scaledOfsX, float scaledOfsY) {
  const CoordScale ts{scaleX, scaleY, scaledOfsX, scaledOfsY};
  EmitCustGeomHeader(out);
  out.openElement("a:pathLst").closeElementStart();
  OpenPath(out, pathWidth, pathHeight);
  for (const auto& c : contours) {
    EmitContour(out, c, ts);
  }
  out.closeElement();  // a:path
  out.closeElement();  // a:pathLst
  out.closeElement();  // a:custGeom
}

void EmitGroupCustGeom(XMLBuilder& out, const std::vector<PathContour>& contours,
                       const std::vector<size_t>& group, int64_t pathWidth, int64_t pathHeight,
                       float scaleX, float scaleY, float scaledOfsX, float scaledOfsY,
                       BoundsMarkerStyle markerStyle) {
  const CoordScale ts{scaleX, scaleY, scaledOfsX, scaledOfsY};
  EmitCustGeomCore(out, contours, {group}, pathWidth, pathHeight, ts, markerStyle);
}

}  // namespace pagx
