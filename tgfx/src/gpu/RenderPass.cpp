/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "RenderPass.h"

namespace tgfx {
void RenderPass::begin() {
  drawPipelineStatus = DrawPipelineStatus::NotConfigured;
}

void RenderPass::end() {
  resetActiveBuffers();
}

void RenderPass::resetActiveBuffers() {
  _program = nullptr;
  _indexBuffer = nullptr;
  _vertexBuffer = nullptr;
}

void RenderPass::bindProgramAndScissorClip(const ProgramInfo* programInfo, const Rect& drawBounds) {
  resetActiveBuffers();
  if (!onBindProgramAndScissorClip(programInfo, drawBounds)) {
    drawPipelineStatus = DrawPipelineStatus::FailedToBind;
    return;
  }
  drawPipelineStatus = DrawPipelineStatus::Ok;
}

void RenderPass::bindBuffers(std::shared_ptr<GpuBuffer> indexBuffer,
                             std::shared_ptr<GpuBuffer> vertexBuffer) {
  if (drawPipelineStatus != DrawPipelineStatus::Ok) {
    return;
  }
  onBindBuffers(std::move(indexBuffer), std::move(vertexBuffer));
}

void RenderPass::draw(PrimitiveType primitiveType, int baseVertex, int vertexCount) {
  if (drawPipelineStatus != DrawPipelineStatus::Ok) {
    return;
  }
  onDraw(primitiveType, baseVertex, vertexCount);
}

void RenderPass::drawIndexed(PrimitiveType primitiveType, int baseIndex, int indexCount) {
  if (drawPipelineStatus != DrawPipelineStatus::Ok) {
    return;
  }
  onDrawIndexed(primitiveType, baseIndex, indexCount);
}

void RenderPass::clear(const Rect& scissor, Color color) {
  drawPipelineStatus = DrawPipelineStatus::NotConfigured;
  onClear(scissor, color);
}
}  // namespace tgfx
