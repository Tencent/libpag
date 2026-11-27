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

#include <array>
#include <deque>
#include "gpu/proxies/RenderTargetProxy.h"
#include "gpu/resources/TextureView.h"
#include "platform/ImageStream.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/drawables/SurfaceDrawable.h"
#include "rendering/graphics/Graphic.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/platform/ImageReader.h"
#include "utils/TestUtils.h"

#ifdef PAG_TEST_ACCESS_PRIVATE
namespace pag {
namespace {
class SubmissionStream final : public tgfx::ImageStream {
 public:
  SubmissionStream() : ImageStream(1, 1) {
  }

  const std::shared_ptr<tgfx::ColorSpace>& colorSpace() const override {
    return space;
  }

  void push(uint8_t identity, int64_t timestamp) {
    frames.push_back({identity, timestamp});
  }

  int reads = 0;
  bool invalidateWhenEmpty = false;

 protected:
  ReadResult onReadTexture(tgfx::Context* context, bool mipmapped,
                           std::shared_ptr<tgfx::TextureView> texture, int64_t timestamp) override {
    reads++;
    if (frames.empty()) {
      if (invalidateWhenEmpty) {
        hasSource = false;
        return {nullptr, tgfx::ImageBufferStatus::Failed, true};
      }
      auto status =
          hasSource && texture != nullptr && (timestamp < 0 || timestamp == deliveredTimestamp)
              ? tgfx::ImageBufferStatus::Ready
              : tgfx::ImageBufferStatus::Timeout;
      return {std::move(texture), status, false};
    }
    auto frame = frames.front();
    frames.pop_front();
    uint8_t pixels[] = {frame.identity, 0, 0, 255};
    texture = tgfx::TextureView::MakeRGBA(context, 1, 1, pixels, sizeof(pixels), mipmapped);
    hasSource = texture != nullptr;
    if (!hasSource) {
      return {nullptr, tgfx::ImageBufferStatus::Failed, true};
    }
    deliveredTimestamp = frame.timestamp;
    auto status = timestamp < 0 || frame.timestamp == timestamp ? tgfx::ImageBufferStatus::Ready
                                                                : tgfx::ImageBufferStatus::Timeout;
    return {std::move(texture), status, true};
  }

 private:
  struct SourceFrame {
    uint8_t identity;
    int64_t timestamp;
  };
  std::deque<SourceFrame> frames = {};
  bool hasSource = false;
  int64_t deliveredTimestamp = -1;
  std::shared_ptr<tgfx::ColorSpace> space = nullptr;
};

/** Uses a persistent GPU backbuffer and counts the automatic Window presentation callback. */
class SubmissionWindow final : public tgfx::Window {
 public:
  SubmissionWindow(std::shared_ptr<tgfx::Device> device, tgfx::ImageOrigin origin,
                   std::shared_ptr<tgfx::ColorSpace> colorSpace)
      : Window(std::move(device), std::move(colorSpace)), origin(origin) {
  }

  void resize(int size) {
    dimension = size;
    target = nullptr;
  }

  int size() const {
    return dimension;
  }

  int creations = 0;
  int swaps = 0;
  int64_t nextTimestamp = -1;
  int64_t presentedTimestamp = -1;
  std::shared_ptr<tgfx::RenderTargetProxy> target = nullptr;

 protected:
  std::shared_ptr<tgfx::RenderTargetProxy> onCreateRenderTarget(tgfx::Context* context) override {
    creations++;
    if (target == nullptr) {
      target =
          tgfx::RenderTargetProxy::Make(context, dimension, dimension, false, 1, false, origin);
    }
    return target;
  }

  void onPresent(tgfx::Context*) override {
    swaps++;
    presentedTimestamp = nextTimestamp;
  }

 private:
  int dimension = 16;
  tgfx::ImageOrigin origin;
};

class SubmissionDrawable final : public SurfaceDrawable {
 public:
  explicit SubmissionDrawable(std::shared_ptr<SubmissionWindow> window)
      : window(std::move(window)) {
  }

  int width() const override {
    return window->size();
  }

  int height() const override {
    return window->size();
  }

  std::shared_ptr<tgfx::Device> getDevice() override {
    return window->getDevice();
  }

  void setTimeStamp(int64_t timestamp) override {
    currentTimestamp = timestamp;
  }

