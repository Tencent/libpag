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

#include "DisplacementMapFilter.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        #version 100
        precision mediump float;
        varying highp vec2 vertexColor;
        uniform sampler2D inputImageTexture;
        uniform sampler2D mapTexture;
        uniform vec2 mapTextureSize;

        uniform vec2 uUseForDisplacement;
        uniform vec2 uMaxDisplacement;
        uniform int uDisplacementMapBehavior;
        uniform int uEdgeBehavior;
        uniform int uExpandOutput;

        const vec2 factor = vec2(0.004, 0.004);
        const vec4 grayColor = vec4(0.5, 0.5, 0.5, 0.5);

        float EdgeDetect(vec2 target) {
            vec2 edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);
            return edgeDetect.x * edgeDetect.y;
        }

        void main() {
            vec2 mapVertexColor;
            if (uDisplacementMapBehavior == 0) {
                mapVertexColor = vertexColor / mapTextureSize + vec2(0.5) - 0.5 / mapTextureSize;
            } else if (uDisplacementMapBehavior == 1) {
                mapVertexColor = vertexColor;
            } else if (uDisplacementMapBehavior == 2) {
                mapVertexColor = fract(vertexColor / mapTextureSize);
            }
            mapVertexColor = vec2(mapVertexColor.x, mapVertexColor.y);

            vec4 mapColor = mix(grayColor, texture2D(mapTexture, mapVertexColor), EdgeDetect(mapVertexColor));

            vec2 offset = vec2(0.0, 0.0);
            if (uUseForDisplacement.x == 0.0) {
                offset.x = 0.5 - mapColor.r;
            } else if (uUseForDisplacement.x == 1.0) {
                offset.x = 0.5 - mapColor.g;
            } else if (uUseForDisplacement.x == 2.0) {
                offset.x = 0.5 - mapColor.b;
            } else if (uUseForDisplacement.x == 3.0) {
                offset.x = 0.5 - mapColor.a;
            } else if (uUseForDisplacement.x == 4.0) {
                offset.x = 0.299 * mapColor.r + 0.587 * mapColor.g + 0.114 * mapColor.b - 0.5;
            }

            if (uUseForDisplacement.y == 0.0) {
                offset.y = mapColor.r - 0.5;
            } else if (uUseForDisplacement.y == 1.0) {
                offset.y = mapColor.g - 0.5;
            } else if (uUseForDisplacement.y == 2.0) {
                offset.y = mapColor.b - 0.5;
            } else if (uUseForDisplacement.y == 3.0) {
                offset.y = mapColor.a - 0.5;
            } else if (uUseForDisplacement.y == 4.0) {
                offset.y = 0.5 - 0.299 * mapColor.r - 0.587 * mapColor.g - 0.114 * mapColor.b;
            }

            vec2 target = vertexColor - offset * factor * uMaxDisplacement;
            gl_FragColor = texture2D(inputImageTexture, clamp(target, 0.0, 1.0));
        }
    )";

DisplacementMapFilter::DisplacementMapFilter(Effect* effect) : effect(effect) {
}

std::string DisplacementMapFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void DisplacementMapFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  useForDisplacementHandle = gl->getUniformLocation(program, "uUseForDisplacement");
  maxDisplacementHandle = gl->getUniformLocation(program, "uMaxDisplacement");
  displacementMapBehaviorHandle = gl->getUniformLocation(program, "uDisplacementMapBehavior");
  edgeBehaviorHandle = gl->getUniformLocation(program, "uEdgeBehavior");
  expandOutputHandle = gl->getUniformLocation(program, "uExpandOutput");
  mapTextureHandle = gl->getUniformLocation(program, "mapTexture");
  mapTextureSizeHandle = gl->getUniformLocation(program, "mapTextureSize");
}

void DisplacementMapFilter::updateMapTexture(RenderCache* cache, const Graphic* mapGraphic,
                                             const tgfx::Rect& bounds) {
  if (mapSurface == nullptr || mapBounds.width() != bounds.width() ||
      mapBounds.height() != bounds.height()) {
    mapSurface = tgfx::Surface::Make(cache->getContext(), static_cast<int>(bounds.width()),
                                     static_cast<int>(bounds.height()));
    mapBounds = bounds;
  }
  mapGraphic->draw(mapSurface->getCanvas(), cache);
}

void DisplacementMapFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                           const tgfx::Point&) {
  auto* pagEffect = reinterpret_cast<const DisplacementMapEffect*>(effect);
  ActiveGLTexture(context, 1, mapSurface->getTexture()->getSampler());
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform2f(useForDisplacementHandle,
                pagEffect->useForHorizontalDisplacement->getValueAt(layerFrame),
                pagEffect->useForVerticalDisplacement->getValueAt(layerFrame));
  gl->uniform2f(maxDisplacementHandle, pagEffect->maxHorizontalDisplacement->getValueAt(layerFrame),
                pagEffect->maxVerticalDisplacement->getValueAt(layerFrame));
  gl->uniform1i(displacementMapBehaviorHandle,
                pagEffect->displacementMapBehavior->getValueAt(layerFrame));
  gl->uniform1i(edgeBehaviorHandle, pagEffect->edgeBehavior->getValueAt(layerFrame));
  gl->uniform1i(expandOutputHandle, pagEffect->expandOutput->getValueAt(layerFrame));
  gl->uniform1i(mapTextureHandle, 1);
  gl->uniform2f(mapTextureSizeHandle, mapBounds.width() / contentBounds.width(),
                mapBounds.height() / contentBounds.height());
}
}  // namespace pag
