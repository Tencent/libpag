/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "EffectFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Canvas.h"

namespace pag {
static constexpr char VERTEX_SHADER[] = R"(
    #version 100
    attribute vec2 aPosition;
    attribute vec2 aTextureCoord;
    varying vec2 vertexColor;
    void main() {
    vec3 position = vec3(aPosition, 1);
    gl_Position = vec4(position.xy, 0, 1);
    vertexColor = aTextureCoord;
    }
)";

static constexpr char FRAGMENT_SHADER[] = R"(
    #version 100
    precision mediump float;
    varying vec2 vertexColor;
    uniform sampler2D sTexture;

    void main() {
        gl_FragColor = texture2D(sTexture, vertexColor);
    }
)";

std::unique_ptr<EffectProgram> EffectProgram::Make(tgfx::Context* context,
                                                   const std::string& vertex,
                                                   const std::string& fragment) {
  auto gl = tgfx::GLFunctions::Get(context);
  auto program = CreateGLProgram(context, vertex, fragment);
  if (program == 0) {
    return nullptr;
  }
  auto filterProgram = std::unique_ptr<EffectProgram>(new EffectProgram(context));
  filterProgram->program = program;
  if (gl->bindVertexArray != nullptr) {
    gl->genVertexArrays(1, &filterProgram->vertexArray);
  }
  gl->genBuffers(1, &filterProgram->vertexBuffer);
  return filterProgram;
}

void EffectProgram::onReleaseGPU() {
  auto gl = tgfx::GLFunctions::Get(getContext());
  if (program > 0) {
    gl->deleteProgram(program);
    program = 0;
  }
  if (vertexArray > 0) {
    gl->deleteVertexArrays(1, &vertexArray);
    vertexArray = 0;
  }
  if (vertexBuffer > 0) {
    gl->deleteBuffers(1, &vertexBuffer);
    vertexBuffer = 0;
  }
}

std::unique_ptr<tgfx::RuntimeProgram> FilterEffect::onCreateProgram(tgfx::Context* context) const {
  // 防止前面产生的GLError，导致后面CheckGLError逻辑返回错误结果
  CheckGLError(context);

  auto vertex = onBuildVertexShader();
  auto fragment = onBuildFragmentShader();
  auto filterProgram = EffectProgram::Make(context, vertex, fragment);
  if (filterProgram == nullptr) {
    return nullptr;
  }
  filterProgram->uniforms = onPrepareProgram(context, filterProgram->program);
  if (!CheckGLError(context)) {
    return nullptr;
  }
  return filterProgram;
}

std::string FilterEffect::onBuildVertexShader() const {
  return VERTEX_SHADER;
}

std::string FilterEffect::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> FilterEffect::onPrepareProgram(tgfx::Context* context,
                                                         unsigned program) const {
  return std::make_unique<Uniforms>(context, program);
}

void FilterEffect::onUpdateParams(tgfx::Context*, const EffectProgram*,
                                  const std::vector<tgfx::BackendTexture>&) const {
}

static void EnableMultisample(tgfx::Context* context, bool usesMSAA) {
  if (usesMSAA && context->caps()->multisampleDisableSupport) {
    auto gl = tgfx::GLFunctions::Get(context);
    gl->enable(GL_MULTISAMPLE);
  }
}

static void DisableMultisample(tgfx::Context* context, bool usesMSAA) {
  if (usesMSAA && context->caps()->multisampleDisableSupport) {
    auto gl = tgfx::GLFunctions::Get(context);
    gl->disable(GL_MULTISAMPLE);
  }
}

