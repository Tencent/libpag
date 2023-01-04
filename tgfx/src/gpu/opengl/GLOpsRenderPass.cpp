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

#include "GLOpsRenderPass.h"
#include "GLGpu.h"
#include "GLProgramCreator.h"
#include "GLUtil.h"
#include "gpu/ProgramCache.h"

namespace tgfx {
struct AttribLayout {
  bool normalized = false;  // Only used by floating point types.
  int count = 0;
  unsigned type = 0;
};

static constexpr std::pair<ShaderVar::Type, AttribLayout> attribLayoutPair[] = {
    {ShaderVar::Type::Float, {false, 1, GL_FLOAT}},
    {ShaderVar::Type::Float2, {false, 2, GL_FLOAT}},
    {ShaderVar::Type::Float3, {false, 3, GL_FLOAT}},
    {ShaderVar::Type::Float4, {false, 4, GL_FLOAT}}};

static AttribLayout GetAttribLayout(ShaderVar::Type type) {
  for (const auto& pair : attribLayoutPair) {
    if (pair.first == type) {
      return pair.second;
    }
  }
  return {false, 0, 0};
}

static void ComputeRecycleKey(BytesKey* recycleKey) {
  static const uint32_t Type = UniqueID::Next();
  recycleKey->write(Type);
}

void GLOpsRenderPass::Vertex::computeRecycleKey(BytesKey* recycleKey) const {
  ComputeRecycleKey(recycleKey);
}

std::unique_ptr<GLOpsRenderPass> GLOpsRenderPass::Make(Context* context) {
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey);
  auto vertex = std::static_pointer_cast<GLOpsRenderPass::Vertex>(
      context->resourceCache()->getRecycled(recycleKey));
  if (vertex == nullptr) {
    vertex = Resource::Wrap(context, new GLOpsRenderPass::Vertex());
    if (!vertex->init(context)) {
      return nullptr;
    }
  }
  return std::unique_ptr<GLOpsRenderPass>(new GLOpsRenderPass(context, vertex));
}

bool GLOpsRenderPass::Vertex::init(Context* context) {
  CheckGLError(context);
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  if (caps->vertexArrayObjectSupport) {
    // Using VAO is required in the core profile.
    gl->genVertexArrays(1, &array);
  }
  gl->genBuffers(1, &buffer);
  return CheckGLError(context);
}

void GLOpsRenderPass::Vertex::onReleaseGPU() {
  auto gl = GLFunctions::Get(context);
  if (array > 0) {
    gl->deleteVertexArrays(1, &array);
    array = 0;
  }
  if (buffer > 0) {
    gl->deleteBuffers(1, &buffer);
    buffer = 0;
  }
}

static void UpdateScissor(Context* context, const Rect& scissorRect) {
  auto gl = GLFunctions::Get(context);
  if (scissorRect.isEmpty()) {
    gl->disable(GL_SCISSOR_TEST);
  } else {
    gl->enable(GL_SCISSOR_TEST);
    gl->scissor(static_cast<int>(scissorRect.x()), static_cast<int>(scissorRect.y()),
                static_cast<int>(scissorRect.width()), static_cast<int>(scissorRect.height()));
  }
}

static const unsigned gXfermodeCoeff2Blend[] = {
    GL_ZERO,      GL_ONE,
    GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
};

static void UpdateBlend(
    Context* context,
    const std::optional<std::pair<BlendModeCoeff, BlendModeCoeff>>& blendFactors) {
  auto gl = GLFunctions::Get(context);
  if (blendFactors.has_value()) {
    gl->enable(GL_BLEND);
    gl->blendFunc(gXfermodeCoeff2Blend[static_cast<int>(blendFactors->first)],
                  gXfermodeCoeff2Blend[static_cast<int>(blendFactors->second)]);
    gl->blendEquation(GL_FUNC_ADD);
  } else {
    gl->disable(GL_BLEND);
    auto caps = GLCaps::Get(context);
    if (caps->frameBufferFetchSupport && caps->frameBufferFetchRequiresEnablePerSample) {
      gl->enable(GL_FETCH_PER_SAMPLE_ARM);
    }
  }
}

void GLOpsRenderPass::set(std::shared_ptr<RenderTarget> renderTarget,
                          std::shared_ptr<Texture> renderTargetTexture) {
  _renderTarget = std::move(renderTarget);
  _renderTargetTexture = std::move(renderTargetTexture);
}

