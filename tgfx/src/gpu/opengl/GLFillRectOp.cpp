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

#include "GLFillRectOp.h"

#include "gpu/QuadPerEdgeAAGeometryProcessor.h"

namespace pag {
std::unique_ptr<GeometryProcessor> GLFillRectOp::getGeometryProcessor(const DrawArgs& args) {
  return QuadPerEdgeAAGeometryProcessor::Make(
      args.renderTarget->width(), args.renderTarget->height(), args.viewMatrix, args.aa);
}

std::vector<float> GLFillRectOp::vertices(const DrawArgs& args) {
  auto bounds = args.rectToDraw;
  auto normalBounds = Rect::MakeLTRB(0, 0, 1, 1);
  // Vertex coordinates are arranged in a 2D pixel coordinate system, and textures are arranged
  // according to a texture coordinate system (0 - 1).
  if (args.aa != AAType::Coverage) {
    return {
        bounds.right, bounds.bottom, normalBounds.right, normalBounds.bottom,
        bounds.right, bounds.top,    normalBounds.right, normalBounds.top,
        bounds.left,  bounds.bottom, normalBounds.left,  normalBounds.bottom,
        bounds.left,  bounds.top,    normalBounds.left,  normalBounds.top,
    };
  }
  auto scale = sqrtf(args.viewMatrix.getScaleX() * args.viewMatrix.getScaleX() +
                     args.viewMatrix.getSkewY() * args.viewMatrix.getSkewY());
  // we want the new edge to be .5px away from the old line.
  auto padding = 0.5f / scale;
  auto insetBounds = bounds.makeInset(padding, padding);
  auto outsetBounds = bounds.makeOutset(padding, padding);

  auto normalPadding = Point::Make(padding / bounds.width(), padding / bounds.height());
  auto normalInset = normalBounds.makeInset(normalPadding.x, normalPadding.y);
  auto normalOutset = normalBounds.makeOutset(normalPadding.x, normalPadding.y);
  return {
      insetBounds.left,   insetBounds.top,     1.0f, normalInset.left,   normalInset.top,
      insetBounds.left,   insetBounds.bottom,  1.0f, normalInset.left,   normalInset.bottom,
      insetBounds.right,  insetBounds.top,     1.0f, normalInset.right,  normalInset.top,
      insetBounds.right,  insetBounds.bottom,  1.0f, normalInset.right,  normalInset.bottom,
      outsetBounds.left,  outsetBounds.top,    0.0f, normalOutset.left,  normalOutset.top,
      outsetBounds.left,  outsetBounds.bottom, 0.0f, normalOutset.left,  normalOutset.bottom,
      outsetBounds.right, outsetBounds.top,    0.0f, normalOutset.right, normalOutset.top,
      outsetBounds.right, outsetBounds.bottom, 0.0f, normalOutset.right, normalOutset.bottom,
  };
}

std::unique_ptr<GLFillRectOp> GLFillRectOp::Make() {
  return std::make_unique<GLFillRectOp>();
}

static constexpr size_t kIndicesPerAAFillRect = 30;

// clang-format off
static constexpr uint16_t gFillAARectIdx[] = {
  0, 1, 2, 1, 3, 2,
  0, 4, 1, 4, 5, 1,
  0, 6, 4, 0, 2, 6,
  2, 3, 6, 3, 7, 6,
  1, 5, 3, 3, 5, 7,
};
// clang-format on

std::shared_ptr<GLBuffer> GLFillRectOp::getIndexBuffer(const DrawArgs& args) {
  if (args.aa == AAType::Coverage) {
    return GLBuffer::Make(args.context, gFillAARectIdx, kIndicesPerAAFillRect);
  }
  return nullptr;
}
}  // namespace pag
