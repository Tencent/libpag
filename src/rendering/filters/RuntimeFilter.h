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

#pragma once

#include "tgfx/gpu/RuntimeEffect.h"
#include "tgfx/gpu/RuntimeProgram.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
namespace pag {

std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds);

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

class RuntimeProgram : public tgfx::RuntimeProgram {
 public:
  static std::unique_ptr<RuntimeProgram> Make(tgfx::Context* context, const std::string& vertex,
                                              const std::string& fragment);

  unsigned program = 0;
  unsigned int vertexArray = 0;
  unsigned int vertexBuffer = 0;

  std::unique_ptr<Uniforms> uniforms = nullptr;

 protected:
  void onReleaseGPU() override;

  explicit RuntimeProgram(tgfx::Context* context) : tgfx::RuntimeProgram(context) {
  }
};

class RuntimeFilter : public tgfx::RuntimeEffect {
 public:
  explicit RuntimeFilter(const std::vector<std::shared_ptr<tgfx::Image>>& extraInputs = {})
      : RuntimeEffect(extraInputs) {
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
  virtual void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                              const std::vector<tgfx::BackendTexture>& sources) const;

  /**
   * filter的收集顶点数据的接口
   * 如果顶点数据和普通滤镜不一致，需要需重载该接口。
   * @param sources : 滤镜输入的纹理
   * @param target : 滤镜输出的纹理
   * @param offset : 滤镜输出的纹理的偏移量
   * @return : 返回所有顶点数据
   */
  virtual std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                             const tgfx::BackendRenderTarget& target,
                                             const tgfx::Point& offset) const;

  virtual void bindVertices(tgfx::Context* context, const RuntimeProgram* program,
                            const std::vector<float>& points) const;
};

}  // namespace pag
