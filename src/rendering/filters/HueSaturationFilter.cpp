/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
#include "tgfx/core/ImageFilter.h"

namespace pag {
static const char FRAGMENT_SHADER[] = R"(
        precision highp float;
        in vec2 vertexColor;
        uniform sampler2D sTexture;
        layout(std140) uniform Args {
            float mHue;
            float mSaturation;
            float mLightness;
            float mColorize;
            float mColorizeHue;
            float mColorizeSaturation;
            float mColorizeLightness;
        };
        out vec4 tgfx_FragColor;

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
            vec4 color = texture(sTexture, vertexColor);
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
            tgfx_FragColor = vec4(rgbColor * color.a, color.a);
        }
    )";

std::shared_ptr<tgfx::Image> HueSaturationFilter::Apply(std::shared_ptr<tgfx::Image> input,
                                                        RenderCache* cache, Effect* effect,
                                                        Frame layerFrame, tgfx::Point* offset) {
  auto* hueSaturationEffect = reinterpret_cast<const HueSaturationEffect*>(effect);
  auto channelControl = static_cast<int>(hueSaturationEffect->channelControl);
  auto hue = hueSaturationEffect->hue[channelControl];
  auto saturation = hueSaturationEffect->saturation[channelControl];
  auto lightness = hueSaturationEffect->lightness[channelControl];
  auto colorize = hueSaturationEffect->colorize;
  auto colorizeHue = hueSaturationEffect->colorizeHue->getValueAt(layerFrame);
  auto colorizeSaturation = hueSaturationEffect->colorizeSaturation->getValueAt(layerFrame);
  auto colorizeLightness = hueSaturationEffect->colorizeLightness->getValueAt(layerFrame);
  auto filter = std::make_shared<HueSaturationFilter>(cache, hue, saturation, lightness, colorize,
                                                      colorizeHue, colorizeSaturation,
                                                      colorizeLightness);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(filter), offset);
}

HueSaturationFilter::HueSaturationFilter(RenderCache* cache, float hue, float saturation,
                                         float lightness, float colorize, float colorizeHue,
                                         float colorizeSaturation, float colorizeLightness)
    : RuntimeFilter(cache), hue(hue), saturation(saturation), lightness(lightness),
      colorize(colorize), colorizeHue(colorizeHue), colorizeSaturation(colorizeSaturation),
      colorizeLightness(colorizeLightness) {
}

std::string HueSaturationFilter::onBuildFragmentShader() const {
  return FRAGMENT_SHADER;
}

std::vector<tgfx::BindingEntry> HueSaturationFilter::uniformBlocks() const {
  return {{"Args", 0}};
}

void HueSaturationFilter::onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                                           const std::vector<std::shared_ptr<tgfx::Texture>>&,
                                           const tgfx::Point&) const {
  struct Uniforms {
    float hue = 0.0f;
    float saturation = 0.0f;
    float lightness = 0.0f;
    float colorize = 0.0f;
    float colorizeHue = 0.0f;
    float colorizeSaturation = 0.0f;
    float colorizeLightness = 0.0f;
  };

  Uniforms uniforms = {};
  uniforms.hue = hue / 360.f;
  uniforms.saturation = saturation / 100.f;
  uniforms.lightness = lightness / 100.f;
  uniforms.colorize = colorize;
  uniforms.colorizeHue = colorizeHue / 360.f;
  uniforms.colorizeSaturation = colorizeSaturation / 100.f;
  uniforms.colorizeLightness = colorizeLightness / 100.f;

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
}  // namespace pag
