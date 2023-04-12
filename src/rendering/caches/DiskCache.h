/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

namespace pag {
class FileInfo;

class DiskCache {
 public:
  /**
   * Returns the size limit of the disk cache in bytes.
   */
  static size_t MaxDiskSize();

  /**
   * Sets the size limit of the disk cache in bytes. The default disk cache limit is 1 GB.
   */
  static void SetMaxDiskSize(size_t size);

  /**
   * Opens a sequence file by the specified key, creates a new one if the corresponding file does
   * not exist. Returns a temporary sequence file immediately if the key is empty. The temporary
   * file will be deleted automatically when the last reference to it is released.
   */
  static std::shared_ptr<SequenceFile> OpenSequence(const std::string& key,
                                                    const tgfx::ImageInfo& info,
                                                    uint32_t frameCount, float frameRate);

 private:
  std::mutex locker = {};
  std::string configPath;
  std::string cacheFolder;
  uint32_t fileIDCount = 1;
  size_t totalDiskSize = 0;
  size_t maxDiskSize = 1073741824;  // 1 GB
  std::unordered_map<std::string, uint32_t> fileIDMap = {};
  std::list<std::shared_ptr<FileInfo>> cachedFiles = {};
  std::unordered_map<uint32_t, std::shared_ptr<FileInfo>> cachedFileMap = {};
  std::unordered_map<uint32_t, std::weak_ptr<SequenceFile>> openedFiles = {};

  static DiskCache* GetInstance();

  DiskCache();

  size_t getMaxDiskSize();
  void setMaxDiskSize(size_t size);
  std::shared_ptr<SequenceFile> openSequence(const std::string& key, const tgfx::ImageInfo& info,
                                             uint32_t frameCount, float frameRate);

  bool checkDiskSpace();
  void addToCachedFiles(std::shared_ptr<FileInfo> fileInfo);
  void removeFromCachedFiles(std::shared_ptr<FileInfo> fileInfo);
  void moveToFront(std::shared_ptr<FileInfo> fileInfo);
  void readConfig();
  void saveConfig();
  uint32_t getFileID(const std::string& key);
  std::string fileIDToPath(uint32_t fileID);
  uint32_t filePathToID(const std::string& path);
  void notifyFileClosed(uint32_t fileID);
  void notifyFileSizeChanged(uint32_t fileID, size_t fileSize);
  friend class SequenceFile;
};
}  // namespace pag
