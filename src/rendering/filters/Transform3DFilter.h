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

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/RuntimeFilter.h"
#include "rendering/filters/utils/Matrix3D.h"

namespace pag {

struct Transform3DResources : public FilterResources {
  /**
   * Holds the depth state shared by all the 3D layers of one composition. The layers of a
   * composition are drawn in separate render passes, so the depth texture has to live across them
   * to resolve the occlusion between layers.
   */
  struct CompositionState {
    std::shared_ptr<tgfx::Texture> depthTexture = nullptr;
    int depthTextureWidth = 0;
    int depthTextureHeight = 0;
    // The frame sequence in which the composition was drawn last, and whether it was ever drawn.
    uint64_t frameSequence = 0;
    bool frameInitialized = false;
    // The layers already drawn in the current draw pass of the composition.
    std::unordered_set<ID> renderedLayers = {};
  };

  std::unordered_map<ID, CompositionState> compositions = {};
};

class Transform3DFilter : public RuntimeFilter {
 public:
  /**
   * Appends the 3D transform of the specified layer to the input image. Returns nullptr if the
   * layer cannot be rendered with 3D.
   */
  static std::shared_ptr<tgfx::Image> Apply(const std::shared_ptr<tgfx::Image>& input,
                                            RenderCache* cache, Layer* layer, Frame layerFrame,
                                            const tgfx::Rect& contentBounds,
                                            const tgfx::Point& sourceScale, tgfx::Point* offset);

  Transform3DFilter(RenderCache* cache, ID compositionID, ID layerID, int width, int height,
                    int opacity, const Matrix3D& matrix);

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

  void onConfigurePipeline(tgfx::RenderPipelineDescriptor* descriptor) const override;

  std::unique_ptr<FilterResources> onCreateFilterResources() const override;

  void onConfigureRenderPass(tgfx::RenderPassDescriptor* renderPassDesc, FilterResources* resources,
                             tgfx::GPU* gpu,
                             const std::shared_ptr<tgfx::Texture>& outputTexture) const override;

 private:
  Transform3DResources::CompositionState* findCompositionState(
      Transform3DResources* resources) const;

  Transform3DResources::CompositionState* getOrCreateCompositionState(
      Transform3DResources* resources, tgfx::GPU* gpu) const;

  bool prepareFrameState(Transform3DResources* resources,
                         Transform3DResources::CompositionState* state) const;

  void purgeIdleCompositions(Transform3DResources* resources, uint64_t frameSequence) const;

  float opacity = 1.0f;
  ID compositionID = 0;
  ID layerID = 0;
  int width = 0;
  int height = 0;
  Matrix3D matrix = {};
};

}  // namespace pag
