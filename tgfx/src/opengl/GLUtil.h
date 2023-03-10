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

#include <array>
#include <string>
#include "opengl/GLContext.h"
#include "opengl/GLSampler.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/gpu/ImageOrigin.h"

namespace tgfx {
struct GLVersion {
  int majorVersion = -1;
  int minorVersion = -1;

  GLVersion() = default;

  GLVersion(int major, int minor) : majorVersion(major), minorVersion(minor) {
  }
};

PixelFormat GLSizeFormatToPixelFormat(unsigned sizeFormat);

unsigned PixelFormatToGLSizeFormat(PixelFormat pixelFormat);

GLVersion GetGLVersion(const char* versionString);

unsigned CreateGLProgram(Context* context, const std::string& vertex, const std::string& fragment);

unsigned LoadGLShader(Context* context, unsigned shaderType, const std::string& source);

bool CheckGLErrorImpl(Context* context, std::string file, int line);

std::array<float, 9> ToGLMatrix(const Matrix& matrix);

#ifdef DEBUG

#define CheckGLError(context) CheckGLErrorImpl(context, __FILE__, __LINE__)

#else

#define CheckGLError(context) CheckGLErrorImpl(context, "", 0)

#endif
}  // namespace tgfx
