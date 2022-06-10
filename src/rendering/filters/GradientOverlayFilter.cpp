///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "GradientOverlayFilter.h"
#include <format>

namespace pag {

static const char GRADIENT_OVERLAY_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    varying highp vec2 vertexColor;
    uniform sampler2D inputImageTexture;
    
    uniform float uOpacity;
    uniform ColorStop uColors[16];
    uniform int uColorSize;
    uniform AlphaStop uAlpha[16];
    uniform int uAlphaSize;
    uniform float uAngle;
    uniform int uStyle;
    uniform float uScale;
    uniform vec2 uOffset;
    
    uniform vec2 uSize;
    
    struct ColorStop {
        float position;
        float midpoint;
        vec3 color;
    };
    
    struct AlphaStop {
        float position;
        float midpoint;
        float opacity;
    };
    
    vec3 GradientColor(vec3 color1, vec3 color2, float weight, float location) {
        vec3 midColor = mix(color1, color2, 0.5);
        return location < weight ? mix(color1, midColor, location / weight)
                                 : mix(midColor, color2, (location - weight) / (1.0 - weight));
    }
    
    float GradientAlpha(float alpha1, float alpha2, float weight, float location) {
        vec3 midAlpha = mix(alpha1, alpha2, 0.5);
        return location < weight ? mix(alpha1, midAlpha, location / weight)
                                 : mix(midAlpha, alpha2, (location - weight) / (1.0 - weight));
    }
    
    vec4 ColorWithValue(float position) {
      vec4 color;
      for (int i = 1; i < uColorSize; i += 1) {
        if (uColors[i - 1].position < position && uColors[i].position > position) {
          float location = (position - uColors[i - 1].position) / (uColors[i].position - uColors[i - 1].position);
          color.rgb = GradientColor(uColors[i - 1].color, uColors[i].color, uColors[i - 1].midpoint, location);
          break;
        }
      }
      for (int i = 1; i < uAlphaSize; i += 1) {
        if (uAlpha[i - 1].position < position && uAlpha[i].position > position) {
          float location = (position - uAlpha[i - 1].position) / (uAlpha[i].position - uAlpha[i - 1].position);
          color.a = GradientAlpha(uAlpha[i - 1].opacity, uAlpha[i].opacity, uAlpha[i - 1].midpoint, location);
          break;
        }
      }
      return color;
    }
    
    float StyleLinear(vec2 position, vec2 center, float angle, float ratio) {
        float k = tan(angle - pi * 0.5);
        float b = center.y - center.x / ratio * k;
        float len = -(k * position.x / ratio - position.y + b) / sqrt(1.0 + pow(k, 2.0));
        return len * 0.5 + 0.5;
    }
    
    float StyleRadialToLinear(vec2 position, vec2 center, float ratio) {
        float len = length(position / vec2(ratio, 1.0) - center / vec2(ratio, 1.0));
        return len / 0.5;
    }
    
    float StyleAngleToLinear(vec2 position, vec2 center, float angle, float ratio) {
        float arctan = atan(position.y - center.y, (center.x - position.x) / ratio);
        return mod((arctan + pi + angle), pi * 2.0) / (pi * 2.0);
    }
    
    float StyleReflectedToLinear(vec2 position, vec2 center, float angle, float ratio) {
        float k = tan(angle - pi * 0.5);
        float b = center.y - center.x / ratio * k;
        float len = abs(k * position.x / ratio - position.y + b) / sqrt(1.0 + pow(k, 2.0));
        return len / 0.5;
    }
    
    void main() {
        float ratio = uSize.y / uSize.x;
        vec2 center = vec2(0.5) - uOffset / uSize;
        float value;
        if (uStyle == 1) {
            value = StyleRadialToLinear(vertexColor, center, uAngle, ratio);
        } else if (uStyle == 2) {
            value = StyleAngleToLinear(vertexColor, center, ratio);
        } else if (uStyle == 3) {
            value = StyleReflectedToLinear(vertexColor, center, uAngle, ratio);
        } else {
            value = StyleLinear(vertexColor, center, uAngle, ratio);
        }
        vec4 color = ColorWithValue(value / uScale);
        color.a *= uOpacity;
        gl_FragColor = color;
    }
    )";

