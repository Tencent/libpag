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

#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/VideoExporter.h"
#include "pagx/nodes/Animation.h"
#include "pagx/runtime/Drawable.h"
#include "pagx/tgfx.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/core/Surface.h"
#include "utils/DevicePool.h"
#include "utils/TestUtils.h"

#ifdef TGFX_USE_OPENGL
#if defined(PAG_USE_SWIFTSHADER) || defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif
#endif

namespace pag {
namespace {

std::shared_ptr<pagx::PAGXDocument> MakeStaticDocument() {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer>\n"
      "    <Layer id=\"static\">\n"
      "      <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "      <Fill color=\"#FF0000\"/>\n"
      "    </Layer>\n"
      "  </Layer>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeTranslucentDocument() {
  return pagx::PAGXImporter::FromXML(
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer>\n"
      "    <Rectangle position=\"100,100\" size=\"200,200\"/>\n"
      "    <Fill color=\"#FF0000\" alpha=\"0.5\"/>\n"
      "  </Layer>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeNestedAnimationDocument() {
  return pagx::PAGXImporter::FromXML(
      "<pagx width=\"60\" height=\"60\">\n"
      "  <Resources>\n"
      "    <Composition id=\"card\" width=\"60\" height=\"60\">\n"
      "      <Layer>\n"
      "        <Rectangle position=\"30,30\" size=\"60,60\"/>\n"
      "        <Fill><SolidColor id=\"cardColor\" color=\"#00FF00\"/></Fill>\n"
      "      </Layer>\n"
      "      <Animations>\n"
      "        <Animation id=\"cardShift\" duration=\"20\" frameRate=\"60\" loop=\"once\">\n"
      "          <Object target=\"cardColor\">\n"
      "            <Channel name=\"color\" type=\"color\">\n"
      "              <Key time=\"0\" value=\"#00FF00\" interpolation=\"hold\"/>\n"
      "              <Key time=\"10\" value=\"#FF0000\" interpolation=\"hold\"/>\n"
      "            </Channel>\n"
      "          </Object>\n"
      "        </Animation>\n"
      "      </Animations>\n"
      "    </Composition>\n"
      "  </Resources>\n"
      "  <Layer width=\"60\" height=\"60\" composition=\"@card\">\n"
      "    <Timelines><Animation ref=\"@cardShift\"/></Timelines>\n"
      "  </Layer>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeAnimatedDocument(const std::string& loop,
                                                         const std::string& frameRate = "10") {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"animated\">\n"
      "    <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "    <Fill color=\"#FF0000\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"animation\" duration=\"10\" frameRate=\"" +
      frameRate + "\" loop=\"" + loop +
      "\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"x\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"100\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n");
}

std::shared_ptr<pagx::PAGXDocument> MakeMultiAnimationDocument(const std::string& firstFrameRate,
                                                               const std::string& secondFrameRate) {
  return pagx::PAGXImporter::FromXML(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<pagx width=\"200\" height=\"200\">\n"
      "  <Layer id=\"animated\">\n"
      "    <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
      "    <Fill color=\"#FF0000\"/>\n"
      "  </Layer>\n"
      "  <Animations>\n"
      "    <Animation id=\"first\" duration=\"10\" frameRate=\"" +
      firstFrameRate +
      "\" loop=\"once\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"x\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"100\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "    <Animation id=\"second\" duration=\"10\" frameRate=\"" +
      secondFrameRate +
      "\" loop=\"once\">\n"
      "      <Object target=\"animated\">\n"
      "        <Channel name=\"y\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"10\" value=\"50\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n"
      "</pagx>\n");
}

void SetDocumentFrameRate(const std::shared_ptr<pagx::PAGXDocument>& document, float frameRate) {
  auto* animation = static_cast<pagx::Animation*>(document->animations.front());
  animation->frameRate = frameRate;
}

std::shared_ptr<pagx::PAGSurface> MakeHostSurface(
    int width, int height, tgfx::ColorType colorType = tgfx::ColorType::RGBA_8888,
    std::shared_ptr<tgfx::ColorSpace> colorSpace = nullptr) {
  auto device = Devices::MakeDefault();
  if (device == nullptr) {
    return nullptr;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface =
      tgfx::Surface::Make(context, width, height, colorType, 1, false, 0, std::move(colorSpace));
  device->unlock();
  return pagx::MakeFrom(device, surface);
}

class ObservedDrawable : public pagx::Drawable {
 public:
  ObservedDrawable(std::shared_ptr<tgfx::Device> device, std::shared_ptr<tgfx::Surface> surface,
                   std::shared_ptr<int> calls, int failOnCall = -1)
      : Drawable(std::move(device), std::move(surface)), calls(std::move(calls)),
        failOnCall(failOnCall) {
  }

  std::shared_ptr<tgfx::Surface> getSurface(tgfx::Context* context) override {
    auto call = (*calls)++;
    return call == failOnCall ? nullptr : Drawable::getSurface(context);
  }

 private:
  std::shared_ptr<int> calls = nullptr;
  int failOnCall = -1;
};

std::shared_ptr<pagx::PAGSurface> MakeObservedHostSurface(int width, int height,
                                                          std::shared_ptr<int> calls,
                                                          int failOnCall = -1) {
  auto device = Devices::MakeDefault();
  if (device == nullptr) {
    return nullptr;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return nullptr;
  }
  auto surface = tgfx::Surface::Make(context, width, height);
  device->unlock();
  return pagx::PAGSurface::MakeFrom(
      std::make_shared<ObservedDrawable>(device, surface, std::move(calls), failOnCall));
}

bool WaitAsyncFrame(const std::shared_ptr<pagx::VideoExportFrame>& frame) {
  if (frame == nullptr) {
    return false;
  }
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (frame->status() == pagx::VideoExportFrameStatus::Pending &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return frame->status() == pagx::VideoExportFrameStatus::Ready;
}

bool ConsumeAsyncFrame(const std::shared_ptr<pagx::VideoExportFrame>& frame, int width, int height,
                       std::vector<uint8_t>* pixels) {
  if (!WaitAsyncFrame(frame)) {
    return false;
  }
  auto rowBytes = static_cast<size_t>(width) * 4 + 16;
  pixels->assign(rowBytes * static_cast<size_t>(height), 0);
  return frame->readPixels(pixels->data(), rowBytes);
}

}  // namespace

/**
 * Test case: a null document is rejected and the caller gets a message to show.
 */
PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNullDocument) {
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(nullptr, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_FALSE(errorMsg.empty());
}

/**
 * Test case: the documented 1-120 frame rate range is enforced instead of producing a broken
 * sequence, and the rejection happens before any scene or GPU resource is created.
 */
PAGX_TEST(PAGXVideoExporterTest, MakeRejectsOutOfRangeFrameRate) {
  auto document = MakeStaticDocument();
  ASSERT_TRUE(document != nullptr);
  pagx::VideoExportOptions options = {};
  options.frameRate = 240;
  std::string errorMsg = {};
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("frame rate"), std::string::npos);

  options.frameRate = -1;
  errorMsg.clear();
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("frame rate"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeExportsStaticDocumentAsSingleFrameByDefault) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeStaticDocument());
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 60);
  EXPECT_EQ(exporter->frameCount(), 1);
  ASSERT_TRUE(exporter->nextFrame());
  EXPECT_NE(exporter->pixels(), nullptr);
  EXPECT_EQ(exporter->currentFrame(), 1);
  EXPECT_FALSE(exporter->nextFrame());
}

PAGX_TEST(PAGXVideoExporterTest, RenderModeDefaultsToPartialAndSupportsDirect) {
  auto document = MakeStaticDocument();
  ASSERT_TRUE(document != nullptr);
  auto defaultExporter = pagx::VideoExporter::Make(document);
  ASSERT_TRUE(defaultExporter != nullptr);
  EXPECT_EQ(defaultExporter->renderMode(), pagx::PAGRenderMode::Partial);

  pagx::VideoExportOptions options = {};
  options.renderMode = pagx::PAGRenderMode::Direct;
  auto surface = MakeHostSurface(200, 200);
  ASSERT_TRUE(surface != nullptr);
  auto directExporter = pagx::VideoExporter::MakeWithSurface(document, std::move(surface), options);
  ASSERT_TRUE(directExporter != nullptr);
  EXPECT_EQ(directExporter->renderMode(), pagx::PAGRenderMode::Direct);
}

PAGX_TEST(PAGXVideoExporterTest, MakeWithSurfaceRejectsInvalidInput) {
  auto document = MakeStaticDocument();
  std::string errorMsg = {};
  EXPECT_EQ(pagx::VideoExporter::MakeWithSurface(document, nullptr, {}, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("surface is null"), std::string::npos);

  auto surface = MakeHostSurface(100, 100);
  ASSERT_TRUE(surface != nullptr);
  errorMsg.clear();
  EXPECT_EQ(pagx::VideoExporter::MakeWithSurface(document, surface, {}, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("dimensions"), std::string::npos);
  EXPECT_NE(errorMsg.find("200x200"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeWithSurfaceKeepsTheHostSurfaceAlive) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter synchronous readback is not used in this WebGPU test lane";
#endif
  auto surface = MakeHostSurface(200, 200);
  ASSERT_TRUE(surface != nullptr);
  auto exporter =
      pagx::VideoExporter::MakeWithSurface(MakeStaticDocument(), std::move(surface), {}, nullptr);
  ASSERT_TRUE(exporter != nullptr);
  ASSERT_TRUE(exporter->nextFrame());
  EXPECT_EQ(exporter->currentFrame(), 1);
  EXPECT_NE(exporter->pixels(), nullptr);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRepeatsStaticDocumentForExplicitDuration) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 2.0f;
  auto exporter = pagx::VideoExporter::Make(MakeStaticDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 30);
  ASSERT_EQ(exporter->frameCount(), 60);
  ASSERT_TRUE(exporter->nextFrame());
  auto frameBytes = exporter->rowBytes() * static_cast<size_t>(exporter->height());
  std::vector<uint8_t> firstFrame(exporter->pixels(), exporter->pixels() + frameBytes);
  while (exporter->nextFrame()) {
    ASSERT_EQ(std::memcmp(firstFrame.data(), exporter->pixels(), frameBytes), 0);
  }
  EXPECT_EQ(exporter->currentFrame(), 60);
}

PAGX_TEST(PAGXVideoExporterTest, DoesNotReuseFramesForNestedAnimations) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 60;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(MakeNestedAnimationDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  auto centerOffset = static_cast<size_t>(30) * exporter->rowBytes() + 30 * 4;
  ASSERT_TRUE(exporter->nextFrame());
  EXPECT_EQ(exporter->pixels()[centerOffset + 1], 255);  // Green initially.
  for (int frame = 1; frame <= 10; ++frame) {
    ASSERT_TRUE(exporter->nextFrame());
  }
  EXPECT_EQ(exporter->pixels()[centerOffset], 255);  // Red after frame 10.
  EXPECT_EQ(exporter->pixels()[centerOffset + 1], 0);
}

PAGX_TEST(PAGXVideoExporterTest, AsyncFrameCommitsOnlyAfterConsumption) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  ASSERT_TRUE(started.frame != nullptr);
  EXPECT_EQ(started.frame->index(), 0);
  EXPECT_EQ(exporter->currentFrame(), 0);

  auto busy = exporter->startNextFrame();
  EXPECT_EQ(busy.status, pagx::VideoExportStartStatus::Busy);
  EXPECT_EQ(busy.frame, nullptr);
  EXPECT_EQ(exporter->currentFrame(), 0);

  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(started.frame, exporter->width(), exporter->height(), &pixels));
  EXPECT_EQ(exporter->currentFrame(), 1);
  EXPECT_EQ(started.frame->status(), pagx::VideoExportFrameStatus::Ready);
  auto pixelsBeforeRepeatedRead = pixels;
  EXPECT_FALSE(started.frame->readPixels(pixels.data(), pixels.size() / exporter->height()));
  EXPECT_EQ(pixels, pixelsBeforeRepeatedRead);
  EXPECT_EQ(exporter->currentFrame(), 1);

  auto second = exporter->startNextFrame();
  ASSERT_EQ(second.status, pagx::VideoExportStartStatus::FrameStarted);
  EXPECT_EQ(second.frame->index(), 1);
  EXPECT_EQ(exporter->currentFrame(), 1);
  exporter->cancel();
  EXPECT_EQ(exporter->currentFrame(), 1);
  EXPECT_EQ(second.frame->status(), pagx::VideoExportFrameStatus::Failed);
  EXPECT_EQ(second.frame->error(), "cancelled");
  EXPECT_FALSE(second.frame->readPixels(pixels.data(), pixels.size() / exporter->height()));
  auto cancelled = exporter->startNextFrame();
  EXPECT_EQ(cancelled.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(cancelled.error, "cancelled");
}

PAGX_TEST(PAGXVideoExporterTest, MakeWithSurfaceSupportsAsyncReadback) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter =
      pagx::VideoExporter::MakeWithSurface(MakeAnimatedDocument("once"), MakeHostSurface(200, 200));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted) << started.error;
  ASSERT_TRUE(started.frame != nullptr);
  EXPECT_EQ(started.frame->index(), 0);
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(started.frame, exporter->width(), exporter->height(), &pixels));
  EXPECT_EQ(exporter->currentFrame(), 1);
}

