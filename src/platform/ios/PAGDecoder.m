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
@property(nonatomic, strong) PAGSurface* pagSurface;
@property(nonatomic, strong) PAGPlayer* pagPlayer;
@property(nonatomic, strong) UIImage* lastFrameImage;
@property(nonatomic, assign) NSInteger numFrames;
@end

@implementation PAGDecoder

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
  PAGDecoder* pagDecoder = [[[PAGDecoder alloc] init] autorelease];
  pagDecoder.pagSurface = [PAGSurface
      MakeOffscreen:CGSizeMake([pagComposition width] * scale, [pagComposition height] * scale)];
  if (pagDecoder.pagSurface == nil) {
    return nil;
  }
  pagDecoder.lastFrameImage = nil;
  pagDecoder.pagPlayer = [[[PAGPlayer alloc] init] autorelease];
  pagDecoder.numFrames = [pagComposition duration] * [pagComposition frameRate] / 1000000;
  [pagDecoder.pagPlayer setSurface:pagDecoder.pagSurface];
  [pagDecoder.pagPlayer setComposition:pagComposition];

  return pagDecoder;
}

- (NSInteger)width {
  return [_pagSurface width];
}

- (NSInteger)height {
  return [_pagSurface width];
}

- (NSInteger)numFrames {
  return _numFrames;
}

- (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
  CGImageRef imageRef = nil;
  VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nil, &imageRef);
  UIImage* uiImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return uiImage;
}

- (UIImage*)frameAtIndex:(NSInteger)index {
  if (index < 0 || index >= _numFrames) {
    NSLog(@"Input index is out of bounds!");
    return nil;
  }
  float progress = (index * 1.0 + 0.1) / self.numFrames;
  [_pagPlayer setProgress:progress];
  BOOL result = [_pagPlayer flush];
  if (!result && _lastFrameImage != nil) {
    return _lastFrameImage;
  }
  UIImage* image = [self imageFromCVPixelBufferRef:[_pagSurface makeSnapshot]];
  if (_lastFrameImage) {
    [_lastFrameImage release];
  }
  _lastFrameImage = [image retain];
  return image;
}

- (void)dealloc {
  [_lastFrameImage release];
  _lastFrameImage = nil;
  [_pagSurface release];
  [_pagPlayer release];
  [super dealloc];
}

@end