GradientOverlayFilter::GradientOverlayFilter(GradientOverlayStyle* layerStyle) : layerStyle(layerStyle) {}

std::string GradientOverlayFilter::onBuildFragmentShader() { return GRADIENT_OVERLAY_FRAGMENT_SHADER; }

void GradientOverlayFilter::onPrepareProgram(tgfx::Context* context, unsigned program) {
  auto gl = tgfx::GLFunctions::Get(context);
  opacityHandle = gl->getUniformLocation(program, "uOpacity");
  colorSizeHandle = gl->getUniformLocation(program, "uColorSize");
  alphaSizeHandle = gl->getUniformLocation(program, "uAlphaSize");
  angleHandle = gl->getUniformLocation(program, "uAngle");
  styleHandle = gl->getUniformLocation(program, "uStyle");
  scaleHandle = gl->getUniformLocation(program, "uScale");
  offsetHandle = gl->getUniformLocation(program, "uOffset");
  sizeHandle = gl->getUniformLocation(program, "uSize");
  gradientOverlayProgram = program;
}

void GradientOverlayFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                           const tgfx::Point&) {
  auto opacity = layerStyle->opacity->getValueAt(layerFrame);
  auto colors = layerStyle->colors->getValueAt(layerFrame);
  auto colorSize = static_cast<int>(colors->colorStops.size());
  auto alphaSize = static_cast<int>(colors->alphaStops.size());
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto style = layerStyle->style->getValueAt(layerFrame);
//  auto reverse = layerStyle->reverse->getValueAt(layerFrame);
  auto scale = layerStyle->scale->getValueAt(layerFrame);
  auto offset = layerStyle->offset->getValueAt(layerFrame);
  
  auto gl = tgfx::GLFunctions::Get(context);
  gl->uniform1f(opacityHandle, opacity);
  gl->uniform1i(colorSizeHandle, colorSize);
  gl->uniform1i(alphaSizeHandle, alphaSize);
  gl->uniform1f(angleHandle, angle);
  gl->uniform1i(styleHandle, style);
  gl->uniform1f(scaleHandle, scale);
  gl->uniform2f(offsetHandle, offset.x, offset.y);
  gl->uniform2f(sizeHandle, contentBounds.width(), contentBounds.height());
  
  for (int i = 0; i < colorSize; i++) {
    auto positionStr = std::string("uColors[") + std::to_string(i) + std::string("].position");
    auto midpointStr = std::string("uColors[") + std::to_string(i) + std::string("].midpoint");
    auto colorStr = std::string("uColors[") + std::to_string(i) + std::string("].color");
    auto positionHandle = gl->getUniformLocation(gradientOverlayProgram, positionStr.c_str());
    auto midpointHandle = gl->getUniformLocation(gradientOverlayProgram, midpointStr.c_str());
    auto colorHandle = gl->getUniformLocation(gradientOverlayProgram, colorStr.c_str());
    auto colorStop = colors->colorStops[i];
    gl->uniform1f(positionHandle, colorStop.position);
    gl->uniform1f(midpointHandle, colorStop.midpoint);
    gl->uniform3f(colorHandle, colorStop.color.red / 255.0, colorStop.color.green / 255.0, colorStop.color.blue / 255.0);
  }
  
  for (int i = 0; i < alphaSize; i++) {
    auto positionStr = std::string("uAlpha[") + std::to_string(i) + std::string("].position");
    auto midpointStr = std::string("uAlpha[") + std::to_string(i) + std::string("].midpoint");
    auto opacityStr = std::string("uAlpha[") + std::to_string(i) + std::string("].opacity");
    auto positionHandle = gl->getUniformLocation(gradientOverlayProgram, positionStr.c_str());
    auto midpointHandle = gl->getUniformLocation(gradientOverlayProgram, midpointStr.c_str());
    auto opacityHandle = gl->getUniformLocation(gradientOverlayProgram, opacityStr.c_str());
    auto alphaStop = colors->alphaStops[i];
    gl->uniform1f(positionHandle, alphaStop.position);
    gl->uniform1f(midpointHandle, alphaStop.midpoint);
    gl->uniform1f(opacityHandle, alphaStop.opacity / 255.0);
  }
}
}  // namespace pag
