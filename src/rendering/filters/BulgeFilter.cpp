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

#include "BulgeFilter.h"
#include "gpu/opengl/GLUtil.h"

namespace pag {
static const char BULGE_VERTEX_SHADER[] = R"(
        #version 100
        attribute vec2 aPosition;
        attribute vec2 aTextureCoord;
        uniform mat3 uVertexMatrix;
        uniform mat3 uTextureMatrix;
        uniform vec2 uBulgeCenter;
        varying vec2 vertexColor;
        varying vec2 bulgeCenter;
        void main() {
            vec3 position = uVertexMatrix * vec3(aPosition, 1);
            gl_Position = vec4(position.xy, 0, 1);
            vec3 colorPosition = uTextureMatrix * vec3(aTextureCoord, 1);
            vertexColor = colorPosition.xy;
            vec3 bulgeCenterPosition = uTextureMatrix * vec3(uBulgeCenter, 1);
            bulgeCenter = bulgeCenterPosition.xy;
        }
    )";

static const char BULGE_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    varying highp vec2 vertexColor;
    varying highp vec2 bulgeCenter;
    uniform sampler2D inputImageTexture;

    uniform float uHorizontalRadius;
    uniform float uVerticalRadius;
    uniform float uBulgeHeight;
    uniform bool uPinning;

    float PI = 3.1415926535;
    float edge = 0.005;

    void main() {
        vec2 target = vertexColor;
        vec2 point = target - bulgeCenter;

        float distance = pow(point.x, 2.0) / pow(uHorizontalRadius, 2.0) + pow(point.y, 2.0) / pow(uVerticalRadius, 2.0);

        float len = length(point);
        float radius = atan(point.y, point.x);
        float weight = (1.0 - distance) * atan(len, sqrt(1.0 - sqrt(len))) / PI;
        vec2 newPoint = point - uBulgeHeight * weight * vec2(cos(radius), sin(radius));

        // TODO: 在swiftshader上，此处使用mix来减少分支语句会引起画面异常，待排查
        if (distance <= 1.0) {
            target = newPoint + bulgeCenter;
        }

        if(uPinning) {
            target.x = clamp(target.x, edge, 1.0 - edge);
            target.y = clamp(target.y, edge, 1.0 - edge);
        }

        vec2 edgeDetect = abs(step(vec2(1.0), target) - vec2(1.0)) * step(vec2(0.0), target);

        gl_FragColor = texture2D(inputImageTexture, target) * edgeDetect.x * edgeDetect.y;
    }
    )";

BulgeFilter::BulgeFilter(Effect* effect) : effect(effect) {
}

std::string BulgeFilter::onBuildVertexShader() {
  return BULGE_VERTEX_SHADER;
}

std::string BulgeFilter::onBuildFragmentShader() {
  return BULGE_FRAGMENT_SHADER;
}

void BulgeFilter::onPrepareProgram(const GLInterface* gl, unsigned int program) {
  horizontalRadiusHandle = gl->getUniformLocation(program, "uHorizontalRadius");
  verticalRadiusHandle = gl->getUniformLocation(program, "uVerticalRadius");
  bulgeCenterHandle = gl->getUniformLocation(program, "uBulgeCenter");
  bulgeHeightHandle = gl->getUniformLocation(program, "uBulgeHeight");
  pinningHandle = gl->getUniformLocation(program, "uPinning");
}

void BulgeFilter::onUpdateParams(const GLInterface* gl, const Rect& contentBounds, const Point&) {
  auto* bulgeEffect = reinterpret_cast<const BulgeEffect*>(effect);
  auto horizontalRadius = bulgeEffect->horizontalRadius->getValueAt(layerFrame);
  auto verticalRadius = bulgeEffect->verticalRadius->getValueAt(layerFrame);
  auto bulgeCenter = bulgeEffect->bulgeCenter->getValueAt(layerFrame);
  auto bulgeHeight = bulgeEffect->bulgeHeight->getValueAt(layerFrame);
  auto pinning = bulgeEffect->pinning->getValueAt(layerFrame);

  gl->uniform1f(horizontalRadiusHandle, horizontalRadius / contentBounds.width());
  gl->uniform1f(verticalRadiusHandle, verticalRadius / contentBounds.height());
  gl->uniform2f(bulgeCenterHandle, (bulgeCenter.x - contentBounds.x()) / contentBounds.width(),
                1.0f - (bulgeCenter.y - contentBounds.y()) / contentBounds.height());
  gl->uniform1f(bulgeHeightHandle, bulgeHeight);
  gl->uniform1i(pinningHandle, pinning);
}

std::vector<Point> BulgeFilter::computeVertices(const Rect& inputBounds, const Rect& outputBounds,
                                                const Point&) {
  return ComputeVerticesForMotionBlurAndBulge(inputBounds, outputBounds);
}
}  // namespace pag
