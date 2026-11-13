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

#include <condition_variable>
#include <fstream>
#include "rendering/utils/Directory.h"
#include "tgfx/core/DataView.h"
#include "utils/ScopedDiskCacheDirectory.h"

namespace pag {
namespace {

static std::shared_ptr<tgfx::Data> MakeCacheData() {
  const std::string data = "disk cache test data";
  return tgfx::Data::MakeWithCopy(data.data(), data.size());
}

static void CheckCacheState(size_t count, size_t diskSize) {
  auto cache = DiskCache::GetInstance();
  std::lock_guard<std::mutex> lock(cache->locker);
  EXPECT_EQ(cache->cachedFileIDs.size(), count);
  EXPECT_EQ(cache->cachedFileInfos.size(), count);
  EXPECT_EQ(cache->cachedFiles.size(), count);
  EXPECT_EQ(cache->totalDiskSize, diskSize);
}

static void CheckCacheDirectory(const std::string& dir, const std::string& customDir) {
  auto cache = DiskCache::GetInstance();
  std::lock_guard<std::mutex> lock(cache->locker);
  EXPECT_EQ(cache->customCacheDir, customDir);
  EXPECT_EQ(cache->getCacheDir(), dir);
  EXPECT_EQ(cache->configPath, dir.empty() ? "" : Directory::JoinPath(dir, "cache.cfg"));
  EXPECT_EQ(cache->cacheFolder, dir.empty() ? "" : Directory::JoinPath(dir, "files"));
}

static std::string CacheFilePath(const std::string& key) {
  auto cache = DiskCache::GetInstance();
  std::lock_guard<std::mutex> lock(cache->locker);
  auto result = cache->cachedFileIDs.find(key);
  return result == cache->cachedFileIDs.end() ? "" : cache->fileIDToPath(result->second);
}

static void CheckCacheData(const std::string& key, std::shared_ptr<tgfx::Data> data) {
  auto cached = DiskCache::ReadFile(key);
  ASSERT_TRUE(cached != nullptr);
  ASSERT_EQ(cached->size(), data->size());
  EXPECT_EQ(memcmp(cached->data(), data->data(), data->size()), 0);
}

static void WriteTestFile(const std::string& path, const std::string& data) {
  Directory::CreateRecursively(Directory::GetParentDirectory(path));
  std::ofstream stream(path, std::ios::binary);
  ASSERT_TRUE(stream.is_open());
  stream.write(data.data(), static_cast<std::streamsize>(data.size()));
  ASSERT_TRUE(stream.good());
}

static std::vector<std::string> ForeignFileNames() {
  return {
      "foreign.db", "2024",  "12.txt",         "0000000012.bin",           "12.BIN", "1", "1.txt",
      "0001.bin",   "0.bin", "4294967296.bin", "999999999999999999999.bin"};
}

static void CreateForeignEntries(const std::string& folder) {
  for (const auto& name : ForeignFileNames()) {
    ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(folder, name), "foreign data"));
  }
  ASSERT_TRUE(std::filesystem::create_directory(Directory::JoinPath(folder, "subdir")));
  ASSERT_NO_FATAL_FAILURE(
      WriteTestFile(Directory::JoinPath(folder, "subdir/1.bin"), "nested data"));
}

static void CheckForeignEntries(const std::string& folder) {
  for (const auto& name : ForeignFileNames()) {
    auto path = Directory::JoinPath(folder, name);
    ASSERT_TRUE(std::filesystem::exists(path));
    std::ifstream stream(path, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(data, "foreign data");
  }
  EXPECT_TRUE(std::filesystem::exists(Directory::JoinPath(folder, "subdir/1.bin")));
}

static void CollectFile(std::vector<std::string>* paths, const std::string& path, size_t) {
  paths->push_back(path);
}

static void AppendConfigRecord(std::ofstream& stream, uint32_t id, uint32_t keyLength,
                               const std::string& key) {
  uint8_t header[8] = {};
  tgfx::DataView view(header, sizeof(header));
  view.setUint32(0, id);
  view.setUint32(4, keyLength);
  stream.write(reinterpret_cast<const char*>(header), sizeof(header));
  stream.write(key.data(), static_cast<std::streamsize>(key.size()));
}

class DiskIOBarrier {
 public:
  DiskIOBarrier() {
    DiskIOWorker::GetInstance()->submit(std::bind(&DiskIOBarrier::block, this));
    std::unique_lock<std::mutex> lock(locker);
    condition.wait(lock, std::bind(&DiskIOBarrier::hasStarted, this));
  }

  ~DiskIOBarrier() {
    release();
  }

  /**
   * Releases the IO worker and waits for all queued operations to complete.
   */
  void release() {
    {
      std::lock_guard<std::mutex> lock(locker);
      released = true;
    }
    condition.notify_all();
    DiskIOWorker::GetInstance()->waitAll();
  }

 private:
  std::mutex locker;
  std::condition_variable condition;
  bool started = false;
  bool released = false;

  bool hasStarted() {
    return started;
  }

  bool hasReleased() {
    return released;
  }

  void block() {
    std::unique_lock<std::mutex> lock(locker);
    started = true;
    condition.notify_all();
    condition.wait(lock, std::bind(&DiskIOBarrier::hasReleased, this));
  }
};

}  // namespace

