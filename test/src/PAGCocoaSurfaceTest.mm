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

#if defined(__APPLE__)

#import <TargetConditionals.h>
#include <thread>
#include "base/PAGTest.h"
#import "platform/cocoa/PAGPlayer.h"
#include "platform/cocoa/private/PAGSurfaceFactory.h"
#include "platform/cocoa/private/PAGViewSurface.h"

#if TARGET_OS_IPHONE
#include "platform/cocoa/private/PixelBufferUtil.h"
#import "platform/ios/PAGView.h"
#import "platform/ios/private/PAGSurfaceImpl.h"
#else
#import "platform/mac/PAGView.h"
#import "platform/mac/private/PAGSurfaceImpl.h"
#endif

@interface PAGView (CocoaSurfaceTests)
- (void)initPAGSurface;
@end

static void ProbeViewMutex(std::mutex* mutex, bool* acquired) {
  *acquired = mutex->try_lock();
  if (*acquired) {
    mutex->unlock();
  }
}

static void CheckInvalidHost(id host) {
  SCOPED_TRACE([NSStringFromClass([host class]) UTF8String]);
  EXPECT_EQ(pag::cocoa::MakeSurfaceFromHost(host), nullptr);
  EXPECT_TRUE([PAGSurfaceImpl FromHost:host] == nil);
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
  EXPECT_TRUE([PAGSurface FromLayer:host] == nil);
#else
  EXPECT_TRUE([PAGSurface FromView:host] == nil);
#endif
}

/**
 * Verifies that the selected backend's public and private factories are linked with their classes
 * without requiring Objective-C category force-loading linker options.
 */
PAG_TEST(PAGCocoaSurfaceTest, BackendFactoriesAreLinked) {
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromHost:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromLayer:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromView:)]);
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromLayer:)]);
#else
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromView:)]);
#endif
#if TARGET_OS_IPHONE
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:device:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
  EXPECT_FALSE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
#if defined(TGFX_USE_METAL)
  EXPECT_FALSE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
#else
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
#endif
#endif
}

/**
 * Verifies that a public backend factory can dispatch to the private backend factory without an
 * unrecognized selector, preserving the existing null-input failure behavior.
 */
PAG_TEST(PAGCocoaSurfaceTest, NullHostReturnsNil) {
  @autoreleasepool {
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromHost:)]);
    EXPECT_TRUE([PAGSurfaceImpl FromHost:nil] == nil);
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromLayer:)]);
    EXPECT_TRUE([PAGSurface FromLayer:nil] == nil);
#else
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromView:)]);
    EXPECT_TRUE([PAGSurface FromView:nil] == nil);
#endif
  }
}

/**
 * Verifies that invalid hosts are rejected before accessing backend-specific native properties.
 */
PAG_TEST(PAGCocoaSurfaceTest, InvalidHostReturnsNil) {
  @autoreleasepool {
    CheckInvalidHost([[NSObject alloc] init]);
    CheckInvalidHost([CALayer layer]);
#if defined(TGFX_USE_METAL)
    CheckInvalidHost([[PAGView alloc] initWithFrame:CGRectMake(0, 0, 16, 16)]);
#else
    CheckInvalidHost([CAMetalLayer layer]);
#endif
  }
}

/**
 * Verifies that all iOS backends and macOS Metal serialize view operations, while macOS OpenGL
 * returns an empty lock.
 */
PAG_TEST(PAGCocoaSurfaceTest, ViewBackendLockPolicy) {
#if TARGET_OS_IPHONE
  bool expectsLock = true;
#else
  bool expectsLock = pag::Devices::SupportsBackend(tgfx::Backend::Metal);
#endif
  std::mutex mutex;
  {
    auto lock = pag::cocoa::PAGViewSurface::Lock(mutex);
    EXPECT_EQ(lock.owns_lock(), expectsLock);
    if (lock.owns_lock()) {
      EXPECT_EQ(lock.mutex(), &mutex);
    }
    bool acquired = false;
    std::thread probe(ProbeViewMutex, &mutex, &acquired);
    probe.join();
    if (expectsLock) {
      EXPECT_FALSE(acquired);
    }
  }
  ASSERT_TRUE(mutex.try_lock());
  mutex.unlock();
}

/**
 * Verifies that both view backends retain one surface while resizing, including synchronous updates
 * after growing from zero size.
 */
PAG_TEST(PAGCocoaSurfaceTest, ViewSurfaceSurvivesResize) {
  @autoreleasepool {
    ASSERT_TRUE([NSThread isMainThread]);
    auto view = [[PAGView alloc] initWithFrame:CGRectMake(0, 0, 16, 16)];
#if TARGET_OS_IPHONE
    [view setSync:YES];
#else
    EXPECT_EQ(view.wantsLayer, pag::Devices::SupportsBackend(tgfx::Backend::Metal));
#endif
    [view initPAGSurface];
    PAGSurface* surface = [view valueForKey:@"pagSurface"];
    ASSERT_TRUE(surface != nil);
    int width = surface.width;
    int height = surface.height;
    [view setBounds:CGRectMake(0, 0, 32, 32)];
    EXPECT_EQ([view valueForKey:@"pagSurface"], surface);
    EXPECT_EQ(surface.width, width * 2);
    EXPECT_EQ(surface.height, height * 2);
#if TARGET_OS_IPHONE
    [view setBounds:CGRectMake(0, 0, 0, 0)];
    [view setBounds:CGRectMake(0, 0, 16, 16)];
#else
    [view setBounds:CGRectMake(0, 0, 16, 16)];
    [view setFrameSize:NSMakeSize(0, 0)];
    [view setFrameSize:NSMakeSize(16, 16)];
#endif
    EXPECT_EQ([view valueForKey:@"pagSurface"], surface);
    EXPECT_EQ(surface.width, width);
    EXPECT_EQ(surface.height, height);
  }
}

#if TARGET_OS_IPHONE
/**
 * Verifies that the backend-agnostic pixel-buffer factory reaches the selected private backend
 * method when only the main class entry point is called.
 */
PAG_TEST(PAGCocoaSurfaceTest, DefaultPixelBufferFactoryDispatches) {
  @autoreleasepool {
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:device:)]);
    EXPECT_TRUE([PAGSurface FromCVPixelBuffer:nil] == nil);
    EXPECT_TRUE([PAGSurfaceImpl FromCVPixelBuffer:nil] == nil);
    EXPECT_TRUE([PAGSurfaceImpl FromCVPixelBuffer:nil device:nil] == nil);
  }
}

/**
 * Verifies that invalid native devices are rejected even when a valid pixel buffer is supplied.
 */
PAG_TEST(PAGCocoaSurfaceTest, InvalidPixelBufferDeviceReturnsNil) {
  @autoreleasepool {
    auto pixelBuffer = pag::PixelBufferUtil::Make(16, 16);
    ASSERT_TRUE(pixelBuffer != nil);
    NSArray* invalidDevices = @[[[NSObject alloc] init], [CALayer layer]];
    for (id device in invalidDevices) {
      SCOPED_TRACE([NSStringFromClass([device class]) UTF8String]);
      EXPECT_EQ(pag::cocoa::MakeSurfaceFromPixelBuffer(pixelBuffer, device), nullptr);
      EXPECT_TRUE([PAGSurfaceImpl FromCVPixelBuffer:pixelBuffer device:device] == nil);
#if defined(TGFX_USE_OPENGL)
      EXPECT_TRUE([PAGSurface FromCVPixelBuffer:pixelBuffer context:device] == nil);
#endif
    }
  }
}
#endif

#endif
