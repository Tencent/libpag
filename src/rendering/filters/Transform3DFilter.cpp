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
#include <climits>
#include <cmath>
#include <cstring>
#include "base/utils/MathUtil.h"
#include "rendering/filters/utils/Filter3DFactory.h"
#include "rendering/filters/utils/Std140Uniforms.h"
#include "rendering/renderers/TransformRenderer.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
namespace {

struct SampledTransform {
  Point3D position = {};
  Point3D anchor = {};
  Point3D scale = {1, 1, 1};
  Point3D orientation = {};
  Point3D rotation = {};
};

Point3D SamplePosition(Transform3D* transform, Frame frame) {
  if (transform->position != nullptr) {
    return transform->position->getValueAt(frame);
  }
  return {transform->xPosition->getValueAt(frame), transform->yPosition->getValueAt(frame),
          transform->zPosition->getValueAt(frame)};
}

SampledTransform SampleTransform(Transform3D* transform, Frame frame) {
  SampledTransform sample;
  sample.position = SamplePosition(transform, frame);
  sample.anchor = transform->anchorPoint ? transform->anchorPoint->getValueAt(frame) : Point3D{};
  sample.scale = transform->scale->getValueAt(frame);
  sample.orientation = transform->orientation->getValueAt(frame);
  sample.rotation = {transform->xRotation->getValueAt(frame),
                     transform->yRotation->getValueAt(frame),
                     transform->zRotation->getValueAt(frame)};
  return sample;
}

tgfx::Matrix3D BuildRotationMatrix(const SampledTransform& sample) {
  auto matrix = tgfx::Matrix3D::MakeRotate({0, 0, 1}, sample.rotation.z);
  matrix.postRotate({0, 1, 0}, -sample.rotation.y);
  matrix.postRotate({1, 0, 0}, -sample.rotation.x);
  matrix.postRotate({0, 0, 1}, sample.orientation.z);
  matrix.postRotate({0, 1, 0}, -sample.orientation.y);
  matrix.postRotate({1, 0, 0}, -sample.orientation.x);
  return matrix;
}

tgfx::Matrix3D BuildTransformMatrix(Layer* layer, Frame frame) {
  if (layer->transform3D == nullptr) {
    Transform transform;
    if (layer->transform != nullptr) {
      RenderTransform(&transform, layer->transform, frame);
    }
    return tgfx::Matrix3D(transform.matrix);
  }
  auto sample = SampleTransform(layer->transform3D, frame);
  auto matrix = tgfx::Matrix3D::MakeTranslate(-sample.anchor.x, -sample.anchor.y, sample.anchor.z);
  matrix.postScale(sample.scale.x, sample.scale.y, sample.scale.z);
  matrix.postConcat(BuildRotationMatrix(sample));
  matrix.postTranslate(sample.position.x, sample.position.y, -sample.position.z);
  return matrix;
}

bool MatrixIsFinite(const tgfx::Matrix3D& matrix) {
  float values[16];
  matrix.getColumnMajor(values);
  return FloatsAreFinite(values, 16);
}

tgfx::Matrix3D BuildViewMatrix(int width, int height, CameraLayer* camera, Frame frame,
                               float zoom) {
  tgfx::Vec3 eye = {static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f, zoom};
  tgfx::Vec3 center = {eye.x, eye.y, 0};
  tgfx::Vec3 up = {0, 1, 0};
  if (camera != nullptr && camera->transform3D != nullptr) {
    auto sample = SampleTransform(camera->transform3D, frame);
    auto parent = Transform3DFilter::BuildModelMatrix(camera->parent, frame);
    eye = parent.mapPoint({sample.position.x, sample.position.y, -sample.position.z});
    auto rotation = BuildRotationMatrix(sample);
    if (camera->transform3D->anchorPoint != nullptr) {
      center = parent.mapPoint({sample.anchor.x, sample.anchor.y, -sample.anchor.z});
      up = parent.mapVector(up);
    } else {
      auto direction = parent.mapVector(rotation.mapVector({0, 0, -1}));
      center = eye + direction;
      up = parent.mapVector(rotation.mapVector(up));
    }
    float values[] = {eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z};
    auto direction = center - eye;
    if (!FloatsAreFinite(values, 9) || FloatNearlyZero(direction.length()) ||
        FloatNearlyZero(direction.cross(up).length())) {
      return tgfx::Matrix3D::MakeTranslate(-static_cast<float>(width) * 0.5f,
                                           -static_cast<float>(height) * 0.5f, -zoom);
    }
    auto view = tgfx::Matrix3D::LookAt(eye, center, up);
    if (camera->transform3D->anchorPoint != nullptr) {
      tgfx::Matrix3D inverseRotation;
      if (rotation.invert(&inverseRotation)) {
        view.postConcat(inverseRotation);
      }
    }
    return view;
  }
  return tgfx::Matrix3D::LookAt(eye, center, up);
}

float ClipDistance(const tgfx::Vec4& point, int plane) {
  switch (plane) {
    case 0:
      return point.w + point.x;
    case 1:
      return point.w - point.x;
    case 2:
      return point.w + point.y;
    case 3:
      return point.w - point.y;
    case 4:
      return point.w + point.z;
    default:
      return point.w - point.z;
  }
}

std::vector<tgfx::Vec4> ClipPolygon(const std::vector<tgfx::Vec4>& input, int plane) {
  std::vector<tgfx::Vec4> result;
  if (input.empty()) {
    return result;
  }
  auto previous = input.back();
  auto previousDistance = ClipDistance(previous, plane);
  for (auto current : input) {
    auto distance = ClipDistance(current, plane);
    if ((distance >= 0) != (previousDistance >= 0)) {
      auto ratio = previousDistance / (previousDistance - distance);
      result.push_back(previous + (current - previous) * ratio);
    }
    if (distance >= 0) {
      result.push_back(current);
    }
    previous = current;
    previousDistance = distance;
  }
  return result;
}

// 统一输出零到 w 的裁剪深度，并在片元阶段检查原视锥，兼容 GL 和零到一深度的后端。
static constexpr char VERTEX_SHADER[] = R"(
  in vec2 aPosition;
  in vec2 aTextureCoord;
  layout(std140) uniform VertexUniforms { mat4 uMatrix; };
  out vec2 vertexColor;
  out vec2 clipDepth;
  void main() {
    vec4 position = uMatrix * vec4(aPosition, 0.0, 1.0);
    clipDepth = position.zw;
    position.z = (position.z + position.w) * 0.5;
    gl_Position = position;
    vertexColor = aTextureCoord;
  }
)";