PAGX_TEST(PAGXVideoExporterTest, HostSurfacePreservesPixelsAcrossFormatsAndColorSpaces) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  struct HostCase {
    tgfx::ColorType colorType;
    std::shared_ptr<tgfx::ColorSpace> colorSpace;
  };
  const HostCase cases[] = {
      {tgfx::ColorType::BGRA_8888, nullptr},
      {tgfx::ColorType::RGBA_8888, tgfx::ColorSpace::SRGB()},
      {tgfx::ColorType::RGBA_8888, tgfx::ColorSpace::SRGBLinear()},
  };
  for (const auto& hostCase : cases) {
    auto surface = MakeHostSurface(200, 200, hostCase.colorType, hostCase.colorSpace);
    ASSERT_TRUE(surface != nullptr);
    auto exporter = pagx::VideoExporter::MakeWithSurface(MakeStaticDocument(), std::move(surface));
    ASSERT_TRUE(exporter != nullptr);
    auto started = exporter->startNextFrame();
    ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted) << started.error;
    std::vector<uint8_t> pixels = {};
    ASSERT_TRUE(ConsumeAsyncFrame(started.frame, 200, 200, &pixels)) << started.frame->error();
    auto rowBytes = static_cast<size_t>(200) * 4 + 16;
    auto redOffset = 50 * rowBytes + 100 * 4;
    EXPECT_EQ(pixels[redOffset], 255);
    EXPECT_EQ(pixels[redOffset + 1], 0);
    EXPECT_EQ(pixels[redOffset + 2], 0);
    EXPECT_EQ(pixels[redOffset + 3], 255);
    auto transparentOffset = 150 * rowBytes + 100 * 4;
    EXPECT_EQ(pixels[transparentOffset + 3], 0);
    for (int row = 0; row < 200; ++row) {
      for (size_t byte = 200 * 4; byte < rowBytes; ++byte) {
        ASSERT_EQ(pixels[row * rowBytes + byte], 0);
      }
    }
  }
}

