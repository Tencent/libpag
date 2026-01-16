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

#include <memory>
#include "base/utils/UniqueID.h"
#include "pag/types.h"
#include "tgfx/gpu/GPU.h"
#include "tgfx/gpu/RuntimeEffect.h"

namespace pag {

#define DEFINE_RUNTIME_FILTER_TYPE             \
  ID filterType() const override {             \
    static const auto type = UniqueID::Next(); \
    return type;                               \
  }

struct FilterResources {
  std::shared_ptr<tgfx::RenderPipeline> pipeline = nullptr;

  virtual ~FilterResources() = default;
};

class RenderCache;

std::vector<tgfx::Point> ComputeVerticesForMotionBlurAndBulge(const tgfx::Rect& inputBounds,
                                                              const tgfx::Rect& outputBounds);

class RuntimeFilter : public tgfx::RuntimeEffect {
 public:
  explicit RuntimeFilter(RenderCache* cache,
                         const std::vector<std::shared_ptr<tgfx::Image>>& extraInputs = {})
      : RuntimeEffect(extraInputs), cache(cache) {
  }

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, tgfx::MapDirection) const override {
    return filterBounds(srcRect);
  }

  bool onDraw(tgfx::CommandEncoder* encoder,
              const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
              std::shared_ptr<tgfx::Texture> outputTexture,
              const tgfx::Point& offset) const override;

 protected:
  RenderCache* cache = nullptr;

  virtual ID filterType() const = 0;

  virtual tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const {
    return srcRect;
  }

  virtual std::string onBuildVertexShader() const;

  virtual std::string onBuildFragmentShader() const;

  virtual int sampleCount() const {
    return 1;
  }

  virtual std::vector<tgfx::Attribute> vertexAttributes() const;

  virtual std::vector<tgfx::BindingEntry> uniformBlocks() const {
    return {};
  }

  virtual std::vector<tgfx::BindingEntry> textureSamplers() const {
    return {{"sTexture", 0}};
  }

  virtual std::vector<float> computeVertices(const tgfx::Texture* source,
                                             const tgfx::Texture* target,
                                             const tgfx::Point& offset) const;

  virtual size_t vertexCount() const {
    return 4;
  }

  virtual void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                                const tgfx::Point& offset) const;

  std::shared_ptr<tgfx::RenderPipeline> getPipeline(tgfx::GPU* gpu) const;

 private:
  std::shared_ptr<tgfx::RenderPipeline> createPipeline(tgfx::GPU* gpu) const;
};

}  // namespace pag
