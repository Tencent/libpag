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

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include "base/PAGTest.h"
#include "gtest/gtest-spi.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Task.h"
#include "utils/Baseline.h"
#include "utils/TestDir.h"

namespace pag {
namespace {
#if !defined(UPDATE_BASELINE) && defined(PAG_TEST_ACCESS_PRIVATE)
class BaselineCacheDirectory {
 public:
  explicit BaselineCacheDirectory(const std::string& name)
      : rootPath(TestDir::GetRoot() + "/out/BaselineRegistrationTest/" + name),
        baselinePath(rootPath + "/baseline"), cachePath(rootPath + "/cache"),
        outputPath(rootPath + "/output") {
  }

  ~BaselineCacheDirectory() {
    std::error_code error;
    std::filesystem::remove_all(rootPath, error);
    EXPECT_FALSE(error) << error.message();
  }

  /** Recreates an isolated cache fixture without touching the real baseline cache. */
  bool prepare() {
    std::error_code error;
    std::filesystem::remove_all(rootPath, error);
    if (error) {
      return false;
    }
    std::filesystem::create_directories(baselinePath, error);
    if (error) {
      return false;
    }
    std::filesystem::create_directories(cachePath, error);
    return !error;
  }

  const std::string rootPath;
  const std::string baselinePath;
  const std::string cachePath;
  const std::string outputPath;
};

static bool WriteBaselineTestFile(const std::string& path, const std::string& contents) {
  std::ofstream file(path);
  file << contents;
  file.close();
  return !file.fail();
}

static std::string ReadBaselineTestFile(const std::string& path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
#endif

#if defined(TGFX_USE_METAL)
static void ExpectRegistrationFailure(const std::function<bool()>& compare) {
  testing::TestPartResultArray failures;
  bool result = true;
  {
    testing::ScopedFakeTestPartResultReporter reporter(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &failures);
    result = compare();
  }
  EXPECT_FALSE(result);
  ASSERT_EQ(failures.size(), 1);
  EXPECT_EQ(failures.GetTestPartResult(0).type(), testing::TestPartResult::kNonFatalFailure);
  EXPECT_NE(std::string(failures.GetTestPartResult(0).message())
                .find("Metal pixel comparison requires a pixel-test declaration"),
            std::string::npos);
}
#endif
}  // namespace

PAG_TEST(BaselineRegistrationTest, UnmarkedCaseDoesNotRegisterPixels) {
  EXPECT_FALSE(PAGTest::IsPixelComparison());
}

#ifdef PAG_TEST_ACCESS_PRIVATE
PAG_TEST(BaselineRegistrationTest, NonGpuExceptionsAreLimitedToTheirKeys) {
  EXPECT_TRUE(Baseline::IsNonGpuPixelComparison("PAGImageTest", "image2", "PAGImageTest/image2"));
  EXPECT_FALSE(Baseline::IsNonGpuPixelComparison("PAGImageTest", "image2", "PAGImageTest/image"));
  EXPECT_FALSE(Baseline::IsNonGpuPixelComparison("PAGImageTest", "image3", "PAGImageTest/image2"));
  EXPECT_TRUE(Baseline::IsNonGpuPixelComparison("PAGXHtmlTest", "HtmlScreenshotCompare",
                                                "PAGXTest/html/sample"));
  EXPECT_FALSE(Baseline::IsNonGpuPixelComparison("PAGXHtmlTest", "HtmlScreenshotCompare",
                                                 "PAGXCliTest/RenderText"));
  EXPECT_FALSE(
      Baseline::IsNonGpuPixelComparison("PAGXCliTest", "Render_Text", "PAGXTest/html/sample"));
}
#endif

#ifdef PAG_TEST_ACCESS_PRIVATE
PAG_PIXEL_TEST(BaselineRegistrationTest, RegisteredCaseIsRecognizedBeforeComparison) {
  EXPECT_TRUE(PAGTest::IsPixelComparison());
  EXPECT_TRUE(Baseline::CheckPixelComparison("BaselineRegistrationTest/registered"));
}
#endif

#ifdef PAG_TEST_ACCESS_PRIVATE
PAG_PIXEL_TEST(BaselineRegistrationTest, RegisteredCaseIsRecognizedInTask) {
  bool recognized = false;
  auto task = tgfx::Task::Run([&recognized] {
    recognized = PAGTest::IsPixelComparison() &&
                 Baseline::CheckPixelComparison("BaselineRegistrationTest/registered-task", true);
  });
  ASSERT_NE(task, nullptr);
  task->wait();
  EXPECT_TRUE(recognized);
}
#endif

CLI_TEST(BaselineCliRegistrationTest, UnmarkedCommandDoesNotRegisterPixels) {
  EXPECT_FALSE(PAGTest::IsPixelComparison());
}

#ifdef PAG_TEST_ACCESS_PRIVATE
CLI_PIXEL_TEST(BaselineCliRegistrationTest, RegisteredCommandIsRecognizedBeforeComparison) {
  EXPECT_TRUE(PAGTest::IsPixelComparison());
  EXPECT_TRUE(Baseline::CheckPixelComparison("BaselineCliRegistrationTest/registered"));
}
#endif

#if !defined(UPDATE_BASELINE) && defined(PAG_TEST_ACCESS_PRIVATE)
PAG_TEST(BaselineRegistrationTest, CountsComparisonsWithoutChangingExpectedValues) {
  Baseline instance("/missing/baseline", "/missing/cache", "/missing/output");
  instance.baselineVersions = {{"suite", {{"image", "v1"}}}};
  instance.cacheVersions = instance.baselineVersions;
  instance.cacheMD5 = {{"suite", {{"image", "expected"}}}};
  auto expected = instance.cacheMD5;
  EXPECT_TRUE(instance.compareMd5("suite/image", "expected"));
  EXPECT_FALSE(instance.compareMd5("suite/image", "actual"));
  EXPECT_EQ(instance.cacheMD5, expected);
  auto stats = instance.comparisonStats();
  EXPECT_EQ(stats.compared, 2u);
  EXPECT_EQ(stats.versionSkipped, 0u);
  EXPECT_EQ(stats.missingBaseline, 0u);
}

PAG_TEST(BaselineRegistrationTest, CountsAcceptedVersionChangesSeparately) {
  Baseline instance("/missing/baseline", "/missing/cache", "/missing/output");
  instance.baselineVersions = {{"suite", {{"image", "v2"}}}};
  instance.cacheVersions = {{"suite", {{"image", "v1"}}}};
  instance.cacheMD5 = {{"suite", {{"image", "expected"}}}};
  EXPECT_TRUE(instance.compareMd5("suite/image", "actual"));
  auto stats = instance.comparisonStats();
  EXPECT_EQ(stats.compared, 0u);
  EXPECT_EQ(stats.versionSkipped, 1u);
  EXPECT_EQ(stats.missingBaseline, 0u);
}

PAG_TEST(BaselineRegistrationTest, RejectsMissingRegisteredBaseline) {
  Baseline instance("/missing/baseline", "/missing/cache", "/missing/output");
  EXPECT_FALSE(instance.compareMd5("suite/image", "actual"));
  EXPECT_EQ(instance.comparisonStats().missingBaseline, 1u);
}

PAG_TEST(BaselineRegistrationTest, MissingCacheIsNotAnAcceptedWebVersionChange) {
  Baseline instance("/missing/baseline", "/missing/cache", "/missing/output");
  instance.baselineVersions = {{"suite", {{"image", "v2"}}}};
#if defined(__EMSCRIPTEN__)
  EXPECT_FALSE(instance.compareMd5("suite/image", "actual"));
  EXPECT_EQ(instance.comparisonStats().missingBaseline, 1u);
#else
  EXPECT_TRUE(instance.compareMd5("suite/image", "actual"));
  EXPECT_EQ(instance.comparisonStats().versionSkipped, 1u);
#endif
  instance.cacheVersions = instance.baselineVersions;
  EXPECT_FALSE(instance.compareMd5("suite/image", "actual"));
  EXPECT_EQ(instance.comparisonStats().compared, 0u);
}

PAG_TEST(BaselineRegistrationTest, CachePublicationRecordsActualToolchain) {
  BaselineCacheDirectory directory("CachePublicationRecordsActualToolchain");
  ASSERT_TRUE(directory.prepare());
  const std::string versions = "{\"suite\":{\"image\":\"v1\"}}\n";
  ASSERT_TRUE(WriteBaselineTestFile(directory.baselinePath + "/version.json", versions));
  Baseline instance(directory.baselinePath, directory.cachePath, directory.outputPath);
  instance.outputMD5 = {{"suite", {{"image", "new-digest"}}}};
  ASSERT_TRUE(WriteBaselineTestFile(instance.cacheToolchainPath, "previous-toolchain Release\n"));

  ASSERT_TRUE(instance.writeCacheData());
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheMD5Path), instance.outputMD5.dump(4) + "\n");
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheVersionPath), versions);
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheToolchainPath),
            std::string(PAG_BASELINE_TOOLCHAIN) + "\n");
  EXPECT_FALSE(std::filesystem::exists(instance.cacheToolchainPath + ".tmp"));
}

