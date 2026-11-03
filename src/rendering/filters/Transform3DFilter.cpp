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
#include <array>
#include <cstring>
#include "base/utils/Log.h"
#include "base/utils/MathUtil.h"
#include "base/utils/USE.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/gpu/CompareFunction.h"

namespace pag {

// The depth textures of the compositions that have not been drawn for these frames are released.
static constexpr uint64_t IDLE_FRAME_COUNT = 2;

static constexpr char VERTEX_SHADER[] = R"(
    in vec3 aPosition;
    in vec2 aTextureCoord;
    layout(std140) uniform VertexUniforms {
        mat4 uMatrix;
    };
    out vec2 vertexColor;
    void main() {
        gl_Position = uMatrix * vec4(aPosition, 1.0);
        vertexColor = aTextureCoord;
    }
)";

static constexpr char FRAGMENT_SHADER[] = R"(
    precision mediump float;
    in vec2 vertexColor;
    uniform sampler2D sTexture;
    layout(std140) uniform FragmentUniforms {
        float uOpacity;
    };
    out vec4 tgfx_FragColor;

    void main() {
        tgfx_FragColor = texture(sTexture, vertexColor) * uOpacity;
        if (tgfx_FragColor.a <= 0.0) {
            discard;
        }
    }
)";

std::shared_ptr<tgfx::Image> Transform3DFilter::Apply(
    const std::shared_ptr<tgfx::Image>& input, RenderCache* cache, Layer* layer, Frame layerFrame,
    const tgfx::Rect& contentBounds, const tgfx::Point& sourceScale, tgfx::Point* offset) {
  auto transform3D = layer->transform3D;
  Transform3D* cameraTransform = nullptr;
  CameraOption* cameraOption = nullptr;
  Transform3D* parentTransform3D = nullptr;
  Transform2D* parentTransform2D = nullptr;

  if (layer->parent) {
    if (layer->parent->transform3D) {
      parentTransform3D = layer->parent->transform3D;
    } else {
      parentTransform2D = layer->parent->transform;
    }
  }
  auto containingComposition = layer->containingComposition;
  for (auto aLayer : containingComposition->layers) {
    if (aLayer->type() == LayerType::Camera) {
      auto cameraLayer = static_cast<CameraLayer*>(aLayer);
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
  auto opacity = transform3D->opacity->getValueAt(layerFrame);

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
  auto sourceBounds = contentBounds;
  sourceBounds.scale(sourceScale.x, sourceScale.y);
  sourceBounds.roundOut();
  auto sourceOrigin =
      tgfx::Point::Make(sourceBounds.x() / sourceScale.x, sourceBounds.y() / sourceScale.y);

  Matrix3D modelMatrix = {};
  modelMatrix.postTranslate(1.0, 1.0);
  modelMatrix.postConcat(sourceAspectScaleMatrix);
  modelMatrix.postConcat(invertTargetAspectScaleMatrix);
  modelMatrix.postTranslate((-anchorPoint.x + sourceOrigin.x) / scalar * 2.0f,
                            (-anchorPoint.y + sourceOrigin.y) / (scalar * ratio) * 2.0f,
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

  if (parentTransform3D != nullptr) {
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

    auto parentMatrix = Matrix3D();
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

    modelMatrix.postConcat(parentMatrix);
  } else if (parentTransform2D != nullptr) {
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

    auto parentMatrix = Matrix3D();
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

  auto filter = std::make_shared<Transform3DFilter>(
      cache, containingComposition->uniqueID, layer->id, static_cast<int>(targetWidth),
      static_cast<int>(targetHeight), opacity, finalMatrix);
  auto result = input->makeWithFilter(tgfx::ImageFilter::Runtime(filter));
  if (offset) {
    // Cancel the scaled, rounded input origin set by CreatePictureImage().
    offset->offset(-sourceBounds.x(), -sourceBounds.y());
  }
  return result;
}

Transform3DFilter::Transform3DFilter(RenderCache* cache, ID compositionID, ID layerID, int width,
                                     int height, int opacity, const Matrix3D& matrix)
    : RuntimeFilter(cache), opacity(static_cast<float>(opacity) / 255.0f),
      compositionID(compositionID), layerID(layerID), width(width), height(height), matrix(matrix) {
}

bool Transform3DFilter::onDraw(tgfx::CommandEncoder* encoder,
                               const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                               std::shared_ptr<tgfx::Texture> outputTexture,
                               const tgfx::Point& offset) const {
  if (encoder == nullptr || outputTexture == nullptr || inputTextures.empty() ||
      inputTextures[0] == nullptr) {
    return false;
  }
  auto outputBounds =
      tgfx::Rect::MakeXYWH(-offset.x, -offset.y, static_cast<float>(outputTexture->width()),
                           static_cast<float>(outputTexture->height()));
  auto bounds = tgfx::Rect::MakeWH(width, height);
  if (!bounds.contains(outputBounds)) {
    LOGE("Transform3DFilter: Output bounds are outside the composition");
    return false;
  }
  if (outputTexture->width() == width && outputTexture->height() == height) {
    return RuntimeFilter::onDraw(encoder, inputTextures, std::move(outputTexture),
                                 tgfx::Point::Zero());
  }

  // All layers must use the same viewport and depth attachment before output cropping.
  tgfx::TextureDescriptor descriptor(
      width, height, outputTexture->format(), false, 1,
      tgfx::TextureUsage::RENDER_ATTACHMENT | tgfx::TextureUsage::TEXTURE_BINDING);
  auto renderTexture = encoder->gpu()->createTexture(descriptor);
  if (renderTexture == nullptr ||
      !RuntimeFilter::onDraw(encoder, inputTextures, renderTexture, tgfx::Point::Zero())) {
    return false;
  }
  encoder->copyTextureToTexture(std::move(renderTexture), outputBounds, std::move(outputTexture),
                                tgfx::Point::Zero());
  return true;
}

tgfx::Rect Transform3DFilter::filterBounds(const tgfx::Rect&) const {
  return tgfx::Rect::MakeXYWH(0.f, 0.f, static_cast<float>(width), static_cast<float>(height));
}

std::string Transform3DFilter::onBuildVertexShader() const {
  return VERTEX_SHADER;
}

std::string Transform3DFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::Attribute> Transform3DFilter::vertexAttributes() const {
  return {{"aPosition", tgfx::VertexFormat::Float3}, {"aTextureCoord", tgfx::VertexFormat::Float2}};
}

std::vector<tgfx::BindingEntry> Transform3DFilter::uniformBlocks() const {
  return {{"VertexUniforms", 0}, {"FragmentUniforms", 1}};
}

std::vector<float> Transform3DFilter::computeVertices(const tgfx::Texture* source,
                                                      const tgfx::Texture* target,
                                                      const tgfx::Point& offset) const {
  auto inputBounds = tgfx::Rect::MakeWH(source->width(), source->height());
  auto outBounds = tgfx::Rect::MakeWH(target->width(), target->height());
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
    auto vertexPoint = ToVertexPoint(target, contentPoints[index] + offset);
    results.push_back(vertexPoint.x);
    results.push_back(vertexPoint.y);
    results.push_back(0.0f);
    auto texturePoint = ToTexturePoint(source, texturePoints[index]);
    results.push_back(texturePoint.x);
    results.push_back(texturePoint.y);
  }
  return results;
}

void Transform3DFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                         const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                         const tgfx::Point&) const {
  auto resources = static_cast<Transform3DResources*>(getFilterResources(gpu));
  if (resources == nullptr) {
    return;
  }
  struct VertexUniforms {
    std::array<float, 16> matrixData;
  };

  struct FragmentUniforms {
    float opacityValue;
    float padding[3];
  };

  VertexUniforms vertexUniforms = {};
  matrix.getColMajor(vertexUniforms.matrixData.data());
  auto vertexUniformBuffer =
      gpu->createBuffer(sizeof(VertexUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (vertexUniformBuffer != nullptr) {
    auto* data = vertexUniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &vertexUniforms, sizeof(VertexUniforms));
      vertexUniformBuffer->unmap();
      renderPass->setUniformBuffer(0, vertexUniformBuffer, 0, sizeof(VertexUniforms));
    }
  }

  FragmentUniforms fragmentUniforms = {};
  fragmentUniforms.opacityValue = opacity;
  auto fragmentUniformBuffer =
      gpu->createBuffer(sizeof(FragmentUniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (fragmentUniformBuffer != nullptr) {
    auto* data = fragmentUniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &fragmentUniforms, sizeof(FragmentUniforms));
      fragmentUniformBuffer->unmap();
      renderPass->setUniformBuffer(1, fragmentUniformBuffer, 0, sizeof(FragmentUniforms));
    }
  }
}

void Transform3DFilter::onConfigurePipeline(tgfx::RenderPipelineDescriptor* descriptor) const {
  descriptor->depthStencil.depthCompare = tgfx::CompareFunction::GreaterEqual;
  descriptor->depthStencil.depthWriteEnabled = true;
}

std::unique_ptr<FilterResources> Transform3DFilter::onCreateFilterResources() const {
  return std::make_unique<Transform3DResources>();
}

void Transform3DFilter::onConfigureRenderPass(
    tgfx::RenderPassDescriptor* renderPassDesc, FilterResources* resources, tgfx::GPU* gpu,
    const std::shared_ptr<tgfx::Texture>& outputTexture) const {
  auto transform3DResources = static_cast<Transform3DResources*>(resources);
  DEBUG_ASSERT(outputTexture->width() == width && outputTexture->height() == height);
  USE(outputTexture);
  auto state = getOrCreateCompositionState(transform3DResources, gpu);
  if (state == nullptr) {
    return;
  }
  auto isFirstLayerInFrame = prepareFrameState(transform3DResources, state);
  auto loadAction = isFirstLayerInFrame ? tgfx::LoadAction::Clear : tgfx::LoadAction::Load;
  renderPassDesc->depthStencilAttachment = tgfx::DepthStencilAttachment(
      state->depthTexture, loadAction, tgfx::StoreAction::Store, -1.0f, false);
}

Transform3DResources::CompositionState* Transform3DFilter::findCompositionState(
    Transform3DResources* resources) const {
  auto result = resources->compositions.find(compositionID);
  if (result == resources->compositions.end()) {
    return nullptr;
  }
  return &result->second;
}

Transform3DResources::CompositionState* Transform3DFilter::getOrCreateCompositionState(
    Transform3DResources* resources, tgfx::GPU* gpu) const {
  auto& state = resources->compositions[compositionID];
  if (state.depthTexture != nullptr && state.depthTextureWidth == width &&
      state.depthTextureHeight == height) {
    return &state;
  }
  tgfx::TextureDescriptor descriptor(width, height, tgfx::PixelFormat::DEPTH24_STENCIL8, false, 1,
                                     tgfx::TextureUsage::RENDER_ATTACHMENT);
  state.depthTexture = gpu->createTexture(descriptor);
  if (state.depthTexture == nullptr) {
    state.depthTextureWidth = 0;
    state.depthTextureHeight = 0;
    return nullptr;
  }
  state.depthTextureWidth = width;
  state.depthTextureHeight = height;
  return &state;
}

bool Transform3DFilter::prepareFrameState(Transform3DResources* resources,
                                          Transform3DResources::CompositionState* state) const {
  auto frameSequence = cache->frameSequence();
  if (!state->frameInitialized || state->frameSequence != frameSequence) {
    state->frameSequence = frameSequence;
    state->frameInitialized = true;
    purgeIdleCompositions(resources, frameSequence);
    state->renderedLayers.clear();
    state->renderedLayers.insert(layerID);
    return true;
  }
  if (state->renderedLayers.insert(layerID).second) {
    return false;
  }
  // A layer drawn twice within one frame means that the composition started a new draw pass, so
  // the depth values accumulated by the previous pass must be discarded.
  state->renderedLayers.clear();
  state->renderedLayers.insert(layerID);
  return true;
}

void Transform3DFilter::purgeIdleCompositions(Transform3DResources* resources,
                                              uint64_t frameSequence) const {
  for (auto item = resources->compositions.begin(); item != resources->compositions.end();) {
    if (item->second.frameSequence + IDLE_FRAME_COUNT < frameSequence) {
      item = resources->compositions.erase(item);
    } else {
      ++item;
    }
  }
}

}  // namespace pag
