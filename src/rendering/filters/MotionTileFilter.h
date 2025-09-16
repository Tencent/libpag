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

class MotionTileUniforms : public Uniforms {
 public:
  explicit MotionTileUniforms(tgfx::Context* context, unsigned program)
      : Uniforms(context, program) {
    auto gl = tgfx::GLFunctions::Get(context);
    tileCenterHandle = gl->getUniformLocation(program, "uTileCenter");
    tileWidthHandle = gl->getUniformLocation(program, "uTileWidth");
    tileHeightHandle = gl->getUniformLocation(program, "uTileHeight");
    outputWidthHandle = gl->getUniformLocation(program, "uOutputWidth");
    outputHeightHandle = gl->getUniformLocation(program, "uOutputHeight");
    mirrorEdgesHandle = gl->getUniformLocation(program, "uMirrorEdges");
    phaseHandle = gl->getUniformLocation(program, "uPhase");
    isHorizontalPhaseShiftHandle = gl->getUniformLocation(program, "uIsHorizontalPhaseShift");
  }
  int tileCenterHandle = 0;
  int tileWidthHandle = 0;
  int tileHeightHandle = 0;
  int outputWidthHandle = 0;
  int outputHeightHandle = 0;
  int mirrorEdgesHandle = 0;
  int phaseHandle = 0;
  int isHorizontalPhaseShiftHandle = 0;
};

class MotionTileFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_PROGRAM_ID;

  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, const tgfx::Rect& contentBounds,
                                            tgfx::Point* offset);

  explicit MotionTileFilter(Point tileCenter, float tileWidth, float tileHeight, float outputWidth,
                            float outputHeight, bool mirrorEdges, float phase,
                            bool horizontalPhaseShift)
      : tileCenter(tileCenter), tileWidth(tileWidth), tileHeight(tileHeight),
        outputWidth(outputWidth), outputHeight(outputHeight), mirrorEdges(mirrorEdges),
        phase(phase), horizontalPhaseShift(horizontalPhaseShift) {
  }
  ~MotionTileFilter() override = default;

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>&) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  Point tileCenter = {};
  float tileWidth = 0.f;
  float tileHeight = 0.f;
  float outputWidth = 0.f;
  float outputHeight = 0.f;
  bool mirrorEdges = false;
  float phase = 0.f;
  bool horizontalPhaseShift = false;
};

}  // namespace pag
