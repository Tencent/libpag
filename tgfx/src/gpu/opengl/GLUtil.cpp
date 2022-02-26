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

#include "GLUtil.h"
#include "core/utils/USE.h"

namespace tgfx {
GLVersion GetGLVersion(const char* versionString) {
  if (versionString == nullptr) {
    return {};
  }
  int major, minor;
  int mesaMajor, mesaMinor;
  int n = sscanf(versionString, "%d.%d Mesa %d.%d", &major, &minor, &mesaMajor, &mesaMinor);
  if (4 == n) {
    return {major, minor};
  }
  n = sscanf(versionString, "%d.%d", &major, &minor);
  if (2 == n) {
    return {major, minor};
  }
  int esMajor, esMinor;
  n = sscanf(versionString, "OpenGL ES %d.%d (WebGL %d.%d", &esMajor, &esMinor, &major, &minor);
  if (4 == n) {
    return {major, minor};
  }
  char profile[2];
  n = sscanf(versionString, "OpenGL ES-%c%c %d.%d", profile, profile + 1, &major, &minor);
  if (4 == n) {
    return {major, minor};
  }
  n = sscanf(versionString, "OpenGL ES %d.%d", &major, &minor);
  if (2 == n) {
    return {major, minor};
  }
  return {};
}

bool CreateGLTexture(const GLInterface* gl, int width, int height, GLSampler* texture) {
  texture->target = GL_TEXTURE_2D;
  texture->format = PixelFormat::RGBA_8888;
  gl->functions->genTextures(1, &texture->id);
  if (texture->id <= 0) {
    return false;
  }
  gl->functions->bindTexture(texture->target, texture->id);
  gl->functions->texParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->functions->texParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->functions->texParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->functions->texParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->functions->texImage2D(texture->target, 0, GL_RGBA, width, height, 0, GL_RGBA,
                            GL_UNSIGNED_BYTE, nullptr);
  gl->functions->bindTexture(texture->target, 0);
  return true;
}

static std::array<unsigned, 4> GetGLSwizzleValues(const Swizzle& swizzle) {
  unsigned glValues[4];
  for (int i = 0; i < 4; ++i) {
    switch (swizzle[i]) {
      case 'r':
        glValues[i] = GL_RED;
        break;
      case 'g':
        glValues[i] = GL_GREEN;
        break;
      case 'b':
        glValues[i] = GL_BLUE;
        break;
      case 'a':
        glValues[i] = GL_ALPHA;
        break;
      case '1':
        glValues[i] = GL_ONE;
        break;
      default:
        LOGE("Unsupported component");
    }
  }
  return {glValues[0], glValues[1], glValues[2], glValues[3]};
}

void ActiveGLTexture(const GLInterface* gl, unsigned textureUnit, unsigned target,
                     unsigned textureID, PixelFormat pixelFormat) {
  gl->functions->activeTexture(textureUnit);
  gl->functions->bindTexture(target, textureID);
  gl->functions->texParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->functions->texParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->functions->texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->functions->texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (gl->caps->textureSwizzleSupport) {
    const auto& swizzle = gl->caps->getSwizzle(pixelFormat);
    auto glValues = GetGLSwizzleValues(swizzle);
    if (gl->caps->standard == GLStandard::GL) {
      gl->functions->texParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA,
                                    reinterpret_cast<const int*>(&glValues[0]));
    } else if (gl->caps->standard == GLStandard::GLES) {
      // ES3 added swizzle support but not GL_TEXTURE_SWIZZLE_RGBA.
      gl->functions->texParameteri(target, GL_TEXTURE_SWIZZLE_R, static_cast<int>(glValues[0]));
      gl->functions->texParameteri(target, GL_TEXTURE_SWIZZLE_G, static_cast<int>(glValues[1]));
      gl->functions->texParameteri(target, GL_TEXTURE_SWIZZLE_B, static_cast<int>(glValues[2]));
      gl->functions->texParameteri(target, GL_TEXTURE_SWIZZLE_A, static_cast<int>(glValues[3]));
    }
  }
}

