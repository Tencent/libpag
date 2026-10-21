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
#import "PAGSurface.h"
#import "PAGView.h"

static BOOL FactoryReturnsNil(Class type, SEL selector) {
  NSMethodSignature* signature = [type methodSignatureForSelector:selector];
  if (signature == nil) {
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
  return result == nil;
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
    if (impl == Nil || !FactoryReturnsNil(impl, NSSelectorFromString(@"FromHost:"))) {
      return 2;
    }
    PAGSurface* surface = [PAGSurface MakeOffscreen:CGSizeMake(16, 16)];
    if (surface == nil || surface.width != 16 || surface.height != 16) {
      return 3;
    }
    PAGView* view = [[PAGView alloc] initWithFrame:CGRectMake(0, 0, 16, 16)];
    if (view == nil) {
      return 4;
    }
#if TARGET_OS_IPHONE
    if ([view layer] == nil || [PAGSurface FromCVPixelBuffer:nil] != nil) {
      return 5;
    }
#else
    [view setFrameSize:NSMakeSize(32, 32)];
    if (view.bounds.size.width != 32 || view.bounds.size.height != 32) {
      return 5;
    }
#endif
  }
  return 0;
}
