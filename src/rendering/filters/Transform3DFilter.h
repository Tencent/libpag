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
  /** 为当前合成选择活动相机，并创建可供所有图层共享的视图投影。 */
  static std::shared_ptr<Transform3DContext> MakeContext(VectorComposition* composition,
                                                         Frame frame);

  /** 使用指定活动相机创建视图投影，空相机表示默认相机。 */
  static std::shared_ptr<Transform3DContext> MakeContext(int width, int height, CameraLayer* camera,
                                                         Frame frame);

  /** 在像素空间累乘图层及其完整父链的变换。 */
  static tgfx::Matrix3D BuildModelMatrix(Layer* layer, Frame frame);

  /** 返回包含透视和像素视口映射的二维单应矩阵。 */
  static tgfx::Matrix ProjectedMatrix(Layer* layer, Frame frame, const Transform3DContext& context);

  /** 返回投影平面的窗口深度方程，退化平面返回 false。 */
  static bool DepthPlane(Layer* layer, Frame frame, const Transform3DContext& context,
                         const tgfx::Matrix& extraMatrix, Transform3DDepthPlane* plane);

  /** 将内容边界投影并裁剪到视锥和合成区域。 */
  static void TransformBounds(tgfx::Rect* bounds, Layer* layer, Frame frame,
                              const Transform3DContext& context);

  /** 应用三维投影，输入或矩阵无效时返回 nullptr，不退回未变换的二维内容。 */
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
