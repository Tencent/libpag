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
#include <CommonCrypto/CommonCrypto.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include "PAGTestUtils.h"
#include "TestConstants.h"
#include "base/utils/TGFXCast.h"
#include "nlohmann/json.hpp"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/opengl/GLDevice.h"

namespace pag {
using namespace tgfx;

static nlohmann::json BaselineVersion = {};
static nlohmann::json CacheVersion = {};
static nlohmann::json OutputVersion = {};
static nlohmann::json CacheMD5 = {};
static nlohmann::json OutputMD5 = {};
static std::mutex jsonLocker = {};
static std::string currentVersion;

std::string DumpMD5(const void* bytes, size_t size) {
  unsigned char digest[CC_MD5_DIGEST_LENGTH] = {0};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  CC_MD5(bytes, static_cast<int>(size), digest);
#pragma clang diagnostic pop
  char buffer[33];
  char* position = buffer;
  for (unsigned char i : digest) {
    snprintf(position, 3, "%02x", i);
    position += 2;
  }
  return {buffer, 32};
}

static nlohmann::json* FindJSON(nlohmann::json& md5JSON, const std::string& key,
                                std::string* lastKey) {
  std::vector<std::string> keys = {};
  size_t start;
  size_t end = 0;
  while ((start = key.find_first_not_of('/', end)) != std::string::npos) {
    end = key.find('/', start);
    keys.push_back(key.substr(start, end - start));
  }
  *lastKey = keys.back();
  keys.pop_back();
  auto json = &md5JSON;
  for (auto& jsonKey : keys) {
    if ((*json)[jsonKey] == nullptr) {
      (*json)[jsonKey] = {};
    }
    json = &(*json)[jsonKey];
  }
  return json;
}

static std::string GetJSONValue(nlohmann::json& target, const std::string& key) {
  std::lock_guard<std::mutex> autoLock(jsonLocker);
  std::string jsonKey;
  auto json = FindJSON(target, key, &jsonKey);
  auto value = (*json)[jsonKey];
  return value != nullptr ? value.get<std::string>() : "";
}

static void SetJSONValue(nlohmann::json& target, const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> autoLock(jsonLocker);
  std::string jsonKey;
  auto json = FindJSON(target, key, &jsonKey);
  (*json)[jsonKey] = value;
}

bool Baseline::Compare(std::shared_ptr<tgfx::PixelBuffer> pixelBuffer, const std::string& key) {
  if (pixelBuffer == nullptr) {
    return false;
  }
  auto pixels = pixelBuffer->lockPixels();
  Pixmap pixmap(pixelBuffer->info(), pixels);
  auto result = Baseline::Compare(pixmap, key);
  pixelBuffer->unlockPixels();
  return result;
}

bool Baseline::Compare(const std::shared_ptr<tgfx::Surface> surface, const std::string& key) {
  if (surface == nullptr) {
    return false;
  }
  Bitmap bitmap(surface->width(), surface->height(), false, false);
  Pixmap pixmap(bitmap);
  auto result = surface->readPixels(pixmap.info(), pixmap.writablePixels());
  if (!result) {
    return false;
  }
  return Baseline::Compare(pixmap, key);
}

bool Baseline::Compare(const Bitmap& bitmap, const std::string& key) {
  if (bitmap.isEmpty()) {
    return false;
  }
  Pixmap pixmap(bitmap);
  return Baseline::Compare(pixmap, key);
}

static bool CompareVersionAndMd5(const std::string& md5, const std::string& key,
                                 const std::function<void(bool)>& callback) {
#ifdef UPDATE_BASELINE
  SetJSONValue(OutputMD5, key, md5);
  return true;
#endif
  auto baselineVersion = GetJSONValue(BaselineVersion, key);
  auto cacheVersion = GetJSONValue(CacheVersion, key);
  if (baselineVersion.empty() ||
      (baselineVersion == cacheVersion && GetJSONValue(CacheMD5, key) != md5)) {
    SetJSONValue(OutputVersion, key, currentVersion);
    SetJSONValue(OutputMD5, key, md5);
    if (callback) {
      callback(false);
    }
    return false;
  }
  SetJSONValue(OutputVersion, key, baselineVersion);
  if (callback) {
    callback(true);
  }
  return true;
}

bool Baseline::Compare(const Pixmap& pixmap, const std::string& key) {
  if (pixmap.isEmpty()) {
    return false;
  }
  std::string md5;
  if (pixmap.rowBytes() == pixmap.info().minRowBytes()) {
    md5 = DumpMD5(pixmap.pixels(), pixmap.byteSize());
  } else {
    Bitmap newBitmap(pixmap.width(), pixmap.height(), pixmap.isAlphaOnly(), false);
    Pixmap newPixmap(newBitmap);
    auto result = pixmap.readPixels(newPixmap.info(), newPixmap.writablePixels());
    if (!result) {
      return false;
    }
    md5 = DumpMD5(newPixmap.pixels(), newPixmap.byteSize());
  }
  return CompareVersionAndMd5(md5, key, [key, pixmap](bool result) {
    if (result) {
      RemoveImage(key);
    } else {
      SaveImage(pixmap, key);
    }
#ifdef GENERATOR_BASELINE_IMAGES
    SaveImage(pixmap, key + "_base");
#endif
  });
}

bool Baseline::Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key) {
  if (surface == nullptr) {
    return false;
  }
  Bitmap bitmap(surface->width(), surface->height(), false, false);
  Pixmap pixmap(bitmap);
  auto result = surface->readPixels(ToPAG(pixmap.colorType()), ToPAG(pixmap.alphaType()),
                                    pixmap.writablePixels(), pixmap.rowBytes());
  if (!result) {
    return false;
  }
  return Baseline::Compare(pixmap, key);
}

