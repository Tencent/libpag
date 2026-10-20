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
//  License is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__) && defined(TGFX_USE_METAL)

#import <TargetConditionals.h>
#include <array>
#include "base/PAGTest.h"
#include "pag/pag.h"
#include "platform/cocoa/private/metal/MetalLayerDrawable.h"
#include "platform/cocoa/private/metal/MetalLayerHelper.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/gpu/metal/MetalDevice.h"

#if TARGET_OS_IPHONE
#import "platform/ios/PAGSurface+Metal.h"
#import "platform/ios/PAGView.h"
#import "platform/ios/private/PAGSurface+Internal.h"
#import "platform/ios/private/PAGSurfaceImpl+Internal.h"
#else
#import "platform/mac/PAGSurface+Metal.h"
#import "platform/mac/private/PAGSurface+Internal.h"
#import "platform/mac/private/PAGSurfaceImpl+Internal.h"
#endif

namespace {

static constexpr int SURFACE_SIZE = 16;
using TestPixels = std::array<uint8_t, SURFACE_SIZE * SURFACE_SIZE * 4>;

static std::shared_ptr<pag::PAGPlayer> MakeTestPlayer(
    PAGSurface* surface, const std::shared_ptr<pag::PAGSolidLayer>& solid) {
  auto composition = pag::PAGComposition::Make(SURFACE_SIZE, SURFACE_SIZE);
  if (composition == nullptr || solid == nullptr || !composition->addLayer(solid)) {
    return nullptr;
  }
  auto player = std::make_shared<pag::PAGPlayer>();
  player->setSurface([surface.surface pagSurface]);
  player->setComposition(composition);
  return player;
}

static bool PixelMatches(const uint8_t* pixels, pag::Color color) {
  return pixels[0] == color.blue && pixels[1] == color.green && pixels[2] == color.red &&
         pixels[3] == 255;
}

static bool SnapshotMatches(CVPixelBufferRef snapshot, pag::Color color) {
  if (snapshot == nil ||
      CVPixelBufferLockBaseAddress(snapshot, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess) {
    return false;
  }
  auto pixels = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(snapshot));
  auto rowBytes = CVPixelBufferGetBytesPerRow(snapshot);
  bool matches = pixels != nullptr && CVPixelBufferGetWidth(snapshot) == SURFACE_SIZE &&
                 CVPixelBufferGetHeight(snapshot) == SURFACE_SIZE &&
                 CVPixelBufferGetPixelFormatType(snapshot) == kCVPixelFormatType_32BGRA &&
                 rowBytes >= SURFACE_SIZE * 4;
  for (int y = 0; matches && y < SURFACE_SIZE; y++) {
    for (int x = 0; matches && x < SURFACE_SIZE; x++) {
      matches = PixelMatches(pixels + y * rowBytes + x * 4, color);
    }
  }
  auto status = CVPixelBufferUnlockBaseAddress(snapshot, kCVPixelBufferLock_ReadOnly);
  return matches && status == kCVReturnSuccess;
}

static void CheckWindowReadback(BOOL framebufferOnly) {
  auto device = pag::Devices::MakeDefault();
  if (device == nullptr) {
    GTEST_SKIP() << "Metal device unavailable";
  }
  auto layer = [CAMetalLayer layer];
  pag::cocoa::SetUpPAGMetalLayer(layer);
  EXPECT_TRUE(layer.framebufferOnly);
  layer.framebufferOnly = framebufferOnly;
  pag::cocoa::UpdateMetalLayerDrawableSize(layer, CGSizeMake(SURFACE_SIZE, SURFACE_SIZE), 1);
  auto surface = [PAGSurface FromLayer:layer];
  ASSERT_TRUE(surface != nil);
  auto nativeSurface = [surface.surface pagSurface];
  ASSERT_TRUE(nativeSurface != nullptr);
  auto drawable = std::static_pointer_cast<pag::MetalLayerDrawable>(nativeSurface->drawable);
  ASSERT_TRUE(drawable->window != nullptr);
  EXPECT_EQ(drawable->window->supportsReadback(), framebufferOnly == NO);

  TestPixels pixels = {};
  pixels.fill(91);
  auto originalPixels = pixels;
  if (framebufferOnly) {
    {
      pag::DeviceLockScope scope(drawable->getDevice());
      ASSERT_TRUE(static_cast<bool>(scope));
      auto gpuSurface = drawable->getSurface(scope.context(), false);
      ASSERT_TRUE(gpuSurface != nullptr);
      gpuSurface->getCanvas()->clear(tgfx::Color::Red());
    }
    EXPECT_FALSE(nativeSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                           pixels.data(), SURFACE_SIZE * 4));
    EXPECT_TRUE([surface makeSnapshot] == nil);
    EXPECT_EQ(pixels, originalPixels);
  }

