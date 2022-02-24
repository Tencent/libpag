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
  texture->target = GL::TEXTURE_2D;
  texture->format = PixelFormat::RGBA_8888;
  gl->genTextures(1, &texture->id);
  if (texture->id <= 0) {
    return false;
  }
  gl->bindTexture(texture->target, texture->id);
  gl->texParameteri(texture->target, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
  gl->texParameteri(texture->target, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
  gl->texImage2D(texture->target, 0, GL::RGBA, width, height, 0, GL::RGBA, GL::UNSIGNED_BYTE,
                 nullptr);
  gl->bindTexture(texture->target, 0);
  return true;
}

static std::array<unsigned, 4> GetGLSwizzleValues(const Swizzle& swizzle) {
  unsigned glValues[4];
  for (int i = 0; i < 4; ++i) {
    switch (swizzle[i]) {
      case 'r':
        glValues[i] = GL::RED;
        break;
      case 'g':
        glValues[i] = GL::GREEN;
        break;
      case 'b':
        glValues[i] = GL::BLUE;
        break;
      case 'a':
        glValues[i] = GL::ALPHA;
        break;
      case '1':
        glValues[i] = GL::ONE;
        break;
      default:
        LOGE("Unsupported component");
    }
  }
  return {glValues[0], glValues[1], glValues[2], glValues[3]};
}

void ActiveGLTexture(const GLInterface* gl, unsigned textureUnit, unsigned target,
                     unsigned textureID, PixelFormat pixelFormat) {
  gl->activeTexture(textureUnit);
  gl->bindTexture(target, textureID);
  gl->texParameteri(target, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
  gl->texParameteri(target, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
  gl->texParameteri(target, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
  gl->texParameteri(target, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
  if (gl->caps->textureSwizzleSupport) {
    const auto& swizzle = gl->caps->getSwizzle(pixelFormat);
    auto glValues = GetGLSwizzleValues(swizzle);
    if (gl->caps->standard == GLStandard::GL) {
      gl->texParameteriv(target, GL::TEXTURE_SWIZZLE_RGBA,
                         reinterpret_cast<const int*>(&glValues[0]));
    } else if (gl->caps->standard == GLStandard::GLES) {
      // ES3 added swizzle support but not GL::TEXTURE_SWIZZLE_RGBA.
      gl->texParameteri(target, GL::TEXTURE_SWIZZLE_R, static_cast<int>(glValues[0]));
      gl->texParameteri(target, GL::TEXTURE_SWIZZLE_G, static_cast<int>(glValues[1]));
      gl->texParameteri(target, GL::TEXTURE_SWIZZLE_B, static_cast<int>(glValues[2]));
      gl->texParameteri(target, GL::TEXTURE_SWIZZLE_A, static_cast<int>(glValues[3]));
    }
  }
}

void SubmitGLTexture(const GLInterface* gl, const GLSampler& sampler, int width, int height,
                     size_t rowBytes, int bytesPerPixel, void* pixels) {
  if (pixels == nullptr || rowBytes == 0) {
    return;
  }
  const auto& format = gl->caps->getTextureFormat(sampler.format);
  gl->bindTexture(sampler.target, sampler.id);
  gl->pixelStorei(GL::UNPACK_ALIGNMENT, bytesPerPixel);
  if (gl->caps->unpackRowLengthSupport) {
    // the number of pixels, not bytes
    gl->pixelStorei(GL::UNPACK_ROW_LENGTH, static_cast<int>(rowBytes / bytesPerPixel));
    gl->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage), width,
                   height, 0, format.externalFormat, GL::UNSIGNED_BYTE, pixels);
  } else {
    if (static_cast<size_t>(width) * bytesPerPixel == rowBytes) {
      gl->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage), width,
                     height, 0, format.externalFormat, GL::UNSIGNED_BYTE, pixels);
    } else {
      gl->texImage2D(sampler.target, 0, static_cast<int>(format.internalFormatTexImage), width,
                     height, 0, format.externalFormat, GL::UNSIGNED_BYTE, nullptr);
      auto data = reinterpret_cast<uint8_t*>(pixels);
      for (int row = 0; row < height; ++row) {
        gl->texSubImage2D(sampler.target, 0, 0, row, width, 1, format.externalFormat,
                          GL::UNSIGNED_BYTE, data + (row * rowBytes));
      }
    }
  }
  gl->bindTexture(sampler.target, 0);
}

unsigned CreateGLProgram(const GLInterface* gl, const std::string& vertex,
                         const std::string& fragment) {
  auto vertexShader = LoadGLShader(gl, GL::VERTEX_SHADER, vertex);
  if (vertexShader == 0) {
    return 0;
  }
  auto fragmentShader = LoadGLShader(gl, GL::FRAGMENT_SHADER, fragment);
  if (fragmentShader == 0) {
    return 0;
  }
  auto programHandle = gl->createProgram();
  gl->attachShader(programHandle, vertexShader);
  gl->attachShader(programHandle, fragmentShader);
  gl->linkProgram(programHandle);
  int success;
  gl->getProgramiv(programHandle, GL::LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->getProgramInfoLog(programHandle, 512, nullptr, infoLog);
    gl->deleteProgram(programHandle);
  }
  gl->deleteShader(vertexShader);
  gl->deleteShader(fragmentShader);
  return programHandle;
}

unsigned LoadGLShader(const GLInterface* gl, unsigned shaderType, const std::string& source) {
  auto shader = gl->createShader(shaderType);
  const char* files[] = {source.c_str()};
  gl->shaderSource(shader, 1, files, nullptr);
  gl->compileShader(shader);
  int success;
  gl->getShaderiv(shader, GL::COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    gl->getShaderInfoLog(shader, 512, nullptr, infoLog);
    LOGE("Could not compile shader: %d %s", shaderType, infoLog);
    gl->deleteShader(shader);
    shader = 0;
  }
  return shader;
}

bool CheckGLError(const GLInterface* gl) {
  bool success = true;
  unsigned errorCode;
  while ((errorCode = gl->getError()) != GL::NO_ERROR) {
    success = false;
    LOGE("glCheckError: %d", errorCode);
  }
  return success;
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
