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
#include "rendering/filters/LayerFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "rendering/filters/utils/Matrix3D.h"
#include "tgfx/opengl/GLResource.h"

namespace pag {

class RenderCache;

/**
 * The depth render buffer shared by all the 3D layers of one composition. The layers of a
 * composition are drawn in separate passes, so the depth buffer has to live across them to resolve
 * the occlusion between layers.
 */
class DepthRenderBuffer : public tgfx::GLResource {
 public:
  static std::shared_ptr<const DepthRenderBuffer> Make(tgfx::Context* context, int width,
                                                       int height);

  unsigned id = 0;

 private:
  DepthRenderBuffer() = default;

  void onReleaseGPU() override;
};

/**
 * Identifies the depth render buffer of one composition. The target size is part of the key because
 * a render buffer cannot be attached to a framebuffer of another size.
 */
struct Transform3DStateKey {
  int containerID = 0;
  int width = 0;
  int height = 0;

  bool operator<(const Transform3DStateKey& other) const;
};

struct Transform3DDepthState {
  std::shared_ptr<const DepthRenderBuffer> renderBuffer = nullptr;
  uint64_t frameSerial = 0;
  int lastLayerIndex = -1;
  bool valid = false;
};

class Transform3DFilter : public Filter {
 public:
  Transform3DFilter();

  ~Transform3DFilter() override;

  /** Returns true if the layer is a camera flagged active and inside its own time range. */
  static bool IsActiveCamera(const Layer* layer, Frame frame);

  bool updateLayer(Layer* layer, Frame layerFrame, RenderCache* renderCache);

  bool initialize(tgfx::Context* context) override;

  bool needsMSAA() const override;

  void update(Frame layerFrame, const tgfx::Rect& contentBounds,
              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale);

  void draw(tgfx::Context* context, const FilterSource* source,
            const FilterTarget* target) override;

 protected:
  Frame layerFrame = 0;
  tgfx::Point filterScale = {};
  std::shared_ptr<const FilterProgram> filterProgram = nullptr;

  std::string onBuildVertexShader();

  std::string onBuildFragmentShader();

  void onPrepareProgram(tgfx::Context* context, unsigned program);

  void onUpdateParams(tgfx::Context* context, const FilterSource* source,
                      const FilterTarget* target);

  std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                           const tgfx::Rect& transformedBounds,
                                           const tgfx::Point& filterScale);

  void bindVertices(tgfx::Context* context, const FilterSource* source, const FilterTarget* target,
                    const std::vector<tgfx::Point>& points);

 private:
  tgfx::Context* mContext = nullptr;

  std::map<Transform3DStateKey, Transform3DDepthState> depthStates = {};
  int containerID = 0;
  int layerIndex = -1;
  uint64_t frameSerial = 0;

  tgfx::Rect contentBounds = {};
  tgfx::Rect transformedBounds = {};

  Layer* layer = nullptr;
  Transform3D* layerTransform = nullptr;
  Transform3D* cameraTransform = nullptr;
  CameraOption* cameraOption = nullptr;

  Matrix3D modelMatrix = {};
  Matrix3D viewMatrix = {};
  Matrix3D projectionMatrix = {};

  int modelMatrixHandle = -1;
  int viewMatrixHandle = -1;
  int projectionMatrixHandle = -1;
  int textureMatrixHandle = -1;
  int positionHandle = -1;
  int textureCoordHandle = -1;
};
}  // namespace pag
