/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "PathUtil.h"
#include "tgfx/core/PathEffect.h"

namespace pag {
tgfx::Path ToPath(const PathData& pathData) {
  tgfx::Path path = {};
  auto& points = pathData.points;
  uint32_t index = 0;
  Point control1 = {}, control2 = {}, point = {};
  for (auto& verb : pathData.verbs) {
    switch (verb) {
      case PathDataVerb::Close:
        path.close();
        break;
      case PathDataVerb::MoveTo:
        point = points[index++];
        path.moveTo(point.x, point.y);
        break;
      case PathDataVerb::LineTo:
        point = points[index++];
        path.lineTo(point.x, point.y);
        break;
      case PathDataVerb::CurveTo:
        control1 = points[index++];
        control2 = points[index++];
        point = points[index++];
        path.cubicTo(control1.x, control1.y, control2.x, control2.y, point.x, point.y);
        break;
    }
  }
  return path;
}

tgfx::PathOp ToPathOp(MaskMode maskMode) {
  switch (maskMode) {
    case MaskMode::Subtract:
      return tgfx::PathOp::Difference;
    case MaskMode::Intersect:
      return tgfx::PathOp::Intersect;
    case MaskMode::Difference:
      return tgfx::PathOp::XOR;
    case MaskMode::Darken:  // without the opacity blending, haven't supported it
      return tgfx::PathOp::Intersect;
    default:
      return tgfx::PathOp::Union;
  }
}

void ExpandPath(tgfx::Path* path, float expansion) {
  if (expansion == 0) {
    return;
  }
  auto strokePath = *path;
  tgfx::Stroke stroke(fabsf(expansion) * 2, tgfx::LineCap::Butt, tgfx::LineJoin::Round);
  stroke.applyToPath(&strokePath);
  if (expansion < 0) {
    path->addPath(strokePath, tgfx::PathOp::Difference);
  } else {
    path->addPath(strokePath, tgfx::PathOp::Union);
  }
}
}  // namespace pag
