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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include "base/PAGTest.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/Surface.h"
#include "tgfx/core/Typeface.h"
#include "tgfx/gpu/Context.h"
#include "utils/MetalTestSupport.h"
#include "utils/ProjectPath.h"

namespace pag {
/**
 * Verifies that test-only atlas resets isolate pixel comparisons from earlier text rendering.
 */
PAG_TEST(PAGMetalAtlasTest, ResetRemovesPreviousTextHistory) {
  constexpr int width = 512;
  constexpr int height = 256;
  auto typeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_TRUE(typeface != nullptr);
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  tgfx::Paint paint;
  paint.setColor(tgfx::Color::White());
  auto info = tgfx::ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied);
  std::vector<uint8_t> expected;
  for (int warmupCount : {0, 1, 3, 7, 15}) {
    SCOPED_TRACE(warmupCount);
    ASSERT_TRUE(ResetMetalAtlasForComparison());
    {
      DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      auto surface = tgfx::Surface::Make(scope.context(), width, height);
      ASSERT_TRUE(surface != nullptr);
      for (int index = 0; index < warmupCount; index++) {
        surface->getCanvas()->clear();
        surface->getCanvas()->drawSimpleText("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0, 100,
                                             tgfx::Font(typeface, 11.0f + index), paint);
        scope.context()->flushAndSubmit(true);
      }
    }
    ASSERT_TRUE(ResetMetalAtlasForComparison());
    std::vector<uint8_t> pixels(info.byteSize());
    {
      DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      auto surface = tgfx::Surface::Make(scope.context(), width, height);
      ASSERT_TRUE(surface != nullptr);
      auto canvas = surface->getCanvas();
      canvas->clear();
      canvas->translate(20.25f, 25.75f);
      canvas->rotate(7.0f);
      canvas->drawSimpleText("图集位置一致性 TextAtlas", 0, 90, tgfx::Font(typeface, 32.0f), paint);
      ASSERT_TRUE(surface->readPixels(info, pixels.data()));
    }
    if (expected.empty()) {
      expected = pixels;
    } else {
      size_t changedBytes = 0;
      for (size_t i = 0; i < pixels.size(); i++) {
        changedBytes += pixels[i] != expected[i];
      }
      EXPECT_EQ(changedBytes, 0u);
    }
  }
}

/**
 * Reuses cached glyphs across surfaces and intervening draws without clearing atlas history.
 */
PAG_TEST(PAGMetalAtlasTest, ReusesTextWithoutReset) {
  constexpr int width = 512;
  constexpr int height = 256;
  auto typeface =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_TRUE(typeface != nullptr);
  auto device = Devices::MakeDefault();
  ASSERT_TRUE(device != nullptr);
  tgfx::Paint paint;
  paint.setColor(tgfx::Color::White());
  auto info = tgfx::ImageInfo::Make(width, height, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied);
  std::vector<uint8_t> expected;
  for (int iteration = 0; iteration < 4; iteration++) {
    SCOPED_TRACE(iteration);
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto surface = tgfx::Surface::Make(scope.context(), width, height);
    ASSERT_TRUE(surface != nullptr);
    auto canvas = surface->getCanvas();
    canvas->clear();
    canvas->drawSimpleText("图集复用 TextAtlas", 20, 90, tgfx::Font(typeface, 32.0f), paint);
    std::vector<uint8_t> pixels(info.byteSize());
    ASSERT_TRUE(surface->readPixels(info, pixels.data()));
    if (expected.empty()) {
      expected = pixels;
    } else {
      size_t changedBytes = 0;
      for (size_t i = 0; i < pixels.size(); i++) {
        changedBytes += pixels[i] != expected[i];
      }
      EXPECT_EQ(changedBytes, 0u);
    }
    canvas->clear();
    canvas->drawSimpleText("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 0, 100,
                           tgfx::Font(typeface, 11.0f + iteration), paint);
    scope.context()->flushAndSubmit(true);
  }
}
}  // namespace pag
