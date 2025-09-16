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

#import "PAGImage.h"
#import "platform/cocoa/private/PAGImageImpl.h"

@interface PAGImage ()

@property(nonatomic, strong) PAGImageImpl* image;
@property(nonatomic) CFDataRef imageData;

@end

@implementation PAGImage {
}

+ (PAGImage*)FromCGImage:(CGImageRef)cgImage {
  PAGImageImpl* pagImage = [PAGImageImpl FromCGImage:cgImage];
  if (pagImage == nil) {
    return nil;
  }
  return [[[PAGImage alloc] initWidthPAGImage:pagImage] autorelease];
}

+ (PAGImage*)FromPath:(NSString*)path {
  PAGImageImpl* pagImage = [PAGImageImpl FromPath:path];
  if (pagImage == nil) {
    return nil;
  }
  return [[[PAGImage alloc] initWidthPAGImage:pagImage] autorelease];
}

+ (PAGImage*)FromBytes:(const void*)bytes size:(size_t)length {
  PAGImageImpl* pagImage = [PAGImageImpl FromBytes:bytes size:length];
  if (pagImage == nil) {
    return nil;
  }
  return [[[PAGImage alloc] initWidthPAGImage:pagImage] autorelease];
}

+ (PAGImage*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  PAGImageImpl* pagImage = [PAGImageImpl FromPixelBuffer:pixelBuffer];
  if (pagImage == nil) {
    return nil;
  }
  return [[[PAGImage alloc] initWidthPAGImage:pagImage] autorelease];
}

- (instancetype)initWidthPAGImage:(PAGImageImpl*)value {
  if (self = [super init]) {
    self.image = value;
  }
  return self;
}

- (void)dealloc {
  if (_image) {
    [_image release];
    _image = nil;
  }
  if (_imageData) {
    CFRelease(_imageData);
  }
  [super dealloc];
}

- (NSInteger)width {
  return [_image width];
}

- (NSInteger)height {
  return [_image height];
}

- (PAGScaleMode)scaleMode {
  return (PAGScaleMode)[_image scaleMode];
}

- (void)setScaleMode:(PAGScaleMode)value {
  [_image setScaleMode:value];
}

- (CGAffineTransform)matrix {
  return [_image matrix];
}

- (void)setMatrix:(CGAffineTransform)value {
  [_image setMatrix:value];
}

@end