PAGX_TEST(PAGXVideoExporterTest, HostSurfaceOutputsPremultipliedRGBA) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter =
      pagx::VideoExporter::MakeWithSurface(MakeTranslucentDocument(), MakeHostSurface(200, 200));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted) << started.error;
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(started.frame, 200, 200, &pixels)) << started.frame->error();
  auto rowBytes = static_cast<size_t>(200) * 4 + 16;
  auto offset = 100 * rowBytes + 100 * 4;
  EXPECT_NEAR(pixels[offset], 128, 1);
  EXPECT_EQ(pixels[offset + 1], 0);
  EXPECT_EQ(pixels[offset + 2], 0);
  EXPECT_NEAR(pixels[offset + 3], 128, 1);
}

PAGX_TEST(PAGXVideoExporterTest, ReadbackCreationFailureDoesNotCommit) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto calls = std::make_shared<int>(0);
  // Construction, drawing and readback creation query the surface in that order. Fail the third
  // query so the frame reaches the actual readback creation path.
  auto surface = MakeObservedHostSurface(200, 200, calls, 2);
  ASSERT_TRUE(surface != nullptr);
  auto exporter = pagx::VideoExporter::MakeWithSurface(MakeStaticDocument(), std::move(surface));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  EXPECT_EQ(started.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(started.frame, nullptr);
  EXPECT_NE(started.error.find("readback"), std::string::npos);
  EXPECT_EQ(exporter->currentFrame(), 0);
  EXPECT_EQ(*calls, 3);
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(exporter->currentFrame(), 0);
}