static constexpr char FRAGMENT_SHADER[] = R"(
  precision highp float;
  in vec2 vertexColor;
  in vec2 clipDepth;
  uniform sampler2D sTexture;
  out vec4 tgfx_FragColor;
  void main() {
    if (clipDepth.x < -clipDepth.y || clipDepth.x > clipDepth.y || clipDepth.y <= 0.0) {
      discard;
    }
    tgfx_FragColor = texture(sTexture, vertexColor);
  }
)";

struct alignas(16) VertexUniforms {
  std::array<float, 16> matrixData;
};
static_assert(HasStd140UniformLayout<VertexUniforms, 64>());
static_assert(offsetof(VertexUniforms, matrixData) == 0);

}  // namespace

std::shared_ptr<Transform3DContext> Transform3DFilter::MakeContext(VectorComposition* composition,
                                                                   Frame frame) {
  CameraLayer* camera = nullptr;
  for (auto layer : composition->layers) {
    if (layer->type() == LayerType::Camera && layer->isActive && frame >= layer->startTime &&
        frame < layer->startTime + layer->duration) {
      camera = static_cast<CameraLayer*>(layer);
      break;
    }
  }
  return MakeContext(composition->width, composition->height, camera, frame);
}

std::shared_ptr<Transform3DContext> Transform3DFilter::MakeContext(int width, int height,
                                                                   CameraLayer* camera,
                                                                   Frame frame) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  float zoom = camera ? camera->cameraOption->zoom->getValueAt(frame) : 1000.0f;
  if (!std::isfinite(zoom) || zoom <= 0) {
    zoom = 1000.0f;
  }
  auto context = std::make_shared<Transform3DContext>();
  context->width = width;
  context->height = height;
  auto fov = RadiansToDegrees(2.0f * atanf(static_cast<float>(height) * 0.5f / zoom));
  auto nearPlane = std::min(1.0f, zoom * 0.001f);
  auto farPlane = std::max(100000.0f, zoom * 100.0f);
  auto projection =
      tgfx::Matrix3D::Perspective(fov, static_cast<float>(width) / height, nearPlane, farPlane);
  context->viewProjection = BuildViewMatrix(width, height, camera, frame, zoom);
  context->viewProjection.postConcat(projection);
  return MatrixIsFinite(context->viewProjection) ? context : nullptr;
}

