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

#import <TargetConditionals.h>
#include <stdio.h>
#import "PAGSurface.h"
#import "PAGView.h"

static BOOL FactoryReturnsNil(Class type, SEL selector) {
  NSMethodSignature* signature = [type methodSignatureForSelector:selector];
  if (signature == nil) {
    fprintf(stderr, "PAGCocoaStaticLinkTest: missing factory %s.%s.\n",
            [NSStringFromClass(type) UTF8String], [NSStringFromSelector(selector) UTF8String]);
    return NO;
  }
  NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
  invocation.target = type;
  invocation.selector = selector;
  id host = nil;
  [invocation setArgument:&host atIndex:2];
  [invocation invoke];
  __unsafe_unretained id result = nil;
  [invocation getReturnValue:&result];
  if (result != nil) {
    fprintf(stderr, "PAGCocoaStaticLinkTest: %s.%s must return nil for a nil host.\n",
            [NSStringFromClass(type) UTF8String], [NSStringFromSelector(selector) UTF8String]);
    return NO;
  }
  return YES;
}

int main(void) {
  @autoreleasepool {
    SEL publicFactory = [PAGSurface respondsToSelector:NSSelectorFromString(@"FromLayer:")]
                            ? NSSelectorFromString(@"FromLayer:")
                            : NSSelectorFromString(@"FromView:");
    if (!FactoryReturnsNil([PAGSurface class], publicFactory)) {
      return 1;
    }
    Class impl = NSClassFromString(@"PAGSurfaceImpl");
    if (impl == Nil) {
      fprintf(stderr, "PAGCocoaStaticLinkTest: missing class PAGSurfaceImpl.\n");
      return 2;
    }
    if (!FactoryReturnsNil(impl, NSSelectorFromString(@"FromHost:"))) {
      return 2;
    }
    PAGView* view = [[PAGView alloc] initWithFrame:CGRectMake(0, 0, 16, 16)];
    if (view == nil) {
      fprintf(stderr, "PAGCocoaStaticLinkTest: PAGView initialization returned nil.\n");
      return 3;
    }
#if TARGET_OS_IPHONE
    if ([view layer] == nil) {
      fprintf(stderr, "PAGCocoaStaticLinkTest: iOS PAGView has no backing layer.\n");
      return 10;
    }
    if ([PAGSurface FromCVPixelBuffer:nil] != nil) {
      fprintf(stderr,
              "PAGCocoaStaticLinkTest: iOS PAGSurface.FromCVPixelBuffer: must return nil for a nil "
              "pixel buffer.\n");
      return 11;
    }
#else
    [view setFrameSize:NSMakeSize(32, 32)];
    if (view.bounds.size.width != 32 || view.bounds.size.height != 32) {
      fprintf(stderr,
              "PAGCocoaStaticLinkTest: macOS PAGView resize expected 32x32 but got %gx%g.\n",
              view.bounds.size.width, view.bounds.size.height);
      return 20;
    }
#endif
  }
  return 0;
}