  bool preparePresent(tgfx::Context* context) override {
    if (!SurfaceDrawable::preparePresent(context)) {
      return false;
    }
    window->nextTimestamp = currentTimestamp;
    return true;
  }

 protected:
  std::shared_ptr<tgfx::Window> getWindow() override {
    return window;
  }

 private:
  std::shared_ptr<SubmissionWindow> window;
  int64_t currentTimestamp = -1;
};

class SubmissionGraphic final : public Graphic {
 public:
  SubmissionGraphic(std::shared_ptr<SequenceReadResult> result, std::shared_ptr<tgfx::Image> image)
      : result(std::move(result)), image(std::move(image)) {
  }

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
    canvas->getCache()->usedStaticSequences[1] = result;
    tgfx::Paint paint = {};
    paint.setColor(tgfx::Color::Green());
    canvas->drawRect(tgfx::Rect::MakeWH(16, 16), paint);
    if (image != nullptr) {
      canvas->drawImage(image, tgfx::SamplingOptions(tgfx::FilterMode::Nearest));
    }
  }

 private:
  std::shared_ptr<SequenceReadResult> result;
  std::shared_ptr<tgfx::Image> image;
};

class SubmissionRig {
 public:
  SubmissionRig(std::shared_ptr<tgfx::Device> device,
                tgfx::ImageOrigin origin = tgfx::ImageOrigin::TopLeft,
                std::shared_ptr<tgfx::ColorSpace> colorSpace = nullptr)
      : device(std::move(device)),
        window(std::make_shared<SubmissionWindow>(this->device, origin, std::move(colorSpace))),
        drawable(std::make_shared<SubmissionDrawable>(window)),
        surface(PAGSurface::MakeFrom(drawable)), stream(std::make_shared<SubmissionStream>()),
        reader(tgfx::ImageReader::MakeFrom(stream)) {
    player.setSurface(surface);
  }

  void prepareFrame(int64_t timestamp, SequenceReadStatus status = SequenceReadStatus::Succeeded) {
    buffer = reader->acquireNextBuffer(timestamp);
    auto frame = std::make_shared<FrameRead>();
    frame->buffer = buffer;
    frame->options->targetTimestamp = timestamp;
    frame->verifiedTargetTimestamp = timestamp;
    result = std::make_shared<SequenceReadResult>();
    result->setFrameRead(std::move(frame));
    result->status.store(status);
    graphic = std::make_shared<SubmissionGraphic>(result, tgfx::Image::MakeFrom(buffer));
  }

  bool draw(BackendSemaphore* semaphore = nullptr, bool autoClear = false) {
    player.renderCache->beginFrame();
    return surface->draw(player.renderCache, graphic, semaphore, autoClear);
  }

  void expectPixel(uint8_t red, uint8_t green, uint8_t alpha, int y = 0, int x = 0) {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    ASSERT_NE(window->target, nullptr);
    auto texture = window->target->getTextureView();
    ASSERT_NE(texture, nullptr);
    auto readable =
        tgfx::Surface::MakeFrom(scope.context(), texture->getBackendTexture(), texture->origin());
    ASSERT_NE(readable, nullptr);
    std::array<uint8_t, 4> pixels = {};
    auto info =
        tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
    ASSERT_TRUE(readable->readPixels(info, pixels.data(), x, y));
    EXPECT_EQ(pixels[0], red);
    EXPECT_EQ(pixels[1], green);
    EXPECT_EQ(pixels[3], alpha);
  }

  std::shared_ptr<tgfx::Device> device;
  std::shared_ptr<SubmissionWindow> window;
  std::shared_ptr<SubmissionDrawable> drawable;
  std::shared_ptr<PAGSurface> surface;
  std::shared_ptr<SubmissionStream> stream;
  std::shared_ptr<tgfx::ImageReader> reader;
  std::shared_ptr<tgfx::ImageBuffer> buffer = nullptr;
  std::shared_ptr<SequenceReadResult> result = nullptr;
  std::shared_ptr<Graphic> graphic = nullptr;
  PAGPlayer player = {};
};
}  // namespace

