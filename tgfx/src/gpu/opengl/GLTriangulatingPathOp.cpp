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

#include "GLTriangulatingPathOp.h"
#include "core/PathRef.h"
#include "gpu/DefaultGeometryProcessor.h"
#include "tgfx/core/Mesh.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;

std::unique_ptr<GLTriangulatingPathOp> GLTriangulatingPathOp::Make(Color color, const Path& path,
                                                                   Rect clipBounds,
                                                                   const Matrix& localMatrix) {
  const auto& skPath = PathRef::ReadAccess(path);
  if (skPath.countVerbs() > AA_TESSELLATOR_MAX_VERB_COUNT) {
    return nullptr;
  }
  std::vector<float> vertices;
  auto skRect =
      pk::SkRect::MakeLTRB(clipBounds.left, clipBounds.top, clipBounds.right, clipBounds.bottom);
  int count = skPath.toAATriangles(DefaultTolerance, skRect, &vertices);
  if (count == 0) {
    return nullptr;
  }
  return std::make_unique<GLTriangulatingPathOp>(color,
                                                 std::make_shared<BufferProvider>(vertices, count),
                                                 path.getBounds(), Matrix::I(), localMatrix);
}

GLTriangulatingPathOp::GLTriangulatingPathOp(Color color,
                                             std::shared_ptr<BufferProvider> bufferProvider,
                                             Rect bounds, const Matrix& viewMatrix,
                                             const Matrix& localMatrix)
    : GLDrawOp(ClassID()),
      color(color),
      providers({std::move(bufferProvider)}),
      viewMatrix(viewMatrix),
      localMatrix(localMatrix) {
  setBounds(bounds);
}

bool GLTriangulatingPathOp::onCombineIfPossible(Op* op) {
  if (!GLDrawOp::onCombineIfPossible(op)) {
    return false;
  }
  auto* that = static_cast<GLTriangulatingPathOp*>(op);
  if (viewMatrix != that->viewMatrix || localMatrix != that->localMatrix || color != that->color ||
      !providers[0]->canCombine(that->providers[0].get())) {
    return false;
  }
  providers.insert(providers.end(), that->providers.begin(), that->providers.end());
  return true;
}

void GLTriangulatingPathOp::execute(OpsRenderPass* opsRenderPass) {
  auto info = createProgram(
      opsRenderPass, DefaultGeometryProcessor::Make(color, opsRenderPass->renderTarget()->width(),
                                                    opsRenderPass->renderTarget()->height(),
                                                    viewMatrix, localMatrix));
  opsRenderPass->bindPipelineAndScissorClip(info, scissorRect());
  int vertexCount = 0;
  if (auto buffer = providers[0]->getGpuBuffer(opsRenderPass->context())) {
    opsRenderPass->bindVertexBuffer(std::move(buffer));
    vertexCount = providers[0]->vertexCount();
  } else {
    std::vector<float> vertices;
    for (const auto& provider : providers) {
      vertices.insert(vertices.end(), provider->vertices().begin(), provider->vertices().end());
      vertexCount += provider->vertexCount();
    }
    opsRenderPass->bindVerticesAndIndices(std::move(vertices), nullptr);
  }
  opsRenderPass->draw(GL_TRIANGLES, 0, vertexCount);
}
}  // namespace tgfx
