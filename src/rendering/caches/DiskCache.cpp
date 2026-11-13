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
#include <atomic>
#include <cstdint>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "platform/Platform.h"
#include "rendering/utils/Directory.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/DataView.h"
#include "tgfx/core/Stream.h"

namespace pag {

static void RemoveFileTask(std::string path) {
  if (remove(path.c_str()) != 0) {
    LOGE("Failed to remove cached file: %s", path.c_str());
  }
}

void DiskCache::RemoveFileAsync(const std::string& filePath) {
  if (filePath.empty()) {
    return;
  }
  DiskIOWorker::GetInstance()->submit(std::bind(RemoveFileTask, filePath));
}

class ConfigSaveState {
 public:
  std::mutex locker = {};
  std::atomic<uint64_t> version{0};
};

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

void DiskCache::SetDefaultCacheDir(const std::string& dir) {
  GetInstance()->setDefaultCacheDir(dir);
}

std::string DiskCache::getCacheDir() {
  return customCacheDir.empty() ? getDefaultCacheDir() : customCacheDir;
}

std::string DiskCache::getDefaultCacheDir() {
  return hasDefaultCacheDir ? defaultCacheDir : Platform::Current()->getCacheDir();
}

void DiskCache::setCacheDir(const std::string& dir) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (applyCacheDir(dir.empty() ? getDefaultCacheDir() : dir)) {
    customCacheDir = dir;
  }
}

