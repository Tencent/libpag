/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <algorithm>
#include <iterator>
#include "base/utils/MathUtil.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/utils/FilterHelper.h"

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

std::shared_ptr<const DepthRenderBuffer> DepthRenderBuffer::Make(tgfx::Context* context, int width,
                                                                 int height) {
  auto gl = tgfx::GLFunctions::Get(context);
  unsigned rbo = 0;
  gl->genRenderbuffers(1, &rbo);
  if (rbo == 0) {
    return nullptr;
  }
  gl->bindRenderbuffer(GL_RENDERBUFFER, rbo);
  gl->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  if (!CheckGLError(context)) {
    gl->deleteRenderbuffers(1, &rbo);
    return nullptr;
  }
  auto renderBufferInfo = std::shared_ptr<DepthRenderBuffer>(new DepthRenderBuffer());
  renderBufferInfo->id = rbo;
  tgfx::GLResource::AttachToContext(context, renderBufferInfo);
  return renderBufferInfo;
}

void DepthRenderBuffer::onReleaseGPU() {
  auto gl = tgfx::GLFunctions::Get(getContext());
  gl->deleteRenderbuffers(1, &id);
}

bool Transform3DStateKey::operator<(const Transform3DStateKey& other) const {
  if (containerID != other.containerID) {
    return containerID < other.containerID;
  }
  if (width != other.width) {
    return width < other.width;
  }
  return height < other.height;
}

bool Transform3DFilter::IsActiveCamera(const Layer* layer, Frame frame) {
  return layer->type() == LayerType::Camera && layer->isActive && frame >= layer->startTime &&
         frame < layer->startTime + layer->duration;
}

bool Transform3DFilter::initialize(tgfx::Context* context) {
  // Clear the GLError left by the previous draws so that the check below reports our own errors.
  CheckGLError(context);

  auto vertex = onBuildVertexShader();
  auto fragment = onBuildFragmentShader();
  filterProgram = FilterProgram::Make(context, vertex, fragment);
  if (filterProgram == nullptr) {
    return false;
  }
  onPrepareProgram(context, filterProgram->program);
  if (!CheckGLError(context)) {
    filterProgram = nullptr;
    return false;
  }
  return true;
}

Transform3DFilter::Transform3DFilter() {
}

Transform3DFilter::~Transform3DFilter() {
  depthStates.clear();
}

bool Transform3DFilter::updateLayer(Layer* layer, Frame layerFrame, RenderCache* renderCache) {
  if (layer == nullptr || layer->transform3D == nullptr ||
      layer->containingComposition == nullptr || renderCache == nullptr) {
    return false;
  }
  this->layer = layer;
  layerTransform = layer->transform3D;
  cameraTransform = nullptr;
  cameraOption = nullptr;

  auto containingComposition = layer->containingComposition;
  auto compositionFrame = layerFrame - layer->startTime;
  auto& layers = containingComposition->layers;
  for (auto item = layers.rbegin(); item != layers.rend(); ++item) {
    auto candidate = *item;
    if (candidate->type() != LayerType::Camera) {
      continue;
    }
    auto cameraLayer = static_cast<CameraLayer*>(candidate);
    auto cameraFrame = compositionFrame + cameraLayer->startTime;
    if (IsActiveCamera(cameraLayer, cameraFrame)) {
      cameraTransform = cameraLayer->transform3D;
      cameraOption = cameraLayer->cameraOption;
      break;
    }
  }
  auto layerIter = std::find(layers.begin(), layers.end(), layer);
  layerIndex =
      layerIter == layers.end() ? -1 : static_cast<int>(std::distance(layers.begin(), layerIter));
  containerID = containingComposition->uniqueID;
  frameSerial = renderCache->frameSerial();
  return true;
}

std::string Transform3DFilter::onBuildVertexShader() {
  return VERTEX_SHADER;
}

