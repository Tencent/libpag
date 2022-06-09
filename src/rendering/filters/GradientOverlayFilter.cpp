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

namespace pag {

static const char GRADIENT_OVERLAY_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    varying highp vec2 vertexColor;
    uniform sampler2D inputImageTexture;
    
    uniform float uBlendMode;
    uniform float uOpacity;
    uniform ColorStop uColors[16];
    uniform float uColorSize;
    uniform AlphaStop uAlpha[16];
    uniform float uAlphaSize;
    uniform float uAngle;
    uniform float uStyle;
    uniform float uAlignWithLayer;
    uniform float uScale;
    uniform float uOffset;
    
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
    
    vec4 getColorWithPosition(float position) {
      vec4 color;
      for (float i = 1; i < uColorSize; i += 1.0) {
        if (uColors[i - 1.0].position < position && uColors[i].position > position) {
          color.rgb = mix(uColors[i - 1.0].color, uColors[i].color, position);
          break;
        }
      }
    
      for (float i = 1; i < uAlphaSize; i += 1.0) {
        if (uAlpha[i - 1.0].position < position && uAlpha[i].position > position) {
          color.a = mix(uAlpha[i - 1.0].opacity, uAlpha[i].opacity, position);
          break;
        }
      }
    
      return color;
    }

    vec4 mappingToLinearStyle() {
      vec2 point = vertexColor * uSize;
      float length = mix(uSize.x * 0.5, uSize.y * 0.5, cos(uAngle));
      float distance = abs(cot(uAngle) * point.x + point.y) / sqrt(pow(cot(uAngle), 2.0), + 1.0);
      
    }
    
    
    void main() {
      if
    }
    )";

static const char GRADIENT_OVERLAY_GRADIENT_COLOR[] = R"(
    vec3 GradientColor(vec3 color1, vec3 color2, float weight, float location) {
        vec3 midColor = mix(color1, color2, 0.5);
        return location < weight ? mix(color1, midColor, location / weight)
                                 : mix(midColor, color2, (location - weight) / (1.0 - weight));
    }
    )";

static const char GRADIENT_OVERLAY_STYLE_LINEAR[] = R"(
    float StyleLinear(vec2 position, vec2 center, float angle, float ratio) {
        float k = tan(-angle);
        float b = center.y - center.x / ratio * k;
        float len = -(k * position.x / ratio - position.y + b) / sqrt(1.0 + pow(k, 2.0));
        return (len * 0.5 + 0.5);
    }
    )";

static const char GRADIENT_OVERLAY_STYLE_ANGLE[] = R"(
    float StyleAngleToLinear(vec2 position, vec2 center, float angle, float ratio) {
        float arctan = atan(position.y - center.y, (center.x - position.x) / ratio);
        return mod((arctan + pi + angle), pi * 2.0) / (pi * 2.0);
    }
    )";

static const char GRADIENT_OVERLAY_STYLE_RADIAL[] = R"(
    float StyleRadialToLinear(vec2 position, vec2 center, float ratio) {
        float len = length(position / vec2(ratio, 1.0) - center / vec2(ratio, 1.0));
        return len / 0.5;
    }
    )";

static const char GRADIENT_OVERLAY_STYLE_REFLECTED[] = R"(
    float StyleReflectedToLinear(vec2 position, vec2 center, float angle, float ratio) {
        float k = tan(-angle);
        float b = center.y - center.x / ratio * k;
        float len = abs(k * position.x / ratio - position.y + b) / sqrt(1.0 + pow(k, 2.0));
        return len / 0.5;
    }
    )";

GradientOverlayFilter::GradientOverlayFilter(GradientOverlayStyle* layerStyle) : layerStyle(layerStyle) {}

std::string GradientOverlayFilter::onBuildFragmentShader() { return GRADIENT_OVERLAY_FRAGMENT_SHADER; }

void GradientOverlayFilter::onPrepareProgram(tgfx::Context* context, unsigned program) {
//  amountHandle = gl->getUniformLocation(program, "uAmount");
//  centerHandle = gl->getUniformLocation(program, "uCenter");
}

void GradientOverlayFilter::onUpdateParams(tgfx::Context* context, const tgfx::Rect& contentBounds,
                                           const tgfx::Point& filterScale) {
  auto blendMode = layerStyle->blendMode->getValueAt(layerFrame);
  auto opacity = layerStyle->opacity->getValueAt(layerFrame);
  auto colors = layerStyle->colors->getValueAt(layerFrame);
  auto angle = layerStyle->angle->getValueAt(layerFrame);
  auto style = layerStyle->style->getValueAt(layerFrame);
  auto reverse = layerStyle->reverse->getValueAt(layerFrame);
  auto alignWithLayer = layerStyle->alignWithLayer->getValueAt(layerFrame);
  auto scale = layerStyle->scale->getValueAt(layerFrame);
  auto offset = layerStyle->offset->getValueAt(layerFrame);

//  gl->uniform1f(amountHandle, amount);
//  gl->uniform2f(centerHandle, (center.x - contentBounds.x()) / contentBounds.width(),
//                (center.y - contentBounds.y()) / contentBounds.height());
}
}  // namespace pag
