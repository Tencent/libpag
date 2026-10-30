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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>
#include "pag/pag.h"
#include "platform/Platform.h"
#include "rendering/caches/DiskCache.h"
#include "rendering/utils/BitmapBuffer.h"
#include "rendering/utils/Directory.h"
#include "utils/TestUtils.h"

#ifdef TGFX_USE_OPENGL
#include "tgfx/gpu/opengl/GLDevice.h"
#endif

namespace pag {

//PAG_TEST(PAGDiskCacheTest, GenerateTestCaches) {
//  auto cacheDir = Platform::Current()->getCacheDir();
//  std::filesystem::remove_all(cacheDir);
//  auto pagFile = LoadPAGFile("resources/apitest/ZC2.pag");
//  tgfx::Bitmap bitmap.allocPixels(info.width(), info.height(), false, false);
//  tgfx::Pixmap pixmap(bitmap);
//  auto pagPlayer = std::make_shared<PAGPlayer>();
//  pagPlayer->setComposition(pagFile);
//  auto pagSurface = OffscreenSurface::Make(pagFile->width(), pagFile->height());
//  pagPlayer->setSurface(pagSurface);
//  auto info =
//      tgfx::ImageInfo::Make(pagFile->width(), pagFile->height(), tgfx::ColorType::RGBA_8888);
//  auto sequenceFile =
//      DiskCache::OpenSequence("resources/apitest/ZC2.pag", info, 30, pagFile->frameRate());
//  sequenceFile =
//      DiskCache::OpenSequence("resources/apitest/ZC2.pag.720x1280", info, 30, pagFile->frameRate());
//  for (auto i = 0; i < 11; i++) {
//    pagPlayer->flush();
//    pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixmap.writablePixels(),
//                           pixmap.rowBytes());
//    sequenceFile->writeFrame(i, BitmapBuffer::Wrap(pixmap.info(), pixmap.writablePixels()));
//    pagPlayer->nextFrame();
//  }
//
//  info = tgfx::ImageInfo::Make(360, 640, tgfx::ColorType::RGBA_8888);
//  pixmap.reset();
//  bitmap.allocPixels(info.width(), info.height(), false, false);
//  pixmap.reset(bitmap);
//  pagSurface = OffscreenSurface::Make(info.width(), info.height());
//  pagPlayer->setSurface(pagSurface);
//  sequenceFile =
//      DiskCache::OpenSequence("resources/apitest/ZC2.pag.360x640", info, 30, pagFile->frameRate());
//  pagPlayer->setProgress(0);
//  for (auto i = 0; i < 30; i++) {
//    pagPlayer->flush();
//    pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied, pixmap.writablePixels(),
//                           pixmap.rowBytes());
//    sequenceFile->writeFrame(i, BitmapBuffer::Wrap(pixmap.info(), pixmap.writablePixels()));
//    pagPlayer->nextFrame();
//  }
//  sequenceFile = nullptr;
//
//  auto diskDir = ProjectPath::Absolute("resources/disk/libpag");
//  std::filesystem::remove_all(diskDir);
//  std::filesystem::create_directories(diskDir);
//  std::filesystem::copy(cacheDir, diskDir, std::filesystem::copy_options::recursive);
//  std::filesystem::remove(diskDir + "/files/1.bin");
//  std::filesystem::copy(diskDir + "/files/3.bin", diskDir + "/files/4.bin");
//  std::filesystem::remove_all(cacheDir);
//}

namespace {
static constexpr size_t InitialSequenceDiskSize = 568915u;

static void ClearDiskCacheAndWait() {
  auto worker = DiskIOWorker::GetInstance();
  worker->waitAll();
  PAGDiskCache::RemoveAll();
  worker->waitAll();
}

class ScopedDiskCacheCleanup {
 public:
  const size_t originalMaxDiskSize;

  ScopedDiskCacheCleanup() : originalMaxDiskSize(PAGDiskCache::MaxDiskSize()) {
  }

  ScopedDiskCacheCleanup(const ScopedDiskCacheCleanup&) = delete;
  ScopedDiskCacheCleanup& operator=(const ScopedDiskCacheCleanup&) = delete;

