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

#include "FillRectOp.h"
#include "gpu/Gpu.h"
#include "gpu/Quad.h"
#include "gpu/ResourceProvider.h"
#include "gpu/processors/QuadPerEdgeAAGeometryProcessor.h"
#include "utils/UniqueID.h"

namespace tgfx {
std::vector<float> FillRectOp::coverageVertices() const {
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

    auto normalInsetQuad = Quad::MakeFromRect(insetBounds, localMatrices[i]);
    auto normalOutsetQuad = Quad::MakeFromRect(outsetBounds, localMatrices[i]);

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

std::vector<float> FillRectOp::noCoverageVertices() const {
  std::vector<float> vertices;
  for (size_t i = 0; i < rects.size(); ++i) {
    auto quad = Quad::MakeFromRect(rects[i], viewMatrices[i]);
    auto localQuad = Quad::MakeFromRect(rects[i], localMatrices[i]);
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

std::vector<float> FillRectOp::vertices() {
  if (aa != AAType::Coverage) {
    return noCoverageVertices();
  } else {
    return coverageVertices();
  }
}

std::unique_ptr<FillRectOp> FillRectOp::Make(std::optional<Color> color, const Rect& rect,
                                             const Matrix& viewMatrix, const Matrix& localMatrix) {
  return std::unique_ptr<FillRectOp>(new FillRectOp(color, rect, viewMatrix, localMatrix));
}

FillRectOp::FillRectOp(std::optional<Color> color, const Rect& rect, const Matrix& viewMatrix,
                       const Matrix& localMatrix)
    : DrawOp(ClassID()),
      colors(color ? std::vector<Color>{*color} : std::vector<Color>()),
      rects({rect}),
      viewMatrices({viewMatrix}),
      localMatrices({localMatrix}) {
  auto bounds = Rect::MakeEmpty();
  bounds.join(viewMatrix.mapRect(rect));
  setBounds(bounds);
}

bool FillRectOp::add(std::optional<Color> color, const Rect& rect, const Matrix& viewMatrix,
                     const Matrix& localMatrix) {
  if ((!color && !colors.empty()) || (color && colors.empty()) || !canAdd(1)) {
    return false;
  }
  auto b = bounds();
  b.join(viewMatrix.mapRect(rect));
  setBounds(b);
  rects.emplace_back(rect);
  viewMatrices.emplace_back(viewMatrix);
  localMatrices.emplace_back(localMatrix);
  if (color) {
    colors.push_back(*color);
  }
  return true;
}

bool FillRectOp::canAdd(size_t count) const {
  return rects.size() + count <= static_cast<size_t>(aa == AAType::Coverage
                                                         ? ResourceProvider::MaxNumAAQuads()
                                                         : ResourceProvider::MaxNumNonAAQuads());
}

bool FillRectOp::onCombineIfPossible(Op* op) {
  auto* that = static_cast<FillRectOp*>(op);
  if (colors.empty() != that->colors.empty() || !canAdd(that->rects.size()) ||
      !DrawOp::onCombineIfPossible(op)) {
    return false;
  }
  rects.insert(rects.end(), that->rects.begin(), that->rects.end());
  viewMatrices.insert(viewMatrices.end(), that->viewMatrices.begin(), that->viewMatrices.end());
  localMatrices.insert(localMatrices.end(), that->localMatrices.begin(), that->localMatrices.end());
  colors.insert(colors.end(), that->colors.begin(), that->colors.end());
  return true;
}

bool FillRectOp::needsIndexBuffer() const {
  return rects.size() > 1 || aa == AAType::Coverage;
}

void FillRectOp::onPrepare(Gpu* gpu) {
  auto data = vertices();
  vertexBuffer =
      GpuBuffer::Make(gpu->context(), BufferType::Vertex, data.data(), data.size() * sizeof(float));
  if (vertexBuffer == nullptr) {
    return;
  }
  if (aa == AAType::Coverage) {
    indexBuffer = gpu->context()->resourceProvider()->aaQuadIndexBuffer();
  } else {
    indexBuffer = gpu->context()->resourceProvider()->nonAAQuadIndexBuffer();
  }
}

void FillRectOp::onExecute(RenderPass* renderPass) {
  if (vertexBuffer == nullptr || (needsIndexBuffer() && indexBuffer == nullptr)) {
    return;
  }
  auto pipeline =
      createPipeline(renderPass, QuadPerEdgeAAGeometryProcessor::Make(
                                     renderPass->renderTarget()->width(),
                                     renderPass->renderTarget()->height(), aa, !colors.empty()));
  renderPass->bindProgramAndScissorClip(pipeline.get(), scissorRect());
  renderPass->bindBuffers(indexBuffer, vertexBuffer);
  if (needsIndexBuffer()) {
    uint16_t numIndicesPerQuad;
    if (aa == AAType::Coverage) {
      numIndicesPerQuad = ResourceProvider::NumIndicesPerAAQuad();
    } else {
      numIndicesPerQuad = ResourceProvider::NumIndicesPerNonAAQuad();
    }
    renderPass->drawIndexed(PrimitiveType::Triangles, 0,
                            static_cast<int>(rects.size()) * numIndicesPerQuad);
  } else {
    renderPass->draw(PrimitiveType::TriangleStrip, 0, 4);
  }
}
}  // namespace tgfx
