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
@property(atomic, assign) NSInteger frames;
@end

@implementation PAGDecoder {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  UIImage* lastFrameImage;
  CGFloat _scale;
}

- (instancetype)initWithPAGComposition:(PAGComposition*)composition
                            pagSurface:(PAGSurface*)surface
                             frameRate:(CGFloat)frameRate
                                 scale:(CGFloat)scale {
  self = [super init];
  if (self) {
    pagSurface = [surface retain];
    pagPlayer = [[PAGPlayer alloc] init];
    [pagPlayer setComposition:composition];
    [pagPlayer setSurface:pagSurface];
    _scale = scale;
    float rate = [composition frameRate] > frameRate ? frameRate : [composition frameRate];
    self.frames = [composition duration] * rate / 1000000;
    lastFrameImage = nil;
  }
  return self;
}

- (void)dealloc {
  [pagSurface release];
  [pagPlayer release];
  [super dealloc];
}

#pragma mark - public

+ (instancetype)Make:(PAGComposition*)pagComposition frameRate:(CGFloat)frameRate {
  return [PAGDecoder Make:pagComposition frameRate:frameRate scale:1.0];
}

+ (instancetype)Make:(PAGComposition*)pagComposition
           frameRate:(CGFloat)frameRate
               scale:(CGFloat)scale {
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
                                           frameRate:frameRate
                                               scale:scale] autorelease];
}

- (BOOL)copyFrameAt:(NSInteger)index To:(CVPixelBufferRef)pixelBuffer {
  BOOL result = [self renderCurrentFrame:index];
  if (!result) {
    return nil;
  }
  return [pagSurface copyImageTo:pixelBuffer];
}

- (UIImage*)frameAtIndex:(NSInteger)index {
  BOOL result = [self renderCurrentFrame:index];
  if (!result && lastFrameImage != nil) {
    return nil;
  }
  UIImage* image = [self imageFromCVPixelBufferRef:[pagSurface makeSnapshot]];
  if (lastFrameImage) {
    [lastFrameImage release];
  }
  lastFrameImage = [image retain];
  return [[image retain] autorelease];
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

#pragma mark - private

- (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
  if (pixelBuffer == nil) {
    return nil;
  }
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
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

@end
