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
#include <array>
#include <iterator>
#include <vector>
#include "base/utils/Log.h"
#include "base/utils/MathUtil.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
static constexpr char VERTEX_SHADER[] = R"(
    #version 100
    attribute vec3 aPosition;
    attribute vec2 aTextureCoord;
    uniform mat4 uMatrix;
    varying vec2 vertexColor;
    void main() {
        gl_Position = uMatrix * vec4(aPosition, 1.0);
        vec3 colorPosition =  vec3(aTextureCoord, 1.0);
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

static unsigned MakeRenderBuffer(tgfx::Context* context, int width, int height) {
  auto gl = tgfx::GLFunctions::Get(context);
  unsigned rbo = 0;
  gl->genRenderbuffers(1, &rbo);
  if (rbo == 0) {
    return 0;
  }
  gl->bindRenderbuffer(GL_RENDERBUFFER, rbo);
  gl->renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
  if (!CheckGLError(context)) {
    gl->deleteRenderbuffers(1, &rbo);
    gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
    return 0;
  }
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  return rbo;
}

bool Transform3DStateKey::operator<(const Transform3DStateKey& other) const {
  if (cacheID != other.cacheID) {
    return cacheID < other.cacheID;
  }
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

Transform3DUniforms::Transform3DUniforms(tgfx::Context* context, unsigned program)
    : Uniforms(context, program) {
  auto gl = tgfx::GLFunctions::Get(context);
  matrixHandle = gl->getUniformLocation(program, "uMatrix");
}

std::unique_ptr<Transform3DProgram> Transform3DProgram::Make(tgfx::Context* context) {
  auto gl = tgfx::GLFunctions::Get(context);
  auto program = CreateGLProgram(context, VERTEX_SHADER, FRAGMENT_SHADER);
  if (program == 0) {
    return nullptr;
  }
  auto filterProgram =
      std::unique_ptr<Transform3DProgram>(new Transform3DProgram(context, program));
  if (gl->bindVertexArray != nullptr) {
    gl->genVertexArrays(1, &filterProgram->vertexArray);
  }
  gl->genBuffers(1, &filterProgram->vertexBuffer);
  filterProgram->uniforms = std::make_unique<Transform3DUniforms>(context, program);
  if (!CheckGLError(context)) {
    return nullptr;
  }
  return filterProgram;
}

void Transform3DProgram::onReleaseGPU() {
  auto gl = tgfx::GLFunctions::Get(getContext());
  for (auto& item : depthStates) {
    auto& renderBuffer = item.second.renderBuffer;
    if (renderBuffer != 0) {
      gl->deleteRenderbuffers(1, &renderBuffer);
      renderBuffer = 0;
    }
  }
  depthStates.clear();
  RuntimeProgram::onReleaseGPU();
}

Transform3DProgram::Transform3DProgram(tgfx::Context* context, unsigned program)
    : RuntimeProgram(context) {
  this->program = program;
}

std::unique_ptr<tgfx::RuntimeProgram> Transform3DFilter::onCreateProgram(
    tgfx::Context* context) const {
  // Clear the previously generated GLError
  CheckGLError(context);
  return Transform3DProgram::Make(context);
}

std::shared_ptr<tgfx::Image> Transform3DFilter::Apply(
    const std::shared_ptr<tgfx::Image>& input, RenderCache* cache, Layer* layer, Frame layerFrame,
    const tgfx::Rect& contentBounds, const tgfx::Point& sourceScale, tgfx::Point* offset) {
  offset->set(0, 0);
  auto transform3D = layer == nullptr ? nullptr : layer->transform3D;
  auto containingComposition = layer == nullptr ? nullptr : layer->containingComposition;
  if (transform3D == nullptr || containingComposition == nullptr || cache == nullptr ||
      input == nullptr || sourceScale.x <= 0 || sourceScale.y <= 0) {
    return nullptr;
  }
  Transform3D* cameraTransform = nullptr;
  CameraOption* cameraOption = nullptr;

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

  Point3D position = {};
  if (transform3D->position) {
    position = transform3D->position->getValueAt(layerFrame);
  } else {
    float xPosition = transform3D->xPosition->getValueAt(layerFrame);
    float yPosition = transform3D->yPosition->getValueAt(layerFrame);
    float zPosition = transform3D->zPosition->getValueAt(layerFrame);
    position = Point3D::Make(xPosition, yPosition, zPosition);
  }
  auto anchorPoint = transform3D->anchorPoint->getValueAt(layerFrame);
  auto scale = transform3D->scale->getValueAt(layerFrame);
  auto orientation = transform3D->orientation->getValueAt(layerFrame);
  auto xRotation = transform3D->xRotation->getValueAt(layerFrame);
  auto yRotation = transform3D->yRotation->getValueAt(layerFrame);
  auto zRotation = transform3D->zRotation->getValueAt(layerFrame);

  float cameraZoom;
  if (cameraOption) {
    cameraZoom = cameraOption->zoom->getValueAt(layerFrame);
  } else {
    cameraZoom = 1000.f;
  }
  auto targetWidth = ceilf(static_cast<float>(containingComposition->width) * sourceScale.x);
  auto targetHeight = ceilf(static_cast<float>(containingComposition->height) * sourceScale.y);
  auto aspectScale = static_cast<float>(input->width()) / targetWidth;
  auto sourceAspectScaleMatrix = Matrix3D::Scale(
      1.0, static_cast<float>(input->height()) / static_cast<float>(input->width()));
  auto targetAspectScaleMatrix = Matrix3D::Scale(1.0, targetHeight / targetWidth);
  auto aspectScaleMatrix = Matrix3D::Scale(aspectScale, aspectScale);
  sourceAspectScaleMatrix.postConcat(aspectScaleMatrix);
  Matrix3D invertTargetAspectScaleMatrix;
  targetAspectScaleMatrix.invert(&invertTargetAspectScaleMatrix);

  auto scalar = targetWidth / sourceScale.x;
  auto ratio = targetHeight / targetWidth;

  Matrix3D modelMatrix = {};
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
      Point3D positionP = {};
      if (parentTransform3D->position) {
        positionP = parentTransform3D->position->getValueAt(layerFrame);
      } else {
        float xPosition = parentTransform3D->xPosition->getValueAt(layerFrame);
        float yPosition = parentTransform3D->yPosition->getValueAt(layerFrame);
        float zPosition = parentTransform3D->zPosition->getValueAt(layerFrame);
        positionP = Point3D::Make(xPosition, yPosition, zPosition);
      }
      auto anchorPointP = parentTransform3D->anchorPoint->getValueAt(layerFrame);
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
      Point positionP = {};
      if (parentTransform2D->position) {
        positionP = parentTransform2D->position->getValueAt(layerFrame);
      } else {
        float xPosition = parentTransform2D->xPosition->getValueAt(layerFrame);
        float yPosition = parentTransform2D->yPosition->getValueAt(layerFrame);
        positionP = Point::Make(xPosition, yPosition);
      }
      auto anchorPointP = parentTransform2D->anchorPoint->getValueAt(layerFrame);
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

  Matrix3D viewMatrix = {};
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
  Matrix3D projectionMatrix = Matrix3D::Perspective(zNear, zFar, cameraFov);
  auto finalMatrix = modelMatrix;
  finalMatrix.postConcat(viewMatrix);
  finalMatrix.postConcat(projectionMatrix);

  Transform3DLayerState state;
  state.cacheID = cache->uniqueID();
  state.containerID = containingComposition->id;
  auto layerIter = std::find(layers.begin(), layers.end(), layer);
  state.layerIndex =
      layerIter == layers.end() ? -1 : static_cast<int>(std::distance(layers.begin(), layerIter));
  state.width = static_cast<int>(targetWidth);
  state.height = static_cast<int>(targetHeight);
  state.frameSerial = cache->frameSerial();
  state.matrix = finalMatrix;

  auto filter = std::make_shared<Transform3DFilter>(state);
  auto result = input->makeWithFilter(tgfx::ImageFilter::Runtime(filter));
  if (result == nullptr) {
    return nullptr;
  }
  if (offset) {
    // The 3D filter output covers the input's position in the parent composition, so cancel the
    // offset that was applied before this filter.
    offset->offset(-contentBounds.x(), -contentBounds.y());
  }
  return result;
}

bool Transform3DFilter::onDraw(const tgfx::RuntimeProgram* program,
                               const std::vector<tgfx::BackendTexture>& inputTextures,
                               const tgfx::BackendRenderTarget& target,
                               const tgfx::Point& offset) const {
  if (inputTextures.empty() || !target.isValid() || program == nullptr ||
      inputTextures[0].backend() != tgfx::Backend::OPENGL ||
      target.backend() != tgfx::Backend::OPENGL) {
    LOGE(
        "Transform3DFilter::draw() can not draw filter, "
        "because the argument(source/target) is null");
    return false;
  }

  auto context = program->getContext();
  auto filterProgram = static_cast<Transform3DProgram*>(const_cast<tgfx::RuntimeProgram*>(program));
  Transform3DStateKey key = {cacheID, containerID, target.width(), target.height()};
  auto stateIter = filterProgram->depthStates.find(key);
  if (stateIter == filterProgram->depthStates.end()) {
    auto rboID = MakeRenderBuffer(context, target.width(), target.height());
    if (rboID == 0) {
      return false;
    }
    Transform3DDepthState state = {};
    state.renderBuffer = rboID;
    stateIter = filterProgram->depthStates.insert(std::make_pair(key, state)).first;
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

  tgfx::GLFrameBufferInfo targetInfo;
  target.getGLFramebufferInfo(&targetInfo);
  CheckGLError(context);
  auto needsMSAA = sampleCount() > 1;
  EnableMultisample(context, needsMSAA);
  auto gl = tgfx::GLFunctions::Get(context);
  gl->useProgram(filterProgram->program);
  gl->disable(GL_SCISSOR_TEST);
  gl->enable(GL_BLEND);
  gl->blendEquation(GL_FUNC_ADD);
  gl->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl->bindFramebuffer(GL_FRAMEBUFFER, targetInfo.id);
  gl->viewport(0, 0, target.width(), target.height());
  for (size_t i = 0; i < inputTextures.size(); i++) {
    tgfx::GLTextureInfo sourceInfo;
    if (!inputTextures[i].getGLTextureInfo(&sourceInfo)) {
      return false;
    }
    ActiveGLTexture(context, static_cast<int>(i), &sourceInfo);
  }

  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              depthState.renderBuffer);

  // A complete framebuffer is required before the depth buffer can be attached and drawn into.
  auto status = gl->checkFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    LOGE("Framebuffer is not complete, status: 0x%x", status);
    LOGE("  Target size: %dx%d, Composition size: %dx%d", target.width(), target.height(), width,
         height);
    return false;
  }

  if (newPass) {
    gl->clearDepthf(-1.0);
    gl->clear(GL_DEPTH_BUFFER_BIT);
  }

  // The clear color is shared with tgfx, so set it explicitly to keep the target transparent.
  // tgfx invalidates its GL state cache after this call, so the value does not have to be restored.
  gl->clearColor(0.0f, 0.0f, 0.0f, 0.0f);
  gl->clear(GL_COLOR_BUFFER_BIT);
  auto enableDepth = gl->isEnabled(GL_DEPTH_TEST);
  gl->enable(GL_DEPTH_TEST);
  int depthFunc = 0;
  gl->getIntegerv(GL_DEPTH_FUNC, &depthFunc);
  gl->depthFunc(GL_GREATER);

  depthState.lastLayerIndex = layerIndex;

  onUpdateParams(context, filterProgram, target);
  auto vertices = computeVertices(inputTextures, target, offset);
  bindVertices(context, filterProgram, vertices);
  gl->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (filterProgram->vertexArray > 0) {
    gl->bindVertexArray(0);
  }
  DisableMultisample(context, needsMSAA);
  // Detach the depth render buffer so the framebuffer owned by tgfx is left untouched.
  gl->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
  gl->bindRenderbuffer(GL_RENDERBUFFER, 0);
  gl->depthFunc(depthFunc);
  if (enableDepth) {
    gl->enable(GL_DEPTH_TEST);
  } else {
    gl->disable(GL_DEPTH_TEST);
  }
  return CheckGLError(context);
}

tgfx::Rect Transform3DFilter::filterBounds(const tgfx::Rect&) const {
  return tgfx::Rect::MakeXYWH(0.f, 0.f, static_cast<float>(width), static_cast<float>(height));
}

Transform3DFilter::Transform3DFilter(const Transform3DLayerState& state)
    : cacheID(state.cacheID), containerID(state.containerID), layerIndex(state.layerIndex),
      width(state.width), height(state.height), frameSerial(state.frameSerial),
      matrix(state.matrix) {
}

void Transform3DFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                       const tgfx::BackendRenderTarget& target) const {
  auto uniform = static_cast<Transform3DUniforms*>(program->uniforms.get());
  auto gl = tgfx::GLFunctions::Get(context);
  // Map the fixed pixel area of the composition onto the target area.
  float targetWidth = static_cast<float>(target.width());
  float targetHeight = static_cast<float>(target.height());
  float compWidth = static_cast<float>(width);
  float compHeight = static_cast<float>(height);

  float ndcLeft = -1.0f;
  float ndcRight = (compWidth / targetWidth) * 2.0f - 1.0f;
  float ndcBottom = -1.0f;
  float ndcTop = (compHeight / targetHeight) * 2.0f - 1.0f;

  float scaleX = (ndcRight - ndcLeft) / 2.0f;
  float scaleY = (ndcTop - ndcBottom) / 2.0f;
  float translateX = (ndcRight + ndcLeft) / 2.0f;
  float translateY = (ndcTop + ndcBottom) / 2.0f;

  Matrix3D viewportMatrix = {};
  viewportMatrix.setScale(scaleX, scaleY, 1.0f);
  viewportMatrix.postTranslate(translateX, translateY, 0.0f);

  auto finalMatrix = matrix;
  finalMatrix.postConcat(viewportMatrix);
  std::array<float, 16> matrixData;
  finalMatrix.getRowMajor(matrixData.data());

  gl->uniformMatrix4fv(uniform->matrixHandle, 1, GL_TRUE, matrixData.data());
}

