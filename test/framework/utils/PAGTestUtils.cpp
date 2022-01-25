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

#include "PAGTestUtils.h"
#include <CommonCrypto/CommonCrypto.h>
#include <fstream>
#include "image/Image.h"

namespace pag {
std::string DumpMD5(const void* bytes, size_t size) {
  unsigned char digest[CC_MD5_DIGEST_LENGTH] = {0};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CC_MD5(bytes, size, digest);
#pragma clang diagnostic pop
  char buffer[32];
  char* position = buffer;
  for (unsigned char i : digest) {
    sprintf(position, "%02x", i);
    position += 2;
  }
  return {buffer, 32};
}

std::string DumpMD5(const Bitmap& bitmap) {
  if (bitmap.isEmpty()) {
    return "";
  }
  BitmapLock lock(bitmap);
  return DumpMD5(lock.pixels(), bitmap.byteSize());
}

std::string DumpMD5(std::shared_ptr<PixelBuffer> pixelBuffer) {
  if (pixelBuffer == nullptr) {
    return "";
  }
  auto pixels = pixelBuffer->lockPixels();
  auto result = DumpMD5(pixels, pixelBuffer->byteSize());
  pixelBuffer->unlockPixels();
  return result;
}

std::string DumpMD5(const PixelMap& pixelMap) {
  if (pixelMap.isEmpty()) {
    return "";
  }
  return DumpMD5(pixelMap.pixels(), pixelMap.byteSize());
}

Bitmap MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface) {
  Bitmap bitmap = {};
  if (!bitmap.allocPixels(pagSurface->width(), pagSurface->height())) {
    return bitmap;
  }
  BitmapLock lock(bitmap);
  auto result = pagSurface->readPixels(bitmap.colorType(), bitmap.alphaType(), lock.pixels(),
                                       bitmap.rowBytes());
  if (!result) {
    bitmap.reset();
  }
  return bitmap;
}

std::string DumpMD5(std::shared_ptr<PAGSurface> pagSurface) {
  auto image = MakeSnapshot(pagSurface);
  return DumpMD5(image);
}

static void TraceImage(const PixelMap& pixelMap, const std::string& tag) {
  std::string path = tag;
  if (path.empty()) {
    path = "TraceImage.png";
  } else if (path.rfind(".png") != path.size() - 4 && path.rfind(".PNG") != path.size() - 4) {
    path += ".png";
  }
  auto bytes = Image::Encode(pixelMap.info(), pixelMap.pixels(), EncodedFormat::PNG, 100);
  if (bytes) {
    std::ofstream out(path);
    out.write(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    out.close();
  }
}

void TraceIf(const Bitmap& bitmap, const std::string& path, bool condition) {
  BitmapLock lock(bitmap);
  PixelMap pixelMap(bitmap.info(), lock.pixels());
  TraceIf(pixelMap, path, condition);
}

void TraceIf(std::shared_ptr<PixelBuffer> pixelBuffer, const std::string& path, bool condition) {
  auto pixels = pixelBuffer->lockPixels();
  PixelMap pixelMap(pixelBuffer->info(), pixels);
  TraceIf(pixelMap, path, condition);
  pixelBuffer->unlockPixels();
}

void TraceIf(const PixelMap& pixelMap, const std::string& path, bool condition) {
//      condition = true;  // 忽略条件输出所有图片。
  if (condition) {
    TraceImage(pixelMap, path);
  }
}

void TraceIf(std::shared_ptr<PAGSurface> pagSurface, const std::string& path, bool condition) {
  auto image = MakeSnapshot(pagSurface);
  TraceIf(image, path, condition);
}

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex) {
  if (root == nullptr) return nullptr;
  if (type == LayerType::PreCompose) {
    if (targetIndex == 0) {
      return root;
    }
    targetIndex--;
  }
  for (int i = 0; i < root->numChildren(); i++) {
    auto layer = root->getLayerAt(i);
    if (layer->layerType() == type) {
      if (targetIndex == 0) {
        return layer;
      }
      targetIndex--;
    } else if (layer->layerType() == LayerType::PreCompose) {
      return GetLayer(std::static_pointer_cast<PAGComposition>(layer), type, targetIndex);
    }
  }
  return nullptr;
}
}  // namespace pag