PAG_TEST(BaselineRegistrationTest, CachePublicationFailureInvalidatesOldToolchain) {
  BaselineCacheDirectory directory("CachePublicationFailureInvalidatesOldToolchain");
  ASSERT_TRUE(directory.prepare());
  const std::string versions = "{\"suite\":{\"image\":\"v1\"}}\n";
  ASSERT_TRUE(WriteBaselineTestFile(directory.baselinePath + "/version.json", versions));
  Baseline instance(directory.baselinePath, directory.cachePath, directory.outputPath);
  instance.outputMD5 = {{"suite", {{"image", "new-digest"}}}};
  ASSERT_TRUE(WriteBaselineTestFile(instance.cacheToolchainPath, "previous-toolchain Release\n"));
  std::error_code error;
  std::filesystem::create_directory(instance.cacheToolchainPath + ".tmp", error);
  ASSERT_FALSE(error);

  EXPECT_FALSE(instance.writeCacheData());
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheMD5Path), instance.outputMD5.dump(4) + "\n");
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheVersionPath), versions);
  EXPECT_FALSE(std::filesystem::exists(instance.cacheToolchainPath));
}

PAG_TEST(BaselineRegistrationTest, CachePublicationStopsWhenToolchainCannotBeInvalidated) {
  BaselineCacheDirectory directory("CachePublicationStopsWhenToolchainCannotBeInvalidated");
  ASSERT_TRUE(directory.prepare());
  const std::string oldMD5 = "{\"suite\":{\"image\":\"old-digest\"}}\n";
  ASSERT_TRUE(WriteBaselineTestFile(directory.cachePath + "/md5.json", oldMD5));
  Baseline instance(directory.baselinePath, directory.cachePath, directory.outputPath);
  instance.outputMD5 = {{"suite", {{"image", "new-digest"}}}};
  std::error_code error;
  std::filesystem::create_directory(instance.cacheToolchainPath, error);
  ASSERT_FALSE(error);
  ASSERT_TRUE(WriteBaselineTestFile(instance.cacheToolchainPath + "/occupied", "occupied"));

  EXPECT_FALSE(instance.writeCacheData());
  EXPECT_EQ(ReadBaselineTestFile(instance.cacheMD5Path), oldMD5);
}
#endif

