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

@implementation PAGDecoder {
  PAGPlayer* pagPlayer;
  PAGSurface* pagSurface;
  NSInteger numFrames;
  UIImage* lastFrameImage;
}

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
                                          pagSurface:pagSurface] autorelease];
}

- (instancetype)initWithPAGComposition:(PAGComposition*)composition
                            pagSurface:(PAGSurface*)surface {
  self = [super init];
  if (self) {
    pagSurface = [surface retain];
    pagPlayer = [[PAGPlayer alloc] init];
    [pagPlayer setComposition:composition];
    [pagPlayer setSurface:pagSurface];
    numFrames = [composition duration] * [composition frameRate] / 1000000;
    lastFrameImage = nil;
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
  return numFrames;
}

- (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
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
  if (!result && lastFrameImage != nil) {
    return lastFrameImage;
  }
  UIImage* image = [self imageFromCVPixelBufferRef:[pagSurface makeSnapshot]];
  if (lastFrameImage) {
    [lastFrameImage release];
  }
  lastFrameImage = [image retain];
  return image;
}

- (void)dealloc {
  [lastFrameImage release];
  lastFrameImage = nil;
  [pagSurface release];
  [pagPlayer release];
  [super dealloc];
}

@end