std::vector<float> Transform3DFilter::computeVertices(
    const std::vector<tgfx::BackendTexture>& sources, const tgfx::BackendRenderTarget& target,
    const tgfx::Point& offset) const {
  auto inputBounds = tgfx::Rect::MakeWH(sources[0].width(), sources[0].height());
  auto outBounds = tgfx::Rect::MakeWH(target.width(), target.height());
  tgfx::Point contentPoints[4] = {{outBounds.left, outBounds.bottom},
                                  {outBounds.right, outBounds.bottom},
                                  {outBounds.left, outBounds.top},
                                  {outBounds.right, outBounds.top}};
  tgfx::Point texturePoints[4] = {{inputBounds.left, inputBounds.bottom},
                                  {inputBounds.right, inputBounds.bottom},
                                  {inputBounds.left, inputBounds.top},
                                  {inputBounds.right, inputBounds.top}};
  std::vector<float> results = {};
  results.reserve(4 * 5);
  for (size_t index = 0; index < 4; index++) {
    auto vertexPoint = ToGLVertexPoint(target, contentPoints[index] + offset);
    results.push_back(vertexPoint.x);
    results.push_back(vertexPoint.y);
    results.push_back(0.0f);
    auto texturePoint = ToGLTexturePoint(&sources[0], texturePoints[index]);
    results.push_back(texturePoint.x);
    results.push_back(texturePoint.y);
  }
  return results;
}

void Transform3DFilter::bindVertices(tgfx::Context* context, const RuntimeProgram* program,
                                     const std::vector<float>& points) const {
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = program->uniforms.get();
  if (program->vertexArray > 0) {
    gl->bindVertexArray(program->vertexArray);
  }
  gl->bindBuffer(GL_ARRAY_BUFFER, program->vertexBuffer);
  gl->bufferData(GL_ARRAY_BUFFER, static_cast<tgfx::GLsizeiptr>(points.size() * sizeof(float)),
                 points.data(), GL_STREAM_DRAW);
  gl->vertexAttribPointer(static_cast<unsigned>(uniform->positionHandle), 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), static_cast<void*>(0));
  gl->enableVertexAttribArray(static_cast<unsigned>(uniform->positionHandle));
  gl->vertexAttribPointer(uniform->textureCoordHandle, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
  gl->enableVertexAttribArray(uniform->textureCoordHandle);
  gl->bindBuffer(GL_ARRAY_BUFFER, 0);
}

}  // namespace pag
