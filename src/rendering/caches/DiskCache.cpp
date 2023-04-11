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

#include "DiskCache.h"
#include "platform/Platform.h"
#include "rendering/utils/Directory.h"
#include "tgfx/utils/Buffer.h"
#include "tgfx/utils/DataView.h"

namespace pag {
class FileInfo {
 public:
  FileInfo(std::string cacheKey, uint32_t fileID, size_t fileSize = 0)
      : cacheKey(cacheKey), fileID(fileID), fileSize(fileSize) {
  }

  std::string cacheKey;
  uint32_t fileID = 0;
  size_t fileSize = 0;
  std::list<std::shared_ptr<FileInfo>>::iterator cachedPosition;
};

DiskCache* DiskCache::GetInstance() {
  static auto& diskCache = *new DiskCache();
  return &diskCache;
}

size_t DiskCache::MaxDiskSize() {
  return GetInstance()->getMaxDiskSize();
}

void DiskCache::SetMaxDiskSize(size_t size) {
  GetInstance()->setMaxDiskSize(size);
}

std::shared_ptr<SequenceFile> DiskCache::OpenSequence(const std::string& key, uint32_t width,
                                                      uint32_t height, uint32_t frameCount,
                                                      float frameRate) {
  return GetInstance()->openSequence(key, width, height, frameCount, frameRate);
}

DiskCache::DiskCache() {
  auto cacheDir = Platform::Current()->getCacheDir();
  if (!cacheDir.empty()) {
    configPath = Directory::JoinPath(cacheDir, "cache.cfg");
    cacheFolder = Directory::JoinPath(cacheDir, "caches");
    Directory::CreateRecursively(cacheFolder);
    readConfig();
  }
}

size_t DiskCache::getMaxDiskSize() {
  std::lock_guard<std::mutex> autoLock(locker);
  return maxDiskSize;
}

void DiskCache::setMaxDiskSize(size_t size) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (maxDiskSize == size) {
    return;
  }
  maxDiskSize = size;
  if (checkDiskSpace()) {
    saveConfig();
  }
}

std::shared_ptr<SequenceFile> DiskCache::openSequence(const std::string& key, uint32_t width,
                                                      uint32_t height, uint32_t frameCount,
                                                      float frameRate) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (cacheFolder.empty()) {
    return nullptr;
  }
  auto fileID = getFileID(key);
  auto result = openedFiles.find(fileID);
  if (result != openedFiles.end()) {
    auto sequenceFile = result->second.lock();
    if (sequenceFile != nullptr) {
      moveToFront(cachedFileMap[fileID]);
      return sequenceFile;
    }
    openedFiles.erase(fileID);
  }
  auto filePath = fileIDToPath(fileID);
  auto sequenceFile = SequenceFile::Open(filePath, width, height, frameCount, frameRate);
  if (sequenceFile == nullptr) {
    return nullptr;
  }
  sequenceFile->diskCache = this;
  sequenceFile->fileID = fileID;
  openedFiles[fileID] = sequenceFile;
  if (!key.empty()) {
    fseek(sequenceFile->file, 0, SEEK_END);
    auto fileSize = static_cast<size_t>(ftell(sequenceFile->file));
    totalDiskSize += fileSize;
    auto oldFileInfo = cachedFileMap[fileID];
    if (oldFileInfo) {
      totalDiskSize -= oldFileInfo->fileSize;
      oldFileInfo->fileSize = fileSize;
      moveToFront(oldFileInfo);
    } else {
      addToCachedFiles(std::make_shared<FileInfo>(key, fileID, fileSize));
    }
    if (checkDiskSpace() || oldFileInfo == nullptr) {
      saveConfig();
    };
  }
  return sequenceFile;
}

bool DiskCache::checkDiskSpace() {
  bool changed = false;
  if (totalDiskSize <= maxDiskSize) {
    return changed;
  }
  while (totalDiskSize > maxDiskSize) {
    auto fileInfo = cachedFiles.back();
    if (openedFiles.count(fileInfo->fileID) > 0) {
      break;
    }
    auto filePath = fileIDToPath(fileInfo->fileID);
    remove(filePath.c_str());
    totalDiskSize -= fileInfo->fileSize;
    removeFromCachedFiles(fileInfo);
    changed = true;
  }
  return changed;
}

void DiskCache::addToCachedFiles(std::shared_ptr<FileInfo> fileInfo) {
  cachedFiles.push_front(fileInfo);
  fileInfo->cachedPosition = cachedFiles.begin();
  cachedFileMap[fileInfo->fileID] = fileInfo;
}

void DiskCache::removeFromCachedFiles(std::shared_ptr<FileInfo> fileInfo) {
  cachedFiles.erase(fileInfo->cachedPosition);
  cachedFileMap.erase(fileInfo->fileID);
}

void DiskCache::moveToFront(std::shared_ptr<FileInfo> fileInfo) {
  cachedFiles.erase(fileInfo->cachedPosition);
  cachedFiles.push_front(fileInfo);
  fileInfo->cachedPosition = cachedFiles.begin();
}

