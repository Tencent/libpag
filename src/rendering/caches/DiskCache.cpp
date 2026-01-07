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

#include "DiskCache.h"
#include "pag/pag.h"
#include "platform/Platform.h"
#include "rendering/utils/Directory.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/DataView.h"
#include "tgfx/core/Stream.h"

namespace pag {

class FileInfo {
 public:
  FileInfo(std::string cacheKey, uint32_t fileID, size_t fileSize = 0)
      : cacheKey(std::move(cacheKey)), fileID(fileID), fileSize(fileSize) {
  }

  std::string cacheKey;
  uint32_t fileID = 0;
  size_t fileSize = 0;
  std::list<std::shared_ptr<FileInfo>>::iterator cachedPosition;
};

void PAGDiskCache::SetCacheDir(const std::string& dir) {
  DiskCache::GetInstance()->setCacheDir(dir);
}

size_t PAGDiskCache::MaxDiskSize() {
  return DiskCache::GetInstance()->getMaxDiskSize();
}

void PAGDiskCache::SetMaxDiskSize(size_t size) {
  DiskCache::GetInstance()->setMaxDiskSize(size);
}

void PAGDiskCache::RemoveAll() {
  DiskCache::GetInstance()->removeAll();
}

std::string DiskCache::getCacheDir() {
  if (!customCacheDir.empty()) {
    return customCacheDir;
  }
  return Platform::Current()->getCacheDir();
}

void DiskCache::setCacheDir(const std::string& dir) {
  std::lock_guard<std::mutex> autoLock(locker);
  customCacheDir = dir;
  // Update cache paths
  auto cacheDir = dir.empty() ? Platform::Current()->getCacheDir() : dir;
  if (!cacheDir.empty()) {
    configPath = Directory::JoinPath(cacheDir, "cache.cfg");
    cacheFolder = Directory::JoinPath(cacheDir, "files");
  }
}

DiskCache* DiskCache::GetInstance() {
  static auto& diskCache = *new DiskCache();
  return &diskCache;
}

std::shared_ptr<SequenceFile> DiskCache::OpenSequence(
    const std::string& key, const tgfx::ImageInfo& info, int frameCount, float frameRate,
    const std::vector<TimeRange>& staticTimeRanges) {
  return GetInstance()->openSequence(key, info, frameCount, frameRate, staticTimeRanges);
}

std::shared_ptr<tgfx::Data> DiskCache::ReadFile(const std::string& key) {
  return GetInstance()->readFile(key);
}

bool DiskCache::WriteFile(const std::string& key, std::shared_ptr<tgfx::Data> data) {
  return GetInstance()->writeFile(key, data);
}

DiskCache::DiskCache() {
}

void DiskCache::checkInitialized() {
  if (initialized) {
    return;
  }
  initialized = true;
  auto cacheDir = getCacheDir();
  if (!cacheDir.empty()) {
    configPath = Directory::JoinPath(cacheDir, "cache.cfg");
    cacheFolder = Directory::JoinPath(cacheDir, "files");
    if (!readConfig()) {
      Directory::VisitFiles(cacheFolder,
                            [&](const std::string& path, size_t) { remove(path.c_str()); });
    }
  }
}

size_t DiskCache::getMaxDiskSize() {
  std::lock_guard<std::mutex> autoLock(locker);
  return maxDiskSize;
}

void DiskCache::setMaxDiskSize(size_t size) {
  std::lock_guard<std::mutex> autoLock(locker);
  checkInitialized();
  if (maxDiskSize == size) {
    return;
  }
  maxDiskSize = size;
  if (checkDiskSpace(maxDiskSize)) {
    saveConfig();
  }
}

void DiskCache::removeAll() {
  std::lock_guard<std::mutex> autoLock(locker);
  checkInitialized();
  if (cacheFolder.empty()) {
    return;
  }
  Directory::VisitFiles(cacheFolder, [&](const std::string& path, size_t) {
    auto fileID = filePathToID(path);
    if (openedFiles.count(fileID) > 0) {
      return;
    }
    remove(path.c_str());
  });
  cachedFileIDs.clear();
  cachedFiles.clear();
  cachedFileInfos.clear();
  totalDiskSize = 0;
  saveConfig();
  LOGI("DiskCache::removeAll() all cached files have been removed!");
}

std::shared_ptr<SequenceFile> DiskCache::openSequence(
    const std::string& key, const tgfx::ImageInfo& info, int frameCount, float frameRate,
    const std::vector<TimeRange>& staticTimeRanges) {
  std::lock_guard<std::mutex> autoLock(locker);
  checkInitialized();
  if (cacheFolder.empty()) {
    return nullptr;
  }
  auto fileID = getFileID(key);
  auto result = openedFiles.find(fileID);
  if (result != openedFiles.end()) {
    auto sequenceFile = result->second.lock();
    if (sequenceFile != nullptr) {
      if (sequenceFile->compatible(info, frameCount, frameRate, staticTimeRanges)) {
        moveToFront(cachedFileInfos[fileID]);
        return sequenceFile;
      }
      changeToTemporary(fileID);
      fileID = getFileID(key);
    }
  }
  auto filePath = fileIDToPath(fileID);
  auto sequenceFile = SequenceFile::Open(filePath, info, frameCount, frameRate, staticTimeRanges);
  if (sequenceFile == nullptr) {
    return nullptr;
  }
  sequenceFile->diskCache = this;
  sequenceFile->fileID = fileID;
  openedFiles[fileID] = sequenceFile;
  if (!key.empty()) {
    auto oldFileInfo = cachedFileInfos[fileID];
    if (oldFileInfo) {
      auto fileSize = sequenceFile->fileSize();
      totalDiskSize += fileSize - oldFileInfo->fileSize;
      oldFileInfo->fileSize = fileSize;
      moveToFront(oldFileInfo);
    } else {
      addToCachedFiles(std::make_shared<FileInfo>(key, fileID, 0));
      saveConfig();
    }
  }
  return sequenceFile;
}

std::shared_ptr<tgfx::Data> DiskCache::readFile(const std::string& key) {
  std::lock_guard<std::mutex> autoLock(locker);
  checkInitialized();
  if (cacheFolder.empty() || key.empty()) {
    return nullptr;
  }
  auto fileID = getFileID(key);
  auto filePath = fileIDToPath(fileID);
  auto stream = tgfx::Stream::MakeFromFile(filePath);
  if (stream == nullptr) {
    return nullptr;
  }
  tgfx::Buffer buffer(stream->size());
  auto readLength = stream->read(buffer.data(), buffer.size());
  if (readLength != buffer.size()) {
    return nullptr;
  }
  return buffer.release();
}

bool DiskCache::writeFile(const std::string& key, std::shared_ptr<tgfx::Data> data) {
  std::lock_guard<std::mutex> autoLock(locker);
  checkInitialized();
  if (cacheFolder.empty() || key.empty() || data == nullptr) {
    return false;
  }
  auto changed = checkDiskSpace(maxDiskSize - data->size());
  if (totalDiskSize + data->size() > maxDiskSize) {
    if (changed) {
      saveConfig();
    }
    return false;
  }
  auto fileID = getFileID(key);
  auto filePath = fileIDToPath(fileID);
  Directory::CreateRecursively(Directory::GetParentDirectory(filePath));
  auto file = fopen(filePath.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  auto writeLength = fwrite(data->data(), 1, data->size(), file);
  fclose(file);
  if (writeLength != data->size()) {
    return false;
  }
  totalDiskSize += data->size();
  auto fileInfo = cachedFileInfos[fileID];
  if (fileInfo) {
    totalDiskSize -= fileInfo->fileSize;
    fileInfo->fileSize = data->size();
  } else {
    addToCachedFiles(std::make_shared<FileInfo>(key, fileID, data->size()));
    fileInfo = cachedFileInfos[fileID];
    changed = true;
  }
  moveToBeforeOpenedFiles(fileInfo);
  if (changed) {
    saveConfig();
  }
  return true;
}

bool DiskCache::checkDiskSpace(size_t maxSize) {
  bool changed = false;
  if (totalDiskSize <= maxSize) {
    return changed;
  }
  LOGE("Cached data exceeds threshold, current threshold is:%lld !!! \n", maxSize);
  while (totalDiskSize > maxSize) {
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
  cachedFileInfos[fileInfo->fileID] = fileInfo;
}

void DiskCache::removeFromCachedFiles(std::shared_ptr<FileInfo> fileInfo) {
  cachedFiles.erase(fileInfo->cachedPosition);
  cachedFileInfos.erase(fileInfo->fileID);
}

void DiskCache::moveToFront(std::shared_ptr<FileInfo> fileInfo) {
  cachedFiles.erase(fileInfo->cachedPosition);
  cachedFiles.push_front(fileInfo);
  fileInfo->cachedPosition = cachedFiles.begin();
}

void DiskCache::moveToBeforeOpenedFiles(std::shared_ptr<FileInfo> fileInfo) {
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
}

bool DiskCache::readConfig() {
  auto file = fopen(configPath.c_str(), "rb");
  if (file == nullptr) {
    return false;
  }
  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  if (size == 0) {
    fclose(file);
    return false;
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
    cachedFileIDs[cacheKey] = fileID;
  }
  Directory::VisitFiles(cacheFolder, [&](const std::string& path, size_t fileSize) {
    auto fileID = filePathToID(path);
    auto result = cachedFileInfos.find(fileID);
    if (result == cachedFileInfos.end()) {
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
  if (checkDiskSpace(maxDiskSize) || !expiredFiles.empty()) {
    saveConfig();
  }
  return true;
}

void DiskCache::saveConfig() {
  Directory::CreateRecursively(Directory::GetParentDirectory(configPath));
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
    dataView.setUint32(pos + 4, static_cast<uint32_t>(cacheKey.size()));
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
  auto result = cachedFileIDs.find(key);
  if (result != cachedFileIDs.end()) {
    return result->second;
  }
  auto newFileID = fileIDCount++;
  cachedFileIDs[key] = newFileID;
  return newFileID;
}

void DiskCache::changeToTemporary(uint32_t fileID) {
  auto result = cachedFileInfos.find(fileID);
  if (result == cachedFileInfos.end()) {
    return;
  }
  auto fileInfo = result->second;
  cachedFiles.erase(fileInfo->cachedPosition);
  cachedFileInfos.erase(fileID);
  totalDiskSize -= fileInfo->fileSize;
  cachedFileIDs.erase(fileInfo->cacheKey);
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
  auto result = cachedFileInfos.find(fileID);
  if (result == cachedFileInfos.end()) {
    auto filePath = fileIDToPath(fileID);
    remove(filePath.c_str());
  } else {
    auto fileInfo = result->second;
    moveToBeforeOpenedFiles(fileInfo);
    if (checkDiskSpace(maxDiskSize)) {
      saveConfig();
    }
  }
}

void DiskCache::notifyFileSizeChanged(uint32_t fileID, size_t fileSize) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto result = cachedFileInfos.find(fileID);
  if (result != cachedFileInfos.end()) {
    totalDiskSize += fileSize - result->second->fileSize;
    result->second->fileSize = fileSize;
    if (checkDiskSpace(maxDiskSize)) {
      saveConfig();
    }
  }
}

}  // namespace pag
