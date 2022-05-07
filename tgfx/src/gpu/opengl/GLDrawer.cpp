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

#include "GLDrawer.h"
#include "GLBlend.h"
#include "GLProgramBuilder.h"
#include "GLProgramCreator.h"
#include "GLUtil.h"
#include "core/utils/UniqueID.h"
#include "gpu/PorterDuffXferProcessor.h"
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

static bool isDrawArgsValid(const DrawArgs& args) {
  return args.context != nullptr && args.renderTarget != nullptr;
}

static void ComputeRecycleKey(BytesKey* recycleKey) {
  static const uint32_t Type = UniqueID::Next();
  recycleKey->write(Type);
}

void GLDrawer::computeRecycleKey(BytesKey* recycleKey) const {
  ComputeRecycleKey(recycleKey);
}

std::shared_ptr<GLDrawer> GLDrawer::Make(Context* context) {
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey);
  auto drawer =
      std::static_pointer_cast<GLDrawer>(context->resourceCache()->getRecycled(recycleKey));
  if (drawer != nullptr) {
    return drawer;
  }
  drawer = Resource::Wrap(context, new GLDrawer());
  if (!drawer->init(context)) {
    return nullptr;
  }
  return drawer;
}

bool GLDrawer::init(Context* context) {
  CheckGLError(context);
  auto gl = GLFunctions::Get(context);
  auto caps = GLCaps::Get(context);
  if (caps->vertexArrayObjectSupport) {
    // Using VAO is required in the core profile.
    gl->genVertexArrays(1, &vertexArray);
  }
  gl->genBuffers(1, &vertexBuffer);
  return CheckGLError(context);
}

void GLDrawer::onReleaseGPU() {
  auto gl = GLFunctions::Get(context);
  if (vertexArray > 0) {
    gl->deleteVertexArrays(1, &vertexArray);
    vertexArray = 0;
  }
  if (vertexBuffer > 0) {
    gl->deleteBuffers(1, &vertexBuffer);
    vertexBuffer = 0;
  }
}