void GLOpsRenderPass::reset() {
  program = nullptr;
  _vertices = {};
  _indexBuffer = nullptr;
  _renderTarget = nullptr;
  _renderTargetTexture = nullptr;
  _vertexBuffer = nullptr;
}

void GLOpsRenderPass::bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds) {
  GLProgramCreator creator(info.geometryProcessor.get(), info.pipeline.get());
  program = static_cast<GLProgram*>(_context->programCache()->getProgram(&creator));
  if (program == nullptr) {
    return;
  }
  auto gl = GLFunctions::Get(_context);
  CheckGLError(_context);
  auto glRT = static_cast<GLRenderTarget*>(_renderTarget.get());
  gl->useProgram(program->programID());
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRT->glFrameBuffer().id);
  gl->viewport(0, 0, glRT->width(), glRT->height());
  UpdateScissor(_context, drawBounds);
  UpdateBlend(_context, info.blendFactors);
  if (info.pipeline->requiresBarrier()) {
    gl->textureBarrier();
  }
  program->updateUniformsAndTextureBindings(glRT, *info.geometryProcessor, *info.pipeline);
}

void GLOpsRenderPass::bindVertexBuffer(std::shared_ptr<GpuBuffer> vertexBuffer) {
  _vertexBuffer = std::static_pointer_cast<GLBuffer>(vertexBuffer);
  _vertices = {};
  _indexBuffer = nullptr;
}

void GLOpsRenderPass::bindVerticesAndIndices(std::vector<float> vertices,
                                             std::shared_ptr<GpuBuffer> indices) {
  _vertices = std::move(vertices);
  _indexBuffer = std::static_pointer_cast<GLBuffer>(indices);
  _vertexBuffer = nullptr;
}

static const unsigned gPrimitiveType[] = {GL_TRIANGLES, GL_TRIANGLE_STRIP};

void GLOpsRenderPass::draw(PrimitiveType primitiveType, int baseVertex, int vertexCount) {
  auto func = [&]() {
    auto gl = GLFunctions::Get(_context);
    gl->drawArrays(gPrimitiveType[static_cast<int>(primitiveType)], baseVertex, vertexCount);
  };
  draw(func);
}

void GLOpsRenderPass::drawIndexed(PrimitiveType primitiveType, int baseIndex, int indexCount) {
  auto func = [&]() {
    auto gl = GLFunctions::Get(_context);
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer->bufferID());
    gl->drawElements(gPrimitiveType[static_cast<int>(primitiveType)], indexCount, GL_UNSIGNED_SHORT,
                     reinterpret_cast<void*>(baseIndex * sizeof(uint16_t)));
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  };
  draw(func);
}

void GLOpsRenderPass::draw(const std::function<void()>& func) {
  auto gl = GLFunctions::Get(_context);
  if (vertex->array > 0) {
    gl->bindVertexArray(vertex->array);
  }
  auto resetVertexObject = [&]() {
    if (vertex->array > 0) {
      gl->bindVertexArray(0);
    }
    gl->bindBuffer(GL_ARRAY_BUFFER, 0);
  };
  if (_vertexBuffer) {
    gl->bindBuffer(GL_ARRAY_BUFFER, _vertexBuffer->bufferID());
  } else {
    gl->bindBuffer(GL_ARRAY_BUFFER, vertex->buffer);
    gl->bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(_vertices.size()) * sizeof(float),
                   &_vertices[0], GL_STATIC_DRAW);
    if (!CheckGLError(_context)) {
      resetVertexObject();
      return;
    }
  }
  for (const auto& attribute : program->vertexAttributes()) {
    const AttribLayout& layout = GetAttribLayout(attribute.gpuType);
    gl->vertexAttribPointer(static_cast<unsigned>(attribute.location), layout.count, layout.type,
                            layout.normalized, program->vertexStride(),
                            reinterpret_cast<void*>(attribute.offset));
    gl->enableVertexAttribArray(static_cast<unsigned>(attribute.location));
  }
  func();
  resetVertexObject();
  CheckGLError(_context);
}

void GLOpsRenderPass::clear(const Rect& scissor, Color color) {
  auto gl = GLFunctions::Get(_context);
  auto glRT = static_cast<GLRenderTarget*>(_renderTarget.get());
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRT->glFrameBuffer().id);
  gl->viewport(0, 0, glRT->width(), glRT->height());
  UpdateScissor(_context, scissor);
  gl->clearColor(color.red, color.green, color.blue, color.alpha);
  gl->clear(GL_COLOR_BUFFER_BIT);
}
}  // namespace tgfx
