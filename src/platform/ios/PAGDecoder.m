/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#import "private/PAGDecoderImpl.h"

@implementation PAGDecoder {
  PAGDecoderImpl* pagDecoder;
}
+ (nullable instancetype)Make:(nullable PAGComposition*)composition {
  if (composition == nil) {
    return nil;
  }
  return [PAGDecoder Make:composition maxFrameRate:[composition frameRate] scale:1.0f];
}

+ (nullable instancetype)Make:(nullable PAGComposition*)composition
                 maxFrameRate:(float)maxFrameRate
                        scale:(float)scale {
  PAGDecoderImpl* decoder = [PAGDecoderImpl Make:composition maxFrameRate:maxFrameRate scale:scale];
  if (decoder == nil) {
    return nil;
  }
  return [[[PAGDecoder alloc] initWithDecoder:decoder] autorelease];
}

- (instancetype)initWithDecoder:(PAGDecoderImpl*)value {
  if (self = [super init]) {
    pagDecoder = [value retain];
  }
  return self;
}

- (void)dealloc {
  [pagDecoder release];
  [super dealloc];
}

- (id)impl {
  return pagDecoder;
}

- (NSInteger)width {
  return [pagDecoder width];
}

- (NSInteger)height {
  return [pagDecoder height];
}

- (NSInteger)numFrames {
  return [pagDecoder numFrames];
}

- (float)frameRate {
  return [pagDecoder frameRate];
}

- (BOOL)checkFrameChanged:(int)index {
  return [pagDecoder checkFrameChanged:index];
}

- (BOOL)copyFrameTo:(void*)pixels rowBytes:(size_t)rowBytes at:(NSInteger)index {
  return [pagDecoder copyFrameTo:pixels rowBytes:rowBytes at:index];
}

- (BOOL)readFrame:(NSInteger)index to:(CVPixelBufferRef)pixelBuffer {
  return [pagDecoder readFrame:index to:pixelBuffer];
}

- (nullable UIImage*)frameAtIndex:(NSInteger)index {
  return [pagDecoder frameAtIndex:index];
}

@end
