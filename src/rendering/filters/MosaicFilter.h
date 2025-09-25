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

class MosaicUniforms : public Uniforms {
 public:
  MosaicUniforms(tgfx::Context* context, unsigned program) : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    horizontalBlocksHandle = gl->getUniformLocation(program, "mHorizontalBlocks");
    verticalBlocksHandle = gl->getUniformLocation(program, "mVerticalBlocks");
    sharpColorsHandle = gl->getUniformLocation(program, "mSharpColors");
  }

  int horizontalBlocksHandle = -1;
  int verticalBlocksHandle = -1;
  int sharpColorsHandle = -1;
};

class MosaicFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID

  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, tgfx::Point* offset);

  MosaicFilter(float horizontalBlocks, float verticalBlocks, bool sharpColors)
      : horizontalBlocks(horizontalBlocks), verticalBlocks(verticalBlocks),
        sharpColors(sharpColors) {
  }

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>&) const override;

 private:
  float horizontalBlocks = 1;
  float verticalBlocks = 1;
  bool sharpColors = false;
};

}  // namespace pag
