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

#include "Transform3DFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "core/utils/MathExtra.h"
#include "base/utils/MathUtil.h"

namespace pag {
static constexpr char VERTEX_SHADER[] = R"(
    #version 100
    attribute vec3 aPosition;
    attribute vec2 aTextureCoord;
    uniform mat4 uModelMatrix;
    uniform mat4 uViewMatrix;
    uniform mat4 uProjectionMatrix;
    uniform mat3 uTextureMatrix;
    varying vec2 vertexColor;
    void main() {
        gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);
        vec3 colorPosition = uTextureMatrix * vec3(aTextureCoord, 1.0);
        vertexColor = colorPosition.xy;
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

struct CtmInfo {
  float fNear = 0.05f;
  float fFar = 4.f;
  float fAngle = tgfx::M_PI_F / 4;
  
  tgfx::SkV3 fEye { 0, 0, 1.0f/tan(fAngle/2) - 1 };
  tgfx::SkV3 fCOA { 0, 0, 0 };
  tgfx::SkV3 fUp  { 0, 1, 0 };
};

bool Transform3DFilter::initialize(tgfx::Context* context) {
  // 防止前面产生的GLError，导致后面CheckGLError逻辑返回错误结果
  CheckGLError(context);

  auto vertex = onBuildVertexShader();
  auto fragment = onBuildFragmentShader();
  filterProgram = FilterProgram::Make(context, vertex, fragment);
  if (filterProgram == nullptr) {
    return false;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  auto program = filterProgram->program;
  positionHandle = gl->getAttribLocation(program, "aPosition");
  textureCoordHandle = gl->getAttribLocation(program, "aTextureCoord");
  modelMatrixHandle = gl->getUniformLocation(program, "uModelMatrix");
  viewMatrixHandle = gl->getUniformLocation(program, "uViewMatrix");
  projectionMatrixHandle = gl->getUniformLocation(program, "uProjectionMatrix");
  textureMatrixHandle = gl->getUniformLocation(program, "uTextureMatrix");
  onPrepareProgram(context, program);
  if (!CheckGLError(context)) {
    filterProgram = nullptr;
    return false;
  }
  return true;
}

Transform3DFilter::Transform3DFilter() {
}

bool Transform3DFilter::updateLayer(Layer* layer, Frame layerFrame) {
  auto layerTransform = layer->transform3D;
  Transform3D* cameraTransform = nullptr;
  auto containingComposition = layer->containingComposition;
  for (auto aLayer : containingComposition->layers) {
    if (aLayer->type() == LayerType::Camera) {
      auto cameraLayer = static_cast<CameraLayer*>(aLayer);
      cameraTransform = cameraLayer->transform3D;
      break;
    }
  }
  
  auto bounds = layer->getBounds();
  
  auto anchorPoint = layerTransform->anchorPoint->getValueAt(layerFrame);
  Point3D position;
  if (layerTransform->position) {
    position = layerTransform->position->getValueAt(layerFrame);
  } else {
    float xPosition = layerTransform->xPosition->getValueAt(layerFrame);
    float yPosition = layerTransform->yPosition->getValueAt(layerFrame);
    float zPosition = layerTransform->zPosition->getValueAt(layerFrame);
    position = Point3D::Make(xPosition, yPosition, zPosition);
  }
  auto scale = layerTransform->scale->getValueAt(layerFrame);
  auto orientation = layerTransform->orientation->getValueAt(layerFrame);
  auto xRotation = layerTransform->xRotation->getValueAt(layerFrame);
  auto yRotation = layerTransform->yRotation->getValueAt(layerFrame);
  auto zRotation = layerTransform->zRotation->getValueAt(layerFrame);
  auto opacity = layerTransform->opacity->getValueAt(layerFrame);
  
  modelMatrix = tgfx::Matrix4x4::Translate(-anchorPoint.x / bounds.width(),
                                           anchorPoint.y / bounds.height(),
                                           0);
  
  modelMatrix.setIdentity();

//  modelMatrix.preTranslate(-1.0, -1.0);
//  auto scaleMatrix = tgfx::Matrix4x4::Scale(scale.x, scale.y, scale.z);
//  modelMatrix.preConcat(scaleMatrix);
//  modelMatrix.preTranslate(1.0, 1.0);
//  modelMatrix.preTranslate(2.0, 2.0);
//  auto xRotationMatrix = tgfx::Matrix4x4::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(orientation.x + xRotation));
//  auto yRotationMatrix = tgfx::Matrix4x4::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(-(orientation.y + yRotation)));
//  auto zRotationMatrix = tgfx::Matrix4x4::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(-(orientation.z + zRotation)));
//  modelMatrix.preConcat(xRotationMatrix).preConcat(yRotationMatrix).preConcat(zRotationMatrix);
//  modelMatrix.preTranslate(1.0, 1.0);
//  modelMatrix.preTranslate(-0.5, -0.5);
//  auto scaleMatrix = tgfx::Matrix4x4::Scale(scale.x, scale.y, scale.z);
//  scaleMatrix.preTranslate(0.5, -0.5);
//  scaleMatrix.postTranslate(-0.5, 0.5);
//  modelMatrix.preConcat(scaleMatrix);
//  modelMatrix.preTranslate(0.5, -0.5);
//  modelMatrix.preTranslate(position.x / bounds.width() * 2,
//                           position.y / bounds.height() * 2,
//                           position.z / 1000 * 2);
  CtmInfo info;
  
  if (cameraTransform) {
    auto cameraPointOfInterest = cameraTransform->anchorPoint->getValueAt(layerFrame);
    Point3D cameraPositon;
    if (cameraTransform->position) {
      cameraPositon = cameraTransform->position->getValueAt(layerFrame);
    } else {
      float xPosition = cameraTransform->xPosition->getValueAt(layerFrame);
      float yPosition = cameraTransform->yPosition->getValueAt(layerFrame);
      float zPosition = cameraTransform->zPosition->getValueAt(layerFrame);
      cameraPositon = Point3D::Make(xPosition, yPosition, zPosition);
    }
    auto cameraOrientation = cameraTransform->orientation->getValueAt(layerFrame);
    auto cameraXRotation = cameraTransform->xRotation->getValueAt(layerFrame);
    auto cameraYRotation = cameraTransform->yRotation->getValueAt(layerFrame);
    auto cameraZRotation = cameraTransform->zRotation->getValueAt(layerFrame);
    
//    viewMatrix = tgfx::Matrix4x4::LookAt(info.fEye, info.fCOA, info.fUp);
    viewMatrix = tgfx::Matrix4x4::LookAt(info.fEye, info.fCOA, info.fUp);
//    viewMatrix = tgfx::Matrix4x4::Translate(0, 0, 0) * tgfx::Matrix4x4::Scale(0.5 * bounds.width() / bounds.height(), -0.5, 1.0f);

  } else {
//    viewMatrix = tgfx::Matrix4x4::Translate(0, 0, 0) * tgfx::Matrix4x4::Scale(0.5 * bounds.width() / bounds.height(), -0.5, 1.0f);
  }
  
  projectionMatrix = projectionMatrix.Perspective(info.fNear, info.fFar, info.fAngle);
  
  return true;
}

std::string Transform3DFilter::onBuildVertexShader() {
  return VERTEX_SHADER;
}

std::string Transform3DFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void Transform3DFilter::onPrepareProgram(tgfx::Context*, unsigned) {
}

void Transform3DFilter::onUpdateParams(tgfx::Context*, const tgfx::Rect&, const tgfx::Point&) {
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

void Transform3DFilter::update(Frame frame, const tgfx::Rect& inputBounds, const tgfx::Rect& outputBounds,
                               const tgfx::Point& extraScale) {
  layerFrame = frame;
  contentBounds = inputBounds;
  transformedBounds = outputBounds;
  filterScale = extraScale;
}

void Transform3DFilter::draw(tgfx::Context* context, const FilterSource* source,
                       const FilterTarget* target) {
  if (source == nullptr || target == nullptr || !filterProgram) {
    LOGE(
        "Transform3DFilter::draw() can not draw filter, "
        "because the argument(source/target) is null");
    return;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  EnableMultisample(context, needsMSAA());
  gl->useProgram(filterProgram->program);
  gl->disable(GL_SCISSOR_TEST);
  gl->enable(GL_BLEND);
  gl->blendEquation(GL_FUNC_ADD);
  gl->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl->bindFramebuffer(GL_FRAMEBUFFER, target->frameBuffer.id);
  gl->viewport(0, 0, target->width, target->height);
  ActiveGLTexture(context, 0, &source->sampler);
  onSubmitTransformations(context, source, target);
  onUpdateParams(context, contentBounds, filterScale);
  auto vertices = computeVertices(contentBounds, transformedBounds, filterScale);
  bindVertices(context, source, target, vertices);
  gl->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(0);
  }
  DisableMultisample(context, needsMSAA());
  CheckGLError(context);
}

void Transform3DFilter::onSubmitTransformations(tgfx::Context* context, const FilterSource* source,
                                                const FilterTarget* target) {
  auto gl = tgfx::GLFunctions::Get(context);
  tgfx::Matrix matrix = {};
  auto values = target->vertexMatrix;
  matrix.setAll(values[0], values[3], values[6], values[1], values[4], values[7], values[2],
                values[5], values[8]);
  modelMatrix.preConcat(matrix);

  float modelMatrixRowMajor[16];
  modelMatrix.getRowMajor(modelMatrixRowMajor);
  
  float viewMatrixRowMajor[16];
  viewMatrix.getRowMajor(viewMatrixRowMajor);
  
  float projectionMatrixRowMajor[16];
  projectionMatrix.getRowMajor(projectionMatrixRowMajor);
    
  gl->uniformMatrix4fv(modelMatrixHandle, 1, GL_TRUE, modelMatrixRowMajor);
  gl->uniformMatrix4fv(viewMatrixHandle, 1, GL_TRUE, viewMatrixRowMajor);
  gl->uniformMatrix4fv(projectionMatrixHandle, 1, GL_TRUE, projectionMatrixRowMajor);
  gl->uniformMatrix3fv(textureMatrixHandle, 1, GL_FALSE, source->textureMatrix.data());
}

std::vector<tgfx::Point> Transform3DFilter::computeVertices(const tgfx::Rect& bounds,
                                                      const tgfx::Rect& transformed,
                                                      const tgfx::Point&) {
  std::vector<tgfx::Point> vertices = {};
  tgfx::Point contentPoint[4] = {{transformed.left, transformed.bottom},
                                 {transformed.right, transformed.bottom},
                                 {transformed.left, transformed.top},
                                 {transformed.right, transformed.top}};
  tgfx::Point texturePoints[4] = {{0.0f, bounds.height()},
                                  {bounds.width(), bounds.height()},
                                  {0.0f, 0.0f},
                                  {bounds.width(), 0.0f}};
  for (int ii = 0; ii < 4; ii++) {
    vertices.push_back(contentPoint[ii]);
    vertices.push_back(texturePoints[ii]);
  }
  return vertices;
}

void Transform3DFilter::bindVertices(tgfx::Context* context, const FilterSource* source,
                                     const FilterTarget* target, const std::vector<tgfx::Point>& points) {
  std::vector<float> vertices = {};
  for (size_t i = 0; i < points.size();) {
    auto vertexPoint = ToGLVertexPoint(target, source, contentBounds, points[i++]);
    vertices.push_back(vertexPoint.x);
    vertices.push_back(vertexPoint.y);
    vertices.push_back(0.0f);
    auto texturePoint = ToGLTexturePoint(source, points[i++]);
    vertices.push_back(texturePoint.x);
    vertices.push_back(texturePoint.y);
  }
  auto gl = tgfx::GLFunctions::Get(context);
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(filterProgram->vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, filterProgram->vertexBuffer);
  gl->bufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STREAM_DRAW);
  gl->vertexAttribPointer(static_cast<unsigned>(positionHandle), 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), static_cast<void*>(0));
  gl->enableVertexAttribArray(static_cast<unsigned>(positionHandle));

  gl->vertexAttribPointer(textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
  gl->enableVertexAttribArray(textureCoordHandle);
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Transform3DFilter::needsMSAA() const {
  return Filter::needsMSAA();
}
}  // namespace pag
