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
#include "tgfx/core/Surface.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
#version 100
precision mediump float;
varying highp vec2 vertexColor;
uniform sampler2D inputImageTexture;
uniform sampler2D mapTexture;

uniform vec4 uFlags;
uniform vec2 uInputSize;
uniform mat3 uInputMatrix;
uniform mat3 uMapMatrix;
uniform mat4 uSelectorMatrixRGBA;
uniform mat4 uSelectorMatrixHSLA;
uniform vec4 uSelectorOffset;
uniform float uEffectOpacity;

const float kEps = 0.0001;

float EdgeDetect(vec2 target) {
    vec2 edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);
    return edgeDetect.x * edgeDetect.y;
}

// https://github.com/google/skia/blob/62f22c9c7d670b89371279c589304519b336d661/src/sksl/sksl_shared.sksl#L243
vec4 rgb_to_hsl(vec3 c, float a) {
    vec4 p = (c.g < c.b) ? vec4(c.bg, -1.0,  2.0/3.0)
                          : vec4(c.gb,  0.0, -1.0/3.0);
    vec4 q = (c.r < p.x) ? vec4(p.x, c.r, p.yw)
                          : vec4(c.r, p.x, p.yz);

    // q.x  -> max channel value
    // q.yz -> 2nd/3rd channel values (unsorted)
    // q.w  -> bias value dependent on max channel selection

    float pmV = q.x;
    float pmC = pmV - min(q.y, q.z);
    float pmL = pmV - pmC * 0.5;
    float   H = abs(q.w + (q.y - q.z) / (pmC * 6.0 + kEps));
    float   S = pmC / (a + kEps - abs(pmL * 2.0 - a));
    float   L = pmL / (a + kEps);

    return vec4(H, S, L, a);
}

vec4 input_color(vec2 uv) {
    return mix(texture2D(inputImageTexture, fract(uv)),
               mix(vec4(0.0), texture2D(inputImageTexture, uv), EdgeDetect(uv)),
               step(0.5, uFlags.y));
}

void main() {
    vec2 mapUV = (uMapMatrix * vec3(vertexColor, 1.0)).xy;
    vec4 d = mix(texture2D(mapTexture, fract(mapUV)),
                 mix(vec4(0.0), texture2D(mapTexture, mapUV), EdgeDetect(mapUV)),
                 step(0.5, uFlags.x));
    d = mix(vec4(0.0), uSelectorMatrixRGBA * vec4(d.rgb / max(d.a, kEps), d.a), step(0.5, uFlags.z))
      + mix(vec4(0.0), uSelectorMatrixHSLA * rgb_to_hsl(d.rgb, d.a), step(0.5, uFlags.w))
      + uSelectorOffset;
    vec2 uv = (uInputMatrix * vec3(vertexColor, 1.0)).xy / uInputSize;
    gl_FragColor = mix(input_color(uv), input_color(uv + d.xy * d.zw / uInputSize), uEffectOpacity);
}
)";

DisplacementMapFilter::DisplacementMapFilter(Effect* effect) : effect(effect) {
}

std::string DisplacementMapFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void DisplacementMapFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  mapTextureHandle = gl->getUniformLocation(program, "mapTexture");
  flagsHandle = gl->getUniformLocation(program, "uFlags");
  inputMatrixHandle = gl->getUniformLocation(program, "uInputMatrix");
  mapMatrixHandle = gl->getUniformLocation(program, "uMapMatrix");
  inputSizeHandle = gl->getUniformLocation(program, "uInputSize");
  selectorMatrixRGBAHandle = gl->getUniformLocation(program, "uSelectorMatrixRGBA");
  selectorMatrixHSLAHandle = gl->getUniformLocation(program, "uSelectorMatrixHSLA");
  selectorOffsetHandle = gl->getUniformLocation(program, "uSelectorOffset");
  effectOpacityHandle = gl->getUniformLocation(program, "uEffectOpacity");
}

void DisplacementMapFilter::update(Frame layerFrame, const tgfx::Rect& contentBounds,
                                   const tgfx::Rect& transformedBounds,
                                   const tgfx::Point& filterScale) {
  LayerFilter::update(layerFrame, contentBounds, transformedBounds, filterScale);
  auto* pagEffect = reinterpret_cast<const DisplacementMapEffect*>(effect);
  useForHorizontalDisplacement = pagEffect->useForHorizontalDisplacement->getValueAt(layerFrame);
  maxHorizontalDisplacement = pagEffect->maxHorizontalDisplacement->getValueAt(layerFrame);
  useForVerticalDisplacement = pagEffect->useForVerticalDisplacement->getValueAt(layerFrame);
  maxVerticalDisplacement = pagEffect->maxVerticalDisplacement->getValueAt(layerFrame);
  displacementMapBehavior = pagEffect->displacementMapBehavior->getValueAt(layerFrame);
  edgeBehavior = pagEffect->edgeBehavior->getValueAt(layerFrame);
  expandOutput = pagEffect->expandOutput->getValueAt(layerFrame);
  effectOpacity = ToAlpha(pagEffect->effectOpacity->getValueAt(layerFrame));
}