std::string Transform3DFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void Transform3DFilter::onPrepareProgram(tgfx::Context* context, unsigned program) {
  auto gl = tgfx::GLFunctions::Get(context);
  positionHandle = gl->getAttribLocation(program, "aPosition");
  textureCoordHandle = gl->getAttribLocation(program, "aTextureCoord");
  modelMatrixHandle = gl->getUniformLocation(program, "uModelMatrix");
  viewMatrixHandle = gl->getUniformLocation(program, "uViewMatrix");
  projectionMatrixHandle = gl->getUniformLocation(program, "uProjectionMatrix");
  textureMatrixHandle = gl->getUniformLocation(program, "uTextureMatrix");
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

void Transform3DFilter::update(Frame frame, const tgfx::Rect& inputBounds,
                               const tgfx::Rect& outputBounds, const tgfx::Point& extraScale) {
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
  mContext = context;
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
  auto enableDepth = gl->isEnabled(GL_DEPTH_TEST);
  gl->enable(GL_DEPTH_TEST);
  int depthFunc = 0;
  gl->getIntegerv(GL_DEPTH_FUNC, &depthFunc);
  gl->depthFunc(GL_GREATER);

  Transform3DStateKey key = {containerID, target->width, target->height};
  auto stateIter = depthStates.find(key);
  if (stateIter == depthStates.end()) {
    auto renderBuffer = DepthRenderBuffer::Make(context, target->width, target->height);
    if (renderBuffer == nullptr) {
      gl->depthFunc(depthFunc);
      if (enableDepth) {
        gl->enable(GL_DEPTH_TEST);
      } else {
        gl->disable(GL_DEPTH_TEST);
      }
      return;
    }
    Transform3DDepthState state = {};
    state.renderBuffer = renderBuffer;
    stateIter = depthStates.insert(std::make_pair(key, state)).first;
  }
  auto& depthState = stateIter->second;

  // The layers of a composition are drawn from the last one to the first one, so a layer index that
  // does not decrease means the composition is traversed again.
  auto outOfOrder =
      layerIndex >= 0 && depthState.lastLayerIndex >= 0 && layerIndex >= depthState.lastLayerIndex;
  auto newPass = !depthState.valid || depthState.frameSerial != frameSerial || outOfOrder;
  if (newPass) {
    depthState.frameSerial = frameSerial;
    depthState.lastLayerIndex = -1;
    depthState.valid = true;
  }

  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              depthState.renderBuffer->id);
  auto status = gl->checkFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    LOGE("Framebuffer is not complete, status: 0x%x", status);
    gl->depthFunc(depthFunc);
    if (enableDepth) {
      gl->enable(GL_DEPTH_TEST);
    } else {
      gl->disable(GL_DEPTH_TEST);
    }
    return;
  }

  if (newPass) {
    gl->clearDepthf(-1.0);
    gl->clear(GL_DEPTH_BUFFER_BIT);
  }

  depthState.lastLayerIndex = layerIndex;

  onUpdateParams(context, source, target);
  auto vertices = computeVertices(contentBounds, transformedBounds, filterScale);
  bindVertices(context, source, target, vertices);
  gl->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(0);
  }
  DisableMultisample(context, needsMSAA());
  // Detach the depth render buffer so the framebuffer owned by tgfx is left untouched.
  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  gl->depthFunc(depthFunc);
  if (enableDepth) {
    gl->enable(GL_DEPTH_TEST);
  } else {
    gl->disable(GL_DEPTH_TEST);
  }
  CheckGLError(context);
}

