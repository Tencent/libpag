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
#import "PAGPlayer.h"
#import "PAGSurface.h"

@interface PAGDecoder ()
@property(nonatomic, assign) NSInteger width;
@property(nonatomic, assign) NSInteger height;
@property(nonatomic, assign) NSInteger numFrames;
@property(nonatomic, strong) PAGPlayer* pagPlayer;
@property(nonatomic, strong) PAGSurface* pagSurface;
@end

@implementation PAGDecoder

+ (instancetype)Make:(PAGComposition*)pagComposition {
  return [PAGDecoder Make:pagComposition scale:1.0f];
}

+ (instancetype)Make:(PAGComposition*)pagComposition scale:(CGFloat)scale {
  if (pagComposition == nil) {
    return nil;
  }
  return [[PAGDecoder alloc] initWithPAGComposition:pagComposition scale:scale];
}

- (instancetype)initWithPAGComposition:(PAGComposition*)pagComposition scale:(CGFloat)scale {
  self = [super init];
  if (scale <= 0) {
    scale = 1.0;
  }
  self.width = (NSInteger)([pagComposition width] * scale);
  self.height = (NSInteger)([pagComposition height] * scale);
  self.pagSurface = [PAGSurface MakeFromGPU:CGSizeMake(self.width, self.height)];
  self.pagPlayer = [[PAGPlayer alloc] init];
  self.numFrames = (NSInteger)([pagComposition duration] * [pagComposition frameRate] / 1000000);
  [self.pagPlayer setSurface:self.pagSurface];
  [self.pagPlayer setComposition:pagComposition];
  return self;
}

- (NSInteger)width {
  return _width;
}

- (NSInteger)height {
  return _height;
}

- (NSInteger)numFrames {
  return _numFrames;
}

- (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  CIImage* ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  CIContext* temporaryContext = [CIContext contextWithOptions:nil];
  CGImageRef imageRef =
      [temporaryContext createCGImage:ciImage
                             fromRect:CGRectMake(0, 0, CVPixelBufferGetWidth(pixelBuffer),
                                                 CVPixelBufferGetHeight(pixelBuffer))];

  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
}

- (UIImage*)frameAtIndex:(NSInteger)index {
  if (index < 0 || index >= self.numFrames) {
    NSLog(@"Input index is out of bounds!");
    return nil;
  }
  float progress = (index * 1.0 + 0.1) / self.numFrames;
  [self.pagPlayer setProgress:progress];
  [self.pagPlayer flush];
  CVPixelBufferRef cvPixelBuffer = [self.pagSurface getCVPixelBuffer];
  return [self imageFromCVPixelBufferRef:cvPixelBuffer];
}

- (void)dealloc {
  [self.pagPlayer release];
  [self.pagSurface release];
  [super dealloc];
}

@end
