/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "BrightnessContrastFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        varying vec2 vertexColor;
        uniform sampler2D sTexture;
        uniform float mBrightness;
        uniform float mContrast;
    
        #define EPSILON 1e-10
        vec3 saturate(vec3 v) { return clamp(v, vec3(0.0), vec3(1.0)); }
    
        vec3 HUEtoRGB(float H) {
            float R = abs(H * 6.0 - 3.0) - 1.0;
            float G = 2.0 - abs(H * 6.0 - 2.0);
            float B = 2.0 - abs(H * 6.0 - 4.0);
            return saturate(vec3(R,G,B));
        }
    
        vec3 RGBtoHCV(vec3 RGB) {
            vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0/3.0) : vec4(RGB.gb, 0.0, -1.0/3.0);
            vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
            float C = Q.x - min(Q.w, Q.y);
            float H = abs((Q.w - Q.y) / (6.0 * C + EPSILON) + Q.z);
            return vec3(H, C, Q.x);
        }

        vec3 RGBtoHSV(vec3 RGB) {
            vec3 HCV = RGBtoHCV(RGB);
            float S = HCV.y / (HCV.z + EPSILON);
            return vec3(HCV.x, S, HCV.z);
        }

        vec3 HSVtoRGB(vec3 HSV) {
            vec3 RGB = HUEtoRGB(HSV.x);
            return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
        }

        void main() {
            vec4 color = texture2D(sTexture, vertexColor);
            vec3 rgbColor = color.rgb * mContrast + 0.5 - mContrast * 0.5;
            vec3 hsvColor = RGBtoHSV(rgbColor);
            hsvColor.z *= (mBrightness + 1.0);
            rgbColor = HSVtoRGB(hsvColor);
            rgbColor += (mBrightness / 2.0);
            gl_FragColor = vec4(rgbColor * color.a, color.a);
        }
    )";

BrightnessContrastFilter::BrightnessContrastFilter(pag::Effect* effect) : effect(effect) {
}

std::string BrightnessContrastFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void BrightnessContrastFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  brightnessBlocksHandle = gl->getUniformLocation(program, "mBrightness");
  contrastHandle = gl->getUniformLocation(program, "mContrast");
}

void BrightnessContrastFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect&,
                                              const tgfx::Point&) {
  auto* brightnessContrastEffect = reinterpret_cast<const BrightnessContrastEffect*>(effect);
  auto brightness = brightnessContrastEffect->brightness->getValueAt(layerFrame);
  auto contrast = brightnessContrastEffect->contrast->getValueAt(layerFrame);

  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform1f(brightnessBlocksHandle, brightness > 0 ? brightness / 250.f : brightness / 650.f);
  gl->uniform1f(contrastHandle, 1.0f + contrast / 300.f);
}
}  // namespace pag
