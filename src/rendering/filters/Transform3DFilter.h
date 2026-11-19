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

#include <map>
#include <memory>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/RuntimeFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "rendering/filters/utils/Matrix3D.h"

namespace pag {

class RenderCache;

class Transform3DUniforms : public Uniforms {
 public:
  Transform3DUniforms(tgfx::Context* context, unsigned program);

  int matrixHandle = -1;
};

/**
 * Identifies the depth render buffer shared by the 3D layers of one composition. The target size is
 * part of the key because a render buffer cannot be attached to a framebuffer of another size.
 */
struct Transform3DStateKey {
  ID cacheID = 0;
  int containerID = 0;
  int width = 0;
  int height = 0;

  bool operator<(const Transform3DStateKey& other) const;
};

struct Transform3DDepthState {
  unsigned renderBuffer = 0;
  uint64_t frameSerial = 0;
  int lastLayerIndex = -1;
  bool valid = false;
};

/**
 * Holds the GPU resources shared by all the 3D layers of one composition. The layers of a
 * composition are drawn in separate passes, so the depth render buffer has to live across them to
 * resolve the occlusion between layers.
 */
class Transform3DProgram : public RuntimeProgram {
 public:
  static std::unique_ptr<Transform3DProgram> Make(tgfx::Context* context);

  std::map<Transform3DStateKey, Transform3DDepthState> depthStates = {};

 protected:
  void onReleaseGPU() override;

  Transform3DProgram(tgfx::Context* context, unsigned program);
};

/** The per-draw state that identifies the layer and the render pass it belongs to. */
struct Transform3DLayerState {
  ID cacheID = 0;
  int containerID = 0;
  int layerIndex = -1;
  int width = 0;
  int height = 0;
  uint64_t frameSerial = 0;
  Matrix3D matrix = {};
};

class Transform3DFilter : public tgfx::RuntimeEffect {
 public:
  /**
   * Appends the 3D transform of the specified layer to the input image. Returns nullptr when the
   * layer cannot be rendered with 3D instead of falling back to untransformed 2D content.
   */
  static std::shared_ptr<tgfx::Image> Apply(const std::shared_ptr<tgfx::Image>& input,
                                            RenderCache* cache, Layer* layer, Frame layerFrame,
                                            const tgfx::Rect& contentBounds,
                                            const tgfx::Point& sourceScale, tgfx::Point* offset);

  /** Returns true if the layer is a camera flagged active and inside its own time range. */
  static bool IsActiveCamera(const Layer* layer, Frame frame);

  DEFINE_RUNTIME_EFFECT_PROGRAM_ID

  explicit Transform3DFilter(const Transform3DLayerState& state);

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 protected:
  std::unique_ptr<tgfx::RuntimeProgram> onCreateProgram(tgfx::Context* context) const override;

  bool onDraw(const tgfx::RuntimeProgram* program, const std::vector<tgfx::BackendTexture>& sources,
              const tgfx::BackendRenderTarget& target, const tgfx::Point& offset) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const tgfx::BackendRenderTarget& target) const;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const;

  void bindVertices(tgfx::Context* context, const RuntimeProgram* program,
                    const std::vector<float>& points) const;

 private:
  ID cacheID = 0;
  int containerID = 0;
  int layerIndex = -1;
  int width = 0;
  int height = 0;
  uint64_t frameSerial = 0;
  Matrix3D matrix = {};
};

}  // namespace pag