void DisplacementMapFilter::updateMapTexture(RenderCache* cache, const Graphic* mapGraphic,
                                             const tgfx::Size& size,
                                             const tgfx::Size& displacementSize,
                                             const tgfx::Matrix& layerMatrix,
                                             const tgfx::Rect& contentBounds) {
  _size = size;
  _layerMatrix = layerMatrix;
  if (mapSurface == nullptr || _displacementSize != displacementSize) {
    _displacementSize = displacementSize;
    mapSurface = tgfx::Surface::Make(cache->getContext(), static_cast<int>(displacementSize.width),
                                     static_cast<int>(displacementSize.height));
  }
  Canvas canvas(mapSurface.get(), cache);
  canvas.clear();
  if (displacementMapBehavior == DisplacementMapBehavior::TileMap) {
    canvas.save();
    auto bounds = contentBounds;
    layerMatrix.mapRect(&bounds);
    tgfx::Path path;
    path.addRect(bounds);
    canvas.clipPath(path);
  }
  mapGraphic->draw(&canvas);
  if (displacementMapBehavior == DisplacementMapBehavior::TileMap) {
    canvas.restore();
  }
  mapSurface->flush();
}

struct SelectorCoeffs {
  // displacement contribution
  float dr;
  float dg;
  float db;
  float da;
  float dOffset;
  // coverage as a function of alpha
  float cScale;
  float cOffset;
};

static SelectorCoeffs Coeffs(Enum sel) {
  // D = displacement input
  // C = displacement coverage
  static constexpr SelectorCoeffs gCoeffs[] = {
      {1, 0, 0, 0, 0, 1, 0},                 // kR: D = r, C = a
      {0, 1, 0, 0, 0, 1, 0},                 // kG: D = g, C = a
      {0, 0, 1, 0, 0, 1, 0},                 // kB: D = b, C = a
      {0, 0, 0, 1, 0, 0, 1},                 // kA: D = a, C = 1.0
      {.2126f, .7152f, .0722f, 0, 0, 1, 0},  // kLuminance: D = lum(rgb), C = a
      {1, 0, 0, 0, 0, 0, 1},                 // kH: D = h, C = 1.0   (HSLA)
      {0, 1, 0, 0, 0, 0, 1},                 // kL: D = l, C = 1.0   (HSLA)
      {0, 0, 1, 0, 0, 0, 1},                 // kS: D = s, C = 1.0   (HSLA)
      {0, 0, 0, 0, 1, 0, 1},                 // kFull: D = 1.0, C = 1.0
      {0, 0, 0, 0, .5f, 0, 1},               // kHalf: D = 0.5, C = 1.0
      {0, 0, 0, 0, 0, 0, 1},                 // kOff:  D = 0.0, C = 1.0
  };
  const auto i = static_cast<size_t>(sel);
  return gCoeffs[i];
}

// 0.0: REPEAT
// 1.0: CLAMP_TO_BORDER
static float DisplacementWrapMode(Enum pos) {
  return pos == DisplacementMapBehavior::TileMap ? 0.0 : 1.0;
}

static float InputWrapMode(bool edgeBehavior) {
  return edgeBehavior ? 0.0 : 1.0;
}

static tgfx::Matrix DisplacementMatrix(Enum pos, const tgfx::Size& size,
                                       const tgfx::Size& displacementSize) {
  switch (pos) {
    case DisplacementMapBehavior::CenterMap:
      return tgfx::Matrix::MakeTrans((size.width - displacementSize.width) / 2,
                                     (size.height - displacementSize.height) / 2);
    case DisplacementMapBehavior::StretchMapToFit:
      return tgfx::Matrix::MakeScale(size.width / displacementSize.width,
                                     size.height / displacementSize.height);
    case DisplacementMapBehavior::TileMap:
    default:
      return tgfx::Matrix::I();
  }
}

static bool IsHSL(Enum sel) {
  return sel == DisplacementMapSource::Hue || sel == DisplacementMapSource::Saturation ||
         sel == DisplacementMapSource::Lightness;
}

static bool IsRGB(Enum sel) {
  return sel == DisplacementMapSource::Red || sel == DisplacementMapSource::Green ||
         sel == DisplacementMapSource::Blue;
}

void DisplacementMapFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                           const tgfx::Point&) {
  std::array<float, 4> flags = {0, 0, 0, 0};
  flags[0] = DisplacementWrapMode(displacementMapBehavior);
  auto texture = mapSurface->getBackendTexture();
  tgfx::GLTextureInfo glSampler = {};
  texture.getGLTextureInfo(&glSampler);
  ActiveGLTexture(context, 1, &glSampler);
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform1i(mapTextureHandle, 1);
  flags[1] = InputWrapMode(edgeBehavior);

  float w = contentBounds.width();
  float h = contentBounds.height();
  gl->uniform2f(inputSizeHandle, w, h);

  auto matrix = tgfx::Matrix::I();
  matrix.postScale(w, h);
  if (expandOutput) {
    float expandX = std::abs(maxHorizontalDisplacement);
    float expandY = std::abs(maxVerticalDisplacement);
    float newW = contentBounds.width() + expandX * 2;
    float newH = contentBounds.height() + expandY * 2;
    matrix.postScale(newW / w, newH / h);
    matrix.postTranslate(-expandX, -expandY);
  }
  auto glMatrix = ToGLMatrix(matrix);
  gl->uniformMatrix3fv(inputMatrixHandle, 1, false, glMatrix.data());

  matrix.postTranslate(contentBounds.left, contentBounds.top);
  matrix.postConcat(_layerMatrix);
  auto rect = tgfx::Rect::MakeSize(_size);
  _layerMatrix.mapRect(&rect);
  auto size = tgfx::Size::Make(rect.width(), rect.height());
  auto mapMatrix = DisplacementMatrix(displacementMapBehavior, size, _displacementSize);
  mapMatrix.invert(&mapMatrix);
  matrix.postConcat(mapMatrix);
  matrix.postScale(1.f / _displacementSize.width, 1.f / _displacementSize.height);
  glMatrix = ToGLMatrix(matrix);
  gl->uniformMatrix3fv(mapMatrixHandle, 1, false, glMatrix.data());

  auto xc = Coeffs(useForHorizontalDisplacement);
  auto yc = Coeffs(useForVerticalDisplacement);
  float sx = maxHorizontalDisplacement * 2.f;
  float sy = maxVerticalDisplacement * 2.f;
  // clang-format off
  std::array<float, 16> selectorMatrixRGBA = {
      xc.dr * sx, yc.dr * sy,         0,         0,
      xc.dg * sx, yc.dg * sy,         0,         0,
      xc.db * sx, yc.db * sy,         0,         0,
      xc.da * sx, yc.da * sy, xc.cScale, yc.cScale,

      //  │           │               │          └────  A -> vertical modulation
      //  │           │               └───────────────  B -> horizontal modulation
      //  │           └───────────────────────────────  G -> vertical displacement
      //  └───────────────────────────────────────────  R -> horizontal displacement
  };
  std::array<float, 16> selectorMatrixHSLA = {
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
  };
  // clang-format on
  bool isHorizontalHSL = IsHSL(useForHorizontalDisplacement);
  bool isVerticalHSL = IsHSL(useForVerticalDisplacement);
  if (isHorizontalHSL || isVerticalHSL) {
    if ((isHorizontalHSL && isVerticalHSL) ||
        (!IsRGB(useForHorizontalDisplacement) && !IsRGB(useForVerticalDisplacement))) {
      gl->uniformMatrix4fv(selectorMatrixHSLAHandle, 1, GL_FALSE, selectorMatrixRGBA.data());
      flags[3] = 1.f;
    } else {
      static const int horizontalIndices[] = {0, 4, 8, 12, 14};
      static const int verticalIndices[] = {1, 5, 9, 13, 15};
      for (int index : (isHorizontalHSL ? horizontalIndices : verticalIndices)) {
        selectorMatrixHSLA[index] = selectorMatrixRGBA[index];
        selectorMatrixRGBA[index] = 0;
      }
      flags[2] = 1.f;
      flags[3] = 1.f;
      gl->uniformMatrix4fv(selectorMatrixRGBAHandle, 1, GL_FALSE, selectorMatrixRGBA.data());
      gl->uniformMatrix4fv(selectorMatrixHSLAHandle, 1, GL_FALSE, selectorMatrixHSLA.data());
    }
  } else {
    flags[2] = 1.f;
    gl->uniformMatrix4fv(selectorMatrixRGBAHandle, 1, GL_FALSE, selectorMatrixRGBA.data());
  }

  const float selectorOffset[] = {
      (xc.dOffset - .5f) * sx,
      (yc.dOffset - .5f) * sy,
      xc.cOffset,
      yc.cOffset,
  };
  gl->uniform4fv(selectorOffsetHandle, 1, selectorOffset);
  gl->uniform4fv(flagsHandle, 1, flags.data());
  gl->uniform1f(effectOpacityHandle, effectOpacity);
}
}  // namespace pag
