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
#include "base/PAGTest.h"
#import "platform/cocoa/PAGPlayer.h"
#include "platform/cocoa/private/PAGViewSurface.h"

#if TARGET_OS_IPHONE
#import "platform/ios/PAGView.h"
#import "platform/ios/private/PAGSurfaceImpl.h"
#else
#import "platform/mac/PAGView.h"
#import "platform/mac/private/PAGSurfaceImpl.h"
#endif

@interface PAGView (CocoaSurfaceTests)
- (void)initPAGSurface;
@end

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
 * Verifies that backend lock selection preserves Metal serialization and OpenGL's empty lock.
 */
PAG_TEST(PAGCocoaSurfaceTest, ViewBackendLockPolicy) {
  std::mutex mutex;
  {
    auto lock = pag::cocoa::PAGViewSurface::Lock(mutex);
    EXPECT_EQ(lock.owns_lock(), pag::Devices::SupportsBackend(tgfx::Backend::Metal));
    if (lock.owns_lock()) {
      EXPECT_EQ(lock.mutex(), &mutex);
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
#endif

#endif
