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

#import "PAGImageImpl.h"
#include "base/utils/Log.h"
#include "pag/pag.h"
#include "rendering/editing/StillImage.h"

@interface PAGImageImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGImage> pagImage;

@end

@implementation PAGImageImpl {
}

+ (PAGImageImpl*)FromPath:(NSString*)path {
  if (path == nil) {
    return nil;
  }
  auto data = pag::PAGImage::FromPath(path.UTF8String);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* image = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return image;
}

+ (PAGImageImpl*)FromBytes:(const void*)bytes size:(size_t)length {
  auto data = pag::PAGImage::FromBytes(bytes, length);
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* image = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return image;
}

+ (PAGImageImpl*)FromPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  auto image = tgfx::Image::MakeFrom(pixelBuffer);
  auto pagImage = pag::StillImage::MakeFrom(std::move(image));
  if (pagImage == nullptr) {
    return nil;
  }
  return [[[PAGImageImpl alloc] initWidthPAGImage:pagImage] autorelease];
}

+ (PAGImageImpl*)FromCGImage:(CGImageRef)cgImage {
  if (cgImage == nil) {
    return nil;
  }
  auto image = tgfx::Image::MakeFrom(cgImage);
  auto data = pag::StillImage::MakeFrom(std::move(image));
  if (data == nullptr) {
    return nil;
  }
  PAGImageImpl* pagImage = [[[PAGImageImpl alloc] initWidthPAGImage:data] autorelease];
  return pagImage;
}

- (instancetype)initWidthPAGImage:(std::shared_ptr<pag::PAGImage>)value {
  if (self = [super init]) {
    _pagImage = value;
  }
  return self;
}

- (NSInteger)width {
  return _pagImage->width();
}

- (NSInteger)height {
  return _pagImage->height();
}

- (int)scaleMode {
  return static_cast<int>(_pagImage->scaleMode());
}

- (void)setScaleMode:(int)value {
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