PAGX_TEST(PAGXVideoExporterTest, BusyAndStaticCacheDoNotRenderAgain) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 2;
  options.duration = 1.0f;
  auto calls = std::make_shared<int>(0);
  auto exporter = pagx::VideoExporter::MakeWithSurface(
      MakeStaticDocument(), MakeObservedHostSurface(200, 200, calls), options);
  ASSERT_TRUE(exporter != nullptr);
  auto first = exporter->startNextFrame();
  ASSERT_EQ(first.status, pagx::VideoExportStartStatus::FrameStarted) << first.error;
  EXPECT_EQ(*calls, 3);  // Constructor validation, first draw, and first readback creation.
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::Busy);
  EXPECT_EQ(*calls, 3);
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(first.frame, 200, 200, &pixels));
  EXPECT_EQ(*calls, 3);

  auto second = exporter->startNextFrame();
  ASSERT_EQ(second.status, pagx::VideoExportStartStatus::FrameStarted);
  EXPECT_EQ(second.frame->status(), pagx::VideoExportFrameStatus::Ready);
  EXPECT_EQ(*calls, 3);
  ASSERT_TRUE(ConsumeAsyncFrame(second.frame, 200, 200, &pixels));
  EXPECT_EQ(*calls, 3);
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::End);
}

#ifdef TGFX_USE_OPENGL
PAGX_TEST(PAGXVideoExporterTest, HostSurfaceBottomLeftOriginKeepsTopLeftOutput) {
  auto device = DevicePool::Make();
  ASSERT_TRUE(device != nullptr);
  tgfx::GLTextureInfo textureInfo = {};
  std::shared_ptr<pagx::PAGSurface> surface = nullptr;
  {
    DeviceLockScope lock(device);
    ASSERT_TRUE(static_cast<bool>(lock));
    ASSERT_TRUE(CreateGLTexture(lock.context(), 200, 200, &textureInfo));
    surface = pagx::PAGSurface::MakeFrom(ToBackendTexture(textureInfo, 200, 200),
                                         pag::ImageOrigin::BottomLeft);
  }
  ASSERT_TRUE(surface != nullptr);
  {
    auto exporter = pagx::VideoExporter::MakeWithSurface(MakeStaticDocument(), std::move(surface));
    ASSERT_TRUE(exporter != nullptr);
    auto started = exporter->startNextFrame();
    ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted) << started.error;
    std::vector<uint8_t> pixels = {};
    ASSERT_TRUE(ConsumeAsyncFrame(started.frame, 200, 200, &pixels));
    auto rowBytes = static_cast<size_t>(200) * 4 + 16;
    EXPECT_EQ(pixels[50 * rowBytes + 100 * 4], 255);
    EXPECT_EQ(pixels[50 * rowBytes + 100 * 4 + 3], 255);
    EXPECT_EQ(pixels[150 * rowBytes + 100 * 4 + 3], 0);
  }
  DeviceLockScope lock(device);
  ASSERT_TRUE(static_cast<bool>(lock));
  glDeleteTextures(1, &textureInfo.id);
}
#endif

