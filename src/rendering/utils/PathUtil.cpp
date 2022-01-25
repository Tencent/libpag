/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace pag {
Path ToPath(const PathData& pathData) {
    Path path = {};
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
}  // namespace pag