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

#import "PAGDecoder.h"

#import <VideoToolbox/VideoToolbox.h>

#import "PAGPlayer.h"
#import "PAGSurface.h"

@interface PAGDecoder ()
@property(atomic, assign) float maxFrameRate;
@property(atomic, assign) NSInteger frames;
@end

@implementation PAGDecoder {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  UIImage* lastFrameImage;
  CGFloat _scale;
  dispatch_queue_t queue;
}

@synthesize maxFrameRate = _maxFrameRate;

+ (instancetype)Make:(PAGComposition*)pagComposition {
  return [PAGDecoder Make:pagComposition scale:1.0f];
}

+ (instancetype)Make:(PAGComposition*)pagComposition scale:(CGFloat)scale {
  if (pagComposition == nil) {
    return nil;
  }
  if (scale <= 0) {
    scale = 1.0;
  }
  PAGSurface* pagSurface = [PAGSurface
      MakeOffscreen:CGSizeMake([pagComposition width] * scale, [pagComposition height] * scale)];
  if (pagSurface == nil) {
    return nil;
  }

  return [[[PAGDecoder alloc] initWithPAGComposition:pagComposition
                                          pagSurface:pagSurface
                                               scale:scale] autorelease];
}

- (instancetype)initWithPAGComposition:(PAGComposition*)composition
                            pagSurface:(PAGSurface*)surface
                                 scale:(CGFloat)scale {
  self = [super init];
  if (self) {
    pagSurface = [surface retain];
    pagPlayer = [[PAGPlayer alloc] init];
    [pagPlayer setComposition:composition];
    [pagPlayer setSurface:pagSurface];
    _maxFrameRate = 30.0f;
    _scale = scale;
    float frameRate =
        _maxFrameRate > [composition frameRate] ? [composition frameRate] : _maxFrameRate;
    self.frames = [composition duration] * frameRate / 1000000;
    lastFrameImage = nil;
    queue = dispatch_queue_create("pag.art.PAGDecoder", DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

- (NSInteger)width {
  return [pagSurface width];
}

- (NSInteger)height {
  return [pagSurface width];
}

- (NSInteger)numFrames {
  return self.frames;
}

- (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
}

- (UIImage*)frameAtIndex:(NSInteger)index {
  BOOL result = [self renderCurrentFrame:index];
  if (!result && lastFrameImage != nil) {
    return lastFrameImage;
  }
  UIImage* image = [self imageFromCVPixelBufferRef:[pagSurface makeSnapshot]];
  if (lastFrameImage) {
    [lastFrameImage release];
  }
  lastFrameImage = [image retain];
  return [[image retain] autorelease];
}

- (void)setMaxFrameRate:(float)value {
  if (value > 0) {
    _maxFrameRate = _maxFrameRate > value ? value : _maxFrameRate;
    self.frames = [[pagPlayer getComposition] duration] * _maxFrameRate / 1000000;
  }
}

- (float)maxFrameRate {
  return _maxFrameRate;
}

- (BOOL)renderCurrentFrame:(NSInteger)index {
  if (index < 0 || index >= self.frames) {
    NSLog(@"Input index is out of bounds!");
    return false;
  }
  float progress = (index * 1.0 + 0.1) / self.frames;
  [pagPlayer setProgress:progress];
  return [pagPlayer flush];
}

- (void)readPixelsWithColorType:(PAGColorType)colorType
                      alphaType:(PAGAlphaType)alphaType
                      dstPixels:(void*)dstPixels
                    dstRowBytes:(size_t)dstRowBytes
                          index:(NSInteger)index
                      withBlock:(nullable void (^)(BOOL status))block {
  __block __typeof(self) weakSelf = self;
  dispatch_async(queue, ^{
    BOOL status = [weakSelf readPixelsWithColorType:colorType
                                          alphaType:alphaType
                                          dstPixels:dstPixels
                                        dstRowBytes:dstRowBytes
                                              index:index];
    block(status);
  });
}

- (BOOL)readPixelsWithColorType:(PAGColorType)colorType
                      alphaType:(PAGAlphaType)alphaType
                      dstPixels:(void*)dstPixels
                    dstRowBytes:(size_t)dstRowBytes
                          index:(NSInteger)index {
  [self renderCurrentFrame:index];
  return [pagSurface readPixelsWithColorType:colorType
                                   alphaType:alphaType
                                   dstPixels:dstPixels
                                 dstRowBytes:dstRowBytes];
}

- (void)dealloc {
  [lastFrameImage release];
  lastFrameImage = nil;
  [pagSurface freeCache];
  [pagSurface release];
  [pagPlayer release];
  [super dealloc];
}

@end
