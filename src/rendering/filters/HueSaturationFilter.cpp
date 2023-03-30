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

#include "HueSaturationFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        #version 100
        precision highp float;
        varying vec2 vertexColor;
        uniform sampler2D sTexture;
        uniform float mHue;
        uniform float mSaturation;
        uniform float mLightness;
        uniform float mColorize;
        uniform float mColorizeHue;
        uniform float mColorizeSaturation;
        uniform float mColorizeLightness;
    
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
            vec3 rgbColor = color.rgb;
            vec3 hsvColor = RGBtoHSV(rgbColor);
            if (mColorize == 0.0) {
                hsvColor.x = fract(hsvColor.x + mHue);
                hsvColor.y *= (mSaturation + 1.0);
                rgbColor = HSVtoRGB(hsvColor);
                rgbColor += mLightness;
            } else {
                hsvColor.x = fract(mColorizeHue);
                hsvColor.y = mColorizeSaturation;
                rgbColor = HSVtoRGB(hsvColor);
                rgbColor += mColorizeLightness;
            }
            gl_FragColor = vec4(rgbColor * color.a, color.a);
        }
    )";

HueSaturationFilter::HueSaturationFilter(pag::Effect* effect) : effect(effect) {
}

std::string HueSaturationFilter::onBuildFragmentShader() {
  return FRAGMENT_SHADER;
}

void HueSaturationFilter::onPrepareProgram(tgfx::Context* context, unsigned int program) {
  auto gl = tgfx::GLFunctions::Get(context);
  hueHandle = gl->getUniformLocation(program, "mHue");
  saturationHandle = gl->getUniformLocation(program, "mSaturation");
  lightnessHandle = gl->getUniformLocation(program, "mLightness");
  colorizeHandle = gl->getUniformLocation(program, "mColorize");
  colorizeHueHandle = gl->getUniformLocation(program, "mColorizeHue");
  colorizeSaturationHandle = gl->getUniformLocation(program, "mColorizeSaturation");
  colorizeLightnessHandle = gl->getUniformLocation(program, "mColorizeLightness");
}

void HueSaturationFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect&,
                                         const tgfx::Point&) {
  auto* hueSaturationEffect = reinterpret_cast<const HueSaturationEffect*>(effect);
  auto channelControl = hueSaturationEffect->channelControl;
  auto hue = hueSaturationEffect->hue[channelControl];
  auto saturation = hueSaturationEffect->saturation[channelControl];
  auto lightness = hueSaturationEffect->lightness[channelControl];
  auto colorize = hueSaturationEffect->colorize;
  auto colorizeHue = hueSaturationEffect->colorizeHue->getValueAt(layerFrame);
  auto colorizeSaturation = hueSaturationEffect->colorizeSaturation->getValueAt(layerFrame);
  auto colorizeLightness = hueSaturationEffect->colorizeLightness->getValueAt(layerFrame);

  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform1f(hueHandle, hue / 360.f);
  gl->uniform1f(saturationHandle, saturation / 100.f);
  gl->uniform1f(lightnessHandle, lightness / 100.f);
  gl->uniform1f(colorizeHandle, colorize);
  gl->uniform1f(colorizeHueHandle, colorizeHue / 360.f);
  gl->uniform1f(colorizeSaturationHandle, colorizeSaturation / 100.f);
  gl->uniform1f(colorizeLightnessHandle, colorizeLightness / 100.f);
}
}  // namespace pag
