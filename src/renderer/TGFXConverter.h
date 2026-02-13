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

#include "pagx/nodes/PathData.h"
#include "pagx/types/Point.h"
#include "tgfx/core/Path.h"

namespace pagx {

inline tgfx::Path PathDataToTGFXPath(const PathData& pathData) {
  tgfx::Path path = {};
  pathData.forEach([&path](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        path.moveTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Line:
        path.lineTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Quad:
        path.quadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        break;
      case PathVerb::Cubic:
        path.cubicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        break;
      case PathVerb::Close:
        path.close();
        break;
    }
  });
  return path;
}

inline tgfx::Point PointToTGFX(const Point& p) {
  return tgfx::Point::Make(p.x, p.y);
}

}  // namespace pagx
