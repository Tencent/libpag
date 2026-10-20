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

#if TARGET_OS_IPHONE
#import "platform/ios/PAGSurface.h"
#import "platform/ios/private/PAGSurfaceImpl.h"
#if defined(TGFX_USE_METAL)
#import "platform/ios/PAGSurface+Metal.h"
#import "platform/ios/private/metal/PAGSurfaceImpl+Metal.h"
#else
#import "platform/ios/PAGSurface+OpenGL.h"
#import "platform/ios/private/opengl/PAGSurfaceImpl+OpenGL.h"
#endif
#else
#import "platform/mac/PAGSurface.h"
#import "platform/mac/private/PAGSurfaceImpl.h"
#if defined(TGFX_USE_METAL)
#import "platform/mac/PAGSurface+Metal.h"
#import "platform/mac/private/metal/PAGSurfaceImpl+Metal.h"
#else
#import "platform/mac/PAGSurface+OpenGL.h"
#import "platform/mac/private/opengl/PAGSurfaceImpl+OpenGL.h"
#endif
#endif

/**
 * Verifies that the selected backend's public and private factories are linked with their classes
 * without requiring Objective-C category force-loading linker options.
 */
PAG_TEST(PAGCocoaSurfaceTest, BackendFactoriesAreLinked) {
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromLayer:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromLayer:)]);
#else
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromView:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromView:)]);
#endif
#if TARGET_OS_IPHONE
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:)]);
#if defined(TGFX_USE_METAL)
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
  EXPECT_FALSE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
#else
  EXPECT_TRUE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
  EXPECT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
  EXPECT_FALSE([PAGSurface respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
  EXPECT_FALSE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
#endif
#endif
}

/**
 * Verifies that a public backend factory can dispatch to the private backend factory without an
 * unrecognized selector, preserving the existing null-input failure behavior.
 */
PAG_TEST(PAGCocoaSurfaceTest, NullHostReturnsNil) {
  @autoreleasepool {
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromLayer:)]);
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromLayer:)]);
    EXPECT_TRUE([PAGSurface FromLayer:nil] == nil);
    EXPECT_TRUE([PAGSurfaceImpl FromLayer:nil] == nil);
#else
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromView:)]);
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromView:)]);
    EXPECT_TRUE([PAGSurface FromView:nil] == nil);
    EXPECT_TRUE([PAGSurfaceImpl FromView:nil] == nil);
#endif
  }
}

#if TARGET_OS_IPHONE
/**
 * Verifies that the backend-agnostic pixel-buffer factory reaches the selected private backend
 * method when only the main class entry point is called.
 */
PAG_TEST(PAGCocoaSurfaceTest, DefaultPixelBufferFactoryDispatches) {
  @autoreleasepool {
#if defined(TGFX_USE_METAL)
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:mtlDevice:)]);
#else
    ASSERT_TRUE([PAGSurfaceImpl respondsToSelector:@selector(FromCVPixelBuffer:context:)]);
#endif
    EXPECT_TRUE([PAGSurface FromCVPixelBuffer:nil] == nil);
    EXPECT_TRUE([PAGSurfaceImpl FromCVPixelBuffer:nil] == nil);
  }
}
#endif

#endif
