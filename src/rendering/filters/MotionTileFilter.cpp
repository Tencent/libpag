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

#include "MotionTileFilter.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char MOTIONTILE_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec2 aTextureCoord;
        uniform vec2 uTileCenter;
        varying vec2 vertexColor;
        varying vec2 tileCenter;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord.xy;
            tileCenter = uTileCenter.xy;
        }
    )";

static const char MOTIONTILE_FRAGMENT_SHADER[] = R"(
        #version 100
        precision mediump float;
        varying highp vec2 vertexColor;
        varying highp vec2 tileCenter;
        uniform sampler2D inputImageTexture;

        uniform float uTileWidth;
        uniform float uTileHeight;
        uniform float uOutputWidth;
        uniform float uOutputHeight;
        uniform bool uMirrorEdges;
        uniform float uPhase;
        uniform bool uIsHorizontalPhaseShift;

        float edge = 0.005;

        void main()
        {
            vec2 outputSize = vec2(uOutputWidth, uOutputHeight);
            vec2 scaleSize = vec2(uTileWidth, uTileHeight);
            vec2 originSize = 1.0 / outputSize;

            vec2 newCenter = vec2(0.5) - originSize / 2.0 + tileCenter / outputSize;
            vec2 newOrigin = newCenter - originSize * scaleSize / 2.0;

            vec2 target = mod(vertexColor - newOrigin, originSize * scaleSize) * outputSize / scaleSize;

            target = clamp(target, edge, 1.0 - edge);

            vec2 locationInfo = mod(floor((vertexColor - newOrigin) / (originSize * scaleSize)), 2.0);

            if(uMirrorEdges) {
                target = locationInfo * (1.0 - target) + abs(locationInfo - 1.0) * target;
            }

            if(uPhase > 0.0) {
                if(uIsHorizontalPhaseShift) {
                    target.x = locationInfo.y * fract(target.x + mod(uPhase, 360.0) / 360.0)+ abs(locationInfo.y - 1.0) * target.x;
                } else {
                    target.y = locationInfo.x * fract(target.y + mod(uPhase, 360.0) / 360.0)+ abs(locationInfo.x - 1.0) * target.y;
                }
            }

            gl_FragColor = texture2D(inputImageTexture, target);
        }
    )";

std::shared_ptr<tgfx::Image> MotionTileFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                     Effect* effect, Frame layerFrame,
                                                     const tgfx::Rect& contentBounds,
                                                     tgfx::Point* offset) {
  auto* pagEffect = reinterpret_cast<const MotionTileEffect*>(effect);
  auto tileCenter = pagEffect->tileCenter->getValueAt(layerFrame);
  tileCenter.x = (tileCenter.x - contentBounds.x()) / contentBounds.width();
  tileCenter.y = (tileCenter.y - contentBounds.y()) / contentBounds.height();
  auto tileWidth = pagEffect->tileWidth->getValueAt(layerFrame);
  auto tileHeight = pagEffect->tileHeight->getValueAt(layerFrame);
  auto outputWidth = pagEffect->outputWidth->getValueAt(layerFrame);
  auto outputHeight = pagEffect->outputHeight->getValueAt(layerFrame);
  auto mirrorEdges = pagEffect->mirrorEdges->getValueAt(layerFrame);
  auto phase = pagEffect->phase->getValueAt(layerFrame);
  auto horizontalPhaseShift = pagEffect->horizontalPhaseShift->getValueAt(layerFrame);
  auto filter = std::shared_ptr<RuntimeFilter>(
      new MotionTileFilter(tileCenter, tileWidth, tileHeight, outputWidth, outputHeight,
                           mirrorEdges, phase, horizontalPhaseShift));
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MotionTileFilter::onBuildVertexShader() const {
  return MOTIONTILE_VERTEX_SHADER;
}

std::string MotionTileFilter::onBuildFragmentShader() const {
  return MOTIONTILE_FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> MotionTileFilter::onPrepareProgram(tgfx::Context* context,
                                                             unsigned program) const {
  return std::make_unique<MotionTileUniforms>(context, program);
}

void MotionTileFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                      const std::vector<tgfx::BackendTexture>&) const {
  auto uniform = static_cast<MotionTileUniforms*>(program->uniforms.get());
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform2f(uniform->tileCenterHandle, tileCenter.x, tileCenter.y);
  gl->uniform1f(uniform->tileWidthHandle, tileWidth / 100.f);
  gl->uniform1f(uniform->tileHeightHandle, tileHeight / 100.f);
  gl->uniform1f(uniform->outputWidthHandle, outputWidth / 100.f);
  gl->uniform1f(uniform->outputHeightHandle, outputHeight / 100.f);
  gl->uniform1i(uniform->mirrorEdgesHandle, mirrorEdges);
  gl->uniform1f(uniform->phaseHandle, phase);
  gl->uniform1i(uniform->isHorizontalPhaseShiftHandle, horizontalPhaseShift);
}

tgfx::Rect MotionTileFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto width = srcRect.width() * outputWidth / 100.0f;
  auto height = srcRect.height() * outputHeight / 100.0f;
  auto x = srcRect.x() + (srcRect.width() - width) * 0.5f;
  auto y = srcRect.y() + (srcRect.height() - height) * 0.5f;
  return tgfx::Rect::MakeXYWH(x, y, width, height);
}

}  // namespace pag
