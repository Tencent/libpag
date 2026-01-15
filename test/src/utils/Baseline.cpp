/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "base/utils/USE.h"
#include "nlohmann/json.hpp"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Surface.h"
#include "utils/TestDir.h"
#include "utils/TestUtils.h"

namespace pag {
using namespace tgfx;

static std::unique_ptr<Baseline> baseline = nullptr;
static const std::string BASELINE_ROOT = ProjectPath::Absolute("test/baseline");
static const std::string CACHE_ROOT = TestDir::GetRoot() + "/baseline/.cache";
static const std::string EXISTING_OUT_ROOT = TestDir::GetRoot() + "/out";
#ifdef GENERATE_BASELINE_IMAGES
static const std::string OUT_ROOT = TestDir::GetRoot() + "/baseline-out";
#else
static const std::string OUT_ROOT = TestDir::GetRoot() + "/out";
#endif
static const std::string WEBP_FILE_EXT = ".webp";

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

void Baseline::SetUp() {
  baseline = std::make_unique<Baseline>(BASELINE_ROOT, CACHE_ROOT, OUT_ROOT);
}

void Baseline::TearDown() {
  baseline->saveData();
  baseline = nullptr;
}

bool Baseline::Compare(std::shared_ptr<tgfx::Surface> surface, const std::string& key) {
  return baseline->compare(surface, key);
}

bool Baseline::Compare(const Bitmap& bitmap, const std::string& key) {
  return baseline->compare(bitmap, key);
}

bool Baseline::Compare(const Pixmap& pixmap, const std::string& key) {
  return baseline->compare(pixmap, key);
}

bool Baseline::Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key) {
  return baseline->compare(surface, key);
}

bool Baseline::Compare(const std::shared_ptr<ByteData>& byteData, const std::string& key) {
  return baseline->compare(byteData, key);
}

bool Baseline::compare(std::shared_ptr<tgfx::Surface> surface, const std::string& key) {
  if (surface == nullptr) {
    return false;
  }
  Bitmap bitmap(surface->width(), surface->height(), false, false);
  Pixmap pixmap(bitmap);
  auto result = surface->readPixels(pixmap.info(), pixmap.writablePixels());
  if (!result) {
    return false;
  }
  return compare(pixmap, key);
}

bool Baseline::compare(const Bitmap& bitmap, const std::string& key) {
  if (bitmap.isEmpty()) {
    return false;
  }
  Pixmap pixmap(bitmap);
  return compare(pixmap, key);
}

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

#ifdef GENERATE_BASELINE_IMAGES
bool Baseline::tryCopyExistingBaseline(const std::string& key, const std::string& md5) {
  auto existingPath = EXISTING_OUT_ROOT + "/" + key + "_base" + WEBP_FILE_EXT;
  if (!std::filesystem::exists(existingPath)) {
    return false;
  }
  auto baselineVersion = getJSONValue(baselineVersions, key);
  auto cacheVersion = getJSONValue(cacheVersions, key);
  if (baselineVersion.empty() || baselineVersion != cacheVersion) {
    return false;
  }
  if (getJSONValue(cacheMD5, key) != md5) {
    return false;
  }
  auto destPath = OUT_ROOT + "/" + key + "_base" + WEBP_FILE_EXT;
  std::filesystem::create_directories(std::filesystem::path(destPath).parent_path());
  std::filesystem::copy(existingPath, destPath, std::filesystem::copy_options::overwrite_existing);
  return true;
}
#endif

bool Baseline::compare(const Pixmap& pixmap, const std::string& key) {
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
  auto result = compareMd5(key, md5);
  if (result) {
    RemoveImage(key);
  } else {
    SaveImage(pixmap, key);
  }
#ifdef GENERATE_BASELINE_IMAGES
  if (!tryCopyExistingBaseline(key, md5)) {
    SaveImage(pixmap, key + "_base");
  }
#endif
  return result;
}

bool Baseline::compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key) {
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
  return compare(pixmap, key);
}

bool Baseline::compare(const std::shared_ptr<ByteData>& byteData, const std::string& key) {
  if (!byteData || static_cast<int>(byteData->length()) == 0) {
    return false;
  }
  std::string md5 = DumpMD5(byteData->data(), byteData->length());
  return compareMd5(key, md5);
}

