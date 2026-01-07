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

  void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                        const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                        const tgfx::Point& offset) const override;

  std::vector<tgfx::BindingEntry> uniformBlocks() const override;

  std::vector<tgfx::BindingEntry> textureSamplers() const override;

  std::vector<float> computeVertices(const tgfx::Texture* source, const tgfx::Texture* target,
                                     const tgfx::Point& offset) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

  SolidStrokeOption option;

  bool hasOriginalImage = false;
};

class SolidStrokeNormalFilter : public SolidStrokeFilter {
 public:
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
