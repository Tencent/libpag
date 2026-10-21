/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/html/importer/HTMLBoxGeometry.h"
#include <algorithm>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/nodes/PathData.h"

namespace pagx {
void BuildHTMLBoxPath(PathData& path, const HTMLBoxAttributes& box, const Padding& inset) {
  float l = inset.left, t = inset.top;
  float r = std::max(l, box.widthPx - inset.right);
  float b = std::max(t, box.heightPx - inset.bottom);
  float rx[] = {box.borderRadiusTLPx, box.borderRadiusTRPx, box.borderRadiusBRPx,
                box.borderRadiusBLPx};
  float ry[] = {box.borderRadiusTLYPx, box.borderRadiusTRYPx, box.borderRadiusBRYPx,
                box.borderRadiusBLYPx};
  float dx[] = {inset.left, inset.right, inset.right, inset.left};
  float dy[] = {inset.top, inset.top, inset.bottom, inset.bottom};
  for (int i = 0; i < 4; ++i) {
    // Outsetting a square corner keeps it square; rounded corners grow by the outset.
    rx[i] = rx[i] > 0 ? std::max(0.0f, rx[i] - dx[i]) : 0;
    ry[i] = ry[i] > 0 ? std::max(0.0f, ry[i] - dy[i]) : 0;
    if (rx[i] == 0 || ry[i] == 0) rx[i] = ry[i] = 0;
  }
  float scale = 1;
  for (auto edge : {std::make_pair(r - l, rx[0] + rx[1]), std::make_pair(r - l, rx[2] + rx[3]),
                    std::make_pair(b - t, ry[0] + ry[3]), std::make_pair(b - t, ry[1] + ry[2])})
    if (edge.second > 0) scale = std::min(scale, edge.first / edge.second);
  for (int i = 0; i < 4; ++i) {
    rx[i] *= scale;
    ry[i] *= scale;
  }
  const float k = 1 - 0.5522847498307936f;
  path.moveTo(r, t + ry[1]);
  path.lineTo(r, b - ry[2]);
  if (rx[2] > 0 && ry[2] > 0) path.cubicTo(r, b - ry[2] * k, r - rx[2] * k, b, r - rx[2], b);
  path.lineTo(l + rx[3], b);
  if (rx[3] > 0 && ry[3] > 0) path.cubicTo(l + rx[3] * k, b, l, b - ry[3] * k, l, b - ry[3]);
  path.lineTo(l, t + ry[0]);
  if (rx[0] > 0 && ry[0] > 0) path.cubicTo(l, t + ry[0] * k, l + rx[0] * k, t, l + rx[0], t);
  path.lineTo(r - rx[1], t);
  if (rx[1] > 0 && ry[1] > 0) path.cubicTo(r - rx[1] * k, t, r, t + ry[1] * k, r, t + ry[1]);
  path.close();
}
}  // namespace pagx
