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

#include "GLYUVTexture.h"
#include "GLUtil.h"

namespace pag {
#define I420_PLANE_COUNT 3
#define NV12_PLANE_COUNT 2
#define I420_PIXEL_BYTES 1.5
#define NV12_PIXEL_BYTES 1.5

struct YUVConfig {
  YUVConfig(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height,
            int planeCount)
      : colorSpace(colorSpace),
        colorRange(colorRange),
        width(width),
        height(height),
        planeCount(planeCount) {
  }
  YUVColorSpace colorSpace;
  YUVColorRange colorRange;
  int width = 0;
  int height = 0;
  uint8_t* pixelsPlane[3]{};
  int rowBytes[3]{};
  int bytesPerPixel[3]{};
  TextureFormat formats[3]{};
  int planeCount = 0;
};

class GLI420Texture : public GLYUVTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height) {
    static const uint32_t I420Type = UniqueID::Next();
    recycleKey->write(I420Type);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
  }

  GLI420Texture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height)
      : GLYUVTexture(colorSpace, colorRange, width, height) {
  }

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::I420;
  }

  size_t memoryUsage() const override {
    return static_cast<size_t>(width() * height() * I420_PIXEL_BYTES);
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height());
  }
};

class GLNV12Texture : public GLYUVTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height) {
    static const uint32_t NV12Type = UniqueID::Next();
    recycleKey->write(NV12Type);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
  }
  GLNV12Texture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width, int height)
      : GLYUVTexture(colorSpace, colorRange, width, height) {
  }

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::NV12;
  }
  size_t memoryUsage() const override {
    return static_cast<size_t>(width() * height() * NV12_PIXEL_BYTES);
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height());
  }
};

static std::vector<GLTextureInfo> MakeTexturePlanes(const GLInterface* gl,
                                                    const YUVConfig& yuvConfig) {
  std::vector<GLTextureInfo> texturePlanes{};
  unsigned yuvTextureIDs[] = {0, 0, 0};
  gl->genTextures(yuvConfig.planeCount, yuvTextureIDs);
  if (yuvTextureIDs[0] == 0) {
    return texturePlanes;
  }
  for (int index = 0; index < yuvConfig.planeCount; index++) {
    GLTextureInfo glInfo = {};
    glInfo.id = yuvTextureIDs[index];
    glInfo.target = GL::TEXTURE_2D;
    glInfo.format = yuvConfig.formats[index].sizedFormat;
    texturePlanes.emplace_back(glInfo);
  }
  return texturePlanes;
}

static void SubmitYUVTexture(const GLInterface* gl, const YUVConfig& yuvConfig,
                             GLTextureSampler yuvTextures[]) {
  static constexpr int factor[] = {0, 1, 1};
  for (int index = 0; index < yuvConfig.planeCount; index++) {
    auto glInfo = yuvTextures[index].glInfo;
    auto format = yuvConfig.formats[index];
    auto w = yuvConfig.width >> factor[index];
    auto h = yuvConfig.height >> factor[index];
    auto rowBytes = yuvConfig.rowBytes[index];
    auto bytesPerPixel = yuvConfig.bytesPerPixel[index];
    auto pixels = yuvConfig.pixelsPlane[index];
    SubmitTexture(gl, glInfo, format, w, h, rowBytes, bytesPerPixel, pixels);
  }
}

std::shared_ptr<YUVTexture> YUVTexture::MakeI420(Context* context, YUVColorSpace colorSpace,
                                                 YUVColorRange colorRange, int width, int height,
                                                 uint8_t* pixelsPlane[3], const int lineSize[3]) {
  auto gl = GLContext::Unwrap(context);
  GLStateGuard stateGuard(context);

  YUVConfig yuvConfig = YUVConfig(colorSpace, colorRange, width, height, I420_PLANE_COUNT);
  auto pixelConfig = PixelConfig::Gray_8;
  const auto& format = gl->caps->getTextureFormat(pixelConfig);
  for (int i = 0; i < 3; i++) {
    yuvConfig.pixelsPlane[i] = pixelsPlane[i];
    yuvConfig.rowBytes[i] = lineSize[i];
    yuvConfig.formats[i] = format;
    yuvConfig.bytesPerPixel[i] = 1;
  }

  BytesKey recycleKey = {};
  GLI420Texture::ComputeRecycleKey(&recycleKey, width, height);
  auto texture = std::static_pointer_cast<GLYUVTexture>(context->getRecycledResource(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(gl, yuvConfig);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<GLYUVTexture>(
        Resource::Wrap(context, new GLI420Texture(yuvConfig.colorSpace, yuvConfig.colorRange,
                                                  yuvConfig.width, yuvConfig.height)));
    texture->samplers.emplace_back(pixelConfig, texturePlanes[0]);
    texture->samplers.emplace_back(pixelConfig, texturePlanes[1]);
    texture->samplers.emplace_back(pixelConfig, texturePlanes[2]);
  }
  SubmitYUVTexture(gl, yuvConfig, &texture->samplers[0]);
  return texture;
}

std::shared_ptr<YUVTexture> YUVTexture::MakeNV12(Context* context, YUVColorSpace colorSpace,
                                                 YUVColorRange colorRange, int width, int height,
                                                 uint8_t* pixelsPlane[2], const int lineSize[2]) {
  auto gl = GLContext::Unwrap(context);
  GLStateGuard stateGuard(context);

  YUVConfig yuvConfig = YUVConfig(colorSpace, colorRange, width, height, NV12_PLANE_COUNT);
  PixelConfig pixelConfig[2] = {PixelConfig::Gray_8, PixelConfig::RG_88};
  for (int i = 0; i < 2; i++) {
    yuvConfig.pixelsPlane[i] = pixelsPlane[i];
    yuvConfig.rowBytes[i] = lineSize[i];
    yuvConfig.formats[i] = gl->caps->getTextureFormat(pixelConfig[i]);
  }
  yuvConfig.bytesPerPixel[0] = 1;
  yuvConfig.bytesPerPixel[1] = 2;

  BytesKey recycleKey = {};
  GLNV12Texture::ComputeRecycleKey(&recycleKey, width, height);
  auto texture = std::static_pointer_cast<GLYUVTexture>(context->getRecycledResource(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(gl, yuvConfig);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<GLYUVTexture>(
        Resource::Wrap(context, new GLNV12Texture(yuvConfig.colorSpace, yuvConfig.colorRange,
                                                  yuvConfig.width, yuvConfig.height)));
    texture->samplers.emplace_back(pixelConfig[0], texturePlanes[0]);
    texture->samplers.emplace_back(pixelConfig[1], texturePlanes[1]);
  }
  SubmitYUVTexture(gl, yuvConfig, &texture->samplers[0]);
  return texture;
}

GLYUVTexture::GLYUVTexture(YUVColorSpace colorSpace, YUVColorRange colorRange, int width,
                           int height)
    : YUVTexture(colorSpace, colorRange, width, height) {
}

Point GLYUVTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

const TextureSampler* GLYUVTexture::getSamplerAt(size_t index) const {
  if (index >= samplers.size()) {
    return nullptr;
  }
  return &samplers[index];
}

void GLYUVTexture::onRelease(Context* context) {
  auto gl = GLContext::Unwrap(context);
  for (const auto& sampler : samplers) {
    gl->deleteTextures(1, &sampler.glInfo.id);
  }
}
}  // namespace pag