bool FilterEffect::onDraw(const tgfx::RuntimeProgram* program,
                          const std::vector<tgfx::BackendTexture>& sources,
                          const tgfx::BackendRenderTarget& target,
                          const tgfx::Point& offset) const {
  if (sources.empty() || !target.isValid() || program == nullptr ||
      sources[0].backend() != tgfx::Backend::OPENGL || target.backend() != tgfx::Backend::OPENGL) {
    LOGE(
        "LayerFilter::draw() can not draw filter, "
        "because the argument(source/target) is null");
    return false;
  }
  tgfx::GLFrameBufferInfo targetInfo;
  target.getGLFramebufferInfo(&targetInfo);
  auto context = program->getContext();
  // Clear the previously generated GLError
  CheckGLError(context);
  auto needsMSAA = sampleCount() > 1;
  EnableMultisample(context, needsMSAA);
  auto gl = tgfx::GLFunctions::Get(context);
  auto filterProgram = static_cast<const EffectProgram*>(program);
  gl->useProgram(filterProgram->program);
  gl->disable(GL_SCISSOR_TEST);
  gl->enable(GL_BLEND);
  gl->blendEquation(GL_FUNC_ADD);
  gl->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl->bindFramebuffer(GL_FRAMEBUFFER, targetInfo.id);
  gl->viewport(0, 0, target.width(), target.height());
  for (size_t i = 0; i < sources.size(); i++) {
    tgfx::GLTextureInfo sourceInfo;
    sources[i].getGLTextureInfo(&sourceInfo);
    ActiveGLTexture(context, i, &sourceInfo);
  }
  onUpdateParams(context, filterProgram, sources);
  auto vertices = computeVertices(sources, target, offset);
  bindVertices(context, filterProgram, vertices);
  gl->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(0);
  }
  DisableMultisample(context, needsMSAA);
  return CheckGLError(context);
}

std::vector<float> FilterEffect::computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                                 const tgfx::BackendRenderTarget& target,
                                                 const tgfx::Point& offset) const {
  std::vector<float> vertices = {};
  auto textureWidth = static_cast<float>(sources[0].width());
  auto textureHeight = static_cast<float>(sources[0].height());
  auto targetWidth = static_cast<float>(target.width());
  auto targetHeight = static_cast<float>(target.height());
  tgfx::Point contentPoint[4] = {{0, targetHeight},
                                 {targetWidth, targetHeight},
                                 {0, 0},
                                 {targetWidth, 0}};
  tgfx::Point texturePoints[4] = {{0.0f, textureHeight},
                                  {textureWidth, textureHeight},
                                  {0.0f, 0.0f},
                                  {textureWidth, 0.0f}};

  for (size_t i = 0; i < 4; i++) {
    auto vertexPoint = ToGLVertexPoint(target, contentPoint[i] + offset);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    auto texturePoint = ToGLTexturePoint(&sources[0], texturePoints[i]);
    vertices.push_back(texturePoint.x);
    vertices.push_back(texturePoint.y);
  }
  return vertices;
}

void FilterEffect::bindVertices(tgfx::Context* context, const EffectProgram* filterProgram,
                                const std::vector<float>& points) const {

  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = filterProgram->uniforms.get();
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(filterProgram->vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, filterProgram->vertexBuffer);
  gl->bufferData(GL_ARRAY_BUFFER, static_cast<tgfx::GLsizeiptr>(points.size() * sizeof(float)),
                 points.data(), GL_STREAM_DRAW);
  gl->vertexAttribPointer(static_cast<unsigned>(uniform->positionHandle), 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), static_cast<void*>(0));
  gl->enableVertexAttribArray(static_cast<unsigned>(uniform->positionHandle));

  gl->vertexAttribPointer(uniform->textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
  gl->enableVertexAttribArray(uniform->textureCoordHandle);
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
}

std::shared_ptr<tgfx::RuntimeEffect> EffectFilter::onCreateEffect(Frame, const tgfx::Point&) const {
  return nullptr;
}

bool EffectFilter::draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source,
                        const tgfx::Point& filterScale, const tgfx::Matrix& matrix,
                        tgfx::Canvas* target) {
  auto offset = tgfx::Point::Zero();
  auto image = applyFilterEffect(layerFrame, source, filterScale, &offset);
  if (image == nullptr) {
    return false;
  }
  auto totalMatrix = matrix;
  totalMatrix.preTranslate(offset.x, offset.y);
  target->drawImage(image, totalMatrix);
  return true;
}

std::shared_ptr<tgfx::Image> EffectFilter::applyFilterEffect(Frame layerFrame,
                                                             std::shared_ptr<tgfx::Image> source,
                                                             const tgfx::Point& filterScale,
                                                             tgfx::Point* offset) {
  auto effect = onCreateEffect(layerFrame, filterScale);
  if (effect == nullptr) {
    return nullptr;
  }

  auto filter = tgfx::ImageFilter::Runtime(effect);
  if (filter == nullptr) {
    return nullptr;
  }
  return source->makeWithFilter(filter, offset);
}

}  // namespace pag