PAG_TEST(PAGDiskCacheDirectoryTest, CanonicalCacheFileNames) {
  uint32_t id = 0;
  EXPECT_TRUE(DiskCache::ParseFileID("/cache/files/12.bin", &id));
  EXPECT_EQ(id, 12u);
  EXPECT_TRUE(DiskCache::ParseFileID("C:\\cache\\files\\4294967295.bin", &id));
  EXPECT_EQ(id, UINT32_MAX);
  EXPECT_FALSE(DiskCache::ParseFileID("1.bin", nullptr));
  for (const auto& name : ForeignFileNames()) {
    EXPECT_FALSE(DiskCache::ParseFileID(name, &id)) << name;
  }
  for (const auto& name : {"", ".bin", "-1.bin", "+1.bin", "1.bin.tmp"}) {
    EXPECT_FALSE(DiskCache::ParseFileID(name, &id)) << name;
  }
}

PAG_TEST(PAGDiskCacheDirectoryTest, RepeatedCacheDirectorySetup) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("repeat", data));
  DiskIOWorker::GetInstance()->waitAll();
  auto cache = DiskCache::GetInstance();
  auto fileInfo = cache->cachedFiles.front();
  auto nextFileID = cache->fileIDCount;
  auto path = CacheFilePath("repeat");
  for (int index = 0; index < 3; index++) {
    PAGDiskCache::SetCacheDir(cleanup.directory);
  }
  auto trailingSlash = Directory::JoinPath(cleanup.directory, "");
  PAGDiskCache::SetCacheDir(trailingSlash);
  CheckCacheState(1, data->size());
  EXPECT_EQ(cache->cachedFiles.front(), fileInfo);
  EXPECT_EQ(cache->fileIDCount, nextFileID);
  EXPECT_EQ(cache->customCacheDir, trailingSlash);
  PAGDiskCache::SetMaxDiskSize(0);
  DiskIOWorker::GetInstance()->waitAll();
  CheckCacheState(0, 0);
  EXPECT_FALSE(std::filesystem::exists(path));
}

PAG_TEST(PAGDiskCacheDirectoryTest, CacheDirectorySwitchReloadsConfiguration) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto data = MakeCacheData();
  auto first = Directory::JoinPath(cleanup.directory, "first");
  auto second = Directory::JoinPath(cleanup.directory, "second");
  PAGDiskCache::SetCacheDir(first);
  ASSERT_TRUE(DiskCache::WriteFile("first", data));
  auto firstPath = CacheFilePath("first");
  PAGDiskCache::SetCacheDir(second);
  CheckCacheState(0, 0);
  ASSERT_TRUE(DiskCache::WriteFile("second", data));
  auto nextFileID = DiskCache::GetInstance()->fileIDCount;
  PAGDiskCache::SetCacheDir(first);
  CheckCacheState(1, data->size());
  EXPECT_EQ(CacheFilePath("first"), firstPath);
  EXPECT_TRUE(CacheFilePath("second").empty());
  EXPECT_EQ(DiskCache::GetInstance()->fileIDCount, nextFileID);
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("first", data));
  PAGDiskCache::SetCacheDir(second);
  CheckCacheState(1, data->size());
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("second", data));
}