void SubmitGLTexture(const GLInterface* gl, const GLSampler& sampler, int width, int height,
                     size_t rowBytes, int bytesPerPixel, void* pixels) {
  if (pixels == nullptr || rowBytes == 0) {
    return;
  }
  const auto& format = gl->caps->getTextureFormat(sampler.format);
  gl->functions->bindTexture(sampler.target, sampler.id);
  gl->functions->pixelStorei(GL_UNPACK_ALIGNMENT, bytesPerPixel);
  if (gl->caps->unpackRowLengthSupport) {
    // the number of pixels, not bytes
    gl->functions->pixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<int>(rowBytes / bytesPerPixel));
    gl->functions->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage),
                              width, height, 0, format.externalFormat, GL_UNSIGNED_BYTE, pixels);
  } else {
    if (static_cast<size_t>(width) * bytesPerPixel == rowBytes) {
      gl->functions->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage),
                                width, height, 0, format.externalFormat, GL_UNSIGNED_BYTE, pixels);
    } else {
      gl->functions->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage),
                                width, height, 0, format.externalFormat, GL_UNSIGNED_BYTE, nullptr);
      auto data = reinterpret_cast<uint8_t*>(pixels);
      for (int row = 0; row < height; ++row) {
        gl->functions->texSubImage2D(sampler.target, 0, 0, row, width, 1, format.externalFormat,
                                     GL_UNSIGNED_BYTE, data + (row * rowBytes));
      }
    }
  }
  gl->functions->bindTexture(sampler.target, 0);
}

unsigned CreateGLProgram(const GLInterface* gl, const std::string& vertex,
                         const std::string& fragment) {
  auto vertexShader = LoadGLShader(gl, GL_VERTEX_SHADER, vertex);
  if (vertexShader == 0) {
    return 0;
  }
  auto fragmentShader = LoadGLShader(gl, GL_FRAGMENT_SHADER, fragment);
  if (fragmentShader == 0) {
    return 0;
  }
  auto programHandle = gl->functions->createProgram();
  gl->functions->attachShader(programHandle, vertexShader);
  gl->functions->attachShader(programHandle, fragmentShader);
  gl->functions->linkProgram(programHandle);
  int success;
  gl->functions->getProgramiv(programHandle, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->functions->getProgramInfoLog(programHandle, 512, nullptr, infoLog);
    gl->functions->deleteProgram(programHandle);
  }
  gl->functions->deleteShader(vertexShader);
  gl->functions->deleteShader(fragmentShader);
  return programHandle;
}

unsigned LoadGLShader(const GLInterface* gl, unsigned shaderType, const std::string& source) {
  auto shader = gl->functions->createShader(shaderType);
  const char* files[] = {source.c_str()};
  gl->functions->shaderSource(shader, 1, files, nullptr);
  gl->functions->compileShader(shader);
  int success;
  gl->functions->getShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->functions->getShaderInfoLog(shader, 512, nullptr, infoLog);
    LOGE("Could not compile shader: %d %s", shaderType, infoLog);
    gl->functions->deleteShader(shader);
    shader = 0;
  }
  return shader;
}

bool CheckGLError(const GLInterface* gl) {
#ifdef TGFX_BUILD_FOR_WEB
  USE(gl);
  return true;
#else
  bool success = true;
  unsigned errorCode;
  while ((errorCode = gl->functions->getError()) != GL_NO_ERROR) {
    success = false;
    LOGE("glCheckError: %d", errorCode);
  }
  return success;
#endif
}

std::array<float, 9> ToGLMatrix(const Matrix& matrix) {
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[6], values[1], values[4],
          values[7], values[2], values[5], values[8]};
}

std::array<float, 9> ToGLVertexMatrix(const Matrix& matrix, int width, int height,
                                      ImageOrigin origin) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  auto result = matrix;
  Matrix convertMatrix = {};
  // The following is equivalent：
  // convertMatrix.setScale(1.0f, -1.0f);
  // convertMatrix.postTranslate(1.0f, 1.0f);
  // convertMatrix.postScale(width/2.0f, height/2f);
  convertMatrix.setAll(w * 0.5f, 0.0f, w * 0.5f, 0.0f, h * -0.5f, h * 0.5f, 0.0f, 0.0f, 1.0f);
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == ImageOrigin::TopLeft) {
    result.postScale(1.0f, -1.0f);
  }
  return ToGLMatrix(result);
}

std::array<float, 9> ToGLTextureMatrix(const Matrix& matrix, int width, int height,
                                       ImageOrigin origin) {
  auto w = static_cast<float>(width);
  auto h = static_cast<float>(height);
  auto result = matrix;
  Matrix convertMatrix = {};
  // The following is equivalent：
  // convertMatrix.setScale(1.0f, -1.0f);
  // convertMatrix.postTranslate(0.0f, 1.0f);
  // convertMatrix.postScale(width, height);
  convertMatrix.setAll(w, 0.0f, 0.0f, 0.0f, -h, h, 0.0f, 0.0f, 1.0f);
  result.preConcat(convertMatrix);
  if (convertMatrix.invert(&convertMatrix)) {
    result.postConcat(convertMatrix);
  }
  if (origin == ImageOrigin::TopLeft) {
    result.postScale(1.0f, -1.0f);
    result.postTranslate(0.0f, 1.0f);
  }
  return ToGLMatrix(result);
}
}  // namespace tgfx
