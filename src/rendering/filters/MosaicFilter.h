/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "RuntimeFilter.h"
#include "pag/file.h"

namespace pag {

class MosaicFilter : public RuntimeFilter {
 public:
  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                            Effect* effect, Frame layerFrame, tgfx::Point* offset);

  MosaicFilter(RenderCache* cache, float horizontalBlocks, float verticalBlocks, bool sharpColors)
      : RuntimeFilter(cache), horizontalBlocks(horizontalBlocks), verticalBlocks(verticalBlocks),
        sharpColors(sharpColors) {
  }

 protected:
  DEFINE_RUNTIME_FILTER_TYPE

  std::string onBuildFragmentShader() const override;

  std::vector<tgfx::BindingEntry> uniformBlocks() const override;

  void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                        const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                        const tgfx::Point& offset) const override;

 private:
  float horizontalBlocks = 1;
  float verticalBlocks = 1;
  bool sharpColors = false;
};

}  // namespace pag
