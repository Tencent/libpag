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

#include <functional>
#include <string>
#include "base/PAGTest.h"
#include "gtest/gtest-spi.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Task.h"
#include "utils/Baseline.h"

namespace pag {
namespace {
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

PAG_PIXEL_TEST(BaselineRegistrationTest, RegisteredCaseIsRecognizedBeforeComparison) {
  EXPECT_TRUE(PAGTest::IsPixelComparison());
  EXPECT_TRUE(Baseline::CheckPixelComparison("BaselineRegistrationTest/registered"));
}

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

CLI_TEST(BaselineCliRegistrationTest, UnmarkedCommandDoesNotRegisterPixels) {
  EXPECT_FALSE(PAGTest::IsPixelComparison());
}

CLI_PIXEL_TEST(BaselineCliRegistrationTest, RegisteredCommandIsRecognizedBeforeComparison) {
  EXPECT_TRUE(PAGTest::IsPixelComparison());
  EXPECT_TRUE(Baseline::CheckPixelComparison("BaselineCliRegistrationTest/registered"));
}

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
