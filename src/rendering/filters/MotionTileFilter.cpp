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
        in vec2 aPosition;
        in vec2 aTextureCoord;
        out vec2 vertexColor;
        void main() {
            gl_Position = vec4(aPosition.xy, 0, 1);
            vertexColor = aTextureCoord.xy;
        }
    )";

static const char MOTIONTILE_FRAGMENT_SHADER[] = R"(
        precision mediump float;
        in highp vec2 vertexColor;
        uniform sampler2D sTexture;

        layout(std140) uniform Args {
            vec2 uTileCenter;
            float uTileWidth;
            float uTileHeight;
            float uOutputWidth;
            float uOutputHeight;
            int uMirrorEdges;
            float uPhase;
            int uIsHorizontalPhaseShift;
        };

        out vec4 tgfx_FragColor;

        float edge = 0.005;

        void main()
        {
            vec2 outputSize = vec2(uOutputWidth, uOutputHeight);
            vec2 scaleSize = vec2(uTileWidth, uTileHeight);
            vec2 originSize = 1.0 / outputSize;

            vec2 newCenter = vec2(0.5) - originSize / 2.0 + uTileCenter / outputSize;
            vec2 newOrigin = newCenter - originSize * scaleSize / 2.0;

            vec2 target = mod(vertexColor - newOrigin, originSize * scaleSize) * outputSize / scaleSize;

            target = clamp(target, edge, 1.0 - edge);

            vec2 locationInfo = mod(floor((vertexColor - newOrigin) / (originSize * scaleSize)), 2.0);

            if(uMirrorEdges != 0) {
                target = locationInfo * (1.0 - target) + abs(locationInfo - 1.0) * target;
            }

            if(uPhase > 0.0) {
                if(uIsHorizontalPhaseShift != 0) {
                    target.x = locationInfo.y * fract(target.x + mod(uPhase, 360.0) / 360.0)+ abs(locationInfo.y - 1.0) * target.x;
                } else {
                    target.y = locationInfo.x * fract(target.y + mod(uPhase, 360.0) / 360.0)+ abs(locationInfo.x - 1.0) * target.y;
                }
            }

            tgfx_FragColor = texture(sTexture, target);
        }
    )";

std::shared_ptr<tgfx::Image> MotionTileFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                     RenderCache* cache, Effect* effect,
                                                     Frame layerFrame,
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
      new MotionTileFilter(cache, tileCenter, tileWidth, tileHeight, outputWidth, outputHeight,
                           mirrorEdges, phase, horizontalPhaseShift));
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string MotionTileFilter::onBuildVertexShader() const {
  return MOTIONTILE_VERTEX_SHADER;
}

std::string MotionTileFilter::onBuildFragmentShader() const {
  return MOTIONTILE_FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> MotionTileFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void MotionTileFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                        const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                        const tgfx::Point&) const {
  struct Uniforms {
    float tileCenterX = 0.0f;
    float tileCenterY = 0.0f;
    float tileWidth = 0.0f;
    float tileHeight = 0.0f;
    float outputWidth = 0.0f;
    float outputHeight = 0.0f;
    int mirrorEdges = 0;
    float phase = 0.0f;
    int isHorizontalPhaseShift = 0;
  };

  Uniforms uniforms = {};
  uniforms.tileCenterX = tileCenter.x;
  uniforms.tileCenterY = tileCenter.y;
  uniforms.tileWidth = tileWidth / 100.f;
  uniforms.tileHeight = tileHeight / 100.f;
  uniforms.outputWidth = outputWidth / 100.f;
  uniforms.outputHeight = outputHeight / 100.f;
  uniforms.mirrorEdges = mirrorEdges ? 1 : 0;
  uniforms.phase = phase;
  uniforms.isHorizontalPhaseShift = horizontalPhaseShift ? 1 : 0;

  auto uniformBuffer = gpu->createBuffer(sizeof(Uniforms), tgfx::GPUBufferUsage::UNIFORM);
  if (uniformBuffer != nullptr) {
    auto* data = uniformBuffer->map();
    if (data != nullptr) {
      memcpy(data, &uniforms, sizeof(Uniforms));
      uniformBuffer->unmap();
      renderPass->setUniformBuffer(0, uniformBuffer, 0, sizeof(Uniforms));
    }
  }
}

tgfx::Rect MotionTileFilter::filterBounds(const tgfx::Rect& srcRect) const {
  auto width = srcRect.width() * outputWidth / 100.0f;
  auto height = srcRect.height() * outputHeight / 100.0f;
  auto x = srcRect.x() + (srcRect.width() - width) * 0.5f;
  auto y = srcRect.y() + (srcRect.height() - height) * 0.5f;
  return tgfx::Rect::MakeXYWH(x, y, width, height);
}

}  // namespace pag
