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

#include "rendering/caches/RenderCache.h"
#include "rendering/drawables/Drawable.h"
#include "rendering/graphics/Graphic.h"
#include "tgfx/gpu/Context.h"
#include "utils/TestUtils.h"

#if defined(PAG_TEST_ACCESS_PRIVATE) && !defined(PAG_WEBGL_ONLY)
namespace pag {
namespace {
class RejectedSequenceGraphic final : public Graphic {
 public:
  void measureBounds(tgfx::Rect* bounds) const override {
    *bounds = tgfx::Rect::MakeWH(16, 16);
  }

  bool hitTest(RenderCache*, float, float) override {
    return false;
  }

  GraphicType type() const override {
    return GraphicType::Unknown;
  }

  bool getPath(tgfx::Path*) const override {
    return false;
  }

  void prepare(RenderCache*) const override {
  }

  void draw(Canvas* canvas) const override {
    auto result = std::make_shared<SequenceReadResult>();
    result->status.store(SequenceReadStatus::Failed);
    canvas->getCache()->usedStaticSequences[1] = std::move(result);
    tgfx::Paint paint = {};
    paint.setColor(tgfx::Color::Red());
    canvas->drawRect(tgfx::Rect::MakeWH(16, 16), paint);
  }
};
}  // namespace

PAG_TEST(FrameDeliveryFenceTest, RejectedNonWindowSubmissionRetainsCallerOwnedSemaphore) {
  auto surface = PAGSurface::MakeOffscreen(16, 16);
  ASSERT_NE(surface, nullptr);
  auto device = surface->drawable->getDevice();
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    if (scope.context()->backend() != tgfx::Backend::OpenGL) {
      GTEST_SKIP() << "The public BackendSemaphore currently only wraps GL sync objects.";
    }
  }
  PAGPlayer player = {};
  player.setSurface(surface);
  auto graphic = std::make_shared<RejectedSequenceGraphic>();
  player.renderCache->beginFrame();
  BackendSemaphore semaphore = {};
  EXPECT_FALSE(surface->draw(player.renderCache, graphic, &semaphore, true));
  EXPECT_TRUE(player.renderCache->hasSequenceFrameUnavailable());
  ASSERT_TRUE(semaphore.isInitialized());
  EXPECT_TRUE(surface->wait(semaphore));
  EXPECT_TRUE(surface->waitUntilCompleted());
}
}  // namespace pag
#endif