void Transform3DFilter::onUpdateParams(tgfx::Context* context, const FilterSource* source,
                                       const FilterTarget* target) {
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

  float cameraZoom;
  if (cameraOption) {
    cameraZoom = cameraOption->zoom->getValueAt(layerFrame);
  } else {
    cameraZoom = 1000.f;
  }
  auto aspectScale = 1.0f * source->width / target->width;
  auto sourceAspectScaleMatrix = Matrix3D::Scale(1.0, 1.0 * source->height / source->width);
  auto targetAspectScaleMatrix = Matrix3D::Scale(1.0, 1.0 * target->height / target->width);
  auto aspectScaleMatrix = Matrix3D::Scale(aspectScale, aspectScale);
  sourceAspectScaleMatrix.postConcat(aspectScaleMatrix);
  Matrix3D invertTargetAspectScaleMatrix;
  targetAspectScaleMatrix.invert(&invertTargetAspectScaleMatrix);

  auto scalar = 1.0f * target->width / source->scale.x;
  auto ratio = 1.0f * target->height / target->width;

  modelMatrix = Matrix3D();
  modelMatrix.postTranslate(1.0, 1.0);
  modelMatrix.postConcat(sourceAspectScaleMatrix);
  modelMatrix.postConcat(invertTargetAspectScaleMatrix);
  modelMatrix.postTranslate((-anchorPoint.x + contentBounds.x()) / scalar * 2.0f,
                            (-anchorPoint.y + contentBounds.y()) / (scalar * ratio) * 2.0f,
                            anchorPoint.z / scalar * 2.0f);
  modelMatrix.postConcat(targetAspectScaleMatrix);
  auto scaleMatrix = Matrix3D::Scale(scale.x, scale.y, scale.z);
  modelMatrix.postConcat(scaleMatrix);
  auto xOMatrix = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(-orientation.x));
  auto yOMatrix = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(-orientation.y));
  auto zOMatrix = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(orientation.z));
  auto xRMatrix = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(-xRotation));
  auto yRMatrix = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(-yRotation));
  auto zRMatrix = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(zRotation));
  modelMatrix.postConcat(zRMatrix).postConcat(yRMatrix).postConcat(xRMatrix);
  modelMatrix.postConcat(zOMatrix).postConcat(yOMatrix).postConcat(xOMatrix);
  modelMatrix.postConcat(invertTargetAspectScaleMatrix);
  modelMatrix.postTranslate(position.x / scalar * 2.0f, position.y / (scalar * ratio) * 2.0f,
                            -position.z / scalar * 2.0f);
  modelMatrix.postTranslate(-1.0, -1.0);

  // The whole parent chain is concatenated, so a 3D layer nested in several 3D or 2D parents is
  // projected with the transforms of all of them.
  for (auto parent = layer->parent; parent != nullptr; parent = parent->parent) {
    auto parentMatrix = Matrix3D();
    if (parent->transform3D != nullptr) {
      auto parentTransform3D = parent->transform3D;
      auto anchorPointP = parentTransform3D->anchorPoint->getValueAt(layerFrame);
      Point3D positionP;
      if (parentTransform3D->position) {
        positionP = parentTransform3D->position->getValueAt(layerFrame);
      } else {
        float xPosition = parentTransform3D->xPosition->getValueAt(layerFrame);
        float yPosition = parentTransform3D->yPosition->getValueAt(layerFrame);
        float zPosition = parentTransform3D->zPosition->getValueAt(layerFrame);
        positionP = Point3D::Make(xPosition, yPosition, zPosition);
      }
      auto scaleP = parentTransform3D->scale->getValueAt(layerFrame);
      auto orientationP = parentTransform3D->orientation->getValueAt(layerFrame);
      auto xRotationP = parentTransform3D->xRotation->getValueAt(layerFrame);
      auto yRotationP = parentTransform3D->yRotation->getValueAt(layerFrame);
      auto zRotationP = parentTransform3D->zRotation->getValueAt(layerFrame);

      parentMatrix.postTranslate(1.0, 1.0);
      parentMatrix.postTranslate(-anchorPointP.x / scalar * 2.0f,
                                 -anchorPointP.y / (scalar * ratio) * 2.0f,
                                 anchorPointP.z / scalar * 2.0f);
      parentMatrix.postConcat(targetAspectScaleMatrix);
      auto scaleMatrixP = Matrix3D::Scale(scaleP.x, scaleP.y, scaleP.z);
      parentMatrix.postConcat(scaleMatrixP);
      auto xOMatrixP = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(-orientationP.x));
      auto yOMatrixP = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(-orientationP.y));
      auto zOMatrixP = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(orientationP.z));
      auto xRMatrixP = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(-xRotationP));
      auto yRMatrixP = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(-yRotationP));
      auto zRMatrixP = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(zRotationP));
      parentMatrix.postConcat(zRMatrixP).postConcat(yRMatrixP).postConcat(xRMatrixP);
      parentMatrix.postConcat(zOMatrixP).postConcat(yOMatrixP).postConcat(xOMatrixP);
      parentMatrix.postConcat(invertTargetAspectScaleMatrix);
      parentMatrix.postTranslate(positionP.x / scalar * 2.0f, positionP.y / (scalar * ratio) * 2.0f,
                                 -positionP.z / scalar * 2.0f);
      parentMatrix.postTranslate(-1.0, -1.0);
    } else if (parent->transform != nullptr) {
      auto parentTransform2D = parent->transform;
      auto anchorPointP = parentTransform2D->anchorPoint->getValueAt(layerFrame);
      Point positionP;
      if (parentTransform2D->position) {
        positionP = parentTransform2D->position->getValueAt(layerFrame);
      } else {
        float xPosition = parentTransform2D->xPosition->getValueAt(layerFrame);
        float yPosition = parentTransform2D->yPosition->getValueAt(layerFrame);
        positionP = Point::Make(xPosition, yPosition);
      }
      auto scaleP = parentTransform2D->scale->getValueAt(layerFrame);
      auto rotationP = parentTransform2D->rotation->getValueAt(layerFrame);

      parentMatrix.postTranslate(1.0, 1.0);
      parentMatrix.postTranslate(-anchorPointP.x / scalar * 2.0f,
                                 -anchorPointP.y / (scalar * ratio) * 2.0f, 0.0f);
      parentMatrix.postConcat(targetAspectScaleMatrix);
      auto scaleMatrixP = Matrix3D::Scale(scaleP.x, scaleP.y, 1.0f);
      parentMatrix.postConcat(scaleMatrixP);
      auto zRMatrixP = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(rotationP));
      parentMatrix.postConcat(zRMatrixP);
      parentMatrix.postConcat(invertTargetAspectScaleMatrix);
      parentMatrix.postTranslate(positionP.x / scalar * 2.0f, positionP.y / (scalar * ratio) * 2.0f,
                                 0.0f);
      parentMatrix.postTranslate(-1.0, -1.0);
    } else {
      continue;
    }
    modelMatrix.postConcat(parentMatrix);
  }

  float cameraFov = atanf(0.5f * scalar / cameraZoom) * 2.0f;
  Vec3 cameraEye = {0.0f, 0.0f, 0.0f};
  Vec3 cameraCoa = {0.0f, 0.0f, 0.0f};
  Vec3 cameraUp = {0.0f, 1.0f, 0.0f};

  if (cameraTransform) {
    Point3D cameraPosition;
    Vec3 cameraTransformPosition;
    if (cameraTransform->position) {
      cameraPosition = cameraTransform->position->getValueAt(layerFrame);
    } else {
      cameraPosition.x = cameraTransform->xPosition->getValueAt(layerFrame);
      cameraPosition.y = cameraTransform->yPosition->getValueAt(layerFrame);
      cameraPosition.z = cameraTransform->zPosition->getValueAt(layerFrame);
    }

    auto transZ =
        1.0f * -cameraPosition.z / (scalar / 2.0f) + (cameraPosition.z > 0.0f ? 1.0f : -1.0f);
    cameraTransformPosition = {cameraPosition.x / scalar * 2.0f - 1.0f,
                               cameraPosition.y / (scalar * ratio) * 2.0f - 1.0f, transZ};

    cameraEye += cameraTransformPosition;

    auto cOrientation = cameraTransform->orientation->getValueAt(layerFrame);
    auto cXRotation = cameraTransform->xRotation->getValueAt(layerFrame);
    auto cYRotation = cameraTransform->yRotation->getValueAt(layerFrame);
    auto cZRotation = cameraTransform->zRotation->getValueAt(layerFrame);

    if (cameraTransform->anchorPoint) {
      auto cameraPointOfInterest = cameraTransform->anchorPoint->getValueAt(layerFrame);
      Vec3 cameraTransformPOI = {cameraPointOfInterest.x / scalar * 2.0f - 1.0f,
                                 cameraPointOfInterest.y / (scalar * ratio) * 2.0f - 1.0f, 0.0f};

      cameraCoa += cameraTransformPOI;
    }

    viewMatrix = Matrix3D::LookAt(cameraEye, cameraCoa, cameraUp);
    viewMatrix.postConcat(targetAspectScaleMatrix);
    viewMatrix.postTranslate(0.0f, 0.0f, -1.0f);
    auto cXOMatrix = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(cOrientation.x));
    auto cYOMatrix = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(cOrientation.y));
    auto cZOMatrix = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(-cOrientation.z));
    auto cXRMatrix = Matrix3D::Rotate({1.0, 0.0, 0.0}, DegreesToRadians(cXRotation));
    auto cYRMatrix = Matrix3D::Rotate({0.0, 1.0, 0.0}, DegreesToRadians(cYRotation));
    auto cZRMatrix = Matrix3D::Rotate({0.0, 0.0, 1.0}, DegreesToRadians(-cZRotation));
    viewMatrix.postConcat(cXOMatrix).postConcat(cYOMatrix).postConcat(cZOMatrix);
    viewMatrix.postConcat(cXRMatrix).postConcat(cYRMatrix).postConcat(cZRMatrix);
    viewMatrix.postTranslate(0.0f, 0.0f, 1.0f);
    viewMatrix.postConcat(invertTargetAspectScaleMatrix);
  } else {
    cameraEye += {0.0f, 0.0f, 2.0f * cameraZoom / scalar - 1.0f};
    viewMatrix = Matrix3D::LookAt(cameraEye, cameraCoa, cameraUp);
  }

  float zNear = -0.25f;
  float zFar = 100.0f;
  projectionMatrix = projectionMatrix.Perspective(zNear, zFar, cameraFov);

  auto gl = tgfx::GLFunctions::Get(context);
  tgfx::Matrix matrix = {};
  auto values = target->vertexMatrix;
  matrix.setAll(values[0], values[3], values[6], values[1], values[4], values[7]);
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
                                     const FilterTarget* target,
                                     const std::vector<tgfx::Point>& points) {
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
