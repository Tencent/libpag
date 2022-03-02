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

#include "MotionTileFilter.h"
#include "rendering/filters/utils/FilterHelper.h"

namespace pag {
static const char MOTIONTILE_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec2 aTextureCoord;
        uniform mat3 uVertexMatrix;
        uniform mat3 uTextureMatrix;
        uniform vec2 uTileCenter;
        varying vec2 vertexColor;
        varying vec2 tileCenter;
        void main() {
            vec3 position = uVertexMatrix * vec3(aPosition, 1);
            gl_Position = vec4(position.xy, 0, 1);
            vec3 colorPosition = uTextureMatrix * vec3(aTextureCoord, 1);
            vertexColor = colorPosition.xy;
            vec3 tileCenterPosition = uTextureMatrix * vec3(uTileCenter, 1);
            tileCenter = tileCenterPosition.xy;
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

MotionTileFilter::MotionTileFilter(Effect* effect) : effect(effect) {
}

std::string MotionTileFilter::onBuildVertexShader() {
  return MOTIONTILE_VERTEX_SHADER;
}

std::string MotionTileFilter::onBuildFragmentShader() {
  return MOTIONTILE_FRAGMENT_SHADER;
}

void MotionTileFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
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

void MotionTileFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                      const tgfx::Point&) {
  auto* pagEffect = reinterpret_cast<const MotionTileEffect*>(effect);
  auto tileCenter = pagEffect->tileCenter->getValueAt(layerFrame);
  auto tileWidth = pagEffect->tileWidth->getValueAt(layerFrame);
  auto tileHeight = pagEffect->tileHeight->getValueAt(layerFrame);
  auto outputWidth = pagEffect->outputWidth->getValueAt(layerFrame);
  auto outputHeight = pagEffect->outputHeight->getValueAt(layerFrame);
  auto mirrorEdges = pagEffect->mirrorEdges->getValueAt(layerFrame);
  auto phase = pagEffect->phase->getValueAt(layerFrame);
  auto isHorizontalPhaseShift = pagEffect->horizontalPhaseShift->getValueAt(layerFrame);
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform2f(tileCenterHandle, (tileCenter.x - contentBounds.x()) / contentBounds.width(),
                1.0f - (tileCenter.y - contentBounds.y()) / contentBounds.height());
  gl->uniform1f(tileWidthHandle, tileWidth / 100.f);
  gl->uniform1f(tileHeightHandle, tileHeight / 100.f);
  gl->uniform1f(outputWidthHandle, outputWidth / 100.f);
  gl->uniform1f(outputHeightHandle, outputHeight / 100.f);
  gl->uniform1i(mirrorEdgesHandle, mirrorEdges);
  gl->uniform1f(phaseHandle, phase);
  gl->uniform1i(isHorizontalPhaseShiftHandle, isHorizontalPhaseShift);
}
}  // namespace pag