PAG_TEST(PAGDiskCacheDirectoryTest, ForeignEntriesInCacheFolder) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("foreign", data));
  DiskIOWorker::GetInstance()->waitAll();
  auto path = CacheFilePath("foreign");
  auto folder = Directory::JoinPath(cleanup.directory, "files");
  ASSERT_NO_FATAL_FAILURE(CreateForeignEntries(folder));
  std::vector<std::string> paths;
  Directory::VisitFiles(
      folder, std::bind(CollectFile, &paths, std::placeholders::_1, std::placeholders::_2));
  EXPECT_EQ(paths.size(), ForeignFileNames().size() + 1);
  for (const auto& visited : paths) {
    EXPECT_EQ(visited.find("subdir"), std::string::npos);
  }
  {
    auto cache = DiskCache::GetInstance();
    std::lock_guard<std::mutex> lock(cache->locker);
    EXPECT_TRUE(cache->reloadConfig());
  }
  DiskIOWorker::GetInstance()->waitAll();
  CheckCacheState(1, data->size());
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("foreign", data));
  ASSERT_NO_FATAL_FAILURE(CheckForeignEntries(folder));
  PAGDiskCache::RemoveAll();
  DiskIOWorker::GetInstance()->waitAll();
  CheckCacheState(0, 0);
  EXPECT_FALSE(std::filesystem::exists(path));
  ASSERT_NO_FATAL_FAILURE(CheckForeignEntries(folder));
}

PAG_TEST(PAGDiskCacheDirectoryTest, MissingAndEmptyConfigurationPreservesForeignEntries) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  for (const auto& name : {"missing", "empty"}) {
    auto dir = Directory::JoinPath(cleanup.directory, name);
    auto folder = Directory::JoinPath(dir, "files");
    ASSERT_NO_FATAL_FAILURE(CreateForeignEntries(folder));
    ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(folder, "2.bin"), "orphan"));
    if (std::string(name) == "empty") {
      ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(dir, "cache.cfg"), ""));
    }
    PAGDiskCache::SetCacheDir(dir);
    DiskIOWorker::GetInstance()->waitAll();
    CheckCacheState(0, 0);
    EXPECT_FALSE(std::filesystem::exists(Directory::JoinPath(folder, "2.bin")));
    ASSERT_NO_FATAL_FAILURE(CheckForeignEntries(folder));
  }
}

PAG_TEST(PAGDiskCacheDirectoryTest, ConstructorPreservesForeignEntries) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  for (const auto& name : {"missing", "empty"}) {
    auto dir = Directory::JoinPath(cleanup.directory, name);
    auto folder = Directory::JoinPath(dir, "files");
    ASSERT_NO_FATAL_FAILURE(CreateForeignEntries(folder));
    ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(folder, "2.bin"), "orphan"));
    if (std::string(name) == "empty") {
      ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(dir, "cache.cfg"), ""));
    }
    DiskCache cache(dir);
    DiskIOWorker::GetInstance()->waitAll();
    EXPECT_TRUE(cache.cachedFileIDs.empty());
    EXPECT_EQ(cache.totalDiskSize, 0u);
    EXPECT_FALSE(std::filesystem::exists(Directory::JoinPath(folder, "2.bin")));
    ASSERT_NO_FATAL_FAILURE(CheckForeignEntries(folder));
  }
}

PAG_TEST(PAGDiskCacheDirectoryTest, DefaultCacheDirectoryReset) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto dir = Directory::JoinPath(cleanup.directory, "default");
  DiskCache::SetDefaultCacheDir(dir);
  CheckCacheDirectory(cleanup.directory, cleanup.directory);
  DiskCache::SetDefaultCacheDir("");
  EXPECT_EQ(DiskCache::GetInstance()->defaultCacheDir, dir);
  PAGDiskCache::SetCacheDir("");
  CheckCacheDirectory(dir, "");
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("default", data));
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_TRUE(std::filesystem::exists(CacheFilePath("default")));
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("default", data));
  EXPECT_TRUE(
      Directory::FindFiles(Directory::JoinPath(cleanup.directory, "files"), ".bin").empty());
}

