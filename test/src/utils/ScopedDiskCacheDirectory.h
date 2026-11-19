/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#pragma once

#include <cstdlib>
#include <filesystem>
#include "pag/pag.h"
#include "rendering/caches/DiskCache.h"
#include "utils/TestUtils.h"

namespace pag {

/**
 * Isolates disk cache tests in an exclusively created temporary directory and restores the
 * singleton without reading or modifying the original directory. Requires no open sequence files.
 */
class ScopedDiskCacheDirectory {
 public:
  std::string directory;
  size_t originalMaxDiskSize = 0;

  ScopedDiskCacheDirectory() {
    auto diskCache = DiskCache::GetInstance();
    DiskIOWorker::GetInstance()->waitAll();
    {
      std::lock_guard<std::mutex> lock(diskCache->locker);
      EXPECT_TRUE(diskCache->openedFiles.empty());
      if (!diskCache->openedFiles.empty()) {
        return;
      }
    }
    std::error_code error = {};
    auto root = std::filesystem::temp_directory_path(error);
    if (error) {
      return;
    }
    auto path = (root / "PAGDiskCacheTest_XXXXXX").string();
#ifdef _WIN32
    if (_mktemp_s(path.data(), path.size() + 1) != 0 ||
        !std::filesystem::create_directory(path, error) || error) {
      return;
    }
#else
    if (mkdtemp(path.data()) == nullptr) {
      return;
    }
#endif
    directory = path;
    {
      std::lock_guard<std::mutex> lock(diskCache->locker);
      originalCacheDir = diskCache->customCacheDir;
      originalDefaultCacheDir = diskCache->defaultCacheDir;
      originalHasDefaultCacheDir = diskCache->hasDefaultCacheDir;
      originalConfigPath.swap(diskCache->configPath);
      originalCacheFolder.swap(diskCache->cacheFolder);
      originalMaxDiskSize = diskCache->maxDiskSize;
      originalTotalDiskSize = diskCache->totalDiskSize;
      originalFileIDCount = diskCache->fileIDCount;
      originalConfigSaveState.swap(diskCache->configSaveState);
      originalCachedFileIDs.swap(diskCache->cachedFileIDs);
      originalCachedFileInfos.swap(diskCache->cachedFileInfos);
      originalCachedFiles.swap(diskCache->cachedFiles);
      diskCache->totalDiskSize = 0;
      diskCache->fileIDCount = DiskCache::InitialFileID;
    }
    PAGDiskCache::SetCacheDir(directory);
  }

  ScopedDiskCacheDirectory(const ScopedDiskCacheDirectory&) = delete;
  ScopedDiskCacheDirectory& operator=(const ScopedDiskCacheDirectory&) = delete;

  ~ScopedDiskCacheDirectory() {
    if (directory.empty()) {
      return;
    }
    auto diskCache = DiskCache::GetInstance();
    DiskIOWorker::GetInstance()->waitAll();
    {
      std::lock_guard<std::mutex> lock(diskCache->locker);
      EXPECT_TRUE(diskCache->openedFiles.empty());
      if (!diskCache->openedFiles.empty()) {
        return;
      }
      originalCachedFileIDs.swap(diskCache->cachedFileIDs);
      originalCachedFileInfos.swap(diskCache->cachedFileInfos);
      originalCachedFiles.swap(diskCache->cachedFiles);
      diskCache->customCacheDir = originalCacheDir;
      diskCache->defaultCacheDir = originalDefaultCacheDir;
      diskCache->hasDefaultCacheDir = originalHasDefaultCacheDir;
      diskCache->configPath = originalConfigPath;
      diskCache->cacheFolder = originalCacheFolder;
      diskCache->maxDiskSize = originalMaxDiskSize;
      diskCache->totalDiskSize = originalTotalDiskSize;
      diskCache->fileIDCount = originalFileIDCount;
      diskCache->configSaveState = originalConfigSaveState;
    }
    std::error_code error = {};
    std::filesystem::remove_all(directory, error);
    EXPECT_FALSE(error);
  }

 private:
  std::string originalCacheDir;
  std::string originalDefaultCacheDir;
  bool originalHasDefaultCacheDir = false;
  std::string originalConfigPath;
  std::string originalCacheFolder;
  size_t originalTotalDiskSize = 0;
  uint32_t originalFileIDCount = 0;
  std::shared_ptr<ConfigSaveState> originalConfigSaveState;
  decltype(DiskCache::cachedFileIDs) originalCachedFileIDs;
  decltype(DiskCache::cachedFileInfos) originalCachedFileInfos;
  decltype(DiskCache::cachedFiles) originalCachedFiles;
};

}  // namespace pag
