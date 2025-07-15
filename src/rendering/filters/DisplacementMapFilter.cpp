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

#include "DisplacementMapFilter.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/filters/utils/FilterHelper.h"
#include "tgfx/core/Recorder.h"

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

std::shared_ptr<Graphic> GetDisplacementMapGraphic(const DisplacementMapEffect* effect,
                                                   Frame frame) {
  auto mapLayer = effect->displacementMapLayer;
  auto contentFrame = frame - mapLayer->startTime;
  auto layerCache = LayerCache::Get(mapLayer);
  auto content = layerCache->getContent(contentFrame);
  return static_cast<GraphicContent*>(content)->graphic;
}

std::shared_ptr<tgfx::Image> DisplacementMapFilter::Apply(
    std::shared_ptr<tgfx::Image> input, Effect* effect, Layer* layer, RenderCache* cache,
    const tgfx::Matrix& layerMatrix, Frame layerFrame, const tgfx::Rect& contentBounds,
    tgfx::Point* offset) {

  auto* pagEffect = reinterpret_cast<const DisplacementMapEffect*>(effect);
  auto bounds = layer->getBounds();
  auto size = tgfx::Size::Make(bounds.width(), bounds.height());
  auto mapLayer = static_cast<const DisplacementMapEffect*>(effect)->displacementMapLayer;
  bounds = mapLayer->getBounds();
  auto displacementSize = tgfx::Size::Make(bounds.width(), bounds.height());
  auto displacementMapBehavior = pagEffect->displacementMapBehavior->getValueAt(layerFrame);

  tgfx::Recorder recorder;
  auto tgfxCanvas = recorder.beginRecording();
  Canvas canvas(tgfxCanvas, cache);
  if (displacementMapBehavior == DisplacementMapBehavior::TileMap) {
    auto clipBounds = contentBounds;
    layerMatrix.mapRect(&clipBounds);
    tgfx::Path path;
    path.addRect(clipBounds);
    canvas.clipPath(path);
  }
  auto graphic = GetDisplacementMapGraphic(pagEffect, layerFrame);
  graphic->draw(&canvas);
  auto picture = recorder.finishRecordingAsPicture();
  auto referenceImage = tgfx::Image::MakeFrom(picture, static_cast<int>(displacementSize.width),
                                              static_cast<int>(displacementSize.height));

  auto filter = std::make_shared<DisplacementMapFilter>(
      pagEffect->useForHorizontalDisplacement->getValueAt(layerFrame),
      pagEffect->maxHorizontalDisplacement->getValueAt(layerFrame),
      pagEffect->useForVerticalDisplacement->getValueAt(layerFrame),
      pagEffect->maxVerticalDisplacement->getValueAt(layerFrame),
      pagEffect->displacementMapBehavior->getValueAt(layerFrame),
      pagEffect->edgeBehavior->getValueAt(layerFrame),
      pagEffect->expandOutput->getValueAt(layerFrame),
      ToAlpha(pagEffect->effectOpacity->getValueAt(layerFrame)), layerMatrix, size,
      displacementSize, contentBounds, referenceImage);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

std::string DisplacementMapFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::unique_ptr<Uniforms> DisplacementMapFilter::onPrepareProgram(tgfx::Context* context,
                                                                  unsigned program) const {
  return std::make_unique<DisplacementMapEffectUniforms>(context, program);
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

static SelectorCoeffs Coeffs(DisplacementMapSource sel) {
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
static float DisplacementWrapMode(DisplacementMapBehavior pos) {
  return pos == DisplacementMapBehavior::TileMap ? 0.0 : 1.0;
}

static float InputWrapMode(bool edgeBehavior) {
  return edgeBehavior ? 0.0 : 1.0;
}

static tgfx::Matrix DisplacementMatrix(DisplacementMapBehavior pos, const tgfx::Size& size,
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

static bool IsHSL(DisplacementMapSource sel) {
  return sel == DisplacementMapSource::Hue || sel == DisplacementMapSource::Saturation ||
         sel == DisplacementMapSource::Lightness;
}

static bool IsRGB(DisplacementMapSource sel) {
  return sel == DisplacementMapSource::Red || sel == DisplacementMapSource::Green ||
         sel == DisplacementMapSource::Blue;
}

void DisplacementMapFilter::onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                                           const std::vector<tgfx::BackendTexture>&) const {

  std::array<float, 4> flags = {0, 0, 0, 0};
  flags[0] = DisplacementWrapMode(displacementMapBehavior);
  auto gl = tgfx::GLFunctions::Get(context);
  auto uniform = static_cast<DisplacementMapEffectUniforms*>(program->uniforms.get());
  gl->uniform1i(uniform->mapTextureHandle, 1);
  flags[1] = InputWrapMode(edgeBehavior);

  float w = contentBounds.width();
  float h = contentBounds.height();
  gl->uniform2f(uniform->inputSizeHandle, w, h);

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
  gl->uniformMatrix3fv(uniform->inputMatrixHandle, 1, false, glMatrix.data());

  matrix.postTranslate(contentBounds.left, contentBounds.top);
  matrix.postConcat(layerMatrix);
  auto rect = tgfx::Rect::MakeSize(size);
  layerMatrix.mapRect(&rect);
  auto size = tgfx::Size::Make(rect.width(), rect.height());
  auto mapMatrix = DisplacementMatrix(displacementMapBehavior, size, displacementSize);
  mapMatrix.invert(&mapMatrix);
  matrix.postConcat(mapMatrix);
  matrix.postScale(1.f / displacementSize.width, 1.f / displacementSize.height);
  glMatrix = ToGLMatrix(matrix);
  gl->uniformMatrix3fv(uniform->mapMatrixHandle, 1, false, glMatrix.data());

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
      gl->uniformMatrix4fv(uniform->selectorMatrixHSLAHandle, 1, GL_FALSE,
                           selectorMatrixRGBA.data());
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
      gl->uniformMatrix4fv(uniform->selectorMatrixRGBAHandle, 1, GL_FALSE,
                           selectorMatrixRGBA.data());
      gl->uniformMatrix4fv(uniform->selectorMatrixHSLAHandle, 1, GL_FALSE,
                           selectorMatrixHSLA.data());
    }
  } else {
    flags[2] = 1.f;
    gl->uniformMatrix4fv(uniform->selectorMatrixRGBAHandle, 1, GL_FALSE, selectorMatrixRGBA.data());
  }

  const float selectorOffset[] = {
      (xc.dOffset - .5f) * sx,
      (yc.dOffset - .5f) * sy,
      xc.cOffset,
      yc.cOffset,
  };
  gl->uniform4fv(uniform->selectorOffsetHandle, 1, selectorOffset);
  gl->uniform4fv(uniform->flagsHandle, 1, flags.data());
  gl->uniform1f(uniform->effectOpacityHandle, effectOpacity);
}

tgfx::Rect DisplacementMapFilter::filterBounds(const tgfx::Rect& srcRect) const {
  tgfx::Rect rect = srcRect;
  if (expandOutput) {
    float expandX = std::abs(maxHorizontalDisplacement);
    float expandY = std::abs(maxVerticalDisplacement);
    float scaleX = srcRect.width() / contentBounds.width();
    float scaleY = srcRect.height() / contentBounds.height();
    rect.outset(expandX * scaleX, expandY * scaleY);
  }
  return rect;
}

}  // namespace pag
