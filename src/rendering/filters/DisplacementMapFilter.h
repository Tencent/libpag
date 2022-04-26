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

  void updateMapTexture(RenderCache* cache, const Graphic* mapGraphic, const tgfx::Rect& bounds);

 protected:
  std::string onBuildFragmentShader() override;

  void onPrepareProgram(tgfx::Context* context, unsigned program) override;

  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale) override;

 private:
  Effect* effect = nullptr;
  tgfx::Rect mapBounds = {};
  std::shared_ptr<tgfx::Surface> mapSurface = nullptr;

  int useForDisplacementHandle = 0;
  int maxDisplacementHandle = 0;
  int displacementMapBehaviorHandle = 0;
  int edgeBehaviorHandle = 0;
  int expandOutputHandle = 0;
  int mapTextureHandle = 0;
  int mapTextureSizeHandle = 0;
};
}  // namespace pag