tgfx::Matrix3D Transform3DFilter::BuildModelMatrix(Layer* layer, Frame frame) {
  tgfx::Matrix3D matrix;
  while (layer != nullptr) {
    matrix.postConcat(BuildTransformMatrix(layer, frame));
    layer = layer->parent;
  }
  return matrix;
}

tgfx::Matrix Transform3DFilter::ProjectedMatrix(Layer* layer, Frame frame,
                                                const Transform3DContext& context) {
  auto matrix = BuildModelMatrix(layer, frame);
  matrix.postConcat(context.viewProjection);
  auto viewport = tgfx::Matrix3D::MakeScale(context.width * 0.5f, context.height * 0.5f, 1);
  viewport.postTranslate(context.width * 0.5f, context.height * 0.5f, 0);
  matrix.postConcat(viewport);
  return matrix.asMatrix();
}

// 直接以双精度解投影平面的深度系数，避免单精度逆矩阵引入伪斜率和共面排序误差。
bool Transform3DFilter::DepthPlane(Layer* layer, Frame frame, const Transform3DContext& context,
                                   const tgfx::Matrix& extraMatrix, Transform3DDepthPlane* plane) {
  auto matrix = BuildModelMatrix(layer, frame);
  matrix.postConcat(context.viewProjection);
  auto viewport = tgfx::Matrix::MakeScale(context.width * 0.5f, context.height * 0.5f);
  viewport.postTranslate(context.width * 0.5f, context.height * 0.5f);
  tgfx::Matrix inverseViewport;
  if (!viewport.invert(&inverseViewport)) {
    return false;
  }
  auto extra = viewport;
  extra.postConcat(extraMatrix);
  extra.postConcat(inverseViewport);
  matrix.postConcat(tgfx::Matrix3D(extra));
  if (!MatrixIsFinite(matrix)) {
    return false;
  }
  auto rowX = matrix.getRow(0);
  auto rowY = matrix.getRow(1);
  auto rowZ = matrix.getRow(2);
  auto rowW = matrix.getRow(3);
  double x[] = {rowX.x, rowX.y, rowX.w};
  double y[] = {rowY.x, rowY.y, rowY.w};
  double z[] = {rowZ.x, rowZ.y, rowZ.w};
  double w[] = {rowW.x, rowW.y, rowW.w};
  double cofactors[3][3] = {
      {y[1] * w[2] - y[2] * w[1], y[2] * w[0] - y[0] * w[2], y[0] * w[1] - y[1] * w[0]},
      {w[1] * x[2] - w[2] * x[1], w[2] * x[0] - w[0] * x[2], w[0] * x[1] - w[1] * x[0]},
      {x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0]}};
  auto determinant = x[0] * cofactors[0][0] + x[1] * cofactors[0][1] + x[2] * cofactors[0][2];
  double tolerance = FLOAT_NEARLY_ZERO;
  if (std::abs(determinant) <= tolerance * tolerance * tolerance) {
    return false;
  }
  double coefficients[3] = {};
  for (size_t i = 0; i < 3; i++) {
    coefficients[i] =
        (z[0] * cofactors[i][0] + z[1] * cofactors[i][1] + z[2] * cofactors[i][2]) / determinant;
  }
  *plane = {coefficients[0] * 0.5, coefficients[1] * 0.5, (coefficients[2] + 1) * 0.5};
  return std::isfinite(plane->x) && std::isfinite(plane->y) && std::isfinite(plane->z);
}

