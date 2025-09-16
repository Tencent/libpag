/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <pag/types.h>
#include "rendering/filters/RuntimeFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Color.h"

namespace pag {
enum class SolidStrokeMode { Normal, Thick };

struct SolidStrokeOption {
  StrokePosition position = StrokePosition::Invalid;
  tgfx::Color color = tgfx::Color::Black();
  float spreadSizeX = 0.0f;
  float spreadSizeY = 0.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;

  bool valid() const {
    return spreadSizeX != 0 || spreadSizeY != 0 || offsetX != 0 || offsetY != 0;
  }
};

class SolidStrokeUniforms : public Uniforms {
 public:
  SolidStrokeUniforms(tgfx::Context* context, unsigned program) : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    originalTextureHandle = gl->getUniformLocation(program, "uOriginalTextureInput");
    isUseOriginalTextureHandle = gl->getUniformLocation(program, "uIsUseOriginalTexture");
    colorHandle = gl->getUniformLocation(program, "uColor");
    sizeHandle = gl->getUniformLocation(program, "uSize");
    isOutsideHandle = gl->getUniformLocation(program, "uIsOutside");
    isCenterHandle = gl->getUniformLocation(program, "uIsCenter");
    isInsideHandle = gl->getUniformLocation(program, "uIsInside");
    CheckGLError(context);
  }
  int colorHandle = -1;
  int sizeHandle = -1;
  int originalTextureHandle = -1;
  int isUseOriginalTextureHandle = -1;
  int isOutsideHandle = -1;
  int isCenterHandle = -1;
  int isInsideHandle = -1;
};

class SolidStrokeFilter : public RuntimeFilter {
 public:
  static std::shared_ptr<tgfx::ImageFilter> CreateFilter(
      const SolidStrokeOption& option, SolidStrokeMode mode,
      std::shared_ptr<tgfx::Image> source = nullptr);

  SolidStrokeFilter(const SolidStrokeOption& option) : option(option) {
  }

  SolidStrokeFilter(const SolidStrokeOption& option, std::shared_ptr<tgfx::Image> originalImage)
      : RuntimeFilter({std::move(originalImage)}), option(option), hasOriginalImage(true) {
  }

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

  SolidStrokeOption option;

  bool hasOriginalImage = false;
};

class SolidStrokeNormalFilter : public SolidStrokeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID
  static std::shared_ptr<SolidStrokeNormalFilter> Make(SolidStrokeOption option,
                                                       std::shared_ptr<tgfx::Image> originalImage);
  explicit SolidStrokeNormalFilter(SolidStrokeOption option) : SolidStrokeFilter(option) {
  }
  SolidStrokeNormalFilter(SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage)
      : SolidStrokeFilter(option, std::move(originalImage)) {
  }

  std::string onBuildFragmentShader() const override;
};

class SolidStrokeThickFilter : public SolidStrokeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID

  static std::shared_ptr<SolidStrokeThickFilter> Make(SolidStrokeOption option,
                                                      std::shared_ptr<tgfx::Image> originalImage);

  explicit SolidStrokeThickFilter(SolidStrokeOption option) : SolidStrokeFilter(option) {
  }

  SolidStrokeThickFilter(SolidStrokeOption option, std::shared_ptr<tgfx::Image> originalImage)
      : SolidStrokeFilter(option, std::move(originalImage)) {
  }

  std::string onBuildFragmentShader() const override;
};

}  // namespace pag