PAGX_TEST(PAGXVideoExporterTest, AsyncStaticFramesReuseCPUPixels) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 2;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(MakeStaticDocument(), options);
  ASSERT_TRUE(exporter != nullptr);
  ASSERT_EQ(exporter->frameCount(), 2);

  auto first = exporter->startNextFrame();
  std::vector<uint8_t> firstPixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(first.frame, exporter->width(), exporter->height(), &firstPixels));
  EXPECT_EQ(exporter->currentFrame(), 1);

  auto second = exporter->startNextFrame();
  ASSERT_EQ(second.status, pagx::VideoExportStartStatus::FrameStarted);
  EXPECT_EQ(second.frame->index(), 1);
  EXPECT_EQ(second.frame->status(), pagx::VideoExportFrameStatus::Ready);
  std::vector<uint8_t> secondPixels = {};
  ASSERT_TRUE(
      ConsumeAsyncFrame(second.frame, exporter->width(), exporter->height(), &secondPixels));
  EXPECT_EQ(secondPixels, firstPixels);
  EXPECT_EQ(exporter->currentFrame(), 2);
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::End);
}

PAGX_TEST(PAGXVideoExporterTest, AsyncAndSyncModesCannotBeMixed) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto synchronous = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(synchronous->nextFrame());
  auto mixed = synchronous->startNextFrame();
  EXPECT_EQ(mixed.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(mixed.error, "mixed_mode");

  auto asynchronous = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  auto started = asynchronous->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  EXPECT_FALSE(asynchronous->nextFrame());
  EXPECT_EQ(asynchronous->error(), "mixed_mode");
  EXPECT_EQ(asynchronous->currentFrame(), 0);
  EXPECT_EQ(started.frame->status(), pagx::VideoExportFrameStatus::Failed);
  EXPECT_EQ(started.frame->error(), "mixed_mode");
}

