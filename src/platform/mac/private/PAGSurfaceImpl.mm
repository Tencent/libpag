/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#import "PAGSurfaceImpl.h"
#include "GPUDrawable.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"
#include "PixelBufferUtils.h"
#include "base/utils/Log.h"

@interface PAGSurfaceImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGSurface> pagSurface;
@property(nonatomic) CVPixelBufferRef cvPixelBuffer;

@end

@implementation PAGSurfaceImpl

+ (PAGSurfaceImpl*)FromView:(NSView*)view {
  auto drawable = pag::GPUDrawable::FromView(view);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
  return pagSurface;
}

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size {
  auto pixelBuffer = pag::PixelBufferUtils::Make(static_cast<int>(roundf(size.width)),
                                                 static_cast<int>(roundf(size.height)));
  auto drawable = pag::GPUDrawable::FromCVPixelBuffer(pixelBuffer);
  if (drawable == nullptr) {
    return nil;
  }
  auto surface = pag::PAGSurface::MakeFrom(drawable);
  PAGSurfaceImpl* pagSurface = [[[PAGSurfaceImpl alloc] initWithSurface:surface] autorelease];
  pagSurface.cvPixelBuffer = pixelBuffer;
  return pagSurface;
}

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value {
  if (self = [super init]) {
    _pagSurface = value;
  }
  return self;
}

- (void)dealloc {
  if (_cvPixelBuffer != nil) {
    CFRelease(_cvPixelBuffer);
    _cvPixelBuffer = nil;
  }
  [super dealloc];
}

- (void)updateSize {
  _pagSurface->updateSize();
}

- (int)width {
  return _pagSurface->width();
}

- (int)height {
  return _pagSurface->height();
}

- (BOOL)clearAll {
  return _pagSurface->clearAll();
}

- (void)freeCache {
  _pagSurface->freeCache();
}

- (CVPixelBufferRef)getCVPixelBuffer {
  return _cvPixelBuffer;
}

- (CVPixelBufferRef)makeSnapshot {
  CVPixelBufferRef pixelBuffer = [self getCVPixelBuffer];
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  int bufferWidth = (int)CVPixelBufferGetWidth(pixelBuffer);
  int bufferHeight = (int)CVPixelBufferGetHeight(pixelBuffer);
  size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
  void* baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);

  // Copy the pixel buffer
  CVPixelBufferRef pixelBufferCopy = nullptr;
  CFDictionaryRef empty =
      CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);  // our empty IOSurface
  NSDictionary* options =
      [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:YES],
                                                 kCVPixelBufferCGImageCompatibilityKey,
                                                 [NSNumber numberWithBool:YES],
                                                 kCVPixelBufferCGBitmapContextCompatibilityKey,
                                                 empty, kCVPixelBufferIOSurfacePropertiesKey, nil];
  CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, bufferWidth, bufferHeight,
                                        CVPixelBufferGetPixelFormatType(pixelBuffer),
                                        (__bridge CFDictionaryRef)options, &pixelBufferCopy);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (status == kCVReturnSuccess) {
    CVPixelBufferLockBaseAddress(pixelBufferCopy, 0);
    void* copyBaseAddress = CVPixelBufferGetBaseAddress(pixelBufferCopy);
    memcpy(copyBaseAddress, baseAddress, bufferHeight * bytesPerRow);
    CVPixelBufferUnlockBaseAddress(pixelBufferCopy, 0);
  } else {
    NSLog(@"CVPixelBufferRef copy failed!");
  }
  CFRelease(empty);
  CFAutorelease(pixelBufferCopy);
  return pixelBufferCopy;
}

@end
