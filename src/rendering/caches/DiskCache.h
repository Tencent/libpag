/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <list>
#include <unordered_map>
#include "SequenceFile.h"
#include "pag/types.h"

namespace pag {
class FileInfo;

class DiskCache {
 public:
  /**
   * Opens a sequence file by the specified key, creates a new one if the corresponding file does
   * not exist. Returns a temporary sequence file immediately if the key is empty. The temporary
   * file will be deleted automatically when the last reference to it is released.
   */
  static std::shared_ptr<SequenceFile> OpenSequence(
      const std::string& key, const tgfx::ImageInfo& info, int frameCount, float frameRate,
      const std::vector<TimeRange>& staticTimeRanges = {});

  /**
   * Reads a file from the disk cache by the specified key. Returns nullptr if the key is empty or
   * the cache does not exist.
   */
  static std::shared_ptr<tgfx::Data> ReadFile(const std::string& key);

  /**
   * Writes a file to the disk cache by the specified key. Returns false if the key is empty, the
   * data is nullptr, or the cache cannot be written.
   */
  static bool WriteFile(const std::string& key, std::shared_ptr<tgfx::Data> data);

 private:
  std::mutex locker = {};
  std::string customCacheDir;
  std::string configPath;
  std::string cacheFolder;
  uint32_t fileIDCount = 1;
  size_t totalDiskSize = 0;
  size_t maxDiskSize = 1073741824;  // 1 GB
  std::unordered_map<std::string, uint32_t> cachedFileIDs = {};
  std::unordered_map<uint32_t, std::shared_ptr<FileInfo>> cachedFileInfos = {};
  std::list<std::shared_ptr<FileInfo>> cachedFiles = {};
  std::unordered_map<uint32_t, std::weak_ptr<SequenceFile>> openedFiles = {};

  static DiskCache* GetInstance();

  std::string getCacheDir();
  void setCacheDir(const std::string& dir);

  DiskCache();
  size_t getMaxDiskSize();
  void setMaxDiskSize(size_t size);
  void removeAll();
  std::shared_ptr<SequenceFile> openSequence(const std::string& key, const tgfx::ImageInfo& info,
                                             int frameCount, float frameRate,
                                             const std::vector<TimeRange>& staticTimeRanges);
  std::shared_ptr<tgfx::Data> readFile(const std::string& key);
  bool writeFile(const std::string& key, std::shared_ptr<tgfx::Data> data);

  bool checkDiskSpace(size_t maxSize);
  void addToCachedFiles(std::shared_ptr<FileInfo> fileInfo);
  void removeFromCachedFiles(std::shared_ptr<FileInfo> fileInfo);
  void moveToFront(std::shared_ptr<FileInfo> fileInfo);
  void moveToBeforeOpenedFiles(std::shared_ptr<FileInfo> fileInfo);
  bool readConfig();
  void saveConfig();
  uint32_t getFileID(const std::string& key);
  void changeToTemporary(uint32_t fileID);
  std::string fileIDToPath(uint32_t fileID);
  uint32_t filePathToID(const std::string& path);
  void notifyFileClosed(uint32_t fileID);
  void notifyFileSizeChanged(uint32_t fileID, size_t fileSize);

  friend class SequenceFile;
  friend class PAGDiskCache;
};
}  // namespace pag
