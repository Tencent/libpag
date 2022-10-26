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
#include "GLBlend.h"
#include "GLGpu.h"
#include "GLProgramBuilder.h"
#include "GLProgramCreator.h"
#include "GLUtil.h"
#include "core/utils/UniqueID.h"
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

static std::shared_ptr<Texture> CreateDstTexture(OpsRenderPass* opsRenderPass, Rect dstRect,
                                                 Point* dstOffset) {
  auto gl = GLFunctions::Get(opsRenderPass->context());
  auto caps = GLCaps::Get(opsRenderPass->context());
  if (caps->textureBarrierSupport && opsRenderPass->renderTargetTexture()) {
    *dstOffset = {0, 0};
    return opsRenderPass->renderTargetTexture();
  }
  auto bounds = Rect::MakeWH(static_cast<float>(opsRenderPass->renderTarget()->width()),
                             static_cast<float>(opsRenderPass->renderTarget()->height()));
  if (opsRenderPass->renderTarget()->origin() == ImageOrigin::BottomLeft) {
    auto height = dstRect.height();
    dstRect.top = static_cast<float>(opsRenderPass->renderTarget()->height()) - dstRect.bottom;
    dstRect.bottom = dstRect.top + height;
  }
  if (!dstRect.intersect(bounds)) {
    return nullptr;
  }
  dstRect.roundOut();
  *dstOffset = {dstRect.x(), dstRect.y()};
  auto dstTexture = GLTexture::MakeRGBA(opsRenderPass->context(), static_cast<int>(dstRect.width()),
                                        static_cast<int>(dstRect.height()),
                                        opsRenderPass->renderTarget()->origin());
  if (dstTexture == nullptr) {
    LOGE("Failed to create dst texture(%f*%f).", dstRect.width(), dstRect.height());
    return nullptr;
  }
  auto renderTarget = std::static_pointer_cast<GLRenderTarget>(opsRenderPass->renderTarget());
  gl->bindFramebuffer(GL_FRAMEBUFFER, renderTarget->glFrameBuffer().id);
  auto glSampler = std::static_pointer_cast<GLTexture>(dstTexture)->glSampler();
  gl->bindTexture(glSampler.target, glSampler.id);
  // format != BGRA && !srcHasMSAARenderBuffer && !dstHasMSAARenderBuffer && dstIsTextureable &&
  // dstOrigin == srcOrigin && canConfigBeFBOColorAttachment(srcConfig) && (!srcIsTextureable ||
  // srcIsGLTexture2D)
  gl->copyTexSubImage2D(glSampler.target, 0, 0, 0, static_cast<int>(dstRect.x()),
                        static_cast<int>(dstRect.y()), static_cast<int>(dstRect.width()),
                        static_cast<int>(dstRect.height()));
  return dstTexture;
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

static void UpdateBlend(Context* context,
                        const std::optional<std::pair<unsigned, unsigned>>& blendFactors) {
  auto gl = GLFunctions::Get(context);
  if (blendFactors.has_value()) {
    gl->enable(GL_BLEND);
    gl->blendFunc(blendFactors->first, blendFactors->second);
    gl->blendEquation(GL_FUNC_ADD);
  } else {
    gl->disable(GL_BLEND);
    auto caps = GLCaps::Get(context);
    if (caps->frameBufferFetchSupport && caps->frameBufferFetchRequiresEnablePerSample) {
      gl->enable(GL_FETCH_PER_SAMPLE_ARM);
    }
  }
}

ProgramInfo GLDrawOp::createProgram(OpsRenderPass* opsRenderPass,
                                    std::unique_ptr<GeometryProcessor> gp) {
  auto numColorProcessors = _colors.size();
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors = {};
  fragmentProcessors.resize(numColorProcessors + _masks.size());
  std::move(_colors.begin(), _colors.end(), fragmentProcessors.begin());
  std::move(_masks.begin(), _masks.end(),
            fragmentProcessors.begin() + static_cast<int>(numColorProcessors));
  std::shared_ptr<Texture> dstTexture;
  Point dstTextureOffset = Point::Zero();
  ProgramInfo info;
  info.blendFactors = _blendFactors;
  if (_requiresDstTexture) {
    dstTexture = CreateDstTexture(opsRenderPass, bounds(), &dstTextureOffset);
  }
  auto config = std::static_pointer_cast<GLRenderTarget>(opsRenderPass->renderTarget())
                    ->glFrameBuffer()
                    .format;
  const auto& swizzle = GLCaps::Get(opsRenderPass->context())->getOutputSwizzle(config);
  info.pipeline =
      std::make_unique<Pipeline>(std::move(fragmentProcessors), numColorProcessors,
                                 std::move(_xferProcessor), dstTexture, dstTextureOffset, &swizzle);
  info.pipeline->setRequiresBarrier(dstTexture != nullptr &&
                                    dstTexture == opsRenderPass->renderTargetTexture());
  info.geometryProcessor = std::move(gp);
  GLProgramCreator creator(info.geometryProcessor.get(), info.pipeline.get());
  info.program = opsRenderPass->context()->programCache()->getProgram(&creator);
  return info;
}

static bool CompareFragments(const std::vector<std::unique_ptr<FragmentProcessor>>& frags1,
                             const std::vector<std::unique_ptr<FragmentProcessor>>& frags2) {
  if (frags1.size() != frags2.size()) {
    return false;
  }
  for (size_t i = 0; i < frags1.size(); i++) {
    if (!frags1[i]->isEqual(*frags2[i])) {
      return false;
    }
  }
  return true;
}

bool GLDrawOp::onCombineIfPossible(Op* op) {
  auto* that = static_cast<GLDrawOp*>(op);
  return aa == that->aa && _scissorRect == that->_scissorRect &&
         _blendFactors == that->_blendFactors && _requiresDstTexture == that->_requiresDstTexture &&
         CompareFragments(_colors, that->_colors) && CompareFragments(_masks, that->_masks) &&
         _xferProcessor == that->_xferProcessor;
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
}

void GLOpsRenderPass::bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds) {
  if (info.program == nullptr) {
    return;
  }
  program = static_cast<GLProgram*>(info.program);
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

void GLOpsRenderPass::bindVerticesAndIndices(std::vector<float> vertices,
                                             std::shared_ptr<Resource> indices) {
  _vertices = std::move(vertices);
  _indexBuffer = std::static_pointer_cast<GLBuffer>(indices);
}

void GLOpsRenderPass::draw(unsigned primitiveType, int baseVertex, int vertexCount) {
  draw([&]() {
    auto gl = GLFunctions::Get(_context);
    gl->drawArrays(primitiveType, baseVertex, vertexCount);
  });
}

void GLOpsRenderPass::drawIndexed(unsigned primitiveType, int baseIndex, int indexCount) {
  draw([&]() {
    auto gl = GLFunctions::Get(_context);
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuffer->bufferID());
    gl->drawElements(primitiveType, indexCount, GL_UNSIGNED_SHORT,
                     reinterpret_cast<void*>(baseIndex * sizeof(uint16_t)));
    gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  });
}

void GLOpsRenderPass::draw(std::function<void()> func) {
  auto gl = GLFunctions::Get(_context);
  if (vertex->array > 0) {
    gl->bindVertexArray(vertex->array);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, vertex->buffer);
  auto resetVertexObject = [&]() {
    if (vertex->array > 0) {
      gl->bindVertexArray(0);
    }
    gl->bindBuffer(GL_ARRAY_BUFFER, 0);
  };
  gl->bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(_vertices.size()) * sizeof(float),
                 &_vertices[0], GL_STATIC_DRAW);
  if (!CheckGLError(_context)) {
    resetVertexObject();
    return;
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