  auto solid = pag::PAGSolidLayer::Make(1000000, SURFACE_SIZE, SURFACE_SIZE, pag::Red);
  auto player = MakeTestPlayer(surface, solid);
  ASSERT_TRUE(player != nullptr);
  const pag::Color colors[] = {pag::Red, pag::Green, pag::Blue, pag::White, pag::Black, pag::Red};
  for (const auto& color : colors) {
    SCOPED_TRACE(static_cast<int>(color.red));
    solid->setSolidColor(color);
    ASSERT_TRUE(player->flush());
    EXPECT_FALSE(nativeSurface->readPixels(pag::ColorType::BGRA_8888, pag::AlphaType::Premultiplied,
                                           pixels.data(), SURFACE_SIZE * 4));
    EXPECT_TRUE([surface makeSnapshot] == nil);
    EXPECT_TRUE([surface getCVPixelBuffer] == nil);
#if TARGET_OS_IPHONE
    EXPECT_FALSE([surface copyPixelsTo:pixels.data() rowBytes:SURFACE_SIZE * 4]);
#endif
    EXPECT_EQ(pixels, originalPixels);
  }
}

}  // namespace

/**
 * Verifies that framebuffer-only layer surfaces reject readback before and after submission while
 * continuing to render more frames than the drawable pool can hold.
 */
PAG_TEST(PAGMetalSurfaceTest, FramebufferOnlyWindowStillRenders) {
  @autoreleasepool {
    CheckWindowReadback(YES);
  }
}

/**
 * Verifies that allowing copies from a layer does not allow snapshots after PAGPlayer has flushed
 * and submitted the frame. Rejected readback must not overwrite the caller's pixels.
 */
PAG_TEST(PAGMetalSurfaceTest, WindowReadbackAfterFlushIsRejected) {
  @autoreleasepool {
    CheckWindowReadback(NO);
  }
}

/**
 * Verifies that the recommended offscreen surface retains each submitted frame for repeated
 * snapshots and pixel readback without requiring a new drawing batch.
 */
PAG_TEST(PAGMetalSurfaceTest, OffscreenReadbackAfterFlush) {
  @autoreleasepool {
    auto device = pag::Devices::MakeDefault();
    if (device == nullptr) {
      GTEST_SKIP() << "Metal device unavailable";
    }
    auto surface = [PAGSurface MakeOffscreen:CGSizeMake(SURFACE_SIZE, SURFACE_SIZE)];
    ASSERT_TRUE(surface != nil);
    auto nativeSurface = [surface.surface pagSurface];
    ASSERT_TRUE(nativeSurface != nullptr);
    auto solid = pag::PAGSolidLayer::Make(1000000, SURFACE_SIZE, SURFACE_SIZE, pag::Red);
    auto player = MakeTestPlayer(surface, solid);
    ASSERT_TRUE(player != nullptr);
    const pag::Color colors[] = {pag::Red, pag::Green, pag::Blue};
    for (const auto& color : colors) {
      solid->setSolidColor(color);
      ASSERT_TRUE(player->flush());
      EXPECT_TRUE(SnapshotMatches([surface makeSnapshot], color));
      EXPECT_TRUE(SnapshotMatches([surface makeSnapshot], color));
      TestPixels pixels = {};
      ASSERT_TRUE(nativeSurface->readPixels(pag::ColorType::BGRA_8888,
                                            pag::AlphaType::Premultiplied, pixels.data(),
                                            SURFACE_SIZE * 4));
#if TARGET_OS_IPHONE
      pixels.fill(0);
      ASSERT_TRUE([surface copyPixelsTo:pixels.data() rowBytes:SURFACE_SIZE * 4]);
#endif
      for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        EXPECT_TRUE(PixelMatches(pixels.data() + offset, color));
      }
    }
  }
}

/**
 * Verifies that textures and render targets resolve to the owning Metal device and that captured
 * device identities remain compatible with the same device.
 */
