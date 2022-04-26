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

#include "GradientCache.h"

#include <utility>

namespace tgfx {
// Each bitmap will be 256x1.
static constexpr size_t kMaxNumCachedGradientBitmaps = 32;
static constexpr int kGradientTextureSize = 256;

const Texture* GradientCache::find(const BytesKey& bytesKey) {
  auto iter = textures.find(bytesKey);
  if (iter == textures.end()) {
    return nullptr;
  }
  keys.remove(bytesKey);
  keys.push_front(bytesKey);
  return iter->second.get();
}

void GradientCache::add(const BytesKey& bytesKey, std::shared_ptr<Texture> texture) {
  textures[bytesKey] = std::move(texture);
  keys.push_front(bytesKey);
  while (keys.size() > kMaxNumCachedGradientBitmaps) {
    auto key = keys.back();
    keys.pop_back();
    textures.erase(key);
  }
}

std::shared_ptr<PixelBuffer> CreateGradient(const Color* colors, const float* positions, int count,
                                            int resolution) {
  auto pixelBuffer = PixelBuffer::Make(resolution, 1, false, false);
  Bitmap bitmap(pixelBuffer);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  bitmap.eraseAll();
  auto* pixels = reinterpret_cast<uint8_t*>(bitmap.writablePixels());
  int prevIndex = 0;
  for (int i = 1; i < count; ++i) {
    int nextIndex =
        std::min(static_cast<int>(positions[i] * static_cast<float>(resolution)), resolution - 1);

    if (nextIndex > prevIndex) {
      auto r0 = colors[i - 1].red;
      auto g0 = colors[i - 1].green;
      auto b0 = colors[i - 1].blue;
      auto a0 = colors[i - 1].alpha;
      auto r1 = colors[i].red;
      auto g1 = colors[i].green;
      auto b1 = colors[i].blue;
      auto a1 = colors[i].alpha;

      auto step = 1.0f / static_cast<float>(nextIndex - prevIndex);
      auto deltaR = (r1 - r0) * step;
      auto deltaG = (g1 - g0) * step;
      auto deltaB = (b1 - b0) * step;
      auto deltaA = (a1 - a0) * step;

      for (int curIndex = prevIndex; curIndex <= nextIndex; ++curIndex) {
        pixels[curIndex * 4] = static_cast<uint8_t>(r0 * 255.0f);
        pixels[curIndex * 4 + 1] = static_cast<uint8_t>(g0 * 255.0f);
        pixels[curIndex * 4 + 2] = static_cast<uint8_t>(b0 * 255.0f);
        pixels[curIndex * 4 + 3] = static_cast<uint8_t>(a0 * 255.0f);
        r0 += deltaR;
        g0 += deltaG;
        b0 += deltaB;
        a0 += deltaA;
      }
    }
    prevIndex = nextIndex;
  }
  return pixelBuffer;
}

const Texture* GradientCache::getGradient(const Color* colors, const float* positions, int count) {
  BytesKey bytesKey = {};
  for (int i = 0; i < count; ++i) {
    bytesKey.write(colors[i].red);
    bytesKey.write(colors[i].green);
    bytesKey.write(colors[i].blue);
    bytesKey.write(colors[i].alpha);
    bytesKey.write(positions[i]);
  }

  const auto* texture = find(bytesKey);
  if (texture) {
    return texture;
  }
  auto pixelBuffer = CreateGradient(colors, positions, count, kGradientTextureSize);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto tex = pixelBuffer->makeTexture(context);
  if (tex) {
    add(bytesKey, tex);
  }
  return tex.get();
}

void GradientCache::releaseAll() {
  textures.clear();
  keys.clear();
}

bool GradientCache::empty() const {
  return textures.empty() && keys.empty();
}
}  // namespace tgfx