#if defined(TGFX_USE_METAL)
PAG_TEST(BaselineRegistrationTest, RejectsUnregisteredPixmapBeforeMd5) {
  tgfx::Bitmap bitmap(1, 1, false, false);
  tgfx::Pixmap pixmap(bitmap);
  ASSERT_FALSE(pixmap.isEmpty());
  ExpectRegistrationFailure([&pixmap] {
    return Baseline::Compare(pixmap, "BaselineRegistrationTest/unregistered-pixmap");
  });
}

PAG_TEST(BaselineRegistrationTest, RejectsUnregisteredBitmapBeforeMd5) {
  tgfx::Bitmap bitmap(1, 1, false, false);
  ASSERT_FALSE(bitmap.isEmpty());
  ExpectRegistrationFailure([&bitmap] {
    return Baseline::Compare(bitmap, "BaselineRegistrationTest/unregistered-bitmap");
  });
}

PAG_TEST(BaselineRegistrationTest, RejectsUnregisteredSurfacesBeforeReadback) {
  ExpectRegistrationFailure([] {
    return Baseline::Compare(std::shared_ptr<tgfx::Surface>{},
                             "BaselineRegistrationTest/unregistered-tgfx-surface");
  });
  ExpectRegistrationFailure([] {
    return Baseline::Compare(std::shared_ptr<PAGSurface>{},
                             "BaselineRegistrationTest/unregistered-pag-surface");
  });
  ExpectRegistrationFailure([] {
    return Baseline::Compare(std::shared_ptr<pagx::PAGSurface>{},
                             "BaselineRegistrationTest/unregistered-pagx-surface");
  });
}

PAG_TEST(BaselineRegistrationTest, RejectsUnregisteredAsyncSnapshot) {
  tgfx::Bitmap bitmap(1, 1, false, false);
  ASSERT_FALSE(bitmap.isEmpty());
  auto task = tgfx::Task::Run([bitmap] {
    ExpectRegistrationFailure([&bitmap] {
      return Baseline::Compare(bitmap, "BaselineRegistrationTest/unregistered-task");
    });
  });
  ASSERT_NE(task, nullptr);
  task->wait();
}

CLI_TEST(BaselineCliRegistrationTest, RejectsUnregisteredCommandPixels) {
  tgfx::Bitmap bitmap(1, 1, false, false);
  tgfx::Pixmap pixmap(bitmap);
  ASSERT_FALSE(pixmap.isEmpty());
  ExpectRegistrationFailure(
      [&pixmap] { return Baseline::Compare(pixmap, "BaselineCliRegistrationTest/unregistered"); });
}
#endif
}  // namespace pag