PAGX_TEST(PAGXVideoExporterTest, FailedSynchronousFrameDoesNotLockMode) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto calls = std::make_shared<int>(0);
  // Construction, drawing and synchronous reading query the surface in that order. The read
  // failure must not prevent the caller from restarting the sequence through the async API.
  auto surface = MakeObservedHostSurface(200, 200, calls, 2);
  auto exporter = pagx::VideoExporter::MakeWithSurface(MakeStaticDocument(), std::move(surface));
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_FALSE(exporter->nextFrame());
  EXPECT_NE(exporter->error().find("read"), std::string::npos);
  EXPECT_EQ(exporter->currentFrame(), 0);

  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted) << started.error;
  EXPECT_TRUE(exporter->error().empty());
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(started.frame, 200, 200, &pixels));
  EXPECT_EQ(exporter->currentFrame(), 1);
}

PAGX_TEST(PAGXVideoExporterTest, ReleasingAsyncFrameDoesNotConsumeIt) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  started.frame = nullptr;
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::Busy);
  EXPECT_EQ(exporter->currentFrame(), 0);
  exporter->cancel();
  auto cancelled = exporter->startNextFrame();
  EXPECT_EQ(cancelled.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(cancelled.error, "cancelled");
  EXPECT_EQ(exporter->currentFrame(), 0);
}

PAGX_TEST(PAGXVideoExporterTest, AsyncCopyFailureIsTerminalAndDoesNotCommit) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  ASSERT_TRUE(WaitAsyncFrame(started.frame));
  std::vector<uint8_t> pixels(16, 0xCD);
  EXPECT_FALSE(started.frame->readPixels(pixels.data(), 0));
  EXPECT_EQ(pixels, std::vector<uint8_t>(16, 0xCD));
  EXPECT_EQ(started.frame->status(), pagx::VideoExportFrameStatus::Failed);
  EXPECT_EQ(exporter->currentFrame(), 0);
  auto failed = exporter->startNextFrame();
  EXPECT_EQ(failed.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(failed.frame, nullptr);
  EXPECT_EQ(exporter->currentFrame(), 0);
}

PAGX_TEST(PAGXVideoExporterTest, CancelAfterFailureReleasesPendingAndPreservesError) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  ASSERT_TRUE(WaitAsyncFrame(started.frame));
  std::vector<uint8_t> pixels(16, 0);
  ASSERT_FALSE(started.frame->readPixels(pixels.data(), 0));
  auto originalError = started.frame->error();
  ASSERT_FALSE(originalError.empty());
  std::weak_ptr<pagx::VideoExportFrame> frameReference = started.frame;

  exporter->cancel();
  EXPECT_EQ(exporter->error(), originalError);
  EXPECT_EQ(started.frame->error(), originalError);
  started.frame = nullptr;
  EXPECT_TRUE(frameReference.expired());
  auto failed = exporter->startNextFrame();
  EXPECT_EQ(failed.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(failed.error, originalError);
  EXPECT_EQ(exporter->currentFrame(), 0);
}