PAG_TEST(PAGMetalSurfaceTest, ExternalTextureDeviceResolution) {
  @autoreleasepool {
    auto device = pag::Devices::MakeDefault();
    ASSERT_TRUE(device != nullptr);
    auto metalDevice = std::static_pointer_cast<tgfx::MetalDevice>(device);
    auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                       width:SURFACE_SIZE
                                                                      height:SURFACE_SIZE
                                                                   mipmapped:NO];
    id<MTLTexture> texture =
        [[metalDevice->metalDevice() newTextureWithDescriptor:descriptor] autorelease];
    ASSERT_TRUE(texture != nil);
    tgfx::MetalTextureInfo info = {};
    info.texture = (__bridge const void*)texture;
    info.format = static_cast<unsigned>(texture.pixelFormat);
    tgfx::BackendTexture backendTexture(info, SURFACE_SIZE, SURFACE_SIZE);
    tgfx::BackendRenderTarget renderTarget(info, SURFACE_SIZE, SURFACE_SIZE);
    EXPECT_EQ(pag::Devices::MakeForTexture(backendTexture), device);
    EXPECT_EQ(pag::Devices::MakeForTexture(renderTarget), device);
    auto identity = pag::Devices::CaptureFromTexture(backendTexture);
    ASSERT_TRUE(identity != nullptr);
    pag::DeviceLockScope scope(device);
    ASSERT_TRUE(static_cast<bool>(scope));
    EXPECT_TRUE(pag::Devices::CanSampleFrom(scope.context(), identity.get()));
  }
}

/**
 * Verifies that device resolution and identity capture reject invalid descriptions, null Metal
 * texture handles, and resources tagged with a different backend.
 */
PAG_TEST(PAGMetalSurfaceTest, ExternalTextureDeviceResolutionRejectsInvalidResources) {
  tgfx::MetalTextureInfo metalInfo = {};
  tgfx::GLTextureInfo glInfo = {};
  glInfo.id = 1;
  const tgfx::BackendTexture textures[] = {
      tgfx::BackendTexture(), tgfx::BackendTexture(metalInfo, SURFACE_SIZE, SURFACE_SIZE),
      tgfx::BackendTexture(glInfo, SURFACE_SIZE, SURFACE_SIZE)};
  for (const auto& texture : textures) {
    EXPECT_TRUE(pag::Devices::MakeForTexture(texture) == nullptr);
    EXPECT_TRUE(pag::Devices::CaptureFromTexture(texture) == nullptr);
  }
  tgfx::GLFrameBufferInfo glFramebufferInfo = {};
  glFramebufferInfo.id = 1;
  const tgfx::BackendRenderTarget targets[] = {
      tgfx::BackendRenderTarget(), tgfx::BackendRenderTarget(metalInfo, SURFACE_SIZE, SURFACE_SIZE),
      tgfx::BackendRenderTarget(glFramebufferInfo, SURFACE_SIZE, SURFACE_SIZE)};
  for (const auto& target : targets) {
    EXPECT_TRUE(pag::Devices::MakeForTexture(target) == nullptr);
  }
}

#if TARGET_OS_IPHONE
/**
 * Verifies that detaching and reattaching a view preserves its cached surface unless the Metal
 * drawable size is stale, in which case reattachment updates the size and invalidates the cache.
 */
PAG_TEST(PAGMetalSurfaceTest, ViewWindowMovesPreserveUnchangedSurface) {
  @autoreleasepool {
    ASSERT_TRUE([NSThread isMainThread]);
    auto window = [[[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 64, 64)] autorelease];
    auto view = [[[PAGView alloc] initWithFrame:CGRectMake(0, 0, SURFACE_SIZE, SURFACE_SIZE)]
        autorelease];
    [view setSync:YES];
    [window addSubview:view];
    PAGSurface* surface = [view valueForKey:@"pagSurface"];
    ASSERT_TRUE(surface != nil);
    auto nativeSurface = [surface.surface pagSurface];
    ASSERT_TRUE(nativeSurface != nullptr);
    auto drawable = nativeSurface->drawable;
    auto device = drawable->getDevice();
    ASSERT_TRUE(device != nullptr);
    std::shared_ptr<tgfx::Surface> cachedSurface;
    {
      pag::DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      cachedSurface = drawable->getSurface(scope.context(), false);
      ASSERT_TRUE(cachedSurface != nullptr);
    }
    [view removeFromSuperview];
    EXPECT_TRUE(view.window == nil);
    {
      pag::DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      EXPECT_EQ(drawable->getSurface(scope.context(), true), cachedSurface);
    }
    [window addSubview:view];
    {
      pag::DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      EXPECT_EQ(drawable->getSurface(scope.context(), true), cachedSurface);
    }
    [view removeFromSuperview];
    auto layer = (CAMetalLayer*)view.layer;
    auto size = view.bounds.size;
    auto scale = view.contentScaleFactor;
    auto expectedSize = CGSizeMake(size.width * scale, size.height * scale);
    layer.drawableSize = CGSizeMake(expectedSize.width + 16, expectedSize.height + 16);
    [window addSubview:view];
    EXPECT_TRUE(CGSizeEqualToSize(layer.drawableSize, expectedSize));
    {
      pag::DeviceLockScope scope(device);
      ASSERT_TRUE(static_cast<bool>(scope));
      EXPECT_TRUE(drawable->getSurface(scope.context(), true) == nullptr);
    }
    [view removeFromSuperview];
  }
}
#endif

#endif
