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

#include "core/utils/UniqueID.h"
#include "gpu/Quad.h"
#include "gpu/QuadPerEdgeAAGeometryProcessor.h"

namespace tgfx {
std::unique_ptr<GeometryProcessor> GLFillRectOp::getGeometryProcessor(const DrawArgs& args) {
  return QuadPerEdgeAAGeometryProcessor::Make(
      args.renderTarget->width(), args.renderTarget->height(), args.aa, !colors.empty());
}

std::vector<float> GLFillRectOp::coverageVertices() const {
  auto normalBounds = Rect::MakeLTRB(0, 0, 1, 1);
  std::vector<float> vertices;
  for (size_t i = 0; i < rects.size(); ++i) {
    auto scale = sqrtf(viewMatrices[i].getScaleX() * viewMatrices[i].getScaleX() +
                       viewMatrices[i].getSkewY() * viewMatrices[i].getSkewY());
    // we want the new edge to be .5px away from the old line.
    auto padding = 0.5f / scale;
    auto insetBounds = rects[i].makeInset(padding, padding);
    auto insetQuad = Quad::MakeFromRect(insetBounds, viewMatrices[i]);
    auto outsetBounds = rects[i].makeOutset(padding, padding);
    auto outsetQuad = Quad::MakeFromRect(outsetBounds, viewMatrices[i]);

    auto normalPadding = Point::Make(padding / rects[i].width(), padding / rects[i].height());
    auto normalInset = normalBounds.makeInset(normalPadding.x, normalPadding.y);
    auto normalInsetQuad = Quad::MakeFromRect(normalInset, localMatrices[i]);
    auto normalOutset = normalBounds.makeOutset(normalPadding.x, normalPadding.y);
    auto normalOutsetQuad = Quad::MakeFromRect(normalOutset, localMatrices[i]);

    for (int j = 0; j < 2; ++j) {
      const auto& quad = j == 0 ? insetQuad : outsetQuad;
      const auto& normalQuad = j == 0 ? normalInsetQuad : normalOutsetQuad;
      auto coverage = j == 0 ? 1.0f : 0.0f;
      for (int k = 0; k < 4; ++k) {
        vertices.push_back(quad.point(k).x);
        vertices.push_back(quad.point(k).y);
        vertices.push_back(coverage);
        vertices.push_back(normalQuad.point(k).x);
        vertices.push_back(normalQuad.point(k).y);
        if (!colors.empty()) {
          vertices.push_back(colors[i].red);
          vertices.push_back(colors[i].green);
          vertices.push_back(colors[i].blue);
          vertices.push_back(colors[i].alpha);
        }
      }
    }
  }
  return vertices;
}

std::vector<float> GLFillRectOp::noCoverageVertices() const {
  auto normalBounds = Rect::MakeLTRB(0, 0, 1, 1);
  std::vector<float> vertices;
  for (size_t i = 0; i < rects.size(); ++i) {
    auto quad = Quad::MakeFromRect(rects[i], viewMatrices[i]);
    auto localQuad = Quad::MakeFromRect(normalBounds, localMatrices[i]);
    for (int j = 3; j >= 0; --j) {
      vertices.push_back(quad.point(j).x);
      vertices.push_back(quad.point(j).y);
      vertices.push_back(localQuad.point(j).x);
      vertices.push_back(localQuad.point(j).y);
      if (!colors.empty()) {
        vertices.push_back(colors[i].red);
        vertices.push_back(colors[i].green);
        vertices.push_back(colors[i].blue);
        vertices.push_back(colors[i].alpha);
      }
    }
  }
  return vertices;
}

std::vector<float> GLFillRectOp::vertices(const DrawArgs& args) {
  if (args.aa != AAType::Coverage) {
    return noCoverageVertices();
  } else {
    return coverageVertices();
  }
}

std::unique_ptr<GLFillRectOp> GLFillRectOp::Make(const Rect& rect, const Matrix& viewMatrix) {
  return std::unique_ptr<GLFillRectOp>(new GLFillRectOp({rect}, {viewMatrix}, {Matrix::I()}, {}));
}

std::unique_ptr<GLFillRectOp> GLFillRectOp::Make(const std::vector<Rect>& rects,
                                                 const std::vector<Matrix>& viewMatrices,
                                                 const std::vector<Matrix>& localMatrices,
                                                 const std::vector<Color>& colors) {
  return std::unique_ptr<GLFillRectOp>(
      new GLFillRectOp(rects, viewMatrices, localMatrices, colors));
}

GLFillRectOp::GLFillRectOp(std::vector<Rect> rects, std::vector<Matrix> viewMatrices,
                           std::vector<Matrix> localMatrices, std::vector<Color> colors)
    : rects(std::move(rects)),
      viewMatrices(std::move(viewMatrices)),
      localMatrices(std::move(localMatrices)),
      colors(std::move(colors)) {
  auto bounds = Rect::MakeEmpty();
  for (size_t i = 0; i < this->rects.size(); ++i) {
    bounds.join(this->viewMatrices[i].mapRect(this->rects[i]));
  }
  setBounds(bounds);
}

static constexpr int kVerticesPerNonAAQuad = 4;
static constexpr int kIndicesPerNonAAQuad = 6;
static constexpr int kVerticesPerAAQuad = 8;
static constexpr int kIndicesPerAAQuad = 30;

// clang-format off
static constexpr uint16_t kNonAAQuadIndexPattern[] = {
  0, 1, 2, 2, 1, 3
};

static constexpr uint16_t kAAQuadIndexPattern[] = {
  0, 1, 2, 1, 3, 2,
  0, 4, 1, 4, 5, 1,
  0, 6, 4, 0, 2, 6,
  2, 3, 6, 3, 7, 6,
  1, 5, 3, 3, 5, 7,
};
// clang-format on

void GLFillRectOp::draw(const DrawArgs& args) {
  if (rects.size() > 1 || args.aa == AAType::Coverage) {
    std::vector<uint16_t> indexes;
    static auto kNonAAType = UniqueID::Next();
    static auto kAAType = UniqueID::Next();
    uint32_t type;
    const uint16_t* indexPattern;
    int verticesPerQuad;
    int indexesPerQuad;
    if (args.aa == AAType::Coverage) {
      type = kAAType;
      indexPattern = kAAQuadIndexPattern;
      verticesPerQuad = kVerticesPerAAQuad;
      indexesPerQuad = kIndicesPerAAQuad;
    } else {
      type = kNonAAType;
      indexPattern = kNonAAQuadIndexPattern;
      verticesPerQuad = kVerticesPerNonAAQuad;
      indexesPerQuad = kIndicesPerNonAAQuad;
    }
    for (size_t i = 0; i < rects.size(); ++i) {
      for (int j = 0; j < indexesPerQuad; ++j) {
        indexes.push_back(i * verticesPerQuad + indexPattern[j]);
      }
    }
    auto indexBuffer = GLBuffer::Make(args.context, &indexes[0], indexes.size(), type);
    GLDrawer::DrawIndexBuffer(args.context, indexBuffer);
    return;
  }
  GLDrawer::DrawArrays(args.context, GL_TRIANGLE_STRIP, 0, 4);
}
}  // namespace tgfx