PAGX_TEST(PAGXVideoExporterTest, AsyncDynamicSequenceEndsAfterLastFrameIsConsumed) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  pagx::VideoExportOptions options = {};
  options.frameRate = 10;
  options.duration = 0.1f;
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"), options);
  ASSERT_TRUE(exporter != nullptr);
  ASSERT_EQ(exporter->frameCount(), 1);
  auto started = exporter->startNextFrame();
  ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
  EXPECT_EQ(exporter->startNextFrame().status, pagx::VideoExportStartStatus::Busy);
  std::vector<uint8_t> pixels = {};
  ASSERT_TRUE(ConsumeAsyncFrame(started.frame, exporter->width(), exporter->height(), &pixels));
  EXPECT_EQ(exporter->currentFrame(), 1);
  auto end = exporter->startNextFrame();
  EXPECT_EQ(end.status, pagx::VideoExportStartStatus::End);
  EXPECT_EQ(end.frame, nullptr);
  EXPECT_FALSE(exporter->nextFrame());
  EXPECT_EQ(exporter->error(), "mixed_mode");
  auto mixed = exporter->startNextFrame();
  EXPECT_EQ(mixed.status, pagx::VideoExportStartStatus::Failed);
  EXPECT_EQ(mixed.error, "mixed_mode");
}

PAGX_TEST(PAGXVideoExporterTest, AsyncFrameDoesNotCreateAnOwnershipCycle) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "Native test does not pump the WebGPU event loop";
#endif
  std::weak_ptr<pagx::VideoExporter> exporterReference;
  std::weak_ptr<pagx::VideoExportFrame> frameReference;
  {
    auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
    ASSERT_TRUE(exporter != nullptr);
    auto started = exporter->startNextFrame();
    ASSERT_EQ(started.status, pagx::VideoExportStartStatus::FrameStarted);
    ASSERT_TRUE(started.frame != nullptr);
    exporterReference = exporter;
    frameReference = started.frame;
  }
  EXPECT_TRUE(exporterReference.expired());
  EXPECT_TRUE(frameReference.expired());
}

