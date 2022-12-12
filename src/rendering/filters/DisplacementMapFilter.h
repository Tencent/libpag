/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "LayerFilter.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
class RenderCache;
class DisplacementMapFilter : public LayerFilter {
 public:
  explicit DisplacementMapFilter(Effect* effect);
  ~DisplacementMapFilter() override = default;

  void update(Frame layerFrame, const tgfx::Rect& contentBounds,
              const tgfx::Rect& transformedBounds, const tgfx::Point& filterScale) override;

  void updateMapTexture(RenderCache* cache, const Graphic* mapGraphic, const Point& size,
                        const Point& displacementSize, const tgfx::Matrix& layerMatrix,
                        const tgfx::Rect& contentBounds);

 protected:
  std::string onBuildFragmentShader() override;

  void onPrepareProgram(tgfx::Context* context, unsigned program) override;

  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale) override;

  int textureWrapMode() const override;

 private:
  Effect* effect = nullptr;
  Enum useForHorizontalDisplacement = DisplacementMapSource::Red;
  float maxHorizontalDisplacement = 0.f;
  Enum useForVerticalDisplacement = DisplacementMapSource::Red;
  float maxVerticalDisplacement = 0.f;
  Enum displacementMapBehavior = DisplacementMapBehavior::CenterMap;
  bool edgeBehavior = false;
  bool expandOutput = true;
  float effectOpacity = 1.f;
  Point _size = Point::Zero();
  Point _displacementSize = Point::Zero();
  tgfx::Matrix _layerMatrix = tgfx::Matrix::I();
  std::shared_ptr<tgfx::Surface> mapSurface = nullptr;

  int flagsHandle = 0;
  int inputMatrixHandle = 0;
  int mapMatrixHandle = 0;
  int inputSizeHandle = 0;
  int selectorMatrixRGBAHandle = 0;
  int selectorMatrixHSLAHandle = 0;
  int selectorOffsetHandle = 0;
  int mapTextureHandle = 0;
  int effectOpacityHandle = 0;
};
}  // namespace pag