  ~ScopedDiskCacheCleanup() {
    DiskIOWorker::GetInstance()->waitAll();
    PAGDiskCache::SetMaxDiskSize(originalMaxDiskSize);
    ClearDiskCacheAndWait();
  }
};

static void ResetDiskCacheFixture(const std::string& cacheDir, size_t maxDiskSize,
                                  const std::string& fixturePath = {}) {
  ASSERT_FALSE(cacheDir.empty());
  auto diskCache = DiskCache::GetInstance();
  DiskIOWorker::GetInstance()->waitAll();
  {
    std::lock_guard<std::mutex> lock(diskCache->locker);
    ASSERT_TRUE(diskCache->openedFiles.empty());
    ASSERT_EQ(diskCache->getCacheDir(), cacheDir);
  }
  ClearDiskCacheAndWait();
  if (fixturePath.empty()) {
    return;
  }
  std::filesystem::remove_all(cacheDir);
  std::filesystem::create_directories(cacheDir);
  std::filesystem::copy(fixturePath, cacheDir, std::filesystem::copy_options::recursive);
  {
    std::lock_guard<std::mutex> lock(diskCache->locker);
    // Reload the fixture into the already-live singleton, not just its on-disk files.
    diskCache->fileIDCount = DiskCache::InitialFileID;
    diskCache->maxDiskSize = maxDiskSize;
    ASSERT_TRUE(diskCache->readConfig());
  }
  DiskIOWorker::GetInstance()->waitAll();
}

struct SequenceFileState {
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<PAGPlayer> pagPlayer = nullptr;
  std::shared_ptr<PAGSurface> pagSurface = nullptr;
  tgfx::Bitmap bitmap = {};
  tgfx::Pixmap pixmap = {};
  std::shared_ptr<BitmapBuffer> buffer = nullptr;
  std::shared_ptr<SequenceFile> sequenceFile = nullptr;
  size_t halfSequenceFileSize = 0;
};

static void ResizeSequenceBitmap(SequenceFileState& state, int width, int height) {
  state.pixmap.reset();
  ASSERT_TRUE(state.bitmap.allocPixels(width, height, false, false));
  state.pixmap.reset(state.bitmap);
  state.buffer = BitmapBuffer::Wrap(state.pixmap.info(), state.pixmap.writablePixels());
}

static void CheckSequenceFileMetadata(SequenceFileState& state, const std::string& cacheDir) {
  state.pagFile = LoadPAGFile("resources/apitest/ZC2.pag");
  ASSERT_TRUE(state.pagFile != nullptr);
  auto info = tgfx::ImageInfo::Make(state.pagFile->width(), state.pagFile->height(),
                                    tgfx::ColorType::RGBA_8888);
  state.sequenceFile = DiskCache::OpenSequence("resources/apitest/ZC2.pag.720x1280", info, 30,
                                               state.pagFile->frameRate());
  ASSERT_TRUE(state.sequenceFile != nullptr);
  EXPECT_EQ(state.sequenceFile->width(), state.pagFile->width());
  EXPECT_EQ(state.sequenceFile->height(), state.pagFile->height());
  EXPECT_EQ(state.sequenceFile->numFrames(), 30u);
  EXPECT_EQ(state.sequenceFile->frameRate(), state.pagFile->frameRate());
  EXPECT_FALSE(state.sequenceFile->isComplete());
  EXPECT_EQ(state.sequenceFile->cachedFrames, 11);
  auto diskCache = DiskCache::GetInstance();
  EXPECT_EQ(state.sequenceFile->diskCache, diskCache);
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_FALSE(std::filesystem::exists(cacheDir + "/files/4.bin"));
  EXPECT_TRUE(diskCache->openedFiles.size() == 1);
  EXPECT_TRUE(diskCache->cachedFileInfos.size() == 2);
  EXPECT_EQ(diskCache->cachedFiles.size(), 2u);
  EXPECT_EQ(diskCache->fileIDCount, 4u);
  EXPECT_EQ(diskCache->totalDiskSize, InitialSequenceDiskSize);
}

static void CheckSequenceFileReadAndWrite(SequenceFileState& state) {
  ASSERT_NO_FATAL_FAILURE(
      ResizeSequenceBitmap(state, state.pagFile->width(), state.pagFile->height()));
  auto success = state.sequenceFile->readFrame(10, state.buffer);
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/SequenceFile_10"));
  success = state.sequenceFile->readFrame(15, state.buffer);
  EXPECT_FALSE(success);
  success = state.sequenceFile->readFrame(20, state.buffer);
  EXPECT_FALSE(success);

  state.pagPlayer = std::make_shared<PAGPlayer>();
  state.pagPlayer->setComposition(state.pagFile);
  state.pagSurface = OffscreenSurface::Make(state.pagFile->width(), state.pagFile->height());
  state.pagPlayer->setSurface(state.pagSurface);
  auto initialFileSize = state.sequenceFile->fileSize();
  auto diskCache = DiskCache::GetInstance();
  for (auto i = 0; i < 30; i++) {
    state.pagPlayer->flush();
    success = state.pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                           state.pixmap.writablePixels(), state.pixmap.rowBytes());
    ASSERT_TRUE(success);
    success = state.sequenceFile->writeFrame(i, state.buffer);
    if (i < 11) {
      EXPECT_FALSE(success);
      EXPECT_EQ(diskCache->totalDiskSize, InitialSequenceDiskSize);
    }
    state.pagPlayer->nextFrame();
  }
  EXPECT_EQ(diskCache->totalDiskSize,
            InitialSequenceDiskSize + state.sequenceFile->fileSize() - initialFileSize);
  EXPECT_TRUE(state.sequenceFile->isComplete());
  EXPECT_TRUE(state.sequenceFile->encoder == nullptr);
  success = state.sequenceFile->readFrame(15, state.buffer);
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/SequenceFile_15"));
  success = state.sequenceFile->readFrame(30, state.buffer);
  EXPECT_FALSE(success);
}

static void CheckHalfResolutionSequence(SequenceFileState& state) {
  auto info = tgfx::ImageInfo::Make(360, 640, tgfx::ColorType::RGBA_8888);
  ASSERT_NO_FATAL_FAILURE(ResizeSequenceBitmap(state, info.width(), info.height()));
  auto halfSequenceFile = DiskCache::OpenSequence("resources/apitest/ZC2.pag.360x640", info, 30,
                                                  state.pagFile->frameRate());
  ASSERT_TRUE(halfSequenceFile != nullptr);
  EXPECT_EQ(halfSequenceFile->width(), 360);
  EXPECT_EQ(halfSequenceFile->height(), 640);
  EXPECT_EQ(halfSequenceFile->numFrames(), 30u);
  EXPECT_EQ(halfSequenceFile->frameRate(), state.pagFile->frameRate());
  EXPECT_TRUE(halfSequenceFile->isComplete());
  auto success = halfSequenceFile->readFrame(20, state.buffer);
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/SequenceFile_20"));
  state.halfSequenceFileSize = halfSequenceFile->fileSize();
  halfSequenceFile = nullptr;
}

static void CheckSequenceFileEviction(SequenceFileState& state, const std::string& cacheDir) {
  auto info = tgfx::ImageInfo::Make(540, 960, tgfx::ColorType::RGBA_8888);
  state.pagSurface = OffscreenSurface::Make(info.width(), info.height());
  state.pagPlayer->setSurface(state.pagSurface);
  ASSERT_NO_FATAL_FAILURE(ResizeSequenceBitmap(state, info.width(), info.height()));
  auto diskCache = DiskCache::GetInstance();
  const auto lastTotalDiskSize = diskCache->totalDiskSize;
  PAGDiskCache::SetMaxDiskSize(1500000u);
  EXPECT_EQ(PAGDiskCache::MaxDiskSize(), 1500000u);
  state.sequenceFile = DiskCache::OpenSequence("resources/apitest/ZC2.pag.540x960", info, 30,
                                               state.pagFile->frameRate());
  state.pagPlayer->setProgress(0);
  for (auto i = 0; i < 30; i++) {
    state.pagPlayer->flush();
    auto success =
        state.pagSurface->readPixels(ColorType::RGBA_8888, AlphaType::Premultiplied,
                                     state.pixmap.writablePixels(), state.pixmap.rowBytes());
    ASSERT_TRUE(success);
    state.sequenceFile->writeFrame(i, state.buffer);
    state.pagPlayer->nextFrame();
  }
  auto success = state.sequenceFile->readFrame(22, state.buffer);
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/SequenceFile_22"));
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_EQ(diskCache->totalDiskSize,
            lastTotalDiskSize + state.sequenceFile->fileSize() - state.halfSequenceFileSize);
  EXPECT_FALSE(std::filesystem::exists(cacheDir + "/files/3.bin"));
  EXPECT_TRUE(std::filesystem::exists(cacheDir + "/files/2.bin"));
  PAGDiskCache::SetMaxDiskSize(0);
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_EQ(diskCache->totalDiskSize, state.sequenceFile->fileSize());
  EXPECT_FALSE(std::filesystem::exists(cacheDir + "/files/2.bin"));
  EXPECT_TRUE(std::filesystem::exists(cacheDir + "/files/4.bin"));
}

static void CheckSequenceFileReuseAndRelease(SequenceFileState& state,
                                             const std::string& cacheDir) {
  auto info = tgfx::ImageInfo::Make(540, 960, tgfx::ColorType::RGBA_8888);
  auto sequenceFile2 = DiskCache::OpenSequence("resources/apitest/ZC2.pag.540x960", info, 30,
                                               state.pagFile->frameRate());
  EXPECT_EQ(sequenceFile2, state.sequenceFile);
  auto sequenceFile3 = DiskCache::OpenSequence("resources/apitest/ZC2.pag.540x960", info, 30,
                                               state.pagFile->frameRate() * 0.5f);
  EXPECT_NE(sequenceFile3, state.sequenceFile);
  EXPECT_EQ(DiskCache::GetInstance()->totalDiskSize, 0u);
  EXPECT_TRUE(std::filesystem::exists(cacheDir + "/files/4.bin"));
  EXPECT_TRUE(std::filesystem::exists(cacheDir + "/files/5.bin"));
  state.sequenceFile = nullptr;
  sequenceFile2 = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  EXPECT_FALSE(std::filesystem::exists(cacheDir + "/files/4.bin"));
}
}  // namespace