PAGX_TEST(PAGXVideoExporterTest, MakeUsesTheFullPingPongPeriod) {
  auto once = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  auto pingPong = pagx::VideoExporter::Make(MakeAnimatedDocument("pingPong"));
  ASSERT_TRUE(once != nullptr);
  ASSERT_TRUE(pingPong != nullptr);
  EXPECT_EQ(once->frameRate(), 10);
  EXPECT_EQ(once->frameCount(), 10);
  EXPECT_EQ(pingPong->frameRate(), 10);
  EXPECT_EQ(pingPong->frameCount(), 20);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNonIntegerDocumentFrameRateByDefault) {
  for (const auto& frameRate : {"29.97", "0.25"}) {
    std::string errorMsg = {};
    auto exporter =
        pagx::VideoExporter::Make(MakeAnimatedDocument("once", frameRate), {}, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("must be an integer"), std::string::npos);
  }

  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once", "29.97"), options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameRate(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRequiresConsistentDocumentFrameRatesByDefault) {
  auto consistent = pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", "60"));
  ASSERT_TRUE(consistent != nullptr);
  EXPECT_EQ(consistent->frameRate(), 60);

  for (const auto& secondFrameRate : {"29.97", "30"}) {
    std::string errorMsg = {};
    auto exporter =
        pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", secondFrameRate), {}, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("mixed frame rates"), std::string::npos);
  }

  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  auto overridden = pagx::VideoExporter::Make(MakeMultiAnimationDocument("60", "29.97"), options);
  ASSERT_TRUE(overridden != nullptr);
  EXPECT_EQ(overridden->frameRate(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsEmptyAndDuplicateAnimationIds) {
  auto emptyIdDocument = MakeMultiAnimationDocument("60", "60");
  emptyIdDocument->animations.front()->id.clear();
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(emptyIdDocument, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("non-empty ids"), std::string::npos);

  auto duplicateIdDocument = MakeMultiAnimationDocument("60", "60");
  duplicateIdDocument->animations.back()->id = duplicateIdDocument->animations.front()->id;
  errorMsg.clear();
  exporter = pagx::VideoExporter::Make(duplicateIdDocument, {}, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("duplicate top-level animation id"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsInvalidRawAnimationFrameRate) {
  for (float frameRate :
       {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), -1.0f}) {
    auto document = MakeAnimatedDocument("once");
    SetDocumentFrameRate(document, frameRate);
    pagx::VideoExportOptions options = {};
    options.frameRate = 30;
    options.duration = 1.0f;
    std::string errorMsg = {};
    auto exporter = pagx::VideoExporter::Make(document, options, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("invalid frame rate"), std::string::npos);
  }
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsOverflowingPlaybackPeriod) {
  auto document = MakeAnimatedDocument("once");
  SetDocumentFrameRate(document, std::numeric_limits<float>::min());
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 1.0f;
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(document, options, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("overflowing playback period"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeAllowsUndeclaredRateWithExplicitSamplingOptions) {
  auto document = MakeAnimatedDocument("once");
  SetDocumentFrameRate(document, 0.0f);
  pagx::VideoExportOptions options = {};
  options.frameRate = 30;
  options.duration = 1.0f;
  auto exporter = pagx::VideoExporter::Make(document, options);
  ASSERT_TRUE(exporter != nullptr);
  EXPECT_EQ(exporter->frameCount(), 30);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsNonFiniteDuration) {
  auto document = MakeAnimatedDocument("once");
  ASSERT_TRUE(document != nullptr);
  pagx::VideoExportOptions options = {};
  options.duration = std::numeric_limits<float>::infinity();
  std::string errorMsg = {};
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("finite"), std::string::npos);

  options.duration = std::numeric_limits<float>::quiet_NaN();
  errorMsg.clear();
  EXPECT_EQ(pagx::VideoExporter::Make(document, options, &errorMsg), nullptr);
  EXPECT_NE(errorMsg.find("finite"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsInvalidScale) {
  for (float scale : {std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(), 0.0f, -1.0f}) {
    pagx::VideoExportOptions options = {};
    options.scale = scale;
    std::string errorMsg = {};
    auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"), options, &errorMsg);
    EXPECT_EQ(exporter, nullptr);
    EXPECT_NE(errorMsg.find("scale"), std::string::npos);
  }
}

PAGX_TEST(PAGXVideoExporterTest, MakeRejectsExcessiveOutputDimensions) {
  pagx::VideoExportOptions options = {};
  options.scale = std::numeric_limits<float>::max();
  std::string errorMsg = {};
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"), options, &errorMsg);
  EXPECT_EQ(exporter, nullptr);
  EXPECT_NE(errorMsg.find("dimensions"), std::string::npos);
}

PAGX_TEST(PAGXVideoExporterTest, NextFrameAdvancesSequentiallyAndStopsAtEnd) {
#ifdef TGFX_USE_WEBGPU
  GTEST_SKIP() << "VideoExporter currently requires synchronous pixel readback";
#endif
  auto exporter = pagx::VideoExporter::Make(MakeAnimatedDocument("once"));
  ASSERT_TRUE(exporter != nullptr);
  for (int64_t index = 0; index < exporter->frameCount(); ++index) {
    EXPECT_EQ(exporter->currentFrame(), index);
    ASSERT_TRUE(exporter->nextFrame());
    EXPECT_EQ(exporter->currentFrame(), index + 1);
    ASSERT_TRUE(exporter->pixels() != nullptr);
    EXPECT_GE(exporter->rowBytes(), static_cast<size_t>(exporter->width()) * 4);
  }
  EXPECT_FALSE(exporter->nextFrame());
  EXPECT_EQ(exporter->currentFrame(), exporter->frameCount());
}

}  // namespace pag