PAG_TEST(WindowSubmissionTest, EmptySourceCannotSwapEvenAfterStagingSubmit) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  SubmissionRig rig(device);
  rig.player.setComposition(PAGComposition::Make(16, 16));
  ASSERT_NE(rig.player.renderCache->getContentVersion(), rig.surface->contentVersion);
  auto contentVersion = rig.surface->contentVersion;
  rig.prepareFrame(1000);
  EXPECT_EQ(rig.buffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_EQ(rig.stream->reads, 0);
  BackendSemaphore semaphore = {};
  EXPECT_FALSE(rig.draw(&semaphore, true));
  EXPECT_EQ(rig.surface->contentVersion, contentVersion);
  EXPECT_EQ(rig.stream->reads, 1);
  EXPECT_EQ(rig.buffer->status(), tgfx::ImageBufferStatus::Timeout);
  EXPECT_TRUE(rig.player.renderCache->hasSequenceFrameUnavailable());
  EXPECT_EQ(rig.window->creations, 0);
  EXPECT_EQ(rig.window->swaps, 0);
  EXPECT_FALSE(semaphore.isInitialized());
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto staging = rig.drawable->getSurface(scope.context(), true);
    ASSERT_NE(staging, nullptr);
    uint8_t pixels[4] = {};
    auto info =
        tgfx::ImageInfo::Make(1, 1, tgfx::ColorType::RGBA_8888, tgfx::AlphaType::Premultiplied);
    ASSERT_TRUE(staging->readPixels(info, pixels));
    EXPECT_EQ(pixels[1], 255);
    EXPECT_EQ(pixels[3], 255);
    EXPECT_EQ(rig.window->creations, 0);
    EXPECT_EQ(rig.window->swaps, 0);
  }
  rig.stream->push(180, 1000);
  rig.prepareFrame(1000);
  ASSERT_TRUE(rig.draw(nullptr, true));
  EXPECT_EQ(rig.surface->contentVersion, rig.player.renderCache->getContentVersion());
  EXPECT_EQ(rig.window->swaps, 1);
  EXPECT_FALSE(rig.draw(nullptr, true));
  EXPECT_EQ(rig.stream->reads, 2);
  EXPECT_EQ(rig.window->swaps, 1);
}

PAG_TEST(WindowSubmissionTest, WrongTimestampAndInvalidatedSourcePreserveWindowPixels) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  SubmissionRig rig(device);
  rig.stream->push(180, 1000);
  rig.prepareFrame(1000);
  ASSERT_TRUE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 1);
  rig.expectPixel(180, 0, 255);
  rig.stream->push(45, 1000);
  rig.prepareFrame(2000);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.buffer->status(), tgfx::ImageBufferStatus::Timeout);
  EXPECT_EQ(rig.result->getFrameRead()->verifiedTargetTimestamp, 2000);
  EXPECT_EQ(rig.window->swaps, 1);
  rig.expectPixel(180, 0, 255);
  rig.stream->invalidateWhenEmpty = true;
  rig.prepareFrame(2000);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.buffer->status(), tgfx::ImageBufferStatus::Failed);
  EXPECT_EQ(rig.window->swaps, 1);
  rig.expectPixel(180, 0, 255);
  rig.stream->push(90, 2000);
  rig.prepareFrame(2000);
  ASSERT_TRUE(rig.draw());
  EXPECT_EQ(rig.stream->reads, 4);
  EXPECT_EQ(rig.window->swaps, 2);
  rig.expectPixel(90, 0, 255);
}

PAG_TEST(WindowSubmissionTest, StaleGenerationAndFailedPreparationCannotSwap) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  SubmissionRig rig(device);
  rig.stream->push(180, 1000);
  rig.prepareFrame(1000);
  auto frame = rig.result->getFrameRead();
  frame->sourceGeneration = std::make_shared<std::atomic<uint64_t>>(2);
  frame->generation = 1;
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.stream->reads, 1);
  EXPECT_EQ(rig.window->swaps, 0);
  rig.stream->push(90, 2000);
  rig.prepareFrame(2000, SequenceReadStatus::Failed);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.stream->reads, 2);
  EXPECT_TRUE(rig.player.renderCache->hasSequenceDecodeFailure());
  EXPECT_EQ(rig.window->swaps, 0);
  rig.stream->push(45, 3000);
  rig.prepareFrame(3000);
  auto newerBuffer = rig.reader->acquireNextBuffer(4000);
  EXPECT_EQ(newerBuffer->status(), tgfx::ImageBufferStatus::Pending);
  EXPECT_EQ(rig.buffer->status(), tgfx::ImageBufferStatus::Expired);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.stream->reads, 2);
  EXPECT_EQ(rig.window->creations, 0);
  EXPECT_EQ(rig.window->swaps, 0);
}