void DiskCache::readConfig() {
  auto file = fopen(configPath.c_str(), "rb");
  if (file == nullptr) {
    return;
  }
  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  if (size == 0) {
    fclose(file);
    return;
  }
  fseek(file, 0, SEEK_SET);
  tgfx::Buffer buffer(size);
  auto length = fread(buffer.data(), 1, size, file);
  fclose(file);
  tgfx::DataView dataView(buffer.bytes(), length);
  size_t pos = 0;
  while (pos + 8 < dataView.size()) {
    auto fileID = dataView.getUint32(pos);
    auto keyLength = dataView.getUint32(pos + 4);
    pos += 8;
    if (pos + keyLength > dataView.size()) {
      break;
    }
    auto cacheKey = std::string(reinterpret_cast<const char*>(dataView.bytes()) + pos, keyLength);
    pos += keyLength;
    fileIDCount = std::max(fileIDCount, fileID + 1);
    addToCachedFiles(std::make_shared<FileInfo>(cacheKey, fileID, 0));
    fileIDMap[cacheKey] = fileID;
  }
  Directory::VisitFiles(cacheFolder, [&](const std::string& path, size_t fileSize) {
    auto fileID = filePathToID(path);
    auto result = cachedFileMap.find(fileID);
    if (result == cachedFileMap.end()) {
      remove(path.c_str());
    } else {
      result->second->fileSize = fileSize;
    }
  });
  std::vector<std::shared_ptr<FileInfo>> expiredFiles = {};
  for (auto& item : cachedFiles) {
    if (item->fileSize > 0) {
      totalDiskSize += item->fileSize;
    } else {
      expiredFiles.push_back(item);
    }
  }
  for (auto& item : expiredFiles) {
    removeFromCachedFiles(item);
  }
  if (checkDiskSpace() || !expiredFiles.empty()) {
    saveConfig();
  }
}

void DiskCache::saveConfig() {
  auto file = fopen(configPath.c_str(), "wb");
  if (file == nullptr) {
    return;
  }
  size_t bufferSize = 0;
  for (auto& item : cachedFiles) {
    bufferSize += 8 + item->cacheKey.size();
  }
  tgfx::Buffer buffer(bufferSize);
  tgfx::DataView dataView(buffer.bytes(), buffer.size());
  size_t pos = 0;
  for (auto item = cachedFiles.rbegin(); item != cachedFiles.rend(); item++) {
    auto& fileInfo = *item;
    auto& cacheKey = fileInfo->cacheKey;
    dataView.setUint32(pos, fileInfo->fileID);
    dataView.setUint32(pos + 4, cacheKey.size());
    pos += 8;
    memcpy(dataView.writableBytes() + pos, cacheKey.data(), cacheKey.size());
    pos += cacheKey.size();
  }
  fwrite(buffer.data(), 1, bufferSize, file);
  fclose(file);
}

uint32_t DiskCache::getFileID(const std::string& key) {
  if (key.empty()) {
    return fileIDCount++;
  }
  auto result = fileIDMap.find(key);
  if (result != fileIDMap.end()) {
    return result->second;
  }
  auto newFileID = fileIDCount++;
  fileIDMap[key] = newFileID;
  return newFileID;
}

std::string DiskCache::fileIDToPath(uint32_t fileID) {
  return Directory::JoinPath(cacheFolder, std::to_string(fileID) + ".bin");
}

uint32_t DiskCache::filePathToID(const std::string& path) {
  auto fileName = Directory::GetFileName(path);
  if (fileName.empty()) {
    return 0;
  }
  return static_cast<uint32_t>(std::stoull(fileName));
}

void DiskCache::notifyFileClosed(uint32_t fileID) {
  std::lock_guard<std::mutex> autoLock(locker);
  openedFiles.erase(fileID);
  auto result = cachedFileMap.find(fileID);
  if (result == cachedFileMap.end()) {
    auto filePath = fileIDToPath(fileID);
    remove(filePath.c_str());
  } else {
    auto fileInfo = result->second;
    cachedFiles.erase(fileInfo->cachedPosition);
    for (auto it = cachedFiles.begin(); it != cachedFiles.end(); ++it) {
      if (openedFiles.count((*it)->fileID) == 0) {
        fileInfo->cachedPosition = cachedFiles.insert(it, fileInfo);
        fileInfo = nullptr;
        break;
      }
    }
    if (fileInfo) {
      cachedFiles.push_back(fileInfo);
      fileInfo->cachedPosition = --cachedFiles.end();
    }
    if (checkDiskSpace()) {
      saveConfig();
    }
  }
}

void DiskCache::notifyFileSizeChanged(uint32_t fileID, size_t fileSize) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto result = cachedFileMap.find(fileID);
  if (result != cachedFileMap.end()) {
    totalDiskSize += fileSize - result->second->fileSize;
    result->second->fileSize = fileSize;
    if (checkDiskSpace()) {
      saveConfig();
    }
  }
}
}  // namespace pag
