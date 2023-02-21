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

#include "tgfx/gpu/opengl/GLYUVTexture.h"
#include "GLUtil.h"
#include "gpu/Gpu.h"
#include "utils/UniqueID.h"

namespace tgfx {
static constexpr int YUV_SIZE_FACTORS[] = {0, 1, 1};

class GLI420Texture : public GLYUVTexture {
 public:
  static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height) {
    static const uint32_t I420Type = UniqueID::Next();
    recycleKey->write(I420Type);
    recycleKey->write(static_cast<uint32_t>(width));
    recycleKey->write(static_cast<uint32_t>(height));
  }

  GLI420Texture(int width, int height, YUVColorSpace colorSpace, YUVColorRange colorRange)
      : GLYUVTexture(width, height, colorSpace, colorRange) {
  }

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::I420;
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
  GLNV12Texture(int width, int height, YUVColorSpace colorSpace, YUVColorRange colorRange)
      : GLYUVTexture(width, height, colorSpace, colorRange) {
  }

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::NV12;
  }

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override {
    ComputeRecycleKey(recycleKey, width(), height());
  }
};

static std::vector<GLSampler> MakeTexturePlanes(Context* context, const YUVData* yuvData,
                                                const PixelFormat* formats) {
  std::vector<GLSampler> texturePlanes{};
  auto count = static_cast<int>(yuvData->planeCount());
  for (int index = 0; index < count; index++) {
    auto w = yuvData->width() >> YUV_SIZE_FACTORS[index];
    auto h = yuvData->height() >> YUV_SIZE_FACTORS[index];
    auto sampler = context->gpu()->createSampler(w, h, formats[index], 1);
    if (sampler == nullptr) {
      for (auto& glSampler : texturePlanes) {
        context->gpu()->deleteSampler(&glSampler);
      }
      return {};
    }
    texturePlanes.emplace_back(*static_cast<GLSampler*>(sampler.get()));
  }
  return texturePlanes;
}

static void SubmitYUVTexture(Context* context, const YUVData* yuvData, const PixelFormat* formats,
                             const GLSampler yuvTextures[]) {
  auto count = static_cast<int>(yuvData->planeCount());
  for (int index = 0; index < count; index++) {
    const auto& sampler = yuvTextures[index];
    auto w = yuvData->width() >> YUV_SIZE_FACTORS[index];
    auto h = yuvData->height() >> YUV_SIZE_FACTORS[index];
    auto pixels = yuvData->getBaseAddressAt(index);
    auto rowBytes = yuvData->getRowBytesAt(index);
    auto format = formats[index];
    context->gpu()->writePixels(&sampler, Rect::MakeWH(w, h), pixels, rowBytes, format);
  }
}

std::shared_ptr<YUVTexture> YUVTexture::MakeI420(Context* context, const YUVData* yuvData,
                                                 YUVColorSpace colorSpace,
                                                 YUVColorRange colorRange) {
  if (context == nullptr || yuvData == nullptr ||
      yuvData->planeCount() != YUVData::I420_PLANE_COUNT) {
    return nullptr;
  }
  PixelFormat yuvFormats[YUVData::I420_PLANE_COUNT] = {PixelFormat::GRAY_8, PixelFormat::GRAY_8,
                                                       PixelFormat::GRAY_8};
  BytesKey recycleKey = {};
  GLI420Texture::ComputeRecycleKey(&recycleKey, yuvData->width(), yuvData->height());
  auto texture =
      std::static_pointer_cast<GLYUVTexture>(context->resourceCache()->getRecycled(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(context, yuvData, yuvFormats);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<GLYUVTexture>(Resource::Wrap(
        context, new GLI420Texture(yuvData->width(), yuvData->height(), colorSpace, colorRange)));
    texture->samplers = texturePlanes;
  }
  SubmitYUVTexture(context, yuvData, yuvFormats, &texture->samplers[0]);
  return texture;
}

std::shared_ptr<YUVTexture> YUVTexture::MakeNV12(Context* context, const YUVData* yuvData,
                                                 YUVColorSpace colorSpace,
                                                 YUVColorRange colorRange) {
  if (context == nullptr || yuvData == nullptr ||
      yuvData->planeCount() != YUVData::NV12_PLANE_COUNT) {
    return nullptr;
  }
  PixelFormat yuvFormats[YUVData::NV12_PLANE_COUNT] = {PixelFormat::GRAY_8, PixelFormat::RG_88};
  BytesKey recycleKey = {};
  GLNV12Texture::ComputeRecycleKey(&recycleKey, yuvData->width(), yuvData->height());
  auto texture =
      std::static_pointer_cast<GLYUVTexture>(context->resourceCache()->getRecycled(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(context, yuvData, yuvFormats);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<GLYUVTexture>(Resource::Wrap(
        context, new GLNV12Texture(yuvData->width(), yuvData->height(), colorSpace, colorRange)));
    texture->samplers = texturePlanes;
  }
  SubmitYUVTexture(context, yuvData, yuvFormats, &texture->samplers[0]);
  return texture;
}

GLYUVTexture::GLYUVTexture(int width, int height, YUVColorSpace colorSpace,
                           YUVColorRange colorRange)
    : YUVTexture(width, height, colorSpace, colorRange) {
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

size_t GLYUVTexture::memoryUsage() const {
  return width() * height() * 3 / 2;
}

void GLYUVTexture::onReleaseGPU() {
  for (auto& sampler : samplers) {
    context->gpu()->deleteSampler(&sampler);
  }
}
}  // namespace tgfx
