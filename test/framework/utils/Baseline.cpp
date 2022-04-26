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
#include "base/utils/TGFXCast.h"
#include "nlohmann/json.hpp"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"

namespace pag {
using namespace tgfx;

#define BASELINE_VERSION_PATH "../test/baseline/version.json"
#define CACHE_MD5_PATH "../test/baseline/.cache/md5.json"
#define OUT_MD5_PATH "../test/out/md5.json"
#define CACHE_VERSION_PATH "../test/baseline/.cache/version.json"
#define OUT_VERSION_PATH "../test/out/version.json"
#define OUT_ROOT "../test/out/"
#define WEBP_FILE_EXT ".webp"

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

static void SaveImage(const Bitmap& bitmap, const std::string& key) {
  auto data = bitmap.encode(EncodedFormat::WEBP, 100);
  if (data == nullptr) {
    return;
  }
  std::filesystem::path path = OUT_ROOT + key + WEBP_FILE_EXT;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out.write(reinterpret_cast<const char*>(data->data()),
            static_cast<std::streamsize>(data->size()));
  out.close();
}

bool Baseline::Compare(const std::shared_ptr<PixelBuffer>& pixelBuffer, const std::string& key) {
  if (pixelBuffer == nullptr) {
    return false;
  }
  Bitmap bitmap(pixelBuffer);
  return Baseline::Compare(bitmap, key);
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

bool Baseline::Compare(const Bitmap& bitmap, const std::string& key) {
  if (bitmap.isEmpty()) {
    return false;
  }
  std::string md5;
  if (bitmap.rowBytes() == bitmap.info().minRowBytes()) {
    md5 = DumpMD5(bitmap.pixels(), bitmap.byteSize());
  } else {
    auto pixelBuffer = PixelBuffer::Make(bitmap.width(), bitmap.height(),
                                         bitmap.colorType() == tgfx::ColorType::ALPHA_8, false);
    Bitmap newBitmap(pixelBuffer);
    auto result = bitmap.readPixels(newBitmap.info(), newBitmap.writablePixels());
    if (!result) {
      return false;
    }
    md5 = DumpMD5(newBitmap.pixels(), newBitmap.byteSize());
  }
  return CompareVersionAndMd5(md5, key, [key, bitmap](bool result) {
    if (result) {
      std::filesystem::remove(OUT_ROOT + key + WEBP_FILE_EXT);
    } else {
      SaveImage(bitmap, key);
    }
  });
}

bool Baseline::Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key) {
  if (surface == nullptr) {
    return false;
  }
  auto pixelBuffer = PixelBuffer::Make(surface->width(), surface->height(), false, false);
  Bitmap bitmap(pixelBuffer);
  auto result = surface->readPixels(ToPAG(bitmap.colorType()), ToPAG(bitmap.alphaType()),
                                    bitmap.writablePixels(), bitmap.rowBytes());
  if (!result) {
    return false;
  }
  return Baseline::Compare(bitmap, key);
}

bool Baseline::Compare(const std::shared_ptr<ByteData>& byteData, const std::string& key) {
  if (!byteData || static_cast<int>(byteData->length()) == 0) {
    return false;
  }
  std::string md5 = DumpMD5(byteData->data(), byteData->length());
  return CompareVersionAndMd5(md5, key, {});
}

void Baseline::SetUp() {
  std::ifstream cacheMD5File(CACHE_MD5_PATH);
  if (cacheMD5File.is_open()) {
    cacheMD5File >> CacheMD5;
    cacheMD5File.close();
  }
  std::ifstream baselineVersionFile(BASELINE_VERSION_PATH);
  if (baselineVersionFile.is_open()) {
    baselineVersionFile >> BaselineVersion;
    baselineVersionFile.close();
  }
  std::ifstream cacheVersionFile(CACHE_VERSION_PATH);
  if (cacheVersionFile.is_open()) {
    cacheVersionFile >> CacheVersion;
    cacheVersionFile.close();
  }
  std::ifstream headFile("./HEAD");
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
  CreateFolder(CACHE_MD5_PATH);
  std::ofstream outMD5File(CACHE_MD5_PATH);
  outMD5File << std::setw(4) << OutputMD5 << std::endl;
  outMD5File.close();
  CreateFolder(CACHE_VERSION_PATH);
  std::ofstream outVersionFile(CACHE_VERSION_PATH);
  outVersionFile << std::setw(4) << BaselineVersion << std::endl;
  outVersionFile.close();
#else
  std::filesystem::remove(OUT_MD5_PATH);
  if (!OutputMD5.empty()) {
    CreateFolder(OUT_MD5_PATH);
    std::ofstream outMD5File(OUT_MD5_PATH);
    outMD5File << std::setw(4) << OutputMD5 << std::endl;
    outMD5File.close();
  }
  CreateFolder(OUT_VERSION_PATH);
  std::ofstream versionFile(OUT_VERSION_PATH);
  versionFile << std::setw(4) << OutputVersion << std::endl;
  versionFile.close();
#endif
  RemoveEmptyFolder("../test/out");
}
}  // namespace pag
