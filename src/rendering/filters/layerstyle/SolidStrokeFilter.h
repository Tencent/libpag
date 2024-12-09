/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <utility>
#include "EffectFilter.h"

namespace pag {
enum class SolidStrokeMode { Normal, Thick };

struct SolidStrokeOption {
  Enum position = -1;
  Color color = Black;
  float opacity = 0.0f;
  float spreadSizeX = 0.0f;
  float spreadSizeY = 0.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;

  bool valid() {
    return opacity != 0 && (spreadSizeX != 0 || spreadSizeY != 0 || offsetX != 0 || offsetY != 0);
  }
};

class SolidStrokeUniforms : public Uniforms {
 public:
  SolidStrokeUniforms(tgfx::Context* context, unsigned program) : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    originalTextureHandle = gl->getUniformLocation(program, "uOriginalTextureInput");
    isUseOriginalTextureHandle = gl->getUniformLocation(program, "uIsUseOriginalTexture");
    colorHandle = gl->getUniformLocation(program, "uColor");
    alphaHandle = gl->getUniformLocation(program, "uAlpha");
    sizeHandle = gl->getUniformLocation(program, "uSize");
    isOutsideHandle = gl->getUniformLocation(program, "uIsOutside");
    isCenterHandle = gl->getUniformLocation(program, "uIsCenter");
    isInsideHandle = gl->getUniformLocation(program, "uIsInside");
    CheckGLError(context);
  }
  int colorHandle = -1;
  int alphaHandle = -1;
  int sizeHandle = -1;
  int originalTextureHandle = -1;
  int isUseOriginalTextureHandle = -1;
  int isOutsideHandle = -1;
  int isCenterHandle = -1;
  int isInsideHandle = -1;
};

class SolidStrokeBaseEffect : public FilterEffect {
 public:
  SolidStrokeBaseEffect(tgfx::UniqueType type, SolidStrokeOption option)
      : FilterEffect(std::move(type)), option(option) {
  }

  SolidStrokeBaseEffect(tgfx::UniqueType type, SolidStrokeOption option,
                        std::shared_ptr<tgfx::Image> originalImage)
      : FilterEffect(std::move(type), {std::move(originalImage)}), option(option),
        hasOriginalImage(true) {
  }

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const EffectProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

  SolidStrokeOption option;

  bool hasOriginalImage = false;
};

class SolidStrokeNormalEffect : public SolidStrokeBaseEffect {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE
  static std::shared_ptr<SolidStrokeNormalEffect> Make(SolidStrokeOption option,
                                                       std::shared_ptr<tgfx::Image> originalImage);
  explicit SolidStrokeNormalEffect(SolidStrokeOption option)
      : SolidStrokeBaseEffect(Type(), option) {
  }
  SolidStrokeNormalEffect(SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage)
      : SolidStrokeBaseEffect(Type(), option, std::move(originalImage)) {
  }

  std::string onBuildFragmentShader() const override;
};

class SolidStrokeThickEffect : public SolidStrokeBaseEffect {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE

  static std::shared_ptr<SolidStrokeThickEffect> Make(SolidStrokeOption option,
                                                      std::shared_ptr<tgfx::Image> originalImage);

  explicit SolidStrokeThickEffect(SolidStrokeOption option)
      : SolidStrokeBaseEffect(Type(), option) {
  }

  SolidStrokeThickEffect(SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage)
      : SolidStrokeBaseEffect(Type(), option, std::move(originalImage)) {
  }

  std::string onBuildFragmentShader() const override;
};

class SolidStrokeFilter : public EffectFilter {
 public:
  SolidStrokeFilter() = default;
  ~SolidStrokeFilter() override = default;

  void onUpdateOption(SolidStrokeOption option);

  void onUpdateOriginalImage(std::shared_ptr<tgfx::Image> originalImage);

  void setSolidStrokeMode(SolidStrokeMode mode) {
    styleMode = mode;
  }

  std::shared_ptr<tgfx::RuntimeEffect> onCreateEffect(
      Frame layerFrame, const tgfx::Point& filterScale) const override;

 private:
  SolidStrokeOption option;
  SolidStrokeMode styleMode;
  std::shared_ptr<tgfx::Image> originalImage;
};
}  // namespace pag
