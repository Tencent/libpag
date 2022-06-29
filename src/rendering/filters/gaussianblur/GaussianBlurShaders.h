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

#pragma once

namespace pag {
static const char BLUR_DOWN_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    
    varying vec2 vertexColor;
    uniform sampler2D uTextureInput;
    
    uniform vec2 uStep;
    uniform vec2 uOffset;
    
    float edge = 0.995;
    
    vec2 clampEdge(vec2 point) {
        return clamp(point, vec2(0.0), vec2(edge));
    }

    void main()
    {
        vec4 sum = texture2D(uTextureInput, clampEdge(vertexColor)) * 4.0;
        sum += texture2D(uTextureInput, clampEdge(vertexColor - uStep.xy * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + uStep.xy * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(uStep.x, -uStep.y) * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor - vec2(uStep.x, -uStep.y) * uOffset));

        vec4 color = sum / 8.0;
    
        gl_FragColor = color;
    }
    )";

static const char BLUR_UP_FRAGMENT_SHADER[] = R"(
    #version 100
    precision highp float;
    
    varying vec2 vertexColor;
    uniform sampler2D uTextureInput;
        
    uniform vec2 uStep;
    uniform vec2 uOffset;
    
    float edge = 0.995;
    
    vec2 clampEdge(vec2 point) {
        return clamp(point, vec2(0.0), vec2(edge));
    }
    
    void main()
    {
        vec4 sum = texture2D(uTextureInput, clampEdge(vertexColor + vec2(-uStep.x * 2.0, 0.0) * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(-uStep.x, uStep.y) * uOffset)) * 2.0;
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(0.0, uStep.y * 2.0) * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(uStep.x, uStep.y) * uOffset)) * 2.0;
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(uStep.x * 2.0, 0.0) * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(uStep.x, -uStep.y) * uOffset)) * 2.0;
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(0.0, -uStep.y * 2.0) * uOffset));
        sum += texture2D(uTextureInput, clampEdge(vertexColor + vec2(-uStep.x, -uStep.y) * uOffset)) * 2.0;

        vec4 color = sum / 12.0;
    
        gl_FragColor = color;
    }
    )";

static const char BLUR_UP_FRAGMENT_SHADER_NO_REPEAT_EDGE[] = R"(
    #version 100
    precision highp float;
    
    varying vec2 vertexColor;
    uniform sampler2D uTextureInput;
        
    uniform vec2 uStep;
    uniform vec2 uOffset;
    
    float check(vec2 point) {
        vec2 result = step(point, vec2(1.0)) * step(vec2(0.0), point);
        return step(0.5, result.x * result.y);
    }
    
    void main()
    {
        vec2 point = vertexColor + vec2(-uStep.x * 2.0, 0.0) * uOffset;
        vec4 sum = texture2D(uTextureInput, point) * check(point);
        point = vertexColor + vec2(-uStep.x, uStep.y) * uOffset;
        sum += texture2D(uTextureInput, point) * 2.0 * check(point);
        point = vertexColor + vec2(0.0, uStep.y * 2.0) * uOffset;
        sum += texture2D(uTextureInput, point) * check(point);
        point = vertexColor + vec2(uStep.x, uStep.y) * uOffset;
        sum += texture2D(uTextureInput, point) * 2.0 * check(point);
        point = vertexColor + vec2(uStep.x * 2.0, 0.0) * uOffset;
        sum += texture2D(uTextureInput, point) * check(point);
        point = vertexColor + vec2(uStep.x, -uStep.y) * uOffset;
        sum += texture2D(uTextureInput, point) * 2.0 * check(point);
        point = vertexColor + vec2(0.0, -uStep.y * 2.0) * uOffset;
        sum += texture2D(uTextureInput, point) * check(point);
        point = vertexColor + vec2(-uStep.x, -uStep.y) * uOffset;
        sum += texture2D(uTextureInput, point) * 2.0 * check(point);

        vec4 color = sum / 12.0;
    
        gl_FragColor = color;
    }
    )";
}  // namespace pag