PAG_TEST(PAGDiskCacheDirectoryTest, DefaultRegistrationRefreshesExistingSingleton) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto cache = DiskCache::GetInstance();
  {
    std::lock_guard<std::mutex> lock(cache->locker);
    cache->defaultCacheDir.clear();
    cache->hasDefaultCacheDir = true;
  }
  PAGDiskCache::SetCacheDir("");
  CheckCacheDirectory("", "");
  EXPECT_GT(PAGDiskCache::MaxDiskSize(), 0u);
  EXPECT_TRUE(DiskCache::OpenSequence("disabled",
                                      tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888), 1,
                                      30) == nullptr);
  auto dir = Directory::JoinPath(cleanup.directory, "late-default");
  DiskCache::SetDefaultCacheDir(dir);
  CheckCacheDirectory(dir, "");
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("enabled", data));
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("enabled", data));
}

PAG_TEST(PAGDiskCacheDirectoryTest, OpenSequenceRejectsDirectorySwitch) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto info = tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888);
  uint32_t pixels[1] = {0xFFFFFFFF};
  auto bitmap = BitmapBuffer::Wrap(info, pixels);
  auto sequence = DiskCache::OpenSequence("active", info, 2, 30);
  ASSERT_TRUE(sequence != nullptr);
  ASSERT_TRUE(sequence->writeFrame(0, bitmap));
  auto path = CacheFilePath("active");
  auto next = Directory::JoinPath(cleanup.directory, "next");
  PAGDiskCache::SetCacheDir(next);
  CheckCacheDirectory(cleanup.directory, cleanup.directory);
  EXPECT_EQ(DiskCache::OpenSequence("active", info, 2, 30), sequence);
  PAGDiskCache::SetCacheDir(Directory::JoinPath(cleanup.directory, ""));
  EXPECT_EQ(DiskCache::OpenSequence("active", info, 2, 30), sequence);
  ASSERT_TRUE(sequence->writeFrame(1, bitmap));
  CheckCacheState(1, sequence->fileSize());
  sequence = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  PAGDiskCache::SetCacheDir(next);
  CheckCacheDirectory(next, next);
  CheckCacheState(0, 0);
  auto replacement = DiskCache::OpenSequence("replacement", info, 2, 30);
  ASSERT_TRUE(replacement != nullptr);
  EXPECT_FALSE(replacement->readFrame(0, bitmap));
  ASSERT_TRUE(replacement->writeFrame(0, bitmap));
  EXPECT_TRUE(std::filesystem::exists(path));
}

PAG_TEST(PAGDiskCacheDirectoryTest, ClosingSequenceRejectsDirectorySwitch) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto sequence = DiskCache::OpenSequence(
      "active", tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888), 1, 30);
  ASSERT_TRUE(sequence != nullptr);
  DiskIOWorker::GetInstance()->waitAll();
  auto next = Directory::JoinPath(cleanup.directory, "next");
  DiskIOBarrier barrier;
  PAGDiskCache::RemoveAll();
  sequence = nullptr;
  PAGDiskCache::SetCacheDir(next);
  CheckCacheDirectory(cleanup.directory, cleanup.directory);
  barrier.release();
  PAGDiskCache::SetCacheDir(next);
  CheckCacheDirectory(next, next);
  CheckCacheState(0, 0);
}

PAG_TEST(PAGDiskCacheDirectoryTest, SwitchFlushesPendingConfiguration) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto first = cleanup.directory;
  auto second = Directory::JoinPath(first, "second");
  auto data = MakeCacheData();
  DiskIOBarrier barrier;
  ASSERT_TRUE(DiskCache::WriteFile("first", data));
  PAGDiskCache::SetCacheDir(second);
  EXPECT_GT(std::filesystem::file_size(Directory::JoinPath(first, "cache.cfg")), 0u);
  ASSERT_TRUE(DiskCache::WriteFile("second", data));
  PAGDiskCache::SetCacheDir(first);
  CheckCacheState(1, data->size());
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("first", data));
  barrier.release();
  PAGDiskCache::SetCacheDir(second);
  CheckCacheState(1, data->size());
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("second", data));
}

PAG_TEST(PAGDiskCacheDirectoryTest, EmptyConfigurationSupersedesPendingSave) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  DiskIOBarrier barrier;
  ASSERT_TRUE(DiskCache::WriteFile("pending", MakeCacheData()));
  auto path = CacheFilePath("pending");
  PAGDiskCache::RemoveAll();
  barrier.release();
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_EQ(std::filesystem::file_size(Directory::JoinPath(cleanup.directory, "cache.cfg")), 0u);
  CheckCacheState(0, 0);
}

