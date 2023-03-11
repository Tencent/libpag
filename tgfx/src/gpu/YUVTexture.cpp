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

#include "YUVTexture.h"
#include "gpu/Gpu.h"
#include "utils/UniqueID.h"

namespace tgfx {
static constexpr int YUV_SIZE_FACTORS[] = {0, 1, 1};

static void ComputeRecycleKey(BytesKey* recycleKey, int width, int height, YUVPixelFormat format) {
  static const uint32_t YUVTextureType = UniqueID::Next();
  recycleKey->write(YUVTextureType);
  recycleKey->write(static_cast<uint32_t>(width));
  recycleKey->write(static_cast<uint32_t>(height));
  recycleKey->write(static_cast<uint32_t>(format));
}

static std::vector<std::unique_ptr<TextureSampler>> MakeTexturePlanes(Context* context,
                                                                      const YUVData* yuvData,
                                                                      const PixelFormat* formats) {
  std::vector<std::unique_ptr<TextureSampler>> texturePlanes = {};
  auto count = static_cast<int>(yuvData->planeCount());
  for (int index = 0; index < count; index++) {
    auto w = yuvData->width() >> YUV_SIZE_FACTORS[index];
    auto h = yuvData->height() >> YUV_SIZE_FACTORS[index];
    auto sampler = context->gpu()->createSampler(w, h, formats[index], 1);
    if (sampler == nullptr) {
      for (auto& plane : texturePlanes) {
        context->gpu()->deleteSampler(plane.get());
      }
      return {};
    }
    texturePlanes.push_back(std::move(sampler));
  }
  return texturePlanes;
}

static void SubmitYUVTexture(Context* context, const YUVData* yuvData, const PixelFormat* formats,
                             const std::vector<std::unique_ptr<TextureSampler>>* samplers) {
  auto count = static_cast<int>(yuvData->planeCount());
  for (int index = 0; index < count; index++) {
    auto sampler = (*samplers)[index].get();
    auto w = yuvData->width() >> YUV_SIZE_FACTORS[index];
    auto h = yuvData->height() >> YUV_SIZE_FACTORS[index];
    auto pixels = yuvData->getBaseAddressAt(index);
    auto rowBytes = yuvData->getRowBytesAt(index);
    auto format = formats[index];
    context->gpu()->writePixels(sampler, Rect::MakeWH(w, h), pixels, rowBytes, format);
  }
}

std::shared_ptr<Texture> Texture::MakeI420(Context* context, const YUVData* yuvData,
                                           YUVColorSpace colorSpace) {
  if (context == nullptr || yuvData == nullptr ||
      yuvData->planeCount() != YUVData::I420_PLANE_COUNT) {
    return nullptr;
  }
  PixelFormat yuvFormats[YUVData::I420_PLANE_COUNT] = {PixelFormat::GRAY_8, PixelFormat::GRAY_8,
                                                       PixelFormat::GRAY_8};
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, yuvData->width(), yuvData->height(), YUVPixelFormat::I420);
  auto texture =
      std::static_pointer_cast<YUVTexture>(context->resourceCache()->getRecycled(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(context, yuvData, yuvFormats);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<YUVTexture>(Resource::Wrap(
        context,
        new YUVTexture(yuvData->width(), yuvData->height(), YUVPixelFormat::I420, colorSpace)));
    texture->samplers = std::move(texturePlanes);
  }
  SubmitYUVTexture(context, yuvData, yuvFormats, &texture->samplers);
  return texture;
}

std::shared_ptr<Texture> Texture::MakeNV12(Context* context, const YUVData* yuvData,
                                           YUVColorSpace colorSpace) {
  if (context == nullptr || yuvData == nullptr ||
      yuvData->planeCount() != YUVData::NV12_PLANE_COUNT) {
    return nullptr;
  }
  PixelFormat yuvFormats[YUVData::NV12_PLANE_COUNT] = {PixelFormat::GRAY_8, PixelFormat::RG_88};
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, yuvData->width(), yuvData->height(), YUVPixelFormat::NV12);
  auto texture =
      std::static_pointer_cast<YUVTexture>(context->resourceCache()->getRecycled(recycleKey));
  if (texture == nullptr) {
    auto texturePlanes = MakeTexturePlanes(context, yuvData, yuvFormats);
    if (texturePlanes.empty()) {
      return nullptr;
    }
    texture = std::static_pointer_cast<YUVTexture>(Resource::Wrap(
        context,
        new YUVTexture(yuvData->width(), yuvData->height(), YUVPixelFormat::NV12, colorSpace)));
    texture->samplers = std::move(texturePlanes);
  }
  SubmitYUVTexture(context, yuvData, yuvFormats, &texture->samplers);
  return texture;
}

YUVTexture::YUVTexture(int width, int height, YUVPixelFormat pixelFormat, YUVColorSpace colorSpace)
    : Texture(width, height, ImageOrigin::TopLeft),
      _pixelFormat(pixelFormat),
      _colorSpace(colorSpace) {
}

Point YUVTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

BackendTexture YUVTexture::getBackendTexture() const {
  return {};
}

const TextureSampler* YUVTexture::getSamplerAt(size_t index) const {
  if (index >= samplers.size()) {
    return nullptr;
  }
  return samplers[index].get();
}

size_t YUVTexture::memoryUsage() const {
  return width() * height() * 3 / 2;
}

void YUVTexture::computeRecycleKey(BytesKey* recycleKey) const {
  ComputeRecycleKey(recycleKey, width(), height(), _pixelFormat);
}

void YUVTexture::onReleaseGPU() {
  for (auto& sampler : samplers) {
    context->gpu()->deleteSampler(sampler.get());
  }
}
}  // namespace tgfx
