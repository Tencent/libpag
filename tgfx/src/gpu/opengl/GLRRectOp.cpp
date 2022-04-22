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

#include "GLRRectOp.h"

#include "core/utils/MathExtra.h"
#include "core/utils/UniqueID.h"
#include "gpu/EllipseGeometryProcessor.h"

namespace tgfx {
// We have three possible cases for geometry for a round rect.
//
// In the case of a normal fill or a stroke, we draw the round rect as a 9-patch:
//    ____________
//   |_|________|_|
//   | |        | |
//   | |        | |
//   | |        | |
//   |_|________|_|
//   |_|________|_|
//
// For strokes, we don't draw the center quad.
//
// For circular round rects, in the case where the stroke width is greater than twice
// the corner radius (over stroke), we add additional geometry to mark out the rectangle
// in the center. The shared vertices are duplicated, so we can set a different outer radius
// for the fill calculation.
//    ____________
//   |_|________|_|
//   | |\ ____ /| |
//   | | |    | | |
//   | | |____| | |
//   |_|/______\|_|
//   |_|________|_|
//
// We don't draw the center quad from the fill rect in this case.
//
// For filled rrects that need to provide a distance vector we reuse the overstroke
// geometry but make the inner rect degenerate (either a point or a horizontal or
// vertical line).

// clang-format off
static const uint16_t gOverstrokeRRectIndices[] = {
  // overstroke quads
  // we place this at the beginning so that we can skip these indices when rendering normally
  16, 17, 19, 16, 19, 18,
  19, 17, 23, 19, 23, 21,
  21, 23, 22, 21, 22, 20,
  22, 16, 18, 22, 18, 20,

  // corners
  0, 1, 5, 0, 5, 4,
  2, 3, 7, 2, 7, 6,
  8, 9, 13, 8, 13, 12,
  10, 11, 15, 10, 15, 14,

  // edges
  1, 2, 6, 1, 6, 5,
  4, 5, 9, 4, 9, 8,
  6, 7, 11, 6, 11, 10,
  9, 10, 14, 9, 14, 13,

  // center
  // we place this at the end so that we can ignore these indices when not rendering as filled
  5, 6, 10, 5, 10, 9,
};
// clang-format on

static constexpr int kOverstrokeIndicesCount = 6 * 4;
static constexpr int kCornerIndicesCount = 6 * 4;
static constexpr int kEdgeIndicesCount = 6 * 4;
static constexpr int kCenterIndicesCount = 6;

// fill and standard stroke indices skip the overstroke "ring"
static const uint16_t* gStandardRRectIndices = gOverstrokeRRectIndices + kOverstrokeIndicesCount;

// overstroke count is arraysize minus the center indices
// static constexpr int kIndicesPerOverstrokeRRect =
//    kOverstrokeIndicesCount + kCornerIndicesCount + kEdgeIndicesCount;
// fill count skips overstroke indices and includes center
static constexpr size_t kIndicesPerFillRRect =
    kCornerIndicesCount + kEdgeIndicesCount + kCenterIndicesCount;
// stroke count is fill count minus center indices
// static constexpr int kIndicesPerStrokeRRect = kCornerIndicesCount + kEdgeIndicesCount;

std::unique_ptr<GLRRectOp> GLRRectOp::Make(const RRect& rRect, const Matrix& viewMatrix) {
  if (/*!isStrokeOnly && */ 0.5f <= rRect.radii.x && 0.5f <= rRect.radii.y) {
    return std::unique_ptr<GLRRectOp>(new GLRRectOp(rRect, viewMatrix));
  }
  return nullptr;
}

GLRRectOp::GLRRectOp(const RRect& rRect, const Matrix& viewMatrix)
    : rRect(rRect), viewMatrix(viewMatrix), xRadius(rRect.radii.x), yRadius(rRect.radii.y) {
  setTransformedBounds(rRect.rect, viewMatrix);
}

static bool UseScale(const DrawArgs& args) {
  return !args.context->caps()->floatIs32Bits;
}

std::unique_ptr<GeometryProcessor> GLRRectOp::getGeometryProcessor(const DrawArgs& args) {
  auto localMatrix = Matrix::MakeScale(1.f / rRect.rect.width(), 1.f / rRect.rect.height());
  localMatrix.postTranslate(-rRect.rect.left / rRect.rect.width(),
                            -rRect.rect.top / rRect.rect.height());
  return EllipseGeometryProcessor::Make(args.renderTarget->width(), args.renderTarget->height(),
                                        false, UseScale(args), localMatrix, viewMatrix);
}

std::vector<float> GLRRectOp::vertices(const DrawArgs& args) {
  auto useScale = UseScale(args);
  float reciprocalRadii[4] = {1e6f, 1e6f, 1e6f, 1e6f};
  if (xRadius > 0) {
    reciprocalRadii[0] = 1.f / xRadius;
  }
  if (yRadius > 0) {
    reciprocalRadii[1] = 1.f / yRadius;
  }
  if (innerXRadius > 0) {
    reciprocalRadii[2] = 1.f / innerXRadius;
  }
  if (innerYRadius > 0) {
    reciprocalRadii[3] = 1.f / innerYRadius;
  }
  // On MSAA, bloat enough to guarantee any pixel that might be touched by the rRect has
  // full sample coverage.
  float aaBloat = args.aa == AAType::MSAA ? FLOAT_SQRT2 : .5f;
  // Extend out the radii to antialias.
  float xOuterRadius = xRadius + aaBloat;
  float yOuterRadius = yRadius + aaBloat;

  float xMaxOffset = xOuterRadius;
  float yMaxOffset = yOuterRadius;
  //  if (!stroked) {
  // For filled rRects we map a unit circle in the vertex attributes rather than
  // computing an ellipse and modifying that distance, so we normalize to 1.
  xMaxOffset /= xRadius;
  yMaxOffset /= yRadius;
  //  }
  auto bounds = rRect.rect.makeOutset(aaBloat, aaBloat);
  float yCoords[4] = {bounds.top, bounds.top + yOuterRadius, bounds.bottom - yOuterRadius,
                      bounds.bottom};
  float yOuterOffsets[4] = {
      yMaxOffset,
      FLOAT_NEARLY_ZERO,  // we're using inversesqrt() in shader, so can't be exactly 0
      FLOAT_NEARLY_ZERO, yMaxOffset};
  std::vector<float> vertices;
  auto maxRadius = std::max(xRadius, yRadius);
  for (int i = 0; i < 4; ++i) {
    vertices.push_back(bounds.left);
    vertices.push_back(yCoords[i]);
    vertices.push_back(xMaxOffset);
    vertices.push_back(yOuterOffsets[i]);
    if (useScale) {
      vertices.push_back(maxRadius);
    }
    vertices.push_back(reciprocalRadii[0]);
    vertices.push_back(reciprocalRadii[1]);
    vertices.push_back(reciprocalRadii[2]);
    vertices.push_back(reciprocalRadii[3]);

    vertices.push_back(bounds.left + xOuterRadius);
    vertices.push_back(yCoords[i]);
    vertices.push_back(FLOAT_NEARLY_ZERO);
    vertices.push_back(yOuterOffsets[i]);
    if (useScale) {
      vertices.push_back(maxRadius);
    }
    vertices.push_back(reciprocalRadii[0]);
    vertices.push_back(reciprocalRadii[1]);
    vertices.push_back(reciprocalRadii[2]);
    vertices.push_back(reciprocalRadii[3]);

    vertices.push_back(bounds.right - xOuterRadius);
    vertices.push_back(yCoords[i]);
    vertices.push_back(FLOAT_NEARLY_ZERO);
    vertices.push_back(yOuterOffsets[i]);
    if (useScale) {
      vertices.push_back(maxRadius);
    }
    vertices.push_back(reciprocalRadii[0]);
    vertices.push_back(reciprocalRadii[1]);
    vertices.push_back(reciprocalRadii[2]);
    vertices.push_back(reciprocalRadii[3]);

    vertices.push_back(bounds.right);
    vertices.push_back(yCoords[i]);
    vertices.push_back(xMaxOffset);
    vertices.push_back(yOuterOffsets[i]);
    if (useScale) {
      vertices.push_back(maxRadius);
    }
    vertices.push_back(reciprocalRadii[0]);
    vertices.push_back(reciprocalRadii[1]);
    vertices.push_back(reciprocalRadii[2]);
    vertices.push_back(reciprocalRadii[3]);
  }
  return vertices;
}

void GLRRectOp::draw(const DrawArgs& args) {
  static auto Type = UniqueID::Next();
  auto buffer = GLBuffer::Make(args.context, gStandardRRectIndices, kIndicesPerFillRRect, Type);
  GLDrawer::DrawIndexBuffer(args.context, buffer);
}
}  // namespace tgfx
