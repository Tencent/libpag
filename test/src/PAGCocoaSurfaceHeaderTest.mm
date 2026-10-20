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
#else
#import "platform/mac/PAGSurface.h"
#endif

/**
 * Verifies that importing only PAGSurface.h exposes the selected backend's factories. This test
 * is compiled with ARC, which rejects calls whose class method declarations are not visible.
 */
PAG_TEST(PAGCocoaSurfaceTest, PrimaryHeaderExposesBackendFactories) {
  @autoreleasepool {
#if TARGET_OS_IPHONE || defined(TGFX_USE_METAL)
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromLayer:)]);
    EXPECT_TRUE([PAGSurface FromLayer:nil] == nil);
#else
    ASSERT_TRUE([PAGSurface respondsToSelector:@selector(FromView:)]);
    EXPECT_TRUE([PAGSurface FromView:nil] == nil);
#endif
#if TARGET_OS_IPHONE
#if defined(TGFX_USE_METAL)
    EXPECT_TRUE([PAGSurface FromCVPixelBuffer:nil mtlDevice:nil] == nil);
#else
    EXPECT_TRUE([PAGSurface FromCVPixelBuffer:nil context:nil] == nil);
#endif
#endif
  }
}

#endif