PAG_TEST(WindowSubmissionTest, ValidFallbackCanPublishButStrictOrMismatchedFallbackCannot) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  SubmissionRig rig(device);
  rig.stream->push(180, 1000);
  rig.prepareFrame(-1, SequenceReadStatus::Fallback);
  ASSERT_TRUE(rig.draw());
  EXPECT_TRUE(rig.player.renderCache->hasSequenceDecodeFailure());
  EXPECT_FALSE(rig.player.renderCache->hasSequenceFrameUnavailable());
  EXPECT_EQ(rig.window->swaps, 1);
  rig.player.renderCache->setSequenceFrameWaitTime(30000);
  rig.prepareFrame(-1, SequenceReadStatus::Fallback);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 1);
  rig.player.renderCache->setSequenceFrameWaitTime(0);
  rig.prepareFrame(2000, SequenceReadStatus::Fallback);
  EXPECT_FALSE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 1);
  rig.expectPixel(180, 0, 255);
}

PAG_TEST(WindowSubmissionTest, AcceptedCopySignalsAndPreservesOriginAndColorSpace) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  auto colorSpace = tgfx::ColorSpace::SRGB();
  SubmissionRig rig(device, tgfx::ImageOrigin::BottomLeft, colorSpace);
  rig.stream->push(180, 1000);
  rig.prepareFrame(1000);
  BackendSemaphore semaphore = {};
  ASSERT_TRUE(rig.draw(&semaphore));
  EXPECT_EQ(rig.window->swaps, 1);
  EXPECT_EQ(rig.window->presentedTimestamp, rig.player.getTimeStampInternal());
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
#ifdef PAG_WEBGL_ONLY
    EXPECT_FALSE(semaphore.isInitialized());
#else
    EXPECT_EQ(semaphore.isInitialized(), scope.context()->backend() == tgfx::Backend::OpenGL);
#endif
  }
  if (semaphore.isInitialized()) {
    EXPECT_TRUE(rig.surface->wait(semaphore));
  }
  rig.expectPixel(180, 0, 255);
  rig.expectPixel(0, 255, 255, 15);
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto staging = rig.drawable->getSurface(scope.context(), true);
    ASSERT_NE(staging, nullptr);
    EXPECT_EQ(staging->colorSpace(), colorSpace);
    EXPECT_EQ(staging->width(), 16);
    EXPECT_EQ(staging->height(), 16);
  }
  ASSERT_TRUE(rig.surface->clearAll());
  EXPECT_EQ(rig.window->swaps, 2);
  EXPECT_EQ(rig.window->presentedTimestamp, 0);
  rig.expectPixel(0, 0, 0);
}

PAG_TEST(WindowSubmissionTest, ResizeScalesAcceptedPixelsOverTheEntireCurrentWindow) {
  auto device = Devices::MakeDefault();
  ASSERT_NE(device, nullptr);
  SubmissionRig rig(device);
  rig.stream->push(180, 1000);
  rig.prepareFrame(1000);
  ASSERT_TRUE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 1);
  rig.window->resize(32);
  rig.stream->push(90, 2000);
  rig.prepareFrame(2000);
  ASSERT_TRUE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 2);
  rig.expectPixel(90, 0, 255);
  rig.expectPixel(90, 0, 255, 1, 1);
  rig.expectPixel(0, 255, 255, 31, 31);
  {
    DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    auto staging = rig.drawable->getSurface(scope.context(), true);
    ASSERT_NE(staging, nullptr);
    EXPECT_EQ(staging->width(), 16);
    EXPECT_EQ(staging->height(), 16);
  }
  rig.surface->updateSize();
  rig.stream->push(90, 2000);
  rig.prepareFrame(2000);
  ASSERT_TRUE(rig.draw());
  EXPECT_EQ(rig.window->swaps, 3);
  DeviceLockScope scope(device);
  ASSERT_TRUE(static_cast<bool>(scope));
  auto staging = rig.drawable->getSurface(scope.context(), true);
  ASSERT_NE(staging, nullptr);
  EXPECT_EQ(staging->width(), 32);
  EXPECT_EQ(staging->height(), 32);
}
}  // namespace pag
#endif
