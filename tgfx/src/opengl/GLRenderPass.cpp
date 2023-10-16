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

#include "GLRenderPass.h"
#include "GLGpu.h"
#include "GLUtil.h"
#include "gpu/ProgramCache.h"

namespace tgfx {
struct AttribLayout {
  bool normalized = false;  // Only used by floating point types.
  int count = 0;
  unsigned type = 0;
};

static constexpr std::pair<SLType, AttribLayout> attribLayoutPair[] = {
    {SLType::Float, {false, 1, GL_FLOAT}},  {SLType::Float2, {false, 2, GL_FLOAT}},
    {SLType::Float3, {false, 3, GL_FLOAT}}, {SLType::Float4, {false, 4, GL_FLOAT}},
    {SLType::Int, {false, 1, GL_INT}},      {SLType::Int2, {false, 2, GL_INT}},
    {SLType::Int3, {false, 3, GL_INT}},     {SLType::Int4, {false, 4, GL_INT}}};

static AttribLayout GetAttribLayout(SLType type) {
  for (const auto& pair : attribLayoutPair) {
    if (pair.first == type) {
      return pair.second;
    }
  }
  return {false, 0, 0};
}

class VertexArrayObject : public Resource {
 public:
  static std::shared_ptr<VertexArrayObject> Make(Context* context) {
    auto gl = GLFunctions::Get(context);
    unsigned id = 0;
    // Using VAO is required in the core profile.
    gl->genVertexArrays(1, &id);
    if (id == 0) {
      return nullptr;
    }
    return Resource::Wrap(context, new VertexArrayObject(id));
  }

  explicit VertexArrayObject(unsigned id) : id(id) {
  }

  size_t memoryUsage() const override {
    return 0;
  }

  unsigned id = 0;

 private:
  void onReleaseGPU() override {
    auto gl = GLFunctions::Get(context);
    if (id > 0) {
      gl->deleteVertexArrays(1, &id);
      id = 0;
    }
  }
};

std::unique_ptr<GLRenderPass> GLRenderPass::Make(Context* context) {
  std::shared_ptr<VertexArrayObject> vertexArrayObject;
  if (GLCaps::Get(context)->vertexArrayObjectSupport) {
    vertexArrayObject = VertexArrayObject::Make(context);
    if (vertexArrayObject == nullptr) {
      return nullptr;
    }
  }
  return std::unique_ptr<GLRenderPass>(new GLRenderPass(context, vertexArrayObject));
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

static void UpdateBlend(Context* context, const BlendInfo* blendFactors) {
  auto gl = GLFunctions::Get(context);
  if (blendFactors != nullptr) {
    gl->enable(GL_BLEND);
    gl->blendFunc(gXfermodeCoeff2Blend[static_cast<int>(blendFactors->srcBlend)],
                  gXfermodeCoeff2Blend[static_cast<int>(blendFactors->dstBlend)]);
    gl->blendEquation(GL_FUNC_ADD);
  } else {
    gl->disable(GL_BLEND);
    auto caps = GLCaps::Get(context);
    if (caps->frameBufferFetchSupport && caps->frameBufferFetchRequiresEnablePerSample) {
      gl->enable(GL_FETCH_PER_SAMPLE_ARM);
    }
  }
}

void GLRenderPass::set(std::shared_ptr<RenderTarget> renderTarget,
                       std::shared_ptr<Texture> renderTargetTexture) {
  _renderTarget = std::move(renderTarget);
  _renderTargetTexture = std::move(renderTargetTexture);
}

void GLRenderPass::reset() {
  _renderTarget = nullptr;
  _renderTargetTexture = nullptr;
}

bool GLRenderPass::onBindProgramAndScissorClip(const ProgramInfo* programInfo,
                                               const Rect& drawBounds) {
  _program = static_cast<GLProgram*>(_context->programCache()->getProgram(programInfo));
  if (_program == nullptr) {
    return false;
  }
  auto gl = GLFunctions::Get(_context);
  CheckGLError(_context);
  auto glRT = static_cast<GLRenderTarget*>(_renderTarget.get());
  auto* program = static_cast<GLProgram*>(_program);
  gl->useProgram(program->programID());
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRT->getFrameBufferID());
  gl->viewport(0, 0, glRT->width(), glRT->height());
  UpdateScissor(_context, drawBounds);
  UpdateBlend(_context, programInfo->blendInfo());
  if (programInfo->requiresBarrier()) {
    gl->textureBarrier();
  }
  program->updateUniformsAndTextureBindings(glRT, programInfo);
  return true;
}

void GLRenderPass::onBindBuffers(std::shared_ptr<GpuBuffer> indexBuffer,
                                 std::shared_ptr<GpuBuffer> vertexBuffer) {
  _indexBuffer = std::move(indexBuffer);
  _vertexBuffer = std::move(vertexBuffer);
}

static const unsigned gPrimitiveType[] = {GL_TRIANGLES, GL_TRIANGLE_STRIP};

void GLRenderPass::onDraw(PrimitiveType primitiveType, int baseVertex, int vertexCount) {
  auto func = [&]() {
    auto gl = GLFunctions::Get(_context);
    gl->drawArrays(gPrimitiveType[static_cast<int>(primitiveType)], baseVertex, vertexCount);
  };
  draw(func);
}

void GLRenderPass::onDrawIndexed(PrimitiveType primitiveType, int baseIndex, int indexCount) {
  auto func = [&]() {
    auto gl = GLFunctions::Get(_context);
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                   std::static_pointer_cast<GLBuffer>(_indexBuffer)->bufferID());
    gl->drawElements(gPrimitiveType[static_cast<int>(primitiveType)], indexCount, GL_UNSIGNED_SHORT,
                     reinterpret_cast<void*>(baseIndex * sizeof(uint16_t)));
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  };
  draw(func);
}

void GLRenderPass::draw(const std::function<void()>& func) {
  auto gl = GLFunctions::Get(_context);
  if (vertexArrayObject) {
    gl->bindVertexArray(vertexArrayObject->id);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, std::static_pointer_cast<GLBuffer>(_vertexBuffer)->bufferID());
  auto* program = static_cast<GLProgram*>(_program);
  for (const auto& attribute : program->vertexAttributes()) {
    const AttribLayout& layout = GetAttribLayout(attribute.gpuType);
    gl->vertexAttribPointer(static_cast<unsigned>(attribute.location), layout.count, layout.type,
                            layout.normalized, program->vertexStride(),
                            reinterpret_cast<void*>(attribute.offset));
    gl->enableVertexAttribArray(static_cast<unsigned>(attribute.location));
  }
  func();
  if (vertexArrayObject) {
    gl->bindVertexArray(0);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
  CheckGLError(_context);
}

void GLRenderPass::onClear(const Rect& scissor, Color color) {
  auto gl = GLFunctions::Get(_context);
  auto glRT = static_cast<GLRenderTarget*>(_renderTarget.get());
  gl->bindFramebuffer(GL_FRAMEBUFFER, glRT->getFrameBufferID());
  gl->viewport(0, 0, glRT->width(), glRT->height());
  UpdateScissor(_context, scissor);
  gl->clearColor(color.red, color.green, color.blue, color.alpha);
  gl->clear(GL_COLOR_BUFFER_BIT);
}
}  // namespace tgfx