void DiskCache::setDefaultCacheDir(const std::string& dir) {
  if (dir.empty()) {
    return;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  if (!customCacheDir.empty() || applyCacheDir(dir)) {
    defaultCacheDir = dir;
    hasDefaultCacheDir = true;
  }
}

bool DiskCache::applyCacheDir(const std::string& dir) {
  auto newConfigPath = dir.empty() ? std::string() : Directory::JoinPath(dir, "cache.cfg");
  auto newCacheFolder = dir.empty() ? std::string() : Directory::JoinPath(dir, "files");
  if (configPath == newConfigPath && cacheFolder == newCacheFolder) {
    return true;
  }
  if (!openedFiles.empty()) {
    LOGE("Cannot change the disk cache directory while sequence files are open.");
    return false;
  }
  if (!configPath.empty()) {
    saveConfig(true);
  }
  configPath = newConfigPath;
  cacheFolder = newCacheFolder;
  configSaveState = std::make_shared<ConfigSaveState>();
  if (!reloadConfig() && !cacheFolder.empty()) {
    Directory::VisitFiles(cacheFolder, RemoveCacheFile);
  }
  return true;
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
  applyCacheDir(getDefaultCacheDir());
}

DiskCache::DiskCache(const std::string& defaultDir)
    : defaultCacheDir(defaultDir), hasDefaultCacheDir(true) {
  applyCacheDir(defaultDir);
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
  if (checkDiskSpace(maxDiskSize)) {
    saveConfig();
  }
}

void DiskCache::removeAll() {
  std::lock_guard<std::mutex> autoLock(locker);
  if (cacheFolder.empty()) {
    return;
  }
  Directory::VisitFiles(cacheFolder, std::bind(&DiskCache::removeClosedCacheFile, this,
                                               std::placeholders::_1, std::placeholders::_2));
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
  if (cacheFolder.empty()) {
    return nullptr;
  }
  auto fileID = getFileID(key);
  if (fileID == 0) {
    return nullptr;
  }
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
      if (fileID == 0) {
        return nullptr;
      }
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
  if (cacheFolder.empty() || key.empty()) {
    return nullptr;
  }
  auto fileID = getFileID(key);
  auto filePath = fileIDToPath(fileID);
  if (fileID == 0) {
    return nullptr;
  }
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
  if (cacheFolder.empty() || key.empty() || data == nullptr) {
    return false;
  }
  if (data->size() > maxDiskSize) {
    return false;
  }
  auto changed = checkDiskSpace(maxDiskSize - data->size());
  if (totalDiskSize > maxDiskSize - data->size()) {
    if (changed) {
      saveConfig();
    }
    return false;
  }
  auto fileID = getFileID(key);
  auto filePath = fileIDToPath(fileID);
  if (fileID == 0) {
    return false;
  }
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
    RemoveFileAsync(filePath);
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
  cachedFileIDs.erase(fileInfo->cacheKey);
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

bool DiskCache::reloadConfig() {
  cachedFileIDs.clear();
  cachedFileInfos.clear();
  cachedFiles.clear();
  totalDiskSize = 0;
  if (configPath.empty()) {
    return false;
  }
  auto file = fopen(configPath.c_str(), "rb");
  if (file == nullptr) {
    return false;
  }
  fseek(file, 0, SEEK_END);
  auto size = ftell(file);
  if (size <= 0) {
    fclose(file);
    return false;
  }
  fseek(file, 0, SEEK_SET);
  tgfx::Buffer buffer(size);
  auto length = fread(buffer.data(), 1, size, file);
  fclose(file);
  tgfx::DataView dataView(buffer.bytes(), length);
  size_t pos = 0;
  while (dataView.size() - pos >= 8) {
    auto fileID = dataView.getUint32(pos);
    auto keyLength = dataView.getUint32(pos + 4);
    pos += 8;
    if (keyLength > dataView.size() - pos) {
      break;
    }
    auto cacheKey = std::string(reinterpret_cast<const char*>(dataView.bytes()) + pos, keyLength);
    pos += keyLength;
    if (fileID == 0 || fileID == UINT32_MAX || cacheKey.empty() ||
        cachedFileInfos.count(fileID) > 0 || cachedFileIDs.count(cacheKey) > 0) {
      continue;
    }
    fileIDCount = std::max(fileIDCount, fileID + 1);
    addToCachedFiles(std::make_shared<FileInfo>(cacheKey, fileID, 0));
    cachedFileIDs[cacheKey] = fileID;
  }
  Directory::VisitFiles(cacheFolder, std::bind(&DiskCache::reconcileCacheFile, this,
                                               std::placeholders::_1, std::placeholders::_2));
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

void DiskCache::saveConfig(bool synchronous) {
  if (configPath.empty()) {
    return;
  }
  size_t bufferSize = 0;
  for (auto& item : cachedFiles) {
    bufferSize += 8 + item->cacheKey.size();
  }
  auto data = std::make_shared<tgfx::Buffer>(bufferSize);
  tgfx::DataView dataView(data->bytes(), data->size());
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
  auto currentVersion = ++configSaveState->version;
  auto tempPath = configPath + ".tmp." + std::to_string(getpid());
  if (synchronous) {
    WriteConfigTask(configPath, tempPath, data, configSaveState, currentVersion);
  } else {
    DiskIOWorker::GetInstance()->submit(std::bind(&DiskCache::WriteConfigTask, configPath, tempPath,
                                                  data, configSaveState, currentVersion));
  }
}

// The IO task never acquires DiskCache::locker. Switching directories can therefore flush the
// latest snapshot under that lock without waiting for close notifications on the IO worker.
void DiskCache::WriteConfigTask(const std::string& path, const std::string& tempPath,
                                std::shared_ptr<tgfx::Buffer> data,
                                std::shared_ptr<ConfigSaveState> state, uint64_t currentVersion) {
  std::lock_guard<std::mutex> autoLock(state->locker);
  if (currentVersion != state->version.load(std::memory_order_acquire)) {
    return;
  }
  Directory::CreateRecursively(Directory::GetParentDirectory(path));
  auto file = fopen(tempPath.c_str(), "wb");
  if (file == nullptr) {
    LOGE("Failed to open config file for writing: %s", tempPath.c_str());
    return;
  }
  auto written = fwrite(data->data(), 1, data->size(), file);
  fclose(file);
  if (written == data->size()) {
    rename(tempPath.c_str(), path.c_str());
  } else {
    LOGE("Failed to write config file: %s (written %zu of %zu bytes)", tempPath.c_str(), written,
         data->size());
    remove(tempPath.c_str());
  }
}

void DiskCache::CloseFileTask(FILE* fileToClose, DiskCache* cache, uint32_t id) {
  if (fileToClose != nullptr) {
    fclose(fileToClose);
  }
  if (cache != nullptr) {
    cache->notifyFileClosed(id);
  }
}

uint32_t DiskCache::getFileID(const std::string& key) {
  auto result = cachedFileIDs.find(key);
  if (result != cachedFileIDs.end()) {
    return result->second;
  }
  if (fileIDCount == UINT32_MAX) {
    return 0;
  }
  if (key.empty()) {
    return fileIDCount++;
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

bool DiskCache::ParseFileID(const std::string& path, uint32_t* fileID) {
  auto nameStart = path.find_last_of("/\\");
  nameStart = nameStart == std::string::npos ? 0 : nameStart + 1;
  auto nameLength = path.size() - nameStart;
  if (nameLength < 5 || path.compare(path.size() - 4, 4, ".bin") != 0 || path[nameStart] < '1' ||
      path[nameStart] > '9' || fileID == nullptr) {
    return false;
  }
  uint64_t value = 0;
  for (size_t index = nameStart; index < path.size() - 4; index++) {
    auto ch = path[index];
    if (ch < '0' || ch > '9') {
      return false;
    }
    value = value * 10 + static_cast<uint64_t>(ch - '0');
    if (value > UINT32_MAX) {
      return false;
    }
  }
  *fileID = static_cast<uint32_t>(value);
  return true;
}

void DiskCache::RemoveCacheFile(const std::string& path, size_t) {
  uint32_t fileID = 0;
  if (ParseFileID(path, &fileID)) {
    RemoveFileAsync(path);
  }
}

void DiskCache::removeClosedCacheFile(const std::string& path, size_t) {
  uint32_t fileID = 0;
  if (ParseFileID(path, &fileID) && openedFiles.count(fileID) == 0) {
    RemoveFileAsync(path);
  }
}

void DiskCache::reconcileCacheFile(const std::string& path, size_t fileSize) {
  uint32_t fileID = 0;
  if (!ParseFileID(path, &fileID)) {
    return;
  }
  auto result = cachedFileInfos.find(fileID);
  if (result == cachedFileInfos.end()) {
    RemoveFileAsync(path);
  } else {
    result->second->fileSize = fileSize;
  }
}

void DiskCache::notifyFileClosed(uint32_t fileID) {
  std::lock_guard<std::mutex> autoLock(locker);
  openedFiles.erase(fileID);
  auto result = cachedFileInfos.find(fileID);
  if (result == cachedFileInfos.end()) {
    auto filePath = fileIDToPath(fileID);
    RemoveFileAsync(filePath);
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
