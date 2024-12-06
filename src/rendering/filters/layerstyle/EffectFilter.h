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

#pragma once

#include "LayerStyleFilter.h"
#include "tgfx/gpu/RuntimeProgram.h"
namespace pag {

class Uniforms {
 public:
  Uniforms(tgfx::Context* context, unsigned program) {
    auto gl = tgfx::GLFunctions::Get(context);
    positionHandle = gl->getAttribLocation(program, "aPosition");
    textureCoordHandle = gl->getAttribLocation(program, "aTextureCoord");
  }

  virtual ~Uniforms() = default;

  int positionHandle = -1;
  int textureCoordHandle = -1;
};

class EffectProgram : public tgfx::RuntimeProgram {
 public:
  static std::unique_ptr<EffectProgram> Make(tgfx::Context* context, const std::string& vertex,
                                             const std::string& fragment);

  unsigned program = 0;
  unsigned int vertexArray = 0;
  unsigned int vertexBuffer = 0;

  std::unique_ptr<Uniforms> uniforms = nullptr;

 protected:
  void onReleaseGPU() override;

 private:
  explicit EffectProgram(tgfx::Context* context) : RuntimeProgram(context) {
  }
};

class FilterEffect : public tgfx::RuntimeEffect {
 public:
  explicit FilterEffect(tgfx::UniqueType type,
                        const std::vector<std::shared_ptr<tgfx::Image>>& extraInputs = {})
      : RuntimeEffect(std::move(type), extraInputs) {
  }

  std::unique_ptr<tgfx::RuntimeProgram> onCreateProgram(tgfx::Context* context) const override;

 protected:
  bool onDraw(const tgfx::RuntimeProgram* program, const std::vector<tgfx::BackendTexture>& sources,
              const tgfx::BackendRenderTarget& target, const tgfx::Point& offset) const override;

  virtual std::string onBuildVertexShader() const;

  virtual std::string onBuildFragmentShader() const;

  virtual std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                                     unsigned program) const;

  /**
   * filter的给shader上传数据接口
   */
  virtual void onUpdateParams(tgfx::Context* context, const EffectProgram* program,
                              const std::vector<tgfx::BackendTexture>& sources) const;

  /**
   * filter的收集顶点数据的接口
   * 如果顶点数据和普通滤镜不一致，需要需重载该接口。
   * @param source : 滤镜输入的纹理
   * @param target : 滤镜输出的纹理
   * @param offset : 滤镜输出的纹理的偏移量
   * @return : 返回所有顶点数据
   */
  virtual std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                             const tgfx::BackendRenderTarget& target,
                                             const tgfx::Point& offset) const;

  void bindVertices(tgfx::Context* context, const EffectProgram* program,
                    const std::vector<float>& points) const;
};

class EffectFilter : public LayerStyleFilter {
 public:
  bool draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source, const tgfx::Point& filterScale,
            const tgfx::Matrix& matrix, tgfx::Canvas* target) override;

  std::shared_ptr<tgfx::Image> applyFilterEffect(Frame layerFrame,
                                                 std::shared_ptr<tgfx::Image> source,
                                                 const tgfx::Point& filterScale,
                                                 tgfx::Point* offset) override;

 protected:
  /**
   * 具体effect的创建接口。
   * @param layerFrame : 当前绘制的layerFrame, 主要用来从effect或者style中获取滤镜所需的参数
   * @param filterScale :
   * 滤镜效果的scale，比如LayerStyle本应该在图层matrix之后应用的，但是我们为了简化渲染改到了matrix之前，
   * 因此需要反向排除图层matrix的缩放值
   * 该filterScale等于反向matrix的scale，不包含source.scale，source.scale会整体应用到FilterTarget
   * 上屏的matrix里。
   */
  virtual std::shared_ptr<tgfx::RuntimeEffect> onCreateEffect(Frame layerFrame,
                                                              const tgfx::Point& filterScale) const;
};
}  // namespace pag