void Transform3DFilter::TransformBounds(tgfx::Rect* bounds, Layer* layer, Frame frame,
                                        const Transform3DContext& context) {
  if (bounds->isEmpty()) {
    return;
  }
  auto matrix = BuildModelMatrix(layer, frame);
  matrix.postConcat(context.viewProjection);
  if (!MatrixIsFinite(matrix)) {
    bounds->setEmpty();
    return;
  }
  std::vector<tgfx::Vec4> polygon = {matrix.mapHomogeneous(bounds->left, bounds->top, 0, 1),
                                     matrix.mapHomogeneous(bounds->right, bounds->top, 0, 1),
                                     matrix.mapHomogeneous(bounds->right, bounds->bottom, 0, 1),
                                     matrix.mapHomogeneous(bounds->left, bounds->bottom, 0, 1)};
  for (int plane = 0; plane < 6; plane++) {
    polygon = ClipPolygon(polygon, plane);
  }
  bounds->setEmpty();
  bool first = true;
  for (auto point : polygon) {
    if (point.w <= 0) {
      continue;
    }
    auto x = (point.x / point.w + 1) * 0.5f * context.width;
    auto y = (point.y / point.w + 1) * 0.5f * context.height;
    if (first) {
      bounds->setLTRB(x, y, x, y);
      first = false;
    }
    bounds->left = std::min(bounds->left, x);
    bounds->right = std::max(bounds->right, x);
    bounds->top = std::min(bounds->top, y);
    bounds->bottom = std::max(bounds->bottom, y);
  }
}

std::shared_ptr<tgfx::Image> Transform3DFilter::Apply(
    const std::shared_ptr<tgfx::Image>& input, RenderCache* cache, Layer* layer, Frame layerFrame,
    const tgfx::Rect& contentBounds, const tgfx::Point& sourceScale,
    const Transform3DContext& context, tgfx::Point* offset) {
  offset->set(0, 0);
  auto widthValue = ceilf(context.width * sourceScale.x);
  auto heightValue = ceilf(context.height * sourceScale.y);
  if (input == nullptr || !std::isfinite(widthValue) || !std::isfinite(heightValue) ||
      widthValue <= 0 || heightValue <= 0 || widthValue >= static_cast<float>(INT_MAX) ||
      heightValue >= static_cast<float>(INT_MAX) || sourceScale.x <= 0 || sourceScale.y <= 0) {
    return nullptr;
  }
  auto sourceBounds = contentBounds;
  sourceBounds.scale(sourceScale.x, sourceScale.y);
  sourceBounds.roundOut();
  auto matrix = tgfx::Matrix3D::MakeScale(1.0f / sourceScale.x, 1.0f / sourceScale.y, 1);
  matrix.postTranslate(sourceBounds.x() / sourceScale.x, sourceBounds.y() / sourceScale.y, 0);
  matrix.postConcat(BuildModelMatrix(layer, layerFrame));
  matrix.postConcat(context.viewProjection);
  // 向上取整的纹理仅增加边缘留白，不能拉伸逻辑视口。
  auto viewportScaleX = context.width * sourceScale.x / widthValue;
  auto viewportScaleY = context.height * sourceScale.y / heightValue;
  matrix.postScale(viewportScaleX, viewportScaleY, 1);
  matrix.postTranslate(viewportScaleX - 1, viewportScaleY - 1, 0);
  if (!MatrixIsFinite(matrix)) {
    return nullptr;
  }
  auto filter = std::make_shared<Transform3DFilter>(cache, static_cast<int>(widthValue),
                                                    static_cast<int>(heightValue), matrix);
  auto result = input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
  if (result != nullptr) {
    offset->offset(-sourceBounds.x(), -sourceBounds.y());
  }
  return result;
}