bool Baseline::Compare(const std::shared_ptr<ByteData>& byteData, const std::string& key) {
  if (!byteData || static_cast<int>(byteData->length()) == 0) {
    return false;
  }
  std::string md5 = DumpMD5(byteData->data(), byteData->length());
  return CompareVersionAndMd5(md5, key, {});
}

void Baseline::SetUp() {
  std::ifstream cacheMD5File(TestConstants::CACHE_MD5_PATH);
  if (cacheMD5File.is_open()) {
    cacheMD5File >> CacheMD5;
    cacheMD5File.close();
  }
  std::ifstream baselineVersionFile(TestConstants::BASELINE_VERSION_PATH);
  if (baselineVersionFile.is_open()) {
    baselineVersionFile >> BaselineVersion;
    baselineVersionFile.close();
  }
  std::ifstream cacheVersionFile(TestConstants::CACHE_VERSION_PATH);
  if (cacheVersionFile.is_open()) {
    cacheVersionFile >> CacheVersion;
    cacheVersionFile.close();
  }
  std::ifstream headFile(TestConstants::HEAD_PATH);
  if (headFile.is_open()) {
    headFile >> currentVersion;
    headFile.close();
  }
}

static void RemoveEmptyFolder(const std::filesystem::path& path) {
  if (!std::filesystem::is_directory(path)) {
    if (path.filename() == ".DS_Store") {
      std::filesystem::remove(path);
    }
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    RemoveEmptyFolder(entry.path());
  }
  if (std::filesystem::is_empty(path)) {
    std::filesystem::remove(path);
  }
}

static void CreateFolder(const std::string& path) {
  std::filesystem::path filePath = path;
  std::filesystem::create_directories(filePath.parent_path());
}

void Baseline::TearDown() {
#ifdef UPDATE_BASELINE
  CreateFolder(TestConstants::CACHE_MD5_PATH);
  std::ofstream outMD5File(TestConstants::CACHE_MD5_PATH);
  outMD5File << std::setw(4) << OutputMD5 << std::endl;
  outMD5File.close();
  CreateFolder(TestConstants::CACHE_VERSION_PATH);
  std::ofstream outVersionFile(TestConstants::CACHE_VERSION_PATH);
  outVersionFile << std::setw(4) << BaselineVersion << std::endl;
  outVersionFile.close();
#else
  std::filesystem::remove(TestConstants::OUT_MD5_PATH);
  if (!OutputMD5.empty()) {
    CreateFolder(TestConstants::OUT_MD5_PATH);
    std::ofstream outMD5File(TestConstants::OUT_MD5_PATH);
    outMD5File << std::setw(4) << OutputMD5 << std::endl;
    outMD5File.close();
  }
  CreateFolder(TestConstants::OUT_VERSION_PATH);
  std::ofstream versionFile(TestConstants::OUT_VERSION_PATH);
  versionFile << std::setw(4) << OutputVersion << std::endl;
  versionFile.close();
#endif
  RemoveEmptyFolder(TestConstants::OUT_ROOT);
}
}  // namespace pag