static void CreateFolder(const std::string& path) {
  std::filesystem::path filePath = path;
  std::filesystem::create_directories(filePath.parent_path());
}

Baseline::Baseline(const std::string& baselinePath, const std::string& cachePath,
                   const std::string& outputPath, const std::string& prefix) {
  baselineVersionPath = baselinePath + "/" + prefix + "version.json";
  cacheVersionPath = cachePath + "/" + prefix + "version.json";
  cacheMD5Path = cachePath + "/" + prefix + "md5.json";
  outVersionPath = outputPath + "/" + prefix + "version.json";
  outMD5Path = outputPath + "/" + prefix + "md5.json";

  std::ifstream baselineVersionFile(baselineVersionPath);
  if (baselineVersionFile.is_open()) {
    baselineVersionFile >> baselineVersions;
    baselineVersionFile.close();
  }
  std::ifstream cacheVersionFile(cacheVersionPath);
  if (cacheVersionFile.is_open()) {
    cacheVersionFile >> cacheVersions;
    cacheVersionFile.close();
  }

  std::ifstream cacheMD5File(cacheMD5Path);
  if (cacheMD5File.is_open()) {
    cacheMD5File >> cacheMD5;
    cacheMD5File.close();
  }

  std::ifstream headFile(cachePath + "/HEAD");
  if (headFile.is_open()) {
    headFile >> currentVersion;
    headFile.close();
  }
}

void Baseline::saveData() {
#ifdef UPDATE_BASELINE
  if (!PAGTest::HasFailure()) {
#ifdef GENERATE_BASELINE_IMAGES
    auto outPath = TestDir::GetRoot() + "/out";
    std::filesystem::remove_all(outPath);
    if (std::filesystem::exists(OUT_ROOT)) {
      std::filesystem::rename(OUT_ROOT, outPath);
    }
#endif
    CreateFolder(cacheMD5Path);
    std::ofstream outMD5File(cacheMD5Path);
    outMD5File << std::setw(4) << outputMD5 << std::endl;
    outMD5File.close();
    CreateFolder(cacheVersionPath);
    std::filesystem::copy(baselineVersionPath, cacheVersionPath,
                          std::filesystem::copy_options::overwrite_existing);
  } else {
    std::filesystem::remove_all(OUT_ROOT);
  }
  USE(RemoveEmptyFolder);
#else
  std::filesystem::remove(outMD5Path);
  if (!outputMD5.empty()) {
    CreateFolder(outMD5Path);
    std::ofstream outMD5File(outMD5Path);
    outMD5File << std::setw(4) << outputMD5 << std::endl;
    outMD5File.close();
  }
  CreateFolder(outVersionPath);
  std::ofstream versionFile(outVersionPath);
  versionFile << std::setw(4) << outputVersions << std::endl;
  versionFile.close();
  RemoveEmptyFolder(OUT_ROOT);
#endif
}

bool Baseline::compareMd5(const std::string& key, const std::string& md5) {
#ifdef UPDATE_BASELINE
  setJSONValue(outputMD5, key, md5);
  return true;
#endif
  auto baselineVersion = getJSONValue(baselineVersions, key);
  auto cacheVersion = getJSONValue(cacheVersions, key);
  if (baselineVersion.empty() ||
      (baselineVersion == cacheVersion && getJSONValue(cacheMD5, key) != md5)) {
    setJSONValue(outputVersions, key, currentVersion);
    setJSONValue(outputMD5, key, md5);
    return false;
  }
  setJSONValue(outputVersions, key, baselineVersion);
  return true;
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

std::string Baseline::getJSONValue(nlohmann::json& target, const std::string& key) {
  std::lock_guard<std::mutex> autoLock(locker);
  std::string jsonKey;
  auto json = FindJSON(target, key, &jsonKey);
  auto value = (*json)[jsonKey];
  return value != nullptr ? value.get<std::string>() : "";
}

void Baseline::setJSONValue(nlohmann::json& target, const std::string& key,
                            const std::string& value) {
  std::lock_guard<std::mutex> autoLock(locker);
  std::string jsonKey;
  auto json = FindJSON(target, key, &jsonKey);
  (*json)[jsonKey] = value;
}
}  // namespace pag
