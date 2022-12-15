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

@implementation PAGDecoder {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  NSInteger width;
  NSInteger height;
  NSInteger numFrames;
  UIImage* currentImage;
}

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
  self = [[super init] autorelease];
  if (scale <= 0) {
    scale = 1.0;
  }
  width = (NSInteger)([pagComposition width] * scale);
  height = (NSInteger)([pagComposition height] * scale);
  pagSurface = [PAGSurface MakeFromGPU:CGSizeMake(width, height)];
  if (pagSurface == nil) {
    return nil;
  }
  currentImage = nil;
  pagPlayer = [[PAGPlayer alloc] init];
  numFrames = (NSInteger)([pagComposition duration] * [pagComposition frameRate] / 1000000);
  [pagPlayer setSurface:pagSurface];
  [pagPlayer setComposition:pagComposition];
  return self;
}

- (NSInteger)width {
  return width;
}

- (NSInteger)height {
  return height;
}

- (NSInteger)numFrames {
  return numFrames;
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
  if (index < 0 || index >= numFrames) {
    NSLog(@"Input index is out of bounds!");
    return nil;
  }
  float progress = (index * 1.0 + 0.1) / self.numFrames;
  [pagPlayer setProgress:progress];
  BOOL result = [pagPlayer flush];
  if (!result && currentImage != nil) {
    return currentImage;
  }
  UIImage* image = [self imageFromCVPixelBufferRef:[pagSurface getCVPixelBuffer]];
  currentImage = image;
  return image;
}

- (void)dealloc {
  currentImage = nil;
  if (pagSurface) {
    [pagSurface freeCache];
  }
  if (pagPlayer) {
    [pagPlayer release];
  }
  [super dealloc];
}

@end
