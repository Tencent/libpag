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

#pragma once

#include "Filter.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/gpu/Resource.h"

namespace pag {
std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds);

class FilterProgram : public tgfx::Resource {
 public:
  static std::shared_ptr<const FilterProgram> Make(tgfx::Context* context,
                                                   const std::string& vertex,
                                                   const std::string& fragment);

  unsigned program = 0;
  unsigned int vertexArray = 0;
  unsigned int vertexBuffer = 0;

 private:
  FilterProgram() = default;

  void onReleaseGPU() override;
};

class LayerFilter : public Filter {
 public:
  static std::unique_ptr<LayerFilter> Make(LayerStyle* layerStyle);

  static std::unique_ptr<LayerFilter> Make(Effect* effect);

  bool initialize(tgfx::Context* context) override;

  virtual bool needsMSAA() const override;

  /**
   * filter的绘制接口
   * 单次render pass的滤镜无需重载该接口，只有滤镜需经过多次render pass时，才重载该函数的实现
   * 具体样例参见: DropShadow、Glow、GaussBlur这几个滤镜。
   * @param source : 滤镜的输入source,
   * 该参数里含有纹理的ID、像素级的宽高、纹理点的matrix、以及纹理相对于layer bounds的scale信息
   * @param target : 滤镜的输出target，该参数里含有frame buffer的ID、像素级的宽高、顶点的matrix信息
   */
  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

  /**
   * filter的刷新接口。
   * 单次render pass的滤镜无需重载该接口，只有滤镜需经过多次render pass时，才重载该函数的实现
   * 具体样例参见: DropShadow、Glow、GaussBlur这几个滤镜。
   * @param layerFrame : 当前绘制的layerFrame, 主要用来从effect或者style中获取滤镜所需的参数
   * @param contentBounds : 滤镜输入的基于layer bounds尺寸的矩形区域
   * @param transformedBounds : 滤镜输出的基于layer bounds尺寸的矩形区域
   * @param filterScale :
   * 滤镜效果的scale，比如LayerStyle本应该在图层matrix之后应用的，但是我们为了简化渲染改到了matrix之前，
   *                      因此需要反向排除图层matrix的缩放值
   * 该filterScale等于反向matrix的scale，不包含source.scale，
   *                      source.scale会整体应用到FilterTarget上屏的matrix里。
   */
  virtual void update(Frame layerFrame, const tgfx::Rect& contentBounds,
                      const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale);

 protected:
  Frame layerFrame = 0;
  tgfx::Point filterScale = {};
  std::shared_ptr<const FilterProgram> filterProgram = nullptr;

  virtual std::string onBuildVertexShader();

  virtual std::string onBuildFragmentShader();

  virtual void onPrepareProgram(tgfx::Context* context, unsigned program);

  /**
   * filter的给shader上传数据接口
   * 单次render pass的滤镜需重载该接口。具体样例参见：Bulge、MotionBlur、MotionTile
   * @param contentBounds : 滤镜输入的基于layer bounds尺寸的矩形区域，某些滤镜需要用到content
   * bounds来计算shader中的参数，比如：bulge、MotionTile
   * @param filterScale : 滤镜效果的scale
   */
  virtual void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                              const tgfx::Point& filterScale);

  /**
   * filter的收集顶点数据的接口
   * 单次render pass的滤镜需重载该接口。具体样例参见：Bulge、MotionBlur、MotionTile
   * @param contentBounds : 滤镜输入的基于layer bounds尺寸的矩形区域
   * @param transformedBounds :  滤镜输出的基于layer bounds尺寸的矩形区域
   * @param filterScale : 滤镜效果的scale
   * @return : 返回顶点与纹理的point数组，顶点 + 纹理 + 顶点 +
   * 纹理的组合方式，所有的Point都是基于layer bounds尺寸的点
   */
  virtual std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                                   const tgfx::Rect& transformedBounds,
                                                   const tgfx::Point& filterScale);

  virtual void bindVertices(tgfx::Context* context, const FilterSource* source,
                            const FilterTarget* target, const std::vector<tgfx::Point>& points);

 private:
  tgfx::Rect contentBounds = {};
  tgfx::Rect transformedBounds = {};

  int vertexMatrixHandle = -1;
  int textureMatrixHandle = -1;
  int positionHandle = -1;
  int textureCoordHandle = -1;

  friend class CornerPinFilter;
};
}  // namespace pag