static std::shared_ptr<Texture> CreateDstTexture(const DrawArgs& args, Rect dstRect,
                                                 Point* dstOffset) {
  auto gl = GLFunctions::Get(args.context);
  auto caps = GLCaps::Get(args.context);
  if (caps->textureBarrierSupport && args.renderTargetTexture) {
    *dstOffset = {0, 0};
    return args.renderTargetTexture;
  }
  auto bounds = Rect::MakeWH(static_cast<float>(args.renderTarget->width()),
                             static_cast<float>(args.renderTarget->height()));
  if (!dstRect.intersect(bounds)) {
    return nullptr;
  }
  dstRect.roundOut();
  *dstOffset = {dstRect.x(), dstRect.y()};
  auto dstTexture =
      GLTexture::MakeRGBA(args.context, static_cast<int>(dstRect.width()),
                          static_cast<int>(dstRect.height()), args.renderTarget->origin());
  if (dstTexture == nullptr) {
    LOGE("Failed to create dst texture(%f*%f).", dstRect.width(), dstRect.height());
    return nullptr;
  }
  auto renderTarget = static_cast<const GLRenderTarget*>(args.renderTarget);
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

static PixelFormat GetOutputPixelFormat(const DrawArgs& args) {
  if (args.renderTargetTexture) {
    return args.renderTargetTexture->getSampler()->format;
  }
  return PixelFormat::RGBA_8888;
}

static void UpdateScissor(Context* context, const DrawArgs& args) {
  auto gl = GLFunctions::Get(context);
  if (args.scissorRect.isEmpty()) {
    gl->disable(GL_SCISSOR_TEST);
  } else {
    gl->enable(GL_SCISSOR_TEST);
    gl->scissor(static_cast<int>(args.scissorRect.x()), static_cast<int>(args.scissorRect.y()),
                static_cast<int>(args.scissorRect.width()),
                static_cast<int>(args.scissorRect.height()));
  }
}

static void UpdateBlend(Context* context, bool blendAsCoeff, unsigned first, unsigned second) {
  auto gl = GLFunctions::Get(context);
  if (blendAsCoeff) {
    gl->enable(GL_BLEND);
    gl->blendFunc(first, second);
    gl->blendEquation(GL_FUNC_ADD);
  } else {
    gl->disable(GL_BLEND);
    auto caps = GLCaps::Get(context);
    if (caps->frameBufferFetchSupport && caps->frameBufferFetchRequiresEnablePerSample) {
      gl->enable(GL_FETCH_PER_SAMPLE_ARM);
    }
  }
}

void GLDrawer::draw(DrawArgs args, std::unique_ptr<GLDrawOp> op) const {
  if (!isDrawArgsValid(args) || op == nullptr) {
    return;
  }
  auto numColorProcessors = args.colors.size();
  std::vector<std::unique_ptr<FragmentProcessor>> fragmentProcessors = {};
  fragmentProcessors.resize(numColorProcessors + args.masks.size());
  std::move(args.colors.begin(), args.colors.end(), fragmentProcessors.begin());
  std::move(args.masks.begin(), args.masks.end(),
            fragmentProcessors.begin() + static_cast<int>(numColorProcessors));
  auto gl = GLFunctions::Get(args.context);
  auto caps = GLCaps::Get(args.context);
  std::unique_ptr<XferProcessor> xferProcessor;
  std::shared_ptr<Texture> dstTexture;
  Point dstTextureOffset = Point::Zero();
  unsigned first = -1;
  unsigned second = -1;
  auto blendAsCoeff = BlendAsCoeff(args.blendMode, &first, &second);
  if (!blendAsCoeff) {
    xferProcessor = PorterDuffXferProcessor::Make(args.blendMode);
    if (!caps->frameBufferFetchSupport) {
      dstTexture = CreateDstTexture(args, op->bounds(), &dstTextureOffset);
    }
  }
  auto config = GetOutputPixelFormat(args);
  const auto& swizzle = caps->getOutputSwizzle(config);
  Pipeline pipeline(std::move(fragmentProcessors), numColorProcessors, std::move(xferProcessor),
                    dstTexture, dstTextureOffset, &swizzle);
  auto geometryProcessor = op->getGeometryProcessor(args);
  GLProgramCreator creator(geometryProcessor.get(), &pipeline);
  auto program = static_cast<GLProgram*>(args.context->programCache()->getProgram(&creator));
  if (program == nullptr) {
    return;
  }
  CheckGLError(args.context);
  auto renderTarget = static_cast<const GLRenderTarget*>(args.renderTarget);
  gl->useProgram(program->programID());
  gl->bindFramebuffer(GL_FRAMEBUFFER, renderTarget->glFrameBuffer().id);
  gl->viewport(0, 0, renderTarget->width(), renderTarget->height());
  UpdateScissor(args.context, args);
  UpdateBlend(args.context, blendAsCoeff, first, second);
  if (pipeline.needsBarrierTexture(args.renderTargetTexture.get())) {
    gl->textureBarrier();
  }
  program->updateUniformsAndTextureBindings(*geometryProcessor, pipeline);
  if (vertexArray > 0) {
    gl->bindVertexArray(vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  auto vertices = op->vertices(args);
  gl->bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size()) * sizeof(float),
                 &vertices[0], GL_STATIC_DRAW);
  for (const auto& attribute : program->vertexAttributes()) {
    const AttribLayout& layout = GetAttribLayout(attribute.gpuType);
    gl->vertexAttribPointer(static_cast<unsigned>(attribute.location), layout.count, layout.type,
                            layout.normalized, program->vertexStride(),
                            reinterpret_cast<void*>(attribute.offset));
    gl->enableVertexAttribArray(static_cast<unsigned>(attribute.location));
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
  op->draw(args);
  if (vertexArray > 0) {
    gl->bindVertexArray(0);
  }
  CheckGLError(args.context);
}

void GLDrawer::DrawIndexBuffer(Context* context, const std::shared_ptr<GLBuffer>& indexBuffer) {
  auto gl = GLFunctions::Get(context);
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->bufferID());
  gl->drawElements(GL_TRIANGLES, static_cast<int>(indexBuffer->length()), GL_UNSIGNED_SHORT,
                   nullptr);
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GLDrawer::DrawArrays(Context* context, unsigned int mode, int first, int count) {
  auto gl = GLFunctions::Get(context);
  gl->drawArrays(mode, first, count);
}
}  // namespace tgfx
