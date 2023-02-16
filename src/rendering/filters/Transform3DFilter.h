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

#include "Filter.h"
#include "LayerFilter.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/gpu/Resource.h"
#include "tgfx/core/Matrix4x4.h"

namespace pag {
class Transform3DFilter : public Filter {
public:
  Transform3DFilter();
  ~Transform3DFilter() override = default;
  
  bool updateLayer(Layer* layer, Frame layerFrame);
  
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
  
  void onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                      const tgfx::Point& filterScale);
  
  std::vector<tgfx::Point> computeVertices(const tgfx::Rect& contentBounds,
                                           const tgfx::Rect& transformedBounds,
                                           const tgfx::Point& filterScale);
  
  void onSubmitTransformations(tgfx::Context* context, const FilterSource* source,
                               const FilterTarget* target);
  
  void bindVertices(tgfx::Context* context, const FilterSource* source,
                    const FilterTarget* target, const std::vector<tgfx::Point>& points);
  
private:
  tgfx::Rect contentBounds = {};
  tgfx::Rect transformedBounds = {};
  
  tgfx::Matrix4x4 modelMatrix = {};
  tgfx::Matrix4x4 viewMatrix = {};
  tgfx::Matrix4x4 projectionMatrix = {};

  int modelMatrixHandle = -1;
  int viewMatrixHandle = -1;
  int projectionMatrixHandle = -1;
  int textureMatrixHandle = -1;
  int positionHandle = -1;
  int textureCoordHandle = -1;
};
}  // namespace pag