PAG_TEST(PAGDiskCacheDirectoryTest, InvalidConfigurationRecordsAreIgnored) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto dir = Directory::JoinPath(cleanup.directory, "invalid");
  auto folder = Directory::JoinPath(dir, "files");
  ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(folder, "1.bin"), "one"));
  ASSERT_NO_FATAL_FAILURE(WriteTestFile(Directory::JoinPath(folder, "2.bin"), "two"));
  std::ofstream config(Directory::JoinPath(dir, "cache.cfg"), std::ios::binary);
  ASSERT_TRUE(config.is_open());
  AppendConfigRecord(config, 0, 4, "zero");
  AppendConfigRecord(config, UINT32_MAX, 3, "max");
  AppendConfigRecord(config, 1, 3, "one");
  AppendConfigRecord(config, 1, 9, "duplicate");
  AppendConfigRecord(config, 3, 3, "one");
  AppendConfigRecord(config, 2, 3, "two");
  AppendConfigRecord(config, 4, UINT32_MAX, "");
  config.close();
  PAGDiskCache::SetCacheDir(dir);
  CheckCacheState(2, 6);
  auto cache = DiskCache::GetInstance();
  EXPECT_EQ(cache->cachedFileIDs.at("one"), 1u);
  EXPECT_EQ(cache->cachedFileIDs.at("two"), 2u);
  EXPECT_EQ(cache->fileIDCount, 3u);
  EXPECT_TRUE(CacheFilePath("duplicate").empty());
}

PAG_TEST(PAGDiskCacheDirectoryTest, ExpiredConfigurationRemovesKeyMapping) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto dir = Directory::JoinPath(cleanup.directory, "expired");
  Directory::CreateRecursively(dir);
  std::ofstream config(Directory::JoinPath(dir, "cache.cfg"), std::ios::binary);
  ASSERT_TRUE(config.is_open());
  AppendConfigRecord(config, 1, 7, "expired");
  config.close();
  PAGDiskCache::SetCacheDir(dir);
  CheckCacheState(0, 0);
  auto sequence = DiskCache::OpenSequence(
      "expired", tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888), 1, 30);
  ASSERT_TRUE(sequence != nullptr);
  EXPECT_EQ(sequence->fileID, 2u);
}

PAG_TEST(PAGDiskCacheDirectoryTest, FileIDExhaustionDoesNotWrap) {
  ScopedDiskCacheDirectory cleanup;
  ASSERT_FALSE(cleanup.directory.empty());
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("existing", data));
  auto cache = DiskCache::GetInstance();
  cache->fileIDCount = UINT32_MAX;
  EXPECT_FALSE(DiskCache::WriteFile("overflow", data));
  EXPECT_TRUE(DiskCache::ReadFile("overflow") == nullptr);
  EXPECT_TRUE(DiskCache::OpenSequence("overflow",
                                      tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888), 1,
                                      30) == nullptr);
  EXPECT_EQ(cache->fileIDCount, UINT32_MAX);
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("existing", data));
}

PAG_TEST(PAGDiskCacheDirectoryTest, FixtureRestoresFileIDAndConfigurationState) {
  ScopedDiskCacheDirectory outer;
  ASSERT_FALSE(outer.directory.empty());
  auto data = MakeCacheData();
  ASSERT_TRUE(DiskCache::WriteFile("outer", data));
  DiskIOWorker::GetInstance()->waitAll();
  auto cache = DiskCache::GetInstance();
  auto nextID = cache->fileIDCount;
  auto saveState = cache->configSaveState;
  {
    ScopedDiskCacheDirectory inner;
    ASSERT_FALSE(inner.directory.empty());
    ASSERT_TRUE(DiskCache::WriteFile("inner", data));
    ASSERT_TRUE(DiskCache::WriteFile("inner-second", data));
  }
  EXPECT_EQ(cache->fileIDCount, nextID);
  EXPECT_EQ(cache->configSaveState, saveState);
  CheckCacheDirectory(outer.directory, outer.directory);
  CheckCacheState(1, data->size());
  ASSERT_NO_FATAL_FAILURE(CheckCacheData("outer", data));
}

}  // namespace pag