Transform3DFilter::Transform3DFilter(RenderCache* cache, int width, int height,
                                     const tgfx::Matrix3D& matrix)
    : RuntimeFilter(cache), width(width), height(height), matrix(matrix) {
}

bool Transform3DFilter::onDraw(tgfx::CommandEncoder* encoder,
                               const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                               std::shared_ptr<tgfx::Texture> outputTexture,
                               const tgfx::Point& offset) const {
  if (encoder == nullptr || outputTexture == nullptr || !Has3DSupport(encoder->gpu())) {
    return false;
  }
  auto bounds = tgfx::Rect::MakeWH(outputTexture->width(), outputTexture->height());
  bounds.offset(-offset.x, -offset.y);
  if (!tgfx::Rect::MakeWH(width, height).contains(bounds)) {
    return false;
  }
  auto croppedMatrix = matrix;
  auto targetWidth = static_cast<float>(outputTexture->width());
  auto targetHeight = static_cast<float>(outputTexture->height());
  croppedMatrix.postScale(width / targetWidth, height / targetHeight, 1);
  croppedMatrix.postTranslate((width + 2 * offset.x) / targetWidth - 1,
                              (height + 2 * offset.y) / targetHeight - 1, 0);
#ifdef TGFX_USE_WEBGPU
  croppedMatrix.postScale(1.0f, -1.0f, 1.0f);
#endif
  Transform3DFilter cropped(cache, outputTexture->width(), outputTexture->height(), croppedMatrix);
  VertexUniforms uniforms = {};
  croppedMatrix.getColumnMajor(uniforms.matrixData.data());
  cropped.uniformBuffer =
      encoder->gpu()->createBuffer(sizeof(uniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (cropped.uniformBuffer == nullptr) {
    return false;
  }
  auto data = cropped.uniformBuffer->map();
  if (data == nullptr) {
    return false;
  }
  memcpy(data, &uniforms, sizeof(uniforms));
  cropped.uniformBuffer->unmap();
  return cropped.RuntimeFilter::onDraw(encoder, inputTextures, std::move(outputTexture),
                                       tgfx::Point::Zero());
}

tgfx::Rect Transform3DFilter::filterBounds(const tgfx::Rect&) const {
  return tgfx::Rect::MakeWH(width, height);
}

std::string Transform3DFilter::onBuildVertexShader() const {
  return VERTEX_SHADER;
}

std::string Transform3DFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::Attribute> Transform3DFilter::vertexAttributes() const {
  return {{"aPosition", tgfx::VertexFormat::Float2}, {"aTextureCoord", tgfx::VertexFormat::Float2}};
}

std::vector<tgfx::BindingEntry> Transform3DFilter::uniformBlocks() const {
  return {{"VertexUniforms", 0, tgfx::ShaderVisibility::Vertex}};
}

std::vector<float> Transform3DFilter::computeVertices(const tgfx::Texture* source,
                                                      const tgfx::Texture*,
                                                      const tgfx::Point&) const {
  auto w = static_cast<float>(source->width());
  auto h = static_cast<float>(source->height());
  return {0, h, 0, 1, w, h, 1, 1, 0, 0, 0, 0, w, 0, 1, 0};
}

void Transform3DFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU*,
                                         const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                         const tgfx::Point&) const {
  renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(VertexUniforms));
}

}  // namespace pag
