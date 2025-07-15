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
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "rendering/editing/StillImage.h"

@interface PAGImage ()

@property(nonatomic) std::shared_ptr<pag::PAGImage> pagImage;
@property(nonatomic) CFDataRef imageData;

@end

@implementation PAGImage {
}

+ (PAGImage*)FromCGImage:(CGImageRef)cgImage {
  if (cgImage == nil) {
    return nil;
  }
  auto image = tgfx::Image::MakeFrom(cgImage);
  auto data = pag::StillImage::MakeFrom(std::move(image));
  if (data == nullptr) {
    return nil;
  }
  PAGImage* pagImage = [[[PAGImage alloc] initWidthPAGImage:data] autorelease];
  return pagImage;
}

+ (PAGImage*)FromPath:(NSString*)path {
  if (path == nil) {
    return nil;
  }
  auto data = pag::PAGImage::FromPath(path.UTF8String);
  if (data == nullptr) {
    return nil;
  }
  PAGImage* image = [[[PAGImage alloc] initWidthPAGImage:data] autorelease];
  return image;
}

+ (PAGImage*)FromBytes:(const void*)bytes size:(size_t)length {
  auto data = pag::PAGImage::FromBytes(bytes, length);
  if (data == nullptr) {
    return nil;
  }
  PAGImage* image = [[[PAGImage alloc] initWidthPAGImage:data] autorelease];
  return image;
}

+ (PAGImage*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  auto image = tgfx::Image::MakeFrom(pixelBuffer);
  auto pagImage = pag::StillImage::MakeFrom(std::move(image));
  if (pagImage == nullptr) {
    return nil;
  }
  return [[[PAGImage alloc] initWidthPAGImage:pagImage] autorelease];
}

- (instancetype)initWidthPAGImage:(std::shared_ptr<pag::PAGImage>)value {
  if (self = [super init]) {
    _pagImage = value;
  }
  return self;
}

- (void)dealloc {
  if (_imageData) {
    CFRelease(_imageData);
  }
  [super dealloc];
}

- (NSInteger)width {
  return _pagImage->width();
}

- (NSInteger)height {
  return _pagImage->height();
}

- (PAGScaleMode)scaleMode {
  return (PAGScaleMode)_pagImage->scaleMode();
}

- (void)setScaleMode:(PAGScaleMode)value {
  _pagImage->setScaleMode(static_cast<pag::PAGScaleMode>(value));
}

- (CGAffineTransform)matrix {
  auto matrix = _pagImage->matrix();
  float values[9];
  matrix.get9(values);
  return {values[0], values[3], values[1], values[4], values[2], values[5]};
}

- (void)setMatrix:(CGAffineTransform)value {
  pag::Matrix matrix = {};
  matrix.setAffine(value.a, value.b, value.c, value.d, value.tx, value.ty);
  _pagImage->setMatrix(matrix);
}

@end
