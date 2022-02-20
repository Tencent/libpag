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

#include "EAGLNV12Texture.h"
#include "gpu/opengl/eagl/EAGLDevice.h"

namespace tgfx {
static GLTextureInfo ToGLTexture(CVOpenGLESTextureRef texture, unsigned format) {
  GLTextureInfo glInfo = {};
  glInfo.target = CVOpenGLESTextureGetTarget(texture);
  glInfo.id = CVOpenGLESTextureGetName(texture);
  glInfo.format = format;
  return glInfo;
}

std::shared_ptr<EAGLNV12Texture> EAGLNV12Texture::MakeFrom(Context* context,
                                                           CVPixelBufferRef pixelBuffer,
                                                           YUVColorSpace colorSpace,
                                                           YUVColorRange colorRange) {
  auto glDevice = std::static_pointer_cast<EAGLDevice>(GLDevice::Current());
  if (glDevice == nullptr) {
    return nullptr;
  }
  CVOpenGLESTextureCacheRef textureCache = glDevice->getTextureCache();
  if (textureCache == nil) {
    return nullptr;
  }
  auto width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
  auto height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
  CVOpenGLESTextureRef outputTextureLuma = nil;
  CVOpenGLESTextureRef outputTextureChroma = nil;
  auto oneComponentFormat = PixelFormat::GRAY_8;
  auto gl = GLContext::Unwrap(context);
  const auto& oneComponentFormat = gl->caps->getTextureFormat(oneComponentFormat);
  // 返回的 texture 对象是一个强引用计数为 1 的对象。
  CVOpenGLESTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, textureCache, pixelBuffer, NULL, GL_TEXTURE_2D,
      oneComponentFormat.internalFormatTexImage, width, height, oneComponentFormat.externalFormat,
      GL_UNSIGNED_BYTE, 0, &outputTextureLuma);
  auto twoComponentFormat = PixelFormat::RG_88;
  const auto& twoComponentFormat = gl->caps->getTextureFormat(twoComponentFormat);
  // 返回的 texture 对象是一个强引用计数为 1 的对象。
  CVOpenGLESTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, textureCache, pixelBuffer, NULL, GL_TEXTURE_2D,
      twoComponentFormat.internalFormatTexImage, width / 2, height / 2,
      twoComponentFormat.externalFormat, GL_UNSIGNED_BYTE, 1, &outputTextureChroma);
  if (outputTextureLuma == nil || outputTextureChroma == nil) {
    return nullptr;
  }
  auto texture = Resource::Wrap(context, new EAGLNV12Texture(pixelBuffer, colorSpace, colorRange));
  texture->lumaTexture = outputTextureLuma;
  texture->samplers.emplace_back(oneComponentFormat,
                                 ToGLTexture(outputTextureLuma, oneComponentFormat.sizedFormat));
  texture->chromaTexture = outputTextureChroma;
  texture->samplers.emplace_back(twoComponentFormat,
                                 ToGLTexture(outputTextureChroma, twoComponentFormat.sizedFormat));
  return texture;
}

EAGLNV12Texture::EAGLNV12Texture(CVPixelBufferRef pixelBuffer, YUVColorSpace colorSpace,
                                 YUVColorRange colorRange)
    : GLYUVTexture(colorSpace, colorRange, static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)),
                   static_cast<int>(CVPixelBufferGetHeight(pixelBuffer))),
      pixelBuffer(pixelBuffer) {
  CFRetain(pixelBuffer);
}

EAGLNV12Texture::~EAGLNV12Texture() {
  CFRelease(pixelBuffer);
  if (lumaTexture) {
    CFRelease(lumaTexture);
  }
  if (chromaTexture) {
    CFRelease(chromaTexture);
  }
}

size_t EAGLNV12Texture::memoryUsage() const {
  // 显存来自 CVPixelBuffer，这里不做重复统计。
  return 0;
}

void EAGLNV12Texture::onRelease(Context*) {
  if (lumaTexture == nil || chromaTexture == nil) {
    return;
  }
  auto glDevice = std::static_pointer_cast<EAGLDevice>(GLDevice::Current());
  if (glDevice == nullptr) {
    return;
  }
  auto cache = glDevice->getTextureCache();
  if (!cache) {
    return;
  }
  CFRelease(lumaTexture);
  lumaTexture = nil;
  CFRelease(chromaTexture);
  chromaTexture = nil;
  CVOpenGLESTextureCacheFlush(cache, 0);
}
}