/**
 * 用例描述: 测试 SequenceFile 的磁盘缓存功能。
 */
PAG_PIXEL_TEST(PAGDiskCacheTest, SequenceFile) {
  auto cacheDir = Platform::Current()->getCacheDir();
  ScopedDiskCacheCleanup cleanup;
  ASSERT_NO_FATAL_FAILURE(ResetDiskCacheFixture(cacheDir, cleanup.originalMaxDiskSize,
                                                ProjectPath::Absolute("resources/disk/libpag")));
  SequenceFileState state;
  ASSERT_NO_FATAL_FAILURE(CheckSequenceFileMetadata(state, cacheDir));
  ASSERT_NO_FATAL_FAILURE(CheckSequenceFileReadAndWrite(state));
  ASSERT_NO_FATAL_FAILURE(CheckHalfResolutionSequence(state));
  ASSERT_NO_FATAL_FAILURE(CheckSequenceFileEviction(state, cacheDir));
  ASSERT_NO_FATAL_FAILURE(CheckSequenceFileReuseAndRelease(state, cacheDir));
}

namespace {
struct DecoderCacheState {
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<PAGFile> pagFile2 = nullptr;
  std::shared_ptr<PAGComposition> composition = nullptr;
  std::shared_ptr<PAGDecoder> decoder = nullptr;
  std::shared_ptr<PAGDecoder> decoder2 = nullptr;
  std::shared_ptr<PAGDecoder> decoder3 = nullptr;
  std::shared_ptr<PAGDecoder> decoder4 = nullptr;
  std::shared_ptr<PAGDecoder> decoder5 = nullptr;
  tgfx::Bitmap bitmap = {};
  tgfx::Pixmap pixmap = {};
};

static void CheckDecoderFrameCache(DecoderCacheState& state) {
  state.pagFile = LoadPAGFile("resources/apitest/data_bmp.pag");
  ASSERT_TRUE(state.pagFile != nullptr);
  state.decoder = PAGDecoder::MakeFrom(state.pagFile, 30, 0.5f);
  ASSERT_TRUE(state.decoder != nullptr);
  EXPECT_EQ(state.decoder->width(), state.pagFile->width() / 2);
  EXPECT_EQ(state.decoder->height(), state.pagFile->height() / 2);
  EXPECT_EQ(state.decoder->numFrames(), 95);
  EXPECT_EQ(state.decoder->frameRate(), 24.0f);
  state.pagFile = nullptr;
  ASSERT_TRUE(
      state.bitmap.allocPixels(state.decoder->width(), state.decoder->height(), false, false));
  state.pixmap.reset(state.bitmap);
  for (int i = 0; i < state.decoder->numFrames(); i++) {
    auto success =
        state.decoder->readFrame(i, state.pixmap.writablePixels(), state.pixmap.rowBytes());
    EXPECT_TRUE(success);
  }
  EXPECT_TRUE(state.decoder->sequenceFile->isComplete());
  EXPECT_TRUE(state.decoder->sequenceFile != nullptr);
  EXPECT_TRUE(state.decoder->reader == nullptr);
  EXPECT_TRUE(state.decoder->getComposition() == nullptr);
  auto success =
      state.decoder->readFrame(50, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/decoder_frame_50"));
}

static void CheckDecoderReuseAndFormats(DecoderCacheState& state) {
  state.decoder2 = PAGDecoder::MakeFrom(state.pagFile, 30, 0.5f);
  EXPECT_TRUE(state.decoder2 == nullptr);
  state.pagFile = LoadPAGFile("resources/apitest/data_bmp.pag");
  state.decoder2 = PAGDecoder::MakeFrom(state.pagFile, 30, 0.5f);
  ASSERT_TRUE(state.decoder2 != nullptr);
  auto success =
      state.decoder2->readFrame(50, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_EQ(state.decoder->sequenceFile, state.decoder2->sequenceFile);
  EXPECT_TRUE(state.decoder2->reader == nullptr);
  EXPECT_TRUE(state.decoder2->getComposition() != nullptr);
  success = state.decoder2->readFrame(50, state.pixmap.writablePixels(), state.pixmap.rowBytes(),
                                      ColorType::BGRA_8888);
  EXPECT_FALSE(success);
  success = state.decoder2->readFrame(50, state.pixmap.writablePixels(), state.pixmap.rowBytes(),
                                      ColorType::RGBA_8888, AlphaType::Unpremultiplied);
  EXPECT_FALSE(success);
  success =
      state.decoder2->readFrame(50, state.pixmap.writablePixels(), state.pixmap.rowBytes() - 1);
  EXPECT_FALSE(success);
  success = state.decoder2->readFrame(-1, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_FALSE(success);
  success = state.decoder2->readFrame(120, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_FALSE(success);

  state.decoder3 = PAGDecoder::MakeFrom(state.pagFile, 30, 0.5f);
  EXPECT_TRUE(state.decoder3 != nullptr);
  EXPECT_TRUE(state.decoder3->getComposition() != nullptr);
  EXPECT_TRUE(state.decoder2->getComposition() == nullptr);
  success = state.decoder3->readFrame(0, state.pixmap.writablePixels(), state.pixmap.rowBytes(),
                                      ColorType::BGRA_8888, AlphaType::Unpremultiplied);
  EXPECT_TRUE(success);
  EXPECT_TRUE(state.decoder3->getComposition() != nullptr);
  auto info = tgfx::ImageInfo::Make(state.pixmap.width(), state.pixmap.height(),
                                    tgfx::ColorType::BGRA_8888, tgfx::AlphaType::Unpremultiplied);
  EXPECT_TRUE(Baseline::Compare(tgfx::Pixmap(info, state.pixmap.pixels()),
                                "PAGDiskCacheTest/decoder_frame_0"));
  EXPECT_NE(state.decoder->sequenceFile, state.decoder3->sequenceFile);
  EXPECT_TRUE(state.decoder3->reader != nullptr);
  EXPECT_TRUE(state.decoder3->getComposition() != nullptr);
}

static void CheckDecoderCompositionChanges(DecoderCacheState& state) {
  state.pagFile2 = LoadPAGFile("resources/apitest/ZC2.pag");
  ASSERT_TRUE(state.pagFile2 != nullptr);
  state.composition = PAGComposition::Make(state.pagFile2->width(), state.pagFile2->height());
  state.composition->addLayer(state.pagFile2);
  state.composition->addLayer(state.pagFile);
  auto success =
      state.decoder3->readFrame(1, state.pixmap.writablePixels(), state.pixmap.rowBytes(),
                                ColorType::BGRA_8888, AlphaType::Unpremultiplied);
  EXPECT_FALSE(success);
  state.decoder4 = PAGDecoder::MakeFrom(state.composition);
  ASSERT_TRUE(state.decoder4 != nullptr);
  EXPECT_EQ(state.decoder4->height(), state.pagFile2->height());
  EXPECT_EQ(state.decoder4->width(), state.pagFile2->width());
  EXPECT_EQ(state.decoder4->numFrames(), 119);
  EXPECT_EQ(state.decoder4->frameRate(), 30.0f);
  state.pixmap.reset();
  ASSERT_TRUE(
      state.bitmap.allocPixels(state.pagFile2->width(), state.pagFile2->height(), false, false));
  state.pixmap.reset(state.bitmap);
  success = state.decoder4->readFrame(15, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/decoder_frame_15"));
  state.composition->removeLayerAt(1);
  EXPECT_EQ(state.decoder4->numFrames(), 30);
  EXPECT_EQ(state.decoder4->frameRate(), 24.0f);
  success = state.decoder4->readFrame(12, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(state.pixmap, "PAGDiskCacheTest/decoder_frame_12"));

  state.decoder5 = PAGDecoder::MakeFrom(state.composition);
  EXPECT_TRUE(state.decoder5 != nullptr);
  success = state.decoder4->readFrame(12, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  success = state.decoder4->readFrame(13, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_FALSE(success);
  success = state.decoder5->readFrame(12, state.pixmap.writablePixels(), state.pixmap.rowBytes());
  EXPECT_TRUE(success);
  auto decoder6 = PAGDecoder::MakeFrom(state.pagFile2, 0, 0.5f);
  EXPECT_TRUE(decoder6 == nullptr);
  auto decoder7 = PAGDecoder::MakeFrom(state.pagFile2, 20, 0.f);
  EXPECT_TRUE(decoder7 == nullptr);
}

static void CheckDecoderFileLifetimes(DecoderCacheState& state, const std::string& cacheDir) {
  DiskIOWorker::GetInstance()->waitAll();
  auto files = Directory::FindFiles(cacheDir + "/files", ".bin");
  auto diskFileCount = files.size();
  state.decoder = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  files = Directory::FindFiles(cacheDir + "/files", ".bin");
  EXPECT_EQ(files.size(), diskFileCount);
  state.decoder2 = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  files = Directory::FindFiles(cacheDir + "/files", ".bin");
  EXPECT_EQ(files.size(), diskFileCount - 1);
  state.decoder3 = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  files = Directory::FindFiles(cacheDir + "/files", ".bin");
  EXPECT_EQ(files.size(), diskFileCount - 1);
  state.decoder4 = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  files = Directory::FindFiles(cacheDir + "/files", ".bin");
  EXPECT_EQ(files.size(), diskFileCount - 2);
  state.decoder5 = nullptr;
  DiskIOWorker::GetInstance()->waitAll();
  files = Directory::FindFiles(cacheDir + "/files", ".bin");
  EXPECT_EQ(files.size(), diskFileCount - 3);
}
}  // namespace

PAG_PIXEL_TEST(PAGDiskCacheTest, PAGDecoder) {
  auto cacheDir = Platform::Current()->getCacheDir();
  ScopedDiskCacheCleanup cleanup;
  ASSERT_NO_FATAL_FAILURE(ResetDiskCacheFixture(cacheDir, cleanup.originalMaxDiskSize));
  DecoderCacheState state;
  ASSERT_NO_FATAL_FAILURE(CheckDecoderFrameCache(state));
  ASSERT_NO_FATAL_FAILURE(CheckDecoderReuseAndFormats(state));
  ASSERT_NO_FATAL_FAILURE(CheckDecoderCompositionChanges(state));
  ASSERT_NO_FATAL_FAILURE(CheckDecoderFileLifetimes(state, cacheDir));
}

/**
 * Regression test for the iOS dangling GL context crash: after MakeFrom captures the caller's
 * current GL context, destroying that caller context must not affect subsequent readFrame calls,
 * even when they happen on another thread. Before the fix, PAGDecoder held a raw pointer to the
 * caller's context and dereferenced it later in readFrame(), causing a use-after-free once the
 * caller context was released.
 */
#ifdef TGFX_USE_OPENGL
static void ReadFirstFrame(PAGDecoder* decoder, bool* success) {
  tgfx::Bitmap bitmap(decoder->width(), decoder->height(), false, false);
  tgfx::Pixmap pixmap(bitmap);
  *success = decoder->readFrame(0, pixmap.writablePixels(), pixmap.rowBytes());
}
#endif  // TGFX_USE_OPENGL

// GL-specific: validates that PAGDecoder::MakeFrom() captures the caller's GL context via
// GLDevice::CurrentNativeHandle() and keeps working after the caller destroys that context.
// Metal / Vulkan / D3D12 / WebGPU have no thread-local "current context" concept, so the same
// scenario does not apply; skip the case on non-GL backends.
#ifdef TGFX_USE_OPENGL
PAG_TEST(PAGDiskCacheTest, PAGDecoderContextDestroyed) {
  pag::PAGDiskCache::RemoveAll();
  auto pagFile = LoadPAGFile("resources/apitest/data_bmp.pag");
  ASSERT_TRUE(pagFile != nullptr);

  // Make a standalone GL context current on this thread so MakeFrom() captures it as the caller
  // context via GLDevice::CurrentNativeHandle().
  auto callerDevice = tgfx::GLDevice::Make();
  ASSERT_TRUE(callerDevice != nullptr);
  auto callerContext = callerDevice->lockContext();
  ASSERT_TRUE(callerContext != nullptr);
  auto decoder = PAGDecoder::MakeFrom(pagFile, 30, 0.5f);
  ASSERT_TRUE(decoder != nullptr);
  EXPECT_TRUE(decoder->sharedDevice != nullptr);

  // Destroy the caller's context; a correct decoder no longer depends on its lifetime.
  callerDevice->unlock();
  callerDevice = nullptr;

  // Read on another thread with no current GL context: must render through the self-owned
  // sharedDevice instead of crashing on the freed caller context.
  bool success = false;
  std::thread reader(ReadFirstFrame, decoder.get(), &success);
  reader.join();
  EXPECT_TRUE(success);
  EXPECT_TRUE(decoder->reader != nullptr);

  decoder = nullptr;
  pag::PAGDiskCache::RemoveAll();
}
#endif  // TGFX_USE_OPENGL

PAG_PIXEL_TEST(PAGDiskCacheTest, PAGDecoder_StaticTimeRanges) {
  pag::PAGDiskCache::RemoveAll();
  auto pagFile = LoadPAGFile("resources/apitest/polygon.pag");
  ASSERT_TRUE(pagFile != nullptr);
  auto decoder = PAGDecoder::MakeFrom(pagFile);
  ASSERT_TRUE(decoder != nullptr);
  pagFile = nullptr;
  tgfx::Bitmap bitmap(decoder->width(), decoder->height(), false, false);
  tgfx::Pixmap pixmap(bitmap);
  auto success = decoder->readFrame(5, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_polygon_5"));
  EXPECT_TRUE(decoder->sequenceFile != nullptr);
  EXPECT_TRUE(decoder->sequenceFile->isComplete());
  EXPECT_TRUE(decoder->reader == nullptr);
  EXPECT_TRUE(decoder->getComposition() == nullptr);
  success = decoder->readFrame(55, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_polygon_5"));

  pagFile = LoadPAGFile("resources/apitest/ImageDecodeTest.pag");
  ASSERT_TRUE(pagFile != nullptr);
  decoder = PAGDecoder::MakeFrom(pagFile, 24.0f);
  ASSERT_TRUE(decoder != nullptr);
  EXPECT_EQ(decoder->frameRate(), 24.0f);
  pixmap.reset();
  bitmap.allocPixels(decoder->width(), decoder->height(), false, false);
  pixmap.reset(bitmap);
  success = decoder->readFrame(0, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_Image_0"));
  EXPECT_FALSE(decoder->sequenceFile->isComplete());
  EXPECT_EQ(decoder->sequenceFile->staticTimeRanges().size(), 5u);
  EXPECT_EQ(decoder->sequenceFile->cachedFrames, 8);
  EXPECT_FALSE(decoder->checkFrameChanged(5));
  EXPECT_FALSE(decoder->checkFrameChanged(7));
  EXPECT_TRUE(decoder->checkFrameChanged(8));
  success = decoder->readFrame(11, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_Image_11"));
  EXPECT_EQ(decoder->sequenceFile->cachedFrames, 12);
  EXPECT_FALSE(decoder->checkFrameChanged(8));
  EXPECT_FALSE(decoder->checkFrameChanged(10));
  EXPECT_TRUE(decoder->checkFrameChanged(0));
  pagFile->removeAllLayers();
  EXPECT_TRUE(decoder->checkFrameChanged(11));
  EXPECT_TRUE(decoder->checkFrameChanged(8));
  EXPECT_TRUE(decoder->checkFrameChanged(10));
  decoder = nullptr;

  pagFile = LoadPAGFile("resources/apitest/ImageDecodeTest.pag");
  ASSERT_TRUE(pagFile != nullptr);
  decoder = PAGDecoder::MakeFrom(pagFile, 24.0f);
  ASSERT_TRUE(decoder != nullptr);
  pagFile = nullptr;
  success = decoder->readFrame(7, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_Image_0"));
  EXPECT_FALSE(decoder->sequenceFile->isComplete());
  EXPECT_EQ(decoder->sequenceFile->cachedFrames, 12);
  success = decoder->readFrame(8, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_Image_11"));
  EXPECT_EQ(decoder->sequenceFile->cachedFrames, 12);
  success = decoder->readFrame(15, pixmap.writablePixels(), pixmap.rowBytes());
  EXPECT_TRUE(success);
  EXPECT_TRUE(Baseline::Compare(pixmap, "PAGDiskCacheTest/decoder_Image_15"));
  EXPECT_EQ(decoder->sequenceFile->cachedFrames, 28);
  decoder = nullptr;
  pag::PAGDiskCache::RemoveAll();
}

PAG_TEST(PAGDiskCacheTest, FileCache) {
  pag::PAGDiskCache::RemoveAll();
  auto data = ReadFile("resources/apitest/polygon.pag");
  ASSERT_TRUE(data != nullptr);
  std::string cacheKey = "https://pag.io/resources/apitest/polygon.pag";
  auto success = DiskCache::WriteFile(cacheKey, data);
  EXPECT_TRUE(success);
  auto cacheData = DiskCache::ReadFile(cacheKey);
  ASSERT_TRUE(cacheData != nullptr);
  EXPECT_EQ(data->size(), cacheData->size());
  EXPECT_TRUE(memcmp(data->bytes(), cacheData->bytes(), data->size()) == 0);
  pag::PAGDiskCache::RemoveAll();
}

/**
 * Tests for DiskIOWorker: serial execution order and waitAll behavior.
 */
PAG_TEST(PAGDiskCacheTest, DiskIOWorker) {
  auto worker = DiskIOWorker::GetInstance();

  // Test 1: Verify serial execution order
  std::vector<int> executionOrder;
  std::mutex orderMutex;
  for (int i = 0; i < 5; i++) {
    worker->submit([i, &executionOrder, &orderMutex]() {
      std::lock_guard<std::mutex> lock(orderMutex);
      executionOrder.push_back(i);
    });
  }
  worker->waitAll();
  ASSERT_EQ(executionOrder.size(), 5u);
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(executionOrder[i], i);
  }

  // Test 2: Verify waitAll blocks until all tasks complete
  std::atomic<int> counter{0};
  for (int i = 0; i < 10; i++) {
    worker->submit([&counter]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      counter++;
    });
  }
  // Before waitAll, counter may not be 10 yet
  worker->waitAll();
  // After waitAll, all tasks must be complete
  EXPECT_EQ(counter.load(), 10);

  // Test 3: Verify nullptr task is safely ignored
  worker->submit(nullptr);
  worker->waitAll();  // Should not crash

  // Test 4: Verify concurrent submit from multiple threads
  counter = 0;
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; i++) {
    threads.emplace_back([worker, &counter]() {
      for (int j = 0; j < 25; j++) {
        worker->submit([&counter]() { counter++; });
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  worker->waitAll();
  EXPECT_EQ(counter.load(), 100);
}

/**
 * Concurrency stress test around issue #3684. It repeatedly creates, uses, and destroys independent
 * PAGDecoders on multiple threads (each thread owns its own PAGFile and decoder, running the
 * teardown chain ~PAGDecoder -> ~CompositionReader -> ~PAGPlayer -> ~VideoReader) while the shared
 * disk cache is cleared concurrently. Note that this does NOT deterministically reproduce the
 * intra-decoder teardown race: within a single thread the decoder is created, used, and destroyed
 * sequentially, so a teardown never overlaps an in-flight readFrame() on the same decoder. It
 * exercises the destructor locking and the shared DiskCache paths under concurrent load, guarding
 * against regressions of #3684 without pinning down the exact race window.
 */
PAG_TEST(PAGDiskCacheTest, PAGDecoderTeardownRace) {
  pag::PAGDiskCache::RemoveAll();
  std::vector<std::shared_ptr<PAGFile>> pagFiles;
  for (int i = 0; i < 4; i++) {
    auto pagFile = LoadPAGFile("resources/apitest/video_sequence_size.pag");
    ASSERT_TRUE(pagFile != nullptr);
    pagFiles.push_back(pagFile);
  }

  std::atomic<bool> stopped = {false};
  std::thread cacheCleaner([&] {
    while (!stopped.load()) {
      PAGDiskCache::RemoveAll();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  });

  std::atomic<int> successFrames = {0};
  std::vector<std::thread> readers;
  for (int i = 0; i < 4; i++) {
    readers.emplace_back([&, i] {
      for (int round = 0; round < 3; round++) {
        auto decoder = PAGDecoder::MakeFrom(pagFiles[i], 60, 1.0f);
        if (decoder == nullptr) {
          continue;
        }
        tgfx::Bitmap bitmap(decoder->width(), decoder->height(), false, false);
        tgfx::Pixmap pixmap(bitmap);
        auto frameCount = std::min(decoder->numFrames(), 10);
        for (int frame = 0; frame < frameCount; frame++) {
          if (decoder->readFrame(frame, pixmap.writablePixels(), pixmap.rowBytes())) {
            successFrames++;
          }
        }
      }
    });
  }
  for (auto& reader : readers) {
    reader.join();
  }
  stopped = true;
  cacheCleaner.join();

  EXPECT_GT(successFrames.load(), 0);
  pag::PAGDiskCache::RemoveAll();
}

}  // namespace pag
