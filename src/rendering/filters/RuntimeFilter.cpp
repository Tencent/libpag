/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "RuntimeFilter.h"
#include "base/utils/Log.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
static constexpr char VERTEX_SHADER[] = R"(
    #version 100
    attribute vec2 aPosition;
    attribute vec2 aTextureCoord;
    varying vec2 vertexColor;
    void main() {
      gl_Position = vec4(aPosition.xy, 0, 1);
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

std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{outputBounds.left, outputBounds.bottom},
                                 {outputBounds.right, outputBounds.bottom},
                                 {outputBounds.left, outputBounds.top},
                                 {outputBounds.right, outputBounds.top}};
  auto deltaX = outputBounds.left - inputBounds.left;
  auto deltaY = outputBounds.top - inputBounds.top;
  tgfx::Point texturePoints[4] = {
      {deltaX, (outputBounds.height() + deltaY)},
      {(outputBounds.width() + deltaX), (outputBounds.height() + deltaY)},
      {deltaX, deltaY},
      {(outputBounds.width() + deltaX), deltaY}};
  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}

std::unique_ptr<RuntimeProgram> RuntimeProgram::Make(tgfx::Context* context,
                                                     const std::string& vertex,
                                                     const std::string& fragment) {
  auto gl = tgfx::GLFunctions::Get(context);
  auto program = CreateGLProgram(context, vertex, fragment);
  if (program == 0) {
    return nullptr;
  }
  auto filterProgram = std::unique_ptr<RuntimeProgram>(new RuntimeProgram(context));
  filterProgram->program = program;
  if (gl->bindVertexArray != nullptr) {
    gl->genVertexArrays(1, &filterProgram->vertexArray);
  }
  gl->genBuffers(1, &filterProgram->vertexBuffer);
  return filterProgram;
}

void RuntimeProgram::onReleaseGPU() {
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

std::unique_ptr<tgfx::RuntimeProgram> RuntimeFilter::onCreateProgram(tgfx::Context* context) const {
  // 防止前面产生的GLError，导致后面CheckGLError逻辑返回错误结果
  CheckGLError(context);

  auto vertex = onBuildVertexShader();
  auto fragment = onBuildFragmentShader();
  auto filterProgram = RuntimeProgram::Make(context, vertex, fragment);
  if (filterProgram == nullptr) {
    return nullptr;
  }
  filterProgram->uniforms = onPrepareProgram(context, filterProgram->program);
  if (!CheckGLError(context)) {
    return nullptr;
  }
  return filterProgram;
}

std::string RuntimeFilter::onBuildVertexShader() const {
  return VERTEX_SHADER;
}

std::string RuntimeFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> RuntimeFilter::onPrepareProgram(tgfx::Context* context,
                                                          unsigned program) const {
  return std::make_unique<Uniforms>(context, program);
}

void RuntimeFilter::onUpdateParams(tgfx::Context*, const RuntimeProgram*,
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

bool RuntimeFilter::onDraw(const tgfx::RuntimeProgram* program,
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
  auto filterProgram = static_cast<const RuntimeProgram*>(program);
  gl->useProgram(filterProgram->program);
  gl->disable(GL_SCISSOR_TEST);
  gl->enable(GL_BLEND);
  gl->blendEquation(GL_FUNC_ADD);
  gl->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl->bindFramebuffer(GL_FRAMEBUFFER, targetInfo.id);
  gl->viewport(0, 0, target.width(), target.height());
  gl->clearColor(0, 0, 0, 0);
  gl->clear(GL_COLOR_BUFFER_BIT);
  for (size_t i = 0; i < sources.size(); i++) {
    tgfx::GLTextureInfo sourceInfo;
    if (!sources[i].getGLTextureInfo(&sourceInfo)) {
      return false;
    }
    ActiveGLTexture(context, static_cast<int>(i), &sourceInfo);
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

std::vector<float> RuntimeFilter::computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                                  const tgfx::BackendRenderTarget& target,
                                                  const tgfx::Point& offset) const {
  std::vector<float> vertices = {};
  auto inputBounds = tgfx::Rect::MakeWH(sources[0].width(), sources[0].height());
  auto targetBounds = filterBounds(inputBounds);
  tgfx::Point contentPoint[4] = {{targetBounds.left, targetBounds.bottom},
                                 {targetBounds.right, targetBounds.bottom},
                                 {targetBounds.left, targetBounds.top},
                                 {targetBounds.right, targetBounds.top}};
  tgfx::Point texturePoints[4] = {{inputBounds.left, inputBounds.bottom},
                                  {inputBounds.right, inputBounds.bottom},
                                  {inputBounds.left, inputBounds.top},
                                  {inputBounds.right, inputBounds.top}};

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

void RuntimeFilter::bindVertices(tgfx::Context* context, const RuntimeProgram* filterProgram,
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

}  // namespace pag
