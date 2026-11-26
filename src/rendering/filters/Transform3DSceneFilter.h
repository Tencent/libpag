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

#include <array>
#include "Transform3DFilter.h"
#include "pag/types.h"
#include "rendering/filters/RuntimeFilter.h"

namespace pag {

class Transform3DSceneFilter : public RuntimeFilter {
 public:
  /** Sorts the projected layers per pixel by depth while preserving alpha, coplanar order and
   * blend modes. */
  Transform3DSceneFilter(RenderCache* cache,
                         const std::vector<std::shared_ptr<tgfx::Image>>& layers,
                         const std::vector<Transform3DDepthPlane>& planes,
                         const std::vector<BlendMode>& blendModes, int width, int height);

  bool onDraw(tgfx::CommandEncoder* encoder,
              const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
              std::shared_ptr<tgfx::Texture> outputTexture,
              const tgfx::Point& offset) const override;

 protected:
  DEFINE_RUNTIME_FILTER_TYPE
  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;
  std::string onBuildFragmentShader() const override;
  std::vector<tgfx::BindingEntry> uniformBlocks() const override;
  std::vector<tgfx::BindingEntry> textureSamplers() const override;
  void onConfigurePipeline(tgfx::RenderPipelineDescriptor* descriptor) const override;
  std::unique_ptr<FilterResources> onCreateFilterResources() const override;

 private:
  struct Resources : FilterResources {
    int width = 0;
    int height = 0;
    int idleFrames = 0;
    bool used = false;
    std::shared_ptr<tgfx::Texture> depth = nullptr;
    std::shared_ptr<tgfx::Texture> color = nullptr;
    std::array<std::shared_ptr<tgfx::Texture>, 2> encodedDepth = {};
    std::array<std::shared_ptr<tgfx::Texture>, 2> index = {};
    std::array<std::shared_ptr<tgfx::Texture>, 2> accumulated = {};

    /** Returns the bytes of the currently held single-sample textures without mipmaps, both of
     * which use 4 bytes per pixel. */
    size_t memoryUsage() const override;

    /** Releases scene textures that stayed idle for two consecutive frames at the end of a
     * drawing frame. */
    void onFrameEnd() override;

    /** Releases scene textures immediately when a non-drawing query ends, keeping the pipelines
     * and samplers. */
    void onQueryEnd() override;

   private:
    void releaseTextures();
  };

  bool prepareTextures(Resources* resources, tgfx::GPU* gpu) const;
  bool drawPeel(tgfx::CommandEncoder* encoder, Resources* resources,
                const std::vector<std::shared_ptr<tgfx::Texture>>& inputs,
                const std::shared_ptr<tgfx::GPUBuffer>& uniforms, size_t uniformStride,
                const std::shared_ptr<tgfx::GPUBuffer>& vertices, size_t passIndex) const;
  bool drawBlend(tgfx::CommandEncoder* encoder, Resources* resources,
                 const std::shared_ptr<tgfx::RenderPipeline>& pipeline,
                 const std::shared_ptr<tgfx::Texture>& background,
                 const std::shared_ptr<tgfx::GPUBuffer>& vertices, size_t passIndex) const;

  std::vector<Transform3DDepthPlane> planes;
  std::vector<BlendMode> blendModes;
  int width = 0;
  int height = 0;
};

}  // namespace pag
