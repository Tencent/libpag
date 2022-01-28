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

#include "Baseline.h"
#include <chrono>
#include <fstream>
#include <unordered_set>
#include "LzmaUtil.h"
#include "core/Data.h"
#include "image/Image.h"
#include "image/PixelMap.h"

namespace pag {
#define BASELINE_ROOT "../test/baseline/"
#define OUT_BASELINE_ROOT "../test/out/baseline/"
#define OUT_COMPARE_ROOT "../test/out/compare/"
#define COMPRESS_FILE_EXT ".lzma2"

static std::unordered_set<std::string> clearedFolders = {};

static void ClearPreviousOutput(const std::string& key) {
  std::filesystem::path folder = key;
  while (!folder.parent_path().empty()) {
    folder = folder.parent_path();
  }
  if (clearedFolders.count(folder) > 0) {
    return;
  }
  clearedFolders.insert(folder);
  std::filesystem::remove_all(OUT_BASELINE_ROOT + folder.string());
  std::filesystem::remove_all(OUT_COMPARE_ROOT + folder.string());
}

static ImageInfo MakeInfo(int with, int height) {
  return ImageInfo::Make(with, height, ColorType::RGBA_8888, AlphaType::Premultiplied);
}

static std::shared_ptr<Data> LoadImageData(const std::string& key) {
  auto data = Data::MakeFromFile(BASELINE_ROOT + key + COMPRESS_FILE_EXT);
  if (data == nullptr) {
    return nullptr;
  }
  return LzmaUtil::Decompress(data);
}

static void SaveData(const std::shared_ptr<Data>& data, const std::string& path) {
  std::filesystem::path filePath = path;
  std::filesystem::create_directories(filePath.parent_path());
  std::ofstream out(path);
  out.write(reinterpret_cast<const char*>(data->data()),
            static_cast<std::streamsize>(data->size()));
  out.close();
}

static void SaveImage(const ImageInfo& info, const std::shared_ptr<Data>& imageData,
                      const std::string& key) {
  auto data = LzmaUtil::Compress(imageData);
  if (data == nullptr) {
    return;
  }
  auto path = OUT_BASELINE_ROOT + key + COMPRESS_FILE_EXT;
  SaveData(data, path);
  auto baselineData = LoadImageData(key);
  if (baselineData == nullptr) {
    return;
  }
  auto baselineImage = Image::Encode(info, baselineData->data(), EncodedFormat::WEBP, 100);
  SaveData(baselineImage, OUT_COMPARE_ROOT + key + "_baseline.webp");
  auto compareImage = Image::Encode(info, imageData->data(), EncodedFormat::WEBP, 100);
  SaveData(compareImage, OUT_COMPARE_ROOT + key + "_new.webp");
}

bool ComparePixelData(const std::shared_ptr<Data>& pixelData, const std::string& key) {
  ClearPreviousOutput(key);
  if (pixelData == nullptr) {
    return false;
  }
  auto baselineData = LoadImageData(key);
  if (baselineData == nullptr || pixelData->size() != baselineData->size()) {
    return false;
  }
  auto baseline = baselineData->bytes();
  auto pixels = pixelData->bytes();
  auto byteSize = pixelData->size();
  for (size_t index = 0; index < byteSize; index++) {
    auto pixelA = pixels[index];
    auto pixelB = baseline[index];
    if (abs(pixelA - pixelB) > 3) {
      return false;
    }
  }
  return true;
}

bool Baseline::Compare(const std::shared_ptr<PixelBuffer>& pixelBuffer, const std::string& key) {
  if (pixelBuffer == nullptr) {
    return false;
  }
  auto srcPixels = pixelBuffer->lockPixels();
  PixelMap pixelMap(pixelBuffer->info(), srcPixels);
  auto info = MakeInfo(pixelBuffer->width(), pixelBuffer->height());
  auto pixels = new uint8_t[info.byteSize()];
  auto data = Data::MakeAdopted(pixels, info.byteSize(), Data::DeleteProc);
  auto result = pixelMap.readPixels(info, pixels);
  pixelBuffer->unlockPixels();
  if (!result) {
    return false;
  }
  result = ComparePixelData(data, key);
  if (!result) {
    SaveImage(info, data, key);
  }
  return result;
}

bool Baseline::Compare(const Bitmap& bitmap, const std::string& key) {
  if (bitmap.isEmpty()) {
    return false;
  }
  auto info = MakeInfo(bitmap.width(), bitmap.height());
  auto pixels = new uint8_t[info.byteSize()];
  auto data = Data::MakeAdopted(pixels, info.byteSize(), Data::DeleteProc);
  auto result = bitmap.readPixels(info, pixels);
  if (!result) {
    return false;
  }
  result = ComparePixelData(data, key);
  if (!result) {
    SaveImage(info, data, key);
  }
  return result;
}

bool Baseline::Compare(const PixelMap& pixelMap, const std::string& key) {
  if (pixelMap.isEmpty()) {
    return false;
  }
  auto info = MakeInfo(pixelMap.width(), pixelMap.height());
  auto pixels = new uint8_t[info.byteSize()];
  auto data = Data::MakeAdopted(pixels, info.byteSize(), Data::DeleteProc);
  auto result = pixelMap.readPixels(info, pixels);
  if (!result) {
    return false;
  }
  result = ComparePixelData(data, key);
  if (!result) {
    SaveImage(info, data, key);
  }
  return result;
}

bool Baseline::Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key) {
  if (surface == nullptr) {
    return false;
  }
  auto info = MakeInfo(surface->width(), surface->height());
  auto pixels = new uint8_t[info.byteSize()];
  auto data = Data::MakeAdopted(pixels, info.byteSize(), Data::DeleteProc);
  auto result =
      surface->readPixels(info.colorType(), AlphaType::Premultiplied, pixels, info.rowBytes());
  if (!result) {
    return false;
  }
  result = ComparePixelData(data, key);
  if (!result) {
    SaveImage(info, data, key);
  }
  return result;
}
}  // namespace pag
