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

#pragma once

#include "pag/file.h"
#include "rendering/filters/RuntimeFilter.h"
#include "tgfx/core/Matrix3D.h"

namespace pag {

/** Window depth coefficients retained at binary64 precision until GPU encoding. */
struct Transform3DDepthPlane {
  double x = 0;
  double y = 0;
  double z = 0;
};

struct Transform3DContext {
  int width = 0;
  int height = 0;
  tgfx::Matrix3D viewProjection = {};
};

class Transform3DFilter : public RuntimeFilter {
 public:
  /** Selects the active camera of the composition and builds the view projection shared by all
   * layers. */
  static std::shared_ptr<Transform3DContext> MakeContext(VectorComposition* composition,
                                                         Frame frame);

  /** Builds the view projection with the given active camera; a null camera means the default
   * camera. */
  static std::shared_ptr<Transform3DContext> MakeContext(int width, int height, CameraLayer* camera,
                                                         Frame frame);

  /** Returns true if the layer is a camera active at the given frame, that is active-flagged and
   * inside its time range. */
  static bool IsActiveCamera(const Layer* layer, Frame frame);

  /** Concatenates the transforms of the layer and its full parent chain in pixel space. */
  static tgfx::Matrix3D BuildModelMatrix(Layer* layer, Frame frame);

  /** Returns the 2D homography including perspective and pixel viewport mapping. */
  static tgfx::Matrix ProjectedMatrix(Layer* layer, Frame frame, const Transform3DContext& context);

  /** Returns the window depth equation of the projected plane, or false for a degenerate plane. */
  static bool DepthPlane(Layer* layer, Frame frame, const Transform3DContext& context,
                         const tgfx::Matrix& extraMatrix, Transform3DDepthPlane* plane);

  /** Projects the content bounds and clips them to the view frustum and the composition area. */
  static void TransformBounds(tgfx::Rect* bounds, Layer* layer, Frame frame,
                              const Transform3DContext& context);

  /** Applies the 3D projection. Returns nullptr when the input or matrix is invalid instead of
   * falling back to untransformed 2D content. */
  static std::shared_ptr<tgfx::Image> Apply(const std::shared_ptr<tgfx::Image>& input,
                                            RenderCache* cache, Layer* layer, Frame layerFrame,
                                            const tgfx::Rect& contentBounds,
                                            const tgfx::Point& sourceScale,
                                            const Transform3DContext& context, tgfx::Point* offset);

  Transform3DFilter(RenderCache* cache, int width, int height, const tgfx::Matrix3D& matrix);

  bool onDraw(tgfx::CommandEncoder* encoder,
              const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
              std::shared_ptr<tgfx::Texture> outputTexture,
              const tgfx::Point& offset) const override;

 protected:
  DEFINE_RUNTIME_FILTER_TYPE
  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;
  std::string onBuildVertexShader() const override;
  std::string onBuildFragmentShader() const override;
  std::vector<tgfx::Attribute> vertexAttributes() const override;
  std::vector<tgfx::BindingEntry> uniformBlocks() const override;
  std::vector<float> computeVertices(const tgfx::Texture* source, const tgfx::Texture* target,
                                     const tgfx::Point& offset) const override;
  void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                        const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                        const tgfx::Point& offset) const override;

 private:
  int width = 0;
  int height = 0;
  tgfx::Matrix3D matrix = {};
  std::shared_ptr<tgfx::GPUBuffer> uniformBuffer = nullptr;
};

}  // namespace pag
